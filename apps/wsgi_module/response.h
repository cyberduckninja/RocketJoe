#pragma once

#include "connection.h"
#include "utils.h"

#include <boost/format.hpp>
#include <boost/system/error_code.hpp>

#include <string>
#include <vector>

namespace wsgi_boost {
    typedef std::vector<std::pair<std::string, std::string>> out_headers_t;

    // HTTP response
    template<class conn_t>
    class Response {
    private:
        const std::string m_server_name = "WsgiBoost v." WSGI_BOOST_VERSION;
        conn_t& m_connection;
        bool m_header_sent = false;

    public:
        std::string http_version = "HTTP/1.1";
        bool keep_alive;

        Response(const Response&) = delete;
        Response& operator=(const Response&) = delete;

        explicit Response(conn_t& connection)
            : m_connection{connection} {}

        // Send HTTP header (status code + headers)
        boost::system::error_code send_header(const std::string& status, out_headers_t& headers) {
            m_connection.buffer_output(http_version + " " + status + "\r\n");
            headers.emplace_back("Server", m_server_name);
            headers.emplace_back("Date", get_current_gmt_time());
            if (keep_alive)
                headers.emplace_back("Connection", "keep-alive");
            else
                headers.emplace_back("Connection", "close");
            for (const auto& header : headers)
                m_connection.buffer_output(header.first + ": " + header.second + "\r\n");
            m_connection.buffer_output("\r\n");
            m_header_sent = true;
            return m_connection.flush();
        }

        // Send data to the client
        boost::system::error_code send_data(const std::string& data) {
            m_connection.buffer_output(data);
            return m_connection.flush();
        }

        // Send a plain text HTTP message to a client
        boost::system::error_code send_mesage(const std::string& status, const std::string& message = std::string()) {
            out_headers_t headers;
            headers.emplace_back("Content-Length", std::to_string(message.length()));
            if (!message.empty())
                headers.emplace_back("Content-Type", "text/plain");
            boost::system::error_code ec = send_header(status, headers);
            if (!ec)
                ec = send_data(message);
            return ec;
        }

        // Send a html HTTP message to a client
        boost::system::error_code send_html(const std::string& status, const std::string& title,
                                            const std::string& header, const std::string& text) {
            boost::format tpl{html_template};
            tpl % title % header % text;
            std::string html = tpl.str();
            out_headers_t headers;
            headers.emplace_back("Content-Type", "text/html");
            headers.emplace_back("Content-Length", std::to_string(html.length()));
            boost::system::error_code ec = send_header(status, headers);
            if (!ec)
                ec = send_data(html);
            return ec;
        }

        // Check if HTTP header has been sent
        bool header_sent() const { return m_header_sent; }
    };
} // namespace wsgi_boost
