// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <regex>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include <fmt/color.h>
#include <fmt/format.h>
#include <fmt/ostream.h>
#include <fmt/xchar.h>
#include <reproc++/run.hpp>
#ifdef _WIN32
#include <WinReg.hpp>

#include "mamba/util/os_win.hpp"
#endif

#include "mamba/core/activation.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/shell_init.hpp"
#include "mamba/core/util.hpp"
#include "mamba/core/util_os.hpp"
#include "mamba/util/build.hpp"
#include "mamba/util/environment.hpp"
#include "mamba/util/path_manip.hpp"
#include "mamba/util/string.hpp"

namespace mamba
{
    namespace
    {
        static const std::regex MAMBA_INITIALIZE_RE_BLOCK("\n?# >>> mamba initialize >>>(?:\n|\r\n)?"
                                                          "([\\s\\S]*?)"
                                                          "# <<< mamba initialize <<<(?:\n|\r\n)?");

        static const std::regex MAMBA_INITIALIZE_PS_RE_BLOCK("\n?#region mamba initialize(?:\n|\r\n)?"
                                                             "([\\s\\S]*?)"
                                                             "#endregion(?:\n|\r\n)?");
    }

    std::string guess_shell()
    {
        std::string parent_process_name = get_process_name_by_pid(getppid());

        LOG_DEBUG << "Guessing shell. Parent process name: " << parent_process_name;

        std::string parent_process_name_lower = util::to_lower(parent_process_name);

        if (util::contains(parent_process_name_lower, "bash"))
        {
            return "bash";
        }
        if (util::contains(parent_process_name_lower, "zsh"))
        {
            return "zsh";
        }
        if (util::contains(parent_process_name_lower, "csh"))
        {
            return "csh";
        }
        if (util::contains(parent_process_name_lower, "dash"))
        {
            return "dash";
        }
        if (util::contains(parent_process_name, "nu"))
        {
            return "nu";
        }

        // xonsh in unix, Python in macOS
        if (util::contains(parent_process_name_lower, "python"))
        {
            Console::stream() << "Your parent process name is " << parent_process_name
                              << ".\nIf your shell is xonsh, please use \"-s xonsh\".";
        }
        if (util::contains(parent_process_name_lower, "xonsh"))
        {
            return "xonsh";
        }
        if (util::contains(parent_process_name_lower, "cmd.exe"))
        {
            return "cmd.exe";
        }
        if (util::contains(parent_process_name_lower, "powershell")
            || util::contains(parent_process_name_lower, "pwsh"))
        {
            return "powershell";
        }
        if (util::contains(parent_process_name_lower, "fish"))
        {
            return "fish";
        }
        return "";
    }

    namespace  // Windows-specific but must be available for cli on all platforms
    {
        struct RunInfo  // FIXME: find a better name
        {
            fs::u8path this_exe_path = get_self_exe_path();
            fs::u8path this_exe_name_path = this_exe_path.stem();
            std::string this_exe_name = this_exe_name_path;
            std::wregex regex_mamba_cmd_hook{ L"(\"[^\"]*?" + this_exe_name_path.wstring()
                                                  + L"[-_] hook\\.bat\")",  // TODO: replace by fmt?
                                              std::regex_constants::icase };
        };

        const RunInfo& run_info()  // FIXME: find a better name
        {
            static const RunInfo info;
            return info;
        }

        struct ShellInitPathsWindowsCmd
        {
            fs::u8path condabin;
            fs::u8path scripts;

            fs::u8path mamba_bat;
            fs::u8path _mamba_activate_bat;
            fs::u8path condabin_activate_bat;
            fs::u8path scripts_activate_bat;
            fs::u8path mamba_hook_bat;

            explicit ShellInitPathsWindowsCmd(fs::u8path root_prefix)
                : condabin(root_prefix / "condabin")
                , scripts(root_prefix / "Scripts")
                , mamba_bat(condabin / (run_info().this_exe_name + ".bat"))
                , _mamba_activate_bat(condabin / ("_" + run_info().this_exe_name + "_activate.bat"))
                , condabin_activate_bat(condabin / "activate.bat")
                , scripts_activate_bat(scripts / "activate.bat")
                , mamba_hook_bat(condabin / "mamba_hook.bat")
            {
            }

            auto every_generated_files_paths() const -> std::vector<fs::u8path>
            {
                return { mamba_bat,
                         _mamba_activate_bat,
                         condabin_activate_bat,
                         scripts_activate_bat,
                         mamba_hook_bat };
            }

            auto every_generated_directories_paths() const -> std::vector<fs::u8path>
            {
                return { condabin, scripts };
            }
        };


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

    void set_autorun_registry_key(
        const std::wstring& reg_path,
        const std::wstring& value,
        const Context::GraphicsParams& graphics
    )
    {
        auto out = Console::stream();
        fmt::print(
            out,
            "Setting cmd.exe AUTORUN to: {}",
            fmt::styled(util::windows_encoding_to_utf8(value), graphics.palette.success)
        );

        winreg::RegKey key{ HKEY_CURRENT_USER, reg_path };
        key.SetStringValue(L"AutoRun", value);
    }

    std::wstring get_hook_string(const fs::u8path& conda_prefix)
    {
        const ShellInitPathsWindowsCmd paths{ conda_prefix };
        auto hook_path = fs::canonical(paths.mamba_hook_bat).std_path();
        return fmt::format(LR"("{}")", hook_path.make_preferred().wstring());
    }

    void
    init_cmd_exe_registry(const Context& context, const std::wstring& reg_path, const fs::u8path& conda_prefix)
    {
        std::wstring prev_value = get_autorun_registry_key(reg_path);
        std::wstring hook_string = get_hook_string(conda_prefix);

        // modify registry key
        std::wstring replace_str(L"__CONDA_REPLACE_ME_123__");
        std::wstring replaced_value = std::regex_replace(
            prev_value,
            run_info().regex_mamba_cmd_hook,
            replace_str,
            std::regex_constants::format_first_only
        );

        std::wstring new_value = replaced_value;

        if (replaced_value.find(replace_str) == std::wstring::npos)
        {
            if (!new_value.empty())
            {
                if (new_value.find(hook_string) == std::wstring::npos)
                {
                    new_value += L" & " + hook_string;
                }
                // else the hook path already exists in the string
            }
            else
            {
                new_value = hook_string;
            }
        }
        else
        {
            util::replace_all(new_value, replace_str, hook_string);
        }

        // set modified registry key
        if (new_value != prev_value)
        {
            set_autorun_registry_key(reg_path, new_value, context.graphics_params);
        }
        else
        {
            auto out = Console::stream();
            fmt::print(
                out,
                "{}",
                fmt::styled("cmd.exe already initialized.", context.graphics_params.palette.success)
            );
        }
    }

    void deinit_cmd_exe_registry(
        const std::wstring& reg_path,
        const fs::u8path& conda_prefix,
        const Context::GraphicsParams& graphics
    )
    {
        std::wstring prev_value = get_autorun_registry_key(reg_path);
        std::wstring hook_string = get_hook_string(conda_prefix);

        // modify registry key
        // remove the mamba hook from the autorun list
        std::wstringstream stringstream(prev_value);
        std::wstring segment;
        std::vector<std::wstring> autorun_list;

        autorun_list = util::split(std::wstring_view(prev_value), std::wstring_view(L"&"));

        // remove the mamba hook from the autorun list
        autorun_list.erase(
            std::remove_if(
                autorun_list.begin(),
                autorun_list.end(),
                [&hook_string](const std::wstring& s) { return util::strip(s) == hook_string; }
            ),
            autorun_list.end()
        );

        // join the list back into a string
        std::wstring new_value = util::join(L" & ", autorun_list);

        // set modified registry key
        if (new_value != prev_value)
        {
            set_autorun_registry_key(reg_path, new_value, graphics);
        }
        else
        {
            auto out = Console::stream();
            fmt::print(out, "{}", fmt::styled("cmd.exe not initialized yet.", graphics.palette.success));
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
        if (util::contains(parent_process_name.filename().string(), "bash"))
        {
            bash = parent_process_name;
        }
        else
        {
            bash = util::which("bash");
        }
        const std::string command = bash.empty() ? "cygpath"
                                                 : (bash.parent_path() / "cygpath").string();
        std::string out, err;
        try
        {
            std::vector<std::string> args{ command, path };
            if (is_a_path_env)
            {
                args.push_back("--path");
            }
            auto [status, ec] = reproc::run(
                args,
                reproc::options{},
                reproc::sink::string(out),
                reproc::sink::string(err)
            );
            if (ec)
            {
                throw std::runtime_error(ec.message());
            }
            return std::string(util::strip(out));
        }
        catch (...)
        {
            throw std::runtime_error(
                "Could not find bash, or use cygpath to convert Windows path to Unix."
            );
        }
    }

    std::string
    rcfile_content_unix(const fs::u8path& env_prefix, std::string_view shell, const fs::u8path& mamba_exe)
    {
        // Note that fs::path are already quoted by fmt.
        return fmt::format(
            "\n"
            "# >>> mamba initialize >>>\n"
            "# !! Contents within this block are managed by '{mamba_exe_name} shell init' !!\n"
            "export MAMBA_EXE={mamba_exe_path};\n"
            "export MAMBA_ROOT_PREFIX={root_prefix};\n"
            R"sh(__mamba_setup="$("$MAMBA_EXE" shell hook --shell {shell} --root-prefix "$MAMBA_ROOT_PREFIX" 2> /dev/null)")sh"
            "\n"
            "if [ $? -eq 0 ]; then\n"
            "    eval \"$__mamba_setup\"\n"
            "else\n"
            R"sh(    alias {mamba_exe_name}="$MAMBA_EXE"  # Fallback on help from {mamba_exe_name} activate)sh"
            "\n"
            "fi\n"
            "unset __mamba_setup\n"
            "# <<< mamba initialize <<<\n",
            fmt::arg("mamba_exe_path", mamba_exe),
            fmt::arg("mamba_exe_name", mamba_exe.stem().string()),
            fmt::arg("root_prefix", env_prefix),
            fmt::arg("shell", shell)
        );
    }

    std::string
    rcfile_content_win(const fs::u8path& env_prefix, std::string_view shell, const fs::u8path& mamba_exe)
    {
        return fmt::format(
            "\n"
            "# >>> mamba initialize >>>\n"
            "# !! Contents within this block are managed by '{mamba_exe_name} shell init' !!\n"
            R"(export MAMBA_EXE="{mamba_exe_path}";)"
            "\n"
            R"(export MAMBA_ROOT_PREFIX="{root_prefix}";)"
            "\n"
            R"sh(eval "$("$MAMBA_EXE" shell hook --shell {shell} --root-prefix "$MAMBA_ROOT_PREFIX")")sh"
            "\n"
            "# <<< mamba initialize <<<\n",
            fmt::arg("mamba_exe_path", native_path_to_unix(mamba_exe.string())),
            fmt::arg("mamba_exe_name", mamba_exe.stem().string()),
            fmt::arg("root_prefix", native_path_to_unix(env_prefix.string())),
            fmt::arg("shell", shell)
        );
    }

    std::string
    rcfile_content(const fs::u8path& env_prefix, std::string_view shell, const fs::u8path& mamba_exe)
    {
        if (util::on_win)
        {
            return rcfile_content_win(env_prefix, shell, mamba_exe);
        }
        return rcfile_content_unix(env_prefix, shell, mamba_exe);
    }

    std::string
    xonsh_content(const fs::u8path& env_prefix, const std::string& /*shell*/, const fs::u8path& mamba_exe)
    {
        std::string s_mamba_exe;
        if (util::on_win)
        {
            s_mamba_exe = native_path_to_unix(mamba_exe.string());
        }
        else
        {
            s_mamba_exe = mamba_exe.string();
        }

        const auto exe_name = mamba_exe.stem().string();

        std::stringstream content;
        content << "\n# >>> mamba initialize >>>\n";
        content << "# !! Contents within this block are managed by '" << exe_name
                << " shell init' !!\n";
        content << "$MAMBA_EXE = " << mamba_exe << "\n";
        content << "$MAMBA_ROOT_PREFIX = " << env_prefix << "\n";
        content << "import sys as _sys\n";
        content << "from types import ModuleType as _ModuleType\n";
        content << "_mod = _ModuleType(\"xontrib.mamba\",\n";
        content << "                   \'Autogenerated from $($MAMBA_EXE shell hook -s xonsh -r $MAMBA_ROOT_PREFIX)\')\n";
        content << "__xonsh__.execer.exec($($MAMBA_EXE shell hook -s xonsh -r $MAMBA_ROOT_PREFIX),\n";
        content << "                      glbs=_mod.__dict__,\n";
        content << "                      filename=\'$($MAMBA_EXE shell hook -s xonsh -r $MAMBA_ROOT_PREFIX)\')\n";
        content << "_sys.modules[\"xontrib.mamba\"] = _mod\n";
        content << "del _sys, _mod, _ModuleType\n";
        content << "# <<< mamba initialize <<<\n";
        return content.str();
    }

    std::string
    fish_content(const fs::u8path& env_prefix, const std::string& /*shell*/, const fs::u8path& mamba_exe)
    {
        std::string s_mamba_exe;
        if (util::on_win)
        {
            s_mamba_exe = native_path_to_unix(mamba_exe.string());
        }
        else
        {
            s_mamba_exe = mamba_exe.string();
        }

        const auto exe_name = mamba_exe.stem().string();

        std::stringstream content;
        content << "\n# >>> mamba initialize >>>\n";
        content << "# !! Contents within this block are managed by '" << exe_name
                << " shell init' !!\n";
        content << "set -gx MAMBA_EXE " << mamba_exe << "\n";
        content << "set -gx MAMBA_ROOT_PREFIX " << env_prefix << "\n";
        content << "$MAMBA_EXE shell hook --shell fish --root-prefix $MAMBA_ROOT_PREFIX | source\n";
        content << "# <<< mamba initialize <<<\n";
        return content.str();
    }

    std::string
    nu_content(const fs::u8path& env_prefix, const std::string& /*shell*/, const fs::u8path& mamba_exe)
    {
        std::string s_mamba_exe;
        if (util::on_win)
        {
            s_mamba_exe = native_path_to_unix(mamba_exe.string());
        }
        else
        {
            s_mamba_exe = mamba_exe.string();
        }

        const auto exe_name = mamba_exe.stem().string();

        std::stringstream content;
        content << "\n# >>> mamba initialize >>>\n";
        content << "# !! Contents within this block are managed by '" << exe_name
                << " shell init' !!\n";
        content << "$env.MAMBA_EXE = " << mamba_exe << "\n";
        content << "$env.MAMBA_ROOT_PREFIX = " << env_prefix << "\n";
        content << "$env.PATH = ($env.PATH | append ([$env.MAMBA_ROOT_PREFIX bin] | path join) | uniq)\n";
        content << "$env.PROMPT_COMMAND_BK = $env.PROMPT_COMMAND" << "\n";
        // TODO the following shouldn't live here but in a shell hook
        content << R"nu(def --env ")nu" << exe_name << R"nu( activate"  [name: string] {)nu";
        content << R"###(
    #add condabin when base env
    if $env.MAMBA_SHLVL? == null {
        $env.MAMBA_SHLVL = 0
        $env.PATH = ($env.PATH | prepend $"($env.MAMBA_ROOT_PREFIX)/condabin")
    }
    #ask mamba how to setup the environment and set the environment
    (^($env.MAMBA_EXE) shell activate --shell nu $name
      | str replace --regex '\s+' '' --all
      | split row ";"
      | parse --regex '(.*)=(.+)'
      | transpose --header-row
      | into record
      | load-env
    )
    # update prompt
    if ($env.CONDA_PROMPT_MODIFIER? != null) {
      $env.PROMPT_COMMAND = {|| $env.CONDA_PROMPT_MODIFIER + (do $env.PROMPT_COMMAND_BK)}
    }
})###" << "\n";

        content << R"nu(def --env ")nu" << exe_name << R"nu( deactivate"  [] {)nu";
        content << R"###(
    #remove active environment except base env
    if $env.CONDA_PROMPT_MODIFIER? != null {
      # unset set variables
      for x in (^$env.MAMBA_EXE shell deactivate --shell nu
              | split row ";") {
          if ("hide-env" in $x) {
            hide-env ($x | parse "hide-env {var}").var.0
          } else if $x != "" {
            let keyValue = ($x
            | str replace --regex '\s+' "" --all
            | parse '{key}={value}'
            )
            load-env {$keyValue.0.key: $keyValue.0.value}
          }
    }
    # reset prompt
    $env.PROMPT_COMMAND = $env.PROMPT_COMMAND_BK
  }
})###" << "\n";
        content << "# <<< mamba initialize <<<\n";
        return content.str();
    }

    std::string
    csh_content(const fs::u8path& env_prefix, const std::string& /*shell*/, const fs::u8path& mamba_exe)
    {
        std::string s_mamba_exe;
        if (util::on_win)
        {
            s_mamba_exe = native_path_to_unix(mamba_exe.string());
        }
        else
        {
            s_mamba_exe = mamba_exe.string();
        }

        const auto exe_name = mamba_exe.stem().string();

        std::stringstream content;
        content << "\n# >>> mamba initialize >>>\n";
        content << "# !! Contents within this block are managed by '" << exe_name
                << " shell init' !!\n";
        content << "setenv MAMBA_EXE " << mamba_exe << ";\n";
        content << "setenv MAMBA_ROOT_PREFIX " << env_prefix << ";\n";
        content << "source $MAMBA_ROOT_PREFIX/etc/profile.d/mamba.csh;\n";
        content << "# <<< mamba initialize <<<\n";
        return content.str();
    }

    void modify_rc_file(
        const Context& context,
        const fs::u8path& file_path,
        const fs::u8path& conda_prefix,
        const std::string& shell,
        const fs::u8path& mamba_exe
    )
    {
        auto out = Console::stream();
        fmt::print(
            out,
            "Modifying RC file {}\n"
            "Generating config for root prefix {}\n"
            "Setting mamba executable to: {}\n",
            fmt::streamed(file_path),
            fmt::styled(fmt::streamed(conda_prefix), fmt::emphasis::bold),
            fmt::styled(fmt::streamed(mamba_exe), fmt::emphasis::bold)
        );

        // TODO do we need binary or not?
        std::string conda_init_content, rc_content;
        if (fs::exists(file_path))
        {
            rc_content = read_contents(file_path, std::ios::in);
        }
        else
        {
            // Ensure base directory of config file exists (in case it is under ~/.config)
            fs::create_directories(file_path.parent_path());
        }

        if (shell == "xonsh")
        {
            conda_init_content = xonsh_content(conda_prefix, shell, mamba_exe);
        }
        else if (shell == "fish")
        {
            conda_init_content = fish_content(conda_prefix, shell, mamba_exe);
        }
        else if (shell == "nu")
        {
            conda_init_content = nu_content(conda_prefix, shell, mamba_exe);
        }
        else if (shell == "csh")
        {
            conda_init_content = csh_content(conda_prefix, shell, mamba_exe);
        }
        else
        {
            conda_init_content = rcfile_content(conda_prefix, shell, mamba_exe);
        }

        fmt::print(
            out,
            "Adding (or replacing) the following in your {} file\n{}",
            fmt::streamed(file_path),
            fmt::styled(conda_init_content, context.graphics_params.palette.success)
        );

        if (context.dry_run)
        {
            return;
        }

        std::string result = std::regex_replace(rc_content, MAMBA_INITIALIZE_RE_BLOCK, conda_init_content);

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

    void
    reset_rc_file(const Context& context, const fs::u8path& file_path, const std::string&, const fs::u8path&)
    {
        Console::stream() << "Resetting RC file " << file_path << "\nDeleting config for root prefix "
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

        auto out = Console::stream();
        fmt::print(
            out,
            "Removing the following in your {} file\n{}",
            fmt::streamed(file_path),
            fmt::styled(
                "# >>> mamba initialize >>>\n...\n# <<< mamba initialize <<<",
                context.graphics_params.palette.success
            )
        );

        if (rc_content.find("# >>> mamba initialize >>>") == std::string::npos)
        {
            LOG_INFO << "No mamba initialize block found, nothing to do.";
            return;
        }

        std::string result = std::regex_replace(rc_content, MAMBA_INITIALIZE_RE_BLOCK, "");

        if (context.dry_run)
        {
            return;
        }

        std::ofstream rc_file = open_ofstream(file_path, std::ios::out | std::ios::binary);
        rc_file << result;
    }

    std::string get_hook_contents(const Context& context, const std::string& shell)
    {
        fs::u8path exe = get_self_exe_path();

        if (shell == "zsh" || shell == "bash" || shell == "posix")
        {
            std::string contents = data_mamba_sh;
            // Using /unix/like/paths on Unix shell (even on Windows)
            util::replace_all(contents, "${MAMBA_EXE}", util::path_to_posix(exe.string()));
            return contents;
        }
        else if (shell == "csh")
        {
            std::string contents = data_mamba_csh;
            // Using /unix/like/paths on Unix shell (even on Windows)
            util::replace_all(contents, "$MAMBA_EXE", util::path_to_posix(exe.string()));
            return contents;
        }
        else if (shell == "xonsh")
        {
            std::string contents = data_mamba_xsh;
            // Using /unix/like/paths on Unix shell (even on Windows)
            util::replace_all(contents, "$MAMBA_EXE", util::path_to_posix(exe.string()));
            return contents;
        }
        else if (shell == "powershell")
        {
            std::stringstream contents;
            contents << "$Env:MAMBA_EXE='" << exe.string() << "'\n";
            std::string psm1 = data_Mamba_psm1;
            auto begin = psm1.find("## AFTER PARAM ##");
            auto end = psm1.find("## EXPORTS ##");
            psm1 = psm1.substr(begin, end - begin);
            contents << psm1;
            return contents.str();
        }
        else if (shell == "cmd.exe")
        {
            init_root_prefix_cmdexe(context, context.prefix_params.root_prefix);
            LOG_WARNING << "Hook installed, now 'manually' execute:";
            LOG_WARNING
                << "       CALL "
                << std::quoted(
                       (context.prefix_params.root_prefix / "condabin" / "mamba_hook.bat").string()
                   );
        }
        else if (shell == "fish")
        {
            std::string contents = data_mamba_fish;
            // Using /unix/like/paths on Unix shell (even on Windows)
            util::replace_all(contents, "$MAMBA_EXE", util::path_to_posix(exe.string()));
            return contents;
        }
        else if (shell == "nu")
        {
            // not implemented due to inability in nu to evaluate dynamically generated code
            return "";
        }
        return "";
    }

    void init_root_prefix_cmdexe(const Context&, const fs::u8path& root_prefix)
    {
        const ShellInitPathsWindowsCmd paths{ root_prefix };

        for (const auto directory : paths.every_generated_directories_paths())
        {
            // Maybe the prefix isn't writable. No big deal, just keep going.
            std::error_code maybe_error [[maybe_unused]];
            fs::create_directories(directory, maybe_error);
            if (maybe_error)
            {
                LOG_ERROR << "Failed to create directory '" << directory.string()
                          << "' : " << maybe_error.message();
            }
        }


        const auto replace_insert_root_prefix = [&](auto& text)
        {
            return util::replace_all(
                text,
                std::string("__MAMBA_DEFINE_ROOT_PREFIX__"),
                "@SET \"MAMBA_ROOT_PREFIX=" + root_prefix.string() + "\""
            );
        };

        const auto replace_insert_mamba_exe = [&](auto& text)
        {
            return util::replace_all(
                text,
                std::string("__MAMBA_DEFINE_MAMBA_EXE__"),
                "@SET \"MAMBA_EXE=" + run_info().this_exe_path.string() + "\""
            );
        };

        static const auto MARKER_INSERT_EXE_NAME = std::string("__MAMBA_INSERT_EXE_NAME__");
        static const auto MARKER_INSERT_MAMBA_BAT_NAME = std::string("__MAMBA_INSERT_BAT_NAME__");

        // mamba.bat
        std::string mamba_bat_contents(data_mamba_bat);
        replace_insert_root_prefix(mamba_bat_contents);
        replace_insert_mamba_exe(mamba_bat_contents);
        static const auto MARKER_MAMBA_INSERT_ACTIVATE_BAT_NAME = std::string(
            "__MAMBA_INSERT_ACTIVATE_BAT_NAME__"
        );
        util::replace_all(
            mamba_bat_contents,
            MARKER_MAMBA_INSERT_ACTIVATE_BAT_NAME,
            paths._mamba_activate_bat.stem().string()
        );
        std::ofstream mamba_bat_f = open_ofstream(paths.mamba_bat);
        mamba_bat_f << mamba_bat_contents;

        // _mamba_activate.bat
        std::ofstream _mamba_activate_bat_f = open_ofstream(paths._mamba_activate_bat);
        _mamba_activate_bat_f << data__mamba_activate_bat;

        // condabin/activate.bat
        std::string activate_bat_contents(data_activate_bat);
        replace_insert_root_prefix(activate_bat_contents);
        replace_insert_mamba_exe(activate_bat_contents);
        util::replace_all(activate_bat_contents, MARKER_INSERT_EXE_NAME, run_info().this_exe_name);
        static const auto MARKER_MAMBA_INSERT_HOOK_BAT_NAME = std::string(
            "__MAMBA_INSERT_HOOK_BAT_NAME__"
        );
        util::replace_all(
            activate_bat_contents,
            MARKER_MAMBA_INSERT_HOOK_BAT_NAME,
            paths.mamba_hook_bat.filename().string()
        );
        std::ofstream condabin_activate_bat_f = open_ofstream(paths.condabin_activate_bat);
        condabin_activate_bat_f << activate_bat_contents;

        // Scripts/activate.bat
        std::ofstream scripts_activate_bat_f = open_ofstream(paths.scripts_activate_bat);
        scripts_activate_bat_f << activate_bat_contents;

        // mamba_hook.bat
        std::string hook_content = data_mamba_hook_bat;
        replace_insert_mamba_exe(hook_content);
        util::replace_all(hook_content, MARKER_INSERT_EXE_NAME, run_info().this_exe_name);
        util::replace_all(
            hook_content,
            MARKER_INSERT_MAMBA_BAT_NAME,
            paths.mamba_bat.filename().string()
        );

        std::ofstream mamba_hook_bat_f = open_ofstream(paths.mamba_hook_bat);
        mamba_hook_bat_f << hook_content;
    }

    void deinit_root_prefix_cmdexe(const Context& context, const fs::u8path& root_prefix)
    {
        if (context.dry_run)
        {
            return;
        }

        const ShellInitPathsWindowsCmd paths{ root_prefix };

        for (auto& f : paths.every_generated_files_paths())
        {
            if (fs::remove(f))
            {
                LOG_INFO << "Removed " << f << " file.";
            }
            else
            {
                LOG_INFO << "Could not remove " << f << " because it doesn't exist.";
            }
        }

        // remove condabin and Scripts if empty
        for (auto& d : paths.every_generated_directories_paths())
        {
            if (fs::is_empty(d))
            {
                if (fs::remove(d))
                {
                    LOG_INFO << "Removed " << d << " directory.";
                }
            }
        }
    }

    void init_root_prefix(Context& context, const std::string& shell, const fs::u8path& root_prefix)
    {
        context.prefix_params.root_prefix = root_prefix;

        if (!fs::exists(root_prefix))
        {
            fs::create_directories(root_prefix / "conda-meta");
        }

        if (shell == "zsh" || shell == "bash" || shell == "posix")
        {
            PosixActivator activator{ context };
            auto sh_source_path = activator.hook_source_path();
            try
            {
                fs::create_directories(sh_source_path.parent_path());
            }
            catch (...)
            {
                // Maybe the prefix isn't writable. No big deal, just keep going.
            }
            std::ofstream sh_file = open_ofstream(sh_source_path);
            sh_file << data_mamba_sh;
        }
        else if (shell == "csh")
        {
            CshActivator activator{ context };
            auto sh_source_path = activator.hook_source_path();
            try
            {
                fs::create_directories(sh_source_path.parent_path());
            }
            catch (...)
            {
                // Maybe the prefix isn't writable. No big deal, just keep going.
            }
            std::ofstream sh_file = open_ofstream(sh_source_path);
            sh_file << data_mamba_csh;
        }
        else if (shell == "xonsh")
        {
            XonshActivator activator{ context };
            auto sh_source_path = activator.hook_source_path();
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
            FishActivator activator{ context };
            auto sh_source_path = activator.hook_source_path();
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
        else if (shell == "nu")
        {
            NuActivator a{ context };
            auto sh_source_path = a.hook_source_path();
            try
            {
                fs::create_directories(sh_source_path.parent_path());
            }
            catch (...)
            {
                // no hooks are used since nu cannot
                // dynamically load; no big deal, keep going.
            }
        }
        else if (shell == "cmd.exe")
        {
            init_root_prefix_cmdexe(context, root_prefix);
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

    void deinit_root_prefix(Context& context, const std::string& shell, const fs::u8path& root_prefix)
    {
        if (context.dry_run)
        {
            return;
        }

        context.prefix_params.root_prefix = root_prefix;

        if (shell == "zsh" || shell == "bash" || shell == "posix")
        {
            PosixActivator activator{ context };
            auto sh_source_path = activator.hook_source_path();

            fs::remove(sh_source_path);
            LOG_INFO << "Removed " << sh_source_path << " file.";
        }
        else if (shell == "csh")
        {
            CshActivator activator{ context };
            auto sh_source_path = activator.hook_source_path();

            fs::remove(sh_source_path);
            LOG_INFO << "Removed " << sh_source_path << " file.";
        }
        else if (shell == "xonsh")
        {
            XonshActivator activator{ context };
            auto sh_source_path = activator.hook_source_path();

            fs::remove(sh_source_path);
            LOG_INFO << "Removed " << sh_source_path << " file.";
        }
        else if (shell == "fish")
        {
            FishActivator activator{ context };
            auto sh_source_path = activator.hook_source_path();

            fs::remove(sh_source_path);
            LOG_INFO << "Removed " << sh_source_path << " file.";
        }
        else if (shell == "cmd.exe")
        {
            deinit_root_prefix_cmdexe(context, root_prefix);
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
                LOG_INFO << "Removed " << root_prefix / "condabin" << " directory.";
            }
        }
    }

    std::string powershell_contents(const fs::u8path& conda_prefix)
    {
        fs::u8path self_exe = get_self_exe_path();

        std::stringstream out;

        out << "\n#region mamba initialize\n";
        out << "# !! Contents within this block are managed by 'mamba shell init' !!\n";
        out << "$Env:MAMBA_ROOT_PREFIX = \"" << conda_prefix.string() << "\"\n";
        out << "$Env:MAMBA_EXE = \"" << self_exe.string() << "\"\n";
        out << "(& $Env:MAMBA_EXE 'shell' 'hook' -s 'powershell' -r $Env:MAMBA_ROOT_PREFIX) | Out-String | Invoke-Expression\n";
        out << "#endregion\n";
        return out.str();
    }

    void
    init_powershell(const Context& context, const fs::u8path& profile_path, const fs::u8path& conda_prefix)
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

        bool found_mamba_initialize = profile_content.find("#region mamba initialize")
                                      != std::string::npos;

        // Find what content we need to add.
        auto out = Console::stream();
        fmt::print(
            out,
            "Adding (or replacing) the following in your {} file\n{}",
            fmt::streamed(profile_path),
            fmt::styled(conda_init_content, context.graphics_params.palette.success)
        );

        if (found_mamba_initialize)
        {
            LOG_DEBUG << "Found mamba initialize. Replacing mamba initialize block.";
            profile_content = std::regex_replace(
                profile_content,
                MAMBA_INITIALIZE_PS_RE_BLOCK,
                conda_init_content
            );
        }

        LOG_DEBUG << "Original profile content:\n" << profile_original_content;
        LOG_DEBUG << "Profile content:\n" << profile_content;

        if (context.dry_run)
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
                std::ofstream lout = open_ofstream(profile_path, std::ios::app | std::ios::binary);
                lout << conda_init_content;
            }
            else
            {
                std::ofstream lout = open_ofstream(profile_path, std::ios::out | std::ios::binary);
                lout << profile_content;
            }

            return;
        }
        return;
    }

    void deinit_powershell(const Context& context, const fs::u8path& profile_path, const fs::u8path&)
    {
        if (!fs::exists(profile_path))
        {
            LOG_INFO << "No existing PowerShell profile at " << profile_path << ".";
            return;
        }

        std::string profile_content = read_contents(profile_path);
        LOG_DEBUG << "Original profile content:\n" << profile_content;

        {
            auto out = Console::stream();
            fmt::print(
                out,
                "Removing the following in your {} file\n{}",
                fmt::streamed(profile_path),
                fmt::styled(
                    "#region mamba initialize\n...\n#endregion\n",
                    context.graphics_params.palette.success
                )
            );
        }

        profile_content = std::regex_replace(profile_content, MAMBA_INITIALIZE_PS_RE_BLOCK, "");
        LOG_DEBUG << "Profile content:\n" << profile_content;

        if (context.dry_run)
        {
            return;
        }

        if (util::strip(profile_content).empty())
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
                reproc::sink::string(err)
            );
            if (ec)
            {
                throw std::runtime_error(ec.message());
            }
            return std::string(util::strip(out));
        }
        catch (const std::exception& ex)
        {
            LOG_DEBUG << "Failed to find PowerShell profile paths: " << ex.what();
            return "";
        }
    }

    void init_shell(Context& context, const std::string& shell, const fs::u8path& conda_prefix)
    {
        init_root_prefix(context, shell, conda_prefix);
        auto mamba_exe = get_self_exe_path();
        fs::u8path home = util::user_home_dir();
        if (shell == "bash")
        {
            // On Linux, when opening the terminal, .bashrc is sourced (because it is an interactive
            // shell).
            // On macOS on the other hand, the .bash_profile gets sourced by default when executing
            // it in Terminal.app. Some other programs do the same on macOS so that's why we're
            // initializing conda in .bash_profile.
            // On Windows, there are multiple ways to open bash depending on how it was installed.
            // Git Bash, Cygwin, and MSYS2 all use .bash_profile by default.
            fs::u8path bashrc_path = (util::on_mac || util::on_win) ? home / ".bash_profile"
                                                                    : home / ".bashrc";
            modify_rc_file(context, bashrc_path, conda_prefix, shell, mamba_exe);
        }
        else if (shell == "zsh")
        {
            fs::u8path zshrc_path = home / ".zshrc";
            modify_rc_file(context, zshrc_path, conda_prefix, shell, mamba_exe);
        }
        else if (shell == "csh")
        {
            fs::u8path cshrc_path = home / ".tcshrc";
            modify_rc_file(context, cshrc_path, conda_prefix, shell, mamba_exe);
        }
        else if (shell == "xonsh")
        {
            fs::u8path xonshrc_path = home / ".xonshrc";
            modify_rc_file(context, xonshrc_path, conda_prefix, shell, mamba_exe);
        }
        else if (shell == "fish")
        {
            fs::u8path fishrc_path = home / ".config" / "fish" / "config.fish";
            modify_rc_file(context, fishrc_path, conda_prefix, shell, mamba_exe);
        }
        else if (shell == "nu")
        {
            fs::u8path nu_config_path = home / ".config" / "nushell" / "config.nu";
            modify_rc_file(context, nu_config_path, conda_prefix, shell, mamba_exe);
        }
        else if (shell == "cmd.exe")
        {
#ifndef _WIN32
            throw std::runtime_error("CMD.EXE can only be initialized on Windows.");
#else
            init_cmd_exe_registry(context, L"Software\\Microsoft\\Command Processor", conda_prefix);
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
                    {
                        Console::stream()
                            << exe << " profile already initialized at '" << profile_path << "'";
                    }
                    else
                    {
                        pwsh_profiles.insert(profile_path);
                        Console::stream() << "Init " << exe << " profile at '" << profile_path << "'";
                        init_powershell(context, profile_path, conda_prefix);
                    }
                }
            }
        }
        else
        {
            throw std::runtime_error("Support for other shells not yet implemented.");
        }
#ifdef _WIN32
        enable_long_paths_support(false, context.graphics_params.palette);
#endif
    }

    void deinit_shell(Context& context, const std::string& shell, const fs::u8path& conda_prefix)
    {
        auto mamba_exe = get_self_exe_path();
        fs::u8path home = util::user_home_dir();
        if (shell == "bash")
        {
            fs::u8path bashrc_path = (util::on_mac || util::on_win) ? home / ".bash_profile"
                                                                    : home / ".bashrc";
            reset_rc_file(context, bashrc_path, shell, mamba_exe);
        }
        else if (shell == "zsh")
        {
            fs::u8path zshrc_path = home / ".zshrc";
            reset_rc_file(context, zshrc_path, shell, mamba_exe);
        }
        else if (shell == "xonsh")
        {
            fs::u8path xonshrc_path = home / ".xonshrc";
            reset_rc_file(context, xonshrc_path, shell, mamba_exe);
        }
        else if (shell == "csh")
        {
            fs::u8path tcshrc_path = home / ".tcshrc";
            reset_rc_file(context, tcshrc_path, shell, mamba_exe);
        }
        else if (shell == "fish")
        {
            fs::u8path fishrc_path = home / ".config" / "fish" / "config.fish";
            reset_rc_file(context, fishrc_path, shell, mamba_exe);
        }
        else if (shell == "nu")
        {
            fs::u8path nu_config_path = home / ".config" / "nushell" / "config.nu";
            reset_rc_file(context, nu_config_path, shell, mamba_exe);
        }
        else if (shell == "cmd.exe")
        {
#ifndef _WIN32
            throw std::runtime_error("CMD.EXE can only be deinitialized on Windows.");
#else
            deinit_cmd_exe_registry(
                L"Software\\Microsoft\\Command Processor",
                conda_prefix,
                context.graphics_params
            );
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
                    deinit_powershell(context, profile_path, conda_prefix);
                }
            }
        }
        else
        {
            throw std::runtime_error("Support for other shells not yet implemented.");
        }

        deinit_root_prefix(context, shell, conda_prefix);
    }

    fs::u8path config_path_for_shell(const std::string& shell)
    {
        fs::u8path home = util::user_home_dir();
        fs::u8path config_path;
        if (shell == "bash")
        {
            config_path = (util::on_mac || util::on_win) ? home / ".bash_profile" : home / ".bashrc";
        }
        else if (shell == "zsh")
        {
            config_path = home / ".zshrc";
        }
        else if (shell == "xonsh")
        {
            config_path = home / ".xonshrc";
        }
        else if (shell == "csh")
        {
            config_path = home / ".tcshrc";
        }
        else if (shell == "fish")
        {
            config_path = home / ".config" / "fish" / "config.fish";
        }
        else if (shell == "nu")
        {
            config_path = "";
        }
        return config_path;
    }

    std::vector<std::string> find_initialized_shells()
    {
        fs::u8path home = util::user_home_dir();

        std::vector<std::string> result;
        std::vector<std::string> supported_shells = { "bash", "zsh", "xonsh", "csh", "fish", "nu" };
        for (const std::string& shell : supported_shells)
        {
            fs::u8path config_path = config_path_for_shell(shell);

            if (fs::exists(config_path))
            {
                auto contents = read_contents(config_path);
                if (contents.find("# >>> mamba initialize >>>") != std::string::npos)
                {
                    result.push_back(shell);
                }
            }
        }

#ifdef _WIN32
        // cmd.exe
        const std::wstring reg = get_autorun_registry_key(L"Software\\Microsoft\\Command Processor");
        if (std::regex_match(reg, run_info().regex_mamba_cmd_hook))
        {
            result.push_back("cmd.exe");
        }
#endif
        // powershell
        {
            std::set<std::string> pwsh_profiles;
            for (auto& exe : std::vector<std::string>{ "powershell", "pwsh", "pwsh-preview" })
            {
                auto profile_path = find_powershell_paths(exe);
                if (!profile_path.empty() && fs::exists(profile_path))
                {
                    result.push_back("powershell");
                }
            }
        }
        return result;
    }
}
