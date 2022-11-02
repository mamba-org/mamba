#include "common_options.hpp"
#include "mamba/core/environments_manager.hpp"
#include "mamba/core/prefix_data.hpp"
#include "mamba/core/url.hpp"
#include "mamba/core/channel.hpp"

#include "mamba/api/configuration.hpp"
#include "mamba/api/remove.hpp"


using namespace mamba;  // NOLINT(build/namespaces)

std::string
get_env_name(const fs::u8path& px)
{
    const auto& ctx = Context::instance();
    auto& ed = ctx.envs_dirs[0];
    if (px == ctx.root_prefix)
    {
        return "base";
    }
    else if (mamba::starts_with(px.string(), ed.string()))
    {
        return fs::relative(px, ed).string();
    }
    else
    {
        return "";
    }
}


void
set_env_command(CLI::App* com)
{
    init_general_options(com);
    init_prefix_options(com);

    auto* list_subcom = com->add_subcommand("list", "List known environments");
    init_general_options(list_subcom);
    init_prefix_options(list_subcom);

    static bool explicit_format;
    static bool no_md5;

    static bool no_build = false;
    static bool from_history = false;

    auto* export_subcom = com->add_subcommand("export", "Export environment");
    init_general_options(export_subcom);
    init_prefix_options(export_subcom);
    export_subcom->add_flag("-e,--explicit", explicit_format, "Use explicit format");
    export_subcom->add_flag("--no-md5,!--md5", no_md5, "Disable md5");
    export_subcom->add_flag("--no-build,!--build", no_build, "Disable the build string in spec");
    export_subcom->add_flag(
        "--from-history", from_history, "Build environment spec from explicit specs in history");

    export_subcom->callback(
        []()
        {
            auto const& ctx = Context::instance();
            auto& config = Configuration::instance();
            config.at("show_banner").set_value(false);
            config.load();

            if (explicit_format)
            {
                // TODO: handle error
                auto pd = PrefixData::create(ctx.target_prefix).value();
                auto records = pd.sorted_records();
                std::cout << "# This file may be used to create an environment using:\n"
                          << "# $ conda create --name <env> --file <this file>\n"
                          << "# platform: " << Context::instance().platform << "\n"
                          << "@EXPLICIT\n";

                for (const auto& record : records)
                {
                    std::string clean_url, token;
                    split_anaconda_token(record.url, clean_url, token);
                    std::cout << clean_url;
                    if (!no_md5)
                    {
                        std::cout << "#" << record.md5;
                    }
                    std::cout << "\n";
                }
            }
            else
            {
                auto pd = PrefixData::create(ctx.target_prefix).value();
                History& hist = pd.history();

                const auto& versions_map = pd.records();

                std::cout << "name: " << get_env_name(ctx.target_prefix) << "\n";
                std::cout << "channels:\n";

                auto requested_specs_map = hist.get_requested_specs_map();
                std::stringstream dependencies;
                std::set<std::string> channels;
                for (const auto& [k, v] : versions_map)
                {
                    if (from_history && requested_specs_map.find(k) == requested_specs_map.end())
                        continue;

                    if (from_history)
                    {
                        dependencies << "- " << requested_specs_map[k].str() << "\n";
                    }
                    else
                    {
                        dependencies << "- " << v.name << "=" << v.version;
                        if (!no_build)
                            dependencies << "=" << v.build_string;
                        dependencies << "\n";
                    }

                    channels.insert(make_channel(v.url).base_url());
                }

                for (const auto& c : channels)
                    std::cout << "- " << c << "\n";
                std::cout << "dependencies:\n" << dependencies.str() << std::endl;
                std::cout.flush();
            }
        });

    list_subcom->callback(
        []()
        {
            const auto& ctx = Context::instance();
            auto& config = Configuration::instance();
            config.load();

            EnvironmentsManager env_manager;

            if (ctx.json)
            {
                nlohmann::json res;
                const auto pfxs = env_manager.list_all_known_prefixes();
                std::vector<std::string> envs(pfxs.size());
                std::transform(pfxs.begin(),
                               pfxs.end(),
                               envs.begin(),
                               [](const fs::u8path& path) { return path.string(); });
                res["envs"] = envs;
                std::cout << res.dump(4) << std::endl;
                return;
            }

            // format and print table
            printers::Table t({ "Name", "Active", "Path" });
            t.set_alignment({ printers::alignment::left,
                              printers::alignment::left,
                              printers::alignment::left });
            t.set_padding({ 2, 2, 2 });

            for (auto& env : env_manager.list_all_known_prefixes())
            {
                bool is_active = (env == ctx.target_prefix);
                t.add_row({ get_env_name(env), is_active ? "*" : "", env.string() });
            }
            t.print(std::cout);
        });

    auto* remove_subcom = com->add_subcommand("remove", "Remove an environment");
    init_general_options(remove_subcom);
    init_prefix_options(remove_subcom);

    remove_subcom->callback(
        []()
        {
            // Remove specs if exist
            remove(MAMBA_REMOVE_ALL);

            const auto& ctx = Context::instance();
            if (!ctx.dry_run)
            {
                const auto& prefix = ctx.target_prefix;
                // Remove env directory or rename it (e.g. if used)
                remove_or_rename(env::expand_user(prefix));

                EnvironmentsManager env_manager;
                // Unregister environment
                env_manager.unregister_env(env::expand_user(prefix));

                Console::instance().print(
                    join("",
                         std::vector<std::string>(
                             { "Environment removed at prefix: ", prefix.string() })));
                Console::instance().json_write({ { "success", true } });
            }
            else
            {
                Console::stream() << "Dry run. The environment was not removed.";
            }
        });
}
