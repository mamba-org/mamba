#include "common_options.hpp"
#include "mamba/core/environments_manager.hpp"
#include "mamba/core/prefix_data.hpp"
#include "mamba/core/url.hpp"

#include "mamba/api/configuration.hpp"

using namespace mamba;  // NOLINT(build/namespaces)

std::string
get_env_name(const fs::path& px)
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

    auto* export_subcom = com->add_subcommand("export", "Export environment");
    init_general_options(export_subcom);
    init_prefix_options(export_subcom);
    export_subcom->add_flag("-e,--explicit", explicit_format, "Use explicit format");
    export_subcom->add_flag("--no-md5,!--md5", no_md5, "Disable md5");

    export_subcom->callback([]() {
        auto& ctx = Context::instance();
        auto& config = Configuration::instance();
        config.at("show_banner").set_value(false);
        config.load();

        if (explicit_format)
        {
            PrefixData pd(ctx.target_prefix);
            pd.load();
            std::cout << "# This file may be used to create an environment using:\n"
                      << "# $ conda create --name <env> --file <this file>\n"
                      << "# platform: " << Context::instance().platform << "\n"
                      << "@EXPLICIT\n";
            for (auto& [k, record] : pd.records())
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
            History hist(ctx.target_prefix);
            PrefixData pd(ctx.target_prefix);
            pd.load();
            auto versions_map = pd.records();
            auto m = hist.get_requested_specs_map();
            for (auto& [k, v] : m)
            {
                auto it = versions_map.find(k);
                if (it != versions_map.end())
                {
                    std::cout << "- " << it->second.name << "=" << it->second.version << "="
                              << it->second.build_string << "\n";
                }
            }
            std::cout.flush();
        }
    });

    list_subcom->callback([]() {
        auto& ctx = Context::instance();
        auto& config = Configuration::instance();
        config.load();

        EnvironmentsManager env_manager;

        if (ctx.json)
        {
            nlohmann::json res;
            auto pfxs = env_manager.list_all_known_prefixes();
            std::vector<std::string> envs(pfxs.begin(), pfxs.end());
            res["envs"] = envs;
            std::cout << res.dump(4) << std::endl;
            return;
        }

        // format and print table
        printers::Table t({ "Name", "Active", "Path" });
        t.set_alignment(
            { printers::alignment::left, printers::alignment::left, printers::alignment::left });
        t.set_padding({ 2, 2, 2 });

        for (auto& env : env_manager.list_all_known_prefixes())
        {
            bool is_active = (env == ctx.target_prefix);
            t.add_row({ get_env_name(env), is_active ? "*" : "", env.string() });
        }
        t.print(std::cout);
    });
}
