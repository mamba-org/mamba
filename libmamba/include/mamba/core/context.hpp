// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_CONTEXT_HPP
#define MAMBA_CORE_CONTEXT_HPP

#include <map>
#include <optional>
#include <regex>
#include <string>
#include <vector>

#include "mamba/core/common_types.hpp"
#include "mamba/core/mamba_fs.hpp"
#include "mamba/core/palette.hpp"
#include "mamba/core/tasksync.hpp"
#include "mamba/version.hpp"

#define ROOT_ENV_NAME "base"

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
#elif defined(__riscv) && defined(__riscv_xlen) && (__riscv_xlen == 32)
        static const char MAMBA_PLATFORM[] = "linux-riscv32";
#elif defined(__riscv) && defined(__riscv_xlen) && (__riscv_xlen == 64)
        static const char MAMBA_PLATFORM[] = "linux-riscv64";
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

    enum class AuthenticationType
    {
        kBasicHTTPAuthentication,
        kCondaToken
    };

    struct AuthenticationInfo
    {
        AuthenticationType type;
        std::string value;
    };

    class Logger;

    std::string env_name(const fs::u8path& prefix);
    fs::u8path locate_prefix_by_name(const std::string& name);

    // Context singleton class
    class Context
    {
    public:

        std::string caller_version = "";
        std::string conda_version = "3.8.0";
        std::string current_command = "mamba";
        std::string custom_banner = "";
        bool is_micromamba = false;
        bool experimental = false;
        bool debug = false;

        fs::u8path target_prefix;
        fs::u8path root_prefix;
        fs::u8path conda_prefix;
        fs::u8path relocate_prefix;

        // TODO check writable and add other potential dirs
        std::vector<fs::u8path> envs_dirs;
        std::vector<fs::u8path> pkgs_dirs;
        std::optional<std::string> env_lockfile;

        bool use_index_cache = false;
        std::size_t local_repodata_ttl = 1;  // take from header
        bool offline = false;
        bool quiet = false;
        bool json = false;
        ChannelPriority channel_priority = ChannelPriority::kFlexible;
        bool auto_activate_base = false;

        std::size_t download_threads = 5;
        int extract_threads = 0;
        bool extract_sparse = false;

        int verbosity = 0;
        void set_verbosity(int lvl);
        void set_log_level(log_level level);

        log_level logging_level = log_level::warn;
        std::string log_pattern = "%^%-9!l%-8n%$ %v";
        std::size_t log_backtrace = 0;

        bool dev = false;
        bool on_ci = false;
        bool dry_run = false;
        bool download_only = false;
        bool always_yes = false;

        bool no_progress_bars = false;
        Palette palette;

        bool allow_softlinks = false;
        bool always_copy = false;
        bool always_softlink = false;

        // solver options
        bool allow_uninstall = true;
        bool allow_downgrade = false;

        // add start menu shortcuts on Windows (not implemented on Linux / macOS)
        bool shortcuts = true;

        VerificationLevel safety_checks = VerificationLevel::kWarn;
        bool extra_safety_checks = false;
        bool verify_artifacts = false;

        // debug helpers
        bool keep_temp_files = false;
        bool keep_temp_directories = false;

        bool change_ps1 = true;
        std::string env_prompt = "({default_env}) ";
        bool ascii_only = false;
        // micromamba only
        bool shell_completion = true;

        std::string user_agent = "mamba/" LIBMAMBA_VERSION_STRING;
        bool curl_initialized = false;
        int connect_timeout_secs = 10;
        // int read_timeout_secs = 60;
        int retry_timeout = 2;  // seconds
        int retry_backoff = 3;  // retry_timeout * retry_backoff
        int max_retries = 3;    // max number of retries

        std::map<std::string, std::string> proxy_servers;
        // ssl verify can be either an empty string (regular SSL verification),
        // the string "<false>" to indicate no SSL verification, or a path to
        // a directory with cert files, or a cert file.
        std::string ssl_verify = "";
        bool ssl_no_revoke = false;

        bool no_rc = false;
        bool no_env = false;

        std::size_t lock_timeout = 0;
        bool use_lockfiles = true;

        bool compile_pyc = true;

        // Conda compat
        bool add_pip_as_python_dependency = true;

        std::string host_platform = MAMBA_PLATFORM;
        std::string platform = MAMBA_PLATFORM;
        std::vector<std::string> platforms();

        std::vector<std::string> channels;
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

        std::string channel_alias = "https://conda.anaconda.org";
        std::map<std::string, AuthenticationInfo>& authentication_info();
        std::vector<fs::u8path> token_locations{ "~/.continuum/anaconda-client/tokens" };

        bool override_channels_enabled = true;

        std::vector<std::string> pinned_packages = {};

        bool use_only_tar_bz2 = false;

        bool repodata_use_zst = false;
        std::vector<std::string> repodata_has_zst = { "https://conda.anaconda.org/conda-forge" };

        // usernames on anaconda.org can have a underscore, which influences the
        // first two characters
        const std::regex token_regex{ "/t/([a-zA-Z0-9-_]{0,2}[a-zA-Z0-9-]*)" };
        const std::regex http_basicauth_regex{ "(://|^)([^\\s]+):([^\\s]+)@" };
        const std::regex scheme_regex{ "[a-z][a-z0-9]{0,11}://" };

        static Context& instance();

        Context(const Context&) = delete;
        Context& operator=(const Context&) = delete;

        Context(Context&&) = delete;
        Context& operator=(Context&&) = delete;

        void debug_print() const;
        void dump_backtrace_no_guards();

    protected:

        Context();
        ~Context();


    private:

        void load_authentication_info();
        std::map<std::string, AuthenticationInfo> m_authentication_info;
        bool m_authentication_infos_loaded = false;

        std::shared_ptr<Logger> logger;

        TaskSynchronizer tasksync;
    };
}  // namespace mamba

#endif  // MAMBA_CONTEXT_HPP
