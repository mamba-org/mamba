// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_CONTEXT_HPP
#define MAMBA_CORE_CONTEXT_HPP

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "mamba/core/common_types.hpp"
#include "mamba/core/context_params.hpp"
#include "mamba/core/palette.hpp"
#include "mamba/core/subdir_parameters.hpp"
#include "mamba/core/tasksync.hpp"
#include "mamba/download/mirror_map.hpp"
#include "mamba/download/parameters.hpp"
#include "mamba/fs/filesystem.hpp"
#include "mamba/solver/libsolv/parameters.hpp"
#include "mamba/solver/request.hpp"
#include "mamba/specs/authentication_info.hpp"
#include "mamba/specs/platform.hpp"
#include "mamba/version.hpp"

#define ROOT_ENV_NAME "base"

namespace mamba
{
    enum class VerificationLevel
    {
        Disabled,
        Warn,
        Enabled
    };

    struct ValidationParams
    {
        VerificationLevel safety_checks = VerificationLevel::Warn;
        bool extra_safety_checks = false;
        bool verify_artifacts = false;

        // TODO Uncomment `conda-forge` or whatever trusted_channels when possible
        // (i.e server side package signing ready)
        // Remove "http://127.0.0.1:8000/get/channel0"
        // (should only be used in integration tests,
        // this one is for testing with quetz)
        std::vector<std::string> trusted_channels = {
            /*"conda-forge", */ "http://127.0.0.1:8000/get/channel0"
        };
    };

    enum class ChannelPriority
    {
        Disabled,
        Flexible,
        Strict
    };


    class Logger;
    class Context;

    struct ContextOptions
    {
        bool enable_logging = false;
        bool enable_signal_handling = false;
    };

    // Context singleton class
    class Context
    {
    public:

        static void use_default_signal_handler(bool val);

        struct OutputParams
        {
            int verbosity{ 0 };
            log_level logging_level{ log_level::warn };

            bool json{ false };
            bool quiet{ false };

            std::string log_pattern{ "%^%-9!l%-8n%$ %v" };
            std::size_t log_backtrace{ 0 };
        };

        struct GraphicsParams
        {
            bool no_progress_bars{ false };
            Palette palette;
        };

        struct SrcParams
        {
            bool no_rc{ false };
            bool no_env{ false };
        };

        // Configurable
        bool experimental = false;
        bool experimental_repodata_parsing = true;
        bool experimental_matchspec_parsing = false;
        bool debug = false;
        bool use_uv = false;

        // TODO check writable and add other potential dirs
        std::vector<fs::u8path> envs_dirs;
        std::vector<fs::u8path> pkgs_dirs;
        std::optional<std::string> env_lockfile;

        bool use_index_cache = false;
        std::size_t local_repodata_ttl = 1;  // take from header
        bool offline = false;

        ChannelPriority channel_priority = ChannelPriority::Flexible;
        bool auto_activate_base = false;

        bool extract_sparse = false;

        bool dry_run = false;
        bool download_only = false;
        bool always_yes = false;

        bool register_envs = true;

        bool show_anaconda_channel_warnings = true;

        // solver options
        solver::Request::Flags solver_flags = {};

        // add start menu shortcuts on Windows (not implemented on Linux / macOS)
        bool shortcuts = true;

        // debug helpers
        bool keep_temp_files = false;
        bool keep_temp_directories = false;

        bool change_ps1 = true;
        std::string env_prompt = "({default_env}) ";
        bool ascii_only = false;
        // micromamba only
        bool shell_completion = true;

        OutputParams output_params;
        GraphicsParams graphics_params;
        SrcParams src_params;
        CommandParams command_params;
        ThreadsParams threads_params;
        PrefixParams prefix_params;
        ValidationParams validation_params;
        LinkParams link_params;

        download::RemoteFetchParams remote_fetch_params = {
            .ssl_verify = { "" },
            .ssl_no_revoke = false,
            .curl_initialized = false,
            .user_agent = { "mamba/" LIBMAMBA_VERSION_STRING },
            .connect_timeout_secs = 10.,
            .retry_timeout = 2,
            .retry_backoff = 3,
            .max_retries = 3,
            .proxy_servers = {},
        };

        download::Options download_options() const
        {
            return {
                .download_threads = this->threads_params.download_threads,
                .fail_fast = false,
                .sort = true,
                .verbose = this->output_params.verbosity >= 2,
            };
        }

        SubdirParams subdir_params() const
        {
            const auto get_local_repodata_ttl = [&]() -> std::optional<std::size_t>
            {
                // Force the use of index cache by setting TTL to 0
                if (this->use_index_cache)
                {
                    return { 0 };
                }
                // This is legacy where from where 1 meant to read from header
                if (this->local_repodata_ttl == 1)
                {
                    return std::nullopt;
                }
                return { this->local_repodata_ttl };
            };

            return {
                .local_repodata_ttl_s = get_local_repodata_ttl(),
                .offline = this->offline,
                .repodata_force_use_zst = false  // Must override based on ChannelContext
            };
        }

        SubdirDownloadParams subdir_download_params() const
        {
            return {
                .offline = this->offline,
                .repodata_check_zst = this->repodata_use_zst,
            };
        }

        TransactionParams transaction_params() const
        {
            return {
                .is_mamba_exe = command_params.is_mamba_exe,
                .json_output = output_params.json,
                .verbosity = output_params.verbosity,
                .shortcuts = shortcuts,
                .envs_dirs = envs_dirs,
                .platform = platform,
                .prefix_params = prefix_params,
                .link_params = link_params,
                .threads_params = threads_params,
            };
        }

        std::size_t lock_timeout = 0;
        bool use_lockfiles = true;

        // Conda compat
        bool add_pip_as_python_dependency = true;

        std::string host_platform = std::string(specs::build_platform_name());
        std::string platform = std::string(specs::build_platform_name());
        std::vector<std::string> platforms() const;

        std::vector<std::string> channels = { "conda-forge" };
        std::map<std::string, std::string> custom_channels;
        std::map<std::string, std::vector<std::string>> custom_multichannels;

        std::vector<std::string> default_channels = {
#ifdef _WIN32
            "https://repo.anaconda.com/pkgs/main",
            "https://repo.anaconda.com/pkgs/r",
            "https://repo.anaconda.com/pkgs/msys2"
#else
            "https://repo.anaconda.com/pkgs/main",
            "https://repo.anaconda.com/pkgs/r"
#endif
        };

        std::map<std::string, std::vector<std::string>> mirrored_channels;
        std::string channel_alias = "https://conda.anaconda.org";
        specs::AuthenticationDataBase& authentication_info();
        const specs::AuthenticationDataBase& authentication_info() const;
        std::vector<fs::u8path> token_locations{ "~/.continuum/anaconda-client/tokens" };

        bool override_channels_enabled = true;

        std::vector<std::string> pinned_packages = {};

        bool use_only_tar_bz2 = false;

        bool repodata_use_zst = true;
        std::vector<std::string> repodata_has_zst = { "https://conda.anaconda.org/conda-forge" };

        // FIXME: Should not be stored here
        // Notice that we cannot build this map directly from mirrored_channels,
        // since we need to add a single "mirror" for non mirrored channels
        download::mirror_map mirrors;

        Context(const Context&) = delete;
        Context& operator=(const Context&) = delete;

        Context(Context&&) = delete;
        Context& operator=(Context&&) = delete;

        void debug_print() const;
        void dump_backtrace_no_guards();

        void set_verbosity(int lvl);
        void set_log_level(log_level level);

        Context(const ContextOptions& options = {});
        ~Context();

    private:

        // Used internally
        bool on_ci = false;

        void load_authentication_info();
        specs::AuthenticationDataBase m_authentication_info;
        bool m_authentication_infos_loaded = false;

        class ScopedLogger;
        std::vector<ScopedLogger> loggers;

        std::shared_ptr<Logger> main_logger();
        void add_logger(std::shared_ptr<Logger>);

        TaskSynchronizer tasksync;


        // Enables the provided context setup signal handling.
        // This function must be called only for one Context in the lifetime of the program.
        void enable_signal_handling();

        // Enables the provided context to drive the logging system.
        // This function must be called only for one Context in the lifetime of the program.
        void enable_logging();
    };


}  // namespace mamba

#endif  // MAMBA_CONTEXT_HPP
