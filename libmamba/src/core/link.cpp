// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <regex>
#include <string>
#include <tuple>
#include <vector>

#include "termcolor/termcolor.hpp"
#include <reproc++/run.hpp>

#include "mamba/core/environment.hpp"
#include "mamba/core/menuinst.hpp"
#include "mamba/core/link.hpp"
#include "mamba/core/match_spec.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/transaction_context.hpp"
#include "mamba/core/util.hpp"
#include "mamba/core/validate.hpp"
#include "mamba/core/shell_init.hpp"
#include "mamba/core/activation.hpp"

#if _WIN32
#include "../data/conda_exe.hpp"
#endif

namespace mamba
{
    static const std::regex MENU_PATH_REGEX("^menu[/\\\\].*\\.json$", std::regex_constants::icase);

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

    static std::regex shebang_regex(
        "^(#!"                    // pretty much the whole match string
        "(?:[ ]*)"                // allow spaces between #! and beginning of the executable path
        "(/(?:\\ |[^ \n\r\t])*)"  // the executable is the next text block without an escaped space
                                  // or non-space whitespace character
        "(.*))$");                // end whole_shebang group

    std::string replace_long_shebang(const std::string& shebang)
    {
        if (shebang.size() <= 127)
        {
            return shebang;
        }
        else
        {
            assert(shebang.substr(0, 2) == "#!");
            std::smatch match;
            if (std::regex_match(shebang, match, shebang_regex))
            {
                fs::path shebang_path = match[2].str();
                LOG_INFO << "New shebang path " << shebang_path;
                return concat("#!/usr/bin/env ", shebang_path.filename().string(), match[3].str());
            }
            else
            {
                LOG_WARNING << "Could not replace shebang (" << shebang << ")";
                return shebang;
            }
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
        std::ofstream out_file = open_ofstream(script_path);

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

        std::ofstream conda_exe_f
            = open_ofstream(m_context->target_prefix / script_exe, std::ios::binary);
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

        std::ofstream out_file = open_ofstream(target_full_path);
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
                std::ifstream msgs = open_ifstream(messages_file);
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
            LOG_DEBUG << action << " script for '" << pkg_info.name << "' does not exist ('"
                      << path.string() << "')";
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
            auto comspec = env::get("COMSPEC");
            if (!comspec)
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

                command_args = { comspec.value(), "/d", "/c", script_file->path() };
            }
            else
            {
                command_args = { comspec.value(), "/d", "/c", path };
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

        std::string PATH = env::get("PATH").value_or("");
        envmap["PATH"] = concat(path.parent_path().c_str(), env::pathsep(), PATH);

        std::string cargs = join(" ", command_args);
        LOG_DEBUG << "For " << pkg_info.name << " at " << envmap["PREFIX"]
                  << ", executing script: $ " << cargs;
        LOG_TRACE << "Calling " << cargs;

        reproc::options options;
        options.redirect.parent = true;
        options.env.behavior = reproc::env::extend;
        options.env.extra = envmap;
        std::string cwd = path.parent_path();
        options.working_directory = cwd.c_str();

        LOG_TRACE << "ENV MAP:"
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
            if (script_file != nullptr && env::get("CONDA_TEST_SAVE_TEMPS"))
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

        LOG_TRACE << "Unlinking '" << dst.string() << "'";
        std::error_code err;

        if (remove_or_rename(dst) == 0)
            LOG_DEBUG << "Error when removing file '" << dst.string() << "' will be ignored";

        // TODO what do we do with empty directories?
        // remove empty parent path
        auto parent_path = dst.parent_path();
        while (true)
        {
            bool exists = fs::exists(parent_path, err);
            if (err)
                break;
            if (exists)
            {
                bool is_empty = fs::is_empty(parent_path, err);
                if (err)
                    break;
                if (is_empty)
                {
                    remove_or_rename(parent_path);
                }
                else
                {
                    break;
                }
            }
            parent_path = parent_path.parent_path();
            if (parent_path == m_context->target_prefix)
            {
                break;
            }
        }
        return true;
    }

    bool UnlinkPackage::execute()
    {
        // find the recorded JSON file
        fs::path json = m_context->target_prefix / "conda-meta" / (m_specifier + ".json");
        LOG_INFO << "Unlinking package '" << m_specifier << "'";
        LOG_DEBUG << "Use metadata found at '" << json.string() << "'";

        std::ifstream json_file = open_ifstream(json);
        nlohmann::json json_record;
        json_file >> json_record;

        for (auto& path : json_record["paths_data"]["paths"])
        {
            std::string fpath = path["_path"];
            if (std::regex_match(fpath, MENU_PATH_REGEX))
            {
                remove_menu_from_json(m_context->target_prefix / fpath, m_context);
            }

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
        LOG_TRACE << "linking '" << subtarget << "'";
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
            LOG_WARNING << "Clobberwarning: $CONDA_PREFIX/" << rel_dst.string();
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
            LOG_TRACE << "Copying file & replace prefix " << src << " -> " << dst;
            // TODO windows does something else here

            std::string buffer;
            if (path_data.file_mode != FileMode::BINARY)
            {
                buffer = read_contents(src, std::ios::in | std::ios::binary);
                replace_all(buffer, path_data.prefix_placeholder, new_prefix);

                // we need to check the first line for a shebang and replace it if it's too long
                if (!on_win && buffer[0] == '#' && buffer[1] == '!')
                {
                    std::size_t end_of_line = buffer.find_first_of('\n');
                    std::string first_line = buffer.substr(0, end_of_line);
                    if (first_line.size() > 127)
                    {
                        std::string new_shebang = replace_long_shebang(first_line);
                        buffer.replace(0, end_of_line, new_shebang);
                    }
                }
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
                        std::ofstream fo = open_ofstream(dst, std::ios::out | std::ios::binary);
                        fo << launcher << shebang << (buffer.c_str() + arc_pos);
                        fo.close();
                    }
                    return std::make_tuple(validate::sha256sum(dst), rel_dst);
                }
#else
                std::size_t padding_size
                    = (path_data.prefix_placeholder.size() > new_prefix.size())
                          ? path_data.prefix_placeholder.size() - new_prefix.size()
                          : 0;
                std::string padding(padding_size, '\0');

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

                    std::string replacement = concat(new_prefix, suffix, padding);
                    buffer.replace(pos, end - pos, replacement);

                    pos = buffer.find(path_data.prefix_placeholder, pos + new_prefix.size());
                }
#endif
            }

            std::ofstream fo = open_ofstream(dst, std::ios::out | std::ios::binary);
            fo << buffer;
            fo.close();

            fs::permissions(dst, fs::status(src).permissions());
#if defined(__APPLE__)
            if (binary_changed && m_pkg_info.subdir == "osx-arm64")
            {
                reproc::options options;
                if (Context::instance().verbosity <= 1)
                {
                    reproc::redirect silence;
                    silence.type = reproc::redirect::discard;
                    options.redirect.out = silence;
                    options.redirect.err = silence;
                }

                std::vector<std::string> cmd
                    = { "/usr/bin/codesign", "-s", "-", "-f", dst.string() };
                auto [status, ec] = reproc::run(cmd, options);
                if (ec)
                {
                    throw std::runtime_error(std::string("Could not codesign executable")
                                             + ec.message());
                }
            }
#endif
            return std::make_tuple(validate::sha256sum(dst), rel_dst);
        }

        if ((path_data.path_type == PathType::HARDLINK) || path_data.no_link)
        {
            bool copy = path_data.no_link || m_context->always_copy;
            bool softlink = m_context->always_softlink;

            if (!copy && !softlink)
            {
                std::error_code ec;
                fs::create_hard_link(src, dst, ec);

                if (ec)
                {
                    softlink = m_context->allow_softlinks;
                    copy = !softlink;
                }
                else
                {
                    LOG_TRACE << "hard-linked '" << src.string() << "'" << std::endl
                              << " --> '" << dst.string() << "'";
                }
            }
            if (softlink)
            {
                std::error_code ec;
                fs::create_symlink(src, dst, ec);
                if (ec)
                {
                    copy = true;
                }
                else
                {
                    LOG_TRACE << "soft-linked '" << src.string() << "'" << std::endl
                              << " --> '" << dst.string() << "'";
                }
            }
            if (copy)
            {
                fs::copy(src, dst);
                LOG_TRACE << "copied '" << src.string() << "'" << std::endl
                          << " --> '" << dst.string() << "'";
            }
        }
        else if (path_data.path_type == PathType::SOFTLINK)
        {
            LOG_TRACE << "soft-linked '" << src.string() << "'" << std::endl
                      << " --> '" << dst.string() << "'";
            fs::copy_symlink(src, dst);
            // we need to wait until all files are linked to compute the SHA256 sum!
            // otherwise the file that's pointed to might not be linked yet.
            return std::make_tuple("", rel_dst);
        }
        else
        {
            throw std::runtime_error(std::string("Path type not implemented: ")
                                     + std::to_string(static_cast<int>(path_data.path_type)));
        }
        return std::make_tuple(
            path_data.sha256.empty() ? validate::sha256sum(dst) : path_data.sha256, rel_dst);
    }

    std::vector<fs::path> LinkPackage::compile_pyc_files(const std::vector<fs::path>& py_files)
    {
        if (py_files.size() == 0)
            return {};

        if (!m_context->has_python)
        {
            LOG_WARNING << "Can't compile pyc: Python not found";
            return {};
        }

        std::vector<fs::path> pyc_files;
        TemporaryFile all_py_files;
        std::ofstream all_py_files_f = open_ofstream(all_py_files.path());

        for (auto& f : py_files)
        {
            all_py_files_f << f.c_str() << '\n';
            pyc_files.push_back(pyc_path(f, m_context->short_python_version));
            LOG_TRACE << "Compiling " << pyc_files.back();
        }
        LOG_INFO << "Compiling " << pyc_files.size() << " python files for " << m_pkg_info.name
                 << " to pyc";

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

        try
        {
            if (std::stoull(std::string(py_ver_split[0])) >= 3
                && std::stoull(std::string(py_ver_split[1])) > 5)
            {
                // activate parallel pyc compilation
                command.push_back("-j0");
            }
        }
        catch (const std::exception& e)
        {
            LOG_ERROR << "Bad conversion of Python version '" << m_context->python_version
                      << "': " << e.what();
            throw std::runtime_error("Bad conversion. Aborting.");
        }

        reproc::options options;
        std::string out, err;

        std::string cwd = m_context->target_prefix;
        options.working_directory = cwd.c_str();

        auto [wrapped_command, script_file]
            = prepare_wrapped_call(m_context->target_prefix, command);

        LOG_DEBUG << "Running wrapped python compilation command " << join(" ", command);
        auto [_, ec] = reproc::run(
            wrapped_command, options, reproc::sink::string(out), reproc::sink::string(err));

        if (ec || !err.empty())
        {
            LOG_INFO << "noarch pyc compilation failed (cross-compiling?). " << ec.message();
            LOG_INFO << err;
        }

        std::vector<fs::path> final_pyc_files;
        for (auto& f : pyc_files)
        {
            if (fs::exists(m_context->target_prefix / f))
            {
                final_pyc_files.push_back(f);
            }
            else if (!ec)
            {
                LOG_WARNING << "Python file couldn't be compiled to pyc: " << f;
            }
        }

        return final_pyc_files;
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
        nlohmann::json index_json, out_json;
        LOG_TRACE << "Preparing linking from '" << m_source.string() << "'";

        LOG_TRACE << "Opening: " << m_source / "info" / "paths.json";
        auto paths_data = read_paths(m_source);

        LOG_TRACE << "Opening: " << m_source / "info" / "repodata_record.json";

        std::ifstream repodata_f = open_ifstream(m_source / "info" / "repodata_record.json");
        repodata_f >> index_json;

        std::string f_name = m_pkg_info.str();

        LOG_DEBUG << "Linking package '" << f_name << "' from '" << m_source.string() << "'";

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
                }
                else if (na_t == "generic")
                {
                    noarch_type = NoarchType::GENERIC_V2;
                }
            }
        }

        std::vector<std::string> files_record;

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

        for (std::size_t i = 0; i < paths_data.size(); ++i)
        {
            auto& path = paths_data[i];
            if (path.path_type == PathType::SOFTLINK)
            {
                // here we try to avoid recomputing the costly sha256 sum
                std::error_code ec;
                auto points_to = fs::canonical(m_context->target_prefix / files_record[i], ec);
                bool found = false;
                if (!ec)
                {
                    for (std::size_t pix = 0; pix < files_record.size(); ++pix)
                    {
                        if ((m_context->target_prefix / files_record[pix]) == points_to)
                        {
                            if (paths_json["paths"][pix].contains("sha256_in_prefix"))
                            {
                                LOG_TRACE << "Found symlink and target " << files_record[i]
                                          << " -> " << files_record[pix];
                                // use already computed value
                                paths_json["paths"][i]["sha256_in_prefix"]
                                    = paths_json["paths"][pix]["sha256_in_prefix"];
                                found = true;
                                break;
                            }
                        }
                    }
                }
                if (!found)
                {
                    paths_json["paths"][i]["sha256_in_prefix"]
                        = validate::sha256sum(m_context->target_prefix / files_record[i]);
                }
            }
        }

        LOG_DEBUG << paths_data.size() << " files linked";

        out_json = index_json;
        out_json["paths_data"] = paths_json;
        out_json["files"] = files_record;

        MatchSpec* requested_spec = nullptr;
        for (auto& ms : m_context->requested_specs)
        {
            if (ms.name == m_pkg_info.name)
            {
                requested_spec = &ms;
            }
        }
        out_json["requested_spec"] = requested_spec != nullptr ? requested_spec->str() : "";
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
                std::ifstream link_json_file = open_ifstream(link_json_path);
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

            if (m_context->compile_pyc)
            {
                std::vector<fs::path> pyc_files = compile_pyc_files(for_compilation);

                for (const fs::path& pyc_path : pyc_files)
                {
                    out_json["paths_data"]["paths"].push_back(
                        { { "_path", std::string(pyc_path) }, { "path_type", "pyc_file" } });

                    out_json["files"].push_back(pyc_path);
                }
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
                    LOG_TRACE << "entry point path: " << entry_point_path << std::endl;
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

        // Create all start menu shortcuts if prefix name doesn't start with underscore
        if (on_win && Context::instance().shortcuts
            && m_context->target_prefix.filename().string()[0] != '_')
        {
            for (auto& path : paths_data)
            {
                if (std::regex_match(path.path, MENU_PATH_REGEX))
                {
                    create_menu_from_json(m_context->target_prefix / path.path, m_context);
                }
            }
        }

        run_script(m_context->target_prefix, m_pkg_info, "post-link", "", true);

        fs::path prefix_meta = m_context->target_prefix / "conda-meta";
        if (!fs::exists(prefix_meta))
        {
            fs::create_directory(prefix_meta);
        }

        LOG_DEBUG << "Finalizing linking";
        auto meta = prefix_meta / (f_name + ".json");
        LOG_TRACE << "Adding package to prefix metadata at '" << meta.string() << "'";
        std::ofstream out_file = open_ofstream(meta);
        out_file << out_json.dump(4);

        return true;
    }

    bool LinkPackage::undo()
    {
        UnlinkPackage ulp(m_pkg_info, m_cache_path, m_context);
        return ulp.execute();
    }
}  // namespace mamba
