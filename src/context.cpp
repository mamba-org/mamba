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

#ifdef UMAMBA_ONLY
#include "mamba/config.hpp"
#endif

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
        MessageLogger::global_log_severity() = mamba::LogSeverity::warning;
        if (lvl == 1)
        {
            MessageLogger::global_log_severity() = mamba::LogSeverity::info;
        }
        else if (lvl > 1)
        {
            MessageLogger::global_log_severity() = mamba::LogSeverity::debug;
        }
        this->verbosity = lvl;
    }

#ifdef UMAMBA_ONLY
    void Context::load_config()
    {
        Configurable& config = Configurable::instance();
        auto c = config.get_config();

#define UMAMBA_EXTRACT_CONFIG(OPTIONNAME, TYPE)                                                    \
    if (c[#OPTIONNAME])                                                                            \
    {                                                                                              \
        OPTIONNAME = c[#OPTIONNAME].as<TYPE>();                                                    \
    }

        UMAMBA_EXTRACT_CONFIG(channels, std::vector<std::string>);
        UMAMBA_EXTRACT_CONFIG(pinned_packages, std::vector<std::string>);
        UMAMBA_EXTRACT_CONFIG(ssl_verify, bool);
        UMAMBA_EXTRACT_CONFIG(extra_safety_checks, bool);
        UMAMBA_EXTRACT_CONFIG(auto_activate_base, bool);
        UMAMBA_EXTRACT_CONFIG(override_channels_enabled, bool);
        UMAMBA_EXTRACT_CONFIG(channel_alias, std::string);
        UMAMBA_EXTRACT_CONFIG(channel_alias, std::string);

        if (c["safety_checks"])
        {
            std::string conf_safety_checks = to_lower(c["safety_checks"].as<std::string>());
            if (conf_safety_checks == "enabled")
            {
                safety_checks = VerificationLevel::ENABLED;
            }
            else if (conf_safety_checks == "disabled")
            {
                safety_checks = VerificationLevel::DISABLED;
            }
            else if (conf_safety_checks == "warn")
            {
                safety_checks = VerificationLevel::WARN;
            }
            else
            {
                LOG_WARNING
                    << "Could not parse safety_checks option (possible values: enabled, warn, disabled)";
            }
        }

        if (c["channel_priority"])
        {
            std::string conf_channel_priority = to_lower(c["channel_priority"].as<std::string>());
            if (conf_channel_priority == "strict")
            {
                strict_channel_priority = true;
            }
            else if (conf_channel_priority == "disabled" || conf_channel_priority == "false")
            {
                strict_channel_priority = false;
            }
            else
            {
                LOG_WARNING
                    << "Could not parse channel_priority option (possible values are strict and disabled)";
            }
        }
    }
#endif

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
}  // namespace mamba
