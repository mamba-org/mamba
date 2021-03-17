// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CONTEXT_HPP
#define MAMBA_CONTEXT_HPP

#include <map>
#include <string>
#include <vector>

#include "mamba_fs.hpp"

#define ROOT_ENV_NAME "base"

namespace mamba
{
    enum class VerificationLevel
    {
        kDisabled,
        kWarn,
        kEnabled
    };


    enum class ChannelPriority
    {
        kDisabled,
        kFlexible,
        kStrict
    };

    enum class LogLevel
    {
        kOff = 0,
        kFatal,
        kError,
        kWarning,
        kInfo,
        kDebug,
        kTrace
    };

    std::string env_name(const fs::path& prefix);
    fs::path locate_prefix_by_name(const std::string& name);

    // Context singleton class
    class Context
    {
    public:
        std::string conda_version = "3.8.0";
        std::string current_command = "mamba";
        bool is_micromamba = false;

        fs::path target_prefix = "";
        // Need to prevent circular imports here (otherwise using env::get())
        fs::path root_prefix
            = std::getenv("MAMBA_ROOT_PREFIX") ? std::getenv("MAMBA_ROOT_PREFIX") : "";
        fs::path conda_prefix = root_prefix;

        // TODO check writable and add other potential dirs
        std::vector<fs::path> envs_dirs = { root_prefix / "envs" };
        std::vector<fs::path> pkgs_dirs = { root_prefix / "pkgs" };

        bool use_index_cache = false;
        std::size_t local_repodata_ttl = 1;  // take from header
        bool offline = false;
        bool quiet = false;
        bool json = false;
        ChannelPriority channel_priority = ChannelPriority::kFlexible;
        bool auto_activate_base = false;

        long max_parallel_downloads = 5;
        LogLevel verbosity = LogLevel::kWarning;

        bool dev = false;
        bool on_ci = false;
        bool no_progress_bars = false;
        bool dry_run = false;
        bool always_yes = false;

        bool allow_softlinks = false;
        bool always_copy = false;
        bool always_softlink = false;

        VerificationLevel safety_checks = VerificationLevel::kWarn;
        bool extra_safety_checks = false;

        // debug helpers
        bool keep_temp_files = std::getenv("MAMBA_KEEP_TEMP") ? 1 : 0;
        bool keep_temp_directories = std::getenv("MAMBA_KEEP_TEMP_DIRS") ? 1 : 0;

        bool change_ps1 = true;

        int connect_timeout_secs = 10;
        // int read_timeout_secs = 60;
        int retry_timeout = 2;  // seconds
        int retry_backoff = 3;  // retry_timeout * retry_backoff
        int max_retries = 3;    // max number of retries

        std::string env_prompt = "({default_env}) ";

        // ssl verify can be either an empty string (regular SSL verification),
        // the string "<false>" to indicate no SSL verification, or a path to
        // a directory with cert files, or a cert file.
        std::string ssl_verify = "";
        bool ssl_no_revoke = false;

        bool no_rc = false;
        bool no_env = false;

        // Conda compat
        bool add_pip_as_python_dependency = true;

        void set_verbosity(int lvl);

        static std::string platform();
        static std::vector<std::string> platforms();

        std::vector<std::string> channels = {};
        std::vector<std::string> default_channels = {
#ifdef _WIN32
            "https://repo.anaconda.com/pkgs/main",
            "https://repo.anaconda.com/pkgs/r",
            "https://repo.anaconda.com/pkgs/msys2"
#else
            "https://repo.anaconda.com/pkgs/main", "https://repo.anaconda.com/pkgs/r"
#endif
        };

        std::string channel_alias = "https://conda.anaconda.org";
        bool override_channels_enabled = true;

        std::vector<std::string> pinned_packages = {};

        bool use_only_tar_bz2 = false;

        static Context& instance();

        Context(const Context&) = delete;
        Context& operator=(const Context&) = delete;

        Context(Context&&) = delete;
        Context& operator=(Context&&) = delete;

    private:
        Context();
        ~Context() = default;
    };
}  // namespace mamba

#endif  // MAMBA_CONTEXT_HPP
