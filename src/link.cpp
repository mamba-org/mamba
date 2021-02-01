// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <regex>
#include <string>
#include <tuple>
#include <vector>

#include "thirdparty/termcolor.hpp"
#include <reproc++/run.hpp>

#include "mamba/environment.hpp"
#include "mamba/link.hpp"
#include "mamba/match_spec.hpp"
#include "mamba/output.hpp"
#include "mamba/transaction_context.hpp"
#include "mamba/util.hpp"
#include "mamba/validate.hpp"
#include "mamba/shell_init.hpp"
#include "mamba/activation.hpp"

#if _WIN32
#include "../data/conda_exe.hpp"
#endif

namespace mamba
{
    void python_entry_point_template(std::ostream& out, const python_entry_point_parsed& p)
    {
        auto import_name = split(p.func, ".")[0];
        out << "# -*- coding: utf-8 -*-\n";
        out << "import re\n";
        out << "import sys\n\n";

        out << "from " << p.module << " import " << import_name << "\n\n";

        out << "if __name__ == '__main__':\n";
        out << "    sys.argv[0] = re.sub(r'(-script\\.pyw?|\\.exe)?$', '', "
               "sys.argv[0])\n";
        out << "    sys.exit(" << p.func << "())\n";
    }

    void application_entry_point_template(std::ostream& out,
                                          const std::string_view& source_full_path)
    {
        out << "# -*- coding: utf-8 -*-\n";
        out << "if __name__ == '__main__':\n";
        out << "    import os\n";
        out << "    import sys\n";
        out << "    args = ['" << source_full_path << "']\n";
        out << "    if len(sys.argv) > 1:\n";
        out << "        args += sys.argv[1:]\n";
        out << "    os.execv(args[0], args)\n";
    }

    fs::path pyc_path(const fs::path& py_path, const std::string& py_ver)
    {
        /*
        This must not return backslashes on Windows as that will break
        tests and leads to an eventual need to make url_to_path return
        backslashes too and that may end up changing files on disc or
        to the result of comparisons with the contents of them.
        */
        if (py_ver[0] == '2')
        {
            // make `.pyc` file in same directory
            return concat(py_path.c_str(), 'c');
        }
        else
        {
            auto directory = py_path.parent_path();
            auto py_file_stem = py_path.stem();
            std::string py_ver_nodot = py_ver;
            replace_all(py_ver_nodot, ".", "");
            return directory / fs::path("__pycache__")
                   / concat(py_file_stem.c_str(), ".cpython-", py_ver_nodot, ".pyc");
        }
    }

    python_entry_point_parsed parse_entry_point(const std::string& ep_def)
    {
        // def looks like: "wheel = wheel.cli:main"
        auto cmd_mod_func = rsplit(ep_def, ":", 1);
        auto command_module = rsplit(cmd_mod_func[0], "=", 1);
        python_entry_point_parsed result;
        result.command = strip(command_module[0]);
        result.module = strip(command_module[1]);
        result.func = strip(cmd_mod_func[1]);
        return result;
    }

    std::string replace_long_shebang(const std::string& shebang)
    {
        if (shebang.size() <= 127)
        {
            return shebang;
        }
        else
        {
            assert(shebang.substr(0, 2) == "#!");
            auto path_begin = shebang.find_first_not_of(WHITESPACES);
            auto path_end = shebang.substr(path_begin).find_first_not_of(WHITESPACES);
            fs::path shebang_path = shebang.substr(path_begin, path_end);
            return concat("#!/usr/bin/env ",
                          std::string(shebang_path.filename()),
                          " ",
                          shebang.substr(path_end),
                          "\n");
        }
    }

    // for noarch python packages that have entry points
    auto LinkPackage::create_python_entry_point(const fs::path& path,
                                                const python_entry_point_parsed& entry_point)
    {
#ifdef _WIN32
        // We add -script.py to WIN32, and link the conda.exe launcher which will
        // automatically find the correct script to launch
        std::string win_script = path.string() + "-script.py";
        fs::path script_path = m_context->target_prefix / win_script;
#else
        fs::path script_path = m_context->target_prefix / path;
#endif
        if (fs::exists(script_path))
        {
            std::cerr << termcolor::yellow << "Clobberwarning: " << termcolor::reset
                      << "$CONDA_PREFIX/"
                      << fs::relative(script_path, m_context->target_prefix).string() << std::endl;
            fs::remove(script_path);
        }
        std::ofstream out_file(script_path);

        fs::path python_path;
        if (m_context->has_python)
        {
            python_path = m_context->target_prefix / m_context->python_path;
        }
        if (!python_path.empty())
        {
            std::string py_str = python_path.c_str();
            // Shebangs cannot be longer than 127 characters
            if (py_str.size() > (127 - 2))
            {
                out_file << "#!/usr/bin/env python\n";
            }
            else
            {
                out_file << "#!" << py_str << "\n";
            }
        }

        python_entry_point_template(out_file, entry_point);
        out_file.close();

#ifdef _WIN32
        fs::path script_exe = path;
        script_exe.replace_extension("exe");

        if (fs::exists(m_context->target_prefix / script_exe))
        {
            std::cerr << termcolor::yellow << "Clobberwarning: " << termcolor::reset
                      << "$CONDA_PREFIX/" << script_exe.string() << std::endl;
            fs::remove(m_context->target_prefix / script_exe);
        }

        std::ofstream conda_exe_f(m_context->target_prefix / script_exe, std::ios::binary);
        conda_exe_f.write(reinterpret_cast<char*>(conda_exe), conda_exe_len);
        conda_exe_f.close();
        make_executable(m_context->target_prefix / script_exe);
        return std::array<std::string, 2>{ win_script, script_exe };
#else
        if (!python_path.empty())
        {
            make_executable(script_path);
        }
        return path;
#endif
    }

    std::string ensure_pad(const std::string& str, char pad = '_')
    {
        // Examples:
        // >>> ensure_pad('conda')
        // '_conda_'
        // >>> ensure_pad('_conda')
        // '__conda_'
        // >>> ensure_pad('')
        // ''
        if (str.size() == 0)
        {
            return str;
        }
        if (str[0] == pad && str[str.size() - 1] == pad)
        {
            return str;
        }
        else
        {
            return concat(pad, str, pad);
        }
    }

    std::string win_path_double_escape(const std::string& path)
    {
#ifdef _WIN32
        std::string path_copy = path;
        replace_all(path_copy, "\\", "\\\\");
        return path_copy;
#else
        return path;
#endif
    }

    void create_application_entry_point(const fs::path& source_full_path,
                                        const fs::path& target_full_path,
                                        const fs::path& python_full_path)
    {
        // source_full_path: where the entry point file points to
        // target_full_path: the location of the new entry point file being created
        if (fs::exists(target_full_path))
        {
            std::cerr << termcolor::yellow << "Clobberwarning: " << termcolor::reset
                      << target_full_path.string() << std::endl;
        }

        if (!fs::is_directory(target_full_path.parent_path()))
        {
            fs::create_directories(target_full_path.parent_path());
        }

        std::ofstream out_file(target_full_path);
        out_file << "!#" << python_full_path.c_str() << "\n";
        application_entry_point_template(out_file, win_path_double_escape(source_full_path));
        out_file.close();

        make_executable(target_full_path);
    }

    // def create_application_entry_point(source_full_path, target_full_path,
    // python_full_path):
    //     # source_full_path: where the entry point file points to
    //     # target_full_path: the location of the new entry point file being
    //     created if lexists(target_full_path):
    //         maybe_raise(BasicClobberError(
    //             source_path=None,
    //             target_path=target_full_path,
    //             context=context,
    //         ), context)

    //     entry_point = application_entry_point_template % {
    //         "source_full_path": win_path_double_escape(source_full_path),
    //     }
    //     if not isdir(dirname(target_full_path)):
    //         mkdir_p(dirname(target_full_path))
    //     with open(target_full_path, str("w")) as fo:
    //         if ' ' in python_full_path:
    //             python_full_path = ensure_pad(python_full_path, '"')
    //         fo.write('#!%s\n' % python_full_path)
    //         fo.write(entry_point)
    //     make_executable(target_full_path)

    std::string get_prefix_messages(const fs::path& prefix)
    {
        auto messages_file = prefix / ".messages.txt";
        if (fs::exists(messages_file))
        {
            try
            {
                std::ifstream msgs(messages_file);
                std::stringstream res;
                std::copy(std::istreambuf_iterator<char>(msgs),
                          std::istreambuf_iterator<char>(),
                          std::ostreambuf_iterator<char>(res));
                return res.str();
            }
            catch (...)
            {
                // ignore
            }
            fs::remove(messages_file);
        }
        return "";
    }

    bool ensure_comspec_set()
    {
        std::string cmd_exe = env::get("COMSPEC");
        if (!ends_with(to_lower(cmd_exe), "cmd.exe"))
        {
            cmd_exe = fs::path(env::get("SystemRoot")) / "System32" / "cmd.exe";
            if (!fs::is_regular_file(cmd_exe))
            {
                cmd_exe = fs::path(env::get("windir")) / "System32" / "cmd.exe";
            }
            if (!fs::is_regular_file(cmd_exe))
            {
                LOG_WARNING << "cmd.exe could not be found. Looked in SystemRoot and "
                               "windir env vars.";
            }
            else
            {
                env::set("COMSPEC", cmd_exe);
            }
        }
        return true;
    }

    std::unique_ptr<TemporaryFile> wrap_call(const fs::path& root_prefix,
                                             const fs::path& prefix,
                                             bool dev_mode,
                                             bool debug_wrapper_scripts,
                                             const std::vector<std::string>& arguments)
    {
        // todo add abspath here
        fs::path tmp_prefix = prefix / ".tmp";

#ifdef _WIN32
        ensure_comspec_set();
        std::string comspec = env::get("COMSPEC");
        std::string conda_bat;

        // TODO
        std::string CONDA_PACKAGE_ROOT = "";

        std::string bat_name = Context::instance().is_micromamba ? "micromamba.bat" : "conda.bat";

        if (dev_mode)
        {
            conda_bat = fs::path(CONDA_PACKAGE_ROOT) / "shell" / "condabin" / "conda.bat";
        }
        else
        {
            conda_bat = env::get("CONDA_BAT");
            if (conda_bat.size() == 0)
            {
                conda_bat = fs::absolute(root_prefix) / "condabin" / bat_name;
            }
        }
        if (!fs::exists(conda_bat) && Context::instance().is_micromamba)
        {
            // this adds in the needed .bat files for activation
            init_root_prefix_cmdexe(Context::instance().root_prefix);
        }

        auto tf = std::make_unique<TemporaryFile>("mamba_bat_", ".bat");

        std::ofstream out(tf->path());

        std::string silencer = debug_wrapper_scripts ? "" : "@";

        out << silencer << "ECHO OFF\n";
        out << silencer << "SET PYTHONIOENCODING=utf-8\n";
        out << silencer << "SET PYTHONUTF8=1\n";
        out << silencer
            << "FOR /F \"tokens=2 delims=:.\" %%A in (\'chcp\') do for %%B in (%%A) "
               "do set \"_CONDA_OLD_CHCP=%%B\"\n";
        out << silencer << "chcp 65001 > NUL\n";

        if (dev_mode)
        {
            // from conda.core.initialize import CONDA_PACKAGE_ROOT
            out << silencer << "SET CONDA_DEV=1\n";
            // In dev mode, conda is really:
            // 'python -m conda'
            // *with* PYTHONPATH set.
            out << silencer << "SET PYTHONPATH=" << CONDA_PACKAGE_ROOT << "\n";
            out << silencer << "SET CONDA_EXE="
                << "python.exe"
                << "\n";  // TODO this should be `sys.executable`
            out << silencer << "SET _CE_M=-m\n";
            out << silencer << "SET _CE_CONDA=conda\n";
        }

        if (debug_wrapper_scripts)
        {
            out << "echo *** environment before *** 1>&2\n";
            out << "SET 1>&2\n";
        }

        out << silencer << "CALL \"" << conda_bat << "\" activate " << prefix << "\n";
        out << silencer << "IF %ERRORLEVEL% NEQ 0 EXIT /b %ERRORLEVEL%\n";

        if (debug_wrapper_scripts)
        {
            out << "echo *** environment after *** 1>&2\n";
            out << "SET 1>&2\n";
        }
#else
        auto tf = std::make_unique<TemporaryFile>();
        std::ofstream out(tf->path());
        std::stringstream hook_quoted;

        std::string shebang, dev_arg;

        if (!Context::instance().is_micromamba)
        {
            // During tests, we sometimes like to have a temp env with e.g. an old python
            // in it and have it run tests against the very latest development sources.
            // For that to work we need extra smarts here, we want it to be instead:
            if (dev_mode)
            {
                shebang += std::string(root_prefix / "bin" / "python");
                shebang += " -m conda";

                dev_arg = "--dev";
            }
            else
            {
                if (std::getenv("CONDA_EXE"))
                {
                    shebang = std::getenv("CONDA_EXE");
                }
                else
                {
                    shebang = std::string(root_prefix / "bin" / "conda");
                }
            }

            if (dev_mode)
            {
                // out << ">&2 export PYTHONPATH=" << CONDA_PACKAGE_ROOT << "\n";
            }

            hook_quoted << std::quoted(shebang, '\'') << " 'shell.posix' 'hook' " << dev_arg;
        }
        else
        {
            // Micromamba hook
            hook_quoted << std::quoted(get_self_exe_path().string(), '\'')
                        << " 'shell' 'hook' '-s' 'bash' '-p' "
                        << std::quoted(Context::instance().root_prefix.string(), '\'');
        }
        if (debug_wrapper_scripts)
        {
            out << "set -x\n";
            out << ">&2 echo \"*** environment before ***\"\n"
                << ">&2 env\n"
                << ">&2 echo \"$(" << hook_quoted.str() << ")\"\n";
        }
        out << "eval \"$(" << hook_quoted.str() << ")\"\n";

        if (!Context::instance().is_micromamba)
        {
            out << "conda activate " << dev_arg << " " << std::quoted(prefix.string()) << "\n";
        }
        else
        {
            out << "micromamba activate " << std::quoted(prefix.string()) << "\n";
        }


        if (debug_wrapper_scripts)
        {
            out << ">&2 echo \"*** environment after ***\"\n"
                << ">&2 env\n";
        }
#endif
        // write our command
        out << "\n" << join(" ", arguments);
        return tf;
    }

    auto prepare_wrapped_call(const fs::path& prefix, const std::vector<std::string>& cmd)
    {
        std::vector<std::string> command_args;
        std::unique_ptr<TemporaryFile> script_file;

        if (on_win)
        {
            ensure_comspec_set();
            std::string comspec = env::get("COMSPEC");
            if (comspec.size() == 0)
            {
                throw std::runtime_error(
                    concat("Failed to run script: COMSPEC not set in env vars."));
            }

            script_file = wrap_call(
                Context::instance().root_prefix, prefix, Context::instance().dev, false, cmd);

            command_args = { comspec, "/d", "/c", script_file->path() };
        }
        else
        {
            // shell_path = 'sh' if 'bsd' in sys.platform else 'bash'
            fs::path shell_path = env::which("bash");
            if (shell_path.empty())
            {
                shell_path = env::which("sh");
            }

            script_file = wrap_call(
                Context::instance().root_prefix, prefix, Context::instance().dev, false, cmd);
            command_args.push_back(shell_path);
            command_args.push_back(script_file->path());
        }
        return std::make_tuple(command_args, std::move(script_file));
    }

    /*
       call the post-link or pre-unlink script and return true / false on success /
       failure
    */
    bool run_script(const fs::path& prefix,
                    const PackageInfo& pkg_info,
                    const std::string& action = "post-link",
                    const std::string& env_prefix = "",
                    bool activate = false)
    {
        fs::path path;
        if (on_win)
        {
            path = prefix / get_bin_directory_short_path()
                   / concat(".", pkg_info.name, "-", action, ".bat");
        }
        else
        {
            path = prefix / get_bin_directory_short_path()
                   / concat(".", pkg_info.name, "-", action, ".sh");
        }

        if (!fs::exists(path))
        {
            LOG_INFO << action << " script for " << pkg_info.name << " does not exist (" << path
                     << ")";
            return true;
        }

        // TODO impl.
        std::map<std::string, std::string> envmap;  // = env::copy();

        if (action == "pre-link")
        {
            throw std::runtime_error("mamba does not support pre-link scripts");
        }

        // script_caller = None
        std::vector<std::string> command_args;
        std::unique_ptr<TemporaryFile> script_file;

        if (on_win)
        {
            ensure_comspec_set();
            std::string comspec = env::get("COMSPEC");
            if (comspec.size() == 0)
            {
                LOG_ERROR << "Failed to run " << action << " for " << pkg_info.name
                          << " due to COMSPEC not set in env vars.";
                return false;
            }

            if (activate)
            {
                script_file = wrap_call(Context::instance().root_prefix,
                                        prefix,
                                        Context::instance().dev,
                                        false,
                                        { "@CALL", path });

                command_args = { comspec, "/d", "/c", script_file->path() };
            }
            else
            {
                command_args = { comspec, "/d", "/c", path };
            }
        }

        else
        {
            // shell_path = 'sh' if 'bsd' in sys.platform else 'bash'
            fs::path shell_path = env::which("bash");
            if (shell_path.empty())
            {
                shell_path = env::which("sh");
            }

            if (activate)
            {
                // std::string caller
                script_file = wrap_call(Context::instance().root_prefix,
                                        prefix,
                                        Context::instance().dev,
                                        false,
                                        { ".", path });
                command_args.push_back(shell_path);
                command_args.push_back(script_file->path());
            }
            else
            {
                command_args.push_back(shell_path);
                command_args.push_back("-x");
                command_args.push_back(path);
            }
        }

        envmap["ROOT_PREFIX"] = Context::instance().root_prefix;
        envmap["PREFIX"] = env_prefix.size() ? env_prefix : std::string(prefix);
        envmap["PKG_NAME"] = pkg_info.name;
        envmap["PKG_VERSION"] = pkg_info.version;
        envmap["PKG_BUILDNUM"] = pkg_info.build_string.empty()
                                     ? std::to_string(pkg_info.build_number)
                                     : pkg_info.build_string;

        std::string PATH = env::get("PATH");
        envmap["PATH"] = concat(path.parent_path().c_str(), env::pathsep(), PATH);

        std::string cargs = join(" ", command_args);
        LOG_DEBUG << "For " << pkg_info.name << " at " << envmap["PREFIX"]
                  << ", executing script: $ " << cargs;
        LOG_WARNING << "Calling " << cargs;

        reproc::options options;
        options.redirect.parent = true;
        options.env.behavior = reproc::env::extend;
        options.env.extra = envmap;
        std::string cwd = path.parent_path();
        options.working_directory = cwd.c_str();

        LOG_DEBUG << "ENV MAP:"
                  << "\n ROOT_PREFIX: " << envmap["ROOT_PREFIX"]
                  << "\n PREFIX: " << envmap["PREFIX"] << "\n PKG_NAME: " << envmap["PKG_NAME"]
                  << "\n PKG_VERSION: " << envmap["PKG_VERSION"]
                  << "\n PKG_BUILDNUM: " << envmap["PKG_BUILDNUM"] << "\n PATH: " << envmap["PATH"]
                  << "\n CWD: " << cwd;

        auto [status, ec] = reproc::run(command_args, options);

        auto msg = get_prefix_messages(envmap["PREFIX"]);
        if (Context::instance().json)
        {
            // TODO implement cerr also on Console?
            std::cerr << msg;
        }
        else
        {
            Console::print(msg);
        }

        if (ec)
        {
            LOG_ERROR << "response code: " << status << " error message: " << ec.message();
            if (script_file != nullptr && env::get("CONDA_TEST_SAVE_TEMPS").size())
            {
                LOG_ERROR << "CONDA_TEST_SAVE_TEMPS :: retaining run_script" << script_file->path();
            }
            throw std::runtime_error("failed to execute pre/post link script for " + pkg_info.name);
        }
        return true;
    }

    UnlinkPackage::UnlinkPackage(const PackageInfo& pkg_info,
                                 const fs::path& cache_path,
                                 TransactionContext* context)
        : m_pkg_info(pkg_info)
        , m_cache_path(cache_path)
        , m_specifier(m_pkg_info.str())
        , m_context(context)
    {
    }

    bool UnlinkPackage::unlink_path(const nlohmann::json& path_data)
    {
        std::string subtarget = path_data["_path"].get<std::string>();
        fs::path dst = m_context->target_prefix / subtarget;
        fs::remove(dst);

        // TODO what do we do with empty directories?
        // remove empty parent path
        auto parent_path = dst.parent_path();
        while (fs::is_empty(parent_path))
        {
            fs::remove(parent_path);
            parent_path = parent_path.parent_path();
        }
        return true;
    }

    bool UnlinkPackage::execute()
    {
        // find the recorded JSON file
        fs::path json = m_context->target_prefix / "conda-meta" / (m_specifier + ".json");
        LOG_INFO << "unlink: opening " << json << std::endl;
        std::ifstream json_file(json);
        nlohmann::json json_record;
        json_file >> json_record;

        for (auto& path : json_record["paths_data"]["paths"])
        {
            unlink_path(path);
        }

        json_file.close();

        fs::remove(json);

        return true;
    }

    bool UnlinkPackage::undo()
    {
        LinkPackage lp(m_pkg_info, m_cache_path, m_context);
        return lp.execute();
    }

    LinkPackage::LinkPackage(const PackageInfo& pkg_info,
                             const fs::path& cache_path,
                             TransactionContext* context)
        : m_pkg_info(pkg_info)
        , m_cache_path(cache_path)
        , m_source(cache_path / m_pkg_info.str())
        , m_context(context)
    {
    }

    std::tuple<std::string, std::string> LinkPackage::link_path(const PathData& path_data,
                                                                bool noarch_python)
    {
        std::string subtarget = path_data.path;
        LOG_INFO << "linking path " << subtarget;
        fs::path dst, rel_dst;
        if (noarch_python)
        {
            rel_dst = get_python_noarch_target_path(subtarget, m_context->site_packages_path);
            dst = m_context->target_prefix / rel_dst;
        }
        else
        {
            rel_dst = subtarget;
            dst = m_context->target_prefix / rel_dst;
        }

        fs::path src = m_source / subtarget;
        if (!fs::exists(dst.parent_path()))
        {
            fs::create_directories(dst.parent_path());
        }

        if (fs::exists(dst))
        {
            // Sometimes we might want to raise here ...
            std::cerr << termcolor::yellow << "Clobberwarning: " << termcolor::reset
                      << "$CONDA_PREFIX/" << rel_dst.string() << std::endl;
#ifdef _WIN32
            return std::make_tuple(validate::sha256sum(dst), rel_dst);
#endif
            fs::remove(dst);
        }

#ifdef __APPLE__
        bool binary_changed = false;
#endif
        // std::string path_type = path_data["path_type"].get<std::string>();
        if (!path_data.prefix_placeholder.empty())
        {
            // we have to replace the PREFIX stuff in the data
            // and copy the file
            std::string new_prefix = m_context->target_prefix;
#ifdef _WIN32
            replace_all(new_prefix, "\\", "/");
#endif
            LOG_INFO << "Copying file & replace prefix " << src << " -> " << dst;
            // TODO windows does something else here

            std::string buffer;
            if (path_data.file_mode != FileMode::BINARY)
            {
                buffer = read_contents(src, std::ios::in | std::ios::binary);
                replace_all(buffer, path_data.prefix_placeholder, new_prefix);
            }
            else
            {
                assert(path_data.file_mode == FileMode::BINARY);
                buffer = read_contents(src, std::ios::in | std::ios::binary);

#ifdef _WIN32
                auto has_pyzzer_entrypoint
                    = [](const std::string& data) { return data.rfind("PK\x05\x06"); };

                // on win we only replace pyzzer entrypoints apparently
                auto entry_point = has_pyzzer_entrypoint(buffer);

                struct pyzzer_struct
                {
                    uint32_t cdr_size;
                    uint32_t cdr_offset;
                } pyzzer_entry;

                if (entry_point != std::string::npos)
                {
                    std::string launcher, shebang;
                    pyzzer_entry
                        = *reinterpret_cast<const pyzzer_struct*>(buffer.c_str() + entry_point);
                    std::size_t arc_pos
                        = entry_point - pyzzer_entry.cdr_size - pyzzer_entry.cdr_offset;

                    if (arc_pos > 0)
                    {
                        auto pos = buffer.rfind("#!", arc_pos);
                        if (pos != std::string::npos)
                        {
                            shebang = buffer.substr(pos, arc_pos);
                            if (pos > 0)
                            {
                                launcher = buffer.substr(0, pos);
                            }
                        }
                    }

                    if (!shebang.empty() && !launcher.empty())
                    {
                        replace_all(shebang, path_data.prefix_placeholder, new_prefix);
                        std::ofstream fo(dst, std::ios::out | std::ios::binary);
                        fo << launcher << shebang << (buffer.c_str() + arc_pos);
                        fo.close();
                    }
                    return std::make_tuple(validate::sha256sum(dst), rel_dst);
                }
                else
                {
                    std::make_tuple(validate::sha256sum(dst), rel_dst);
                }

#else
                std::size_t padding_size
                    = (path_data.prefix_placeholder.size() > new_prefix.size())
                          ? path_data.prefix_placeholder.size() - new_prefix.size()
                          : 0;
                std::string padding(padding_size, '\0');

                auto binary_replace
                    = [&](std::size_t pos, std::size_t end, const std::string& suffix) {
                          std::string replacement = concat(new_prefix, suffix, padding);
                          buffer.replace(pos, end - pos, replacement);
                      };

                std::size_t pos = buffer.find(path_data.prefix_placeholder);
                while (pos != std::string::npos)
                {
#if defined(__APPLE__)
                    binary_changed = true;
#endif
                    std::size_t end = pos + path_data.prefix_placeholder.size();
                    std::string suffix;

                    while (end < buffer.size() && buffer[end] != '\0')
                    {
                        suffix += buffer[end];
                        ++end;
                    }

                    binary_replace(pos, end, suffix);
                    pos = buffer.find(path_data.prefix_placeholder, end);
                }
#endif
            }

            auto open_mode = (path_data.file_mode == FileMode::BINARY)
                                 ? std::ios::out | std::ios::binary
                                 : std::ios::out | std::ios::binary;
            std::ofstream fo(dst, open_mode);
            fo << buffer;
            fo.close();

            fs::permissions(dst, fs::status(src).permissions());
#if defined(__APPLE__)
            if (binary_changed && m_pkg_info.subdir == "osx-arm64")
            {
                std::vector<std::string> cmd
                    = { "/usr/bin/codesign", "-s", "-", "-f", dst.string() };
                auto [status, ec] = reproc::run(cmd, reproc::options{});
                if (ec)
                {
                    throw std::runtime_error(std::string("Could not codesign executable")
                                             + ec.message());
                }
            }
#endif

            return std::make_tuple(validate::sha256sum(dst), rel_dst);
        }

        if (path_data.path_type == PathType::HARDLINK)
        {
            LOG_INFO << "hard linked " << src << " --> " << dst;
            fs::create_hard_link(src, dst);
        }
        else if (path_data.path_type == PathType::SOFTLINK)
        {
            LOG_INFO << "soft linked " << src << " --> " << dst;
            fs::copy_symlink(src, dst);
        }
        else
        {
            throw std::runtime_error(std::string("Path type not implemented: ")
                                     + std::to_string(static_cast<int>(path_data.path_type)));
        }
        // TODO we could also use the SHA256 sum of the paths json
        return std::make_tuple(validate::sha256sum(dst), rel_dst);
    }

    std::vector<fs::path> LinkPackage::compile_pyc_files(const std::vector<fs::path>& py_files)
    {
        if (py_files.size() == 0)
        {
            return {};
        }
        std::vector<fs::path> pyc_files;

        TemporaryFile all_py_files;
        std::ofstream all_py_files_f(all_py_files.path());

        for (auto& f : py_files)
        {
            all_py_files_f << f.c_str() << '\n';
            pyc_files.push_back(pyc_path(f, m_context->short_python_version));
            LOG_INFO << "Compiling " << pyc_files[pyc_files.size() - 1];
        }
        all_py_files_f.close();

        std::vector<std::string> command = { m_context->target_prefix / m_context->python_path,
                                             "-Wi",
                                             "-m",
                                             "compileall",
                                             "-q",
                                             "-l",
                                             "-i",
                                             all_py_files.path() };

        auto py_ver_split = split(m_context->python_version, ".");

        if (std::stoi(std::string(py_ver_split[0])) >= 3
            && std::stoi(std::string(py_ver_split[1])) > 5)
        {
            // activate parallel pyc compilation
            command.push_back("-j0");
        }

        reproc::options options;
        options.redirect.parent = true;
        std::string cwd = m_context->target_prefix;
        options.working_directory = cwd.c_str();

        auto [wrapped_command, script_file]
            = prepare_wrapped_call(m_context->target_prefix, command);
        LOG_INFO << "Running wrapped python compilation command " << join(" ", command);
        auto [_, ec] = reproc::run(wrapped_command, options);

        if (ec)
        {
            throw std::runtime_error(ec.message());
        }

        return pyc_files;
    }

    enum class NoarchType
    {
        NOT_A_NOARCH,
        GENERIC_V1,
        GENERIC_V2,
        PYTHON
    };

    bool LinkPackage::execute()
    {
        LOG_INFO << "Executing install for " << m_source;
        nlohmann::json index_json, out_json;
        LOG_WARNING << "Opening: " << m_source / "info" / "paths.json";

        auto paths_data = read_paths(m_source);

        LOG_WARNING << "Opening: " << m_source / "info" / "repodata_record.json";
        std::ifstream repodata_f(m_source / "info" / "repodata_record.json");

        repodata_f >> index_json;

        // handle noarch packages
        NoarchType noarch_type = NoarchType::NOT_A_NOARCH;
        if (index_json.find("noarch") != index_json.end())
        {
            if (index_json["noarch"].type() == nlohmann::json::value_t::boolean)
            {
                noarch_type = NoarchType::GENERIC_V1;
            }
            else
            {
                std::string na_t(index_json["noarch"].get<std::string>());
                if (na_t == "python")
                {
                    noarch_type = NoarchType::PYTHON;
                    LOG_INFO << "Installing Python noarch package";
                }
                else if (na_t == "generic")
                {
                    noarch_type = NoarchType::GENERIC_V2;
                }
            }
        }

        std::vector<std::string> files_record;

        // for (auto& path : paths_json["paths"])
        nlohmann::json paths_json = nlohmann::json::object();
        paths_json["paths"] = nlohmann::json::array();
        paths_json["paths_version"] = 1;
        for (auto& path : paths_data)
        {
            auto [sha256_in_prefix, final_path]
                = link_path(path, noarch_type == NoarchType::PYTHON);
            files_record.push_back(final_path);

            nlohmann::json json_record
                = { { "_path", final_path }, { "sha256_in_prefix", sha256_in_prefix } };

            if (!path.sha256.empty())
            {
                json_record["sha256"] = path.sha256;
            }
            if (path.path_type == PathType::SOFTLINK)
            {
                json_record["path_type"] = "softlink";
            }
            else if (path.path_type == PathType::HARDLINK)
            {
                json_record["path_type"] = "hardlink";
            }
            else if (path.path_type == PathType::DIRECTORY)
            {
                json_record["path_type"] = "directory";
            }

            if (path.no_link)
            {
                json_record["no_link"] = true;
            }

            if (path.size_in_bytes != 0)
            {
                // note: in conda this is the size in bytes _before_ prefix replacement
                json_record["size_in_bytes"] = path.size_in_bytes;
            }

            paths_json["paths"].push_back(json_record);
        }

        std::string f_name = index_json["name"].get<std::string>() + "-"
                             + index_json["version"].get<std::string>() + "-"
                             + index_json["build"].get<std::string>();

        out_json = index_json;
        out_json["paths_data"] = paths_json;
        out_json["files"] = files_record;
        out_json["requested_spec"] = "TODO";
        out_json["package_tarball_full_path"] = std::string(m_source) + ".tar.bz2";
        out_json["extracted_package_dir"] = m_source;

        // TODO find out what `1` means
        out_json["link"] = { { "source", std::string(m_source) }, { "type", 1 } };

        if (noarch_type == NoarchType::PYTHON)
        {
            fs::path link_json_path = m_source / "info" / "link.json";
            nlohmann::json link_json;
            if (fs::exists(link_json_path))
            {
                std::ifstream link_json_file(link_json_path);
                link_json_file >> link_json;
            }

            std::vector<fs::path> for_compilation;
            static std::regex py_file_re("^site-packages[/\\\\][^\\t\\n\\r\\f\\v]+\\.py$");
            for (auto& sub_path_json : paths_data)
            {
                if (std::regex_match(sub_path_json.path, py_file_re))
                {
                    for_compilation.push_back(get_python_noarch_target_path(
                        sub_path_json.path, m_context->site_packages_path));
                }
            }

            std::vector<fs::path> pyc_files = compile_pyc_files(for_compilation);

            for (const fs::path& pyc_path : pyc_files)
            {
                out_json["paths_data"]["paths"].push_back(
                    { { "_path", std::string(pyc_path) }, { "path_type", "pyc_file" } });

                out_json["files"].push_back(pyc_path);
            }

            if (link_json.find("noarch") != link_json.end()
                && link_json["noarch"].find("entry_points") != link_json["noarch"].end())
            {
                for (auto& ep : link_json["noarch"]["entry_points"])
                {
                    // install entry points
                    auto entry_point_parsed = parse_entry_point(ep.get<std::string>());
                    auto entry_point_path
                        = get_bin_directory_short_path() / entry_point_parsed.command;
                    LOG_INFO << "entry point path: " << entry_point_path << std::endl;
                    auto files = create_python_entry_point(entry_point_path, entry_point_parsed);

#ifdef _WIN32
                    out_json["paths_data"]["paths"].push_back(
                        { { "_path", files[0] },
                          { "path_type", "windows_python_entry_point_script" } });
                    out_json["paths_data"]["paths"].push_back(
                        { { "_path", files[1] },
                          { "path_type", "windows_python_entry_point_exe" } });
                    out_json["files"].push_back(files[0]);
                    out_json["files"].push_back(files[1]);
#else
                    out_json["paths_data"]["paths"].push_back(
                        { { "_path", files }, { "path_type", "unix_python_entry_point" } });
                    out_json["files"].push_back(files);
#endif
                }
            }
        }

        run_script(m_context->target_prefix, m_pkg_info, "post-link", "", true);

        fs::path prefix_meta = m_context->target_prefix / "conda-meta";
        if (!fs::exists(prefix_meta))
        {
            fs::create_directory(prefix_meta);
        }

        LOG_INFO << "Finalizing package " << f_name << " installation";
        std::ofstream out_file(prefix_meta / (f_name + ".json"));
        out_file << out_json.dump(4);

        return true;
    }

    bool LinkPackage::undo()
    {
        UnlinkPackage ulp(m_pkg_info, m_cache_path, m_context);
        return ulp.execute();
    }
}  // namespace mamba
