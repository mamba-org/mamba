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

#include "termcolor/termcolor.hpp"

namespace mamba
{
    Context::Context()
    {
        on_ci = bool(env::get("CI"));
        root_prefix = env::get("MAMBA_ROOT_PREFIX").value_or("");
        conda_prefix = root_prefix;

        envs_dirs = { root_prefix / "envs" };
        pkgs_dirs = { root_prefix / "pkgs",
                      fs::path("~") / ".mamba" / "pkgs"
#ifdef _WIN32
                      ,
                      fs::path(env::get("APPDATA").value_or("")) / ".mamba" / "pkgs"
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
        spdlog::set_level(log_level);
    }

    Context& Context::instance()
    {
        static Context ctx;
        return ctx;
    }

    void Context::set_verbosity(int lvl)
    {
        this->verbosity = lvl;

        switch (lvl)
        {
            case -3:
                this->log_level = spdlog::level::off;
                break;
            case -2:
                this->log_level = spdlog::level::critical;
                break;
            case -1:
                this->log_level = spdlog::level::err;
                break;
            case 0:
                this->log_level = spdlog::level::warn;
                break;
            case 1:
                this->log_level = spdlog::level::info;
                break;
            case 2:
                this->log_level = spdlog::level::debug;
                break;
            case 3:
                this->log_level = spdlog::level::trace;
                break;
            default:
                this->log_level = spdlog::level::info;
                break;
        }
        spdlog::set_level(log_level);
    }

    void Context::set_log_level(const spdlog::level::level_enum& level)
    {
        log_level = level;
        spdlog::set_level(level);
    }

    std::vector<std::string> Context::platforms()
    {
        return { platform, "noarch" };
    }

    std::string env_name(const fs::path& prefix)
    {
        if (prefix.empty())
        {
            throw std::runtime_error("Empty path");
        }
        if (paths_equal(prefix, Context::instance().root_prefix))
        {
            return ROOT_ENV_NAME;
        }
        fs::path maybe_env_dir = prefix.parent_path();
        for (const auto& d : Context::instance().envs_dirs)
        {
            if (paths_equal(d, maybe_env_dir))
            {
                return prefix.filename();
            }
        }
        return prefix;
    }

    // Find the location of a prefix given a conda env name.
    // If the location does not exist, an error is raised.
    fs::path locate_prefix_by_name(const std::string& name)
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
            fs::path prefix = d / name;
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
}  // namespace mamba
