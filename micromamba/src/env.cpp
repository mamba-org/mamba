// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <string>

#include "mamba/api/configuration.hpp"
#include "mamba/api/create.hpp"
#include "mamba/api/env.hpp"
#include "mamba/api/remove.hpp"
#include "mamba/api/update.hpp"
#include "mamba/core/channel_context.hpp"
#include "mamba/core/environments_manager.hpp"
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
                auto pd = mamba::PrefixData::create(ctx.prefix_params.target_prefix, channel_context)
                              .value();
                mamba::History& hist = pd.history();

                const auto& versions_map = pd.records();
                const auto& pip_versions_map = pd.pip_records();

                std::cout << "name: "
                          << mamba::detail::get_env_name(ctx, ctx.prefix_params.target_prefix)
                          << "\n";
                std::cout << "channels:\n";

                auto requested_specs_map = hist.get_requested_specs_map();
                std::stringstream dependencies;
                std::set<std::string> channels;

                for (const auto& [k, v] : versions_map)
                {
                    if (from_history && requested_specs_map.find(k) == requested_specs_map.end())
                    {
                        continue;
                    }

                    auto chans = channel_context.make_channel(v.channel);

                    if (from_history)
                    {
                        dependencies << "  - " << requested_specs_map[k].to_string() << "\n";
                    }
                    else
                    {
                        dependencies << "  - ";
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
                        dependencies << "\n";
                    }

                    for (const auto& chan : chans)
                    {
                        channels.insert(chan.display_name());
                    }
                }
                // Add a `pip` subsection in `dependencies` listing wheels installed from PyPI
                if (!pip_versions_map.empty())
                {
                    dependencies << "  - pip:\n";
                    for (const auto& [k, v] : pip_versions_map)
                    {
                        dependencies << "    - " << v.name << "==" << v.version << "\n";
                    }
                }

                for (const auto& c : channels)
                {
                    std::cout << "  - " << c << "\n";
                }
                std::cout << "dependencies:\n" << dependencies.str() << std::endl;

                std::cout << "prefix: " << ctx.prefix_params.target_prefix << std::endl;

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
}
