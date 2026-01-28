// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <fstream>
#include <iostream>

#include <yaml-cpp/yaml.h>

#include "mamba/api/configuration.hpp"
#include "mamba/api/create.hpp"
#include "mamba/api/install.hpp"
#include "mamba/core/channel_context.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/environments_manager.hpp"
#include "mamba/core/error_handling.hpp"
#include "mamba/core/prefix_data.hpp"
#include "mamba/core/util.hpp"
#include "mamba/specs/package_info.hpp"

namespace mamba
{
    namespace
    {
        fs::u8path compute_clone_source_prefix(const Context& ctx, const std::string& clone_value)
        {
            if (clone_value.empty())
            {
                throw mamba_error("Empty clone source provided", mamba_error_code::incorrect_usage);
            }

            // Treat values containing a path separator as a path, otherwise as an env name.
            if (clone_value.find_first_of("/\\") != std::string::npos)
            {
                return fs::u8path(clone_value);
            }

            if (clone_value == ROOT_ENV_NAME)
            {
                return ctx.prefix_params.root_prefix;
            }

            for (const auto& dir : ctx.envs_dirs)
            {
                const auto candidate = dir / clone_value;
                if (is_conda_environment(candidate))
                {
                    return candidate;
                }
            }

            throw mamba_error(
                "Could not find environment to clone: " + clone_value,
                mamba_error_code::incorrect_usage
            );
        }

        void clone_environment(
            Context& ctx,
            ChannelContext& channel_context,
            Configuration& config,
            const fs::u8path& source_prefix,
            bool create_env,
            bool remove_prefix_on_failure
        )
        {
            if (!is_conda_environment(source_prefix))
            {
                const auto msg = "Source prefix '" + source_prefix.string()
                                 + "' is not a valid conda environment.";
                LOG_ERROR << msg;
                throw mamba_error(msg, mamba_error_code::incorrect_usage);
            }

            auto maybe_prefix_data = PrefixData::create(source_prefix, channel_context);
            if (!maybe_prefix_data)
            {
                throw maybe_prefix_data.error();
            }
            const PrefixData& source_prefix_data = maybe_prefix_data.value();

            // Export source environment to YAML format
            TemporaryFile yaml_file("mamba_env_", ".yml");
            {
                YAML::Emitter out;
                out << YAML::BeginMap;

                // Add dependencies section
                out << YAML::Key << "dependencies" << YAML::Value << YAML::BeginSeq;

                // Add conda packages
                const auto records = source_prefix_data.sorted_records();
                for (const auto& pkg : records)
                {
                    out << fmt::format("{}={}={}", pkg.name, pkg.version, pkg.build_string);
                }

                // Add pip packages as a sub-map
                const auto& pip_records = source_prefix_data.pip_records();
                if (!pip_records.empty())
                {
                    out << YAML::BeginMap;
                    out << YAML::Key << "pip" << YAML::Value << YAML::BeginSeq;
                    for (const auto& [name, pkg] : pip_records)
                    {
                        out << (pkg.name + "==" + pkg.version);
                    }
                    out << YAML::EndSeq;
                    out << YAML::EndMap;
                }

                out << YAML::EndSeq;
                out << YAML::EndMap;

                // Write YAML to temporary file
                std::ofstream yaml_out = open_ofstream(yaml_file.path());
                yaml_out << out.c_str();
                yaml_out.close();
            }

            // Read YAML file and populate config
            const auto parse_result = detail::read_yaml_file(
                ctx,
                yaml_file.path().string(),
                ctx.platform,
                ctx.use_uv
            );

            // Populate config with parsed YAML contents
            if (!parse_result.dependencies.empty())
            {
                auto& specs = config.at("specs").value<std::vector<std::string>>();
                specs.insert(
                    specs.end(),
                    parse_result.dependencies.begin(),
                    parse_result.dependencies.end()
                );
            }

            if (!parse_result.others_pkg_mgrs_specs.empty())
            {
                auto& others_pkg_mgrs_specs = config.at("others_pkg_mgrs_specs")
                                                  .value<std::vector<detail::other_pkg_mgr_spec>>();
                others_pkg_mgrs_specs.insert(
                    others_pkg_mgrs_specs.end(),
                    parse_result.others_pkg_mgrs_specs.begin(),
                    parse_result.others_pkg_mgrs_specs.end()
                );
            }

            // Install packages from config
            const auto& install_specs_vec = config.at("specs").value<std::vector<std::string>>();
            install_specs(
                ctx,
                channel_context,
                config,
                install_specs_vec,
                create_env,
                remove_prefix_on_failure
            );
        }
    }  // namespace

    void create(Configuration& config)
    {
        auto& ctx = config.context();

        config.at("use_target_prefix_fallback").set_value(false);
        config.at("use_default_prefix_fallback").set_value(false);
        config.at("use_root_prefix_fallback").set_value(false);
        config.at("target_prefix_checks")
            .set_value(
                MAMBA_ALLOW_EXISTING_PREFIX | MAMBA_ALLOW_NOT_ENV_PREFIX
                | MAMBA_NOT_ALLOW_MISSING_PREFIX | MAMBA_NOT_EXPECT_EXISTING_PREFIX
            );
        config.load();

        auto& create_specs = config.at("specs").value<std::vector<std::string>>();
        auto& use_explicit = config.at("explicit_install").value<bool>();
        auto& json_format = config.at("json").get_cli_config<bool>();
        auto& env_vars = config.at("spec_file_env_vars").value<std::map<std::string, std::string>>();
        auto& no_env = config.at("no_env").value<bool>();
        auto& clone_cfg = config.at("clone");
        const bool is_clone = clone_cfg.configured() && !clone_cfg.value<std::string>().empty();

        if (is_clone)
        {
            if (!create_specs.empty())
            {
                const auto msg = "Cannot use --clone together with package specs.";
                LOG_ERROR << msg;
                throw mamba_error(msg, mamba_error_code::incorrect_usage);
            }
            if (config.at("file_specs").configured())
            {
                const auto msg = "Cannot use --clone together with --file.";
                LOG_ERROR << msg;
                throw mamba_error(msg, mamba_error_code::incorrect_usage);
            }
            if (ctx.env_lockfile)
            {
                const auto msg = "Cannot use --clone together with an environment lockfile.";
                LOG_ERROR << msg;
                throw mamba_error(msg, mamba_error_code::incorrect_usage);
            }
        }

        auto channel_context = ChannelContext::make_conda_compatible(ctx);

        bool remove_prefix_on_failure = false;
        bool create_env = true;

        if (!ctx.dry_run)
        {
            if (fs::exists(ctx.prefix_params.target_prefix))
            {
                if (ctx.prefix_params.target_prefix == ctx.prefix_params.root_prefix)
                {
                    const auto message = "Overwriting root prefix is not permitted - aborting.";
                    LOG_ERROR << message;
                    throw mamba_error(message, mamba_error_code::incorrect_usage);
                }
                else if (!fs::is_directory(ctx.prefix_params.target_prefix))
                {
                    const auto message = "Target prefix already exists and is not a folder - aborting.";
                    LOG_ERROR << message;
                    throw mamba_error(message, mamba_error_code::incorrect_usage);
                }
                else if (fs::is_empty(ctx.prefix_params.target_prefix))
                {
                    LOG_WARNING << "Using existing empty folder as target prefix";
                }
                else if (fs::exists(ctx.prefix_params.target_prefix / "conda-meta"))
                {
                    if (Console::prompt(
                            "Found conda-prefix at '" + ctx.prefix_params.target_prefix.string()
                                + "'. Overwrite?",
                            'n'
                        ))
                    {
                        fs::remove_all(ctx.prefix_params.target_prefix);
                    }
                    else
                    {
                        throw std::runtime_error("Aborting.");
                    }
                }
                else
                {
                    const auto message = "Non-conda folder exists at prefix - aborting.";
                    LOG_ERROR << message;
                    throw mamba_error(message, mamba_error_code::incorrect_usage);
                }
            }
            if (!is_clone && create_specs.empty())
            {
                detail::create_empty_target(ctx, ctx.prefix_params.target_prefix, env_vars, no_env);
            }

            if (config.at("platform").configured() && !config.at("platform").rc_configured())
            {
                detail::store_platform_config(
                    ctx.prefix_params.target_prefix,
                    ctx.platform,
                    remove_prefix_on_failure
                );
            }
        }
        else
        {
            if (!is_clone && create_specs.empty() && json_format)
            {
                // Just print the JSON
                nlohmann::json output;
                output["actions"]["FETCH"] = nlohmann::json::array();
                output["actions"]["PREFIX"] = ctx.prefix_params.target_prefix;
                output["dry_run"] = true;
                output["prefix"] = ctx.prefix_params.target_prefix;
                output["success"] = true;
                std::cout << output.dump(2) << std::endl;
                return;
            }
        }

        if (is_clone)
        {
            const auto clone_value = clone_cfg.value<std::string>();
            const auto source_prefix = compute_clone_source_prefix(ctx, clone_value);
            clone_environment(
                ctx,
                channel_context,
                config,
                source_prefix,
                create_env,
                remove_prefix_on_failure
            );
            return;
        }

        if (ctx.env_lockfile)
        {
            const auto lockfile_path = ctx.env_lockfile.value();
            install_lockfile_specs(
                ctx,
                channel_context,
                lockfile_path,
                config.at("categories").value<std::vector<std::string>>(),
                create_env,
                remove_prefix_on_failure
            );
        }
        else if (!create_specs.empty())
        {
            if (use_explicit)
            {
                install_explicit_specs(
                    ctx,
                    channel_context,
                    create_specs,
                    create_env,
                    remove_prefix_on_failure
                );
            }
            else
            {
                install_specs(
                    ctx,
                    channel_context,
                    config,
                    create_specs,
                    create_env,
                    remove_prefix_on_failure
                );
            }
        }
    }

    namespace detail
    {
        void store_platform_config(
            const fs::u8path& prefix,
            const std::string& platform,
            bool& remove_prefix_on_failure
        )
        {
            if (!fs::exists(prefix))
            {
                remove_prefix_on_failure = true;
                fs::create_directories(prefix);
            }

            auto out = open_ofstream(prefix / ".mambarc");
            out << "platform: " << platform;
        }
    }
}
