// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <csignal>
#include <fstream>

#include "mamba/core/context.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/environment.hpp"
#include "mamba/core/fetch.hpp"
#include "mamba/core/thread_utils.hpp"
#include "mamba/core/util.hpp"

#include "nlohmann/json.hpp"
#include "thirdparty/termcolor.hpp"

namespace mamba
{
    Context::Context()
    {
        set_verbosity(0);
        on_ci = (std::getenv("CI") != nullptr);
        if (on_ci || !termcolor::_internal::is_atty(std::cout))
        {
            no_progress_bars = true;
        }

        set_default_signal_handler();
        load_channel_root_cache();
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

    void Context::load_channel_root_cache()
    {
        if (fs::is_regular_file(channel_root_cache_path))
        {
            std::ifstream file(channel_root_cache_path);
            std::string url;
            while (std::getline(file, url))
            {
                channel_root_cache.emplace_back(std::move(url));
            }
        }
    }

    void Context::set_channel_root_cache(const fs::path& p)
    {
        if (p != channel_root_cache_path)
        {
            channel_root_cache_path = p;
            load_channel_root_cache();
        }
    }

    std::optional<std::string> Context::resolve_channel_platform(std::string& url)
    {
        // We assume the url has been sanitized, and does not end with `/`.
        std::string_view view(url);
        for (const auto& root : channel_root_cache)
        {
            if (view.substr(0, root.size()) == root
                && (view.size() == root.size() || view[root.size()] == '/'))
            {
                if (view.size() == root.size())
                {
                    return {};
                }
                else
                {
                    auto platform = std::string(view.substr(root.size() + 1));
                    url = root;
                    return platform;
                }
            }
        }

        std::optional<std::string> platform;

        // No cache hit, query the url and ancestors to determine if it's a
        // channel root (channeldata.json) or a platform subdir.
        //
        // Ideally we could use the repodata.json content (.info.subdir) to
        // discover the channel root quickly, however it seems that sometimes
        // the repodata.json content is invalid json!
        while (true)
        {
            auto result = DownloadTarget::head_request(std::string(view) + "/channeldata.json");
            if (result != CURLE_OK)
            {
                auto pos = view.find_last_of('/');
                if (pos == std::string_view::npos)
                {
                    LOG_WARNING << "could not find channel root for " << url;
                    return {};
                }
                if (platform)
                {
                    *platform = std::string(view.substr(pos + 1)) + "/" + *platform;
                }
                else
                {
                    platform = std::string(view.substr(pos + 1));
                }
                view = view.substr(0, pos);
            }
            else
            {
                break;
            }
        }

        // Insert resulting url into cache
        {
            channel_root_cache.emplace_back(view);
            std::ofstream f(channel_root_cache_path, std::ios_base::app);
            f << view << std::endl;
        }

        url = std::string(view);

        return platform;
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
                  PRINT_CTX(max_parallel_downloads)
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
