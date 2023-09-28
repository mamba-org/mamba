// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <string>

#include <CLI/App.hpp>

#include "mamba/core/environment.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/util.hpp"
#include "mamba/util/string.hpp"
#include "mamba/util/url.hpp"


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
    const auto url = mamba::util::URL::parse(host);

    std::string maybe_colon_and_port{};
    if (!url.port().empty())
    {
        maybe_colon_and_port.push_back(':');
        maybe_colon_and_port.append(url.port());
    }
    return mamba::util::concat(
        url.host(),
        maybe_colon_and_port,
        mamba::util::rstrip(url.pretty_path(), '/')
    );
}

void
set_logout_command(CLI::App* subcom)
{
    static std::string host;
    subcom->add_option("host", host, "Host for the account");

    static bool all;
    subcom->add_flag("--all", all, "Log out from all hosts");

    subcom->callback(
        []()
        {
            static auto path = mamba::env::home_directory() / ".mamba" / "auth";
            mamba::fs::u8path auth_file = path / "authentication.json";

            if (all)
            {
                if (mamba::fs::exists(auth_file))
                {
                    mamba::fs::remove(auth_file);
                }
                return 0;
            }

            nlohmann::json auth_info;

            try
            {
                if (mamba::fs::exists(auth_file))
                {
                    auto fi = mamba::open_ifstream(auth_file);
                    fi >> auth_info;
                }

                auto token_base = get_token_base(host);
                auto it = auth_info.find(token_base);
                if (it != auth_info.end())
                {
                    auth_info.erase(it);
                    std::cout << "Logged out from " << token_base << std::endl;
                }
                else
                {
                    std::cout << "You are not logged in to " << token_base << std::endl;
                }
            }
            catch (std::exception& e)
            {
                LOG_ERROR << "Could not parse " << auth_file;
                LOG_ERROR << e.what();
                return 1;
            }

            auto fo = mamba::open_ofstream(auth_file);
            fo << auth_info;
            return 0;
        }
    );
}

void
set_login_command(CLI::App* subcom)
{
    static std::string user, pass, token, bearer, host;
    static bool pass_stdin = false;
    static bool token_stdin = false;
    static bool bearer_stdin = false;
    subcom->add_option("-p,--password", pass, "Password for account");
    subcom->add_option("-u,--username", user, "User name for the account");
    subcom->add_option("-t,--token", token, "Token for the account");
    subcom->add_option("-b,--bearer", bearer, "Bearer token for the account");
    subcom->add_flag("--password-stdin", pass_stdin, "Read password from stdin");
    subcom->add_flag("--token-stdin", token_stdin, "Read token from stdin");
    subcom->add_flag("--bearer-stdin", bearer_stdin, "Read bearer token from stdin");
    subcom->add_option(
        "host",
        host,
        "Host for the account. The scheme (e.g. https://) is ignored\n"
        "but not the port (optional) nor the channel (optional)."
    );

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
            {
                pass = read_stdin();
            }

            if (token_stdin)
            {
                token = read_stdin();
            }

            if (bearer_stdin)
            {
                bearer = read_stdin();
            }

            static auto path = mamba::env::home_directory() / ".mamba" / "auth";
            mamba::fs::create_directories(path);


            nlohmann::json auth_info;
            mamba::fs::u8path auth_file = path / "authentication.json";

            try
            {
                if (mamba::fs::exists(auth_file))
                {
                    auto fi = mamba::open_ifstream(auth_file);
                    fi >> auth_info;
                }
                else
                {
                    auth_info = nlohmann::json::object();
                }
                nlohmann::json auth_object = nlohmann::json::object();

                if (pass.empty() && token.empty() && bearer.empty())
                {
                    throw std::runtime_error("No password or token given.");
                }

                if (!pass.empty())
                {
                    auth_object["type"] = "BasicHTTPAuthentication";

                    auto pass_encoded = mamba::encode_base64(mamba::util::strip(pass));
                    if (!pass_encoded)
                    {
                        throw pass_encoded.error();
                    }

                    auth_object["password"] = pass_encoded.value();
                    auth_object["user"] = user;
                }
                else if (!token.empty())
                {
                    auth_object["type"] = "CondaToken";
                    auth_object["token"] = mamba::util::strip(token);
                }
                else if (!bearer.empty())
                {
                    auth_object["type"] = "BearerToken";
                    auth_object["token"] = mamba::util::strip(bearer);
                }

                auth_info[token_base] = auth_object;
            }
            catch (std::exception& e)
            {
                LOG_ERROR << "Could not modify " << auth_file;
                LOG_ERROR << e.what();
                return 1;
            }

            auto out = mamba::open_ofstream(auth_file);
            out << auth_info.dump(4);
            std::cout << "Successfully stored login information" << std::endl;
            return 0;
        }
    );
}

void
set_auth_command(CLI::App* subcom)
{
    CLI::App* login_cmd = subcom->add_subcommand("login", "Store login information for a specific host");
    set_login_command(login_cmd);

    CLI::App* logout_cmd = subcom->add_subcommand(
        "logout",
        "Erase login information for a specific host"
    );
    set_logout_command(logout_cmd);
}
