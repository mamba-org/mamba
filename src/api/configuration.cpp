// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/api/configuration.hpp"
#include "mamba/api/info.hpp"
#include "mamba/api/install.hpp"

#include "mamba/core/environment.hpp"
#include "mamba/core/fetch.hpp"
#include "mamba/core/fsutil.hpp"

#include <reproc++/run.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <stdexcept>


namespace mamba
{
    namespace detail
    {
        void ssl_verify_hook(std::string& value)
        {
            bool& offline = Configuration::instance().at("offline").value<bool>();
            if (offline)
            {
                LOG_DEBUG << "SSL verification disabled by offline mode";
                value = "<false>";
                return;
            }
            if ((value == "false") || (value == "0") || (value == "<false>"))
            {
                value = "<false>";
                return;
            }

            auto& cacert
                = Configuration::instance().at("cacert_path").template get_wrapped<std::string>();
            if (!cacert.value().empty())
            {
                value = cacert.value();
                return;
            }
            else
            {
                if (value.empty() || (value == "true") || (value == "1") || (value == "<true>"))
                {
                    value = "<system>";
                }
            }
        };

        void always_softlink_hook(bool& value)
        {
            auto& config = Configuration::instance();
            auto& always_copy = config.at("always_copy").value<bool>();

            if (value && always_copy)
            {
                throw std::runtime_error(
                    "'always-softlink' and 'always-copy' options are exclusive.");
            }
        }

        void file_spec_env_name_hook(std::string& name)
        {
            if (name.find_first_of("/\\") != std::string::npos)
            {
                throw std::runtime_error(
                    "An unexpected file-system separator was found in environment name: '" + name
                    + "'");
            }
        }

        void env_name_hook(std::string& name)
        {
            file_spec_env_name_hook(name);

            auto& config = Configuration::instance();
            auto& root_prefix = config.at("root_prefix").value<fs::path>();

            auto& env_name = config.at("env_name");

            auto& spec_file_env_name = config.at("spec_file_env_name");
            auto& spec_file_name = spec_file_env_name.value<std::string>();

            // Allow spec file environment name to be overriden by target prefix
            if (env_name.cli_configured() && config.at("target_prefix").cli_configured())
            {
                LOG_ERROR << "Cannot set both prefix and env name";
                throw std::runtime_error("Aborting.");
            }

            // Consider file spec environment name as env_name specified at CLI level
            if (!env_name.configured() && spec_file_env_name.configured())
            {
                name = spec_file_name;
                env_name.set_cli_value<std::string>(spec_file_name);
            }

            if (!name.empty())
            {
                fs::path prefix;
                if (name == "base")
                {
                    prefix = root_prefix;
                }
                else
                {
                    prefix = root_prefix / "envs" / name;
                }

                if (!config.at("target_prefix").cli_configured()
                    && config.at("env_name").cli_configured())
                    config.at("target_prefix").set_cli_value<fs::path>(prefix);

                if (!config.at("target_prefix").api_configured()
                    && config.at("env_name").api_configured())
                    config.at("target_prefix").set_value(prefix);
            }
        }

        void target_prefix_hook(fs::path& prefix)
        {
            auto& config = Configuration::instance();
            auto& root_prefix = config.at("root_prefix").value<fs::path>();

            if (!prefix.empty())
            {
                if (prefix.string().find_first_of("/\\") == std::string::npos)
                {
                    std::string old_prefix = prefix.string();
                    prefix = root_prefix / "envs" / prefix;
                    LOG_WARNING << unindent((R"(
                                    ')" + old_prefix
                                             + R"(' does not contain any filesystem separator.
                                    It will be handled as env name, resulting to the following
                                    'target_prefix': ')"
                                             + prefix.string() + R"('
                                    If 'target_prefix' is expressed as a relative directory to
                                    the current working directory, use './some_prefix')")
                                                .c_str());
                }
            }
            else
            {
                bool use_fallback = config.at("use_target_prefix_fallback").value<bool>();
                if (use_fallback)
                    prefix = std::getenv("CONDA_PREFIX") ? std::getenv("CONDA_PREFIX") : "";
            }

#ifdef _WIN32
            std::string sep = "\\";
#else
            std::string sep = "/";
#endif
            if (!prefix.empty())
                prefix = rstrip(fs::weakly_canonical(env::expand_user(prefix)).string(), sep);

            if ((prefix == root_prefix)
                && Configuration::instance().at("create_base").value<bool>())
            {
                path::touch(root_prefix / "conda-meta" / "history", true);
            }
        }

        void root_prefix_hook(fs::path& prefix)
        {
            auto& env_name = Configuration::instance().at("env_name");

            if (prefix.empty())
            {
                if (env::get("MAMBA_DEFAULT_ROOT_PREFIX").empty())
                {
                    prefix = env::home_directory() / "micromamba";
                }
                else
                {
                    prefix = env::get("MAMBA_DEFAULT_ROOT_PREFIX");
                    LOG_WARNING << unindent(R"(
                                    'MAMBA_DEFAULT_ROOT_PREFIX' is meant for testing purpose.
                                    Consider using 'MAMBA_ROOT_PREFIX' instead)");
                }

                if (env_name.configured())
                {
                    LOG_WARNING << "'root_prefix' set with default value: " << prefix.string();
                }

                if (fs::exists(prefix) && !fs::is_empty(prefix))
                {
                    if (!(fs::exists(prefix / "pkgs") || fs::exists(prefix / "conda-meta")
                          || fs::exists(prefix / "envs")))
                    {
                        LOG_ERROR << "Could not use default 'root_prefix': " << prefix.string();
                        LOG_ERROR << "Directory exists, is not empty and not a conda prefix.";
                        exit(1);
                    }
                }

                if (env_name.configured())
                {
                    LOG_INFO << unindent(R"(
                            You have not set the 'root_prefix' environment variable.
                            To permanently modify the root prefix location, either:
                            - set the 'MAMBA_ROOT_PREFIX' environment variable
                            - use the '-r,--root-prefix' CLI option
                            - use 'micromamba shell init ...' to initialize your shell
                                (then restart or source the contents of the shell init script))");
                }
            }

            prefix = fs::weakly_canonical(env::expand_user(prefix));

            auto& ctx = Context::instance();
            ctx.envs_dirs = { prefix / "envs" };
            ctx.pkgs_dirs = { prefix / "pkgs" };
        }
        /*
                void log_level_hook(LogLevel& lvl)
                {
                    MessageLogger::global_log_level() = lvl;
                }
        */
        void verbose_hook(std::uint8_t& lvl)
        {
            auto& ctx = Context::instance();
            /*
            auto& config = Configuration::instance();
            auto& log_level = config.at("log_level").get_wrapped<LogLevel>();

            if (config.at("verbose").configured()
                && (config.at("verbose").value<std::uint8_t>() > 0))
            {
                if (log_level.cli_configured() || log_level.api_configured()
                    || log_level.env_var_configured())
                {
                    LOG_ERROR << "'verbose' can't override 'log_level' (skipped)";
                    config.at("verbose")
                        .clear_values();  // Avoid the error message appears multiple times
                }
                else
                {
                    ctx.set_verbosity(lvl);
                    // Make it appears like set with the CLI
                    // TODO: find a better way than passing by YAML to convert to string?
                    log_level.set_cli_config(YAML::Node(ctx.verbosity).as<std::string>());
                }
            }
            */
            ctx.set_verbosity(lvl);
        }

        void target_prefix_checks_hook(int& options)
        {
            auto& ctx = Context::instance();
            auto& prefix = ctx.target_prefix;

            bool no_checks = options & MAMBA_NO_PREFIX_CHECK;
            bool allow_missing = options & MAMBA_ALLOW_MISSING_PREFIX;
            bool allow_not_env = options & MAMBA_ALLOW_NOT_ENV_PREFIX;
            bool allow_existing = options & MAMBA_ALLOW_EXISTING_PREFIX;
            bool expect_existing = options & MAMBA_EXPECT_EXISTING_PREFIX;

            if (no_checks)
                return;

            if (prefix.empty())
            {
                if (allow_missing)
                {
                    return;
                }
                else
                {
                    LOG_ERROR << "No target prefix specified";
                    throw std::runtime_error("Aborting.");
                }
            }

            if (fs::exists(prefix))
            {
                if (!allow_existing)
                {
                    LOG_ERROR << "Not allowed pre-existing prefix: " << prefix.string();
                    throw std::runtime_error("Aborting.");
                }

                if (!fs::exists(prefix / "conda-meta") && !allow_not_env)
                {
                    LOG_ERROR << "Expected environment not found at prefix: " << prefix.string();
                    throw std::runtime_error("Aborting.");
                }
            }
            else if (expect_existing)
            {
                LOG_ERROR << "No prefix found at: " << prefix.string();
                throw std::runtime_error("Aborting.");
            }
        }

        void show_banner_hook(bool& show)
        {
            if (show)
                Console::print(banner());
        }

        void rc_file_hook(std::vector<fs::path>& files)
        {
            auto& ctx = Context::instance();
            auto& config = Configuration::instance();

            config.clear_rc_values();

            if (!files.empty())
            {
                if (ctx.no_rc)
                {
                    LOG_ERROR << "'rc_file' disabled by 'no_rc'";
                    throw std::runtime_error("Aborting.");
                }
                for (auto& f : files)
                {
                    if (!fs::exists(f))
                    {
                        LOG_ERROR << "'rc_file' specified but does not exist at: " << f.string();
                        throw std::runtime_error("Aborting.");
                    }
                }
                config.set_possible_rc_sources(files);
            }
            else
            {
                config.set_possible_rc_sources();
            }

            if (!ctx.no_rc)
            {
                config.load_rc_files();
                config.at("no_env").compute(MAMBA_CONF_FORCE_COMPUTE).set_context();
                config.at("always_yes").compute(MAMBA_CONF_FORCE_COMPUTE).set_context();
                config.at("quiet").compute(MAMBA_CONF_FORCE_COMPUTE).set_context();
                config.at("json").compute(MAMBA_CONF_FORCE_COMPUTE).set_context();
            }
        }

        void experimental_hook(bool& value)
        {
            if (value)
                LOG_WARNING << "Experimental mode enabled";
        }

        void debug_hook(bool& value)
        {
            if (value)
                LOG_WARNING << "Debug mode enabled";
        }

        void print_config_only_hook(bool& value)
        {
            if (value)
            {
                if (!Configuration::instance().at("debug").value<bool>())
                {
                    LOG_ERROR << "Debug mode required to use 'print_config_only'";
                    throw std::runtime_error("Aborting.");
                }
                Configuration::instance().at("quiet").set_value(true);
            }
        }

        void print_context_only_hook(bool& value)
        {
            if (value)
            {
                if (!Configuration::instance().at("debug").value<bool>())
                {
                    LOG_ERROR << "Debug mode required to use 'print_context_only'";
                    throw std::runtime_error("Aborting.");
                }
                Configuration::instance().at("quiet").set_value(true);
            }
        }
    }

    fs::path get_conda_root_prefix()
    {
        std::vector<std::string> args = { "conda", "config", "--show", "root_prefix", "--json" };
        std::string out, err;
        auto [status, ec] = reproc::run(
            args, reproc::options{}, reproc::sink::string(out), reproc::sink::string(err));

        if (ec)
        {
            LOG_ERROR << "Conda root prefix not found using 'conda config' command";
            throw std::runtime_error("Aborting.");
        }
        else
        {
            auto j = nlohmann::json::parse(out);
            return j.at("root_prefix").get<std::string>();
        }
    }

    void use_conda_root_prefix(bool force)
    {
        if (!Configuration::instance().at("root_prefix").configured() || force)
        {
            env::set("MAMBA_ROOT_PREFIX", get_conda_root_prefix());
        }
    }

    Configuration::Configuration()
    {
        set_configurables();
    }

    Configuration& Configuration::instance()
    {
        static Configuration config;
        return config;
    }

    void Configuration::set_configurables()
    {
        auto& ctx = Context::instance();

        // Basic
        insert(Configurable("root_prefix", &ctx.root_prefix)
                   .group("Basic")
                   .set_env_var_name()
                   .needs({ "verbose", "create_base" })
                   .description("Path to the root prefix")
                   .set_post_build_hook(detail::root_prefix_hook));

        insert(Configurable("create_base", false)
                   .group("Basic")
                   .set_single_op_lifetime()
                   .description("Define if base environment will be initialized empty"));

        insert(Configurable("target_prefix", &ctx.target_prefix)
                   .group("Basic")
                   .set_env_var_name()
                   .needs({ "root_prefix",
                            "env_name",
                            "spec_file_env_name",
                            "use_target_prefix_fallback",
                            "verbose",
                            "always_yes" })
                   .set_single_op_lifetime()
                   .description("Path to the target prefix")
                   .set_post_build_hook(detail::target_prefix_hook));

        insert(Configurable("use_target_prefix_fallback", true)
                   .group("Basic")
                   .set_single_op_lifetime()
                   .description("Fallback to the current target prefix or not"));

        insert(Configurable("target_prefix_checks", MAMBA_NO_PREFIX_CHECK)
                   .group("Basic")
                   .needs({ "target_prefix", "rc_file" })
                   .description("The type of checks performed on the target prefix")
                   .set_single_op_lifetime()
                   .set_post_build_hook(detail::target_prefix_checks_hook));

        insert(Configurable("env_name", std::string(""))
                   .group("Basic")
                   .needs({ "root_prefix", "spec_file_env_name" })
                   .set_single_op_lifetime()
                   .set_post_build_hook(detail::env_name_hook)
                   .description("Name of the target prefix"));

        insert(Configurable("platform", &ctx.platform)
                   .group("Basic")
                   .set_rc_configurable()
                   .set_env_var_name("CONDA_SUBDIR")
                   .description("The platform description")
                   .long_description(unindent(R"(
                        The plaftorm description points what channels
                        subdir(s) have to fetched for package solving.
                        This can be 'linux-64' or similar.)")));

        insert(Configurable("spec_file_env_name", std::string(""))
                   .group("Basic")
                   .needs({ "file_specs", "root_prefix" })
                   .set_single_op_lifetime()
                   .set_post_build_hook(detail::file_spec_env_name_hook)
                   .description("Name of the target prefix, specified in a YAML spec file"));

        insert(Configurable("specs", std::vector<std::string>({}))
                   .group("Basic")
                   .needs({ "file_specs" })  // explicit file specs overwrite current specs
                   .set_single_op_lifetime()
                   .description("Packages specification"));

        insert(Configurable("experimental", &ctx.experimental)
                   .group("Basic")
                   .description("Enable experimental features")
                   .set_rc_configurable()
                   .set_env_var_name()
                   .long_description(unindent(R"(
                        Enable experimental features that may be still.
                        under active development and not stable yet.)"))
                   .set_post_build_hook(detail::experimental_hook));

        insert(Configurable("debug", &ctx.debug)
                   .group("Basic")
                   .set_env_var_name()
                   .description("Turn on the debug mode")
                   .long_description(unindent(R"(
                        Turn on the debug mode that allow introspection
                        in intermediate steps of the operation called.
                        Debug features may/will interrupt the operation,
                        if you only need further logs refer to 'verbose'.)"))
                   .set_post_build_hook(detail::debug_hook));

        // Channels
        insert(Configurable("channels", &ctx.channels)
                   .group("Channels")
                   .set_rc_configurable()
                   .set_env_var_name("CONDA_CHANNELS")
                   .description("Define the list of channels")
                   .needs({ "file_specs" })
                   .long_description(unindent(R"(
                        The list of channels where the packages will be searched for.
                        See also 'channel_priority'.)")));

        insert(Configurable("channel_alias", &ctx.channel_alias)
                   .group("Channels")
                   .set_rc_configurable()
                   .set_env_var_name()
                   .description("The prepended url location to associate with channel names"));

        insert(Configurable("default_channels", &ctx.default_channels)
                   .group("Channels")
                   .set_rc_configurable()
                   .set_env_var_name()
                   .description("Default channels used")
                   .long_description(unindent(R"(
                        The list of channel names and/or urls used for the 'defaults'
                        multichannel.)")));

        insert(Configurable("override_channels_enabled", &ctx.override_channels_enabled)
                   .group("Channels")
                   .set_rc_configurable()
                   .set_env_var_name()
                   .description("Permit use of the --overide-channels command-line flag"));

        // Network
        insert(Configurable("cacert_path", std::string(""))
                   .group("Network")
                   .set_rc_configurable()
                   .set_env_var_name()
                   .description("Path (file or directory) SSL certificate(s)")
                   .long_description(unindent(R"(
                        Path (file or directory) SSL certificate(s) to use whe
                        'ssl_verify' in turned on but not set with path to certs.
                        WARNING: overrides 'ssl_verify' if provided and 'ssl_verify'
                        also contains a path to SSL certificates.)")));

        insert(Configurable("local_repodata_ttl", &ctx.local_repodata_ttl)
                   .group("Network")
                   .set_rc_configurable()
                   .description("Repodata time-to-live")
                   .long_description(unindent(R"(
                        For a value of 0, always fetch remote repodata (HTTP 304
                        responses respected).
                        For a value of 1, respect the HTTP Cache-Control max-age header.
                        Any other positive integer values is the number of seconds to
                        locally cache repodata before checking the remote server for
                        an update.)")));

        insert(Configurable("offline", &ctx.offline)
                   .group("Network")
                   .set_rc_configurable()
                   .set_env_var_name()
                   .description("Force use cached repodata"));

        insert(Configurable("ssl_no_revoke", &ctx.ssl_no_revoke)
                   .group("Network")
                   .set_rc_configurable()
                   .set_env_var_name()
                   .description("SSL certificate revocation checks")
                   .long_description(unindent(R"(
                        This option tells curl to disable certificate revocation checks.
                        It's only working for Windows back-end.
                        WARNING: this option loosens the SSL security.)")));

        insert(Configurable("ssl_verify", &ctx.ssl_verify)
                   .group("Network")
                   .set_rc_configurable()
                   .set_env_var_name()
                   .description("Verify SSL certificates for HTTPS requests")
                   .long_description(unindent(R"(
                        'ssl_verify' can be either an empty string (regular SSL verification),
                        the string "<false>" to indicate no SSL verification, or a path to
                        a directory with cert files, or a cert file..)"))
                   .needs({ "cacert_path", "offline" })
                   .set_post_build_hook(detail::ssl_verify_hook));

        // Solver
        insert(Configurable("channel_priority", &ctx.channel_priority)
                   .group("Solver")
                   .set_rc_configurable()
                   .set_env_var_name()
                   .description("Define the channel priority ('strict' or 'disabled')")
                   .long_description(unindent(R"(
                        Accepts values of 'strict' and 'disabled'. The default
                        value is 'strict'. With strict channel priority, packages in lower
                        priority channels are not considered if a package with the same name
                        appears in a higher priority channel.
                        With channel priority disabled, package version takes precedence, and the
                        configured priority of channels is used only to break ties. In
                        previous versions of conda, this parameter was configured as either
                        True or False. True is now an alias to 'flexible'.)")));

        insert(Configurable("explicit_install", false)
                   .group("Solver")
                   .description("Use explicit install instead of solving environment"));

        insert(Configurable("file_specs", std::vector<std::string>({}))
                   .group("Solver")
                   .set_post_build_hook(detail::file_specs_hook)
                   .description("File (yaml, explicit or plain)"));

        insert(Configurable("no_pin", false)
                   .group("Solver")
                   .set_env_var_name()
                   .description("Ignore pinned packages"));

        insert(Configurable("pinned_packages", &ctx.pinned_packages)
                   .group("Solver")
                   .set_rc_configurable()
                   .set_env_var_name()
                   .description("A list of package specs to pin for every environment resolution"));

        insert(Configurable("retry_clean_cache", false)
                   .group("Solver")
                   .set_env_var_name()
                   .description("If solve fails, try to fetch updated repodata"));

        // Link & Install
        insert(Configurable("allow_softlinks", &ctx.allow_softlinks)
                   .group("Link & Install")
                   .set_rc_configurable()
                   .set_env_var_name()
                   .description("Allow to use soft-links when hard-links are not possible")
                   .long_description(unindent(R"(
                        Allow to use soft-links (symlinks) when hard-links are not possible,
                        such as when installing on a different filesystem than the one that
                        the package cache is on.)")));

        insert(Configurable("always_copy", &ctx.always_copy)
                   .group("Link & Install")
                   .set_rc_configurable()
                   .set_env_var_name()
                   .description("Use copy instead of hard-link")
                   .long_description(unindent(R"(
                        Register a preference that files be copied into a prefix during
                        install rather than hard-linked.)")));

        insert(Configurable("always_softlink", &ctx.always_softlink)
                   .group("Link & Install")
                   .set_rc_configurable()
                   .set_env_var_name()
                   .needs({ "always_copy" })
                   .set_post_build_hook(detail::always_softlink_hook)
                   .description("Use soft-link instead of hard-link")
                   .long_description(unindent(R"(
                        Register a preference that files be soft-linked (symlinked) into a
                        prefix during install rather than hard-linked. The link source is the
                        package cache from where the package is being linked.
                        !WARNING: Using this option can result in corruption of long-lived
                        environments due to broken links (deleted cache).)")));

        insert(Configurable("safety_checks", &ctx.safety_checks)
                   .group("Link & Install")
                   .set_rc_configurable()
                   .set_env_var_name()
                   .description("Safety checks policy ('enabled', 'warn', or 'disabled')")
                   .long_description(unindent(R"(
                        Enforce available safety guarantees during package installation. The
                        value must be one of 'enabled', 'warn', or 'disabled'.)")));

        insert(Configurable("extra_safety_checks", &ctx.extra_safety_checks)
                   .group("Link & Install")
                   .set_rc_configurable()
                   .set_env_var_name()
                   .description("Run extra verifications on packages")
                   .long_description(unindent(R"(
                        Spend extra time validating package contents. Currently, runs sha256
                        verification on every file within each package during installation.)")));

        insert(Configurable("artifact_verif", &ctx.artifact_verif)
                   .group("Link & Install")
                   .set_rc_configurable()
                   .set_env_var_name()
                   .description("Run verifications on packages signatures")
                   .long_description(unindent(R"(
                        Spend extra time validating package contents. It consists of running
                        cryptographic verifications on channels and packages metadata.)")));

        // Output, Prompt and Flow
        insert(Configurable("always_yes", &ctx.always_yes)
                   .group("Output, Prompt and Flow Control")
                   .set_rc_configurable()
                   .set_env_var_name()
                   .description("Automatically answer yes on prompted questions"));

        insert(Configurable("auto_activate_base", &ctx.auto_activate_base)
                   .group("Output, Prompt and Flow Control")
                   .set_rc_configurable()
                   .set_env_var_name()
                   .description("Automatically activate the base env")
                   .long_description(unindent(R"(
                        Automatically activate the base environment during shell
                        initialization.)")));

        insert(Configurable("dry_run", &ctx.dry_run)
                   .group("Output, Prompt and Flow Control")
                   .set_env_var_name()
                   .description("Only display what would have been done"));
        /*
                insert(Configurable("log_level", &ctx.verbosity)
                           .group("Output, Prompt and Flow Control")
                           .set_env_var_name()
                           .description("Set the log level")
                           .long_description(unindent(R"(
                            Set the log level. Log level can be one of {'off', 'fatal',
                            'error', 'warning', 'info', 'debug', 'trace'}.)"))
                           .set_post_build_hook(detail::log_level_hook));
        */
        insert(Configurable("json", &ctx.json)
                   .group("Output, Prompt and Flow Control")
                   .set_rc_configurable()
                   .set_env_var_name()
                   .description("Report all output as json"));

        insert(Configurable("print_config_only", false)
                   .group("Output, Prompt and Flow Control")
                   .needs({ "debug" })
                   .set_post_build_hook(detail::print_config_only_hook)
                   .description("Print the config after loading. Allow ultra-dry runs"));

        insert(
            Configurable("print_context_only", false)
                .group("Output, Prompt and Flow Control")
                .needs({ "debug" })
                .set_post_build_hook(detail::print_context_only_hook)
                .description("Print the context after loading the config. Allow ultra-dry runs"));

        insert(Configurable("show_banner", true)
                   .group("Output, Prompt and Flow Control")
                   .needs({ "quiet", "json" })
                   .set_post_build_hook(detail::show_banner_hook)
                   .set_single_op_lifetime()
                   .description("Show the banner"));

        insert(Configurable("show_all_configs", false)
                   .group("Output, Prompt and Flow Control")
                   .description("Display all configurables, including not rc configurable"));

        insert(Configurable("show_config_descriptions", false)
                   .group("Output, Prompt and Flow Control")
                   .description("Display configurables descriptions"));

        insert(Configurable("show_config_groups", false)
                   .group("Output, Prompt and Flow Control")
                   .description("Display configurables groups"));

        insert(Configurable("show_config_long_descriptions", false)
                   .group("Output, Prompt and Flow Control")
                   .description("Display configurables long descriptions"));

        insert(Configurable("show_config_sources", false)
                   .group("Output, Prompt and Flow Control")
                   .description("Display all identified configuration sources"));

        insert(Configurable("show_config_values", false)
                   .group("Output, Prompt and Flow Control")
                   .description("Display configurables values"));

        insert(Configurable("quiet", &ctx.quiet)
                   .group("Output, Prompt and Flow Control")
                   .set_rc_configurable()
                   .set_env_var_name()
                   .needs({ "json" })
                   .implies({ "show_banner" })
                   .description("Set quiet mode (print less output)"));

        insert(Configurable("verbose", std::uint8_t(0))
                   .group("Output, Prompt and Flow Control")
                   .set_post_build_hook(detail::verbose_hook)
                   .description("Set higher verbosity")
                   .long_description(unindent(R"(
                    Set a higher log verbosity than the default one.
                    This configurable has a similar effect as 'log_level',
                    except it can only increase the log level. If you need
                    fine-grained control, prefer 'log_level'.
                    'verbose' and 'log_level' are exclusive.)")));

        // Config
        insert(Configurable("rc_file", std::vector<fs::path>({}))
                   .group("Config sources")
                   .set_env_var_name()
                   .needs({ "no_rc", "target_prefix", "root_prefix" })
                   .set_post_build_hook(detail::rc_file_hook)
                   .description("Path to the unique configuration file to use"));

        insert(Configurable("no_rc", &ctx.no_rc)
                   .group("Config sources")
                   .set_env_var_name()
                   .description("Disable the use of configuration files"));

        insert(Configurable("no_env", &ctx.no_env)
                   .group("Config sources")
                   .set_env_var_name()
                   .description("Disable the use of environment variables"));
    }

    void Configuration::reset_configurables()
    {
        m_config.clear();
        m_config_order.clear();
        set_configurables();
    }

    std::vector<std::pair<std::string, std::vector<ConfigurableInterface*>>>
    Configuration::get_grouped_config()
    {
        std::map<std::string, std::vector<ConfigurableInterface*>> map;
        std::vector<std::pair<std::string, std::vector<ConfigurableInterface*>>> res;
        std::vector<std::string> group_order;

        for (auto& name : m_config_order)
        {
            auto& c = m_config.at(name);

            if (map.count(c.group()) == 0)
            {
                group_order.push_back(c.group());
            }
            map[c.group()].push_back(&c);
        }

        for (auto& g : group_order)
        {
            res.push_back({ g, map.at(g) });
        }

        return res;
    }

    void Configuration::set_possible_rc_sources()
    {
        auto& ctx = Context::instance();

        fs::path condarc_env_var = std::getenv("CONDARC") ? std::getenv("CONDARC") : "";
        fs::path mambarc_env_var = std::getenv("MAMBARC") ? std::getenv("MAMBARC") : "";

        if (on_mac || on_linux)
        {
            m_sources = { "/etc/conda/.condarc",       "/etc/conda/condarc",
                          "/etc/conda/condarc.d/",     "/etc/conda/.mambarc",
                          "/var/lib/conda/.condarc",   "/var/lib/conda/condarc",
                          "/var/lib/conda/condarc.d/", "/var/lib/conda/.mambarc" };
        }
        else
        {
            m_sources = { "C:\\ProgramData\\conda\\.condarc",
                          "C:\\ProgramData\\conda\\condarc",
                          "C:\\ProgramData\\conda\\condarc.d",
                          "C:\\ProgramData\\conda\\.mambarc" };
        }

        std::vector<std::string> not_os_spe = { ctx.root_prefix / ".condarc",
                                                ctx.root_prefix / "condarc",
                                                ctx.root_prefix / "condarc.d",
                                                ctx.root_prefix / ".mambarc",
                                                env::home_directory() / ".conda/.condarc",
                                                env::home_directory() / ".conda/condarc",
                                                env::home_directory() / ".conda/condarc.d",
                                                env::home_directory() / ".condarc",
                                                env::home_directory() / ".mambarc",
                                                ctx.target_prefix / ".condarc",
                                                ctx.target_prefix / "condarc",
                                                ctx.target_prefix / "condarc.d",
                                                ctx.target_prefix / ".mambarc",
                                                condarc_env_var,
                                                mambarc_env_var };

        m_sources.insert(m_sources.end(), not_os_spe.begin(), not_os_spe.end());

        // Sort by precedence
        std::reverse(m_sources.begin(), m_sources.end());

        update_sources();
    }

    void Configuration::set_possible_rc_sources(const std::vector<fs::path>& sources)
    {
        m_sources = sources;
        update_sources();
    }

    void Configuration::load()
    {
        compute_loading_sequence();
        reset_compute_counters();

        m_load_lock = true;
        for (auto& c : m_loading_sequence)
        {
            at(c).compute().set_context();
        }
        m_load_lock = false;
        CONFIG_DEBUGGING;
    }

    bool Configuration::is_loading()
    {
        return m_load_lock;
    }

    void Configuration::compute_loading_sequence()
    {
        // break circular dependencies
        // target_prefix (env configurable) -> no_env (rc configurable) -> rc_file -> target_prefix
        // target_prefix -> always_yes (rc configurable) -> rc_file -> target_prefix
        // hack to eventually display the banner before log messages. Needs to recompute those
        // rc configurable configs in rc_file_hook if any rc file is used
        m_loading_sequence
            = { "no_env", "always_yes", "debug",      "print_context_only", "print_config_only",
                "quiet",  "json",       "show_banner" };

        for (auto& c : m_config_order)
        {
            add_to_loading_sequence(m_loading_sequence, c);
        }
    }

    void Configuration::add_to_loading_sequence(std::vector<std::string>& seq,
                                                const std::string& name)
    {
        auto found = std::find(seq.begin(), seq.end(), name);

        if (found == seq.end())
        {
            at(name).lock();

            for (auto& n : at(name).needed())
            {
                if (at(n).locked())
                {
                    throw std::runtime_error("Circular import detected in Configuration at '" + n
                                             + "'");
                }
                add_to_loading_sequence(seq, n);
            }

            // The given config may have been added by implied configs
            found = std::find(seq.begin(), seq.end(), name);
            if (found == seq.end())
                seq.push_back(name);

            at(name).free();

            for (auto& n : at(name).implied())
            {
                add_to_loading_sequence(seq, n);
            }
        }
    }

    void Configuration::reset_compute_counters()
    {
        for (auto& c : m_config)
        {
            c.second.reset_compute_counter();
        }
    }

    void Configuration::clear_rc_values()
    {
        for (auto& c : m_config)
        {
            c.second.clear_rc_values();
        }
    }

    void Configuration::clear_cli_values()
    {
        for (auto& c : m_config)
        {
            c.second.clear_cli_value();
        }
    }

    void Configuration::operation_teardown()
    {
        for (auto& c : m_config)
        {
            if (c.second.has_single_op_lifetime())
            {
                c.second.clear_values();
            }
            else
            {
                c.second.clear_cli_value();
            }
        }
    }

    std::vector<fs::path> Configuration::sources()
    {
        return m_sources;
    }

    std::vector<fs::path> Configuration::valid_sources()
    {
        return m_valid_sources;
    }

    std::map<std::string, ConfigurableInterface>& Configuration::config()
    {
        return m_config;
    }

    ConfigurableInterface& Configuration::at(const std::string& name)
    {
        try
        {
            return m_config.at(name);
        }
        catch (const std::out_of_range& e)
        {
            LOG_ERROR << "Configurable '" << name << "' does not exists";
            throw e;
        }
    }

    YAML::Node Configuration::load_rc_file(const fs::path& file)
    {
        YAML::Node config;
        try
        {
            config = YAML::LoadFile(file);
        }
        catch (...)
        {
            LOG_ERROR << "Error in file " << file << " (Skipped)";
        }
        return config;
    }

    void Configuration::load_rc_files()
    {
        std::vector<std::shared_ptr<YAML::Node>> rc_yaml_values;
        std::map<std::shared_ptr<YAML::Node>, std::string> sources_map;

        m_valid_sources.clear();
        for (auto s : m_sources)
        {
            auto node = load_rc_file(s);
            if (!node.IsNull())
            {
                auto node_ptr = std::make_shared<YAML::Node>(node);
                rc_yaml_values.push_back(node_ptr);
                sources_map.insert({ node_ptr, env::shrink_user(s).string() });
                m_valid_sources.push_back(s);
            }
        }

        if (rc_yaml_values.size() > 0)
        {
            for (auto& it : m_config)
            {
                auto& key = it.first;
                auto& c = it.second;

                if (!c.rc_configurable())
                    continue;

                for (auto s : rc_yaml_values)
                {
                    if (!(*s)[key] || (*s)[key].IsNull())
                    {
                        continue;
                    }
                    c.set_rc_yaml_value((*s)[key], sources_map.at(s));
                }
            }
        }
    }

    void Configuration::update_sources()
    {
        std::vector<fs::path> sources;

        for (const fs::path& l : m_sources)
        {
            if (detail::is_config_file(l))
            {
                sources.push_back(l);
                LOG_DEBUG << "Configuration found at '" << l.string() << "'";
            }
            else if (fs::is_directory(l))
            {
                for (fs::path p : fs::directory_iterator(l))
                {
                    if (detail::is_config_file(p))
                    {
                        sources.push_back(p);
                        LOG_DEBUG << "Configuration found at '" << p.string() << "'";
                    }
                    else
                    {
                        LOG_DEBUG << "Configuration not found at '" << p.string() << "'";
                    }
                }
            }
            else
            {
                if (!l.empty())
                    LOG_DEBUG << "Configuration not found at '" << l.string() << "'";
            }
        }

        m_sources = sources;
    }

    std::string Configuration::dump(int opts, std::vector<std::string> names)
    {
        bool show_values = opts & MAMBA_SHOW_CONFIG_VALUES;
        bool show_sources = opts & MAMBA_SHOW_CONFIG_SRCS;
        bool show_descs = opts & MAMBA_SHOW_CONFIG_DESCS;
        bool show_long_descs = opts & MAMBA_SHOW_CONFIG_LONG_DESCS;
        bool show_groups = opts & MAMBA_SHOW_CONFIG_GROUPS;
        bool show_all = opts & MAMBA_SHOW_ALL_CONFIGS;

        bool first_config = true;
        YAML::Emitter out;
        // out.SetNullFormat(YAML::EMITTER_MANIP::LowerNull); // TODO: switch from ~ to null

        for (auto& group_it : get_grouped_config())
        {
            auto& group_name = group_it.first;
            auto& configs = group_it.second;
            bool first_group_config = true;

            for (auto& c : configs)
            {
                auto is_required = std::find(names.begin(), names.end(), c->name()) != names.end();
                if (!names.empty() && !is_required)
                {
                    continue;
                }

                if ((c->rc_configurable() && c->configured()) || is_required || show_all)
                {
                    if (show_descs || show_long_descs)
                    {
                        if (show_groups && first_group_config)
                        {
                            if (!first_config)
                                out << YAML::Newline << YAML::Newline;
                            detail::print_group_title(out, group_name);
                        }

                        if (!first_config || (first_config && show_groups))
                            out << YAML::Newline << YAML::Newline;

                        out << YAML::Comment(c->name()) << YAML::Newline;
                        if (show_long_descs)
                        {
                            out << YAML::Comment(prepend(c->long_description(), "  ", "  "));
                        }
                        else
                        {
                            out << YAML::Comment(prepend(c->description(), "  ", "  "));
                        }
                    }

                    if (show_values)
                    {
                        if (first_config)
                            out << YAML::BeginMap;
                        out << YAML::Key << c->name();
                        out << YAML::Value;
                        detail::print_configurable(out, *c, show_sources);
                    }

                    first_config = false;
                    first_group_config = false;
                }
            }
        }
        if (show_values && !first_config)
            out << YAML::EndMap;

        return out.c_str();
    }

    namespace detail
    {
        bool has_config_name(const std::string& file)
        {
            return fs::path(file).filename() == ".condarc" || fs::path(file).filename() == "condarc"
                   || fs::path(file).filename() == ".mambarc"
                   || fs::path(file).filename() == "mambarc" || ends_with(file, ".yml")
                   || ends_with(file, ".yaml");
        }

        bool is_config_file(const fs::path& path)
        {
            return fs::exists(path) && (!fs::is_directory(path)) && has_config_name(path.string());
        }

        void print_scalar_node(YAML::Emitter& out,
                               YAML::Node value,
                               YAML::Node source,
                               bool show_source)
        {
            if (!value.IsScalar())
            {
                throw std::runtime_error("Invalid scalar value");
            }
            out << value;

            if (show_source)
            {
                if (source.IsScalar())
                {
                    out << YAML::Comment("'" + source.as<std::string>() + "'");
                }
                else
                {
                    auto srcs = source.as<std::vector<std::string>>();
                    std::string comment = "'" + srcs.at(0) + "'";
                    for (std::size_t i = 1; i < srcs.size(); ++i)
                    {
                        comment += " > '" + srcs.at(i) + "'";
                    }
                    out << YAML::Comment(comment);
                }
            }
        }

        void print_seq_node(YAML::Emitter& out,
                            YAML::Node value,
                            YAML::Node source,
                            bool show_source)
        {
            if (!value.IsSequence())
            {
                throw std::runtime_error("Invalid sequence value");
            }

            if (value.size() > 0)
            {
                out << YAML::BeginSeq;
                for (std::size_t n = 0; n < value.size(); ++n)
                {
                    if (value[n].IsScalar())
                    {
                        print_scalar_node(out, value[n], source[n], show_source);
                    }
                    if (value[n].IsSequence())
                    {
                        print_seq_node(out, value[n], source[n], show_source);
                    }
                    if (value[n].IsMap())
                    {
                        print_map_node(out, value[n], source[n], show_source);
                    }
                }
                out << YAML::EndSeq;
            }
            else
            {
                out << YAML::_Null();
                if (show_source)
                    out << YAML::Comment("'default'");
            }
        }

        void print_map_node(YAML::Emitter& out,
                            YAML::Node value,
                            YAML::Node source,
                            bool show_source)
        {
            if (!value.IsMap())
            {
                throw std::runtime_error("Invalid map value");
            }

            out << YAML::BeginMap;
            for (auto n : value)
            {
                auto key = n.first.as<std::string>();
                out << YAML::Key << n.first;
                out << YAML::Value;

                if (n.second.IsScalar())
                {
                    print_scalar_node(out, n.second, source[key], show_source);
                }
                if (n.second.IsSequence())
                {
                    print_seq_node(out, n.second, source[key], show_source);
                }
                if (n.second.IsMap())
                {
                    print_map_node(out, n.second, source[key], show_source);
                }
            }
            out << YAML::EndMap;
        }

        void print_configurable(YAML::Emitter& out,
                                const ConfigurableInterface& config,
                                bool show_source)
        {
            auto value = config.yaml_value();
            auto source = config.source();

            if (value.IsScalar())
            {
                print_scalar_node(out, value, source, show_source);
            }
            if (value.IsSequence())
            {
                print_seq_node(out, value, source, show_source);
            }
            if (value.IsMap())
            {
                print_map_node(out, value, source, show_source);
            }
        }

        void print_group_title(YAML::Emitter& out, const std::string& name)
        {
            auto group_title = name + " Configuration";
            auto blk_size = 52 - group_title.size();
            int prepend_blk = blk_size / 2;
            int append_blk = blk_size - prepend_blk;

            out << YAML::Comment(std::string(54, '#')) << YAML::Newline;
            out << YAML::Comment("#" + std::string(prepend_blk, ' ') + group_title
                                 + std::string(append_blk, ' ') + "#")
                << YAML::Newline;
            out << YAML::Comment(std::string(54, '#'));
        }
    }
}
