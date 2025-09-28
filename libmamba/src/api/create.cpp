// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <iostream>

#include "mamba/api/configuration.hpp"
#include "mamba/api/create.hpp"
#include "mamba/api/install.hpp"
#include "mamba/core/channel_context.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/environments_manager.hpp"
#include "mamba/core/prefix_data.hpp"
#include "mamba/core/util.hpp"
#include "mamba/util/path_manip.hpp"

namespace mamba
{
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
        auto& clone_env_name = config.at("clone_env").value<std::string>();
        auto& use_explicit = config.at("explicit_install").value<bool>();
        auto& json_format = config.at("json").get_cli_config<bool>();
        auto& env_vars = config.at("spec_file_env_vars").value<std::map<std::string, std::string>>();
        auto& no_env = config.at("no_env").value<bool>();

        auto channel_context = ChannelContext::make_conda_compatible(ctx);

        // Handle clone environment logic
        std::vector<std::string> clone_specs;
        if (!clone_env_name.empty())
        {
            // Validate that no other specs are provided when cloning
            if (!create_specs.empty())
            {
                const auto message = "Cannot specify packages when cloning an environment. Use --clone only.";
                LOG_ERROR << message;
                throw mamba_error(message, mamba_error_code::incorrect_usage);
            }

            auto& file_specs = config.at("file_specs").value<std::vector<std::string>>();
            if (!file_specs.empty())
            {
                const auto message = "Cannot specify environment file when cloning an environment. Use --clone only.";
                LOG_ERROR << message;
                throw mamba_error(message, mamba_error_code::incorrect_usage);
            }

            // Determine source environment path
            fs::u8path source_prefix;
            if (fs::exists(clone_env_name))
            {
                // Direct path provided
                source_prefix = clone_env_name;
            }
            else
            {
                // Environment name provided, look for it
                if (clone_env_name == "base")
                {
                    source_prefix = ctx.prefix_params.root_prefix;
                }
                else
                {
                    // Search in envs directories
                    bool found = false;
                    for (const auto& envs_dir : ctx.envs_dirs)
                    {
                        auto potential_path = envs_dir / clone_env_name;
                        if (fs::exists(potential_path) && is_conda_environment(potential_path))
                        {
                            source_prefix = potential_path;
                            found = true;
                            break;
                        }
                    }
                    if (!found)
                    {
                        const auto message = "Environment '" + clone_env_name + "' not found.";
                        LOG_ERROR << message;
                        throw mamba_error(message, mamba_error_code::incorrect_usage);
                    }
                }
            }

            // Validate source environment exists and is a conda environment
            if (!fs::exists(source_prefix) || !is_conda_environment(source_prefix))
            {
                const auto message = "Source environment '" + source_prefix.string() + "' is not a valid conda environment.";
                LOG_ERROR << message;
                throw mamba_error(message, mamba_error_code::incorrect_usage);
            }

            // Get package list from source environment
            auto maybe_source_prefix_data = PrefixData::create(source_prefix, channel_context);
            if (!maybe_source_prefix_data)
            {
                const auto message = "Could not load prefix data from source environment: " + maybe_source_prefix_data.error().what();
                LOG_ERROR << message;
                throw mamba_error(message, mamba_error_code::incorrect_usage);
            }
            auto records = maybe_source_prefix_data.value().sorted_records();
            
            // Convert records to specs for installation
            for (const auto& record : records)
            {
                clone_specs.push_back(record.name + "=" + record.version + "=" + record.build_string);
            }

            LOG_INFO << "Cloning environment '" << source_prefix.string() << "' with " << clone_specs.size() << " packages.";
        }

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
            if (create_specs.empty() && clone_specs.empty())
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
            if (create_specs.empty() && clone_specs.empty() && json_format)
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
        else if (!create_specs.empty() || !clone_specs.empty())
        {
            // Use clone_specs if available, otherwise use create_specs
            const auto& specs_to_install = !clone_specs.empty() ? clone_specs : create_specs;
            
            if (use_explicit)
            {
                install_explicit_specs(
                    ctx,
                    channel_context,
                    specs_to_install,
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
                    specs_to_install,
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
