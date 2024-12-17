// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <algorithm>
#include <iostream>
#include <regex>
#include <stdexcept>

#include <nlohmann/json.hpp>
#include <reproc++/run.hpp>
#include <spdlog/spdlog.h>

#include "mamba/api/configuration.hpp"
#include "mamba/api/install.hpp"
#include "mamba/core/fsutil.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/package_fetcher.hpp"
#include "mamba/core/util.hpp"
#include "mamba/core/util_os.hpp"
#include "mamba/util/build.hpp"
#include "mamba/util/environment.hpp"
#include "mamba/util/path_manip.hpp"
#include "mamba/util/string.hpp"

namespace mamba
{
    /************************
     * ConfigurableImplBase *
     ************************/

    namespace detail
    {
        bool ConfigurableImplBase::env_var_configured() const
        {
            if (m_config == nullptr)
            {
                return false;
            }

            if (m_config->context().src_params.no_env)
            {
                return false;
            }

            for (const auto& env_var : m_env_var_names)
            {
                if (util::get_env(env_var))
                {
                    return true;
                }
            }
            return false;
        }

        bool ConfigurableImplBase::env_var_active() const
        {
            if (m_config == nullptr)
            {
                return false;
            }

            return !m_config->context().src_params.no_env || (m_name == "no_env");
        }

        bool ConfigurableImplBase::rc_configured() const
        {
            if (m_config == nullptr)
            {
                return false;
            }

            return m_rc_configured && !m_config->context().src_params.no_rc;
        }

        bool ConfigurableImplBase::is_config_loading() const
        {
            return m_config == nullptr || m_config->is_loading();
        }
    }

    /*******************************
     * Configurable implementation *
     *******************************/

    const std::string& Configurable::name() const
    {
        return p_impl->m_name;
    }

    const std::string& Configurable::group() const
    {
        return p_impl->m_group;
    }

    Configurable&& Configurable::group(const std::string& group)
    {
        p_impl->m_group = group;
        return std::move(*this);
    }

    const std::string& Configurable::description() const
    {
        return p_impl->m_description;
    }

    Configurable&& Configurable::description(const std::string& desc)
    {
        p_impl->m_description = desc;
        return std::move(*this);
    }

    const std::string& Configurable::long_description() const
    {
        return p_impl->m_long_description.empty() ? p_impl->m_description
                                                  : p_impl->m_long_description;
    }

    Configurable&& Configurable::long_description(const std::string& desc)
    {
        p_impl->m_long_description = desc;
        return std::move(*this);
    }

    const std::vector<std::string>& Configurable::sources() const
    {
        return p_impl->m_sources;
    }

    const std::vector<std::string>& Configurable::source() const
    {
        return p_impl->m_source;
    }

    const std::set<std::string>& Configurable::needed() const
    {
        return p_impl->m_needed_configs;
    }

    Configurable&& Configurable::needs(const std::set<std::string>& names)
    {
        p_impl->m_needed_configs.insert(names.cbegin(), names.cend());
        return std::move(*this);
    }

    const std::set<std::string>& Configurable::implied() const
    {
        return p_impl->m_implied_configs;
    }

    Configurable&& Configurable::implies(const std::set<std::string>& names)
    {
        p_impl->m_implied_configs.insert(names.cbegin(), names.cend());
        return std::move(*this);
    }

    bool Configurable::rc_configurable() const
    {
        return p_impl->m_rc_configurable;
    }

    RCConfigLevel Configurable::rc_configurable_level() const
    {
        return p_impl->m_rc_configurable_policy;
    }

    Configurable&& Configurable::set_rc_configurable(RCConfigLevel level)
    {
        p_impl->m_rc_configurable = true;
        p_impl->m_rc_configurable_policy = level;

        if (level == RCConfigLevel::kTargetPrefix)
        {
            p_impl->m_needed_configs.insert("target_prefix");
        }
        else
        {
            p_impl->m_needed_configs.insert("root_prefix");
        }

        return std::move(*this);
    }

    bool Configurable::rc_configured() const
    {
        return p_impl->rc_configured();
    }

    bool Configurable::env_var_configured() const
    {
        return p_impl->env_var_configured();
    }

    bool Configurable::cli_configured() const
    {
        return p_impl->cli_configured();
    }

    bool Configurable::api_configured() const
    {
        return p_impl->m_api_configured;
    }

    bool Configurable::configured() const
    {
        return rc_configured() || env_var_configured() || cli_configured() || api_configured();
    }

    bool Configurable::env_var_active() const
    {
        return p_impl->env_var_active();
    }

    Configurable&& Configurable::set_env_var_names(const std::vector<std::string>& names)
    {
        if (names.empty())
        {
            p_impl->m_env_var_names = { "MAMBA_" + util::to_upper(p_impl->m_name) };
        }
        else
        {
            p_impl->m_env_var_names = names;
        }

        if (name() != "no_env")
        {
            p_impl->m_needed_configs.insert("no_env");
        }

        return std::move(*this);
    }

    bool Configurable::has_single_op_lifetime() const
    {
        return p_impl->m_single_op_lifetime;
    }

    Configurable&& Configurable::set_single_op_lifetime()
    {
        p_impl->m_single_op_lifetime = true;
        return std::move(*this);
    }

    void Configurable::reset_compute_counter()
    {
        p_impl->m_compute_counter = 0;
    }

    void Configurable::lock()
    {
        p_impl->m_lock = true;
    }

    void Configurable::free()
    {
        p_impl->m_lock = false;
    }

    bool Configurable::locked()
    {
        return p_impl->m_lock;
    }

    Configurable&& Configurable::clear_rc_values()
    {
        p_impl->clear_rc_values();
        return std::move(*this);
    }

    Configurable&& Configurable::clear_env_values()
    {
        if (env_var_configured())
        {
            for (const auto& ev : p_impl->m_env_var_names)
            {
                util::unset_env(ev);
            }
        }
        return std::move(*this);
    }

    Configurable&& Configurable::clear_cli_value()
    {
        p_impl->clear_cli_value();
        return std::move(*this);
    }

    Configurable&& Configurable::clear_api_value()
    {
        p_impl->m_api_configured = false;
        return std::move(*this);
    }

    Configurable&& Configurable::clear_values()
    {
        clear_rc_values();
        clear_env_values();
        clear_cli_value();
        clear_api_value();
        p_impl->set_default_value();
        return std::move(*this);
    }

    Configurable&& Configurable::set_post_context_hook(post_context_hook_type hook)
    {
        p_impl->p_post_ctx_hook = hook;
        return std::move(*this);
    }

    Configurable&& Configurable::set_rc_yaml_value(const YAML::Node& value, const std::string& source)
    {
        p_impl->set_rc_yaml_value(value, source);
        return std::move(*this);
    }

    Configurable&& Configurable::set_rc_yaml_values(
        const std::map<std::string, YAML::Node>& values,
        const std::vector<std::string>& sources
    )
    {
        p_impl->set_rc_yaml_values(values, sources);
        return std::move(*this);
    }

    Configurable&& Configurable::set_cli_yaml_value(const YAML::Node& value)
    {
        p_impl->set_cli_yaml_value(value);
        return std::move(*this);
    }

    Configurable&& Configurable::set_cli_yaml_value(const std::string& value)
    {
        p_impl->set_cli_yaml_value(value);
        return std::move(*this);
    }

    Configurable&& Configurable::set_yaml_value(const YAML::Node& value)
    {
        p_impl->set_yaml_value(value);
        return std::move(*this);
    }

    Configurable&& Configurable::set_yaml_value(const std::string& value)
    {
        p_impl->set_yaml_value(value);
        return std::move(*this);
    }

    Configurable&& Configurable::compute(int options, const ConfigurationLevel& level)
    {
        p_impl->compute(options, level);
        return std::move(*this);
    }

    bool Configurable::is_valid_serialization(const std::string& value) const
    {
        return p_impl->is_valid_serialization(value);
    }

    bool Configurable::is_sequence() const
    {
        return p_impl->is_sequence();
    }

    YAML::Node Configurable::yaml_value() const
    {
        return p_impl->yaml_value();
    }

    void Configurable::dump_json(nlohmann::json& node, const std::string& name) const
    {
        p_impl->dump_json(node, name);
    }

    /*********
     * hooks *
     *********/

    static std::string expandvars(std::string s)
    {
        if (s.find("$") == std::string::npos)
        {
            // Bail out early
            return s;
        }
        std::regex env_var_re(R"(\$(\{\w+\}|\w+))");
        for (auto matches = std::sregex_iterator(s.begin(), s.end(), env_var_re);
             matches != std::sregex_iterator();
             ++matches)
        {
            std::smatch match = *matches;
            auto var = match[0].str();
            if (mamba::util::starts_with(var, "${"))
            {
                // strip ${ and }
                var = var.substr(2, var.size() - 3);
            }
            else
            {
                // strip $
                var = var.substr(1);
            }
            auto val = util::get_env(var);
            if (val)
            {
                s.replace(match[0].first, match[0].second, val.value());
                // It turns out to be unsafe to modify the string during
                // sregex_iterator iteration. Start a new search by recursing.
                return expandvars(s);
            }
        }
        return s;
    }

    namespace detail
    {
        void ssl_verify_hook(Configuration& config, std::string& value)
        {
            bool& offline = config.at("offline").value<bool>();
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

            auto& cacert = config.at("cacert_path").value<std::string>();
            if (!cacert.empty())
            {
                value = cacert;
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

        void always_softlink_hook(Configuration& config, bool& value)
        {
            auto& always_copy = config.at("always_copy").value<bool>();

            if (value && always_copy)
            {
                LOG_ERROR << "'always_softlink' and 'always_copy' are mutually exclusive.";
                throw std::runtime_error("Incompatible configuration. Aborting.");
            }
        }

        void file_spec_env_name_hook(std::string& name)
        {
            if (name.find_first_of("/\\") != std::string::npos)
            {
                throw std::runtime_error(
                    "An unexpected file-system separator was found in environment name: '" + name + "'"
                );
            }
        }

        namespace
        {
            /** Find the first directory containing the given subdirectory. */
            auto find_env_in_dirs(std::string_view name, const std::vector<fs::u8path>& dirs)
                -> std::optional<fs::u8path>
            {
                for (const auto& dir : dirs)
                {
                    const auto candidate = dir / name;
                    if (fs::exists(candidate) && fs::is_directory(candidate))
                    {
                        return candidate;
                    }
                }
                return std::nullopt;
            }

            /** Find the first directory that can create the given subdirectory. */
            auto find_writable_env_in_dirs(std::string_view name, const std::vector<fs::u8path>& dirs)
                -> std::optional<fs::u8path>
            {
                for (const auto& dir : dirs)
                {
                    const auto candidate = dir / name;
                    if (mamba::path::is_writable(candidate))
                    {
                        return candidate;
                    }
                }
                return std::nullopt;
            }

            auto compute_prefix_from_name(
                const fs::u8path& root_prefix,
                const std::vector<fs::u8path>& envs_dirs,
                std::string_view name
            ) -> fs::u8path
            {
                if (name == "base")
                {
                    return root_prefix;
                }
                if (auto dir = find_env_in_dirs(name, envs_dirs); dir.has_value())
                {
                    return std::move(dir).value();
                }
                if (auto dir = find_writable_env_in_dirs(name, envs_dirs); dir.has_value())
                {
                    return std::move(dir).value();
                }
                return root_prefix / "envs" / name;
            }
        }

        void env_name_hook(Configuration& config, std::string& name)
        {
            file_spec_env_name_hook(name);

            auto& root_prefix = config.at("root_prefix").value<fs::u8path>();

            auto& env_name = config.at("env_name");

            auto& spec_file_env_name = config.at("spec_file_env_name");
            auto& spec_file_name = spec_file_env_name.value<std::string>();

            // Allow spec file environment name to be overridden by target prefix
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
                const auto& envs_dirs = config.at("envs_dirs").value<std::vector<fs::u8path>>();
                fs::u8path prefix = compute_prefix_from_name(root_prefix, envs_dirs, name);

                if (!config.at("target_prefix").cli_configured()
                    && config.at("env_name").cli_configured())
                {
                    config.at("target_prefix").set_cli_value<fs::u8path>(prefix);
                }

                if (!config.at("target_prefix").api_configured()
                    && config.at("env_name").api_configured())
                {
                    config.at("target_prefix").set_value(prefix);
                }
            }
        }

        void target_prefix_hook(Configuration& config, fs::u8path& prefix)
        {
            // Fall back to environment specified in CONDA_PREFIX
            bool use_target_prefix_fallback = config.at("use_target_prefix_fallback").value<bool>();
            if (prefix.empty() && use_target_prefix_fallback)
            {
                // CONDA_PREFIX is always a complete path
                prefix = util::get_env("CONDA_PREFIX").value_or("");
            }

            // Fall back to environment specified in CONDA_DEFAULT_ENV
            bool use_default_prefix_fallback = config.at("use_default_prefix_fallback").value<bool>();
            if (prefix.empty() && use_default_prefix_fallback)
            {
                prefix = util::get_env("CONDA_DEFAULT_ENV").value_or("");
            }

            // Fall back to base environment
            bool use_root_prefix_fallback = config.at("use_root_prefix_fallback").value<bool>();
            if (prefix.empty() && use_root_prefix_fallback)
            {
                prefix = config.at("root_prefix").value<fs::u8path>();
            }

            auto& root_prefix = config.at("root_prefix").value<fs::u8path>();

            if (!prefix.empty())
            {
                // Prefix can be an environment name rather than a full path
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

#ifdef _WIN32
            std::string sep = "\\";
#else
            std::string sep = "/";
#endif
            if (!prefix.empty())
            {
                prefix = util::rstrip(
                    fs::weakly_canonical(util::expand_home(prefix.string())).string(),
                    sep
                );
            }

            if ((prefix == root_prefix) && config.at("create_base").value<bool>())
            {
                path::touch(root_prefix / "conda-meta" / "history", true);
            }
        }

        auto get_root_prefix_from_mamba_bin(const fs::u8path& mamba_bin_path)
            -> expected_t<fs::u8path>
        {
            if (mamba_bin_path.empty())
            {
                return make_unexpected(
                    "`mamba` binary not found.\nPlease set `MAMBA_ROOT_PREFIX`.",
                    mamba_error_code::incorrect_usage
                );
            }
            // In linux and osx, the install path would be install_prefix/bin/mamba
            // In windows, install_prefix/Scripts/mamba.exe
            return { fs::weakly_canonical(mamba_bin_path.parent_path().parent_path()) };
        }

        auto validate_existing_root_prefix(const fs::u8path& candidate) -> expected_t<fs::u8path>
        {
            auto prefix = fs::u8path(util::expand_home(candidate.string()));

            if (prefix.empty())
            {
                return make_unexpected("Empty root prefix.", mamba_error_code::incorrect_usage);
            }

            if (!fs::exists(prefix / "pkgs")           //
                && !fs::exists(prefix / "conda-meta")  //
                && !fs::exists(prefix / "envs"))
            {
                return make_unexpected(
                    fmt::format(
                        R"(Path "{}" is not an existing root prefix.)"
                        R"( Please set explicitly `MAMBA_ROOT_PREFIX` to "{}" to skip this error.)",
                        prefix.string(),
                        prefix.string()
                    ),
                    mamba_error_code::incorrect_usage
                );
            }

            return { fs::weakly_canonical(std::move(prefix)) };
        }

        auto validate_root_prefix(const fs::u8path& candidate) -> expected_t<fs::u8path>
        {
            auto prefix = fs::u8path(util::expand_home(candidate.string()));

            if (prefix.empty())
            {
                return make_unexpected("Empty root prefix.", mamba_error_code::incorrect_usage);
            }

            if (fs::exists(prefix))
            {
                if (fs::is_directory(prefix))
                {
                    if (auto maybe_prefix = validate_existing_root_prefix(prefix);
                        maybe_prefix.has_value())
                    {
                        return maybe_prefix;
                    }

                    return make_unexpected(
                        fmt::format(
                            R"(Could not use default root_prefix "{}":)"
                            R"( Directory exists, is not empty and not a conda prefix.)"
                            R"( Please set explicitly `MAMBA_ROOT_PREFIX` to "{}" to skip this error.)",
                            prefix.string(),
                            prefix.string()
                        ),
                        mamba_error_code::incorrect_usage
                    );
                }
                return make_unexpected(
                    fmt::format(
                        R"(Could not use default root_prefix "{}": Not a directory.)",
                        prefix.string()
                    ),
                    mamba_error_code::incorrect_usage
                );
            }

            return { fs::weakly_canonical(std::move(prefix)) };
        }

        /**
         * In mamba 1.0, only micromamba was using this location.
         */
        auto default_root_prefix_v1() -> fs::u8path
        {
            return fs::u8path(util::user_home_dir()) / "micromamba";
        }

        /**
         * In mamba 2.0, we change the default location.
         * We unconditionally name the subfolder "mamba" for compatibility between ``mamba``
         * and ``micromamba``, as well as consistency with ``MAMBA_`` environment variables.
         */
        auto default_root_prefix_v2() -> fs::u8path
        {
            return fs::u8path(util::user_data_dir()) / "mamba";
        }

        void root_prefix_hook(Configuration& config, fs::u8path& prefix)
        {
            auto& env_name = config.at("env_name");

            if (prefix.empty())
            {
                if (util::get_env("MAMBA_DEFAULT_ROOT_PREFIX"))
                {
                    prefix = util::get_env("MAMBA_DEFAULT_ROOT_PREFIX").value();
                    LOG_WARNING << unindent(R"(
                                    'MAMBA_DEFAULT_ROOT_PREFIX' is meant for testing purpose.
                                    Consider using 'MAMBA_ROOT_PREFIX' instead)");
                }
                else
                {
#ifdef MAMBA_USE_INSTALL_PREFIX_AS_BASE
                    // mamba case
                    // set the root prefix as the mamba installation path
                    get_root_prefix_from_mamba_bin(util::which("mamba"))
                        .transform([&](fs::u8path&& p) { prefix = std::move(p); })
                        .or_else([](mamba_error&& error) { throw std::move(error); });
#else
                    // micromamba case
                    validate_existing_root_prefix(default_root_prefix_v1())
                        .or_else([](const auto& /* error */)
                                 { return validate_root_prefix(default_root_prefix_v2()); })
                        .transform([&](fs::u8path&& p) { prefix = std::move(p); })
                        .or_else([](mamba_error&& error) { throw std::move(error); });
#endif
                }

                if (env_name.configured())
                {
                    const auto exe_name = get_self_exe_path().stem().string();
                    LOG_WARNING << "You have not set the root prefix environment variable.\n"
                                   "To permanently modify the root prefix location, either:\n"
                                   "  - set the 'MAMBA_ROOT_PREFIX' environment variable\n"
                                   "  - use the '-r,--root-prefix' CLI option\n"
                                   "  - use '"
                                << exe_name
                                << " shell init ...' to initialize your shell\n"
                                   "    (then restart or source the contents of the shell init script)\n"
                                   "Continuing with default value: "
                                << '"' << prefix.string() << '"';
                }
            }

            prefix = fs::weakly_canonical(util::expand_home(prefix.string()));
        }

        void rc_loading_hook(Configuration& config, const RCConfigLevel& level)
        {
            auto& rc_files = config.at("rc_files").value<std::vector<fs::u8path>>();
            config.set_rc_values(rc_files, level);
        }

        void post_root_prefix_rc_loading(Configuration& config)
        {
            if (!config.context().src_params.no_rc)
            {
                rc_loading_hook(config, RCConfigLevel::kHomeDir);
                config.at("no_env").compute(MAMBA_CONF_FORCE_COMPUTE);
            }
        }

        void post_target_prefix_rc_loading(Configuration& config)
        {
            if (!config.context().src_params.no_rc)
            {
                rc_loading_hook(config, RCConfigLevel::kTargetPrefix);
                config.at("no_env").compute(MAMBA_CONF_FORCE_COMPUTE);
            }
        }

        mamba::log_level log_level_fallback_hook(Configuration& config)
        {
            const auto& ctx = config.context();

            if (ctx.output_params.json)
            {
                return mamba::log_level::critical;
            }
            else if (config.at("verbose").configured())
            {
                switch (ctx.output_params.verbosity)
                {
                    case 0:
                        return mamba::log_level::warn;
                    case 1:
                        return mamba::log_level::info;
                    case 2:
                        return mamba::log_level::debug;
                    default:
                        return mamba::log_level::trace;
                }
            }
            else
            {
                return mamba::log_level::warn;
            }
        }

        void verbose_hook(Context& ctx, int& lvl)
        {
            ctx.output_params.verbosity = lvl;
        }

        void target_prefix_checks_hook(const Context& ctx, int& options)
        {
            const auto& prefix = ctx.prefix_params.target_prefix;

            bool no_checks = options & MAMBA_NO_PREFIX_CHECK;
            bool allow_missing = options & MAMBA_ALLOW_MISSING_PREFIX;
            bool allow_not_env = options & MAMBA_ALLOW_NOT_ENV_PREFIX;
            bool allow_existing = options & MAMBA_ALLOW_EXISTING_PREFIX;
            bool expect_existing = options & MAMBA_EXPECT_EXISTING_PREFIX;

            if (no_checks)
            {
                return;
            }

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
                const auto exe_name = get_self_exe_path().stem().string();
                LOG_ERROR << "No prefix found at: " << prefix.string();
                LOG_ERROR << "Environment must first be created with \"" << exe_name
                          << " create -n {env_name} ...\"";
                throw std::runtime_error("Aborting.");
            }
        }

        void rc_files_hook(const Context& ctx, std::vector<fs::u8path>& files)
        {
            if (!files.empty())
            {
                if (ctx.src_params.no_rc)
                {
                    LOG_ERROR << "Configuration files disabled by 'no_rc'";
                    throw std::runtime_error("Incompatible configuration. Aborting.");
                }
                for (auto& f : files)
                {
                    f = util::expand_home(f.string());
                    if (!fs::exists(f))
                    {
                        LOG_ERROR << "Configuration file specified but does not exist at '"
                                  << f.string() << "'";
                        throw std::runtime_error("Aborting.");
                    }
                }
            }
        }

        void experimental_hook(bool& value)
        {
            if (value)
            {
                LOG_WARNING << "Experimental mode enabled";
            }
        }

        // cf. https://github.com/openSUSE/libsolv/issues/562 to track corresponding issue
        void not_supported_option_hook(bool& value)
        {
            if (!value)
            {
                LOG_WARNING << "Parsing with libsolv does not support repodata_version 2";
            }
        }

        void debug_hook(bool& value)
        {
            if (value)
            {
                LOG_WARNING << "Debug mode enabled";
            }
        }

        void print_config_only_hook(Configuration& config, bool& value)
        {
            if (value)
            {
                if (!config.at("debug").value<bool>())
                {
                    LOG_ERROR << "Debug mode required to use 'print_config_only'";
                    throw std::runtime_error("Aborting.");
                }
                config.at("quiet").set_value(true);
                config.at("json").set_value(false);
            }
        }

        void print_context_only_hook(Configuration& config, bool& value)
        {
            if (value)
            {
                if (!config.at("debug").value<bool>())
                {
                    LOG_ERROR << "Debug mode required to use 'print_context_only'";
                    throw std::runtime_error("Aborting.");
                }
                config.at("quiet").set_value(true);
                config.at("json").set_value(false);
            }
        }

        std::vector<fs::u8path> fallback_envs_dirs_hook(const Context& context)
        {
            return { context.prefix_params.root_prefix / "envs" };
        }

        void envs_dirs_hook(std::vector<fs::u8path>& dirs)
        {
            for (auto& d : dirs)
            {
                d = fs::weakly_canonical(util::expand_home(d.string())).string();
                if (fs::exists(d) && !fs::is_directory(d))
                {
                    LOG_ERROR << "Env dir specified is not a directory: " << d.string();
                    throw std::runtime_error("Aborting.");
                }
            }
        }

        std::vector<fs::u8path> fallback_pkgs_dirs_hook(const Context& context)
        {
            std::vector<fs::u8path> paths = {
                context.prefix_params.root_prefix / "pkgs",
                fs::u8path(util::user_home_dir()) / ".mamba" / "pkgs",
            };
#ifdef _WIN32
            auto appdata = util::get_env("APPDATA");
            if (appdata)
            {
                paths.push_back(fs::u8path(appdata.value()) / ".mamba" / "pkgs");
            }
#endif
            return paths;
        }

        void pkgs_dirs_hook(std::vector<fs::u8path>& dirs)
        {
            for (auto& d : dirs)
            {
                d = fs::weakly_canonical(util::expand_home(d.string())).string();
                if (fs::exists(d) && !fs::is_directory(d))
                {
                    LOG_ERROR << "Packages dir specified is not a directory: " << d.string();
                    throw std::runtime_error("Aborting.");
                }
            }
        }

        void download_threads_hook(std::size_t& value)
        {
            if (!value)
            {
                throw std::runtime_error(fmt::format(
                    "Number of download threads as to be positive (currently set to {})",
                    value
                ));
            }
        }

        void extract_threads_hook(const Context& context)
        {
            PackageFetcherSemaphore::set_max(context.threads_params.extract_threads);
        }
    }

    fs::u8path get_conda_root_prefix()
    {
        std::vector<std::string> args = { "conda", "config", "--show", "root_prefix", "--json" };
        std::string out, err;
        auto [status, ec] = reproc::run(
            args,
            reproc::options{},
            reproc::sink::string(out),
            reproc::sink::string(err)
        );

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

    void use_conda_root_prefix(Configuration& config, bool force)
    {
        if (!config.at("root_prefix").configured() || force)
        {
            util::set_env("MAMBA_ROOT_PREFIX", get_conda_root_prefix().string());
        }
    }

    /************
     * printers *
     ************/

    namespace nl = nlohmann;

    namespace detail
    {
        bool has_config_name(const std::string& file)
        {
            const auto filename = fs::u8path(file).filename();
            return filename == ".condarc" || filename == "condarc" || filename == ".mambarc"
                   || filename == "mambarc" || util::ends_with(file, ".yml")
                   || util::ends_with(file, ".yaml");
        }

        bool is_config_file(const fs::u8path& path)
        {
            return fs::exists(path) && (!fs::is_directory(path)) && has_config_name(path.string());
        }

        void print_node(YAML::Emitter& out, YAML::Node value, YAML::Node source, bool show_source);

        void
        print_scalar_node(YAML::Emitter& out, YAML::Node value, YAML::Node source, bool show_source)
        {
            out << value;

            if (show_source)
            {
                if (source.IsScalar())
                {
                    out << YAML::Comment("'" + source.as<std::string>() + "'");
                }
                else if (source.IsSequence())
                {
                    auto srcs = source.as<std::vector<std::string>>();
                    std::string comment = "'" + srcs.at(0) + "'";
                    for (std::size_t i = 1; i < srcs.size(); ++i)
                    {
                        comment += " > '" + srcs.at(i) + "'";
                    }
                    out << YAML::Comment(comment);
                }
                else
                {
                    LOG_ERROR << "YAML source type not handled";
                    throw std::runtime_error("YAML source type not handled");
                }
            }
        }

        void print_seq_node(YAML::Emitter& out, YAML::Node value, YAML::Node source, bool show_source)
        {
            if (value.size() > 0)
            {
                out << YAML::BeginSeq;
                for (std::size_t n = 0; n < value.size(); ++n)
                {
                    if (source.IsSequence() && (source.size() == value.size()))
                    {
                        print_node(out, value[n], source[n], show_source);
                    }
                    else
                    {
                        print_node(out, value[n], source, show_source);
                    }
                }
                out << YAML::EndSeq;
            }
            else
            {
                out << YAML::_Null();
                if (show_source)
                {
                    out << YAML::Comment("'default'");
                }
            }
        }

        void print_map_node(YAML::Emitter& out, YAML::Node value, YAML::Node source, bool show_source)
        {
            out << YAML::BeginMap;
            for (auto n : value)
            {
                auto key = n.first.as<std::string>();
                out << YAML::Key << n.first;
                out << YAML::Value;

                if (source.IsMap())
                {
                    print_node(out, n.second, source[key], show_source);
                }
                else
                {
                    print_node(out, n.second, source, show_source);
                }
            }
            out << YAML::EndMap;
        }

        void print_node(YAML::Emitter& out, YAML::Node value, YAML::Node source, bool show_source)
        {
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

        void print_configurable(YAML::Emitter& out, const Configurable& config, bool show_source)
        {
            auto value = config.yaml_value();
            auto source = YAML::Node(config.source());
            print_node(out, value, source, show_source);
        }

        void print_group_title(YAML::Emitter& out, const std::string& name)
        {
            auto group_title = name + " Configuration";
            auto blk_size = 52 - group_title.size();
            auto prepend_blk = blk_size / 2;
            auto append_blk = blk_size - prepend_blk;

            out << YAML::Comment(std::string(54, '#')) << YAML::Newline;
            out << YAML::Comment(
                "#" + std::string(prepend_blk, ' ') + group_title + std::string(append_blk, ' ') + "#"
            ) << YAML::Newline;
            out << YAML::Comment(std::string(54, '#'));
        }

        void dump_configurable(nl::json& node, const Configurable& c, const std::string& name)
        {
            c.dump_json(node, name);
        }
    }

    /********************************
     * Configuration implementation *
     ********************************/

    Configuration::Configuration(Context& ctx)
        : m_context(ctx)
    {
        set_configurables();
    }

    void Configuration::set_configurables()
    {
        // Basic
        insert(
            Configurable("root_prefix", &m_context.prefix_params.root_prefix)
                .group("Basic")
                .set_env_var_names()
                .needs({ "create_base", "rc_files" })
                .description("Path to the root prefix")
                .set_post_merge_hook<fs::u8path>([&](fs::u8path& value)
                                                 { return detail::root_prefix_hook(*this, value); })
                .set_post_context_hook([this] { return detail::post_root_prefix_rc_loading(*this); })
        );

        insert(Configurable("create_base", false)
                   .group("Basic")
                   .set_single_op_lifetime()
                   .description("Define if base environment will be initialized empty"));

        insert(Configurable("target_prefix", &m_context.prefix_params.target_prefix)
                   .group("Basic")
                   .set_env_var_names()
                   .needs({ "root_prefix",
                            "envs_dirs",
                            "env_name",
                            "spec_file_env_name",
                            "use_target_prefix_fallback",
                            "use_default_prefix_fallback",
                            "use_root_prefix_fallback" })
                   .set_single_op_lifetime()
                   .description("Path to the target prefix")
                   .set_post_merge_hook<fs::u8path>(
                       [this](fs::u8path& value) { return detail::target_prefix_hook(*this, value); }
                   )
                   .set_post_context_hook([this]
                                          { return detail::post_target_prefix_rc_loading(*this); }));

        insert(Configurable("relocate_prefix", &m_context.prefix_params.relocate_prefix)
                   .group("Basic")
                   .set_env_var_names()
                   .needs({ "target_prefix" })
                   .set_single_op_lifetime()
                   .description("Path to the relocation prefix"));

        insert(Configurable("use_target_prefix_fallback", true)
                   .group("Basic")
                   .set_single_op_lifetime()
                   .description("Fallback to the current target prefix or not"));

        insert(Configurable("use_root_prefix_fallback", true)
                   .group("Basic")
                   .set_single_op_lifetime()
                   .description("Fallback to the root prefix or not"));

        insert(Configurable("use_default_prefix_fallback", true)
                   .group("Basic")
                   .set_single_op_lifetime()
                   .description(
                       "Fallback to the prefix specified with environment variable CONDA_DEFAULT_ENV or not"
                   ));

        insert(Configurable("target_prefix_checks", MAMBA_NO_PREFIX_CHECK)
                   .group("Basic")
                   .needs({ "target_prefix", "rc_files" })
                   .description("The type of checks performed on the target prefix")
                   .set_single_op_lifetime()
                   .set_post_merge_hook<int>([this](int& value)
                                             { detail::target_prefix_checks_hook(m_context, value); }
                   ));

        insert(Configurable("env_name", std::string(""))
                   .group("Basic")
                   .needs({ "root_prefix", "spec_file_env_name", "envs_dirs" })
                   .set_single_op_lifetime()
                   .set_post_merge_hook<std::string>([this](std::string& value)
                                                     { return detail::env_name_hook(*this, value); })
                   .description("Name of the target prefix"));

        insert(Configurable("envs_dirs", &m_context.envs_dirs)
                   .group("Basic")
                   .set_rc_configurable(RCConfigLevel::kHomeDir)
                   .set_env_var_names({ "CONDA_ENVS_DIRS" })
                   .needs({ "root_prefix" })
                   .set_fallback_value_hook<decltype(m_context.envs_dirs)>(
                       [this] { return detail::fallback_envs_dirs_hook(m_context); }
                   )
                   .set_post_merge_hook(detail::envs_dirs_hook)
                   .description("Possible locations of named environments"));

        insert(Configurable("pkgs_dirs", &m_context.pkgs_dirs)
                   .group("Basic")
                   .set_rc_configurable()
                   .set_env_var_names({ "CONDA_PKGS_DIRS" })
                   .needs({ "root_prefix" })
                   .set_fallback_value_hook<decltype(m_context.pkgs_dirs)>(
                       [this] { return detail::fallback_pkgs_dirs_hook(m_context); }
                   )
                   .set_post_merge_hook(detail::pkgs_dirs_hook)
                   .description("Possible locations of packages caches"));

        insert(Configurable("platform", &m_context.platform)
                   .group("Basic")
                   .set_rc_configurable()
                   .set_env_var_names({ "CONDA_SUBDIR", "MAMBA_PLATFORM" })
                   .description("The platform description")
                   .long_description(unindent(R"(
                        The platform description points what channels
                        subdir/platform have to be fetched for package solving.
                        This can be 'linux-64' or similar.)")));

        insert(Configurable("spec_file_env_name", std::string(""))
                   .group("Basic")
                   .needs({ "file_specs", "root_prefix" })
                   .set_single_op_lifetime()
                   .set_post_merge_hook(detail::file_spec_env_name_hook)
                   .description("Name of the target prefix, specified in a YAML spec file"));

        insert(Configurable("specs", std::vector<std::string>({}))
                   .group("Basic")
                   .needs({ "file_specs" })  // explicit file specs overwrite current specs
                   .set_single_op_lifetime()
                   .description("Packages specification"));

        insert(Configurable("others_pkg_mgrs_specs", std::vector<detail::other_pkg_mgr_spec>({}))
                   .group("Basic")
                   .set_single_op_lifetime()
                   .description("Others package managers specifications"));

        insert(Configurable("experimental", &m_context.experimental)
                   .group("Basic")
                   .description("Enable experimental features")
                   .set_rc_configurable()
                   .set_env_var_names()
                   .long_description(unindent(R"(
                        Enable experimental features that may be still.
                        under active development and not stable yet.)"))
                   .set_post_merge_hook(detail::experimental_hook));

        insert(Configurable("experimental_repodata_parsing", &m_context.experimental_repodata_parsing)
                   .group("Basic")
                   .description(  //
                       "Enable experimental parsing of `repodata.json` using simdjson.\n"
                       "Default is `true`. `false` means libsolv is used.\n"
                   )
                   .set_rc_configurable()
                   .set_env_var_names()
                   .set_post_merge_hook(detail::not_supported_option_hook));

        insert(Configurable("debug", &m_context.debug)
                   .group("Basic")
                   .set_env_var_names()
                   .description("Turn on the debug mode")
                   .long_description(unindent(R"(
                        Turn on the debug mode that allow introspection
                        in intermediate steps of the operation called.
                        Debug features may/will interrupt the operation,
                        if you only need further logs refer to 'verbose'.)"))
                   .set_post_merge_hook(detail::debug_hook));

        // Channels
        insert(Configurable("channels", &m_context.channels)
                   .group("Channels")
                   .set_rc_configurable()
                   .set_env_var_names({ "CONDA_CHANNELS" })
                   .description("Define the list of channels")
                   .needs({ "file_specs" })
                   .long_description(unindent(R"(
                        The list of channels where the packages will be searched for.
                        Note that '-c local' allows using locally built packages.
                        See also 'channel_priority'.)"))
                   .set_post_merge_hook<decltype(m_context.channels)>(
                       [&](decltype(m_context.channels)& value)
                       { return detail::channels_hook(*this, value); }
                   ));

        insert(Configurable("channel_alias", &m_context.channel_alias)
                   .group("Channels")
                   .set_rc_configurable()
                   .set_env_var_names()
                   .description("The prepended url location to associate with channel names"));

        insert(Configurable("default_channels", &m_context.default_channels)
                   .group("Channels")
                   .set_rc_configurable()
                   .set_env_var_names()
                   .description("Default channels used")
                   .long_description(unindent(R"(
                        The list of channel names and/or urls used for the 'defaults'
                        multichannel.)")));

        insert(Configurable("custom_channels", &m_context.custom_channels)
                   .group("Channels")
                   .set_rc_configurable()
                   .set_env_var_names()
                   .description("Custom channels")
                   .long_description(  //
                       "A dictionary with name: url to use for custom channels.\n"
                   ));

        insert(Configurable("custom_multichannels", &m_context.custom_multichannels)
                   .group("Channels")
                   .set_rc_configurable()
                   .description("Custom multichannels")
                   .long_description(  //
                       "A dictionary where keys are multi channels names, and values are a list "
                       "of corresponding names / urls / file paths to use.\n"
                   )
                   .needs({ "default_channels", "target_prefix", "root_prefix" }));

        insert(Configurable("mirrored_channels", &m_context.mirrored_channels)
                   .group("Channels")
                   .set_rc_configurable()
                   .set_env_var_names()
                   .description("Mirrored channels")
                   .long_description(  //
                       "A dictionary where keys are channels names, and values are a list "
                       "of mirrors urls to use.\n"
                   ));

        insert(Configurable("override_channels_enabled", &m_context.override_channels_enabled)
                   .group("Channels")
                   .set_rc_configurable()
                   .set_env_var_names()
                   .description("Permit use of the --override-channels command-line flag"));

        insert(Configurable("repodata_use_zst", &m_context.repodata_use_zst)
                   .group("Repodata")
                   .set_rc_configurable()
                   .description("Use zstd encoded repodata when fetching ("
                                "Note that this doesn't apply when fetching from an OCI registry - "
                                "using `mirrored_channels` - since compressed repodata is "
                                "automatically used when present.)\n"));


        insert(Configurable("repodata_has_zst", &m_context.repodata_has_zst)
                   .group("Repodata")
                   .set_rc_configurable()
                   .description("Channels that have zstd encoded repodata (saves a HEAD request)"));

        // Network
        insert(Configurable("cacert_path", std::string(""))
                   .group("Network")
                   .set_rc_configurable()
                   .set_env_var_names()
                   .description("Path (file or directory) SSL certificate(s)")
                   .long_description(unindent(R"(
                        Path (file or directory) SSL certificate(s) to use when
                        'ssl_verify' is turned on but not set with path to certs.
                        WARNING: overrides 'ssl_verify' if provided and 'ssl_verify'
                        also contains a path to SSL certificates.)")));

        insert(Configurable("local_repodata_ttl", &m_context.local_repodata_ttl)
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

        insert(Configurable("offline", &m_context.offline)
                   .group("Network")
                   .set_rc_configurable()
                   .set_env_var_names()
                   .description("Force use cached repodata"));

        insert(Configurable("ssl_no_revoke", &m_context.remote_fetch_params.ssl_no_revoke)
                   .group("Network")
                   .set_rc_configurable()
                   .set_env_var_names()
                   .description("SSL certificate revocation checks")
                   .long_description(unindent(R"(
                        This option tells curl to disable certificate revocation checks.
                        It's only working for Windows back-end.
                        WARNING: this option loosens the SSL security.)")));

        insert(Configurable("ssl_verify", &m_context.remote_fetch_params.ssl_verify)
                   .group("Network")
                   .set_rc_configurable()
                   .set_env_var_names()
                   .description("Verify SSL certificates for HTTPS requests")
                   .long_description(unindent(R"(
                        'ssl_verify' can be either an empty string (regular SSL verification),
                        the string "<false>" to indicate no SSL verification, or a path to
                        a directory with cert files, or a cert file..)"))
                   .needs({ "cacert_path", "offline" })
                   .set_post_merge_hook<decltype(m_context.remote_fetch_params.ssl_verify)>(
                       [this](auto&... args) { return detail::ssl_verify_hook(*this, args...); }
                   ));

        insert(Configurable("proxy_servers", &m_context.remote_fetch_params.proxy_servers)
                   .group("Network")
                   .set_rc_configurable()
                   .description("Use a proxy server for network connections")
                   .long_description(unindent(R"(
                        'proxy_servers' should be a dictionary where the key is either in the form of
                        scheme://hostname or just a scheme for which the proxy server should be used and
                        the value is the url of the proxy server, optionally with username and password
                        in the form of scheme://username:password@hostname.)")));

        insert(Configurable("remote_connect_timeout_secs", &m_context.remote_fetch_params.connect_timeout_secs)
                   .group("Network")
                   .set_rc_configurable()
                   .set_env_var_names()
                   .description(
                       "The number seconds conda will wait for your client to establish a connection to a remote url resource."
                   ));

        insert(Configurable("remote_backoff_factor", &m_context.remote_fetch_params.retry_backoff)
                   .group("Network")
                   .set_rc_configurable()
                   .set_env_var_names()
                   .description("The factor determines the time HTTP connection should wait for attempt."
                   ));

        insert(Configurable("remote_max_retries", &m_context.remote_fetch_params.max_retries)
                   .group("Network")
                   .set_rc_configurable()
                   .set_env_var_names()
                   .description("The maximum number of retries each HTTP connection should attempt."));


        // Solver
        insert(Configurable("channel_priority", &m_context.channel_priority)
                   .group("Solver")
                   .set_rc_configurable()
                   .set_env_var_names()
                   .description("Define the channel priority ('strict' or 'disabled')")
                   .long_description(unindent(R"(
                        Accepts values of 'strict' and 'disabled'. The default
                        value is 'strict'. With strict channel priority, packages in lower
                        priority channels are not considered if a package with the same name
                        appears in a higher priority channel.
                        With channel priority disabled, package version takes precedence, and the
                        configured priority of channels is used only to break ties. In
                        previous versions of conda, this parameter was configured as either
                        True or False. True is now an alias to 'flexible'.)"))
                   .set_post_merge_hook<ChannelPriority>(
                       [&](ChannelPriority& value)
                       {
                           m_context.solver_flags.strict_repo_priority = (value == ChannelPriority::Strict);
                       }
                   ));

        insert(Configurable("explicit_install", false)
                   .group("Solver")
                   .description("Use explicit install instead of solving environment"));

        insert(Configurable("file_specs", std::vector<std::string>({}))
                   .group("Solver")
                   .set_post_merge_hook<std::vector<std::string>>(
                       [&](std::vector<std::string>& value)
                       { return detail::file_specs_hook(*this, value); }
                   )
                   .description("File (yaml, explicit or plain)"));

        insert(Configurable("no_pin", false)
                   .group("Solver")
                   .set_env_var_names()
                   .description("Ignore pinned packages"));

        insert(Configurable("no_py_pin", false)
                   .group("Solver")
                   .set_rc_configurable()
                   .set_env_var_names()
                   .description("Do not automatically pin Python")
                   .long_description(unindent(R"(
                        Do not automatically pin Python when not present in
                        the packages specifications, which is the default
                        behavior.)")));

        insert(Configurable("add_pip_as_python_dependency", &m_context.add_pip_as_python_dependency)
                   .group("Solver")
                   .set_rc_configurable()
                   .set_env_var_names()
                   .description("Add pip as a Python dependency")
                   .long_description("Automatically add pip as a Python dependency"));

        insert(Configurable("pinned_packages", &m_context.pinned_packages)
                   .group("Solver")
                   .set_rc_configurable()
                   .set_env_var_names()
                   .description("A list of package specs to pin for every environment resolution"));

        insert(Configurable("freeze_installed", false)
                   .group("Solver")
                   .description("Freeze already installed dependencies"));

        insert(Configurable("no_deps", false)
                   .group("Solver")
                   .description("Do not install dependencies. This WILL lead to broken environments "
                                "and inconsistent behavior. Use at your own risk")
                   .set_post_merge_hook<bool>([&](bool& value)
                                              { m_context.solver_flags.keep_dependencies = !value; }));

        insert(Configurable("only_deps", false)
                   .group("Solver")
                   .description("Only install dependencies")
                   .set_post_merge_hook<bool>([&](bool& value)
                                              { m_context.solver_flags.keep_user_specs = !value; }));

        insert(Configurable("force_reinstall", &m_context.solver_flags.force_reinstall)
                   .group("Solver")
                   .description("Force reinstall of package"));

        insert(Configurable("allow_uninstall", &m_context.solver_flags.allow_uninstall)
                   .group("Solver")
                   .set_rc_configurable()
                   .set_env_var_names()
                   .description("Allow uninstall when installing or updating packages. Default is true."
                   ));

        insert(Configurable("allow_downgrade", &m_context.solver_flags.allow_downgrade)
                   .group("Solver")
                   .set_rc_configurable()
                   .set_env_var_names()
                   .description("Allow downgrade when installing packages. Default is false."));

        insert(Configurable("order_solver_request", &m_context.solver_flags.order_request)
                   .group("Solver")
                   .set_rc_configurable()
                   .set_env_var_names()
                   .description("Order the solver request specs to get a deterministic solution."));

        insert(Configurable("categories", std::vector<std::string>({ "main" }))
                   .group("Solver")
                   .description("Package categories to consider when installing from a lock file"));

        insert(Configurable("retry_clean_cache", false)
                   .group("Solver")
                   .set_env_var_names()
                   .description("If solve fails, try to fetch updated repodata"));

        // Extract, Link & Install
        insert(Configurable("download_threads", &m_context.threads_params.download_threads)
                   .group("Extract, Link & Install")
                   .set_rc_configurable()
                   .set_env_var_names()
                   .set_post_merge_hook(detail::download_threads_hook)
                   .description("Defines the number of threads for package download")
                   .long_description(unindent(R"(
                        Defines the number of threads for package download.
                        It has to be strictly positive.)")));

        insert(Configurable("extract_threads", &m_context.threads_params.extract_threads)
                   .group("Extract, Link & Install")
                   .set_rc_configurable()
                   .set_env_var_names()
                   .set_post_context_hook([this] { return detail::extract_threads_hook(m_context); })
                   .description("Defines the number of threads for package extraction")
                   .long_description(unindent(R"(
                        Defines the number of threads for package extraction.
                        Positive number gives the number of threads, negative number gives
                        host max concurrency minus the value, zero (default) is the host max
                        concurrency value.)")));

        insert(Configurable("allow_softlinks", &m_context.allow_softlinks)
                   .group("Extract, Link & Install")
                   .set_rc_configurable()
                   .set_env_var_names()
                   .description("Allow to use soft-links when hard-links are not possible")
                   .long_description(unindent(R"(
                        Allow to use soft-links (symlinks) when hard-links are not possible,
                        such as when installing on a different filesystem than the one that
                        the package cache is on.)")));

        insert(Configurable("always_copy", &m_context.always_copy)
                   .group("Extract, Link & Install")
                   .set_rc_configurable()
                   .set_env_var_names()
                   .description("Use copy instead of hard-link")
                   .long_description(unindent(R"(
                        Register a preference that files be copied into a prefix during
                        install rather than hard-linked.)")));

        insert(Configurable("always_softlink", &m_context.always_softlink)
                   .group("Extract, Link & Install")
                   .set_rc_configurable()
                   .set_env_var_names()
                   .needs({ "always_copy" })
                   .set_post_merge_hook<decltype(m_context.always_softlink)>(
                       [&](decltype(m_context.always_softlink)& value)
                       { return detail::always_softlink_hook(*this, value); }
                   )
                   .description("Use soft-link instead of hard-link")
                   .long_description(unindent(R"(
                        Register a preference that files be soft-linked (symlinked) into a
                        prefix during install rather than hard-linked. The link source is the
                        package cache from where the package is being linked.
                        !WARNING: Using this option can result in corruption of long-lived
                        environments due to broken links (deleted cache).)")));

        insert(Configurable("show_anaconda_channel_warnings", &m_context.show_anaconda_channel_warnings)
                   .group("Extract, Link & Install")
                   .set_rc_configurable()
                   .set_env_var_names({ "MAMBA_SHOW_ANACONDA_CHANNEL_WARNINGS" })
                   .description("Show the warning when the Anaconda official channels are used"));

        insert(Configurable("shortcuts", &m_context.shortcuts)
                   .group("Extract, Link & Install")
                   .set_rc_configurable()
                   .set_env_var_names()
                   .description(
                       "Install start-menu shortcuts on Windows (not implemented on Linux / macOS)"
                   ));

        insert(Configurable("safety_checks", &m_context.validation_params.safety_checks)
                   .group("Extract, Link & Install")
                   .set_rc_configurable()
                   .set_env_var_names({ "CONDA_SAFETY_CHECKS", "MAMBA_SAFETY_CHECKS" })
                   .description("Safety checks policy ('enabled', 'warn', or 'disabled')")
                   .long_description(unindent(R"(
                        Enforce available safety guarantees during package installation. The
                        value must be one of 'enabled', 'warn', or 'disabled'.)")));

        insert(Configurable("extra_safety_checks", &m_context.validation_params.extra_safety_checks)
                   .group("Extract, Link & Install")
                   .set_rc_configurable()
                   .set_env_var_names({ "CONDA_EXTRA_SAFETY_CHECKS", "MAMBA_EXTRA_SAFETY_CHECKS" })
                   .description("Run extra verifications on packages")
                   .long_description(unindent(R"(
                        Spend extra time validating package contents. Currently, runs sha256
                        verification on every file within each package during installation.)")));

        insert(Configurable("verify_artifacts", &m_context.validation_params.verify_artifacts)
                   .group("Extract, Link & Install")
                   .set_rc_configurable()
                   .set_env_var_names()
                   .description(  //
                       "Run verifications on packages signatures.\n"
                       "This is still experimental and may not be stable yet.\n"
                   )
                   .long_description(unindent(R"(
                        Spend extra time validating package contents. It consists of running
                        cryptographic verifications on channels and packages metadata.)")));

        insert(Configurable("trusted_channels", &m_context.validation_params.trusted_channels)
                   .group("Extract, Link & Install")
                   .set_rc_configurable()
                   .set_env_var_names()
                   .description(  //
                       "The list of trusted channels allowing artifacts verification.\n"
                       "See `verify-artifacts` for more details.\n"
                   ));

        insert(Configurable("lock_timeout", &m_context.lock_timeout)
                   .group("Extract, Link & Install")
                   .set_rc_configurable()
                   .set_env_var_names()
                   .description("Lockfile timeout")
                   .long_description(unindent(R"(
                        Lockfile timeout for blocking mode when waiting for another process
                        to release the path. Default is 0 (no timeout))")));

        insert(Configurable("use_lockfiles", &m_context.use_lockfiles)
                   .group("Extract, Link & Install")
                   .set_rc_configurable()
                   .set_env_var_names()
                   .description("Enable or disable the usage of filesystem lockfiles for shared resources")
                   .long_description(unindent(R"(
                        By default, mamba uses lockfiles on the filesystem to synchronize access to
                        shared resources for multiple mamba processes (such as the package cache).
                        However, some filesystems do not support file locking and locks do not always
                        make sense - like when on an HPC.  Default is true (use a lockfile)")));

        insert(Configurable("compile_pyc", &m_context.compile_pyc)
                   .group("Extract, Link & Install")
                   .set_rc_configurable()
                   .set_env_var_names()
                   .description("Defines if PYC files will be compiled or not"));

        // Output, Prompt and Flow
        insert(Configurable("always_yes", &m_context.always_yes)
                   .group("Output, Prompt and Flow Control")
                   .set_rc_configurable()
                   .set_env_var_names()
                   .description("Automatically answer yes on prompted questions"));

        insert(Configurable("auto_activate_base", &m_context.auto_activate_base)
                   .group("Output, Prompt and Flow Control")
                   .set_rc_configurable()
                   .set_env_var_names()
                   .description("Automatically activate the base env")
                   .long_description(unindent(R"(
                        Automatically activate the base environment during shell
                        initialization.)")));

        insert(Configurable("dry_run", &m_context.dry_run)
                   .group("Output, Prompt and Flow Control")
                   .set_env_var_names()
                   .description("Only display what would have been done"));

        insert(Configurable("download_only", &m_context.download_only)
                   .group("Output, Prompt and Flow Control")
                   .set_env_var_names()
                   .description("Only download and extract packages, do not link them into environment."
                   ));

        insert(Configurable("log_level", &m_context.output_params.logging_level)
                   .group("Output, Prompt and Flow Control")
                   .set_rc_configurable()
                   .set_env_var_names()
                   .needs({ "json", "verbose" })
                   .description("Set the log level")
                   .set_fallback_value_hook<mamba::log_level>(
                       [this] { return detail::log_level_fallback_hook(*this); }
                   )
                   .long_description(unindent(R"(
                            Set globally the log level of all loggers. Log level can
                            be one of {'off', 'fatal', 'error', 'warning', 'info',
                            'debug', 'trace'}.)")));

        insert(Configurable("log_backtrace", &m_context.output_params.log_backtrace)
                   .group("Output, Prompt and Flow Control")
                   .set_rc_configurable()
                   .set_env_var_names()
                   .description("Set the log backtrace size")
                   .long_description(unindent(R"(
                            Set the log backtrace size. It will replay the n last
                            logs if an error is thrown during the execution.)")));

        insert(Configurable("log_pattern", &m_context.output_params.log_pattern)
                   .group("Output, Prompt and Flow Control")
                   .set_rc_configurable()
                   .set_env_var_names()
                   .description("Set the log pattern")
                   .long_description(unindent(R"(
                            Set the log pattern.)")));

        insert(Configurable("json", &m_context.output_params.json)
                   .group("Output, Prompt and Flow Control")
                   .set_rc_configurable()
                   .needs({ "print_config_only", "print_context_only" })
                   .set_env_var_names()
                   .description("Report all output as json"));

        insert(Configurable("changeps1", &m_context.change_ps1)
                   .group("Output, Prompt and Flow Control")
                   .set_rc_configurable()
                   .set_env_var_names()
                   .description(
                       "When using activate, change the command prompt ($PS1) to include the activated environment."
                   ));

        insert(Configurable("shell_completion", &m_context.shell_completion)
                   .group("Output, Prompt and Flow Control")
                   .set_rc_configurable()
                   .set_env_var_names()
                   .description(
                       "Enable or disable shell autocompletion (currently works for bash and zsh)."
                   ));

        insert(Configurable("env_prompt", &m_context.env_prompt)
                   .group("Output, Prompt and Flow Control")
                   .set_rc_configurable()
                   .set_env_var_names()
                   .description("Template for prompt modification based on the active environment.")
                   .long_description(unindent(R"(
                        Currently supported template variables are '{prefix}', '{name}', and '{default_env}'.
                        '{prefix}' is the absolute path to the active environment. '{name}' is the basename
                        of the active environment prefix. '{default_env}' holds the value of '{name}' if the
                        active environment is a named environment ('-n' flag), or otherwise holds the value
                        of '{prefix}'.)")));

        insert(Configurable("print_config_only", false)
                   .group("Output, Prompt and Flow Control")
                   .needs({ "debug" })
                   .set_post_merge_hook<bool>(
                       [&](bool& value) { return detail::print_config_only_hook(*this, value); }
                   )
                   .description("Print the context after loading the config. Allow ultra-dry runs"));

        insert(Configurable("print_context_only", false)
                   .group("Output, Prompt and Flow Control")
                   .needs({ "debug" })
                   .set_post_merge_hook<bool>(
                       [&](bool& value) { return detail::print_context_only_hook(*this, value); }
                   )
                   .description("Print the context after loading the config. Allow ultra-dry runs"));

        insert(Configurable("show_all_configs", false)
                   .group("Output, Prompt and Flow Control")
                   .description("Display all configs, including not rc configurable"));

        insert(Configurable("show_all_rc_configs", false)
                   .group("Output, Prompt and Flow Control")
                   .description("Display all rc configurable configs"));

        insert(Configurable("show_config_descriptions", false)
                   .group("Output, Prompt and Flow Control")
                   .description("Display configs descriptions"));

        insert(Configurable("show_config_groups", false)
                   .group("Output, Prompt and Flow Control")
                   .description("Display configs groups"));

        insert(Configurable("show_config_long_descriptions", false)
                   .group("Output, Prompt and Flow Control")
                   .description("Display configs long descriptions"));

        insert(Configurable("show_config_sources", false)
                   .group("Output, Prompt and Flow Control")
                   .description("Display all configs sources"));

        insert(Configurable("show_config_values", false)
                   .group("Output, Prompt and Flow Control")
                   .description("Display configs values"));

        insert(Configurable("quiet", &m_context.output_params.quiet)
                   .group("Output, Prompt and Flow Control")
                   .set_rc_configurable()
                   .set_env_var_names()
                   .needs({ "json", "print_config_only", "print_context_only" })
                   .description("Set quiet mode (print less output)"));

        insert(Configurable("verbose", 0)
                   .group("Output, Prompt and Flow Control")
                   .set_post_merge_hook<int>([this](int& value)
                                             { return detail::verbose_hook(m_context, value); })
                   .description("Set the verbosity")
                   .long_description(unindent(R"(
                    Set the verbosity of .
                    The verbosity represent the information
                    given to the user about the operation asked for.
                    This information is printed to stdout and should
                    not be considered as logs (see log_level).)")));

        // Config
        insert(Configurable("rc_files", std::vector<fs::u8path>({}))
                   .group("Config sources")
                   .set_env_var_names({ "MAMBARC", "CONDARC" })
                   .needs({ "no_rc" })
                   .set_post_merge_hook<std::vector<fs::u8path>>(
                       [this](std::vector<fs::u8path>& value)
                       { return detail::rc_files_hook(m_context, value); }
                   )
                   .description("Paths to the configuration files to use"));

        insert(Configurable("override_rc_files", true)
                   .group("Config sources")
                   .set_env_var_names()
                   .description("Whether to override rc files by highest precedence"));

        insert(Configurable("no_rc", &m_context.src_params.no_rc)
                   .group("Config sources")
                   .set_env_var_names()
                   .description("Disable the use of configuration files"));

        insert(Configurable("no_env", &m_context.src_params.no_env)
                   .group("Config sources")
                   .set_env_var_names()
                   .description("Disable the use of environment variables"));
    }

    Configuration::~Configuration() = default;

    void Configuration::reset_configurables()
    {
        m_config.clear();
        m_config_order.clear();
        set_configurables();
    }

    auto Configuration::get_grouped_config() const -> std::vector<grouped_config_type>
    {
        std::map<std::string, std::vector<const Configurable*>> map;
        std::vector<std::pair<std::string, std::vector<const Configurable*>>> res;
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

    // Precedence is initially set least to most, and then at the end the list is reversed.
    // Configuration::set_rc_values iterates over all config options, and then over all config
    // file source. Essentially first come first serve.
    // just FYI re "../conda": env::user_config_dir's default value is $XDG_CONFIG_HOME/mamba
    // But we wanted to also allow $XDG_CONFIG_HOME/conda and '..' seems like the best way to
    // make it conda/mamba compatible. Otherwise I would have to set user_config_dir to either
    // be just $XDG_CONFIG_HOME and always supply mamba after calling it, or I would have to
    // give env::user_config_dir a mamba argument, all so I can supply conda in a few default
    // cases. It seems like ../conda is an easier solution
    //
    std::vector<fs::u8path>
    Configuration::compute_default_rc_sources(const Context& context, const RCConfigLevel& level)
    {
        std::vector<fs::u8path> system;
        if constexpr (util::on_mac || util::on_linux)
        {
            system = { "/etc/conda/.condarc",       "/etc/conda/condarc",
                       "/etc/conda/condarc.d/",     "/etc/conda/.mambarc",
                       "/var/lib/conda/.condarc",   "/var/lib/conda/condarc",
                       "/var/lib/conda/condarc.d/", "/var/lib/conda/.mambarc" };
        }
        else
        {
            system = { "C:\\ProgramData\\conda\\.condarc",
                       "C:\\ProgramData\\conda\\condarc",
                       "C:\\ProgramData\\conda\\condarc.d",
                       "C:\\ProgramData\\conda\\.mambarc" };
        }

        std::vector<fs::u8path> root = { context.prefix_params.root_prefix / ".condarc",
                                         context.prefix_params.root_prefix / "condarc",
                                         context.prefix_params.root_prefix / "condarc.d",
                                         context.prefix_params.root_prefix / ".mambarc" };

        std::vector<fs::u8path> conda_user = {
            fs::u8path(util::user_config_dir()) / "conda/.condarc",
            fs::u8path(util::user_config_dir()) / "conda/condarc",
            fs::u8path(util::user_config_dir()) / "conda/condarc.d",
            fs::u8path(util::user_home_dir()) / ".conda/.condarc",
            fs::u8path(util::user_home_dir()) / ".conda/condarc",
            fs::u8path(util::user_home_dir()) / ".conda/condarc.d",
            fs::u8path(util::user_home_dir()) / ".condarc",
        };
        
        std::array<std::string, 3> condarc_list = {"/.condarc", "/condarc", "/condarc.d"};
        if (util::get_env("XDG_CONFIG_HOME"))
        {
            const std::string xgd_config_home = util::get_env("XDG_CONFIG_HOME").value();
            for (const auto& path: condarc_list)
            {
                conda_user.push_back(fs::u8path(xgd_config_home) + "/conda"  + path);
            }       
        }
        if (util::get_env("CONDA_PREFIX"))
        {
            const std::string conda_prefix = util::get_env("CONDA_PREFIX").value();
            for (const auto& path: condarc_list)
            {
                conda_user.push_back(fs::u8path(conda_prefix) + path);
            }       
        }
        
        if (util::get_env("CONDARC"))
        {
            conda_user.push_back(fs::u8path(util::get_env("CONDARC").value()));
        }

        std::vector<fs::u8path> mamba_user = {
            fs::u8path(util::user_config_dir()) / "mamba/.mambarc",
            fs::u8path(util::user_config_dir()) / "mamba/mambarc",
            fs::u8path(util::user_config_dir()) / "mamba/mambarc.d",
            fs::u8path(util::user_home_dir()) / ".mamba/.mambarc",
            fs::u8path(util::user_home_dir()) / ".mamba/mambarc",
            fs::u8path(util::user_home_dir()) / ".mamba/mambarc.d",
            fs::u8path(util::user_home_dir()) / ".mambarc",
        };
        if (util::get_env("MAMBARC"))
        {
            mamba_user.push_back(fs::u8path(util::get_env("MAMBARC").value()));
        }

        std::vector<fs::u8path> prefix = { context.prefix_params.target_prefix / ".condarc",
                                           context.prefix_params.target_prefix / "condarc",
                                           context.prefix_params.target_prefix / "condarc.d",
                                           context.prefix_params.target_prefix / ".mambarc" };

        std::vector<fs::u8path> sources;
        std::set<fs::u8path> known_locations;

        // We only want to insert locations once, with the least precedence
        // to emulate conda's IndexSet behavior

        // This is especially important when the base env is active
        // as target_prefix and root_prefix are the same.
        // If there is a .condarc in the root_prefix, we don't want
        // to load it twice, once for the root_prefix and once for the
        // target_prefix with the highest precedence.
        auto insertIntoSources = [&](const std::vector<fs::u8path>& locations)
        {
            for (auto& location : locations)
            {
                if (known_locations.insert(location).second)
                {
                    sources.emplace_back(location);
                }
            }
        };

        if (level >= RCConfigLevel::kSystemDir)
        {
            insertIntoSources(system);
        }
        if ((level >= RCConfigLevel::kRootPrefix) && !context.prefix_params.root_prefix.empty())
        {
            insertIntoSources(root);
        }
        if (level >= RCConfigLevel::kHomeDir)
        {
            insertIntoSources(conda_user);
            insertIntoSources(mamba_user);
        }
        if ((level >= RCConfigLevel::kTargetPrefix) && !context.prefix_params.target_prefix.empty())
        {
            insertIntoSources(prefix);
        }

        // Sort by precedence
        std::reverse(sources.begin(), sources.end());

        return sources;
    }

    void Configuration::load()
    {
        spdlog::set_level(spdlog::level::n_levels);
        spdlog::flush_on(spdlog::level::n_levels);
        // Hard-coded value assuming it's enough to store the logs emitted
        // before setting the log level, flushing the backtrace and setting
        // its new capacity
        spdlog::enable_backtrace(500);

        LOG_DEBUG << "Loading configuration";

        clear_rc_sources();
        clear_rc_values();

        compute_loading_sequence();
        reset_compute_counters();

        m_load_lock = true;
        for (auto& c : m_loading_sequence)
        {
            at(c).compute();
        }
        m_load_lock = false;

        allow_file_locking(m_context.use_lockfiles);
        set_file_locking_timeout(std::chrono::seconds{ m_context.lock_timeout });

        LOG_DEBUG << m_config.size() << " configurables computed";

        if (this->at("print_config_only").value<bool>())
        {
            int dump_opts = MAMBA_SHOW_CONFIG_VALUES | MAMBA_SHOW_CONFIG_SRCS
                            | MAMBA_SHOW_ALL_CONFIGS;
            std::cout << this->dump(dump_opts) << std::endl;
            exit(0);
        }

        m_context.set_log_level(m_context.output_params.logging_level);

        spdlog::apply_all([&](std::shared_ptr<spdlog::logger> l) { l->flush(); });
        spdlog::flush_on(spdlog::level::off);

        m_context.dump_backtrace_no_guards();
        if (m_context.output_params.log_backtrace > 0)
        {
            spdlog::enable_backtrace(m_context.output_params.log_backtrace);
        }
        else
        {
            spdlog::disable_backtrace();
        }
    }

    bool Configuration::is_loading()
    {
        return m_load_lock;
    }

    void Configuration::compute_loading_sequence()
    {
        m_loading_sequence.clear();

        std::vector<std::string> locks;
        for (auto& c : m_config_order)
        {
            add_to_loading_sequence(m_loading_sequence, c, locks);
        }
    }

    void Configuration::add_to_loading_sequence(
        std::vector<std::string>& seq,
        const std::string& name,
        std::vector<std::string>& locks
    )
    {
        auto found = std::find(seq.begin(), seq.end(), name);

        if (found == seq.end())
        {
            at(name).lock();
            locks.push_back(name);

            for (auto& n : at(name).needed())
            {
                if (at(n).locked())
                {
                    LOG_ERROR << "Circular import: " << util::join("->", locks) << "->" << n;
                    throw std::runtime_error("Circular import detected in configuration. Aborting.");
                }
                add_to_loading_sequence(seq, n, locks);
            }

            // The given config may have been added by implied configs
            found = std::find(seq.begin(), seq.end(), name);
            if (found == seq.end())
            {
                seq.push_back(name);
            }

            at(name).free();
            locks.pop_back();

            for (auto& n : at(name).implied())
            {
                add_to_loading_sequence(seq, n, locks);
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

    void Configuration::clear_rc_sources()
    {
        m_sources.clear();
        m_valid_sources.clear();
        m_rc_yaml_nodes_cache.clear();
    }

    void Configuration::clear_cli_values()
    {
        for (auto& c : m_config)
        {
            c.second.clear_cli_value();
        }
    }

    void Configuration::clear_values()
    {
        for (auto& c : m_config)
        {
            c.second.clear_values();
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

    std::vector<fs::u8path> Configuration::sources() const
    {
        return m_sources;
    }

    std::vector<fs::u8path> Configuration::valid_sources() const
    {
        return m_valid_sources;
    }

    std::map<std::string, Configurable>& Configuration::config()
    {
        return m_config;
    }

    const std::map<std::string, Configurable>& Configuration::config() const
    {
        return m_config;
    }

    namespace
    {
        template <typename MapType>
        auto& configuration_at_impl(const std::string& key, MapType&& map)
        {
            try
            {
                return std::forward<MapType>(map).at(key);
            }
            catch (const std::out_of_range& /*e*/)
            {
                LOG_ERROR << "Configurable '" << key << "' does not exists";
                throw std::runtime_error("ConfigurationError");
            }
        }
    }

    Configurable& Configuration::at(const std::string& name)
    {
        return configuration_at_impl(name, m_config);
    }

    const Configurable& Configuration::at(const std::string& name) const
    {
        return configuration_at_impl(name, m_config);
    }

    YAML::Node Configuration::load_rc_file(const fs::u8path& file)
    {
        YAML::Node config;
        try
        {
            std::ifstream inFile;
            inFile.open(file.std_path());
            std::stringstream strStream;
            strStream << inFile.rdbuf();
            std::string s = strStream.str();
            config = YAML::Load(expandvars(s));
            if (config.IsScalar())
            {
                LOG_WARNING << fmt::format(
                    "The configuration file at {} is misformatted or corrupted. Skipping file.",
                    file.string()
                );
                return YAML::Node();
            }
        }
        catch (const std::exception& ex)
        {
            LOG_ERROR << fmt::format("Error in file {}, skipping: {}", file.string(), ex.what());
        }
        return config;
    }

    void
    Configuration::set_rc_values(std::vector<fs::u8path> possible_rc_paths, const RCConfigLevel& level)
    {
        LOG_TRACE << "Get RC files configuration from locations up to "
                  << YAML::Node(level).as<std::string>();
        if (possible_rc_paths.empty())
        {
            possible_rc_paths = compute_default_rc_sources(m_context, level);
        }

        m_sources = get_existing_rc_sources(possible_rc_paths);
        m_valid_sources.clear();

        for (const auto& s : m_sources)
        {
            if (!m_rc_yaml_nodes_cache.count(s))
            {
                auto node = load_rc_file(s);
                if (node.IsNull())
                {
                    continue;
                }

                m_rc_yaml_nodes_cache.insert({ s, node });
            }
            m_valid_sources.push_back(s);
        }

        if (!m_valid_sources.empty())
        {
            for (auto& it : m_config)
            {
                auto& key = it.first;
                auto& c = it.second;

                if (!c.rc_configurable() || (c.rc_configurable_level() > level) || c.rc_configured())
                {
                    continue;
                }

                for (const auto& source : m_valid_sources)
                {
                    auto yaml = m_rc_yaml_nodes_cache[source];
                    if (!yaml[key] || yaml[key].IsNull())
                    {
                        continue;
                    }

                    c.set_rc_yaml_value(yaml[key], util::shrink_home(source.string()));
                }
            }
        }
    }

    std::vector<fs::u8path>
    Configuration::get_existing_rc_sources(const std::vector<fs::u8path>& possible_rc_paths)
    {
        std::vector<fs::u8path> sources;

        for (const fs::u8path& l : possible_rc_paths)
        {
            if (detail::is_config_file(l))
            {
                sources.push_back(l);
                LOG_TRACE << "Configuration found at '" << l.string() << "'";
            }
            else if (fs::is_directory(l))
            {
                for (fs::u8path p : fs::directory_iterator(l))
                {
                    if (detail::is_config_file(p))
                    {
                        sources.push_back(p);
                        LOG_TRACE << "Configuration found at '" << p.string() << "'";
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
                {
                    LOG_TRACE << "Configuration not found at '" << l.string() << "'";
                }
            }
        }

        return sources;
    }

    /*
     *
     */

    namespace nl = nlohmann;

    namespace detail
    {
        void dump_configurable(nl::json& node, const Configurable& c, const std::string& name);
    }

    std::string dump_json(
        int opts,
        const std::vector<std::string>& names,
        const std::vector<Configuration::grouped_config_type>& grouped_config
    )
    {
        // bool show_values = opts & MAMBA_SHOW_CONFIG_VALUES;
        bool show_sources = opts & MAMBA_SHOW_CONFIG_SRCS;
        bool show_descs = opts & MAMBA_SHOW_CONFIG_DESCS;
        bool show_long_descs = opts & MAMBA_SHOW_CONFIG_LONG_DESCS;
        bool show_groups = opts & MAMBA_SHOW_CONFIG_GROUPS;
        bool show_all_rcs = opts & MAMBA_SHOW_ALL_RC_CONFIGS;
        bool show_all = opts & MAMBA_SHOW_ALL_CONFIGS;

        bool dump_group = (show_descs || show_long_descs) && show_groups;
        nl::json root;
        for (auto& group_it : grouped_config)
        {
            std::string group_name = group_it.first;
            const auto& configs = group_it.second;

            nl::json group;
            nl::json& json_node = dump_group ? group : root;

            for (const auto& c : configs)
            {
                auto is_required = std::find(names.begin(), names.end(), c->name()) != names.end();
                if (!names.empty() && !is_required)
                {
                    continue;
                }

                if ((c->rc_configurable() && (c->configured() || show_all_rcs)) || is_required
                    || show_all)
                {
                    if (show_descs || show_long_descs)
                    {
                        nl::json json_conf;
                        if (show_long_descs)
                        {
                            json_conf["long_description"] = c->long_description();
                        }
                        else
                        {
                            json_conf["description"] = c->description();
                        }
                        if (show_sources)
                        {
                            json_conf["source"] = c->source();
                        }
                        detail::dump_configurable(json_conf, *c, "value");
                        json_node[c->name()] = std::move(json_conf);
                    }
                    else
                    {
                        if (show_sources)
                        {
                            nl::json json_conf;
                            detail::dump_configurable(json_conf, *c, "value");
                            json_conf["source"] = c->source();
                            json_node[c->name()] = std::move(json_conf);
                        }
                        else
                        {
                            detail::dump_configurable(json_node, *c, c->name());
                        }
                    }
                }
            }

            if (dump_group)
            {
                root[group_name + "Configuration"] = std::move(json_node);
            }
        }
        return root.dump(4);
    }

    std::string dump_yaml(
        int opts,
        const std::vector<std::string>& names,
        const std::vector<Configuration::grouped_config_type>& grouped_config
    )
    {
        bool show_values = opts & MAMBA_SHOW_CONFIG_VALUES;
        bool show_sources = opts & MAMBA_SHOW_CONFIG_SRCS;
        bool show_descs = opts & MAMBA_SHOW_CONFIG_DESCS;
        bool show_long_descs = opts & MAMBA_SHOW_CONFIG_LONG_DESCS;
        bool show_groups = opts & MAMBA_SHOW_CONFIG_GROUPS;
        bool show_all_rcs = opts & MAMBA_SHOW_ALL_RC_CONFIGS;
        bool show_all = opts & MAMBA_SHOW_ALL_CONFIGS;

        bool first_config = true;
        YAML::Emitter out;
        // out.SetNullFormat(YAML::EMITTER_MANIP::LowerNull); // TODO: switch from ~ to null

        for (auto& group_it : grouped_config)
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

                if ((c->rc_configurable() && (c->configured() || show_all_rcs)) || is_required
                    || show_all)
                {
                    if (show_descs || show_long_descs)
                    {
                        if (show_groups && first_group_config)
                        {
                            if (!first_config)
                            {
                                out << YAML::Newline << YAML::Newline;
                            }
                            detail::print_group_title(out, group_name);
                        }

                        if (!first_config || (first_config && show_groups))
                        {
                            out << YAML::Newline << YAML::Newline;
                        }

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
                        {
                            out << YAML::BeginMap;
                        }
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
        {
            out << YAML::EndMap;
        }

        return out.c_str();
    }

    std::string Configuration::dump(int opts, std::vector<std::string> names) const
    {
        if (m_config.at("json").value<bool>())
        {
            return dump_json(opts, names, get_grouped_config());
        }
        else
        {
            return dump_yaml(opts, names, get_grouped_config());
        }
    }
}
