// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <iostream>

#include <fmt/format.h>
#include <fmt/ostream.h>
#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include "mamba/core/context.hpp"
#include "mamba/core/execution.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/thread_utils.hpp"
#include "mamba/core/util.hpp"
#include "mamba/core/util_os.hpp"
#include "mamba/util/encoding.hpp"
#include "mamba/util/environment.hpp"
#include "mamba/util/path_manip.hpp"
#include "mamba/util/string.hpp"
#include "mamba/util/url_manip.hpp"

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
            pattern,
            spdlog::pattern_time_type::local,
            eol
        );
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
                    {
                        this->sink_it_(msg);
                    }
                }
            );
        }
    }


    enum class logger_kind
    {
        normal_logger,
        default_logger,
    };

    // Associate the registration of a logger to the lifetime of this object.
    // This is used to help with making sure loggers are unregistered once
    // their logical owner is destroyed.
    class Context::ScopedLogger
    {
        std::shared_ptr<Logger> m_logger;

    public:

        explicit ScopedLogger(std::shared_ptr<Logger> new_logger, logger_kind kind = logger_kind::normal_logger)
            : m_logger(std::move(new_logger))
        {
            assert(m_logger);
            if (kind == logger_kind::default_logger)
            {
                spdlog::set_default_logger(m_logger);
            }
            else
            {
                spdlog::register_logger(m_logger);
            }
        }

        ~ScopedLogger()
        {
            if (m_logger)
            {
                spdlog::drop(m_logger->name());
            }
        }

        std::shared_ptr<Logger> logger() const
        {
            assert(m_logger);
            return m_logger;
        }

        ScopedLogger(ScopedLogger&&) = default;
        ScopedLogger& operator=(ScopedLogger&&) = default;

        ScopedLogger(const ScopedLogger&) = delete;
        ScopedLogger& operator=(const ScopedLogger&) = delete;
    };

    spdlog::level::level_enum convert_log_level(log_level l)
    {
        return static_cast<spdlog::level::level_enum>(l);
    }

    namespace
    {
        std::atomic<bool> use_default_signal_handler_val{ true };
    }

    void Context::use_default_signal_handler(bool val)
    {
        use_default_signal_handler_val = val;
        if (use_default_signal_handler_val)
        {
            set_default_signal_handler();
        }
        else
        {
            restore_system_signal_handler();
        }
    }

    std::shared_ptr<Logger> Context::main_logger()
    {
        if (loggers.empty())
        {
            return {};
        }

        return loggers.front().logger();
    }

    void Context::enable_logging_and_signal_handling(Context& context)
    {
        if (use_default_signal_handler_val)
        {
            set_default_signal_handler();
        }

        context.loggers.clear();  // Make sure we work with a known set of loggers, first one is
                                  // always the default one.

        context.loggers.emplace_back(
            std::make_shared<Logger>("libmamba", context.output_params.log_pattern, "\n"),
            logger_kind::default_logger
        );
        MainExecutor::instance().on_close(
            context.tasksync.synchronized([&context] { context.main_logger()->flush(); })
        );

        context.loggers.emplace_back(
            std::make_shared<Logger>("libcurl", context.output_params.log_pattern, "")
        );

        context.loggers.emplace_back(
            std::make_shared<Logger>("libsolv", context.output_params.log_pattern, "")
        );

        spdlog::set_level(convert_log_level(context.output_params.logging_level));
    }

    Context::Context(const ContextOptions& options)
    {
        on_ci = static_cast<bool>(util::get_env("CI"));
        prefix_params.root_prefix = util::get_env("MAMBA_ROOT_PREFIX").value_or("");
        prefix_params.conda_prefix = prefix_params.root_prefix;

        envs_dirs = { prefix_params.root_prefix / "envs" };
        pkgs_dirs = { prefix_params.root_prefix / "pkgs",
                      fs::u8path("~") / ".mamba" / "pkgs"
#ifdef _WIN32
                      ,
                      fs::u8path(util::get_env("APPDATA").value_or("")) / ".mamba" / "pkgs"
#endif
        };

        keep_temp_files = util::get_env("MAMBA_KEEP_TEMP") ? true : false;
        keep_temp_directories = util::get_env("MAMBA_KEEP_TEMP_DIRS") ? true : false;

        set_persist_temporary_files(keep_temp_files);
        set_persist_temporary_directories(keep_temp_directories);

        {
            const bool cout_is_atty = is_atty(std::cout);
            graphics_params.no_progress_bars = (on_ci || !cout_is_atty);
            graphics_params.palette = cout_is_atty ? Palette::terminal() : Palette::no_color();
        }

#ifdef _WIN32
        ascii_only = true;
#else
        ascii_only = false;
#endif

        if (options.enable_logging_and_signal_handling)
        {
            enable_logging_and_signal_handling(*this);
        }
    }

    Context::~Context() = default;

    void Context::set_verbosity(int lvl)
    {
        this->output_params.verbosity = lvl;

        switch (lvl)
        {
            case -3:
                this->output_params.logging_level = log_level::off;
                break;
            case -2:
                this->output_params.logging_level = log_level::critical;
                break;
            case -1:
                this->output_params.logging_level = log_level::err;
                break;
            case 0:
                this->output_params.logging_level = log_level::warn;
                break;
            case 1:
                this->output_params.logging_level = log_level::info;
                break;
            case 2:
                this->output_params.logging_level = log_level::debug;
                break;
            case 3:
                this->output_params.logging_level = log_level::trace;
                break;
            default:
                this->output_params.logging_level = log_level::info;
                break;
        }
        spdlog::set_level(convert_log_level(output_params.logging_level));
    }

    void Context::set_log_level(log_level level)
    {
        output_params.logging_level = level;
        spdlog::set_level(convert_log_level(level));
    }

    std::vector<std::string> Context::platforms() const
    {
        return { platform, "noarch" };
    }

    specs::AuthenticationDataBase& Context::authentication_info()
    {
        if (!m_authentication_infos_loaded)
        {
            load_authentication_info();
        }
        return m_authentication_info;
    }

    const specs::AuthenticationDataBase& Context::authentication_info() const
    {
        return const_cast<Context*>(this)->authentication_info();
    }

    void Context::load_authentication_info()
    {
        std::vector<fs::u8path> found_tokens;

        for (const auto& loc : token_locations)
        {
            auto px = util::expand_home(loc.string());
            if (!fs::exists(px) || !fs::is_directory(px))
            {
                continue;
            }
            for (const auto& entry : fs::directory_iterator(px))
            {
                if (util::ends_with(entry.path().filename().string(), ".token"))
                {
                    found_tokens.push_back(entry.path());
                    std::string token_url = util::decode_percent(entry.path().filename().string());

                    // anaconda client writes out a token for https://api.anaconda.org...
                    // but we need the token for https://conda.anaconda.org
                    // conda does the same
                    std::size_t api_pos = token_url.find("https://api.");
                    if (api_pos != std::string::npos)
                    {
                        token_url.replace(api_pos, 12, "conda.");
                    }

                    // cut ".token" ending
                    token_url = token_url.substr(0, token_url.size() - 6);

                    LOG_INFO << "Found token for " << token_url << " at " << entry.path();
                    m_authentication_info.emplace(
                        std::move(token_url),
                        specs::CondaToken{ read_contents(entry.path()) }
                    );
                }
            }
        }

        std::map<std::string, specs::AuthenticationInfo> res;
        auto auth_loc = fs::u8path(util::user_home_dir()) / ".mamba" / "auth" / "authentication.json";
        try
        {
            if (fs::exists(auth_loc))
            {
                auto infile = open_ifstream(auth_loc);
                nlohmann::json j;
                infile >> j;
                for (auto& [key, el] : j.items())
                {
                    const std::string host = key;
                    const auto type = el["type"].get<std::string_view>();
                    specs::AuthenticationInfo info;
                    if (type == "CondaToken")
                    {
                        info = specs::CondaToken{ el["token"].get<std::string>() };
                        LOG_INFO << "Found token for host " << host
                                 << " in ~/.mamba/auth/authentication.json";
                    }
                    else if (type == "BasicHTTPAuthentication")
                    {
                        const auto& user = el.value("user", "");
                        auto pass = util::decode_base64(el["password"].get<std::string>());
                        if (pass)
                        {
                            info = specs::BasicHTTPAuthentication{
                                /* user= */ user,
                                /* password= */ pass.value(),
                            };
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
                    else if (type == "BearerToken")
                    {
                        info = specs::BearerToken{ el["token"].get<std::string>() };
                        LOG_INFO << "Found bearer token for host " << host
                                 << " in ~/.mamba/auth/authentication.json";
                    }
                    m_authentication_info.emplace(std::move(host), std::move(info));
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

    std::string env_name(const Context& context, const fs::u8path& prefix)
    {
        if (prefix.empty())
        {
            throw std::runtime_error("Empty path");
        }
        if (paths_equal(prefix, context.prefix_params.root_prefix))
        {
            return ROOT_ENV_NAME;
        }
        fs::u8path maybe_env_dir = prefix.parent_path();
        for (const auto& d : context.envs_dirs)
        {
            if (paths_equal(d, maybe_env_dir))
            {
                return prefix.filename().string();
            }
        }
        return prefix.string();
    }

    std::string env_name(const Context& context)
    {
        return env_name(context, context.prefix_params.target_prefix);
    }

    void Context::debug_print() const
    {
#define PRINT_CTX(xout, xname) fmt::print(xout, "{}: {}\n", #xname, xname)

#define PRINT_CTX_VEC(xout, xname) fmt::print(xout, "{}: [{}]\n", #xname, fmt::join(xname, ", "))

        auto out = Console::stream();
        out << std::boolalpha << ">>> MAMBA CONTEXT <<< \n";
        PRINT_CTX(out, prefix_params.target_prefix);
        PRINT_CTX(out, prefix_params.root_prefix);
        PRINT_CTX(out, dry_run);
        PRINT_CTX(out, always_yes);
        PRINT_CTX(out, allow_softlinks);
        PRINT_CTX(out, offline);
        PRINT_CTX(out, output_params.quiet);
        PRINT_CTX(out, src_params.no_rc);
        PRINT_CTX(out, src_params.no_env);
        PRINT_CTX(out, remote_fetch_params.ssl_no_revoke);
        PRINT_CTX(out, remote_fetch_params.ssl_verify);
        PRINT_CTX(out, remote_fetch_params.retry_timeout);
        PRINT_CTX(out, remote_fetch_params.retry_backoff);
        PRINT_CTX(out, remote_fetch_params.max_retries);
        PRINT_CTX(out, remote_fetch_params.connect_timeout_secs);
        PRINT_CTX(out, add_pip_as_python_dependency);
        PRINT_CTX(out, override_channels_enabled);
        PRINT_CTX(out, use_only_tar_bz2);
        PRINT_CTX(out, auto_activate_base);
        PRINT_CTX(out, validation_params.extra_safety_checks);
        PRINT_CTX(out, threads_params.download_threads);
        PRINT_CTX(out, output_params.verbosity);
        PRINT_CTX(out, channel_alias);
        out << "channel_priority: " << static_cast<int>(channel_priority) << '\n';
        PRINT_CTX_VEC(out, default_channels);
        PRINT_CTX_VEC(out, channels);
        PRINT_CTX_VEC(out, pinned_packages);
        PRINT_CTX(out, platform);
        out << ">>> END MAMBA CONTEXT <<< \n" << std::endl;
#undef PRINT_CTX
#undef PRINT_CTX_VEC
    }

    void Context::dump_backtrace_no_guards()
    {
        if (main_logger())  // REVIEW: is this correct?
        {
            main_logger()->dump_backtrace_no_guards();
        }
    }

}  // namespace mamba
