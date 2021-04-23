#include "mamba/core/environments_manager.hpp"

std::vector<std::string> first_level_completions = { "activate", "deactivate", "install",
                                                     "remove",   "update",     "list",
                                                     "clean",    "config",     "info" };

void
get_completions(int argc, char** argv)
{
    std::string line(argv[3]);
    std::vector<std::string> elems = mamba::split(line, " ");

    if (elems.size() == 2)
    {
        for (auto& el : first_level_completions)
        {
            std::cout << el << "\n";
        }
    }

    if (elems.size() >= 2 && elems[1] == "activate")
    {
        auto env_mgr = mamba::EnvironmentsManager();
        auto pfxs = env_mgr.list_all_known_prefixes();

        auto& ctx = mamba::Context::instance();
        auto& env_dirs = ctx.envs_dirs;

        std::vector<std::string> env_prefixes;
        for (auto& env_dir : env_dirs)
        {
            env_prefixes.push_back(env_dir.string());
        }

        for (auto& px : pfxs)
        {
            for (auto& ed : env_prefixes)
            {
                if (px == ctx.root_prefix)
                {
                    std::cout << "base\n";
                }
                else if (mamba::starts_with(px.string(), ed))
                {
                    std::cout << fs::relative(px, ed).string() << "\n";
                }
                else
                {
                    if (px.string().find("conda-bld") == std::string::npos)
                    {
                        std::cout << px.string() << "\n";
                    }
                }
            }
        }
    }
    exit(0);
}
