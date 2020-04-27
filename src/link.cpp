#include <regex>

#include "link.hpp"
#include "output.hpp"
#include "validate.hpp"
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
            return fs::path("__pycache__") / concat(py_file_stem.c_str(), ".cpython-", py_ver_nodot, ".pyc");
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

    // def replace_long_shebang(mode, data):
    //     # this function only changes a shebang line if it exists and is greater than 127 characters
    //     if mode == FileMode.text:
    //         if not isinstance(data, bytes):
    //             try:
    //                 data = bytes(data, encoding='utf-8')
    //             except:
    //                 data = data.encode('utf-8')

    //         shebang_match = re.match(SHEBANG_REGEX, data, re.MULTILINE)
    //         if shebang_match:
    //             whole_shebang, executable, options = shebang_match.groups()
    //             if len(whole_shebang) > 127:
    //                 executable_name = executable.decode('utf-8').split('/')[-1]
    //                 new_shebang = '#!/usr/bin/env %s%s' % (executable_name, options.decode('utf-8'))
    //                 data = data.replace(whole_shebang, new_shebang.encode('utf-8'))

    //     else:
    //         # TODO: binary shebangs exist; figure this out in the future if text works well
    //         pass
    //     return data


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

    // {
    //     def compile_multiple_pyc(python_exe_full_path, py_full_paths, pyc_full_paths, prefix, py_ver):
    //         py_full_paths = tuple(py_full_paths)
    //         pyc_full_paths = tuple(pyc_full_paths)
    //         if len(py_full_paths) == 0:
    //             return []

    //         fd, filename = tempfile.mkstemp()
    //         try:
    //             for f in py_full_paths:
    //                 f = os.path.relpath(f, prefix)
    //                 if hasattr(f, 'encode'):
    //                     f = f.encode(sys.getfilesystemencoding(), errors='replace')
    //                 os.write(fd, f + b"\n")
    //             os.close(fd)
    //             command = ["-Wi", "-m", "compileall", "-q", "-l", "-i", filename]
    //             # if the python version in the prefix is 3.5+, we have some extra args.
    //             #    -j 0 will do the compilation in parallel, with os.cpu_count() cores
    //             if int(py_ver[0]) >= 3 and int(py_ver.split('.')[1]) > 5:
    //                 command.extend(["-j", "0"])
    //             command[0:0] = [python_exe_full_path]
    //             # command[0:0] = ['--cwd', prefix, '--dev', '-p', prefix, python_exe_full_path]
    //             log.trace(command)
    //             from conda.gateways.subprocess import any_subprocess
    //             # from conda.common.io import env_vars
    //             # This stack does not maintain its _argparse_args correctly?
    //             # from conda.base.context import stack_context_default
    //             # with env_vars({}, stack_context_default):
    //             #     stdout, stderr, rc = run_command(Commands.RUN, *command)
    //             stdout, stderr, rc = any_subprocess(command, prefix)
    //         finally:
    //             os.remove(filename)

    //         created_pyc_paths = []
    //         for py_full_path, pyc_full_path in zip(py_full_paths, pyc_full_paths):
    //             if not isfile(pyc_full_path):
    //                 message = dals("""
    //                 pyc file failed to compile successfully (run_command failed)
    //                 python_exe_full_path: %s
    //                 py_full_path: %s
    //                 pyc_full_path: %s
    //                 compile rc: %s
    //                 compile stdout: %s
    //                 compile stderr: %s
    //                 """)
    //                 log.info(message, python_exe_full_path, py_full_path, pyc_full_path,
    //                          rc, stdout, stderr)
    //             else:
    //                 created_pyc_paths.append(pyc_full_path)

    //         return created_pyc_paths
    // }

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
            all_py_files.path().c_str()
        };

        auto py_ver_split = pystring::split(m_context->python_version, ".");

        if (std::stoi(std::string(py_ver_split[0])) >= 3 && std::stoi(std::string(py_ver_split[1])) > 5)
        {
            command.push_back("-j0");
        }

        // Also looks good: https://github.com/tsaarni/cpp-subprocess/blob/master/include/subprocess.hpp
        auto out = subprocess::check_output(command, subprocess::cwd{std::string(m_context->target_prefix)});

        std::cout << "Data : " << out.buf.data() << std::endl;
        std::cout << "Data len: " << out.length << std::endl;

        return pyc_files;
    }



    bool LinkPackage::execute()
    {
        nlohmann::json paths_json, index_json, out_json;
        std::ifstream paths_file(m_source / "info" / "paths.json");

        paths_file >> paths_json;
        std::ifstream repodata_f(m_source / "info" / "repodata_record.json");
        repodata_f >> index_json;

        bool noarch_python = false;
        // handle noarch packages
        if (index_json.find("noarch") != index_json.end())
        {
            if (index_json["noarch"].get<std::string>() == "python")
            {
                noarch_python = true;
                LOG_INFO << "Install Python noarch package";
            }
        }

        // TODO should we take this from `files.txt`?
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
            if (fs::exists(link_json_path))
            {
                std::ifstream link_json_file(link_json_path);
                nlohmann::json link_json;
                link_json_file >> link_json;
                // {
                //   "noarch": {
                //     "entry_points": [
                //       "wheel = wheel.cli:main"
                //     ],
                //     "type": "python"
                //   },
                //   "package_metadata_version": 1
                // }
                LOG_WARNING << "Creating entry points ... ";
                for (auto& ep : link_json["noarch"]["entry_points"])
                {
                    // install entry points
                    auto entry_point_parsed = parse_entry_point(ep.get<std::string>());
                    LOG_WARNING << "Entry point: " << entry_point_parsed.command << ", " << entry_point_parsed.module << ", " << entry_point_parsed.func;
                    create_python_entry_point(m_context->target_prefix / get_bin_directory_short_path() / entry_point_parsed.command,
                                              m_context->target_prefix / m_context->python_path,
                                              entry_point_parsed);
                }
            }

            std::regex py_file_re("^site-packages[/\\\\][^\\t\\n\\r\\f\\v]+\\.py$");
            std::vector<fs::path> for_compilation;
            for (auto& sub_path_json : paths_json["paths"])
            {
                std::string path = sub_path_json["_path"];
                LOG_WARNING << "Checking file " << path << " for compilation";
                if (std::regex_match(path, py_file_re))
                {
                    LOG_WARNING << "Checking file " << path << " <<< MATCHES!";
                    for_compilation.push_back(get_python_noarch_target_path(path, m_context->site_packages_path));
                }
            }
            std::vector<fs::path> pyc_files = compile_pyc_files(for_compilation);
        }

        fs::path prefix_meta = m_context->target_prefix / "conda-meta";
        if (!fs::exists(prefix_meta))
        {
            fs::create_directory(prefix_meta);
        }
        LOG_INFO << "Writing out.json";

        std::ofstream out_file(prefix_meta / (f_name + ".json"));
        out_file << out_json.dump(4);
    }
}