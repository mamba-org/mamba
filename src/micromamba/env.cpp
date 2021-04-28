#include "common_options.hpp"
#include "mamba/core/environments_manager.hpp"

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
