#pragma once

#include "utils.h"

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>

#include <iostream>
#include <memory>
#include <string>

namespace wsgi_boost {
    // Represents a http connection to a client
    template<class socket_p>
    class Connection {
    private:
        socket_p m_socket;
        boost::asio::streambuf m_istreambuf;
        boost::asio::streambuf m_ostreambuf;
        boost::asio::deadline_timer m_timer;
        unsigned int m_header_timeout;
        unsigned int m_content_timeout;
        long long m_bytes_left = -1;
        long long m_content_length = -1;
        boost::asio::yield_context m_yc;

        void set_timeout(unsigned int timeout) {
            m_timer.expires_from_now(boost::posix_time::seconds(timeout));
            m_timer.async_wait([this](const boost::system::error_code& ec) {
                if (ec != boost::asio::error::operation_aborted) {
                    m_socket->lowest_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_both);
                    m_socket->lowest_layer().close();
                }
            });
        }

    public:
        Connection(const Connection&) = delete;
        Connection& operator=(const Connection&) = delete;

        Connection(socket_p socket, boost::asio::yield_context yc,
                   unsigned int header_timeout, unsigned int content_timeout)
            : m_socket{socket}
            , m_timer{socket->get_executor()}
            , m_yc{yc}
            , m_header_timeout{header_timeout}
            , m_content_timeout{content_timeout} {}

        // Read HTTP header
        boost::system::error_code read_header(std::string& header) {
            boost::system::error_code ec;
            set_timeout(m_header_timeout);
            size_t bytes_read = boost::asio::async_read_until(*m_socket, m_istreambuf, "\r\n\r\n", m_yc[ec]);
            m_timer.cancel();
            if (!ec) {
                auto in_buffer = m_istreambuf.data();
                auto buffers_begin = boost::asio::buffers_begin(in_buffer);
                header.assign(buffers_begin, buffers_begin + bytes_read);
                m_istreambuf.consume(bytes_read);
            }
            return ec;
        }

        // Read data from a socket into the input buffer
        bool read_into_buffer(long long length, bool async = false) {
            if (m_bytes_left <= 0)
                return false;
            size_t residual_bytes = m_istreambuf.size();
            size_t size;
            if (length >= 0 && length > residual_bytes)
                size = std::min(m_bytes_left, length - (long long) residual_bytes);
            else if (length >= 0 && length <= residual_bytes)
                return true;
            else
                size = m_bytes_left - residual_bytes;
            boost::system::error_code ec;
            size_t bytes_read;
            if (async) {
                set_timeout(m_content_timeout); // Timer works only for async operations
                bytes_read = boost::asio::async_read(*m_socket, m_istreambuf,
                                                     boost::asio::transfer_exactly(size), m_yc[ec]);
                m_timer.cancel();
            } else {
                // Read/write operations executed from inside Python must be syncronous!
                bytes_read = boost::asio::read(*m_socket, m_istreambuf, boost::asio::transfer_exactly(size), ec);
            }
            if (!ec || (ec && bytes_read > 0))
                return true;
            return false;
        }

        // Read line including a new line charachter
        std::string read_line() {
            std::istream is{&m_istreambuf};
            std::string line;
            while (true) {
                std::string temp;
                std::getline(is, temp);
                line += temp;
                m_bytes_left -= temp.length();
                if (!is.eof()) {
                    line += '\n';
                    --m_bytes_left;
                    break;
                }
                if (read_into_buffer(std::min(128LL, m_bytes_left)))
                    is.clear();
                else
                    break;
            }
            return line;
        }

        // Read a specified number of bytes or all data left
        bool read_bytes(std::string& data, long long length) {
            bool result = read_into_buffer(length);
            if (result) {
                long long size;
                if (length > 0)
                    size = std::min(length, m_bytes_left);
                else
                    size = m_bytes_left;
                auto in_buffer = m_istreambuf.data();
                auto buffers_begin = boost::asio::buffers_begin(in_buffer);
                data.assign(buffers_begin, buffers_begin + size);
                m_istreambuf.consume(size);
                m_bytes_left -= size;
            }
            return result;
        }

        // Set content length to control reading POST data
        void post_content_length(long long cl) {
            m_bytes_left = cl;
            m_content_length = cl;
        }

        // Get POST content length
        long long post_content_length() const { return m_content_length; }

        // Save data to the output buffer
        void buffer_output(const std::string& data) {
            size_t length = data.length();
            auto out_buffers = m_ostreambuf.prepare(length);
            auto buffers_begin = boost::asio::buffers_begin(out_buffers);
            std::copy(data.begin(), data.end(), buffers_begin);
            m_ostreambuf.commit(length);
        }

        // Send all output data to the client
        boost::system::error_code flush() {
            boost::system::error_code ec;
            set_timeout(m_content_timeout);
            boost::asio::async_write(*m_socket, m_ostreambuf, m_yc[ec]);
            m_timer.cancel();
            return ec;
        }

        // Get asio socket pointer
        socket_p socket() const { return m_socket; }
    };

    // Wraps Connection instance to provide Python file-like object for wsgi.input
    template<class conn_t>
    class InputStream {
    private:
        conn_t& m_connection;

        std::string read_(long long size) {
            pybind11::gil_scoped_release release_gil;
            std::string data;
            if (m_connection.read_bytes(data, size))
                return data;
            return std::string();
        }

        std::string readline_() {
            pybind11::gil_scoped_release release_gil;
            return m_connection.read_line();
        }

    public:
        explicit InputStream(conn_t& conn)
            : m_connection{conn} {}

        // Read data from input content
        pybind11::bytes read(long long size) {
            return read_(size);
        }

        // Read a line from input content
        pybind11::bytes readline(long long size = -1) {
            // size argument is ignored
            return readline_();
        }

        // Read a list of lines from input content
        pybind11::list readlines(long long sizehint) {
            pybind11::list listing;
            std::string line;
            long long total_length = 0;
            while (!(line = readline()).empty()) {
                listing.append(pybind11::bytes(line));
                if (sizehint >= 0) {
                    total_length += line.length();
                    if (total_length > sizehint)
                        break;
                }
            }
            return listing;
        }

        // Return Python iterator
        InputStream* iter() { return this; }

        // Iterate the iterator
        pybind11::bytes next() {
            std::string line = readline_();
            if (line.empty())
                throw pybind11::stop_iteration();
            return line;
        }
    };
} // namespace wsgi_boost
