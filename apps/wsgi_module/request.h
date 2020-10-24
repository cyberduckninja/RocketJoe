#pragma once

#include "connection.h"

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/asio.hpp>
#include <boost/functional/hash.hpp>
#include <boost/regex.hpp>

#include <cctype>
#include <string>
#include <unordered_map>

#define PARSE_OK 0
#define CONN_ERROR 1
#define BAD_REQUEST 2
#define LENGTH_REQUIRED 3

namespace wsgi_boost {
    typedef int parse_result;

    // 2 structs below are used for unordered_map with case-insensitive string keys

    struct iequal_to {
        bool operator()(const std::string& key1, const std::string& key2) const {
            return boost::algorithm::iequals(key1, key2);
        }
    };

    struct ihash {
        size_t operator()(const std::string& key) const {
            std::size_t seed = 0;
            for (auto& c : key)
                boost::hash_combine(seed, std::tolower(c));
            return seed;
        }
    };

    // HTTP request
    template<class conn_t>
    class Request {
    private:
        conn_t& m_connection;

    public:
        std::string method;
        std::string path;
        std::string http_version;
        std::unordered_map<std::string, std::string, ihash, iequal_to> headers;
        boost::regex path_regex;
        std::string content_dir;

        Request(const Request&) = delete;
        Request& operator=(const Request&) = delete;

        explicit Request(conn_t& connection)
            : m_connection{connection} {}

        // Parse HTTP request headers
        parse_result parse_header() {
            std::string header;
            boost::system::error_code ec = m_connection.read_header(header);
            if (ec)
                return CONN_ERROR;
            std::istringstream iss{header};
            std::string line;
            std::getline(iss, line);
            line.pop_back();
            std::vector<std::string> parts;
            boost::algorithm::split(parts, line,
                                    boost::algorithm::is_space(),
                                    boost::algorithm::token_compress_on);
            if (parts.size() != 3)
                return BAD_REQUEST;
            method = parts[0];
            path = parts[1];
            http_version = parts[2];
            while (true) {
                std::getline(iss, line);
                if (!iss.good())
                    break;
                line.pop_back();
                size_t pos = line.find(':');
                if (pos != std::string::npos) {
                    std::string name = boost::algorithm::trim_copy(line.substr(0, pos));
                    std::string value = boost::algorithm::trim_copy(line.substr(pos + 1));
                    if (headers.find(name) == headers.end())
                        headers.emplace(name, value);
                    else
                        headers[name] += ", " + value;
                }
            }
            if (method == "POST" || method == "PUT" || method == "PATCH") {
                try {
                    long long cl = stoll(get_header("Content-Length"));
                    m_connection.post_content_length(cl);
                } catch (const std::logic_error&) {
                    return LENGTH_REQUIRED;
                }
            } else {
                m_connection.post_content_length(-1);
            }
            return PARSE_OK;
        }

        // Check if a header contains a specific value
        bool check_header(const std::string& header, const std::string& value) const {
            auto it = headers.find(header);
            return (it != headers.end()) && boost::algorithm::icontains(it->second, value);
        }

        // Get header value or "" if the header is missing
        std::string get_header(const std::string& header) const {
            auto it = headers.find(header);
            return it != headers.end() ? it->second : std::string();
        }

        // Check if the connection is persistent (keep-alive)
        bool keep_alive() const {
            return check_header("Connection", "keep-alive") || (http_version == "HTTP/1.1" && !check_header("Connection", "close"));
        }

        // Get Connection object for this request
        conn_t& connection() const { return m_connection; }

        // Get remote endpoint address
        std::string remote_address() const { return m_connection.socket()->lowest_layer().remote_endpoint().address().to_string(); }

        // Get remote endpoint port
        unsigned short remote_port() const { return m_connection.socket()->lowest_layer().remote_endpoint().port(); }
    };
} // namespace wsgi_boost
