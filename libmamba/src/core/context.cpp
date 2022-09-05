// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/core/context.hpp"
#include "mamba/core/environment.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/thread_utils.hpp"
#include "mamba/core/util.hpp"
#include "mamba/core/url.hpp"
#include "mamba/core/execution.hpp"

#include "termcolor/termcolor.hpp"

#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/pattern_formatter.h"

namespace mamba
{
    class Logger : public spdlog::logger
    {
    public:
        Logger(const std::string& name, const std::string& pattern, const std::string& eol);

        void dump_backtrace_no_guards();
    };

    Logger::Logger(const std::string& name, const std::string& pattern, const std::string& eol)
        : spdlog::logger(name, std::make_shared<spdlog::sinks::stderr_color_sink_mt>())
    {
        auto f = std::make_unique<spdlog::pattern_formatter>(
            pattern, spdlog::pattern_time_type::local, eol);
        set_formatter(std::move(f));
    }

    void Logger::dump_backtrace_no_guards()
    {
        using spdlog::details::log_msg;
        if (tracer_.enabled())
        {
            tracer_.foreach_pop(
                [this](const log_msg& msg)
                {
                    if (this->should_log(msg.level))
                        this->sink_it_(msg);
                });
        }
    }

    spdlog::level::level_enum convert_log_level(log_level l)
    {
        return static_cast<spdlog::level::level_enum>(l);
    }

    Context::Context()
    {
        MainExecutor::instance().on_close(tasksync.synchronized([this] { logger->flush(); }));

        on_ci = bool(env::get("CI"));
        root_prefix = env::get("MAMBA_ROOT_PREFIX").value_or("");
        conda_prefix = root_prefix;

        envs_dirs = { root_prefix / "envs" };
        pkgs_dirs = { root_prefix / "pkgs",
                      fs::u8path("~") / ".mamba" / "pkgs"
#ifdef _WIN32
                      ,
                      fs::u8path(env::get("APPDATA").value_or("")) / ".mamba" / "pkgs"
#endif
        };

        keep_temp_files = env::get("MAMBA_KEEP_TEMP") ? true : false;
        keep_temp_directories = env::get("MAMBA_KEEP_TEMP_DIRS") ? true : false;

        if (on_ci || !termcolor::_internal::is_atty(std::cout))
        {
            no_progress_bars = true;
        }

#ifdef _WIN32
        ascii_only = true;
#else
        ascii_only = false;
#endif

        set_default_signal_handler();

        std::shared_ptr<spdlog::logger> l = std::make_shared<Logger>("libmamba", log_pattern, "\n");
        std::shared_ptr<spdlog::logger> libcurl_logger
            = std::make_shared<Logger>("libcurl", log_pattern, "");
        std::shared_ptr<spdlog::logger> libsolv_logger
            = std::make_shared<Logger>("libsolv", log_pattern, "");
        spdlog::register_logger(libcurl_logger);
        spdlog::register_logger(libsolv_logger);

        spdlog::set_default_logger(l);
        logger = std::dynamic_pointer_cast<Logger>(l);
        spdlog::set_level(convert_log_level(logging_level));
    }

    Context::~Context() = default;

    void Context::set_verbosity(int lvl)
    {
        this->verbosity = lvl;

        switch (lvl)
        {
            case -3:
                this->logging_level = log_level::off;
                break;
            case -2:
                this->logging_level = log_level::critical;
                break;
            case -1:
                this->logging_level = log_level::err;
                break;
            case 0:
                this->logging_level = log_level::warn;
                break;
            case 1:
                this->logging_level = log_level::info;
                break;
            case 2:
                this->logging_level = log_level::debug;
                break;
            case 3:
                this->logging_level = log_level::trace;
                break;
            default:
                this->logging_level = log_level::info;
                break;
        }
        spdlog::set_level(convert_log_level(logging_level));
    }

    void Context::set_log_level(log_level level)
    {
        logging_level = level;
        spdlog::set_level(convert_log_level(level));
    }

    std::vector<std::string> Context::platforms()
    {
        return { platform, "noarch" };
    }

    std::map<std::string, AuthenticationInfo>& Context::authentication_info()
    {
        if (!m_authentication_infos_loaded)
            load_authentication_info();
        return m_authentication_info;
    }

    void Context::load_authentication_info()
    {
        auto& ctx = Context::instance();
        std::vector<fs::u8path> found_tokens;

        for (const auto& loc : ctx.token_locations)
        {
            auto px = env::expand_user(loc);
            if (!fs::exists(px) || !fs::is_directory(px))
            {
                continue;
            }
            for (const auto& entry : fs::directory_iterator(px))
            {
                if (ends_with(entry.path().filename().string(), ".token"))
                {
                    found_tokens.push_back(entry.path());
                    std::string token_url = decode_url(entry.path().filename().string());

                    // anaconda client writes out a token for https://api.anaconda.org...
                    // but we need the token for https://conda.anaconda.org
                    // conda does the same
                    std::size_t api_pos = token_url.find("://api.");
                    if (api_pos != std::string::npos)
                    {
                        token_url.replace(api_pos, 7, "://conda.");
                    }

                    // cut ".token" ending
                    token_url = token_url.substr(0, token_url.size() - 6);

                    std::string token_content = read_contents(entry.path());
                    AuthenticationInfo auth_info{ AuthenticationType::kCondaToken, token_content };
                    m_authentication_info[token_url] = auth_info;
                    LOG_INFO << "Found token for " << token_url << " at " << entry.path();
                }
            }
        }

        std::map<std::string, AuthenticationInfo> res;
        fs::u8path auth_loc(mamba::env::home_directory() / ".mamba" / "auth"
                            / "authentication.json");
        try
        {
            if (fs::exists(auth_loc))
            {
                auto infile = open_ifstream(auth_loc);
                nlohmann::json j;
                infile >> j;
                for (auto& [key, el] : j.items())
                {
                    std::string host = key;
                    std::string type = el["type"];
                    AuthenticationInfo info;
                    if (type == "CondaToken")
                    {
                        info.type = AuthenticationType::kCondaToken;
                        info.value = el["token"].get<std::string>();
                        LOG_INFO << "Found token for host " << host
                                 << " in ~/.mamba/auth/authentication.json";
                    }
                    else if (type == "BasicHTTPAuthentication")
                    {
                        info.type = AuthenticationType::kBasicHTTPAuthentication;
                        auto const& user = el.value("user", "");
                        auto pass = decode_base64(el["password"].get<std::string>());
                        if (pass)
                        {
                            info.value = concat(user, ":", pass.value());
                            LOG_INFO << "Found credentials for user " << user << " for host "
                                     << host << " in ~/.mamba/auth/authentication.json";
                        }
                        else
                        {
                            LOG_ERROR
                                << "Found credentials for user " << user << " for host " << host
                                << " in ~/.mamba/auth/authentication.json but could not decode base64 password"
                                << std::endl;
                        }
                    }
                    m_authentication_info[host] = info;
                }
            }
        }
        catch (nlohmann::json::exception& e)
        {
            LOG_WARNING << "Could not parse authentication information from " << auth_loc << ": "
                        << e.what();
        }

        m_authentication_infos_loaded = true;
    }


    std::string env_name(const fs::u8path& prefix)
    {
        if (prefix.empty())
        {
            throw std::runtime_error("Empty path");
        }
        if (paths_equal(prefix, Context::instance().root_prefix))
        {
            return ROOT_ENV_NAME;
        }
        fs::u8path maybe_env_dir = prefix.parent_path();
        for (const auto& d : Context::instance().envs_dirs)
        {
            if (paths_equal(d, maybe_env_dir))
            {
                return prefix.filename().string();
            }
        }
        return prefix.string();
    }

    // Find the location of a prefix given a conda env name.
    // If the location does not exist, an error is raised.
    fs::u8path locate_prefix_by_name(const std::string& name)
    {
        assert(!name.empty());
        if (name == ROOT_ENV_NAME)
        {
            return Context::instance().root_prefix;
        }
        for (auto& d : Context::instance().envs_dirs)
        {
            if (!fs::exists(d) || !fs::is_directory(d))
            {
                continue;
            }
            fs::u8path prefix = d / name;
            if (fs::exists(prefix) && fs::is_directory(prefix))
            {
                return fs::absolute(prefix);
            }
        }

        throw std::runtime_error("Environment name not found " + name);
    }

    const void Context::debug_print()
    {
#define PRINT_CTX(xname) << #xname ": " << xname << "\n"

#define PRINT_CTX_VEC(xname)                                                                       \
    << #xname ": [" << join(", ", xname) << "]"                                                    \
    << "\n"

        // clang-format off
        std::cout << std::boolalpha
                  << ">>> MAMBA CONTEXT <<< \n"
                  PRINT_CTX(target_prefix)
                  PRINT_CTX(root_prefix)
                  PRINT_CTX(dry_run)
                  PRINT_CTX(always_yes)
                  PRINT_CTX(allow_softlinks)
                  PRINT_CTX(offline)
                  PRINT_CTX(quiet)
                  PRINT_CTX(no_rc)
                  PRINT_CTX(no_env)
                  PRINT_CTX(ssl_no_revoke)
                  PRINT_CTX(ssl_verify)
                  PRINT_CTX(retry_timeout)
                  PRINT_CTX(retry_backoff)
                  PRINT_CTX(max_retries)
                  PRINT_CTX(connect_timeout_secs)
                  PRINT_CTX(add_pip_as_python_dependency)
                  PRINT_CTX(override_channels_enabled)
                  PRINT_CTX(use_only_tar_bz2)
                  PRINT_CTX(auto_activate_base)
                  PRINT_CTX(extra_safety_checks)
                  PRINT_CTX(download_threads)
                  PRINT_CTX(verbosity)
                  PRINT_CTX(channel_alias)
                  << "channel_priority: " << (int) channel_priority << "\n"
                  PRINT_CTX_VEC(default_channels)
                  PRINT_CTX_VEC(channels)
                  PRINT_CTX_VEC(pinned_packages)
                  << "platform: " << platform << "\n"
                  << ">>> END MAMBA CONTEXT <<< \n"
                  << std::endl;
        // clang-format on
#undef PRINT_CTX
    }

    void Context::dump_backtrace_no_guards()
    {
        logger->dump_backtrace_no_guards();
    }

}  // namespace mamba
