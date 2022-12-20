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

#include "version.hpp"
#include "fmt/format.h"
#include "mamba/core/util_string.hpp"
#include "mamba/core/thread_utils.hpp"
#include <iostream>
#include <poll.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <map>
#include <vector>
#include <fstream>
#include <sstream>

#define BUFSIZE 8096

#define SERVER_NAME "micromamba"
#define SERVER_VERSION UMAMBA_VERSION_STRING

namespace microserver
{
    class Request
    {
    public:
        Request()
        {
        }

        std::string method;
        std::string path;
        std::string params;
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

        void send(std::string str)
        {
            body << str;
        };
        void send(const char* str)
        {
            body << str;
        };
    };

    class server_exception : public std::exception
    {
    public:
        server_exception()
            : pMessage("")
        {
        }

        server_exception(const char* pStr)
            : pMessage(pStr)
        {
        }

        const char* what() const throw()
        {
            return pMessage;
        }

    private:
        const char* pMessage;
    };

    std::map<std::string, std::string> mime;

    using callback_function_t = std::function<void(Request*, Response*)>;

    struct Route
    {
        std::string path;
        std::string method;
        callback_function_t callback;
        std::string params;
    };

    std::vector<Route> ROUTES;

    class Server
    {
    public:
        void get(std::string, callback_function_t);
        void post(std::string, callback_function_t);
        void all(std::string, callback_function_t);

        bool start(int port, const std::string& host);
        bool start(int port);
        bool start();

    private:
        void main_loop(int port);
        void parse_headers(char*, Request*, Response*);
        bool match_route(Request*, Response*);
        std::string trim(const std::string& s);
    };

    std::string Server::trim(const std::string& s)
    {
        return std::string(mamba::strip(s));
    }

    void Server::parse_headers(char* headers, Request* req, Response* res)
    {
        // Parse request headers
        int i = 0;
        char* pch;
        for (pch = strtok(headers, "\n"); pch; pch = strtok(NULL, "\n"))
        {
            if (i++ == 0)
            {
                std::string line(pch);
                auto R = mamba::split(line, " ", 3);

                if (R.size() != 3)
                {
                    throw std::runtime_error("Header split returned wrong number");
                }

                req->method = R[0];
                req->path = R[1];

                size_t pos = req->path.find('?');

                // We have GET params here
                if (pos != std::string::npos)
                {
                    auto Q1 = mamba::split(req->path.substr(pos + 1), "&");

                    for (std::vector<std::string>::size_type q = 0; q < Q1.size(); q++)
                    {
                        auto Q2 = mamba::split(Q1[q], "=");

                        if (Q2.size() == 2)
                        {
                            req->query[Q2[0]] = Q2[1];
                        }
                    }

                    req->path = req->path.substr(0, pos);
                }
            }
            else
            {
                std::string line(pch);
                auto R = mamba::split(line, ": ", 2);

                if (R.size() == 2)
                {
                    req->headers[R[0]] = R[1];

                    // Yeah, cookies!
                    if (R[0] == "Cookie")
                    {
                        auto C1 = mamba::split(R[1], "; ");
                        for (std::vector<std::string>::size_type c = 0; c < C1.size(); c++)
                        {
                            auto C2 = mamba::split(C1[c], "=", 2);
                            req->cookies[C2[0]] = C2[1];
                        }
                    }
                }
            }
        }
    }

    void Server::get(std::string path, callback_function_t callback)
    {
        Route r = { path, "GET", callback };
        ROUTES.push_back(r);
    }

    void Server::post(std::string path, callback_function_t callback)
    {
        Route r = { path, "POST", callback };
        ROUTES.push_back(r);
    }

    void Server::all(std::string path, callback_function_t callback)
    {
        Route r = { path, "ALL", callback };
        ROUTES.push_back(r);
    }

    bool Server::match_route(Request* req, Response* res)
    {
        for (std::vector<Route>::size_type i = 0; i < ROUTES.size(); i++)
        {
            if (ROUTES[i].path == req->path
                && (ROUTES[i].method == req->method || ROUTES[i].method == "ALL"))
            {
                req->params = ROUTES[i].params;

                ROUTES[i].callback(req, res);

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
        std::cout << "Waiting for socket resulted in " << fds[0].revents << std::endl;
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

        if (::bind(sc, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) != 0)
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
                newsc = accept(sc, (struct sockaddr*) &cli_addr, &clilen);

                if (newsc < 0)
                {
                    throw microserver::server_exception("ERROR on accept");
                }

                // handle new connection
                Request req;
                Response res;

                static char headers[BUFSIZE + 1];
                long ret = read(newsc, headers, BUFSIZE);
                if (ret > 0 && ret < BUFSIZE)
                {
                    headers[ret] = 0;
                }
                else
                {
                    headers[0] = 0;
                }

                this->parse_headers(headers, &req, &res);

                if (!this->match_route(&req, &res))
                {
                    res.code = 404;
                    res.phrase = "Not Found";
                    res.type = "text/plain";
                    res.send("Not found");
                }

                std::stringstream buffer;
                std::string body = res.body.str();
                size_t body_len = body.size();

                // build http response
                buffer << fmt::format("HTTP/1.0 {} {}\r\n", res.code, res.phrase)
                       << fmt::format("Server: {} {}\r\n", SERVER_NAME, SERVER_VERSION)
                       << fmt::format("Date: {}\r\n", res.date)
                       << fmt::format("Content-Type: {}\r\n", res.type)
                       << fmt::format("Content-Length: {}\r\n", body_len)
                       // append extra crlf to indicate start of body
                       << "\r\n";

                std::string header_buffer = buffer.str();
                ssize_t t;
                t = write(newsc, header_buffer.c_str(), header_buffer.size());
                t = write(newsc, body.c_str(), body_len);
            }
        }
    }

    bool Server::start(int port, const std::string& host)
    {
        this->main_loop(port);
        return true;
    }

    bool Server::start(int port)
    {
        return this->start(port, "0.0.0.0");
    }

    bool Server::start()
    {
        return this->start(80);
    }
}
