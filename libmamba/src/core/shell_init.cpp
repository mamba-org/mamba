// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <regex>

#include "mamba/core/shell_init.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/util.hpp"
#include "mamba/core/util_os.hpp"
#include "mamba/core/activation.hpp"
#include "mamba/core/environment.hpp"

#include "progress_bar_impl.hpp"

#include "termcolor/termcolor.hpp"

#include <reproc++/run.hpp>
#include <stdexcept>

#ifdef _WIN32
#include "WinReg.hpp"
#endif

namespace mamba
{
    namespace
    {
        std::regex CONDA_INITIALIZE_RE_BLOCK("\n# >>> mamba initialize >>>(?:\n|\r\n)?"
                                             "([\\s\\S]*?)"
                                             "# <<< mamba initialize <<<(?:\n|\r\n)?");

        std::regex CONDA_INITIALIZE_PS_RE_BLOCK("\n#region mamba initialize(?:\n|\r\n)?"
                                                "([\\s\\S]*?)"
                                                "#endregion(?:\n|\r\n)?");
    }

    std::string guess_shell()
    {
        std::string parent_process_name = get_process_name_by_pid(getppid());

        LOG_DEBUG << "Guessing shell. Parent process name: " << parent_process_name;

        std::string parent_process_name_lower = to_lower(parent_process_name);

        if (contains(parent_process_name_lower, "bash"))
        {
            return "bash";
        }
        if (contains(parent_process_name_lower, "zsh"))
        {
            return "zsh";
        }
        if (contains(parent_process_name_lower, "csh"))
        {
            return "csh";
        }
        if (contains(parent_process_name_lower, "dash"))
        {
            return "dash";
        }

        // xonsh in unix, Python in macOS
        if (contains(parent_process_name_lower, "python"))
        {
            Console::stream() << "Your parent process name is " << parent_process_name
                              << ".\nIf your shell is xonsh, please use \"-s xonsh\"." << std::endl;
        }
        if (contains(parent_process_name_lower, "xonsh"))
        {
            return "xonsh";
        }
        if (contains(parent_process_name_lower, "cmd.exe"))
        {
            return "cmd.exe";
        }
        if (contains(parent_process_name_lower, "powershell")
            || contains(parent_process_name_lower, "pwsh"))
        {
            return "powershell";
        }
        if (contains(parent_process_name_lower, "fish"))
        {
            return "fish";
        }
        return "";
    }

#ifdef _WIN32
    std::wstring get_autorun_registry_key(const std::wstring& reg_path)
    {
        winreg::RegKey key{ HKEY_CURRENT_USER, reg_path };
        std::wstring content;
        try
        {
            content = key.GetStringValue(L"AutoRun");
        }
        catch (const std::exception&)
        {
            LOG_INFO << "No AutoRun key detected.";
        }
        return content;
    }

    void set_autorun_registry_key(const std::wstring& reg_path, const std::wstring& value)
    {
        std::cout << "Setting cmd.exe AUTORUN to: " << termcolor::green;
        std::wcout << value;
        std::cout << termcolor::reset << std::endl;

        winreg::RegKey key{ HKEY_CURRENT_USER, reg_path };
        key.SetStringValue(L"AutoRun", value);
    }

    std::wstring get_hook_string(const fs::u8path& conda_prefix)
    {
        // '"%s"' % join(conda_prefix, 'condabin', 'conda_hook.bat')
        return std::wstring(L"\"") + (conda_prefix / "condabin" / "mamba_hook.bat").wstring()
               + std::wstring(L"\"");
    }

    void init_cmd_exe_registry(const std::wstring& reg_path, const fs::u8path& conda_prefix)
    {
        std::wstring prev_value = get_autorun_registry_key(reg_path);
        std::wstring hook_string = get_hook_string(conda_prefix);

        // modify registry key
        std::wstring replace_str(L"__CONDA_REPLACE_ME_123__");
        std::wregex hook_regex(L"(\"[^\"]*?mamba[-_]hook\\.bat\")", std::regex_constants::icase);
        std::wstring replaced_value = std::regex_replace(
            prev_value, hook_regex, replace_str, std::regex_constants::format_first_only);

        std::wstring new_value = replaced_value;

        if (replaced_value.find(replace_str) == std::wstring::npos)
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

        // set modified registry key
        if (new_value != prev_value)
        {
            set_autorun_registry_key(reg_path, new_value);
        }
        else
        {
            std::cout << termcolor::green << "cmd.exe already initialized." << termcolor::reset
                      << std::endl;
        }
    }

    void deinit_cmd_exe_registry(const std::wstring& reg_path, const fs::u8path& conda_prefix)
    {
        std::wstring prev_value = get_autorun_registry_key(reg_path);
        std::wstring hook_string = get_hook_string(conda_prefix);

        // modify registry key
        // remove the mamba hook from the autorun list
        std::wstringstream stringstream(prev_value);
        std::wstring segment;
        std::vector<std::wstring> autorun_list;

        autorun_list = split(std::wstring_view(prev_value), std::wstring_view(L"&"));

        // remove the mamba hook from the autorun list
        autorun_list.erase(std::remove_if(autorun_list.begin(),
                                          autorun_list.end(),
                                          [&hook_string](const std::wstring& s)
                                          { return strip(s) == hook_string; }),
                           autorun_list.end());

        // join the list back into a string
        std::wstring new_value = join(L" & ", autorun_list);

        // set modified registry key
        if (new_value != prev_value)
        {
            set_autorun_registry_key(reg_path, new_value);
        }
        else
        {
            std::cout << termcolor::green << "cmd.exe not initialized yet." << termcolor::reset
                      << std::endl;
        }
    }
#endif  // _WIN32

    // this function calls cygpath to convert win path to unix
    std::string native_path_to_unix(const std::string& path, bool is_a_path_env)
    {
        /*
          It is very easy to end up with a bash in one place and a cygpath in another (e.g. git bash
          + cygpath installed in the env from m2w64 packages). In that case, reactivating from that
          env will replace <prefix> or <prefix>/Library (Windows) by a POSIX root '/', breaking the
          PATH.
        */
        fs::u8path bash;
        fs::u8path parent_process_name = get_process_name_by_pid(getppid());
        if (contains(parent_process_name.filename().string(), "bash"))
            bash = parent_process_name;
        else
#ifdef _WIN32
            bash = env::which("bash.exe");
#else
            bash = env::which("bash");
#endif
        const std::string command
            = bash.empty() ? "cygpath" : (bash.parent_path() / "cygpath").string();
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
            throw std::runtime_error(
                "Could not find bash, or use cygpath to convert Windows path to Unix.");
        }
    }


    std::string rcfile_content(const fs::u8path& env_prefix,
                               const std::string& shell,
                               const fs::u8path& mamba_exe)
    {
        std::stringstream content;

        // todo use get bin dir here!
#ifdef _WIN32
        std::string cyg_mamba_exe = native_path_to_unix(mamba_exe.string());
        std::string cyg_env_prefix = native_path_to_unix(env_prefix.string());
        content << "\n# >>> mamba initialize >>>\n";
        content << "# !! Contents within this block are managed by 'mamba init' !!\n";
        content << "export MAMBA_EXE=" << std::quoted(cyg_mamba_exe, '\'') << ";\n";
        content << "export MAMBA_ROOT_PREFIX=" << std::quoted(cyg_env_prefix, '\'') << ";\n";
        content << "eval \"$(" << std::quoted(cyg_mamba_exe, '\'') << " shell hook --shell "
                << shell << " --prefix " << std::quoted(cyg_env_prefix, '\'') << ")\"\n";
        content << "# <<< mamba initialize <<<\n";
        return content.str();

#else

        fs::u8path env_bin = env_prefix / "bin";

        content << "\n# >>> mamba initialize >>>\n";
        content << "# !! Contents within this block are managed by 'mamba init' !!\n";
        content << "export MAMBA_EXE=" << mamba_exe << ";\n";
        content << "export MAMBA_ROOT_PREFIX=" << env_prefix << ";\n";
        content << "__mamba_setup=\"$(" << std::quoted(mamba_exe.string(), '\'')
                << " shell hook --shell " << shell << " --prefix "
                << std::quoted(env_prefix.string(), '\'') << " 2> /dev/null)\"\n";
        content << "if [ $? -eq 0 ]; then\n";
        content << "    eval \"$__mamba_setup\"\n";
        content << "else\n";
        content << "    if [ -f " << (env_prefix / "etc" / "profile.d" / "micromamba.sh")
                << " ]; then\n";
        content << "        . " << (env_prefix / "etc" / "profile.d" / "micromamba.sh") << "\n";
        content << "    else\n";
        content << "        export  PATH=\"" << env_bin.string().c_str() << ":$PATH\""
                << "  # extra space after export prevents interference from conda init\n";
        content << "    fi\n";
        content << "fi\n";
        content << "unset __mamba_setup\n";
        content << "# <<< mamba initialize <<<\n";

        return content.str();

#endif
    }

    std::string xonsh_content(const fs::u8path& env_prefix,
                              const std::string& /*shell*/,
                              const fs::u8path& mamba_exe)
    {
        std::stringstream content;
        std::string s_mamba_exe;

        if (on_win)
        {
            s_mamba_exe = native_path_to_unix(mamba_exe.string());
        }
        else
        {
            s_mamba_exe = mamba_exe.string();
        }

        content << "\n# >>> mamba initialize >>>\n";
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

    std::string fish_content(const fs::u8path& env_prefix,
                             const std::string& /*shell*/,
                             const fs::u8path& mamba_exe)
    {
        std::stringstream content;
        std::string s_mamba_exe;

        if (on_win)
        {
            s_mamba_exe = native_path_to_unix(mamba_exe.string());
        }
        else
        {
            s_mamba_exe = mamba_exe.string();
        }

        content << "\n# >>> mamba initialize >>>\n";
        content << "# !! Contents within this block are managed by 'mamba init' !!\n";
        content << "set -gx MAMBA_EXE " << mamba_exe << "\n";
        content << "set -gx MAMBA_ROOT_PREFIX " << env_prefix << "\n";
        content << "eval " << mamba_exe << " shell hook --shell fish --prefix " << env_prefix
                << " | source\n";
        content << "# <<< mamba initialize <<<\n";
        return content.str();
    }

    std::string csh_content(const fs::u8path& env_prefix,
                            const std::string& /*shell*/,
                            const fs::u8path& mamba_exe)
    {
        std::stringstream content;
        std::string s_mamba_exe;

        if (on_win)
        {
            s_mamba_exe = native_path_to_unix(mamba_exe.string());
        }
        else
        {
            s_mamba_exe = mamba_exe.string();
        }

        content << "\n# >>> mamba initialize >>>\n";
        content << "# !! Contents within this block are managed by 'mamba init' !!\n";
        content << "setenv MAMBA_EXE " << mamba_exe << ";\n";
        content << "setenv MAMBA_ROOT_PREFIX " << env_prefix << ";\n";
        content << "source $MAMBA_ROOT_PREFIX/etc/profile.d/micromamba.csh;\n";
        content << "# <<< mamba initialize <<<\n";
        return content.str();
    }

    void modify_rc_file(const fs::u8path& file_path,
                        const fs::u8path& conda_prefix,
                        const std::string& shell,
                        const fs::u8path& mamba_exe)
    {
        Console::stream() << "Modifying RC file " << file_path
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
        else if (shell == "fish")
        {
            conda_init_content = fish_content(conda_prefix, shell, mamba_exe);
        }
        else if (shell == "csh")
        {
            conda_init_content = csh_content(conda_prefix, shell, mamba_exe);
        }
        else
        {
            conda_init_content = rcfile_content(conda_prefix, shell, mamba_exe);
        }

        Console::stream() << "Adding (or replacing) the following in your " << file_path
                          << " file\n"
                          << termcolor::colorize << termcolor::green << conda_init_content
                          << termcolor::reset;

        if (Context::instance().dry_run)
        {
            return;
        }

        std::string result
            = std::regex_replace(rc_content, CONDA_INITIALIZE_RE_BLOCK, conda_init_content);

        if (result.find("# >>> mamba initialize >>>") == std::string::npos)
        {
            std::ofstream rc_file = open_ofstream(file_path, std::ios::app | std::ios::binary);
            rc_file << conda_init_content;
        }
        else
        {
            std::ofstream rc_file = open_ofstream(file_path, std::ios::out | std::ios::binary);
            rc_file << result;
        }
    }

    void reset_rc_file(const fs::u8path& file_path,
                       const std::string& shell,
                       const fs::u8path& mamba_exe)
    {
        Console::stream() << "Resetting RC file " << file_path
                          << "\nDeleting config for root prefix "
                          << "\nClearing mamba executable environment variable";

        std::string conda_init_content, rc_content;

        if (!fs::exists(file_path))
        {
            LOG_INFO << "File does not exist, nothing to do.";
            return;
        }
        else
        {
            rc_content = read_contents(file_path, std::ios::in);
        }

        Console::stream() << "Removing the following in your " << file_path << " file\n"
                          << termcolor::colorize << termcolor::green
                          << "# >>> mamba initialize >>>\n...\n# <<< mamba initialize <<<\n"
                          << termcolor::reset;

        if (rc_content.find("# >>> mamba initialize >>>") == std::string::npos)
        {
            LOG_INFO << "No mamba initialize block found, nothing to do.";
            return;
        }

        std::string result = std::regex_replace(rc_content, CONDA_INITIALIZE_RE_BLOCK, "");

        if (Context::instance().dry_run)
        {
            return;
        }

        std::ofstream rc_file = open_ofstream(file_path, std::ios::out | std::ios::binary);
        rc_file << result;
    }

    std::string get_hook_contents(const std::string& shell)
    {
        fs::u8path exe = get_self_exe_path();

        if (shell == "zsh" || shell == "bash" || shell == "posix")
        {
            std::string contents = data_micromamba_sh;
            replace_all(contents, "$MAMBA_EXE", exe.string());
            return contents;
        }
        else if (shell == "csh")
        {
            std::string contents = data_micromamba_csh;
            replace_all(contents, "$MAMBA_EXE", exe.string());
            return contents;
        }
        else if (shell == "xonsh")
        {
            std::string contents = data_mamba_xsh;
            replace_all(contents, "$MAMBA_EXE", exe.string());
            return contents;
        }
        else if (shell == "powershell")
        {
            std::stringstream contents;
            contents << "$Env:MAMBA_EXE='" << exe.string() << "'\n";
            std::string psm1 = data_Mamba_psm1;
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
            std::string contents = data_mamba_fish;
            replace_all(contents, "$MAMBA_EXE", exe.string());
            return contents;
        }
        return "";
    }

    void init_root_prefix_cmdexe(const fs::u8path& root_prefix)
    {
        fs::u8path exe = get_self_exe_path();

        try
        {
            fs::create_directories(root_prefix / "condabin");
            fs::create_directories(root_prefix / "Scripts");
        }
        catch (...)
        {
            // Maybe the prefix isn't writable. No big deal, just keep going.
        }

        std::ofstream mamba_bat_f = open_ofstream(root_prefix / "condabin" / "micromamba.bat");
        std::string mamba_bat_contents(data_micromamba_bat);
        replace_all(mamba_bat_contents,
                    std::string("__MAMBA_INSERT_ROOT_PREFIX__"),
                    std::string("@SET \"MAMBA_ROOT_PREFIX=" + root_prefix.string() + "\""));
        replace_all(mamba_bat_contents,
                    std::string("__MAMBA_INSERT_MAMBA_EXE__"),
                    std::string("@SET \"MAMBA_EXE=" + exe.string() + "\""));

        mamba_bat_f << mamba_bat_contents;
        std::ofstream _mamba_activate_bat_f
            = open_ofstream(root_prefix / "condabin" / "_mamba_activate.bat");
        _mamba_activate_bat_f << data__mamba_activate_bat;


        std::string activate_bat_contents(data_activate_bat);
        replace_all(activate_bat_contents,
                    std::string("__MAMBA_INSERT_ROOT_PREFIX__"),
                    std::string("@SET \"MAMBA_ROOT_PREFIX=" + root_prefix.string() + "\""));
        replace_all(activate_bat_contents,
                    std::string("__MAMBA_INSERT_MAMBA_EXE__"),
                    std::string("@SET \"MAMBA_EXE=" + exe.string() + "\""));


        std::ofstream condabin_activate_bat_f
            = open_ofstream(root_prefix / "condabin" / "activate.bat");
        condabin_activate_bat_f << activate_bat_contents;

        std::ofstream scripts_activate_bat_f
            = open_ofstream(root_prefix / "Scripts" / "activate.bat");
        scripts_activate_bat_f << activate_bat_contents;

        std::string hook_content = data_mamba_hook_bat;
        replace_all(hook_content,
                    std::string("__MAMBA_INSERT_MAMBA_EXE__"),
                    std::string("@SET \"MAMBA_EXE=" + exe.string() + "\""));

        std::ofstream mamba_hook_bat_f = open_ofstream(root_prefix / "condabin" / "mamba_hook.bat");
        mamba_hook_bat_f << hook_content;
    }

    void deinit_root_prefix_cmdexe(const fs::u8path& root_prefix)
    {
        if (Context::instance().dry_run)
        {
            return;
        }

        auto micromamba_bat = root_prefix / "condabin" / "micromamba.bat";
        auto _mamba_activate_bat = root_prefix / "condabin" / "_mamba_activate.bat";
        auto condabin_activate_bat = root_prefix / "condabin" / "activate.bat";
        auto scripts_activate_bat = root_prefix / "Scripts" / "activate.bat";
        auto mamba_hook_bat = root_prefix / "condabin" / "mamba_hook.bat";

        for (auto& f : { micromamba_bat,
                         _mamba_activate_bat,
                         condabin_activate_bat,
                         scripts_activate_bat,
                         mamba_hook_bat })
        {
            if (fs::exists(f))
            {
                fs::remove(f);
                LOG_INFO << "Removed " << f << " file.";
            }
            else
            {
                LOG_INFO << "Could not remove " << f << " because it doesn't exist.";
            }
        }

        // remove condabin and Scripts if empty
        auto condabin = root_prefix / "condabin";
        auto scripts = root_prefix / "Scripts";
        for (auto& d : { condabin, scripts })
        {
            if (fs::exists(d) && fs::is_empty(d))
            {
                fs::remove(d);
                LOG_INFO << "Removed " << d << " directory.";
            }
        }
    }

    void init_root_prefix(const std::string& shell, const fs::u8path& root_prefix)
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
            std::ofstream sh_file = open_ofstream(sh_source_path);
            sh_file << data_micromamba_sh;
        }
        else if (shell == "csh")
        {
            CshActivator a;
            auto sh_source_path = a.hook_source_path();
            try
            {
                fs::create_directories(sh_source_path.parent_path());
            }
            catch (...)
            {
                // Maybe the prefix isn't writable. No big deal, just keep going.
            }
            std::ofstream sh_file = open_ofstream(sh_source_path);
            sh_file << data_micromamba_csh;
        }
        else if (shell == "xonsh")
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
            std::ofstream sh_file = open_ofstream(sh_source_path);
            sh_file << data_mamba_xsh;
        }
        else if (shell == "fish")
        {
            FishActivator a;
            auto sh_source_path = a.hook_source_path();
            try
            {
                fs::create_directories(sh_source_path.parent_path());
            }
            catch (...)
            {
                // Maybe the prefix isn't writable. No big deal, just keep going.
            }
            std::ofstream sh_file = open_ofstream(sh_source_path);
            sh_file << data_mamba_fish;
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
            std::ofstream mamba_hook_f = open_ofstream(root_prefix / "condabin" / "mamba_hook.ps1");
            mamba_hook_f << data_mamba_hook_ps1;
            std::ofstream mamba_psm1_f = open_ofstream(root_prefix / "condabin" / "Mamba.psm1");
            mamba_psm1_f << data_Mamba_psm1;
        }
    }

    void deinit_root_prefix(const std::string& shell, const fs::u8path& root_prefix)
    {
        if (Context::instance().dry_run)
        {
            return;
        }

        Context::instance().root_prefix = root_prefix;

        if (shell == "zsh" || shell == "bash" || shell == "posix")
        {
            PosixActivator a;
            auto sh_source_path = a.hook_source_path();

            fs::remove(sh_source_path);
            LOG_INFO << "Removed " << sh_source_path << " file.";
        }
        else if (shell == "csh")
        {
            CshActivator a;
            auto sh_source_path = a.hook_source_path();

            fs::remove(sh_source_path);
            LOG_INFO << "Removed " << sh_source_path << " file.";
        }
        else if (shell == "xonsh")
        {
            XonshActivator a;
            auto sh_source_path = a.hook_source_path();

            fs::remove(sh_source_path);
            LOG_INFO << "Removed " << sh_source_path << " file.";
        }
        else if (shell == "fish")
        {
            FishActivator a;
            auto sh_source_path = a.hook_source_path();

            fs::remove(sh_source_path);
            LOG_INFO << "Removed " << sh_source_path << " file.";
        }
        else if (shell == "cmd.exe")
        {
            deinit_root_prefix_cmdexe(root_prefix);
        }
        else if (shell == "powershell")
        {
            fs::u8path mamba_hook_f = root_prefix / "condabin" / "mamba_hook.ps1";
            fs::remove(mamba_hook_f);
            LOG_INFO << "Removed " << mamba_hook_f << " file.";
            fs::u8path mamba_psm1_f = root_prefix / "condabin" / "Mamba.psm1";
            fs::remove(mamba_psm1_f);
            LOG_INFO << "Removed " << mamba_psm1_f << " file.";

            if (fs::exists(root_prefix / "condabin") && fs::is_empty(root_prefix / "condabin"))
            {
                fs::remove(root_prefix / "condabin");
                LOG_INFO << "Removed " << root_prefix / "condabin"
                         << " directory.";
            }
        }
    }

    std::string powershell_contents(const fs::u8path& conda_prefix)
    {
        fs::u8path self_exe = get_self_exe_path();

        std::stringstream out;

        out << "\n#region mamba initialize\n";
        out << "# !! Contents within this block are managed by 'mamba shell init' !!\n";
        out << "$Env:MAMBA_ROOT_PREFIX = " << conda_prefix << "\n";
        out << "$Env:MAMBA_EXE = " << self_exe << "\n";
        out << "(& " << self_exe << " 'shell' 'hook' -s 'powershell' -p " << conda_prefix
            << ") | Out-String | Invoke-Expression\n";
        out << "#endregion\n";
        return out.str();
    }

    void init_powershell(const fs::u8path& profile_path, const fs::u8path& conda_prefix)
    {
        // NB: the user may not have created a profile. We need to check
        //     if the file exists first.
        std::string profile_content, profile_original_content;
        if (fs::exists(profile_path))
        {
            LOG_INFO << "Found existing PowerShell profile at " << profile_path << ".";
            profile_content = read_contents(profile_path);
            profile_original_content = profile_content;
        }

        std::string conda_init_content = powershell_contents(conda_prefix);

        bool found_mamba_initialize
            = profile_content.find("#region mamba initialize") != std::string::npos;

        // Find what content we need to add.
        Console::stream() << "Adding (or replacing) the following in your " << profile_path
                          << " file\n"
                          << termcolor::colorize << termcolor::green << conda_init_content
                          << termcolor::reset;

        if (found_mamba_initialize)
        {
            LOG_DEBUG << "Found mamba initialize. Replacing mamba initialize block.";
            profile_content = std::regex_replace(
                profile_content, CONDA_INITIALIZE_PS_RE_BLOCK, conda_init_content);
        }

        LOG_DEBUG << "Original profile content:\n" << profile_original_content;
        LOG_DEBUG << "Profile content:\n" << profile_content;

        if (Context::instance().dry_run)
        {
            return;
        }

        if (profile_content != profile_original_content || !found_mamba_initialize)
        {
            if (!fs::exists(profile_path.parent_path()))
            {
                fs::create_directories(profile_path.parent_path());
                LOG_INFO << "Created " << profile_path.parent_path() << " folder.";
            }

            if (!found_mamba_initialize)
            {
                std::ofstream out = open_ofstream(profile_path, std::ios::app | std::ios::binary);
                out << conda_init_content;
            }
            else
            {
                std::ofstream out = open_ofstream(profile_path, std::ios::out | std::ios::binary);
                out << profile_content;
            }

            return;
        }
        return;
    }

    void deinit_powershell(const fs::u8path& profile_path, const fs::u8path& conda_prefix)
    {
        if (!fs::exists(profile_path))
        {
            LOG_INFO << "No existing PowerShell profile at " << profile_path << ".";
            return;
        }

        std::string profile_content = read_contents(profile_path);
        LOG_DEBUG << "Original profile content:\n" << profile_content;

        Console::stream() << "Removing the following in your " << profile_path << " file\n"
                          << termcolor::colorize << termcolor::green
                          << "#region mamba initialize\n...\n#endregion\n"
                          << termcolor::reset;

        profile_content = std::regex_replace(profile_content, CONDA_INITIALIZE_PS_RE_BLOCK, "");
        LOG_DEBUG << "Profile content:\n" << profile_content;

        if (Context::instance().dry_run)
        {
            return;
        }

        if (strip(profile_content).empty())
        {
            fs::remove(profile_path);
            LOG_INFO << "Removed " << profile_path << " file because it's empty.";

            // remove parent folder if it's empty
            fs::u8path parent_path = profile_path.parent_path();
            if (fs::is_empty(parent_path))
            {
                fs::remove(parent_path);
                LOG_INFO << "Removed " << parent_path << " folder because it's empty.";
            }
        }
        else
        {
            std::ofstream out = open_ofstream(profile_path, std::ios::out | std::ios::binary);
            out << profile_content;
        }
    }

    std::string find_powershell_paths(const std::string& exe)
    {
        std::string profile_var("$PROFILE.CurrentUserAllHosts");
        // if (for_system)
        //     profile = "$PROFILE.AllUsersAllHosts"

        // There's several places PowerShell can store its path, depending
        // on if it's Windows PowerShell, PowerShell Core on Windows, or
        // PowerShell Core on macOS/Linux. The easiest way to resolve it is to
        // just ask different possible installations of PowerShell where their
        // profiles are.

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
        catch (const std::exception& ex)
        {
            LOG_DEBUG << "Failed to find PowerShell profile paths: " << ex.what();
            return "";
        }
    }

    void init_shell(const std::string& shell, const fs::u8path& conda_prefix)
    {
        init_root_prefix(shell, conda_prefix);
        auto mamba_exe = get_self_exe_path();
        fs::u8path home = env::home_directory();
        if (shell == "bash")
        {
            // On Linux, when opening the terminal, .bashrc is sourced (because it is an interactive
            // shell).
            // On macOS on the other hand, the .bash_profile gets sourced by default when executing
            // it in Terminal.app. Some other programs do the same on macOS so that's why we're
            // initializing conda in .bash_profile.
            // On Windows, there are multiple ways to open bash depending on how it was installed.
            // Git Bash, Cygwin, and MSYS2 all use .bash_profile by default.
            fs::u8path bashrc_path = (on_mac || on_win) ? home / ".bash_profile" : home / ".bashrc";
            modify_rc_file(bashrc_path, conda_prefix, shell, mamba_exe);
        }
        else if (shell == "zsh")
        {
            fs::u8path zshrc_path = home / ".zshrc";
            modify_rc_file(zshrc_path, conda_prefix, shell, mamba_exe);
        }
        else if (shell == "csh")
        {
            fs::u8path cshrc_path = home / ".tcshrc";
            modify_rc_file(cshrc_path, conda_prefix, shell, mamba_exe);
        }
        else if (shell == "xonsh")
        {
            fs::u8path xonshrc_path = home / ".xonshrc";
            modify_rc_file(xonshrc_path, conda_prefix, shell, mamba_exe);
        }
        else if (shell == "fish")
        {
            fs::u8path fishrc_path = home / ".config" / "fish" / "config.fish";
            modify_rc_file(fishrc_path, conda_prefix, shell, mamba_exe);
        }
        else if (shell == "cmd.exe")
        {
#ifndef _WIN32
            throw std::runtime_error("CMD.EXE can only be initialized on Windows.");
#else
            init_cmd_exe_registry(L"Software\\Microsoft\\Command Processor", conda_prefix);
#endif
        }
        else if (shell == "powershell")
        {
            std::set<std::string> pwsh_profiles;
            for (auto& exe : std::vector<std::string>{ "powershell", "pwsh", "pwsh-preview" })
            {
                auto profile_path = find_powershell_paths(exe);
                if (!profile_path.empty())
                {
                    if (pwsh_profiles.count(profile_path))
                        Console::stream()
                            << exe << " profile already initialized at '" << profile_path << "'";
                    else
                    {
                        pwsh_profiles.insert(profile_path);
                        Console::stream()
                            << "Init " << exe << " profile at '" << profile_path << "'";
                        init_powershell(profile_path, conda_prefix);
                    }
                }
            }
        }
        else
        {
            throw std::runtime_error("Support for other shells not yet implemented.");
        }
#ifdef _WIN32
        enable_long_paths_support(false);
#endif
    }

    void deinit_shell(const std::string& shell, const fs::u8path& conda_prefix)
    {
        auto mamba_exe = get_self_exe_path();
        fs::u8path home = env::home_directory();
        if (shell == "bash")
        {
            fs::u8path bashrc_path = (on_mac || on_win) ? home / ".bash_profile" : home / ".bashrc";
            reset_rc_file(bashrc_path, shell, mamba_exe);
        }
        else if (shell == "zsh")
        {
            fs::u8path zshrc_path = home / ".zshrc";
            reset_rc_file(zshrc_path, shell, mamba_exe);
        }
        else if (shell == "xonsh")
        {
            fs::u8path xonshrc_path = home / ".xonshrc";
            reset_rc_file(xonshrc_path, shell, mamba_exe);
        }
        else if (shell == "csh")
        {
            fs::u8path tcshrc_path = home / ".tcshrc";
            reset_rc_file(tcshrc_path, shell, mamba_exe);
        }
        else if (shell == "fish")
        {
            fs::u8path fishrc_path = home / ".config" / "fish" / "config.fish";
            reset_rc_file(fishrc_path, shell, mamba_exe);
        }
        else if (shell == "cmd.exe")
        {
#ifndef _WIN32
            throw std::runtime_error("CMD.EXE can only be deinitialized on Windows.");
#else
            deinit_cmd_exe_registry(L"Software\\Microsoft\\Command Processor", conda_prefix);
#endif
        }
        else if (shell == "powershell")
        {
            std::set<std::string> pwsh_profiles;
            for (auto& exe : std::vector<std::string>{ "powershell", "pwsh", "pwsh-preview" })
            {
                auto profile_path = find_powershell_paths(exe);
                if (!profile_path.empty())
                {
                    Console::stream() << "Deinit " << exe << " profile at '" << profile_path << "'";
                    deinit_powershell(profile_path, conda_prefix);
                }
            }
        }
        else
        {
            throw std::runtime_error("Support for other shells not yet implemented.");
        }

        deinit_root_prefix(shell, conda_prefix);
    }
}
