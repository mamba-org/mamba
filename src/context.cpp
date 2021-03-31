// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <csignal>

#include "mamba/context.hpp"
#include "mamba/output.hpp"
#include "mamba/environment.hpp"
#include "mamba/thread_utils.hpp"
#include "mamba/util.hpp"

#include "thirdparty/termcolor.hpp"

namespace mamba
{
    namespace
    {
// Linux
#if defined(__linux__)
#if __x86_64__
        static const char MAMBA_PLATFORM[] = "linux-64";
#elif defined(i386)
        static const char MAMBA_PLATFORM[] = "linux-32";
// armv6l and armv7l
#elif defined(__arm__) || defined(__thumb__)
#ifdef ___ARM_ARCH_6__
        static const char MAMBA_PLATFORM[] = "linux-armv6l";
#elif __ARM_ARCH_7__
        static const char MAMBA_PLATFORM[] = "linux-armv7l";
#else
#error "Unknown Linux arm platform"
#endif
#elif _M_ARM == 6
        static const char MAMBA_PLATFORM[] = "linux-armv6l";
#elif _M_ARM == 7
        static const char MAMBA_PLATFORM[] = "linux-armv7l";
// aarch64
#elif defined(__aarch64__)
        static const char MAMBA_PLATFORM[] = "linux-aarch64";
#elif defined(__ppc64__) || defined(__powerpc64__)
#if (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
        static const char MAMBA_PLATFORM[] = "linux-ppc64";
#else
        static const char MAMBA_PLATFORM[] = "linux-ppc64le";
#endif
#elif defined(__s390x__)
        static const char MAMBA_PLATFORM[] = "linux-s390x";
#else
#error "Unknown Linux platform"
#endif
// OSX
#elif defined(__APPLE__) || defined(__MACH__)
#if __x86_64__
        static const char MAMBA_PLATFORM[] = "osx-64";
#elif __arm64__
        static const char MAMBA_PLATFORM[] = "osx-arm64";
#else
#error "Unknown OSX platform"
#endif
// Windows
#elif defined(_WIN64)
        static const char MAMBA_PLATFORM[] = "win-64";
#elif defined(_WIN32)
        static const char MAMBA_PLATFORM[] = "win-32";
#else
#error "Unknown platform"
#endif
    }  // namespace
    Context::Context()
    {
        set_verbosity(0);
        on_ci = (std::getenv("CI") != nullptr);
        if (on_ci || !termcolor::_internal::is_atty(std::cout))
        {
            no_progress_bars = true;
        }

        set_default_signal_handler();
    }

    Context& Context::instance()
    {
        static Context ctx;
        return ctx;
    }

    void Context::set_verbosity(int lvl)
    {
        MessageLogger::global_log_severity() = mamba::LogSeverity::kWarning;
        if (lvl == 1)
        {
            MessageLogger::global_log_severity() = mamba::LogSeverity::kInfo;
        }
        else if (lvl > 1)
        {
            MessageLogger::global_log_severity() = mamba::LogSeverity::kDebug;
        }
        this->verbosity = lvl;
    }

    std::string Context::platform()
    {
        std::string platform = env::get("CONDA_SUBDIR");
        if (platform.empty())
        {
            return MAMBA_PLATFORM;
        }
        else
        {
            return platform;
        }
    }

    std::vector<std::string> Context::platforms()
    {
        return { platform(), "noarch" };
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
#define PRINT_CTX(xname) \
        << #xname ": " << xname << "\n"

#define PRINT_CTX_VEC(xname) \
        << #xname ": [" << join(", ", xname) << "]" << "\n"

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
                  PRINT_CTX(max_parallel_downloads)
                  PRINT_CTX(verbosity)
                  PRINT_CTX(channel_alias)
                  << "channel_priority: " << (int)channel_priority << "\n"
                  PRINT_CTX_VEC(default_channels)
                  PRINT_CTX_VEC(channels)
                  PRINT_CTX_VEC(pinned_packages)
                  << "platform: " << platform() << "\n"
                  << ">>> END MAMBA CONTEXT <<< \n"
                  << std::endl;
#undef PRINT_CTX
    }
}  // namespace mamba
