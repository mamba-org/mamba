// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <iostream>

#include "mamba/api/configuration.hpp"
#include "mamba/api/env.hpp"
#include "mamba/core/environments_manager.hpp"
#include "mamba/util/string.hpp"

namespace mamba
{
    void print_envs(Configuration& config)
    {
        config.load();
        mamba::detail::print_envs_impl(config);
    }

    namespace detail
    {
        std::string get_env_name(const Context& ctx, const mamba::fs::u8path& px)
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

        void print_envs_impl(const Configuration& config)
        {
            const auto& ctx = config.context();

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
    }
}
