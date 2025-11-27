// Copyright (c) 2025, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <iostream>
#include <map>

#include <nlohmann/json.hpp>

#include "mamba/api/configuration.hpp"
#include "mamba/api/env.hpp"
#include "mamba/core/environments_manager.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/util.hpp"
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
            if (px == ctx.prefix_params.root_prefix)
            {
                return "base";
            }
            // Check all envs_dirs to find which one contains this environment
            for (const auto& ed : ctx.envs_dirs)
            {
                if (util::starts_with(px.string(), ed.string()))
                {
                    return mamba::fs::relative(px, ed).string();
                }
            }
            return "";
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

    namespace
    {
        fs::u8path get_state_file_path(const fs::u8path& prefix)
        {
            return prefix / "conda-meta" / "state";
        }

        nlohmann::ordered_map<std::string, std::string>
        read_env_vars_from_state(const fs::u8path& state_file)
        {
            nlohmann::ordered_map<std::string, std::string> env_vars;
            if (fs::exists(state_file))
            {
                auto fin = open_ifstream(state_file);
                try
                {
                    nlohmann::ordered_json j;
                    fin >> j;
                    if (j.contains("env_vars") && j["env_vars"].is_object())
                    {
                        for (auto it = j["env_vars"].begin(); it != j["env_vars"].end(); ++it)
                        {
                            env_vars[it.key()] = it.value().get<std::string>();
                        }
                    }
                }
                catch (nlohmann::json::exception& error)
                {
                    LOG_WARNING << "Could not read JSON at " << state_file << ": " << error.what();
                }
            }
            return env_vars;
        }

        void write_env_vars_to_state(
            const fs::u8path& state_file,
            const nlohmann::ordered_map<std::string, std::string>& env_vars
        )
        {
            // Read existing state file to preserve other fields
            nlohmann::ordered_json j;
            if (fs::exists(state_file))
            {
                auto fin = open_ifstream(state_file);
                try
                {
                    fin >> j;
                }
                catch (nlohmann::json::exception&)
                {
                    // If parsing fails, start with empty JSON
                    j = nlohmann::ordered_json::object();
                }
            }

            // Update env_vars (preserves order)
            j["env_vars"] = env_vars;

            // Write back
            fs::create_directories(state_file.parent_path());
            std::ofstream out = open_ofstream(state_file);
            if (out.fail())
            {
                throw std::runtime_error("Couldn't open file for writing: " + state_file.string());
            }
            out << j.dump(4);
        }
    }

    void set_env_var(const fs::u8path& prefix, const std::string& key, const std::string& value)
    {
        fs::u8path state_file = get_state_file_path(prefix);
        auto env_vars = read_env_vars_from_state(state_file);
        std::string upper_key = util::to_upper(key);
        // Update or insert: if key exists, update in place; if not, add at end
        env_vars[upper_key] = value;
        write_env_vars_to_state(state_file, env_vars);
    }

    void unset_env_var(const fs::u8path& prefix, const std::string& key)
    {
        fs::u8path state_file = get_state_file_path(prefix);
        auto env_vars = read_env_vars_from_state(state_file);
        std::string upper_key = util::to_upper(key);
        if (env_vars.find(upper_key) != env_vars.end())
        {
            env_vars.erase(upper_key);
            write_env_vars_to_state(state_file, env_vars);
        }
    }

    void list_env_vars(const fs::u8path& prefix)
    {
        // Read directly from state file to preserve insertion order
        fs::u8path state_file = get_state_file_path(prefix);
        auto env_vars = read_env_vars_from_state(state_file);
        auto& console = Console::instance();

        if (console.context().output_params.json)
        {
            nlohmann::ordered_json j;
            j["env_vars"] = env_vars;
            std::cout << j.dump(4) << std::endl;
            return;
        }

        if (env_vars.empty())
        {
            console.print("No environment variables set.");
            return;
        }

        // Output in conda format: "KEY = VALUE" (preserving insertion order)
        for (const auto& [key, value] : env_vars)
        {
            std::cout << key << " = " << value << std::endl;
        }
    }
}
