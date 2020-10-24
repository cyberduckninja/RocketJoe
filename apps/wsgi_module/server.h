#pragma once

#include "io_service_pool.h"
#include "request_handlers.h"

#include <boost/asio/spawn.hpp>

#include <atomic>
#include <csignal>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace wsgi_boost {
    // Base server class template for both HTTP and HTTPS
    template<class socket_p>
    class BaseServer {
        typedef Connection<socket_p> connection_t;
        typedef Request<connection_t> request_t;
        typedef Response<connection_t> response_t;

    protected:
        io_service_pool m_io_service_pool;
        boost::asio::ip::tcp::acceptor m_acceptor;
        std::string m_address;
        unsigned short m_port;
        boost::asio::signal_set m_signals;
        std::vector<std::pair<boost::regex, std::string>> m_static_routes;
        pybind11::object m_app;
        std::atomic_bool m_is_running;

        void init_acceptor(boost::asio::ip::tcp::acceptor& acceptor, unsigned int port) {
            boost::asio::ip::tcp::endpoint endpoint;
            if (!m_address.empty()) {
                boost::asio::ip::tcp::resolver resolver(*m_io_service_pool.get_io_service());
                try {
                    endpoint = *resolver.resolve({m_address, std::to_string(port)});
                } catch (const std::exception&) {
                    throw std::runtime_error("Unable to resolve IP address and port " + m_address + ":" + std::to_string(port) + "!");
                }
            } else {
                endpoint = boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port);
            }
            acceptor.open(endpoint.protocol());
            acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(reuse_address));
            acceptor.bind(endpoint);
            acceptor.listen();
        }

        // Pybind11 cannot expose abstract C++ classes
        virtual void accept(){};

        virtual void process_request(socket_p socket) {
            // A stackful coroutine is needed here to correctly implement keep-alive
            // in case if the number of concurent requests is greater than
            // the number of server threads.
            // Without the coroutine when all threads are busy the next request
            // hangs in limbo and causes io_service to crash.
            //
            // Because our io_service runs in one thread we work on an implicit strand
            // with no special syncronization measures. This also allows us to safely
            // toggle Python GIL around async operations
            // without the risk of crashing Python interpreter.
            boost::asio::spawn(socket->get_executor(), [this, socket](boost::asio::yield_context yc) {
                connection_t connection{socket, yc, header_timeout, content_timeout};
                request_t request{connection};
                response_t response{connection};
                parse_result res = request.parse_header();
                if (!res) {
                    check_static_route(request);
                    response.http_version = request.http_version;
                    response.keep_alive = request.keep_alive();
                    handle_request(request, response);
                } else if (res == BAD_REQUEST) {
                    response.send_mesage("400 Bad Request", "Malformed HTTP request!");
                } else if (res == LENGTH_REQUIRED) {
                    response.send_mesage("411 Length Required", "Content-Length header is missing!");
                } else {
                    return;
                }
                // Send all remaining data from the output buffer and re-use the socket
                // for the next request if this is a keep-alive session.
                if (response.keep_alive)
                    process_request(socket);
            });
        }

        void check_static_route(request_t& request) {
            for (const auto& route : m_static_routes) {
                if (boost::regex_search(request.path, route.first)) {
                    request.path_regex = route.first;
                    request.content_dir = route.second;
                    break;
                }
            }
        }

        void process_error(response_t& response, const std::exception& ex, const std::string& error_msg) const {
            std::cerr << error_msg << ": " << ex.what() << '\n';
            if (!response.header_sent())
                response.send_html("500 Internal Server Error", "Error 500", "Internal Server Error", error_msg);
            else
                // Do not reuse a socket on a fatal error
                response.keep_alive = false;
        }

        void handle_request(request_t& request, response_t& response) {
            if (request.content_dir.empty()) {
                // Try to buffer the first 128KB of request data
                if (request.connection().post_content_length() > 0) {
                    boost::system::error_code ec;
                    if (request.check_header("Expect", "100-continue"))
                        // Send only plain status string with no headers
                        ec = response.send_data("HTTP/1.1 100 Continue\r\n\r\n");
                    if (ec || !request.connection().read_into_buffer(std::min(request.connection().post_content_length(), 131072LL), true)) {
                        std::cerr << "Unable to buffer POST/PUT/PATCH data from " << request.remote_address() << ':' << request.remote_port() << '\n';
                        response.keep_alive = false;
                        return;
                    }
                }
                pybind11::gil_scoped_acquire acquire_gil;
                WsgiRequestHandler<connection_t, request_t, response_t> handler{
                    request, response, m_app, url_scheme, host_name, m_port, m_io_service_pool.size() > 1};
                try {
                    handler.handle();
                } catch (pybind11::error_already_set& ex) {
                    ex.restore();
                    PyErr_Print();
                    ex.clear();
                    pybind11::gil_scoped_release release_gil;
                    process_error(response, ex, "Python error while processing a WSGI request");
                } catch (const std::exception& ex) {
                    pybind11::gil_scoped_release release_gil;
                    process_error(response, ex, "General error while processing a WSGI request");
                }
            } else {
                StaticRequestHandler<request_t, response_t> handler{request, response, static_cache_control, use_gzip};
                try {
                    handler.handle();
                } catch (const std::exception& ex) {
                    process_error(response, ex, "Error while processing a static request");
                }
            }
        }

    public:
        unsigned int header_timeout = 5;
        unsigned int content_timeout = 300;
        bool reuse_address = true;
        std::string url_scheme = "http";
        std::string host_name;
        bool use_gzip = true;
        std::string static_cache_control = "public, max-age=3600";

        BaseServer(const BaseServer&) = delete;
        BaseServer& operator=(const BaseServer&) = delete;
        virtual ~BaseServer() {}

        BaseServer(std::string address, unsigned short port, unsigned int threads)
            : m_address{address}
            , m_port{port}
            , m_io_service_pool{threads}
            ,
            // Pybind11 does not initialize objects to None by default, unlike Boost.Python!
            m_app{pybind11::none()}
            , m_acceptor{*m_io_service_pool.get_io_service()}
            , m_signals{*m_io_service_pool.get_io_service()} {
            m_is_running.store(false);
            m_signals.add(SIGINT);
            m_signals.add(SIGTERM);
#if defined(SIGQUIT)
            m_signals.add(SIGQUIT);
#endif
        }

        // Add a path to static content
        void add_static_route(std::string path, std::string content_dir) {
            m_static_routes.emplace_back(boost::regex(path, boost::regex_constants::icase), content_dir);
        }

        // Set WSGI application
        void set_app(pybind11::object app) {
            if (is_running())
                throw std::runtime_error("Cannot set a WSGI app while the server is running!");
            m_app = app;
        }

        // Start handling HTTP requests
        virtual void start() {
            if (!is_running()) {
                pybind11::gil_scoped_release release_gil;
                m_io_service_pool.reset();
                init_acceptor(m_acceptor, m_port);
                if (host_name.empty())
                    host_name = boost::asio::ip::host_name();
                accept();
                m_signals.async_wait([this](boost::system::error_code, int) { stop(); });
                std::cout << "WsgiBoost server is starting on " << host_name << ':' << m_port << " with " << m_io_service_pool.size() << " thread(s)\n";
                std::cout << "Press Ctrl+C to stop it.\n";
                m_is_running.store(true);
                m_io_service_pool.run();
                m_is_running.store(false);
                std::cout << "WsgiBoost server stopped.\n";
            } else {
                std::cerr << "The server is already running!\n";
            }
        }

        // Stop handling http requests
        virtual void stop() {
            if (is_running()) {
                m_acceptor.close();
                m_io_service_pool.stop();
                m_signals.cancel();
            } else {
                std::cerr << "The server is not running!\n";
            }
        }

        // Check if the server is running
        bool is_running() const {
            return m_is_running.load();
        }
    };

    template<class socket_p>
    class HttpServer : public BaseServer<socket_p> {};

    typedef boost::asio::ip::tcp::socket socket_t;
    typedef std::shared_ptr<socket_t> socket_ptr;

    // HTTP server class
    template<>
    class HttpServer<socket_ptr> : public BaseServer<socket_ptr> {
    protected:
        void accept() {
            socket_ptr socket = std::make_shared<socket_t>(*m_io_service_pool.get_io_service());
            m_acceptor.async_accept(socket->lowest_layer(), [this, socket](const boost::system::error_code& ec) {
                if (ec != boost::asio::error::operation_aborted)
                    accept();
                if (!ec) {
                    socket->lowest_layer().set_option(boost::asio::ip::tcp::no_delay(true));
                    process_request(socket);
                }
            });
        }

    public:
        HttpServer(std::string address = "", unsigned short port = 8000, unsigned int threads = 0)
            : BaseServer<socket_ptr>(address, port, threads) {}
    };
} // namespace wsgi_boost
