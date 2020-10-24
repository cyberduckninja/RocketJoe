#pragma once

#include "constants.h"

#include <boost/algorithm/string.hpp>
#include <boost/regex.hpp>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <cctype>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace wsgi_boost {
#pragma region functions

    // Converts POSIX time to HTTP header format
    inline std::string time_to_header(time_t posix_time) {
        char buffer[40];
        std::locale::global(std::locale("C"));
        std::strftime(buffer, sizeof(buffer), "%a, %d %b %Y %H:%M:%S GMT", std::gmtime(&posix_time));
        return std::string{buffer};
    }

    // Parses HTTP "time" headers to POSIX time
    inline time_t header_to_time(const std::string& time_string) {
        tm t;
#ifdef _WIN32
        std::istringstream ss{time_string};
        ss.imbue(std::locale("C"));
        ss >> std::get_time(&t, "%a, %d %b %Y %H:%M:%S GMT");
        if (ss.fail())
            return std::time(nullptr); // Return current time on a malformed date/time string
        return _mkgmtime(&t);
#else
        std::locale::global(std::locale("C"));
        char* res = strptime(time_string.c_str(), "%a, %d %b %Y %H:%M:%S GMT", &t);
        if (!res)
            return std::time(nullptr);
        return timegm(&t);
#endif
    }

    // Get current GMT time in HTTP header format
    inline std::string get_current_gmt_time() {
        return time_to_header(std::time(nullptr));
    }

    // Splits a full path into a path proper and a query string
    inline std::pair<std::string, std::string> split_path(const std::string& path) {
        size_t pos = path.find("?");
        if (pos != std::string::npos)
            return std::pair<std::string, std::string>{path.substr(0, pos), path.substr(pos + 1)};
        return std::pair<std::string, std::string>{path, ""};
    }

    // Transform a HTTP header to WSGI environ format HTTP_
    inline void transform_header(std::string& header) {
        for (auto& ch : header) {
            ch = std::toupper(ch);
            if (ch == '-')
                ch = '_';
        }
        header = "HTTP_" + header;
    }

    // Parse Range header
    inline std::pair<std::string, std::string> parse_range(std::string& requested_range) {
        std::pair<std::string, std::string> range;
        boost::regex range_regex{"^bytes=(\\d*)-(\\d*)$"};
        boost::smatch range_match;
        boost::regex_search(requested_range, range_match, range_regex);
        if (!range_match.empty()) {
            if (range_match[1].first != requested_range.end())
                range.first = std::string{range_match[1].first, range_match[1].second};
            if (range_match[2].first != requested_range.end())
                range.second = std::string{range_match[2].first, range_match[2].second};
        }
        return range;
    }

    // Get hexadecimal representation of an unisigned int number
    inline std::string hex(size_t u_int) {
        if (u_int == 0)
            return "0";
        std::string hex_repr;
        while (u_int > 0) {
            hex_repr = std::string(1, hex_chars[u_int % 16]) + hex_repr;
            u_int /= 16;
        }
        return hex_repr;
    }

    // Check if a file can be compressed by gzip
    inline bool is_compressable(const std::string& ext) {
        return std::find(compressables.begin(), compressables.end(), boost::to_lower_copy(ext)) != compressables.end();
    }

    // Determine mime type by file extension
    inline std::string get_mime(const std::string& ext) {
        auto it = mime_types.find(boost::to_lower_copy(ext));
        if (it == mime_types.end())
            return "application/octet-stream";
        return it->second;
    }
#pragma endregion

#pragma region classes

    // RAII implementation for auto-closing an iteraterable object passed from a WSGI application
    class Iterable {
    public:
        explicit Iterable(pybind11::object it)
            : m_iterable{it} {}

        ~Iterable() {
            if (pybind11::hasattr(m_iterable, "close"))
                m_iterable.attr("close")();
        }

        pybind11::object attr(const std::string& at) const {
            return m_iterable.attr(at.c_str());
        }

    private:
        pybind11::object m_iterable;
    };

    // wsgi.errors stream implementation
    struct ErrorStream {
        void write(std::string& msg) { std::cerr << msg; }

        void writelines(std::vector<std::string>& lines) {
            for (const auto& line : lines)
                std::cerr << line;
        }

        void flush() { std::cerr.flush(); }
    };

    // wsgi.file_wrapper implementation
    class file_wrapper {
    private:
        pybind11::object m_file;
        int m_block_size;

    public:
        file_wrapper()
            : m_file{pybind11::none()} {}

        file_wrapper* call(pybind11::object file, int block_size = 8192) {
            m_file = file;
            m_block_size = block_size;
            return this;
        }

        pybind11::bytes read(int size = -1) {
            if (size == -1)
                size = m_block_size;
            return m_file.attr("read")(size);
        }

        file_wrapper* iter() { return this; }

        pybind11::bytes next() {
            pybind11::bytes chunk = read();
            if ((std::string{chunk}).empty())
                throw pybind11::stop_iteration();
            return chunk;
        }

        void close() {
            if (pybind11::hasattr(m_file, "close"))
                m_file.attr("close")();
        }
    };
} // namespace wsgi_boost
#pragma endregion
