// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.
#include "common_options.hpp"

#include "mamba/api/configuration.hpp"
#include "mamba/api/list.hpp"
#include "mamba/core/util.hpp"
#include "mamba/core/url.hpp"
#include <openssl/evp.h>

std::string
read_stdin()
{
    std::size_t len;
    std::array<char, 1024> buffer;
    std::string result;

    while ((len = std::fread(buffer.data(), sizeof(char), buffer.size(), stdin)) > 0)
    {
        if (std::ferror(stdin) && !std::feof(stdin))
        {
            throw std::runtime_error("Reading from stdin failed.");
        }
        result.append(buffer.data(), len);
    }
    return result;
}

std::string
base64(const std::string& input)
{
    const auto pl = 4 * ((input.size() + 2) / 3);
    std::vector<unsigned char> output(pl + 1);
    const auto ol
        = EVP_EncodeBlock(output.data(), (const unsigned char*) input.data(), input.size());
    if (pl != ol)
    {
        std::cerr << "Whoops, encode predicted " << pl << " but we got " << ol << "\n";
    }
    return std::string((const char*) output.data());
}

std::string
decode64(const std::string& input)
{
    const auto pl = 3 * input.size() / 4 + 1;
    std::vector<unsigned char> output(pl);
    const auto ol
        = EVP_DecodeBlock(output.data(), (const unsigned char*) input.data(), input.size());
    if (pl != ol)
    {
        std::cerr << "Whoops, decode predicted " << pl << " but we got " << ol << "\n";
    }
    return std::string((const char*) output.data());
}

void
set_login_command(CLI::App* subcom)
{
    static std::string user, pass, token, host;
    static bool pass_stdin = false;
    static bool token_stdin = false;
    subcom->add_option("-p,--password", pass, "Password for account");
    subcom->add_option("-u,--username", user, "User name for the account");
    subcom->add_option("-t,--token", token, "Token for the account");
    subcom->add_flag("--password-stdin", pass_stdin, "Read password from stdin");
    subcom->add_flag("--token-stdin", token_stdin, "Read token from stdin");
    subcom->add_option("host", host, "Host for the account");

    subcom->callback(
        []()
        {
            // remove any scheme etc.
            mamba::URLHandler url_handler(host);
            host = url_handler.host();

            if (pass_stdin)
                pass = read_stdin();

            if (token_stdin)
                token = read_stdin();

            static auto path = mamba::env::home_directory() / ".mamba" / "auth";
            fs::create_directories(path);

            nlohmann::json auth_info;

            fs::path auth_file = path / "authentication.json";
            if (fs::exists(auth_file))
            {
                auto fi = mamba::open_ifstream(auth_file);
                fi >> auth_info;
            }
            else
            {
                auth_info = nlohmann::json::object();
            }
            nlohmann::json auth_object = nlohmann::json::object();

            if (!pass.empty())
            {
                pass = mamba::strip(pass);
                auth_object["type"] = "HTTPBaseAuth";
                auth_object["password"] = base64(pass);
            }
            if (!user.empty())
            {
                auth_object["user"] = user;
            }
            if (!token.empty())
            {
                auth_object["type"] = "CondaToken";
                auth_object["token"] = mamba::strip(token);
            }

            auth_info[host] = auth_object;

            auto out = mamba::open_ofstream(auth_file);
            out << auth_info.dump(4);
        });
}
