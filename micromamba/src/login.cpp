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
            std::string token_base
                = mamba::concat((url_handler.scheme().empty() ? "https" : url_handler.scheme()),
                                "://",
                                url_handler.host());
            if (!url_handler.port().empty())
            {
                token_base.append(":");
                token_base.append(url_handler.port());
            }

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
                auth_object["type"] = "BasicHTTPAuthentication";
                auth_object["password"] = mamba::encode_base64(mamba::strip(pass));
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

            auth_info[token_base] = auth_object;

            auto out = mamba::open_ofstream(auth_file);
            out << auth_info.dump(4);
        });
}
