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

            std::vector<std::string> explicit_urls;
            const auto records = source_prefix_data.sorted_records();
            explicit_urls.reserve(records.size());

            for (const auto& pkg : records)
            {
                if (pkg.package_url.empty())
                {
                    // Fallback to channel/platform/filename if possible.
                    if (pkg.channel.empty() || pkg.platform.empty() || pkg.filename.empty())
                    {
                        LOG_WARNING << "Skipping package without URL information while cloning: "
                                    << pkg.name;
                        continue;
                    }
                    const auto url = pkg.url_for_channel(pkg.channel);
                    explicit_urls.push_back(url);
                }
                else
                {
                    std::string url = pkg.package_url;
                    if (!pkg.sha256.empty())
                    {
                        url += "#sha256:" + pkg.sha256;
                    }
                    else if (!pkg.md5.empty())
                    {
                        url += "#" + pkg.md5;
                    }
                    explicit_urls.push_back(std::move(url));
                }
            }

            install_explicit_specs(ctx, channel_context, explicit_urls, create_env, remove_prefix_on_failure);
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
            clone_environment(ctx, channel_context, source_prefix, create_env, remove_prefix_on_failure);
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
