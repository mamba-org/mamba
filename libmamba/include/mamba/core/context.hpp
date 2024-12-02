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
#include "mamba/core/palette.hpp"
#include "mamba/core/tasksync.hpp"
#include "mamba/download/mirror_map.hpp"
#include "mamba/fs/filesystem.hpp"
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

    std::string env_name(const Context& context, const fs::u8path& prefix);
    std::string env_name(const Context& context);

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

        struct RemoteFetchParams
        {
            // ssl_verify can be either an empty string (regular SSL verification),
            // the string "<false>" to indicate no SSL verification, or a path to
            // a directory with cert files, or a cert file.
            std::string ssl_verify{ "" };
            bool ssl_no_revoke{ false };
            bool curl_initialized{ false };  // non configurable, used in fetch only

            std::string user_agent{ "mamba/" LIBMAMBA_VERSION_STRING };

            double connect_timeout_secs{ 10. };
            // int read_timeout_secs { 60 };
            int retry_timeout{ 2 };  // seconds
            int retry_backoff{ 3 };  // retry_timeout * retry_backoff
            int max_retries{ 3 };    // max number of retries

            std::map<std::string, std::string> proxy_servers;
        };

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

        struct CommandParams
        {
            std::string caller_version{ "" };
            std::string conda_version{ "3.8.0" };
            std::string current_command{ "mamba" };
            /** Is the Context used in a mamba or mamba executable (instead of a lib). */
            bool is_mamba_exe{ false };
        };

        struct ThreadsParams
        {
            std::size_t download_threads{ 5 };
            int extract_threads{ 0 };
        };

        struct PrefixParams
        {
            fs::u8path target_prefix;
            fs::u8path root_prefix;
            fs::u8path conda_prefix;
            fs::u8path relocate_prefix;
        };

        // Configurable
        bool experimental = false;
        bool experimental_repodata_parsing = true;
        bool debug = false;

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

        bool dev = false;  // TODO this is always used as default=false and isn't set anywhere => to
                           // be removed if this is the case...
        bool dry_run = false;
        bool download_only = false;
        bool always_yes = false;

        bool allow_softlinks = false;
        bool always_copy = false;
        bool always_softlink = false;
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

        RemoteFetchParams remote_fetch_params;
        OutputParams output_params;
        GraphicsParams graphics_params;
        SrcParams src_params;
        CommandParams command_params;
        ThreadsParams threads_params;
        PrefixParams prefix_params;
        ValidationParams validation_params;

        std::size_t lock_timeout = 0;
        bool use_lockfiles = true;

        bool compile_pyc = true;

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
