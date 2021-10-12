// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <regex>

#include "mamba/core/shell_init.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/util.hpp"
#include "mamba/core/util_os.hpp"
#include "mamba/core/activation.hpp"
#include "mamba/core/environment.hpp"
#include "mamba/core/virtual_packages.hpp"

#include "thirdparty/termcolor.hpp"

#include <reproc++/run.hpp>

#ifdef _WIN32
#include "thirdparty/WinReg.hpp"
#endif

namespace mamba
{
    namespace
    {
        // Here we are embedding the shell scripts
        constexpr const char mamba_sh[] =
#include "../data/mamba.sh"
            ;
        constexpr const char mamba_bat[] =
#include "../data/micromamba.bat"
            ;
        constexpr const char activate_bat[] =
#include "../data/activate.bat"
            ;
        constexpr const char _mamba_activate_bat[] =
#include "../data/_mamba_activate.bat"
            ;
        constexpr const char mamba_hook_bat[] =
#include "../data/mamba_hook.bat"
            ;
        constexpr const char mamba_hook_ps1[] =
#include "../data/mamba_hook.ps1"
            ;
        constexpr const char mamba_psm1[] =
#include "../data/Mamba.psm1"
            ;
        constexpr const char mamba_xsh[] =
#include "../data/mamba.xsh"
            ;
        constexpr const char mamba_fish[] =
#include "../data/mamba.fish"
            ;

        std::regex CONDA_INITIALIZE_RE_BLOCK("# >>> mamba initialize >>>(?:\n|\r\n)?"
                                             "([\\s\\S]*?)"
                                             "# <<< mamba initialize <<<(?:\n|\r\n)?");

        std::regex CONDA_INITIALIZE_PS_RE_BLOCK("#region mamba initialize(?:\n|\r\n)?"
                                                "([\\s\\S]*?)"
                                                "#endregion(?:\n|\r\n)?");
    }

    std::string guess_shell()
    {
        std::string parent_process_name = get_process_name_by_pid(getppid());

        if (contains(parent_process_name, "bash"))
        {
            return "bash";
        }
        if (contains(parent_process_name, "zsh"))
        {
            return "zsh";
        }
        if (contains(parent_process_name, "xonsh"))
        {
            return "xonsh";
        }
        if (contains(parent_process_name, "cmd.exe"))
        {
            return "cmd.exe";
        }
        if (contains(parent_process_name, "powershell"))
        {
            return "powershell";
        }
        if (contains(parent_process_name, "fish"))
        {
            return "fish";
        }
        return "";
    }

#ifdef _WIN32
    void init_cmd_exe_registry(const std::wstring& reg_path,
                               const fs::path& conda_prefix,
                               bool reverse)
    {
        winreg::RegKey key{ HKEY_CURRENT_USER, reg_path };
        std::wstring prev_value;
        try
        {
            prev_value = key.GetStringValue(L"AutoRun");
        }
        catch (const std::exception& e)
        {
            LOG_INFO << "No AutoRun key detected.";
        }
        // std::wstring hook_path = '"%s"' % join(conda_prefix, 'condabin', 'conda_hook.bat')
        std::wstring hook_string = std::wstring(L"\"")
                                   + (conda_prefix / "condabin" / "mamba_hook.bat").wstring()
                                   + std::wstring(L"\"");
        if (reverse)
        {
            // Not implemented yet
        }
        else
        {
            std::wstring replace_str(L"__CONDA_REPLACE_ME_123__");
            std::wregex hook_regex(L"(\"[^\"]*?mamba[-_]hook\\.bat\")",
                                   std::regex_constants::icase);
            std::wstring replaced_value = std::regex_replace(
                prev_value, hook_regex, replace_str, std::regex_constants::format_first_only);

            std::wstring new_value = replaced_value;

            if (replaced_value.find(replace_str) == new_value.npos)
            {
                if (!new_value.empty())
                {
                    new_value += L" & " + hook_string;
                }
                else
                {
                    new_value = hook_string;
                }
            }
            else
            {
                replace_all(new_value, replace_str, hook_string);
            }

            if (new_value != prev_value)
            {
                std::cout << "Adding to cmd.exe AUTORUN: " << termcolor::green;
                std::wcout << new_value;
                std::cout << termcolor::reset << std::endl;
                key.SetStringValue(L"AutoRun", new_value);
            }
            else
            {
                std::cout << termcolor::green << "cmd.exe already initialized." << termcolor::reset
                          << std::endl;
            }
        }
    }
#endif

    // this function calls cygpath to convert win path to unix
    std::string native_path_to_unix(const std::string& path, bool is_a_path_env)
    {
        fs::path bash = env::which("bash");
        std::string command = bash.empty() ? "cygpath" : bash.parent_path() / "cygpath";
        std::string out, err;
        try
        {
            std::vector<std::string> args{ command, path };
            if (is_a_path_env)
                args.push_back("--path");
            auto [status, ec] = reproc::run(
                args, reproc::options{}, reproc::sink::string(out), reproc::sink::string(err));
            if (ec)
            {
                throw std::runtime_error(ec.message());
            }
            return std::string(strip(out));
        }
        catch (...)
        {
            std::cout
                << termcolor::red
                << "ERROR: Could not find bash, or use cygpath to convert Windows path to Unix."
                << termcolor::reset << std::endl;
            exit(1);
        }
    }


    std::string rcfile_content(const fs::path& env_prefix,
                               const std::string& shell,
                               const fs::path& mamba_exe)
    {
        std::stringstream content;

        // todo use get bin dir here!
#ifdef _WIN32
        std::string cyg_mamba_exe = native_path_to_unix(mamba_exe.string());
        std::string cyg_env_prefix = native_path_to_unix(env_prefix.string());
        content << "# >>> mamba initialize >>>\n";
        content << "# !! Contents within this block are managed by 'mamba init' !!\n";
        content << "export MAMBA_EXE=" << std::quoted(cyg_mamba_exe, '\'') << ";\n";
        content << "export MAMBA_ROOT_PREFIX=" << std::quoted(cyg_env_prefix, '\'') << ";\n";
        content << "eval \"$(" << std::quoted(cyg_mamba_exe, '\'') << " shell hook --shell "
                << shell << " --prefix " << std::quoted(cyg_env_prefix, '\'') << ")\"\n";
        content << "# <<< mamba initialize <<<\n";
        return content.str();

#else

        fs::path env_bin = env_prefix / "bin";

        content << "# >>> mamba initialize >>>\n";
        content << "# !! Contents within this block are managed by 'mamba init' !!\n";
        content << "export MAMBA_EXE=" << mamba_exe << ";\n";
        content << "export MAMBA_ROOT_PREFIX=" << env_prefix << ";\n";
        content << "__mamba_setup=\"$(" << std::quoted(mamba_exe.string(), '\'')
                << " shell hook --shell " << shell << " --prefix "
                << std::quoted(env_prefix.string(), '\'') << " 2> /dev/null)\"\n";
        content << "if [ $? -eq 0 ]; then\n";
        content << "    eval \"$__mamba_setup\"\n";
        content << "else\n";
        content << "    if [ -f " << (env_prefix / "etc" / "profile.d" / "mamba.sh")
                << " ]; then\n";
        content << "        . " << (env_prefix / "etc" / "profile.d" / "mamba.sh") << "\n";
        content << "    else\n";
        content << "        export PATH=\"" << env_bin.c_str() << ":$PATH\"\n";
        content << "    fi\n";
        content << "fi\n";
        content << "unset __mamba_setup\n";
        content << "# <<< mamba initialize <<<\n";

        return content.str();

#endif
    }

    std::string xonsh_content(const fs::path& env_prefix,
                              const std::string& shell,
                              const fs::path& mamba_exe)
    {
        std::stringstream content;
        std::string s_mamba_exe;

        if (on_win)
        {
            s_mamba_exe = native_path_to_unix(mamba_exe.string());
        }
        else
        {
            s_mamba_exe = mamba_exe;
        }

        content << "# >>> mamba initialize >>>\n";
        content << "# !! Contents within this block are managed by 'mamba init' !!\n";
        content << "$MAMBA_EXE = " << mamba_exe << "\n";
        content << "$MAMBA_ROOT_PREFIX = " << env_prefix << "\n";
        content << "import sys as _sys\n";
        content << "from types import ModuleType as _ModuleType\n";
        content << "_mod = _ModuleType(\"xontrib.mamba\",\n";
        content << "                   \'Autogenerated from $(" << mamba_exe
                << " shell hook -s xonsh -p " << env_prefix << ")\')\n";
        content << "__xonsh__.execer.exec($(" << mamba_exe << " \"shell\" \"hook\" -s xonsh -p "
                << env_prefix << "),\n";
        content << "                      glbs=_mod.__dict__,\n";
        content << "                      filename=\'$(" << mamba_exe << " shell hook -s xonsh -p "
                << env_prefix << ")\')\n";
        content << "_sys.modules[\"xontrib.mamba\"] = _mod\n";
        content << "del _sys, _mod, _ModuleType\n";
        content << "# <<< mamba initialize <<<\n";
        return content.str();
    }

    std::string fish_content(const fs::path& env_prefix,
                             const std::string& shell,
                             const fs::path& mamba_exe)
    {
        std::stringstream content;
        std::string s_mamba_exe;

        if (on_win)
        {
            s_mamba_exe = native_path_to_unix(mamba_exe.string());
        }
        else
        {
            s_mamba_exe = mamba_exe;
        }

        content << "# >>> mamba initialize >>>\n";
        content << "# !! Contents within this block are managed by 'mamba init' !!\n";
        content << "set -gx MAMBA_EXE " << mamba_exe << "\n";
        content << "set -gx MAMBA_ROOT_PREFIX " << env_prefix << "\n";
        content << "eval " << mamba_exe << " shell hook --shell fish --prefix " << env_prefix
                << " | source\n";
        content << "# <<< mamba initialize <<<\n";
        return content.str();
    }

    bool modify_rc_file(const fs::path& file_path,
                        const fs::path& conda_prefix,
                        const std::string& shell,
                        const fs::path& mamba_exe)
    {
        Console::stream() << "Modifiying RC file " << file_path
                          << "\nGenerating config for root prefix " << termcolor::bold
                          << conda_prefix << termcolor::reset
                          << "\nSetting mamba executable to: " << termcolor::bold << mamba_exe
                          << termcolor::reset;

        // TODO do we need binary or not?
        std::string conda_init_content, rc_content;
        if (fs::exists(file_path))
        {
            rc_content = read_contents(file_path, std::ios::in);
        }

        if (shell == "xonsh")
        {
            conda_init_content = xonsh_content(conda_prefix, shell, mamba_exe);
        }
        if (shell == "fish")
        {
            conda_init_content = fish_content(conda_prefix, shell, mamba_exe);
        }
        else
        {
            conda_init_content = rcfile_content(conda_prefix, shell, mamba_exe);
        }

        Console::stream() << "Adding (or replacing) the following in your " << file_path
                          << " file\n"
                          << termcolor::colorize << termcolor::green << conda_init_content
                          << termcolor::reset;

        std::string result
            = std::regex_replace(rc_content, CONDA_INITIALIZE_RE_BLOCK, conda_init_content);

        if (result.find("# >>> mamba initialize >>>") == result.npos)
        {
            std::ofstream rc_file(file_path, std::ios::app | std::ios::binary);
            rc_file << std::endl << conda_init_content;
        }
        else
        {
            std::ofstream rc_file(file_path, std::ios::out | std::ios::binary);
            rc_file << result;
        }
        return true;
    }

    std::string get_hook_contents(const std::string& shell)
    {
        fs::path exe = get_self_exe_path();

        if (shell == "zsh" || shell == "bash" || shell == "posix")
        {
            std::string contents = mamba_sh;
            replace_all(contents, "$MAMBA_EXE", exe.string());
            return contents;
        }
        else if (shell == "xonsh")
        {
            std::string contents = mamba_xsh;
            replace_all(contents, "$MAMBA_EXE", exe.string());
            return contents;
        }
        else if (shell == "powershell")
        {
            std::stringstream contents;
            contents << "$Env:MAMBA_EXE='" << exe.native() << "'\n";
            std::string psm1 = mamba_psm1;
            psm1 = psm1.substr(0, psm1.find("## EXPORTS ##"));
            contents << psm1;
            return contents.str();
        }
        else if (shell == "cmd.exe")
        {
            init_root_prefix_cmdexe(Context::instance().root_prefix);
            LOG_WARNING << "Hook installed, now 'manually' execute:";
            LOG_WARNING
                << "       CALL "
                << std::quoted(
                       (Context::instance().root_prefix / "condabin" / "mamba_hook.bat").string());
        }
        else if (shell == "fish")
        {
            std::string contents = mamba_fish;
            replace_all(contents, "$MAMBA_EXE", exe.string());
            return contents;
        }
        return "";
    }

    void init_root_prefix_cmdexe(const fs::path& root_prefix)
    {
        fs::path exe = get_self_exe_path();

        try
        {
            fs::create_directories(root_prefix / "condabin");
            fs::create_directories(root_prefix / "Scripts");
        }
        catch (...)
        {
            // Maybe the prefix isn't writable. No big deal, just keep going.
        }

        std::ofstream mamba_bat_f(root_prefix / "condabin" / "micromamba.bat");
        std::string mamba_bat_contents(mamba_bat);
        replace_all(mamba_bat_contents,
                    std::string("__MAMBA_INSERT_ROOT_PREFIX__"),
                    std::string("@SET \"MAMBA_ROOT_PREFIX=" + root_prefix.string() + "\""));
        replace_all(mamba_bat_contents,
                    std::string("__MAMBA_INSERT_MAMBA_EXE__"),
                    std::string("@SET \"MAMBA_EXE=" + exe.string() + "\""));

        mamba_bat_f << mamba_bat_contents;
        std::ofstream _mamba_activate_bat_f(root_prefix / "condabin" / "_mamba_activate.bat");
        _mamba_activate_bat_f << _mamba_activate_bat;


        std::string activate_bat_contents(activate_bat);
        replace_all(activate_bat_contents,
                    std::string("__MAMBA_INSERT_ROOT_PREFIX__"),
                    std::string("@SET \"MAMBA_ROOT_PREFIX=" + root_prefix.string() + "\""));
        replace_all(activate_bat_contents,
                    std::string("__MAMBA_INSERT_MAMBA_EXE__"),
                    std::string("@SET \"MAMBA_EXE=" + exe.string() + "\""));


        std::ofstream condabin_activate_bat_f(root_prefix / "condabin" / "activate.bat");
        condabin_activate_bat_f << activate_bat_contents;

        std::ofstream scripts_activate_bat_f(root_prefix / "Scripts" / "activate.bat");
        scripts_activate_bat_f << activate_bat_contents;

        std::string hook_content = mamba_hook_bat;
        replace_all(hook_content,
                    std::string("__MAMBA_INSERT_MAMBA_EXE__"),
                    std::string("@SET \"MAMBA_EXE=" + exe.string() + "\""));

        std::ofstream mamba_hook_bat_f(root_prefix / "condabin" / "mamba_hook.bat");
        mamba_hook_bat_f << hook_content;
    }

    void init_root_prefix(const std::string& shell, const fs::path& root_prefix)
    {
        Context::instance().root_prefix = root_prefix;

        if (shell == "zsh" || shell == "bash" || shell == "posix")
        {
            PosixActivator a;
            auto sh_source_path = a.hook_source_path();
            try
            {
                fs::create_directories(sh_source_path.parent_path());
            }
            catch (...)
            {
                // Maybe the prefix isn't writable. No big deal, just keep going.
            }
            std::ofstream sh_file(sh_source_path);
            sh_file << mamba_sh;
        }
        if (shell == "xonsh")
        {
            XonshActivator a;
            auto sh_source_path = a.hook_source_path();
            try
            {
                fs::create_directories(sh_source_path.parent_path());
            }
            catch (...)
            {
                // Maybe the prefix isn't writable. No big deal, just keep going.
            }
            std::ofstream sh_file(sh_source_path);
            sh_file << mamba_xsh;
        }
        else if (shell == "cmd.exe")
        {
            init_root_prefix_cmdexe(root_prefix);
        }
        else if (shell == "powershell")
        {
            try
            {
                fs::create_directories(root_prefix / "condabin");
            }
            catch (...)
            {
                // Maybe the prefix isn't writable. No big deal, just keep going.
            }
            std::ofstream mamba_hook_f(root_prefix / "condabin" / "mamba_hook.ps1");
            mamba_hook_f << mamba_hook_ps1;
            std::ofstream mamba_psm1_f(root_prefix / "condabin" / "Mamba.psm1");
            mamba_psm1_f << mamba_psm1;
        }
    }

    std::string powershell_contents(const fs::path& conda_prefix)
    {
        fs::path self_exe = get_self_exe_path();

        std::stringstream out;

        out << "#region mamba initialize\n";
        out << "# !! Contents within this block are managed by 'mamba shell init' !!\n";
        out << "$Env:MAMBA_ROOT_PREFIX = " << conda_prefix << "\n";
        out << "$Env:MAMBA_EXE = '" << self_exe.native() << "'\n";
        out << "(& '" << self_exe.native() << "' 'shell' 'hook' -s 'powershell' -p " << conda_prefix
            << ") | Out-String | Invoke-Expression\n";
        out << "#endregion\n";
        return out.str();
    }

    bool init_powershell(const fs::path& profile_path, const fs::path& conda_prefix, bool reverse)
    {
        // NB: the user may not have created a profile. We need to check
        //     if the file exists first.
        std::string profile_content, profile_original_content;
        if (fs::exists(profile_path))
        {
            profile_content = read_contents(profile_path);
            profile_original_content = profile_content;
        }

        std::string conda_init_content = powershell_contents(conda_prefix);
        bool found_mamba_initialize
            = profile_content.find("#region mamba initialize") != profile_content.npos;

        if (reverse)
        {
            profile_content = std::regex_replace(profile_content, CONDA_INITIALIZE_PS_RE_BLOCK, "");
        }
        else
        {
            // Find what content we need to add.
            Console::stream() << "Adding (or replacing) the following in your " << profile_path
                              << " file\n"
                              << termcolor::colorize << termcolor::green << conda_init_content
                              << termcolor::reset;

            if (found_mamba_initialize)
            {
                profile_content = std::regex_replace(
                    profile_content, CONDA_INITIALIZE_PS_RE_BLOCK, conda_init_content);
            }
        }

        if (profile_content != profile_original_content || !found_mamba_initialize)
        {
            if (!Context::instance().dry_run)
            {
                if (!fs::exists(profile_path.parent_path()))
                {
                    fs::create_directories(profile_path.parent_path());
                }

                if (!found_mamba_initialize)
                {
                    std::ofstream out(profile_path, std::ios::app | std::ios::binary);
                    out << std::endl << conda_init_content;
                }
                else
                {
                    std::ofstream out(profile_path, std::ios::out | std::ios::binary);
                    out << profile_content;
                }

                return true;
            }
        }
        return false;
    }

    void init_shell(const std::string& shell, const fs::path& conda_prefix)
    {
        init_root_prefix(shell, conda_prefix);
        auto mamba_exe = get_self_exe_path();
        fs::path home = env::home_directory();
        if (shell == "bash")
        {
            fs::path bashrc_path = (on_mac || on_win) ? home / ".bash_profile" : home / ".bashrc";
            modify_rc_file(bashrc_path, conda_prefix, shell, mamba_exe);
        }
        else if (shell == "zsh")
        {
            fs::path zshrc_path = home / ".zshrc";
            modify_rc_file(zshrc_path, conda_prefix, shell, mamba_exe);
        }
        else if (shell == "xonsh")
        {
            fs::path xonshrc_path = home / ".xonshrc";
            // std::cout << xonsh_content(conda_prefix, shell, mamba_exe);
            modify_rc_file(xonshrc_path, conda_prefix, shell, mamba_exe);
        }
        else if (shell == "fish")
        {
            fs::path fishrc_path = home / ".config" / "fish" / "config.fish";
            modify_rc_file(fishrc_path, conda_prefix, shell, mamba_exe);
        }
        else if (shell == "cmd.exe")
        {
#ifndef _WIN32
            throw std::runtime_error("CMD.EXE can only be initialized on Windows.");
#else
            init_cmd_exe_registry(L"Software\\Microsoft\\Command Processor", conda_prefix, false);
#endif
        }
        else if (shell == "powershell")
        {
            std::string profile_var("$PROFILE.CurrentUserAllHosts");
            // if (for_system)
            //     profile = "$PROFILE.AllUsersAllHosts"

            // There's several places PowerShell can store its path, depending
            // on if it's Windows PowerShell, PowerShell Core on Windows, or
            // PowerShell Core on macOS/Linux. The easiest way to resolve it is to
            // just ask different possible installations of PowerShell where their
            // profiles are.

            auto find_powershell_paths = [&profile_var](const std::string& exe) -> std::string {
                try
                {
                    std::string out, err;
                    auto [status, ec] = reproc::run(
                        std::vector<std::string>{ exe, "-NoProfile", "-Command", profile_var },
                        reproc::options{},
                        reproc::sink::string(out),
                        reproc::sink::string(err));
                    if (ec)
                    {
                        throw std::runtime_error(ec.message());
                    }
                    return std::string(strip(out));
                }
                catch (...)
                {
                    return "";
                }
            };

            std::string profile_path, exe;
            for (auto& iter_exe : std::vector<std::string>{ "powershell", "pwsh", "pwsh-preview" })
            {
                auto res = find_powershell_paths(iter_exe);
                if (!res.empty())
                {
                    profile_path = res;
                    exe = iter_exe;
                    break;
                }
            }
            std::cout << "Found powershell at " << exe << " and user profile at " << profile_path
                      << std::endl;

            init_powershell(profile_path, conda_prefix, false);
        }
        else
        {
            throw std::runtime_error("Support for other shells not yet implemented.");
        }
#ifdef _WIN32
        enable_long_paths_support(false);
#endif
    }
}
