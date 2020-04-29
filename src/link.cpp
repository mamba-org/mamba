#include <regex>

#include "link.hpp"
#include "output.hpp"
#include "validate.hpp"
#include "environment.hpp"
#include "util.hpp"

#include "thirdparty/subprocess.hpp"
#include "thirdparty/pystring14/pystring.hpp"

namespace mamba
{
    struct python_entry_point_parsed
    {
        std::string command, module, func;
    };

    void python_entry_point_template(std::ostream& out,
                                     const python_entry_point_parsed& p)
    {
        auto import_name = pystring::split(p.func, ".")[0];
        out << "# -*- coding: utf-8 -*-\n";
        out << "import re\n";
        out << "import sys\n\n";

        out << "from " << p.module << " import " << import_name << "\n\n";

        out << "if __name__ == '__main__':\n";
        out << "    sys.argv[0] = re.sub(r'(-script\\.pyw?|\\.exe)?$', '', sys.argv[0])\n";
        out << "    sys.exit(" << p.func << "())\n";
    }

    void application_entry_point_template(std::ostream& out, const std::string_view& source_full_path)
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
            auto py_ver_nodot = pystring::replace(py_ver, ".", "");
            return directory / fs::path("__pycache__") / concat(py_file_stem.c_str(), ".cpython-", py_ver_nodot, ".pyc");
        }
    }

    python_entry_point_parsed parse_entry_point(const std::string& ep_def)
    {
        // def looks like: "wheel = wheel.cli:main"
        auto cmd_mod_func = pystring::rsplit(ep_def, ":", 1);
        auto command_module = pystring::rsplit(cmd_mod_func[0], "=", 1);
        python_entry_point_parsed result;
        result.command = pystring::strip(command_module[0]);
        result.module = pystring::strip(command_module[1]);
        result.func = pystring::strip(cmd_mod_func[1]);
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
            return concat("#!/usr/bin/env ", std::string(shebang_path.filename()), " ", shebang.substr(path_end));
        }
    }

    inline void make_executable(const fs::path& p)
    {
        fs::permissions(p, fs::perms::owner_all | fs::perms::group_all | fs::perms::others_read | fs::perms::others_exec);
    }

    // for noarch python packages that have entry points
    void create_python_entry_point(const fs::path& target, const fs::path& python, const python_entry_point_parsed& entry_point)
    {
        if (fs::exists(target))
        {
            throw std::runtime_error("clobber warning");
        }

        std::ofstream out_file(target);

        if (!python.empty())
        {
            std::string py_str = python.c_str();
            // Shebangs cannot be longer than 127 characters
            if (py_str.size() > (127 - 2))
            {
                out_file << "#!/usr/bin/env python";
            }
            else
            {
                out_file << "#!" << py_str << "\n";
            }
        }

        python_entry_point_template(out_file, entry_point);
        out_file.close();

        if (!python.empty())
        {
            // make executable (-rwxrwxr-x.)
            make_executable(target);
        }
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
        return pystring::replace(path, "\\", "\\\\");
        #else
        return path;
        #endif
    }

    void create_application_entry_point(const fs::path& source_full_path, const fs::path& target_full_path, const fs::path& python_full_path)
    {
        // source_full_path: where the entry point file points to
        // target_full_path: the location of the new entry point file being created
        if (fs::exists(target_full_path))
        {
            throw std::runtime_error("clobber warning");
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

    // def create_application_entry_point(source_full_path, target_full_path, python_full_path):
    //     # source_full_path: where the entry point file points to
    //     # target_full_path: the location of the new entry point file being created
    //     if lexists(target_full_path):
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
        if (!ends_with(pystring::lower(cmd_exe), "cmd.exe"))
        {
            cmd_exe = fs::path(env::get("SystemRoot")) / "System32" / "cmd.exe";
            if (!fs::is_regular_file(cmd_exe))
            {
                cmd_exe = fs::path(env::get("windir")) / "System32" / "cmd.exe";
            }
            if (!fs::is_regular_file(cmd_exe))
            {
                LOG_WARNING << "cmd.exe could not be found. Looked in SystemRoot and windir env vars.";
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
        bool multiline = false;
        
        #ifdef _WIN32
        ensure_comspec_set();
        std::string comspec = env::get("COMSPEC");
        std::string conda_bat;

        // TODO
        std::string CONDA_PACKAGE_ROOT = "";

        if (dev_mode)
        {
            conda_bat = fs::path(CONDA_PACKAGE_ROOT) / "shell" / "condabin" / "conda.bat";
        }
        else
        {
            conda_bat = env::get("CONDA_BAT");
            if (conda_bat.size() == 0)
            {
                // TODO add call to abspath
                conda_bat = root_prefix / "condabin" / "conda.bat";
            }
        }
        auto tf = std::make_unique<TemporaryFile>("mamba_bat_", ".bat");

        std::ofstream out(tf->path());

        std::string silencer;
        if (!debug_wrapper_scripts) silencer = "@";

        out << silencer << "ECHO OFF\n";
        out << silencer << "SET PYTHONIOENCODING=utf-8\n";
        out << silencer << "SET PYTHONUTF8=1\n";
        out << silencer << "FOR /F \"tokens=2 delims=:.\" %%A in (\'chcp\') do for %%B in (%%A) do set \"_CONDA_OLD_CHCP=%%B\"\n";
        out << silencer << "chcp 65001 > NUL\n";

        if (dev_mode)
        {
            // from conda.core.initialize import CONDA_PACKAGE_ROOT
            out << silencer << "SET CONDA_DEV=1\n";
            // In dev mode, conda is really:
            // 'python -m conda'
            // *with* PYTHONPATH set.
            out << silencer << "SET PYTHONPATH=" << CONDA_PACKAGE_ROOT << "\n";
            out << silencer << "SET CONDA_EXE=" << "python.exe" << "\n"; // TODO this should be `sys.executable`
            out << silencer << "SET _CE_M=-m\n";
            out << silencer << "SET _CE_CONDA=conda\n";
        }

        if (debug_wrapper_scripts)
        {
            out << "echo *** environment before *** 1>&2\n";
            out << "SET 1>&2\n";
        }

        out << silencer << "CALL \"" << conda_bat << "\" activate \"" << prefix << "\"\n";
        out << silencer << "IF %ERRORLEVEL% NEQ 0 EXIT /b %ERRORLEVEL%\n";
        if (debug_wrapper_scripts)
        {
            out << "echo *** environment after *** 1>&2\n";
            out << "SET 1>&2\n";
        }

        #else

        // During tests, we sometimes like to have a temp env with e.g. an old python in it
        // and have it run tests against the very latest development sources. For that to
        // work we need extra smarts here, we want it to be instead:
        std::string shebang;
        std::string dev_arg;
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

        auto tf = std::make_unique<TemporaryFile>();
        std::ofstream out(tf->path());

        if (dev_mode)
        {
            // tf << ">&2 export PYTHONPATH=" << CONDA_PACKAGE_ROOT << "\n";
        }

        std::stringstream hook_quoted;
        hook_quoted << std::quoted(shebang, '\'') << " 'shell.posix' 'hook' " << dev_arg;
        if (debug_wrapper_scripts)
        {
            out << "set -x\n";
            out << ">&2 echo \"*** environment before ***\"\n"
                << ">&2 env\n"
                << ">&2 echo \"$(" << hook_quoted.str() << ")\"\n";
        }
        out << "eval \"$(" << hook_quoted.str() << ")\"\n";
        out << "conda activate " << dev_arg << " " << std::quoted(prefix.c_str()) << "\n";

        if (debug_wrapper_scripts)
        {
            out << ">&2 echo \"*** environment after ***\"\n"
                << ">&2 env\n";
        }

        out << "\n"
            << pystring::join(" ", arguments);
        #endif

        out.close();

        return std::move(tf);
    }

    /*
        call the post-link or pre-unlink script and return true / false on success / failure
    */
    bool run_script(const fs::path& prefix, const std::string& name,
                    const std::string& action = "post-link",
                    const std::string& env_prefix = "",
                    bool activate = false)
    {
        #ifdef _WIN32
        auto path = prefix / get_bin_directory_short_path() / concat(".", name, "-", action, ".bat");
        #else
        auto path = prefix / get_bin_directory_short_path() / concat(".", name, "-", action, ".sh");
        #endif

        if (!fs::exists(path))
        {
            LOG_INFO << action << " script for " << name << " does not exist (" << path << ")";
            return true;
        }

        // TODO impl.
        std::map<std::string, std::string> envmap; // = env::copy();

        if (action == "pre-link")
        {
            throw std::runtime_error("mamba does not support pre-link scripts");
        }

        // script_caller = None
        std::vector<std::string> command_args;
        #ifdef _WIN32
        ensure_comspec_set();
        std::string comspec = env::get("COMSPEC");
        if (comspec.size() == 0)
        {
            LOG_ERROR << "Failed to run " << action << " for " << name << " due to COMSPEC not set in env vars.";
            return false;
        }
        LOG_WARNING << "COMSPEC " << comspec;

        std::unique_ptr<TemporaryFile> script_file;
        if (activate)
        {
            script_file = wrap_call(Context::instance().root_prefix, prefix,
                                    Context::instance().dev, false, {"@CALL", path});

            command_args = {comspec, "/d", "/c", script_file->path()};
        }
        else
        {
            command_args = {comspec, "/d", "/c", path};
        }
        #else
        // shell_path = 'sh' if 'bsd' in sys.platform else 'bash'
        fs::path shell_path = env::which("bash");
        if (shell_path.empty())
        {
            shell_path = env::which("sh");
        }

        std::unique_ptr<TemporaryFile> script_file;
        if (activate)
        {
            // std::string caller
            script_file = wrap_call(
                Context::instance().root_prefix,
                prefix,
                Context::instance().dev,
                false,
                {".", path}
            );
            command_args.push_back(shell_path);
            command_args.push_back(script_file->path());
        }
        else
        {
            command_args.push_back(shell_path);
            command_args.push_back("-x");
            command_args.push_back(path);
        }
        #endif

        envmap["ROOT_PREFIX"] = Context::instance().root_prefix;
        envmap["PREFIX"] = env_prefix.size() ? env_prefix : std::string(prefix);
        // envmap["PKG_NAME"] = prec.name
        // envmap["PKG_VERSION"] = prec.version
        // envmap["PKG_BUILDNUM"] = prec.build_number

        std::string PATH = env::get("PATH");

        LOG_WARNING << "PATH " << PATH;

        // prepend directory to current PATH variable
        std::string cargs = "";
        for (auto& x : command_args)
        {
            std::cout << x << " ";
            cargs += x;
            cargs += " ";
        }

        // std::string cargs = pystring::join(" ", command_args);
        envmap["PATH"] = concat(path.parent_path().c_str(), env::pathsep(), PATH);
        LOG_DEBUG << "for " << name << " at " << envmap["PREFIX"] << ", executing script: $ " << cargs;
        LOG_WARNING << "Caling " << cargs;

        int response = subprocess::call(cargs, subprocess::environment{envmap}, subprocess::cwd{path.parent_path()});
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
        if (response != 0)
        {
            LOG_ERROR << "response code: " << response;
            if (script_file != nullptr && env::get("CONDA_TEST_SAVE_TEMPS").size())
            {
                LOG_ERROR << "CONDA_TEST_SAVE_TEMPS :: retaining run_script" << script_file->path();
            }
            throw std::runtime_error("failed to execute pre/post link script for " + name);
        }
        else
        {
            return true;
        }
    } 

    UnlinkPackage::UnlinkPackage(const std::string& specifier, TransactionContext* context)
        : m_specifier(specifier), m_context(context)
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
        std::cout << "opening " << json << std::endl;
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

    LinkPackage::LinkPackage(const fs::path& source, TransactionContext* context)
        : m_source(source), m_context(context)
    {
    }

    std::string LinkPackage::link_path(const nlohmann::json& path_data, bool noarch_python)
    {
        std::string subtarget = path_data["_path"].get<std::string>();
        LOG_INFO << "linking path! " << subtarget;
        fs::path dst;
        if (noarch_python)
        {
            dst = m_context->target_prefix / get_python_noarch_target_path(subtarget, m_context->site_packages_path);
        }
        else
        {
            dst = m_context->target_prefix / subtarget;
        }

        fs::path src = m_source / subtarget;
        if (!fs::exists(dst.parent_path()))
        {
            fs::create_directories(dst.parent_path());
        }

        if (fs::exists(dst))
        {
            // This needs to raise a clobberwarning
            throw std::runtime_error("clobberwarning");
        }

        std::string path_type = path_data["path_type"].get<std::string>();

        if (path_data.find("prefix_placeholder") != path_data.end())
        {
            // we have to replace the PREFIX stuff in the data
            // and copy the file
            std::string prefix_placeholder = path_data["prefix_placeholder"].get<std::string>();
            std::string new_prefix = m_context->target_prefix;
            bool text_mode = path_data["file_mode"].get<std::string>() == "text";
            if (!text_mode)
            {
                assert(path_data["file_mode"].get<std::string>() == "binary");
                new_prefix.resize(prefix_placeholder.size(), '\0');
            }

            LOG_INFO << "copied file & replace prefix " << src << " -> " << dst;
            // TODO windows does something else here
            // if (path_data["file_mode"].get<std::string>() == "text")
            {
                std::ifstream fi;
                text_mode ? fi.open(src) : fi.open(src, std::ios::binary);
                fi.seekg(0, std::ios::end);
                size_t size = fi.tellg();
                std::string buffer;
                buffer.resize(size);
                fi.seekg(0);
                fi.read(&buffer[0], size);

                std::size_t pos = buffer.find(prefix_placeholder);
                while (pos != std::string::npos)
                {
                    buffer.replace(pos, prefix_placeholder.size(), new_prefix);
                    pos = buffer.find(prefix_placeholder, pos + new_prefix.size());
                }

                std::ofstream fo;
                text_mode ? fo.open(dst) : fo.open(dst, std::ios::binary);
                fo << buffer;
                fo.close();

                fs::permissions(dst, fs::status(src).permissions());
            }
            return validate::sha256sum(dst);
        }

        if (path_type == "hardlink")
        {
            LOG_INFO << "linked " << src << "-->" << dst << std::endl;
            fs::create_hard_link(src, dst);
        }
        else if (path_type == "softlink")
        {
            // if (fs::islink())
            // fs::path link_target = fs::read_symlink(src);
            LOG_INFO << "soft linked " << src << " -> " << dst;
            fs::copy_symlink(src, dst);
        }
        else
        {
            throw std::runtime_error("Path type not implemented: " + path_type);
        }
        // TODO we could also use the SHA256 sum of the paths json
        return validate::sha256sum(dst);
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
        // TODO use the real python file here?!
        std::vector<std::string> command = {
            std::string(m_context->target_prefix / "bin" / "python"), 
            "-Wi", "-m", "compileall", "-q", "-l", "-i",
            all_py_files.path()
        };

        auto py_ver_split = pystring::split(m_context->python_version, ".");

        if (std::stoi(std::string(py_ver_split[0])) >= 3 && std::stoi(std::string(py_ver_split[1])) > 5)
        {
            // activate parallel pyc compilation
            command.push_back("-j0");
        }

        auto out = subprocess::check_output(command, subprocess::cwd{std::string(m_context->target_prefix)});

        return pyc_files;
    }

    bool LinkPackage::execute()
    {
        std::cout << "EXECUTE INSTALL " << m_source;
        nlohmann::json paths_json, index_json, out_json;
        LOG_WARNING << "Opening: " << m_source / "info" / "paths.json";

        std::ifstream paths_file(m_source / "info" / "paths.json");
        paths_file >> paths_json;

        LOG_WARNING << "Opening: " << m_source / "info" / "repodata_record.json";
        std::ifstream repodata_f(m_source / "info" / "repodata_record.json");

        repodata_f >> index_json;

        bool noarch_python = false;

        // handle noarch packages
        if (index_json.find("noarch") != index_json.end())
        {
            if (index_json["noarch"].get<std::string>() == "python")
            {
                noarch_python = true;
                LOG_INFO << "Installing Python noarch package";
            }
        }

        std::vector<std::string> files_record;

        for (auto& path : paths_json["paths"])
        {
            auto sha256_in_prefix = link_path(path, noarch_python);
            files_record.push_back(path["_path"].get<std::string>());
            path["sha256_in_prefix"] = sha256_in_prefix;
        }
 
        std::string f_name = index_json["name"].get<std::string>() + "-" + 
                             index_json["version"].get<std::string>() + "-" + 
                             index_json["build"].get<std::string>();

        out_json = index_json;
        out_json["paths_data"] = paths_json;
        out_json["files"] = files_record;
        out_json["requested_spec"] = "TODO";
        out_json["package_tarball_full_path"] = std::string(m_source) + ".tar.bz2";
        out_json["extracted_package_dir"] = m_source;

        // TODO find out what `1` means
        out_json["link"] = {
            {"source", std::string(m_source)},
            {"type", 1}
        };

        if (noarch_python)
        {
            fs::path link_json_path = m_source / "info" / "link.json";
            nlohmann::json link_json;
            if (fs::exists(link_json_path))
            {
                std::ifstream link_json_file(link_json_path);
                link_json_file >> link_json;
            }

            std::vector<fs::path> for_compilation;
            std::regex py_file_re("^site-packages[/\\\\][^\\t\\n\\r\\f\\v]+\\.py$");
            for (auto& sub_path_json : paths_json["paths"])
            {
                std::string path = sub_path_json["_path"];
                if (std::regex_match(path, py_file_re))
                {
                    for_compilation.push_back(get_python_noarch_target_path(path, m_context->site_packages_path));
                    LOG_WARNING << get_python_noarch_target_path(path, m_context->site_packages_path);
                }
            }
            std::vector<fs::path> pyc_files = compile_pyc_files(for_compilation);

            for (const fs::path& pyc_path : pyc_files)
            {
                out_json["paths_data"]["paths"].push_back(
                {
                    {"_path", std::string(pyc_path)},
                    {"path_type", "pyc_file"}
                });

                out_json["files"].push_back(pyc_path);
            }

            if (link_json.find("noarch") != link_json.end() && 
                link_json["noarch"].find("entry_points") != link_json["noarch"].end())
            {
                for (auto& ep : link_json["noarch"]["entry_points"])
                {
                    // install entry points
                    auto entry_point_parsed = parse_entry_point(ep.get<std::string>());
                    auto entry_point_path = get_bin_directory_short_path() / entry_point_parsed.command;
                    create_python_entry_point(m_context->target_prefix / entry_point_path,
                                              m_context->target_prefix / m_context->python_path,
                                              entry_point_parsed);
                    
                    out_json["paths_data"]["paths"].push_back(
                    {
                        {"_path", std::string(entry_point_path)},
                        {"type", "unix_python_entry_point"}
                    });
                    out_json["files"].push_back(entry_point_path);
                }
            }
        }

        // bool run_script(const fs::path& prefix, const std::string& name,
        //                 const std::string& action = "post-link",
        //                 const std::string& env_prefix = "",
        //                 bool activate = false)

        bool did_run = run_script(m_context->target_prefix, "widgetsnbextension", "post-link", "", true);
        if (!did_run)
        {
            throw std::runtime_error("ULALAL");
        }

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
}