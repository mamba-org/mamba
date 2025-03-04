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
#include "mamba/core/util.hpp"

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
        auto& use_explicit = config.at("explicit_install").value<bool>();
        auto& json_format = config.at("json").get_cli_config<bool>();

        auto channel_context = ChannelContext::make_conda_compatible(ctx);

        bool remove_prefix_on_failure = false;
        bool create_env = true;

        if (!ctx.dry_run)
        {
            if (fs::exists(ctx.prefix_params.target_prefix))
            {
                if (ctx.prefix_params.target_prefix == ctx.prefix_params.root_prefix)
                {
                    LOG_ERROR << "Overwriting root prefix is not permitted";
                    throw std::runtime_error("Aborting.");
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
                    LOG_ERROR << "Non-conda folder exists at prefix";
                    throw std::runtime_error("Aborting.");
                }
            }
            if (create_specs.empty())
            {
                detail::create_empty_target(ctx, ctx.prefix_params.target_prefix);
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
            if (create_specs.empty() && json_format)
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
