#pragma once

#include "server.h"

// This is needed to link against pre-built OpenSSL from https://slproweb.com/products/Win32OpenSSL.html
#if _MSC_VER && _MSC_VER >= 1900
FILE _iob[] = {*stdin, *stdout, *stderr};
extern "C" FILE* __cdecl __iob_func(void) { return _iob; }
#endif // end _MSC_VER

#include <boost/asio/ssl.hpp>

namespace wsgi_boost {
    template<class socket_p>
    class HttpsServer : public BaseServer<socket_p> {};

    typedef boost::asio::ssl::stream<boost::asio::ip::tcp::socket> ssl_socket_t;
    typedef std::shared_ptr<ssl_socket_t> ssl_socket_ptr;

    // HTTPS server class
    template<>
    class HttpsServer<ssl_socket_ptr> : public BaseServer<ssl_socket_ptr> {
    protected:
        boost::asio::ssl::context m_context;
        boost::asio::ip::tcp::acceptor m_redirector;

        void accept() {
            io_service_ptr io_service = m_io_service_pool.get_io_service();
            ssl_socket_ptr socket = std::make_shared<ssl_socket_t>(*io_service, m_context);
            m_acceptor.async_accept(socket->lowest_layer(),
                                    [this, io_service, socket](const boost::system::error_code& ec) {
                                        if (ec != boost::asio::error::operation_aborted)
                                            accept();
                                        if (!ec) {
                                            socket->lowest_layer().set_option(boost::asio::ip::tcp::no_delay(true));
                                            auto timer = std::make_shared<boost::asio::deadline_timer>(*io_service);
                                            timer->expires_from_now(boost::posix_time::seconds(header_timeout));
                                            timer->async_wait([socket](const boost::system::error_code& ec) {
                                                if (ec != boost::asio::error::operation_aborted) {
                                                    socket->lowest_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_both);
                                                    socket->lowest_layer().close();
                                                }
                                            });
                                            socket->async_handshake(boost::asio::ssl::stream_base::server,
                                                                    [this, socket, timer](boost::system::error_code ec) {
                                                                        timer->cancel();
                                                                        if (!ec)
                                                                            process_request(socket);
                                                                    });
                                        }
                                    });
        }

        void accept_redirect() {
            socket_ptr socket = std::make_shared<socket_t>(*m_io_service_pool.get_io_service());
            m_redirector.async_accept(*socket, [this, socket](const boost::system::error_code& ec) {
                if (ec != boost::asio::error::operation_aborted)
                    accept_redirect();
                if (!ec) {
                    socket->set_option(boost::asio::ip::tcp::no_delay(true));
                    boost::asio::spawn(socket->get_executor(), [this, socket](boost::asio::yield_context yc) {
                        Connection<socket_ptr> connection{socket, yc, header_timeout, content_timeout};
                        Request<Connection<socket_ptr>> request{connection};
                        Response<Connection<socket_ptr>> response{connection};
                        parse_result res = request.parse_header();
                        if (!res) {
                            std::string host = request.get_header("Host");
                            if (host.empty()) {
                                response.send_mesage("400 Bad Request", "'Host' header is required!");
                                return;
                            }
                            size_t pos = host.find(":");
                            if (pos != std::string::npos)
                                host = host.substr(0, pos);
                            std::string https_port;
                            if (m_port != 443)
                                https_port = ':' + std::to_string(m_port);
                            std::string location{"https://" + host + https_port + request.path};
                            std::string message{"Redirected to " + location};
                            response.http_version = request.http_version;
                            response.keep_alive = false;
                            out_headers_t out_headers;
                            out_headers.emplace_back("Location", location);
                            out_headers.emplace_back("Content-Length", std::to_string(message.length()));
                            out_headers.emplace_back("Content-Type", "text/plain");
                            response.send_header("301 Moved Permanently", out_headers);
                            response.send_data(message);
                        }
                    });
                }
            });
        }

    public:
        bool redirect_http = false;
        unsigned short redirect_http_port = 80;

        HttpsServer(std::string cert_chain, const std::string private_key, std::string dh = std::string(),
                    std::string address = std::string(), unsigned short port = 4443, unsigned int threads = 0)
            : // sslv23 is an universal option. Deprecated protocols need to be disabled by set_options()
            // More info: https://www.openssl.org/docs/man1.0.2/ssl/SSL_CTX_new.html
            m_context{boost::asio::ssl::context::sslv23_server}
            , m_redirector{*m_io_service_pool.get_io_service()}
            , BaseServer<ssl_socket_ptr>{address, port, threads} {
            url_scheme = "https";
            m_context.set_options(boost::asio::ssl::context::default_workarounds | boost::asio::ssl::context::no_sslv2 | boost::asio::ssl::context::no_sslv3 | boost::asio::ssl::context::single_dh_use);
            m_context.set_password_callback([](size_t, boost::asio::ssl::context::password_purpose) {
                const char* pass = std::getenv("HTTPS_KEY_PASS");
                if (pass)
                    return pass;
                return "";
            });
            m_context.use_certificate_chain_file(cert_chain);
            m_context.use_private_key_file(private_key, boost::asio::ssl::context::pem);
            if (!dh.empty())
                m_context.use_tmp_dh_file(dh);
        }

        void start() {
            if (!is_running() && redirect_http) {
                init_acceptor(m_redirector, redirect_http_port);
                accept_redirect();
            }
            BaseServer<ssl_socket_ptr>::start();
        }

        void stop() {
            if (is_running())
                m_redirector.close();
            BaseServer<ssl_socket_ptr>::stop();
        }
    };
} // namespace wsgi_boost
