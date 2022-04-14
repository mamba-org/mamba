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

std::string
get_token_base(const std::string& host)
{
    mamba::URLHandler url_handler(host);

    std::string token_base = mamba::concat(
        (url_handler.scheme().empty() ? "https" : url_handler.scheme()), "://", url_handler.host());
    if (!url_handler.port().empty())
    {
        token_base.append(":");
        token_base.append(url_handler.port());
    }
    return token_base;
}

void
set_logout_command(CLI::App* subcom)
{
    static std::string host;
    subcom->add_option("host", host, "Host for the account");

    subcom->callback(
        []()
        {
            auto token_base = get_token_base(host);

            nlohmann::json auth_info;

            static auto path = mamba::env::home_directory() / ".mamba" / "auth";
            fs::path auth_file = path / "authentication.json";
            if (fs::exists(auth_file))
            {
                auto fi = mamba::open_ifstream(auth_file);
                fi >> auth_info;
            }

            auto it = auth_info.find(host);
            if (it != auth_info.end())
            {
                auth_info.erase(it);
                std::cout << "Logged out from " << token_base << std::endl;
            }
            else
            {
                std::cout << "You are not logged in to " << token_base << std::endl;
            }

            auto fo = mamba::open_ofstream(auth_file);
            fo << auth_info;
        });
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
            if (host.empty())
            {
                throw std::runtime_error("No host given.");
            }
            // remove any scheme etc.
            auto token_base = get_token_base(host);

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

            if (pass.empty() && token.empty())
            {
                throw std::runtime_error("No password or token given.");
            }

            if (!pass.empty())
            {
                auth_object["type"] = "BasicHTTPAuthentication";
                auth_object["password"] = mamba::encode_base64(mamba::strip(pass));
                auth_object["user"] = user;
            }
            else if (!token.empty())
            {
                auth_object["type"] = "CondaToken";
                auth_object["token"] = mamba::strip(token);
            }

            auth_info[token_base] = auth_object;

            auto out = mamba::open_ofstream(auth_file);
            out << auth_info.dump(4);
            std::cout << "Successfully stored login information" << std::endl;
        });
}

void
set_auth_command(CLI::App* subcom)
{
    CLI::App* login_cmd
        = subcom->add_subcommand("login", "Store login information for a specific host");
    set_login_command(login_cmd);

    CLI::App* logout_cmd
        = subcom->add_subcommand("logout", "Erase login information for a specific host");
    set_logout_command(logout_cmd);
}
