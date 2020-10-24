#pragma once

#include "request.h"
#include "response.h"

#include <boost/filesystem.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_stream.hpp>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>

namespace wsgi_boost {
    template<class req_t, class resp_t>
    class BaseRequestHandler {
    protected:
        req_t& m_request;
        resp_t& m_response;

    public:
        BaseRequestHandler(req_t& request, resp_t& response)
            : m_request{request}
            , m_response{response} {}

        BaseRequestHandler(const BaseRequestHandler&) = delete;
        BaseRequestHandler& operator=(const BaseRequestHandler&) = delete;

        virtual ~BaseRequestHandler() {}

        virtual void handle() = 0;
    };

    // Handles static content
    template<class req_t, class resp_t>
    class StaticRequestHandler : public BaseRequestHandler<req_t, resp_t> {
        using BaseRequestHandler<req_t, resp_t>::m_request;
        using BaseRequestHandler<req_t, resp_t>::m_response;

    private:
        std::string& m_cache_control;
        bool m_use_gzip;

        void open_file(const boost::filesystem::path& content_dir_path) {
            boost::filesystem::path path = content_dir_path;
            path /= boost::regex_replace(m_request.path, m_request.path_regex, "");
            boost::system::error_code ec;
            path = boost::filesystem::canonical(path, ec);
            if (!ec) {
                // Checking if path is inside content_dir
                if (std::distance(content_dir_path.begin(), content_dir_path.end()) <= std::distance(path.begin(), path.end()) &&
                    std::equal(content_dir_path.begin(), content_dir_path.end(), path.begin())) {
                    if (boost::filesystem::is_directory(path))
                        path /= "index.html";
                    if (boost::filesystem::exists(path) && boost::filesystem::is_regular_file(path)) {
                        std::ifstream ifs;
                        ifs.open(path.string(), std::ifstream::in | std::ios::binary);
                        if (ifs) {
                            out_headers_t out_headers;
                            out_headers.emplace_back("Cache-Control", m_cache_control);
                            time_t last_modified = boost::filesystem::last_write_time(path);
                            out_headers.emplace_back("Last-Modified", time_to_header(last_modified));
                            // Use hex representation of the last modified POSIX timestamp as ETag
                            std::string etag = "\"" + hex(static_cast<size_t>(last_modified)) + "\"";
                            out_headers.emplace_back("ETag", etag);
                            std::string ims = m_request.get_header("If-Modified-Since");
                            if (m_request.get_header("If-None-Match") == etag || (!ims.empty() && header_to_time(ims) >= last_modified)) {
                                out_headers.emplace_back("Content-Length", "0");
                                m_response.send_header("304 Not Modified", out_headers);
                                return;
                            }
                            std::string ext = path.extension().string();
                            out_headers.emplace_back("Content-Type", get_mime(ext));
                            if (m_use_gzip && m_request.check_header("Accept-Encoding", "gzip") && is_compressable(ext)) {
                                boost::iostreams::filtering_istream gzstream;
                                gzstream.push(boost::iostreams::gzip_compressor());
                                gzstream.push(ifs);
                                std::stringstream compressed;
                                boost::iostreams::copy(gzstream, compressed);
                                out_headers.emplace_back("Content-Encoding", "gzip");
                                send_file(compressed, out_headers);
                            } else {
                                out_headers.emplace_back("Accept-Ranges", "bytes");
                                send_file(ifs, out_headers);
                            }
                            return;
                        }
                    }
                }
            }
            m_response.send_html("404 Not Found",
                                 "Error 404", "Not Found",
                                 "The requested path <code>" + m_request.path + "</code> was not found on this server.");
        }

        void send_file(std::istream& content_stream, out_headers_t& headers) {
            content_stream.seekg(0, std::ios::end);
            size_t length = content_stream.tellg();
            size_t start_pos = 0;
            size_t end_pos = length - 1;
            std::string requested_range = m_request.get_header("Range");
            std::pair<std::string, std::string> range;
            boost::system::error_code ec;
            if (!requested_range.empty() && ((range = parse_range(requested_range)) != std::pair<std::string, std::string>())) {
                if (!range.first.empty())
                    start_pos = std::stoull(range.first);
                else
                    range.first = std::to_string(start_pos);
                if (!range.second.empty())
                    end_pos = std::stoull(range.second);
                else
                    range.second = std::to_string(end_pos);
                if (start_pos > end_pos || start_pos >= length || end_pos >= length) {
                    m_response.send_mesage("416 Range Not Satisfiable", "Invalid bytes range!");
                    return;
                } else {
                    headers.emplace_back("Content-Length", std::to_string(end_pos - start_pos));
                    headers.emplace_back("Content-Range", "bytes " + range.first + "-" + range.second + "/" + std::to_string(length));
                    ec = m_response.send_header("206 Partial Content", headers);
                    if (ec)
                        return;
                }
            } else {
                headers.emplace_back("Content-Length", std::to_string(length));
                ec = m_response.send_header("200 OK", headers);
                if (ec)
                    return;
            }
            if (m_request.method == "GET") {
                if (start_pos > 0)
                    content_stream.seekg(start_pos);
                else
                    content_stream.seekg(0, std::ios::beg);
                const size_t buffer_size = 131072;
                std::vector<char> buffer(buffer_size);
                size_t read_length;
                size_t bytes_left = end_pos - start_pos + 1;
                while (bytes_left > 0 &&
                       ((read_length = content_stream.read(&buffer[0], std::min(bytes_left, buffer_size)).gcount()) > 0)) {
                    ec = m_response.send_data(std::string{&buffer[0], read_length});
                    if (ec)
                        return;
                    bytes_left -= read_length;
                }
            }
        }

    public:
        StaticRequestHandler(req_t& request, resp_t& response, std::string& cache_control, bool use_gzip)
            : m_cache_control{cache_control}
            , m_use_gzip{use_gzip}
            , BaseRequestHandler<req_t, resp_t>(request, response) {}

        // Handle request
        void handle() {
            if (m_request.method != "GET" && m_request.method != "HEAD") {
                m_response.send_mesage("405 Method Not Allowed", "Invalid HTTP method! Only GET and HEAD are allowed.");
                return;
            }
            const auto content_dir_path = boost::filesystem::path{m_request.content_dir};
            if (!boost::filesystem::exists(content_dir_path)) {
                m_response.send_html("500 Internal Server Error",
                                     "Error 500",
                                     "Internal Server Error",
                                     "Invalid static content directory is configured.");
                return;
            }
            open_file(content_dir_path);
        }
    };

    // Handles WSGI requests
    template<class conn_t, class req_t, class resp_t>
    class WsgiRequestHandler : public BaseRequestHandler<req_t, resp_t> {
        using BaseRequestHandler<req_t, resp_t>::m_request;
        using BaseRequestHandler<req_t, resp_t>::m_response;

    private:
        std::string m_status;
        out_headers_t m_out_headers;
        pybind11::object& m_app;
        pybind11::dict m_environ;
        pybind11::object m_write;
        pybind11::object m_start_response;
        // Reserved for future use. Now it only indicates that a response has Content-Length header.
        long long m_content_length = -1;
        std::string& m_url_scheme;
        std::string& m_host_name;
        unsigned short m_local_port;
        bool m_multithread;
        std::string m_write_data;

        // Create write() callable: https://www.python.org/dev/peps/pep-3333/#the-write-callable
        pybind11::object create_write() {
            auto wr = [this](pybind11::bytes& data) {
                this->m_write_data = data;
                size_t data_len = m_write_data.length();
                if (data_len > 0 && this->m_content_length == -1)
                    m_write_data = hex(data_len) + "\r\n" + m_write_data + "\r\n";
            };
            return pybind11::cpp_function(wr, pybind11::arg("data"));
        }

        // Create start_response() callable: https://www.python.org/dev/peps/pep-3333/#the-start-response-callable
        pybind11::object create_start_response() {
            auto sr = [this](std::string& status, std::list<std::pair<std::string, std::string>>& headers,
                             pybind11::object& exc_info) {
                if (!exc_info.is_none() && this->m_response.header_sent()) {
                    pybind11::tuple exc = exc_info;
                    pybind11::object t = exc[0];
                    pybind11::object v = exc[1];
                    pybind11::object tb = exc[2];
                    std::cerr << "The WSGI app passed exc_info after sending headers to the client!\n";
                    PyErr_Restore(t.ptr(), v.ptr(), tb.ptr());
                    throw pybind11::error_already_set();
                }
                this->m_status = status;
                this->m_out_headers.clear();
                exc_info = pybind11::none();
                for (const auto& h : headers) {
                    if (boost::algorithm::iequals(h.first, "Content-Length")) {
                        try {
                            this->m_content_length = stoll(h.second);
                        } catch (const std::logic_error&) {
                        }
                    }
                    this->m_out_headers.emplace_back(h.first, h.second);
                }
                if (this->m_content_length == -1) {
                    // If a WSGI app does not provide Content-Length header (e.g. Django)
                    // we use Transfer-Encoding: chunked
                    this->m_out_headers.emplace_back("Transfer-Encoding", "chunked");
                }
                return this->m_write;
            };
            return pybind11::cpp_function(sr,
                                          pybind11::arg("status"), pybind11::arg("headers"),
                                          pybind11::arg("exc_info") = pybind11::none());
        }

        void prepare_environ() {
            m_environ["REQUEST_METHOD"] = m_request.method;
            m_environ["SCRIPT_NAME"] = "";
            std::pair<std::string, std::string> path_and_query = split_path(m_request.path);
            m_environ["PATH_INFO"] = path_and_query.first;
            m_environ["QUERY_STRING"] = path_and_query.second;
            m_environ["CONTENT_TYPE"] = m_request.get_header("Content-Type");
            m_environ["CONTENT_LENGTH"] = m_request.get_header("Content-Length");
            m_environ["SERVER_NAME"] = m_host_name;
            m_environ["SERVER_PORT"] = std::to_string(m_local_port);
            m_environ["SERVER_PROTOCOL"] = m_request.http_version;
            m_environ["REMOTE_ADDR"] = m_request.remote_address();
            m_environ["REMOTE_HOST"] = m_request.remote_address();
            m_environ["REMOTE_PORT"] = std::to_string(m_request.remote_port());
            for (const auto& header : m_request.headers) {
                std::string env_header = header.first;
                transform_header(env_header);
                if (env_header == "HTTP_CONTENT_TYPE" || env_header == "HTTP_CONTENT_LENGTH")
                    continue;
                // Headers are already checked for duplicates during parsing
                m_environ[env_header.c_str()] = header.second;
            }
            m_environ["wsgi.version"] = pybind11::make_tuple(1, 0);
            m_environ["wsgi.url_scheme"] = m_url_scheme;
            m_environ["wsgi.input"] = InputStream<conn_t>{m_request.connection()};
            m_environ["wsgi.errors"] = ErrorStream();
            m_environ["wsgi.file_wrapper"] = file_wrapper();
            m_environ["wsgi.multithread"] = m_multithread;
            m_environ["wsgi.multiprocess"] = false;
            m_environ["wsgi.run_once"] = false;
        }

        boost::system::error_code send_header() {
            boost::system::error_code ec;
            ec = m_response.send_header(m_status, m_out_headers);
            if (!(ec || m_write_data.empty()))
                ec = m_response.send_data(m_write_data);
            return ec;
        }

        void send_iterable(Iterable& iterable) {
            boost::system::error_code ec;
            pybind11::object iterator = iterable.attr("__iter__")();
            while (true) {
                try {
                    pybind11::bytes py_chunk = iterator.attr("__next__")();
                    std::string chunk{py_chunk};
                    // Releasing GIL around async operations not only allows other Python threads to run
                    // but also allows io_service to safely transfer control to another coroutine
                    // that may acquire GIL again.
                    // I found this scheme by accident and if we do not release GIL at this point
                    // Python will crash!
                    pybind11::gil_scoped_release release_gil;
                    if (!m_response.header_sent()) {
                        ec = send_header();
                        if (ec)
                            break;
                    }
                    if (m_content_length == -1) {
                        size_t length = chunk.length();
                        // Skip 0-length chunks, if any
                        if (length == 0)
                            continue;
                        chunk = hex(length) + "\r\n" + chunk + "\r\n";
                    }
                    ec = m_response.send_data(chunk);
                    if (ec)
                        break;
                } catch (pybind11::error_already_set& ex) {
                    if (ex.matches(PyExc_StopIteration)) {
                        ex.clear();
                        // Avoid unnecessary GIL manipulation
                        if (!m_response.header_sent() || m_content_length == -1) {
                            pybind11::gil_scoped_release release_gil;
                            if (!m_response.header_sent())
                                ec = send_header();
                            if (!ec && m_content_length == -1)
                                m_response.send_data("0\r\n\r\n");
                        }
                        break;
                    }
                    throw;
                }
            }
        }

    public:
        WsgiRequestHandler(req_t& request, resp_t& response, pybind11::object& app,
                           std::string& scheme, std::string& host, unsigned short local_port, bool multithread)
            : BaseRequestHandler<req_t, resp_t>(request, response)
            , m_app{app}
            , m_url_scheme{scheme}
            , m_host_name{host}
            , m_local_port{local_port}
            , m_multithread{multithread} {
            m_write = create_write();
            m_start_response = create_start_response();
        }

        // Handle request
        void handle() {
            if (m_app.is_none())
                throw std::runtime_error("A WSGI application is not set!");
            prepare_environ();
            Iterable iterable{m_app(m_environ, m_start_response)};
            send_iterable(iterable);
        }
    };
} // namespace wsgi_boost
