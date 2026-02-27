// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <fstream>
#include <iostream>
#include <set>
#include <string>

#include <nlohmann/json.hpp>

#include "mamba/api/configuration.hpp"
#include "mamba/api/create.hpp"
#include "mamba/api/env.hpp"
#include "mamba/api/environment_yaml.hpp"
#include "mamba/api/install.hpp"
#include "mamba/api/remove.hpp"
#include "mamba/api/update.hpp"
#include "mamba/core/channel_context.hpp"
#include "mamba/core/environments_manager.hpp"
#include "mamba/core/history.hpp"
#include "mamba/core/prefix_data.hpp"
#include "mamba/core/util.hpp"
#include "mamba/specs/conda_url.hpp"
#include "mamba/util/path_manip.hpp"
#include "mamba/util/string.hpp"

#include "common_options.hpp"

void
set_env_command(CLI::App* com, mamba::Configuration& config)
{
    init_general_options(com, config);
    init_prefix_options(com, config);

    // env list subcommand
    auto* list_subcom = com->add_subcommand("list", "List known environments");
    init_general_options(list_subcom, config);
    init_prefix_options(list_subcom, config);

    list_subcom->callback([&config] { mamba::print_envs(config); });

    // env create subcommand
    auto* create_subcom = com->add_subcommand(
        "create",
        "Create new environment (pre-commit.com compatibility alias for 'micromamba create')"
    );
    init_install_options(create_subcom, config);
    create_subcom->callback([&] { return mamba::create(config); });

    // env export subcommand
    static bool explicit_format = false;
    static int no_md5 = 0;
    static bool no_build = false;
    static bool channel_subdir = false;
    static bool from_history = false;

    auto* export_subcom = com->add_subcommand("export", "Export environment");

    init_general_options(export_subcom, config);
    init_prefix_options(export_subcom, config);

    export_subcom->add_flag("-e,--explicit", explicit_format, "Use explicit format");
    export_subcom->add_flag("--no-md5,!--md5", no_md5, "Disable md5");
    export_subcom
        ->add_flag("--no-build,--no-builds,!--build", no_build, "Disable the build string in spec");
    export_subcom->add_flag("--channel-subdir", channel_subdir, "Enable channel/subdir in spec");
    export_subcom->add_flag(
        "--from-history",
        from_history,
        "Build environment spec from explicit specs in history"
    );

    export_subcom->callback(
        [&]
        {
            auto& ctx = config.context();
            config.load();

            auto channel_context = mamba::ChannelContext::make_conda_compatible(ctx);

            auto json_format = config.at("json").get_cli_config<bool>();

            // Raise a warning if `--json` and `--explicit` are used together.
            if (json_format && explicit_format)
            {
                std::cerr << "Warning: `--json` and `--explicit` are used together but are incompatible. The `--json` flag will be ignored."
                          << std::endl;
            }

            if (explicit_format)
            {
                // TODO: handle error
                auto pd = mamba::PrefixData::create(ctx.prefix_params.target_prefix, channel_context)
                              .value();
                auto records = pd.sorted_records();
                std::cout << "# This file may be used to create an environment using:\n"
                          << "# $ conda create --name <env> --file <this file>\n"
                          << "# platform: " << ctx.platform << "\n"
                          << "@EXPLICIT\n";

                for (const auto& record : records)
                {
                    std::cout <<  //
                        mamba::specs::CondaURL::parse(record.package_url)
                            .transform(
                                [](mamba::specs::CondaURL&& url)
                                {
                                    return url.pretty_str(
                                        mamba::specs::CondaURL::StripScheme::no,
                                        0,  // don't strip any path characters
                                        mamba::specs::CondaURL::Credentials::Remove
                                    );
                                }
                            )
                            .or_else(
                                [&](const auto&) -> mamba::specs::expected_parse_t<std::string>
                                { return record.package_url; }
                            )
                            .value();

                    if (no_md5 != 1)
                    {
                        std::cout << "#" << record.md5;
                    }
                    std::cout << "\n";
                }
            }
            else if (json_format)
            {
                auto pd = mamba::PrefixData::create(ctx.prefix_params.target_prefix, channel_context)
                              .value();
                mamba::History& hist = pd.history();

                const auto& versions_map = pd.records();
                const auto& pip_versions_map = pd.pip_records();
                auto requested_specs_map = hist.get_requested_specs_map();
                std::stringstream dependencies;
                std::set<std::string> channels;

                bool first_dependency_printed = false;
                for (const auto& [k, v] : versions_map)
                {
                    if (from_history && requested_specs_map.find(k) == requested_specs_map.end())
                    {
                        continue;
                    }

                    dependencies << (first_dependency_printed ? ",\n" : "") << "    \"";
                    first_dependency_printed = true;

                    auto chans = channel_context.make_channel(v.channel);

                    if (from_history)
                    {
                        dependencies << requested_specs_map[k].to_string() << "\"";
                    }
                    else
                    {
                        if (channel_subdir)
                        {
                            dependencies
                                // If the size is not one, it's a custom multi channel
                                << ((chans.size() == 1) ? chans.front().display_name() : v.channel)
                                << "/" << v.platform << "::";
                        }
                        dependencies << v.name << "=" << v.version;
                        if (!no_build)
                        {
                            dependencies << "=" << v.build_string;
                        }
                        if (no_md5 == -1)
                        {
                            dependencies << "[md5=" << v.md5 << "]";
                        }
                        dependencies << "\"";
                    }

                    for (const auto& chan : chans)
                    {
                        channels.insert(chan.display_name());
                    }
                }

                // Add a `pip` subsection in `dependencies` listing wheels installed from PyPI
                if (!pip_versions_map.empty())
                {
                    dependencies << (first_dependency_printed ? ",\n" : "") << "     { \"pip\": [\n";
                    first_dependency_printed = false;
                    for (const auto& [k, v] : pip_versions_map)
                    {
                        dependencies << (first_dependency_printed ? ",\n" : "") << "      \""
                                     << v.name << "==" << v.version << "\"";
                        first_dependency_printed = true;
                    }
                    dependencies << "\n    ]\n    }";
                }

                dependencies << (first_dependency_printed ? "\n" : "");

                std::cout << "{\n";

                std::cout << "  \"channels\": [\n";
                for (auto channel_it = channels.begin(); channel_it != channels.end(); ++channel_it)
                {
                    auto last_channel = std::next(channel_it) == channels.end();
                    std::cout << "    \"" << *channel_it << "\"" << (last_channel ? "" : ",") << "\n";
                }
                std::cout << "  ],\n";

                std::cout << "  \"dependencies\": [\n" << dependencies.str() << "  ],\n";

                std::cout << "  \"name\": \""
                          << mamba::detail::get_env_name(ctx, ctx.prefix_params.target_prefix)
                          << "\",\n";
                std::cout << "  \"prefix\": " << ctx.prefix_params.target_prefix << "\n";

                std::cout << "}\n";

                std::cout.flush();
            }
            else
            {
                // Use the new modular API for YAML export
                auto pd = mamba::PrefixData::create(ctx.prefix_params.target_prefix, channel_context)
                              .value();

                mamba::detail::yaml_file_contents yaml_contents;

                // Handle --from-history: use requested specs from history
                if (from_history)
                {
                    mamba::History& hist = pd.history();
                    auto requested_specs_map = hist.get_requested_specs_map();
                    const auto& versions_map = pd.records();

                    yaml_contents.name = mamba::detail::get_env_name(
                        ctx,
                        ctx.prefix_params.target_prefix
                    );
                    yaml_contents.prefix = ctx.prefix_params.target_prefix.string();

                    // Build dependencies from requested specs
                    std::set<std::string> channels_set;
                    for (const auto& [k, v] : versions_map)
                    {
                        if (requested_specs_map.find(k) != requested_specs_map.end())
                        {
                            std::string dep = requested_specs_map[k].to_string();
                            if (no_md5 == -1 && !v.md5.empty())
                            {
                                dep += "[md5=" + v.md5 + "]";
                            }
                            yaml_contents.dependencies.push_back(std::move(dep));
                            auto chans = channel_context.make_channel(v.channel);
                            for (const auto& chan : chans)
                            {
                                channels_set.insert(chan.display_name());
                            }
                        }
                    }
                    yaml_contents.channels.assign(channels_set.begin(), channels_set.end());

                    // Handle pip packages
                    const auto& pip_versions_map = pd.pip_records();
                    if (!pip_versions_map.empty())
                    {
                        mamba::detail::other_pkg_mgr_spec pip_spec;
                        pip_spec.pkg_mgr = "pip";
                        pip_spec.cwd = ctx.prefix_params.target_prefix.string();
                        for (const auto& [name, pkg] : pip_versions_map)
                        {
                            pip_spec.deps.push_back(pkg.name + "==" + pkg.version);
                        }
                        yaml_contents.others_pkg_mgrs_specs.push_back(std::move(pip_spec));
                        if (std::find(
                                yaml_contents.dependencies.begin(),
                                yaml_contents.dependencies.end(),
                                "pip"
                            )
                            == yaml_contents.dependencies.end())
                        {
                            yaml_contents.dependencies.push_back("pip");
                        }
                    }

                    // Get environment variables - read directly from state file
                    // (reusing the logic from the new API)
                    const auto state_file_path = ctx.prefix_params.target_prefix / "conda-meta"
                                                 / "state";
                    if (mamba::fs::exists(state_file_path))
                    {
                        auto fin = mamba::open_ifstream(state_file_path);
                        try
                        {
                            nlohmann::json j;
                            fin >> j;
                            if (j.contains("env_vars") && j["env_vars"].is_object())
                            {
                                for (auto it = j["env_vars"].begin(); it != j["env_vars"].end(); ++it)
                                {
                                    // Convert UPPERCASE keys to lowercase
                                    std::string key = it.key();
                                    std::string lower_key = mamba::util::to_lower(key);
                                    yaml_contents.variables[lower_key] = it.value().get<std::string>();
                                }
                            }
                        }
                        catch (nlohmann::json::exception&)
                        {
                            // If parsing fails, leave variables empty
                        }
                    }
                }
                else
                {
                    // Use the new API for standard export
                    yaml_contents = mamba::prefix_to_yaml_contents(
                        pd,
                        ctx,
                        mamba::detail::get_env_name(ctx, ctx.prefix_params.target_prefix),
                        { no_build, false, no_md5 == -1 }
                    );

                    // Handle --channel-subdir: modify dependency strings to include platform
                    if (channel_subdir)
                    {
                        const auto& versions_map = pd.records();
                        std::vector<std::string> modified_deps;
                        modified_deps.reserve(yaml_contents.dependencies.size());
                        for (const auto& dep : yaml_contents.dependencies)
                        {
                            // Check if this dependency corresponds to a package we can look up
                            bool modified = false;
                            for (const auto& [k, v] : versions_map)
                            {
                                // Check if dependency string contains this package name
                                std::string dep_name = v.name + "=";
                                if (dep.find(dep_name) == 0
                                    || dep.find("::" + dep_name) != std::string::npos
                                    || dep == v.name)
                                {
                                    auto chans = channel_context.make_channel(v.channel);
                                    std::string new_dep;
                                    if (chans.size() == 1)
                                    {
                                        new_dep = chans.front().display_name() + "/" + v.platform
                                                  + "::";
                                    }
                                    else
                                    {
                                        new_dep = v.channel + "/" + v.platform + "::";
                                    }

                                    // Extract spec part (name=version[=build]) from original dep
                                    auto colon_pos = dep.find("::");
                                    std::string spec_part = (colon_pos != std::string::npos)
                                                                ? dep.substr(colon_pos + 2)
                                                                : dep;
                                    new_dep += spec_part;
                                    modified_deps.push_back(new_dep);
                                    modified = true;
                                    break;
                                }
                            }
                            if (!modified)
                            {
                                modified_deps.push_back(dep);
                            }
                        }
                        yaml_contents.dependencies = std::move(modified_deps);
                    }
                }

                // Write YAML directly to stdout
                // Note: prefix will be included if it was set (either from prefix_to_yaml_contents
                // or in the --from-history case), otherwise it will be omitted
                mamba::yaml_contents_to_stream(yaml_contents, std::cout);

                std::cout.flush();
            }
        }
    );

    // env remove subcommand
    auto* remove_subcom = com->add_subcommand("remove", "Remove an environment");
    init_general_options(remove_subcom, config);
    init_prefix_options(remove_subcom, config);

    remove_subcom->callback(
        [&config]
        {
            // Remove specs if exist
            mamba::RemoveResult remove_env_result = remove(config, mamba::MAMBA_REMOVE_ALL);

            if (remove_env_result == mamba::RemoveResult::NO)
            {
                mamba::Console::stream() << "The environment was not removed.";
                return;
            }

            if (remove_env_result == mamba::RemoveResult::EMPTY)
            {
                mamba::Console::stream() << "No packages to remove from environment.";

                auto res = mamba::Console::prompt("Do you want to remove the environment?", 'Y');
                if (!res)
                {
                    mamba::Console::stream() << "The environment was not removed.";
                    return;
                }
            }

            const auto& ctx = config.context();
            if (!ctx.dry_run)
            {
                const auto& prefix = ctx.prefix_params.target_prefix;
                // Remove env directory or rename it (e.g. if used)
                mamba::remove_or_rename(prefix, mamba::util::expand_home(prefix.string()));

                mamba::EnvironmentsManager env_manager{ ctx };
                // Unregister environment
                env_manager.unregister_env(mamba::util::expand_home(prefix.string()));

                mamba::Console::instance().print(
                    mamba::util::join(
                        "",
                        std::vector<std::string>({ "Environment removed at prefix: ", prefix.string() })
                    )
                );
                mamba::Console::instance().json_write({ { "success", true } });
            }
            else
            {
                mamba::Console::stream() << "Dry run. The environment was not removed.";
            }
        }
    );

    // env update subcommand
    auto* update_subcom = com->add_subcommand("update", "Update an environment");

    init_general_options(update_subcom, config);
    init_prefix_options(update_subcom, config);

    auto& file_specs = config.at("file_specs");
    update_subcom
        ->add_option(
            "-f,--file",
            file_specs.get_cli_config<std::vector<std::string>>(),
            file_specs.description()
        )
        ->option_text("FILE");

    static bool remove_not_specified = false;
    update_subcom->add_flag(
        "--prune",
        remove_not_specified,
        "Remove installed packages not specified in the command and in environment file"
    );

    update_subcom->callback(
        [&config]
        {
            auto update_params = mamba::UpdateParams{
                mamba::UpdateAll::No,
                mamba::PruneDeps::Yes,
                mamba::EnvUpdate::Yes,
                remove_not_specified ? mamba::RemoveNotSpecified::Yes : mamba::RemoveNotSpecified::No,
            };

            update(config, update_params);
        }
    );

    // env config subcommand
    auto* config_subcom = com->add_subcommand("config", "Configure environment");
    init_general_options(config_subcom, config);
    init_prefix_options(config_subcom, config);

    // env config vars subcommand
    auto* vars_subcom = config_subcom->add_subcommand("vars", "Manage environment variables");

    // env config vars list subcommand
    auto* vars_list_subcom = vars_subcom->add_subcommand("list", "List environment variables");
    init_general_options(vars_list_subcom, config);
    init_prefix_options(vars_list_subcom, config);
    vars_list_subcom->callback(
        [&config]
        {
            config.load();
            const auto& target_prefix = config.context().prefix_params.target_prefix;
            mamba::list_env_vars(target_prefix);
        }
    );

    // env config vars set subcommand
    auto* vars_set_subcom = vars_subcom->add_subcommand("set", "Set environment variable");
    init_general_options(vars_set_subcom, config);
    init_prefix_options(vars_set_subcom, config);
    static std::vector<std::string> vars_to_set;
    vars_set_subcom
        ->add_option("vars", vars_to_set, "Environment variables to set (format: KEY=VALUE)")
        ->required();
    vars_set_subcom->callback(
        [&config]
        {
            config.load();
            const auto& target_prefix = config.context().prefix_params.target_prefix;
            for (const auto& var_spec : vars_to_set)
            {
                auto eq_pos = var_spec.find('=');
                if (eq_pos == std::string::npos)
                {
                    throw std::runtime_error(
                        "Invalid format for environment variable. Expected KEY=VALUE, got: " + var_spec
                    );
                }
                std::string key = var_spec.substr(0, eq_pos);
                std::string value = var_spec.substr(eq_pos + 1);
                mamba::set_env_var(target_prefix, key, value);
                mamba::Console::instance().print("Set environment variable: " + key + "=" + value);
            }
            mamba::Console::instance().print(
                "To make your changes take effect please reactivate your environment"
            );
            vars_to_set.clear();
        }
    );

    // env config vars unset subcommand
    auto* vars_unset_subcom = vars_subcom->add_subcommand("unset", "Unset environment variable");
    init_general_options(vars_unset_subcom, config);
    init_prefix_options(vars_unset_subcom, config);
    static std::vector<std::string> vars_to_unset;
    vars_unset_subcom->add_option("KEY", vars_to_unset, "Environment variable names to unset")
        ->required();
    vars_unset_subcom->callback(
        [&config]
        {
            config.load();
            const auto& target_prefix = config.context().prefix_params.target_prefix;
            for (const auto& key : vars_to_unset)
            {
                mamba::unset_env_var(target_prefix, key);
                mamba::Console::instance().print("Unset environment variable: " + key);
            }
            mamba::Console::instance().print(
                "To make your changes take effect please reactivate your environment"
            );
            vars_to_unset.clear();
        }
    );
}
