// Server side C/C++ program to demonstrate Socket
// programming
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#define PORT 8080

#include "microserver.cpp"

#include <CLI/CLI.hpp>

int
run_server()
{
    microserver::Server xserver;
    xserver.get("/hello",
                [](microserver::Request* req, microserver::Response* res)
                { res->send("Hello World!"); });
    xserver.get("/solve",
                [](microserver::Request* req, microserver::Response* res)
                { res->send("Hello World!"); });
    xserver.start(1234);
    return 0;
}

void
set_server_command(CLI::App* subcom)
{
    subcom->callback(run_server);
}
