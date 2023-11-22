// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <string>

#include <solv/solver.h>

#include "mamba/api/configuration.hpp"
#include "mamba/api/create.hpp"
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


using namespace mamba;  // NOLINT(build/namespaces)

std::string
get_env_name(const Context& ctx, const mamba::fs::u8path& px)
{
    auto& ed = ctx.envs_dirs[0];
    if (px == ctx.prefix_params.root_prefix)
    {
        return "base";
    }
    else if (util::starts_with(px.string(), ed.string()))
    {
        return mamba::fs::relative(px, ed).string();
    }
    else
    {
        return "";
    }
}


void
set_env_command(CLI::App* com, Configuration& config)
{
    init_general_options(com, config);
    init_prefix_options(com, config);

    // env list subcommand
    auto* list_subcom = com->add_subcommand("list", "List known environments");
    init_general_options(list_subcom, config);
    init_prefix_options(list_subcom, config);

    list_subcom->callback(
        [&config]
        {
            const auto& ctx = config.context();
            config.load();

            EnvironmentsManager env_manager{ ctx };

            if (ctx.output_params.json)
            {
                nlohmann::json res;
                const auto pfxs = env_manager.list_all_known_prefixes();
                std::vector<std::string> envs(pfxs.size());
                std::transform(
                    pfxs.begin(),
                    pfxs.end(),
                    envs.begin(),
                    [](const mamba::fs::u8path& path) { return path.string(); }
                );
                res["envs"] = envs;
                std::cout << res.dump(4) << std::endl;
                return;
            }

            // format and print table
            printers::Table t({ "Name", "Active", "Path" });
            t.set_alignment(
                { printers::alignment::left, printers::alignment::left, printers::alignment::left }
            );
            t.set_padding({ 2, 2, 2 });

            for (auto& env : env_manager.list_all_known_prefixes())
            {
                bool is_active = (env == ctx.prefix_params.target_prefix);
                t.add_row({ get_env_name(ctx, env), is_active ? "*" : "", env.string() });
            }
            t.print(std::cout);
        }
    );

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
    export_subcom->add_flag("--no-build,!--build", no_build, "Disable the build string in spec");
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

            mamba::ChannelContext channel_context{ ctx };
            if (explicit_format)
            {
                // TODO: handle error
                auto pd = PrefixData::create(ctx.prefix_params.target_prefix, channel_context).value();
                auto records = pd.sorted_records();
                std::cout << "# This file may be used to create an environment using:\n"
                          << "# $ conda create --name <env> --file <this file>\n"
                          << "# platform: " << ctx.platform << "\n"
                          << "@EXPLICIT\n";

                for (const auto& record : records)
                {
                    using Credentials = typename specs::CondaURL::Credentials;
                    std::cout << specs::CondaURL::parse(record.url).str(Credentials::Hide);
                    if (no_md5 != 1)
                    {
                        std::cout << "#" << record.md5;
                    }
                    std::cout << "\n";
                }
            }
            else
            {
                auto pd = PrefixData::create(ctx.prefix_params.target_prefix, channel_context).value();
                History& hist = pd.history();

                const auto& versions_map = pd.records();

                std::cout << "name: " << get_env_name(ctx, ctx.prefix_params.target_prefix) << "\n";
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
                        dependencies << "- " << requested_specs_map[k].str() << "\n";
                    }
                    else
                    {
                        dependencies << "- ";
                        if (channel_subdir)
                        {
                            dependencies
                                // If the size is not one, it's a custom mutli channel
                                << ((chans.size() == 1) ? chans.front().display_name() : v.channel)
                                << "/" << v.subdir << "::";
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

                    for (auto const& chan : chans)
                    {
                        channels.insert(chan.display_name());
                    }
                }

                for (const auto& c : channels)
                {
                    std::cout << "- " << c << "\n";
                }
                std::cout << "dependencies:\n" << dependencies.str() << std::endl;
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
            remove(config, MAMBA_REMOVE_ALL);

            const auto& ctx = config.context();
            if (!ctx.dry_run)
            {
                const auto& prefix = ctx.prefix_params.target_prefix;
                // Remove env directory or rename it (e.g. if used)
                remove_or_rename(ctx, util::expand_home(prefix.string()));

                EnvironmentsManager env_manager{ ctx };
                // Unregister environment
                env_manager.unregister_env(util::expand_home(prefix.string()));

                Console::instance().print(util::join(
                    "",
                    std::vector<std::string>({ "Environment removed at prefix: ", prefix.string() })
                ));
                Console::instance().json_write({ { "success", true } });
            }
            else
            {
                Console::stream() << "Dry run. The environment was not removed.";
            }
        }
    );

    // env update subcommand
    auto* update_subcom = com->add_subcommand("update", "Update an environment");

    init_general_options(update_subcom, config);
    init_prefix_options(update_subcom, config);

    auto& file_specs = config.at("file_specs");
    update_subcom->add_option(
        "-f,--file",
        file_specs.get_cli_config<std::vector<std::string>>(),
        file_specs.description()
    );

    static bool remove_not_specified = false;
    update_subcom->add_flag(
        "--prune",
        remove_not_specified,
        "Remove installed packages not specified in the command and in environment file"
    );

    update_subcom->callback(
        [&config]
        { update(config, /*update_all*/ false, /*prune_deps*/ false, remove_not_specified); }
    );
}
