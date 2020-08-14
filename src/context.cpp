// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <csignal>

#include "mamba/context.hpp"
#include "mamba/output.hpp"
#include "mamba/thirdparty/termcolor.hpp"
#include "mamba/thread_utils.hpp"
#include "mamba/util.hpp"

namespace mamba
{
    namespace
    {
// armv6l and armv7l
#if defined(__arm__) || defined(__thumb__)
#ifdef ___ARM_ARCH_6__
        static const char MAMBA_PLATFORM[] = "armv6l";
#elif __ARM_ARCH_7__
        static const char MAMBA_PLATFORM[] = "armv7l";
#else
#error "Unknown platform"
#endif
#elif _M_ARM == 6
        static const char MAMBA_PLATFORM[] = "armv6l";
#elif _M_ARM == 7
        static const char MAMBA_PLATFORM[] = "armv7l";
// aarch64
#elif defined(__aarch64__)
        static const char MAMBA_PLATFORM[] = "aarch64";
#elif defined(__ppc64__) || defined(__powerpc64__)
#if (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
        static const char MAMBA_PLATFORM[] = "ppc64";
#else
        static const char MAMBA_PLATFORM[] = "ppc64le";
#endif
// Linux
#elif defined(__linux__)
#if __x86_64__
        static const char MAMBA_PLATFORM[] = "linux-64";
#else
        static const char MAMBA_PLATFORM[] = "linux-32";
#endif
// OSX
#elif defined(__APPLE__) || defined(__MACH__)
        static const char MAMBA_PLATFORM[] = "osx-64";
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
        MessageLogger::global_log_severity() = mamba::LogSeverity::error;
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

    std::string Context::platform() const
    {
        return MAMBA_PLATFORM;
    }

    std::vector<std::string> Context::platforms() const
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
