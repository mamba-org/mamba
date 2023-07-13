// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/core/activation.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/environment.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/shell_init.hpp"
#include "mamba/core/util.hpp"
#include "mamba/util/build.hpp"
#include "mamba/util/string.hpp"

namespace mamba
{
    namespace
    {
        fs::u8path PREFIX_STATE_FILE = fs::u8path("conda-meta") / "state";
        fs::u8path PACKAGE_ENV_VARS_DIR = fs::u8path("etc") / "conda" / "env_vars.d";
        std::string CONDA_ENV_VARS_UNSET_VAR = "***unset***";  // NOLINT(runtime/string)
    }                                                          // namespace

    /****************************
     * Activator implementation *
     ****************************/

    Activator::Activator(const Context& context)
        : m_context(context)
        , m_env(env::copy())
    {
    }

    std::vector<fs::u8path> Activator::get_activate_scripts(const fs::u8path& prefix)
    {
        fs::u8path script_dir = prefix / "etc" / "conda" / "activate.d";
        std::vector<fs::u8path> result = filter_dir(script_dir, shell_extension());
        std::sort(result.begin(), result.end());
        return result;
    }

    std::vector<fs::u8path> Activator::get_deactivate_scripts(const fs::u8path& prefix)
    {
        fs::u8path script_dir = prefix / "etc" / "conda" / "deactivate.d";
        std::vector<fs::u8path> result = filter_dir(script_dir, shell_extension());
        // reverse sort!
        std::sort(result.begin(), result.end(), std::greater<fs::u8path>());
        return result;
    }

    std::string Activator::get_default_env(const fs::u8path& prefix)
    {
        if (paths_equal(prefix, m_context.prefix_params.root_prefix))
        {
            return "base";
        }
        // if ../miniconda3/envs/my_super_env, return `my_super_env`, else path
        if (prefix.parent_path().stem() == "envs")
        {
            return prefix.stem().string();
        }
        else
        {
            return prefix.string();
        }
    }

    std::vector<std::pair<std::string, std::string>>
    Activator::get_environment_vars(const fs::u8path& prefix)
    {
        fs::u8path env_vars_file = prefix / PREFIX_STATE_FILE;
        fs::u8path pkg_env_var_dir = prefix / PACKAGE_ENV_VARS_DIR;

        nlohmann::ordered_map<std::string, std::string> env_vars;

        // # First get env vars from packages
        auto env_var_files = filter_dir(pkg_env_var_dir, "");
        std::sort(env_var_files.begin(), env_var_files.end());
        for (auto& f : env_var_files)
        {
            auto fin = open_ifstream(f);
            nlohmann::ordered_json j;
            try
            {
                fin >> j;
                for (auto it = j.begin(); it != j.end(); ++it)
                {
                    env_vars[util::to_upper(it.key())] = it.value();
                }
            }
            catch (nlohmann::json::exception& error)
            {
                LOG_WARNING << "Could not read JSON at " << f << ": " << error.what();
            }
        }

        // Then get env vars from environment specification
        if (fs::exists(env_vars_file))
        {
            auto fin = open_ifstream(env_vars_file);
            try
            {
                nlohmann::ordered_json j;
                fin >> j;
                if (j.contains("env_vars"))
                {
                    auto& prefix_state_env_vars = j["env_vars"];
                    for (auto it = prefix_state_env_vars.begin(); it != prefix_state_env_vars.end();
                         ++it)
                    {
                        if (env_vars.find(it.key()) != env_vars.end())
                        {
                            LOG_WARNING << "Duplicate env vars detected. Vars from the environment "
                                        << "will overwrite those from packages";
                            LOG_WARNING << "Variable " << it.key() << " duplicated";
                        }
                        env_vars[util::to_upper(it.key())] = it.value();
                    }
                }
            }
            catch (nlohmann::json::exception& error)
            {
                LOG_WARNING << "Could not read JSON at " << env_vars_file << ": " << error.what();
            }
        }
        std::vector<std::pair<std::string, std::string>> res(env_vars.begin(), env_vars.end());
        return res;
    }

    std::string Activator::get_prompt_modifier(
        const fs::u8path& prefix,
        const std::string& conda_default_env,
        int old_conda_shlvl
    )
    {
        if (m_context.change_ps1)
        {
            std::vector<std::string> env_stack;
            std::vector<std::string> prompt_stack;
            std::string env_i;
            for (int i = 1; i < old_conda_shlvl + 1; ++i)
            {
                const std::string env_prefix = (i == old_conda_shlvl)
                                                   ? "CONDA_PREFIX"
                                                   : "CONDA_PREFIX_" + std::to_string(i);
                const std::string default_prefix = (m_env.find(env_prefix) != m_env.end())
                                                       ? m_env[env_prefix]
                                                       : "";
                env_i = get_default_env(default_prefix);

                bool stacked_i = m_env.find("CONDA_STACKED_" + std::to_string(i)) != m_env.end();
                env_stack.push_back(env_i);

                if (!stacked_i && prompt_stack.size() != 0)
                {
                    prompt_stack.pop_back();
                }
                prompt_stack.push_back(env_i);
            }

            // Modify prompt stack according to pending operation
            if (m_action == ActivationType::DEACTIVATE)
            {
                if (prompt_stack.size())
                {
                    prompt_stack.pop_back();
                }
                if (env_stack.size())
                {
                    env_stack.pop_back();
                }
                bool stacked = m_env.find("CONDA_STACKED_" + std::to_string(old_conda_shlvl))
                               != m_env.end();
                if (!stacked && env_stack.size())
                {
                    prompt_stack.push_back(env_stack.back());
                }
            }
            else if (m_action == ActivationType::REACTIVATE)
            {
                // DO NOTHING
            }
            else
            {
                // stack = getattr(self, 'stack', False)
                if (!m_stack && prompt_stack.size())
                {
                    prompt_stack.pop_back();
                }
                prompt_stack.push_back(conda_default_env);
                // TODO there may be more missing here.
            }

            auto conda_stacked_env = util::join(";", prompt_stack);

            std::string prompt = m_context.env_prompt;
            util::replace_all(prompt, "{default_env}", conda_default_env);
            util::replace_all(prompt, "{stacked_env}", conda_stacked_env);
            util::replace_all(prompt, "{prefix}", prefix.string());
            util::replace_all(prompt, "{name}", prefix.stem().string());
            return prompt;
        }
        else
        {
            return "";
        }
    }

    std::vector<fs::u8path> get_path_dirs(const fs::u8path& prefix)
    {
        if (util::on_win)
        {
            return { prefix,
                     prefix / "Library" / "mingw-w64" / "bin",
                     prefix / "Library" / "usr" / "bin",
                     prefix / "Library" / "bin",
                     prefix / "Scripts",
                     prefix / "bin" };
        }
        else
        {
            return { prefix / "bin" };
        }
    }

    std::vector<fs::u8path> Activator::get_PATH()
    {
        std::vector<fs::u8path> path;
        if (m_env.find("PATH") != m_env.end())
        {
            auto strings = util::split(m_env["PATH"], env::pathsep());
            for (auto& s : strings)
            {
                path.push_back(s);
            }
        }
        return path;
    }

    std::string Activator::add_prefix_to_path(const fs::u8path& prefix, int old_conda_shlvl)
    {
        // prefix = self.path_conversion(prefix)
        // path_list = list(self.path_conversion(self._get_starting_path_list()))
        auto path_list = get_PATH();
        // If this is the first time we're activating an environment, we need to
        // ensure that the condabin directory is included in the path list. Under
        // normal conditions, if the shell hook is working correctly, this should
        // never trigger.
        if (old_conda_shlvl == 0)
        {
            bool no_condabin = std::none_of(
                path_list.begin(),
                path_list.end(),
                [](const fs::u8path& s) { return util::ends_with(s.string(), "condabin"); }
            );
            if (no_condabin)
            {
                auto condabin_dir = m_context.prefix_params.root_prefix / "condabin";
                path_list.insert(path_list.begin(), condabin_dir);
            }
        }

        // TODO check if path_conversion does something useful here.
        // path_list[0:0] = list(self.path_conversion(self._get_path_dirs(prefix)))
        std::vector<fs::u8path> final_path = get_path_dirs(prefix);
        final_path.insert(final_path.end(), path_list.begin(), path_list.end());
        final_path.erase(std::unique(final_path.begin(), final_path.end()), final_path.end());

        std::string result = util::join(env::pathsep(), final_path).string();
        return result;
    }

    std::string
    Activator::replace_prefix_in_path(const fs::u8path& old_prefix, const fs::u8path& new_prefix)
    {
        // TODO not done yet.
        std::vector<fs::u8path> current_path = get_PATH();
        assert(!old_prefix.empty());

        std::vector<fs::u8path> old_prefix_dirs = get_path_dirs(old_prefix);

        // remove all old paths
        std::vector<fs::u8path> cleaned_path;
        for (auto& cp : current_path)
        {
            bool is_in = false;
            for (auto& op : old_prefix_dirs)
            {
                if (paths_equal(cp, op))
                {
                    is_in = true;
                    break;
                }
            }
            if (!is_in)
            {
                cleaned_path.push_back(cp);
            }
        }
        current_path = cleaned_path;

        // TODO remove `sys.prefix\Library\bin` on Windows?!
        // Not sure if necessary as we don't fiddle with Python
        std::vector<fs::u8path> final_path;
        if (!new_prefix.empty())
        {
            final_path = get_path_dirs(new_prefix);
            final_path.insert(final_path.end(), current_path.begin(), current_path.end());

            // remove duplicates
            final_path.erase(std::unique(final_path.begin(), final_path.end()), final_path.end());
            std::string result = util::join(env::pathsep(), final_path).string();
            return result;
        }
        else
        {
            current_path.erase(
                std::unique(current_path.begin(), current_path.end()),
                current_path.end()
            );
            std::string result = util::join(env::pathsep(), current_path).string();
            return result;
        }
    }

    std::string Activator::remove_prefix_from_path(const fs::u8path& prefix)
    {
        return replace_prefix_in_path(prefix, fs::u8path());
    }

    void Activator::get_export_unset_vars(
        EnvironmentTransform& envt,
        const std::vector<std::pair<std::string, std::string>>& to_export
    )
    {
        // conda_exe_vars_export = OrderedDict()
        // for k, v in context.conda_exe_vars_dict.items():
        //     if v is None or conda_exe_vars_None:
        //         conda_exe_unset_vars.append(k)
        //     else:
        //         conda_exe_vars_export[k] = self.path_conversion(v) if v else v

        for (auto& [k, v] : to_export)
        {
            if (v == "")
            {
                envt.unset_vars.push_back(util::to_upper(k));
            }
            else
            {
                envt.export_vars.push_back({ util::to_upper(k), v });
            }
        }
    }
    EnvironmentTransform Activator::build_reactivate()
    {
        std::string conda_prefix;
        int conda_shlvl = 0;
        if (m_env.find("CONDA_SHLVL") != m_env.end())
        {
            std::string env_shlvl(util::strip(m_env["CONDA_SHLVL"]));
            conda_shlvl = std::stoi(env_shlvl);
        }

        if (m_env.find("CONDA_PREFIX") != m_env.end())
        {
            conda_prefix = m_env["CONDA_PREFIX"];
        }

        EnvironmentTransform envt;
        if (conda_prefix.empty() || conda_shlvl < 1)
        {
            return envt;
        }

        std::string conda_default_env = (m_env.find("CONDA_DEFAULT_ENV") != m_env.end())
                                            ? m_env["CONDA_DEFAULT_ENV"]
                                            : get_default_env(conda_prefix);

        auto new_path = replace_prefix_in_path(conda_prefix, conda_prefix);

        std::string conda_prompt_modifier = get_prompt_modifier(
            conda_prefix,
            conda_default_env,
            conda_shlvl
        );
        if (m_context.change_ps1)
        {
            auto res = update_prompt(conda_prompt_modifier);
            if (!res.first.empty())
            {
                envt.set_vars.push_back(res);
            }
        }

        std::vector<std::pair<std::string, std::string>> env_vars_to_export = {
            { "path", new_path },
            { "conda_shlvl", std::to_string(conda_shlvl) },
            { "conda_prompt_modifier", conda_prompt_modifier }
        };
        get_export_unset_vars(envt, env_vars_to_export);

        // TODO figure out if this is all really necessary?
        // # environment variables are set only to aid transition from conda 4.3 to
        // conda 4.4 conda_environment_env_vars =
        // self._get_environment_env_vars(conda_prefix) for k, v in
        // conda_environment_env_vars.items():
        //     if v == CONDA_ENV_VARS_UNSET_VAR:
        //         env_vars_to_unset = env_vars_to_unset + (k,)
        //     else:
        //         env_vars_to_export[k] = v

        envt.deactivate_scripts = get_deactivate_scripts(conda_prefix);
        envt.activate_scripts = get_activate_scripts(conda_prefix);

        return envt;
    }

    EnvironmentTransform Activator::build_deactivate()
    {
        EnvironmentTransform envt;

        if (m_env.find("CONDA_PREFIX") == m_env.end() || m_env.find("CONDA_SHLVL") == m_env.end())
        {
            // nothing to deactivate
            return envt;
        }
        std::string old_conda_prefix = m_env["CONDA_PREFIX"];
        int old_conda_shlvl = std::stoi(m_env["CONDA_SHLVL"]);
        envt.deactivate_scripts = get_deactivate_scripts(old_conda_prefix);
        auto old_conda_environment_env_vars = get_environment_vars(old_conda_prefix);
        int new_conda_shlvl = old_conda_shlvl - 1;

        std::string conda_prompt_modifier = "";
        if (old_conda_shlvl == 1)
        {
            std::string new_path = remove_prefix_from_path(old_conda_prefix);
            // You might think that you can remove the CONDA_EXE vars by passing
            // conda_exe_vars=None here so that "deactivate means deactivate" but you
            // cannot since the conda shell scripts still refer to them and they only
            // set them once at the top. We could change that though, the conda() shell
            // function could set them instead of doing it at the top.  This would be
            // *much* cleaner. I personally cannot abide that I have deactivated conda
            // and anything at all in my env still references it (apart from the shell
            // script, we need something I suppose!)
            envt.export_path = new_path;
            std::vector<std::pair<std::string, std::string>> env_vars_to_export = {
                { "conda_prefix", "" },
                { "conda_shlvl", std::to_string(new_conda_shlvl) },
                { "conda_default_env", "" },
                { "conda_prompt_modifier", "" }
            };
            get_export_unset_vars(envt, env_vars_to_export);
        }
        else
        {
            assert(old_conda_shlvl > 1);
            std::string new_prefix = m_env.at("CONDA_PREFIX_" + std::to_string(new_conda_shlvl));
            std::string conda_default_env = get_default_env(new_prefix);
            conda_prompt_modifier = get_prompt_modifier(new_prefix, conda_default_env, old_conda_shlvl);
            auto new_conda_environment_env_vars = get_environment_vars(new_prefix);

            bool old_prefix_stacked
                = (m_env.find("CONDA_STACKED_" + std::to_string(old_conda_shlvl)) != m_env.end());
            std::string new_path;
            envt.unset_vars.push_back("CONDA_PREFIX_" + std::to_string(new_conda_shlvl));

            if (old_prefix_stacked)
            {
                new_path = remove_prefix_from_path(old_conda_prefix);
                envt.unset_vars.push_back("CONDA_STACKED_" + std::to_string(old_conda_shlvl));
            }
            else
            {
                new_path = replace_prefix_in_path(old_conda_prefix, new_prefix);
            }

            std::vector<std::pair<std::string, std::string>> env_vars_to_export = {
                { "conda_prefix", new_prefix },
                { "conda_shlvl", std::to_string(new_conda_shlvl) },
                { "conda_default_env", conda_default_env },
                { "conda_prompt_modifier", conda_prompt_modifier }
            };

            get_export_unset_vars(envt, env_vars_to_export);

            for (auto& [k, v] : new_conda_environment_env_vars)
            {
                envt.export_vars.push_back({ k, v });
            }

            envt.export_path = new_path;
            envt.activate_scripts = get_activate_scripts(new_prefix);
        }

        if (m_context.change_ps1)
        {
            auto res = update_prompt(conda_prompt_modifier);
            if (!res.first.empty())
            {
                envt.set_vars.push_back(res);
            }
        }

        for (auto& env_var : old_conda_environment_env_vars)
        {
            envt.unset_vars.push_back(env_var.first);
            std::string save_var = fmt::format("__CONDA_SHLVL_{}_{}", new_conda_shlvl, env_var.first);
            if (m_env.find(save_var) != m_env.end())
            {
                envt.export_vars.push_back({ env_var.first, m_env[save_var] });
                envt.unset_vars.push_back(save_var);
            }
        }

        // TODO if shlvl == 0 unset conda_prefix?!
        return envt;
    }

    EnvironmentTransform Activator::build_activate(const fs::u8path& prefix)
    {
        EnvironmentTransform envt;

        // query environment
        std::string old_conda_prefix;
        int old_conda_shlvl = 0, new_conda_shlvl;
        if (m_env.find("CONDA_SHLVL") != m_env.end())
        {
            std::string env_shlvl(util::strip(m_env["CONDA_SHLVL"]));
            old_conda_shlvl = std::stoi(env_shlvl);
        }
        if (m_env.find("CONDA_PREFIX") != m_env.end())
        {
            old_conda_prefix = m_env["CONDA_PREFIX"];
        }

        new_conda_shlvl = old_conda_shlvl + 1;

        // if the prior active prefix is this prefix we are actually doing a reactivate
        if (old_conda_prefix == prefix && old_conda_shlvl > 0)
        {
            return build_reactivate();
        }

        if (old_conda_shlvl
            && m_env.find("CONDA_PREFIX_" + std::to_string(old_conda_shlvl - 1)) != m_env.end()
            && m_env["CONDA_PREFIX_" + std::to_string(old_conda_shlvl - 1)] == prefix)
        {
            // in this case, user is attempting to activate the previous environment,
            // i.e. step back down
            return build_deactivate();
        }

        envt.activate_scripts = get_activate_scripts(prefix);
        std::string conda_default_env = get_default_env(prefix);
        std::string conda_prompt_modifier = get_prompt_modifier(
            prefix,
            conda_default_env,
            old_conda_shlvl
        );

        auto conda_environment_env_vars = get_environment_vars(prefix);

        // TODO check with conda if that's really what's supposed to happen ...
        std::remove_if(
            conda_environment_env_vars.begin(),
            conda_environment_env_vars.end(),
            [](auto& el) { return el.second == CONDA_ENV_VARS_UNSET_VAR; }
        );

        std::vector<std::string> clobbering_env_vars;
        for (auto& env_var : conda_environment_env_vars)
        {
            if (m_env.find(env_var.first) != m_env.end())
            {
                clobbering_env_vars.push_back(env_var.first);
            }
        }

        for (const auto& v : clobbering_env_vars)
        {
            conda_environment_env_vars.push_back(
                { fmt::format("__CONDA_SHLVL_{}_{}", old_conda_shlvl, v), m_env[v] }
            );
        }

        if (clobbering_env_vars.size())
        {
            LOG_WARNING << "WARNING: overwriting environment variables set in the machine";
            LOG_WARNING << "Overwriting variables: " << util::join(",", clobbering_env_vars);
        }

        std::string new_path = add_prefix_to_path(prefix, old_conda_shlvl);

        std::vector<std::pair<std::string, std::string>> env_vars_to_export{
            { "path", new_path },
            { "conda_prefix", prefix.string() },
            { "conda_shlvl", std::to_string(new_conda_shlvl) },
            { "conda_default_env", conda_default_env },
            { "conda_prompt_modifier", conda_prompt_modifier }
        };

        for (auto& [k, v] : conda_environment_env_vars)
        {
            envt.export_vars.push_back({ k, v });
        }

        if (old_conda_shlvl == 0)
        {
            get_export_unset_vars(envt, env_vars_to_export);
        }
        else if (m_stack)
        {
            get_export_unset_vars(envt, env_vars_to_export);
            envt.export_vars.push_back({ fmt::format("CONDA_PREFIX_{}", old_conda_shlvl),
                                         old_conda_prefix });
            envt.export_vars.push_back({ fmt::format("CONDA_STACKED_{}", new_conda_shlvl), "true" });
        }
        else
        {
            new_path = replace_prefix_in_path(old_conda_prefix, prefix.string());
            envt.deactivate_scripts = get_deactivate_scripts(old_conda_prefix);
            auto old_conda_environment_env_vars = get_environment_vars(old_conda_prefix);

            for (auto& env_var : old_conda_environment_env_vars)
            {
                envt.unset_vars.push_back(env_var.first);
                // restore saved variable
                std::string save_var = std::string("__CONDA_SHLVL_")
                                       + std::to_string(old_conda_shlvl - 1) + "_"
                                       + env_var.first;  // % (new_conda_shlvl, env_var)
                if (m_env.find(save_var) != m_env.end())
                {
                    envt.export_vars.insert(
                        envt.export_vars.begin(),
                        { env_var.first, m_env[save_var] }
                    );
                }
            }

            env_vars_to_export[0] = { "PATH", new_path };
            get_export_unset_vars(envt, env_vars_to_export);
            envt.export_vars.push_back({ fmt::format("CONDA_PREFIX_{}", old_conda_shlvl),
                                         old_conda_prefix });
        }

        if (m_context.change_ps1)
        {
            auto res = update_prompt(conda_prompt_modifier);
            if (!res.first.empty())
            {
                envt.set_vars.push_back(res);
            }
        }

        return envt;
    }

    std::string Activator::activate(const fs::u8path& prefix, bool stack)
    {
        m_stack = stack;
        m_action = ActivationType::ACTIVATE;
        return script(build_activate(prefix));
    }

    std::string Activator::reactivate()
    {
        m_action = ActivationType::REACTIVATE;
        return script(build_reactivate());
    }

    std::string Activator::deactivate()
    {
        m_action = ActivationType::DEACTIVATE;
        return script(build_deactivate());
    }

    std::string Activator::hook(const std::string& shell_type)
    {
        const auto is_cmd = [](const auto* ptr)
        { return dynamic_cast<const CmdExeActivator*>(ptr) != nullptr; };
        const auto is_powershell = [](const auto* ptr)
        { return dynamic_cast<const PowerShellActivator*>(ptr) != nullptr; };

        // special handling for cmd.exe
        if (is_cmd(this))
        {
            get_hook_contents(shell());
            return "";
        }

        std::stringstream builder;
        if (is_powershell(this) && fs::exists(hook_source_path()))
        {
            builder << hook_preamble() << "\n" << read_contents(hook_source_path()) << "\n";
        }
        else
        {
            builder << hook_preamble() << "\n" << get_hook_contents(shell()) << "\n";
        }

        if (m_context.shell_completion)
        {
            if (shell() == "posix" && (shell_type == "zsh" || shell_type == "bash"))
            {
                builder << data_mamba_completion_posix;
            }
        }

        // if we are in a `mamba shell -n <env>` we don't want to activate base
        auto has_prefix = env::get("CONDA_PREFIX");
        if (m_context.auto_activate_base && !has_prefix.has_value())
        {
            builder << "micromamba activate base\n";
        }
        builder << hook_postamble() << "\n";
        return builder.str();
    }

    /*********************************
     * PosixActivator implementation *
     *********************************/

    std::string PosixActivator::script(const EnvironmentTransform& env_transform)
    {
        std::stringstream out;
        if (!env_transform.export_path.empty())
        {
            if (util::on_win)
            {
                out << "export PATH='"
                    << native_path_to_unix(env_transform.export_path, /*is_a_env_path=*/true)
                    << "'\n";
            }
            else
            {
                out << "export PATH='" << env_transform.export_path << "'\n";
            }
        }

        for (const fs::u8path& ds : env_transform.deactivate_scripts)
        {
            out << ". " << ds << "\n";
        }

        for (const std::string& uvar : env_transform.unset_vars)
        {
            out << "unset " << uvar << "\n";
        }

        for (const auto& [skey, svar] : env_transform.set_vars)
        {
            out << skey << "='" << svar << "'\n";
        }

        for (const auto& [ekey, evar] : env_transform.export_vars)
        {
            if (util::on_win && ekey == "PATH")
            {
                out << "export " << ekey << "='"
                    << native_path_to_unix(evar, /*is_a_env_path=*/true) << "'\n";
            }
            else
            {
                out << "export " << ekey << "='" << evar << "'\n";
            }
        }

        for (const fs::u8path& p : env_transform.activate_scripts)
        {
            out << ". " << p << "\n";
        }

        return out.str();
    }

    std::pair<std::string, std::string>
    PosixActivator::update_prompt(const std::string& conda_prompt_modifier)
    {
        std::string ps1 = (m_env.find("PS1") != m_env.end()) ? m_env["PS1"] : "";
        if (ps1.find("POWERLINE_COMMAND") != ps1.npos)
        {
            // Defer to powerline (https://github.com/powerline/powerline) if it's in
            // use.
            return { "", "" };
        }
        auto current_prompt_modifier = env::get("CONDA_PROMPT_MODIFIER");
        if (current_prompt_modifier)
        {
            util::replace_all(ps1, current_prompt_modifier.value(), "");
        }
        // Because we're using single-quotes to set shell variables, we need to handle
        // the proper escaping of single quotes that are already part of the string.
        // Best solution appears to be https://stackoverflow.com/a/1250279
        util::replace_all(ps1, "'", "'\"'\"'");
        return { "PS1", conda_prompt_modifier + ps1 };
    }

    std::string PosixActivator::shell_extension()
    {
        return ".sh";
    }

    std::string PosixActivator::shell()
    {
        return "posix";
    }

    std::string PosixActivator::hook_preamble()
    {
        // result = ''
        // for key, value in context.conda_exe_vars_dict.items():
        //     if value is None:
        //         # Using `unset_var_tmpl` would cause issues for people running
        //         # with shell flag -u set (error on unset).
        //         # result += join(self.unset_var_tmpl % key) + '\n'
        //         result += join(self.export_var_tmpl % (key, '')) + '\n'
        //     else:
        //         if key in ('PYTHONPATH', 'CONDA_EXE'):
        //             result += join(self.export_var_tmpl % (
        //                 key, self.path_conversion(value))) + '\n'
        //         else:
        //             result += join(self.export_var_tmpl % (key, value)) + '\n'
        // return result
        std::string preamble;
        return preamble;
    }

    std::string PosixActivator::hook_postamble()
    {
        return "";
    }

    fs::u8path PosixActivator::hook_source_path()
    {
        return m_context.prefix_params.root_prefix / "etc" / "profile.d" / "micromamba.sh";
    }

    /*********************************
     * CshActivator implementation   *
     *********************************/

    std::string CshActivator::script(const EnvironmentTransform& env_transform)
    {
        std::stringstream out;
        if (!env_transform.export_path.empty())
        {
            if (util::on_win)
            {
                out << "setenv PATH '"
                    << native_path_to_unix(env_transform.export_path, /*is_a_env_path=*/true)
                    << "';\n";
            }
            else
            {
                out << "setenv PATH '" << env_transform.export_path << "';\n";
            }
        }

        for (const fs::u8path& ds : env_transform.deactivate_scripts)
        {
            out << "source '" << ds << "';\n";
        }

        for (const std::string& uvar : env_transform.unset_vars)
        {
            out << "unsetenv " << uvar << ";\n";
        }

        for (const auto& [skey, svar] : env_transform.set_vars)
        {
            out << "set " << skey << "='" << svar << "';\n";
        }

        for (const auto& [ekey, evar] : env_transform.export_vars)
        {
            if (util::on_win && ekey == "PATH")
            {
                out << "setenv " << ekey << " '"
                    << native_path_to_unix(evar, /*is_a_env_path=*/true) << "';\n";
            }
            else
            {
                out << "setenv " << ekey << " '" << evar << "';\n";
            }
        }

        for (const fs::u8path& p : env_transform.activate_scripts)
        {
            out << "source '" << p << "';\n";
        }

        return out.str();
    }

    std::pair<std::string, std::string>
    CshActivator::update_prompt(const std::string& conda_prompt_modifier)
    {
        std::string prompt = (m_env.find("prompt") != m_env.end()) ? m_env["prompt"] : "";
        auto current_prompt_modifier = env::get("CONDA_PROMPT_MODIFIER");
        if (current_prompt_modifier)
        {
            util::replace_all(prompt, current_prompt_modifier.value(), "");
        }
        // Because we're using single-quotes to set shell variables, we need to handle
        // the proper escaping of single quotes that are already part of the string.
        // Best solution appears to be https://stackoverflow.com/a/1250279
        util::replace_all(prompt, "'", "'\"'\"'");
        return { "prompt", conda_prompt_modifier + prompt };
    }


    std::string CshActivator::shell_extension()
    {
        return ".csh";
    }

    std::string CshActivator::shell()
    {
        return "csh";
    }

    std::string CshActivator::hook_preamble()
    {
        return std::string();
    }

    std::string CshActivator::hook_postamble()
    {
        return std::string();
    }

    fs::u8path CshActivator::hook_source_path()
    {
        return m_context.prefix_params.root_prefix / "etc" / "profile.d" / "micromamba.csh";
    }


    std::string CmdExeActivator::shell_extension()
    {
        return ".bat";
    }

    std::string CmdExeActivator::shell()
    {
        return "cmd.exe";
    }

    std::string CmdExeActivator::hook_preamble()
    {
        return "";
    }

    std::string CmdExeActivator::hook_postamble()
    {
        return "";
    }

    fs::u8path CmdExeActivator::hook_source_path()
    {
        return "";
    }

    std::pair<std::string, std::string>
    CmdExeActivator::update_prompt(const std::string& /* conda_prompt_modifier */)
    {
        return { "", "" };
    }

    std::string CmdExeActivator::script(const EnvironmentTransform& env_transform)
    {
        TemporaryFile* tempfile_ptr = new TemporaryFile("mamba_act", ".bat");
        std::stringstream out;

        if (!env_transform.export_path.empty())
        {
            out << "@SET \"PATH=" << env_transform.export_path << "\"\n";
        }

        for (const fs::u8path& ds : env_transform.deactivate_scripts)
        {
            out << "@CALL " << ds << "\n";
        }

        for (const std::string& uvar : env_transform.unset_vars)
        {
            out << "@SET " << uvar << "=\n";
        }

        for (const auto& [skey, svar] : env_transform.set_vars)
        {
            out << "@SET \"" << skey << "=" << svar << "\"\n";
        }

        for (const auto& [ekey, evar] : env_transform.export_vars)
        {
            out << "@SET \"" << ekey << "=" << evar << "\"\n";
        }

        for (const fs::u8path& p : env_transform.activate_scripts)
        {
            out << "@CALL " << p << "\n";
        }

        std::ofstream out_file = open_ofstream(tempfile_ptr->path());
        out_file << out.str();
        // note: we do not delete the tempfile ptr intentionally, so that the temp
        // file stays
        return tempfile_ptr->path().string();
    }

    std::string PowerShellActivator::shell_extension()
    {
        return ".ps1";
    }

    std::string PowerShellActivator::shell()
    {
        return "powershell";
    }

    std::string PowerShellActivator::hook_preamble()
    {
        return fmt::format(
            "$MambaModuleArgs = @{{ChangePs1 = ${}}}",
            m_context.change_ps1 ? "True" : "False"
        );
    }

    std::string PowerShellActivator::hook_postamble()
    {
        return "Remove-Variable MambaModuleArgs";
    }

    fs::u8path PowerShellActivator::hook_source_path()
    {
        return m_context.prefix_params.root_prefix / "condabin" / "mamba_hook.ps1";
    }

    std::pair<std::string, std::string>
    PowerShellActivator::update_prompt(const std::string& /*conda_prompt_modifier*/)
    {
        return { "", "" };
    }

    std::string PowerShellActivator::script(const EnvironmentTransform& env_transform)
    {
        std::stringstream out;

        if (!env_transform.export_path.empty())
        {
            out << "$Env:PATH =\"" << env_transform.export_path << "\"\n";
        }

        for (const fs::u8path& ds : env_transform.deactivate_scripts)
        {
            out << ". " << ds << "\n";
        }

        for (const std::string& uvar : env_transform.unset_vars)
        {
            out << "$Env:" << uvar << " = \"\"\n";
        }

        for (const auto& [skey, svar] : env_transform.set_vars)
        {
            out << "$Env:" << skey << " = \"" << svar << "\"\n";
        }

        for (const auto& [ekey, evar] : env_transform.export_vars)
        {
            out << "$Env:" << ekey << " = \"" << evar << "\"\n";
        }

        for (const fs::u8path& p : env_transform.activate_scripts)
        {
            out << ". " << p << "\n";
        }

        return out.str();
    }

    std::string XonshActivator::shell_extension()
    {
        return ".sh";
    }

    std::string XonshActivator::shell()
    {
        return "xonsh";
    }

    std::string XonshActivator::hook_preamble()
    {
        return "";
    }

    std::string XonshActivator::hook_postamble()
    {
        return "";
    }

    fs::u8path XonshActivator::hook_source_path()
    {
        return m_context.prefix_params.root_prefix / "etc" / "profile.d" / "mamba.xsh";
    }

    std::pair<std::string, std::string>
    XonshActivator::update_prompt(const std::string& /*conda_prompt_modifier*/)
    {
        return { "", "" };
    }

    std::string XonshActivator::script(const EnvironmentTransform& env_transform)
    {
        std::stringstream out;

        if (!env_transform.export_path.empty())
        {
            out << "$PATH=\"" << env_transform.export_path << "\"\n";
        }

        for (const fs::u8path& ds : env_transform.deactivate_scripts)
        {
            out << "source-bash " << ds << "\n";
        }

        for (const std::string& uvar : env_transform.unset_vars)
        {
            out << "del $" << uvar << "\n";
        }

        for (const auto& [skey, svar] : env_transform.set_vars)
        {
            out << "$" << skey << " = \"" << svar << "\"\n";
        }

        for (const auto& [ekey, evar] : env_transform.export_vars)
        {
            out << "$" << ekey << " = \"" << evar << "\"\n";
        }

        for (const fs::u8path& p : env_transform.activate_scripts)
        {
            out << "source-bash " << p << "\n";
        }

        return out.str();
    }

    std::string FishActivator::shell_extension()
    {
        return ".fish";
    }

    std::string FishActivator::shell()
    {
        return "fish";
    }

    std::string FishActivator::hook_preamble()
    {
        return "";
    }

    std::string FishActivator::hook_postamble()
    {
        return "";
    }

    fs::u8path FishActivator::hook_source_path()
    {
        return m_context.prefix_params.root_prefix / "etc" / "fish" / "conf.d" / "mamba.fish";
    }

    std::pair<std::string, std::string>
    FishActivator::update_prompt(const std::string& /*conda_prompt_modifier*/)
    {
        return { "", "" };
    }

    std::string FishActivator::script(const EnvironmentTransform& env_transform)
    {
        std::stringstream out;

        if (!env_transform.export_path.empty())
        {
            out << "set -gx PATH \"" << env_transform.export_path << "\"\n";
        }

        for (const fs::u8path& ds : env_transform.deactivate_scripts)
        {
            out << "source " << ds << "\n";
        }

        for (const std::string& uvar : env_transform.unset_vars)
        {
            out << "set -e " << uvar << " || true\n";
        }

        for (const auto& [skey, svar] : env_transform.set_vars)
        {
            out << "set " << skey << " \"" << svar << "\"\n";
        }

        for (const auto& [ekey, evar] : env_transform.export_vars)
        {
            out << "set -gx " << ekey << " \"" << evar << "\"\n";
        }

        for (const fs::u8path& p : env_transform.activate_scripts)
        {
            out << "source " << p << "\n";
        }

        return out.str();
    }
}  // namespace mamba
