// Copyright (c) Alex Movsisyan
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
// associated documentation files (the 'Software'), to deal in the Software without restriction,
// including without limitation the rights to use, copy, modify, merge, publish, distribute,
// sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions: The above copyright notice and this
// permission notice shall be included in all copies or substantial portions of the Software. THE
// SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT
// LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. original
// source: https://github.com/konteck/wpp

#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <vector>

#include <arpa/inet.h>
#include <fmt/format.h>
#include <limits.h>
#include <netinet/in.h>
#include <poll.h>
#include <spdlog/logger.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "mamba/core/output.hpp"
#include "mamba/core/thread_utils.hpp"
#include "mamba/util/string.hpp"

#include "version.hpp"

#define BUFSIZE 8096

#define SERVER_NAME "micromamba"
#define SERVER_VERSION UMAMBA_VERSION_STRING

namespace microserver
{
    struct Request
    {
        std::string method;
        std::string path;
        std::string params;
        std::string body;
        std::map<std::string, std::string> headers;
        std::map<std::string, std::string> query;
        std::map<std::string, std::string> cookies;
    };

    class Response
    {
    public:

        Response()
        {
            code = 200;
            phrase = "OK";
            type = "text/html";
            body << "";

            // set current date and time for "Date: " header
            char buffer[100];
            time_t now = time(0);
            struct tm tstruct = *gmtime(&now);
            strftime(buffer, sizeof(buffer), "%a, %d %b %Y %H:%M:%S %Z", &tstruct);
            date = buffer;
        }

        int code;
        std::string phrase;
        std::string type;
        std::string date;
        std::stringstream body;

        void send(std::string_view str)
        {
            body << str;
        }
    };

    class server_exception : public std::runtime_error
    {
        using std::runtime_error::runtime_error;
    };

    using callback_function_t = std::function<void(const Request&, Response&)>;

    struct Route
    {
        std::string path;
        std::string method;
        callback_function_t callback;
        std::string params;
    };

    class Server
    {
    public:

        Server(const spdlog::logger& logger)
            : m_logger(logger)
        {
        }

        void get(std::string, callback_function_t);
        void post(std::string, callback_function_t);
        void all(std::string, callback_function_t);

        bool start(int port = 80);

    private:

        void main_loop(int port);
        std::pair<std::string, std::string> parse_header(std::string_view);
        void parse_headers(const std::string&, Request&, Response&);
        bool match_route(Request&, Response&);
        std::vector<Route> m_routes;
        spdlog::logger m_logger;
    };

    std::pair<std::string, std::string> Server::parse_header(std::string_view header)
    {
        assert(header.size() >= 2);
        assert(header[header.size() - 1] == '\n' && header[header.size() - 2] == '\r');

        auto colon_idx = header.find(':');
        if (colon_idx != std::string_view::npos)
        {
            std::string_view key, value;
            key = header.substr(0, colon_idx);
            colon_idx++;
            // remove spaces
            while (std::isspace(header[colon_idx]))
            {
                ++colon_idx;
            }

            // remove \r\n header ending
            value = header.substr(colon_idx, header.size() - colon_idx - 2);
            // http headers are case insensitive!
            std::string lkey = mamba::util::to_lower(key);

            return std::make_pair(lkey, std::string(value));
        }
        return std::make_pair(std::string(), std::string(header));
    }

    void Server::parse_headers(const std::string& headers, Request& req, Response&)
    {
        // Parse request headers
        int i = 0;
        // parse headers into lines delimited by newlines
        std::size_t delim_pos = headers.find_first_of("\n");
        std::string line = headers.substr(0, delim_pos + 1);

        while (line.size() > 2 && i < 10)
        {
            if (i++ == 0)
            {
                auto R = mamba::util::split(line, " ", 3);

                if (R.size() != 3)
                {
                    throw server_exception("Header split returned wrong number");
                }

                req.method = R[0];
                req.path = R[1];

                size_t pos = req.path.find('?');

                // We have GET params here
                if (pos != std::string::npos)
                {
                    auto Q1 = mamba::util::split(req.path.substr(pos + 1), "&");

                    for (std::vector<std::string>::size_type q = 0; q < Q1.size(); q++)
                    {
                        auto Q2 = mamba::util::split(Q1[q], "=");

                        if (Q2.size() == 2)
                        {
                            req.query[Q2[0]] = Q2[1];
                        }
                    }

                    req.path = req.path.substr(0, pos);
                }
            }
            else
            {
                req.headers.insert(parse_header(line));
            }

            std::size_t prev_pos = delim_pos;
            delim_pos = headers.find_first_of("\n", prev_pos + 1);
            line = headers.substr(prev_pos + 1, delim_pos - prev_pos);
        }
    }

    void Server::get(std::string path, callback_function_t callback)
    {
        Route r = { path, "GET", callback, "" };
        m_routes.push_back(r);
    }

    void Server::post(std::string path, callback_function_t callback)
    {
        Route r = { path, "POST", callback, "" };
        m_routes.push_back(r);
    }

    void Server::all(std::string path, callback_function_t callback)
    {
        Route r = { path, "ALL", callback, "" };
        m_routes.push_back(r);
    }

    bool Server::match_route(Request& req, Response& res)
    {
        for (std::vector<Route>::size_type i = 0; i < m_routes.size(); i++)
        {
            if (m_routes[i].path == req.path
                && (m_routes[i].method == req.method || m_routes[i].method == "ALL"))
            {
                req.params = m_routes[i].params;

                try
                {
                    m_routes[i].callback(req, res);
                }
                catch (const std::exception& e)
                {
                    m_logger.error("Error in callback: {}", e.what());
                    res.code = 500;
                    res.body << fmt::format("Internal server error. {}", e.what());
                }

                return true;
            }
        }

        return false;
    }

    bool wait_for_socket(int fd)
    {
        struct pollfd fds[1];
        int ret;

        fds[0].fd = fd;
        fds[0].events = POLLIN;

        ret = poll(fds, 1, 500);

        if (ret == -1)
        {
            throw microserver::server_exception("ERROR on poll");
        }

        // There is data to read
        return fds[0].revents & POLLIN;
    }

    void Server::main_loop(int port)
    {
        int newsc;

        int sc = socket(AF_INET, SOCK_STREAM, 0);

        if (sc < 0)
        {
            throw microserver::server_exception("ERROR opening socket");
        }

        struct sockaddr_in serv_addr, cli_addr;
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = INADDR_ANY;
        serv_addr.sin_port = htons(port);

        // allow faster reuse of the address
        int optval = 1;
        setsockopt(sc, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

        if (::bind(sc, reinterpret_cast<struct sockaddr*>(&serv_addr), sizeof(serv_addr)) != 0)
        {
            throw microserver::server_exception("ERROR on binding");
        }

        listen(sc, 5);

        socklen_t clilen;
        clilen = sizeof(cli_addr);

        while (!mamba::is_sig_interrupted())
        {
            bool have_data = wait_for_socket(sc);

            if (have_data)
            {
                std::chrono::time_point request_start = std::chrono::high_resolution_clock::now();
                newsc = accept(sc, reinterpret_cast<struct sockaddr*>(&cli_addr), &clilen);

                if (newsc < 0)
                {
                    throw microserver::server_exception("ERROR on accept");
                }

                // handle new connection
                Request req;
                Response res;

                static char buf[BUFSIZE + 1];
                std::string content;
                std::streamsize ret = read(newsc, buf, BUFSIZE);
                assert(ret >= 0);
                content = std::string(buf, static_cast<std::size_t>(ret));

                std::size_t header_end = content.find("\r\n\r\n");
                if (header_end == std::string::npos)
                {
                    throw microserver::server_exception("ERROR on parsing headers");
                }

                parse_headers(content, req, res);

                if (req.method == "POST")
                {
                    std::string body = content.substr(header_end + 4, BUFSIZE - header_end - 4);
                    std::streamsize content_length = stoll(req.headers["content-length"]);
                    if (content_length - static_cast<std::streamsize>(body.size()) > 0)
                    {
                        // read the rest of the data and add to body
                        std::streamsize remainder = content_length
                                                    - static_cast<std::streamsize>(body.size());
                        while (ret && remainder > 0)
                        {
                            std::streamsize read_ret = read(newsc, buf, BUFSIZE);
                            body += std::string(buf, static_cast<std::size_t>(read_ret));
                            remainder -= read_ret;
                        }
                    }
                    req.body = body;
                }

                if (!match_route(req, res))
                {
                    res.code = 404;
                    res.phrase = "Not Found";
                    res.type = "text/plain";
                    res.send("Not found");
                }

                std::stringstream buffer;
                std::string body = res.body.str();
                std::size_t body_len = body.size();

                // build http response
                buffer << fmt::format("HTTP/1.0 {} {}\r\n", res.code, res.phrase)
                       << fmt::format("Server: {} {}\r\n", SERVER_NAME, SERVER_VERSION)
                       << fmt::format("Date: {}\r\n", res.date)
                       << fmt::format("Content-Type: {}\r\n", res.type)
                       << fmt::format("Content-Length: {}\r\n", body_len)
                       // append extra crlf to indicate start of body
                       << "\r\n";

                char addrbuf[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &cli_addr.sin_addr, addrbuf, sizeof(addrbuf));
                uint16_t used_port = htons(cli_addr.sin_port);
                std::chrono::time_point request_end = std::chrono::high_resolution_clock::now();

                m_logger.info(
                    "{}:{} - {} {} {} (took {} ms)",
                    addrbuf,
                    used_port,
                    req.method,
                    req.path,
                    fmt::styled(
                        res.code,
                        fmt::fg(res.code < 300 ? fmt::terminal_color::green : fmt::terminal_color::red)
                    ),
                    std::chrono::duration_cast<std::chrono::milliseconds>(request_end - request_start)
                        .count()
                );

                std::string header_buffer = buffer.str();
                auto written = write(newsc, header_buffer.c_str(), header_buffer.size());
                if (written != static_cast<std::streamsize>(header_buffer.size()))
                {
                    LOG_ERROR << "Could not write to socket " << strerror(errno);
                    continue;
                }
                written = write(newsc, body.c_str(), body_len);
                if (written != static_cast<std::streamsize>(body_len))
                {
                    LOG_ERROR << "Could not write to socket " << strerror(errno);
                    continue;
                }
            }
        }
    }

    bool Server::start(int port)
    {
        this->main_loop(port);
        return true;
    }
}
