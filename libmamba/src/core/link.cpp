// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <iostream>
#include <regex>
#include <string>
#include <tuple>
#include <vector>

#include <reproc++/reproc.hpp>
#include <reproc++/run.hpp>

#include "mamba/core/link.hpp"
#include "mamba/core/menuinst.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/transaction_context.hpp"
#include "mamba/specs/match_spec.hpp"
#include "mamba/util/build.hpp"
#include "mamba/util/environment.hpp"
#include "mamba/util/string.hpp"
#include "mamba/validation/tools.hpp"

#ifdef __APPLE__
#include "mamba/core/util_os.hpp"
#endif

#if _WIN32
#include "../data/conda_exe.hpp"
#endif

namespace mamba
{
    static const std::regex MENU_PATH_REGEX("^menu[/\\\\].*\\.json$", std::regex_constants::icase);

    void python_entry_point_template(std::ostream& out, const python_entry_point_parsed& p)
    {
        auto import_name = util::split(p.func, ".")[0];
        out << "# -*- coding: utf-8 -*-\n";
        out << "import re\n";
        out << "import sys\n\n";

        out << "from " << p.module << " import " << import_name << "\n\n";

        out << "if __name__ == '__main__':\n";
        out << "    sys.argv[0] = re.sub(r'(-script\\.pyw?|\\.exe)?$', '', "
               "sys.argv[0])\n";
        out << "    sys.exit(" << p.func << "())\n";
    }

    void application_entry_point_template(std::ostream& out, std::string_view source_full_path)
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

    fs::u8path pyc_path(const fs::u8path& py_path, const std::string& py_ver)
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
            return util::concat(py_path.string(), 'c');
        }
        else
        {
            auto directory = py_path.parent_path();
            auto py_file_stem = py_path.stem();
            std::string py_ver_nodot = py_ver;
            util::replace_all(py_ver_nodot, ".", "");
            return directory / fs::u8path("__pycache__")
                   / util::concat(py_file_stem.string(), ".cpython-", py_ver_nodot, ".pyc");
        }
    }

    python_entry_point_parsed parse_entry_point(const std::string& ep_def)
    {
        // def looks like: "wheel = wheel.cli:main"
        auto cmd_mod_func = util::rsplit(ep_def, ":", 1);
        auto command_module = util::rsplit(cmd_mod_func[0], "=", 1);
        python_entry_point_parsed result;
        result.command = util::strip(command_module[0]);
        result.module = util::strip(command_module[1]);
        result.func = util::strip(cmd_mod_func[1]);
        return result;
    }

    std::string replace_long_shebang(const std::string& shebang)
    {
        if (shebang.size() <= MAX_SHEBANG_LENGTH)
        {
            return shebang;
        }
        else
        {
            assert(shebang.substr(0, 2) == "#!");
            std::smatch match;
            if (std::regex_match(shebang, match, shebang_regex))
            {
                fs::u8path shebang_path = match[2].str();
                LOG_INFO << "New shebang path " << shebang_path;
                return util::concat("#!/usr/bin/env ", shebang_path.filename().string(), match[3].str());
            }
            else
            {
                LOG_WARNING << "Could not replace shebang (" << shebang << ")";
                return shebang;
            }
        }
    }

    std::string python_shebang(const std::string& python_exe)
    {
        // Shebangs cannot be longer than 127 (or 512) characters and executable with
        // spaces are problematic
        if (python_exe.size() > (MAX_SHEBANG_LENGTH - 2)
            || python_exe.find_first_of(" ") != std::string::npos)
        {
            return fmt::format("#!/bin/sh\n'''exec' \"{}\" \"$0\" \"$@\" #'''", python_exe);
        }
        else
        {
            return fmt::format("#!{}", python_exe);
        }
    }

    // for noarch python packages that have entry points
    auto LinkPackage::create_python_entry_point(
        const fs::u8path& path,
        const python_entry_point_parsed& entry_point
    )
    {
        const fs::u8path& target_prefix = m_context->prefix_params().target_prefix;
#ifdef _WIN32
        // We add -script.py to WIN32, and link the conda.exe launcher which will
        // automatically find the correct script to launch
        std::string win_script = path.string() + "-script.py";
        std::string win_script_gen_str = path.generic_string() + "-script.py";
        fs::u8path script_path = target_prefix / win_script;
#else
        fs::u8path script_path = target_prefix / path;
#endif
        if (fs::exists(script_path))
        {
            m_clobber_warnings.push_back(fs::relative(script_path, target_prefix).string());
            fs::remove(script_path);
        }
        if (!fs::is_directory(script_path.parent_path()))
        {
            fs::create_directories(script_path.parent_path());
        }
        std::ofstream out_file = open_ofstream(script_path);

        fs::u8path python_path;
        if (m_context->python_params().has_python)
        {
            python_path = m_context->prefix_params().relocate_prefix / m_context->python_params().python_path;
        }
        if (!python_path.empty())
        {
            out_file << python_shebang(python_path.string()) << "\n";
        }

        python_entry_point_template(out_file, entry_point);
        out_file.close();

#ifdef _WIN32
        fs::u8path script_exe = path;
        script_exe.replace_extension("exe");

        if (fs::exists(target_prefix / script_exe))
        {
            m_clobber_warnings.push_back(fs::relative(script_exe.string()).string());
            fs::remove(target_prefix / script_exe);
        }

        std::ofstream conda_exe_f = open_ofstream(
            target_prefix / script_exe,
            std::ios::binary
        );
        conda_exe_f.write(reinterpret_cast<char*>(conda_exe), conda_exe_len);
        conda_exe_f.close();
        make_executable(target_prefix / script_exe);
        return std::array<std::string, 2>{ win_script_gen_str, script_exe.generic_string() };
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
            return util::concat(pad, str, pad);
        }
    }

    std::string win_path_double_escape(const std::string& path)
    {
#ifdef _WIN32
        std::string path_copy = path;
        util::replace_all(path_copy, "\\", "\\\\");
        return path_copy;
#else
        return path;
#endif
    }

    void LinkPackage::create_application_entry_point(
        const fs::u8path& source_full_path,
        const fs::u8path& target_full_path,
        const fs::u8path& python_full_path
    )
    {
        // source_full_path: where the entry point file points to
        // target_full_path: the location of the new entry point file being created
        if (fs::exists(target_full_path))
        {
            m_clobber_warnings.push_back(target_full_path.string());
        }

        if (!fs::is_directory(target_full_path.parent_path()))
        {
            fs::create_directories(target_full_path.parent_path());
        }

        std::ofstream out_file = open_ofstream(target_full_path);
        out_file << "!#" << python_full_path.string() << "\n";
        application_entry_point_template(out_file, win_path_double_escape(source_full_path.string()));
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

    std::string get_prefix_messages(const fs::u8path& prefix)
    {
        auto messages_file = prefix / ".messages.txt";
        if (fs::exists(messages_file))
        {
            try
            {
                std::ifstream msgs = open_ifstream(messages_file);
                std::stringstream res;
                std::copy(
                    std::istreambuf_iterator<char>(msgs),
                    std::istreambuf_iterator<char>(),
                    std::ostreambuf_iterator<char>(res)
                );
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
    bool run_script(
        const Context& context,
        const fs::u8path& prefix,
        const specs::PackageInfo& pkg_info,
        const std::string& action = "post-link",
        const std::string& env_prefix = "",
        bool activate = false
    )
    {
        fs::u8path path;
        if (util::on_win)
        {
            path = prefix / get_bin_directory_short_path()
                   / util::concat(".", pkg_info.name, "-", action, ".bat");
        }
        else
        {
            path = prefix / get_bin_directory_short_path()
                   / util::concat(".", pkg_info.name, "-", action, ".sh");
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

        if (util::on_win)
        {
            ensure_comspec_set();
            auto comspec = util::get_env("COMSPEC");
            if (!comspec)
            {
                LOG_ERROR << "Failed to run " << action << " for " << pkg_info.name
                          << " due to COMSPEC not set in env vars.";
                return false;
            }

            if (activate)
            {
                script_file = wrap_call(
                    context.prefix_params.root_prefix,
                    prefix,
                    { "@CALL", path.string() },
                    context.command_params.is_mamba_exe
                );

                command_args = { comspec.value(), "/d", "/c", script_file->path().string() };
            }
            else
            {
                command_args = { comspec.value(), "/d", "/c", path.string() };
            }
        }

        else
        {
            // shell_path = 'sh' if 'bsd' in sys.platform else 'bash'
            fs::u8path shell_path = util::which("bash");
            if (shell_path.empty())
            {
                shell_path = util::which("sh");
            }

            if (activate)
            {
                // std::string caller
                script_file = wrap_call(
                    context.prefix_params.root_prefix.string(),
                    prefix,
                    { ".", path.string() },
                    context.command_params.is_mamba_exe
                );
                command_args.push_back(shell_path.string());
                command_args.push_back(script_file->path().string());
            }
            else
            {
                command_args.push_back(shell_path.string());
                command_args.push_back("-x");
                command_args.push_back(path.string());
            }
        }

        envmap["ROOT_PREFIX"] = context.prefix_params.root_prefix.string();
        envmap["PREFIX"] = env_prefix.size() ? env_prefix : prefix.string();
        envmap["PKG_NAME"] = pkg_info.name;
        envmap["PKG_VERSION"] = pkg_info.version;
        envmap["PKG_BUILDNUM"] = std::to_string(pkg_info.build_number);

        std::string PATH = util::get_env("PATH").value_or("");
        envmap["PATH"] = util::concat(path.parent_path().string(), util::pathsep(), PATH);

        std::string cargs = util::join(" ", command_args);
        LOG_DEBUG << "For " << pkg_info.name << " at " << envmap["PREFIX"]
                  << ", executing script: $ " << cargs;
        LOG_TRACE << "Calling " << cargs;

        reproc::options options;
        options.redirect.parent = true;

        options.env.behavior = reproc::env::extend;
        options.env.extra = envmap;

        const std::string cwd = path.parent_path().string();
        options.working_directory = cwd.c_str();

        LOG_TRACE << "ENV MAP:" << "\n ROOT_PREFIX: " << envmap["ROOT_PREFIX"]
                  << "\n PREFIX: " << envmap["PREFIX"] << "\n PKG_NAME: " << envmap["PKG_NAME"]
                  << "\n PKG_VERSION: " << envmap["PKG_VERSION"]
                  << "\n PKG_BUILDNUM: " << envmap["PKG_BUILDNUM"] << "\n PATH: " << envmap["PATH"]
                  << "\n CWD: " << cwd;

        auto [status, ec] = reproc::run(command_args, options);

        auto msg = get_prefix_messages(envmap["PREFIX"]);
        if (context.output_params.json)
        {
            // TODO implement cerr also on Console?
            std::cerr << msg;
        }
        else
        {
            Console::instance().print(msg);
        }

        if (ec)
        {
            LOG_ERROR << "response code: " << status << " error message: " << ec.message();
            if (script_file != nullptr && util::get_env("CONDA_TEST_SAVE_TEMPS"))
            {
                LOG_ERROR << "CONDA_TEST_SAVE_TEMPS :: retaining run_script" << script_file->path();
            }
            throw std::runtime_error("failed to execute pre/post link script for " + pkg_info.name);
        }
        return true;
    }

    UnlinkPackage::UnlinkPackage(
        const specs::PackageInfo& pkg_info,
        const fs::u8path& cache_path,
        TransactionContext* context
    )
        : m_pkg_info(pkg_info)
        , m_cache_path(cache_path)
        , m_specifier(m_pkg_info.str())
        , m_context(context)
    {
        assert(m_context != nullptr);
    }

    bool UnlinkPackage::unlink_path(const nlohmann::json& path_data)
    {
        const auto& context = m_context->context();
        std::string subtarget = path_data["_path"].get<std::string>();
        fs::u8path dst = m_context->prefix_params().target_prefix / subtarget;

        LOG_TRACE << "Unlinking '" << dst.string() << "'";
        std::error_code err;

        if (remove_or_rename(context, dst) == 0)
        {
            LOG_DEBUG << "Error when removing file '" << dst.string() << "' will be ignored";
        }

        // TODO what do we do with empty directories?
        // remove empty parent path
        auto parent_path = dst.parent_path();
        while (true)
        {
            bool exists = fs::exists(parent_path, err);
            if (err)
            {
                break;
            }
            if (exists)
            {
                bool is_empty = fs::is_empty(parent_path, err);
                if (err)
                {
                    break;
                }
                if (is_empty)
                {
                    remove_or_rename(context, parent_path);
                }
                else
                {
                    break;
                }
            }
            parent_path = parent_path.parent_path();
            if (parent_path == m_context->prefix_params().target_prefix)
            {
                break;
            }
        }
        return true;
    }

    bool UnlinkPackage::execute()
    {
        // find the recorded JSON file
        fs::u8path json = m_context->prefix_params().target_prefix / "conda-meta" / (m_specifier + ".json");
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
                remove_menu_from_json(m_context->prefix_params().target_prefix / fpath, m_context);
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

    LinkPackage::LinkPackage(
        const specs::PackageInfo& pkg_info,
        const fs::u8path& cache_path,
        TransactionContext* context
    )
        : m_pkg_info(pkg_info)
        , m_cache_path(cache_path)
        , m_source(cache_path / m_pkg_info.str())
        , m_context(context)
    {
        assert(m_context != nullptr);
    }

    std::tuple<std::string, std::string>
    LinkPackage::link_path(const PathData& path_data, bool noarch_python)
    {
        std::string subtarget = path_data.path;
        LOG_TRACE << "linking '" << subtarget << "'";
        fs::u8path dst, rel_dst;
        if (noarch_python)
        {
            rel_dst = get_python_noarch_target_path(subtarget, m_context->python_params().site_packages_path);
            dst = m_context->prefix_params().target_prefix / rel_dst;
        }
        else
        {
            rel_dst = subtarget;
            dst = m_context->prefix_params().target_prefix / rel_dst;
        }

        fs::u8path src = m_source / subtarget;
        if (!fs::exists(dst.parent_path()))
        {
            fs::create_directories(dst.parent_path());
        }

        std::error_code ec;
        if (lexists(dst, ec) && !ec)
        {
            // Sometimes we might want to raise here ...
            m_clobber_warnings.push_back(rel_dst.string());
#ifdef _WIN32
            return std::make_tuple(std::string(validation::sha256sum(dst)), rel_dst.generic_string());
#endif
            fs::remove(dst);
        }
        if (ec)
        {
            LOG_WARNING << "Could not check file existence: " << ec.message() << " (" << dst << ")";
        }

#ifdef __APPLE__
        bool binary_changed = false;
#endif
        // std::string path_type = path_data["path_type"].get<std::string>();
        if (!path_data.prefix_placeholder.empty())
        {
            // we have to replace the PREFIX stuff in the data
            // and copy the file
            std::string new_prefix = m_context->prefix_params().relocate_prefix.string();
#ifdef _WIN32
            util::replace_all(new_prefix, "\\", "/");
#endif
            LOG_TRACE << "Copying file & replace prefix " << src << " -> " << dst;
            // TODO windows does something else here

            std::string buffer;
            if (path_data.file_mode != FileMode::BINARY)
            {
                buffer = read_contents(src, std::ios::in | std::ios::binary);
                util::replace_all(buffer, path_data.prefix_placeholder, new_prefix);

                if constexpr (!util::on_win)  // only on non-windows platforms
                {
                    // we need to check the first line for a shebang and replace it if it's too long
                    if (buffer[0] == '#' && buffer[1] == '!')
                    {
                        std::size_t end_of_line = buffer.find_first_of('\n');
                        std::string first_line = buffer.substr(0, end_of_line);
                        if (first_line.size() > MAX_SHEBANG_LENGTH)
                        {
                            std::string new_shebang = replace_long_shebang(first_line);
                            buffer.replace(0, end_of_line, new_shebang);
                        }
                    }
                }
            }
            else
            {
                assert(path_data.file_mode == FileMode::BINARY);
                buffer = read_contents(src, std::ios::in | std::ios::binary);

#ifdef _WIN32
                auto has_pyzzer_entrypoint = [](const std::string& data)
                { return data.rfind("PK\x05\x06"); };

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
                    pyzzer_entry = *reinterpret_cast<const pyzzer_struct*>(
                        buffer.c_str() + entry_point
                    );
                    std::size_t arc_pos = entry_point - pyzzer_entry.cdr_size
                                          - pyzzer_entry.cdr_offset;

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
                        util::replace_all(shebang, path_data.prefix_placeholder, new_prefix);
                        std::ofstream fo = open_ofstream(dst, std::ios::out | std::ios::binary);
                        fo << launcher << shebang << (buffer.c_str() + arc_pos);
                        fo.close();
                    }
                    return std::make_tuple(
                        std::string(validation::sha256sum(dst)),
                        rel_dst.generic_string()
                    );
                }
#else
                std::size_t padding_size = (path_data.prefix_placeholder.size() > new_prefix.size())
                                               ? path_data.prefix_placeholder.size()
                                                     - new_prefix.size()
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

                    std::string replacement = util::concat(new_prefix, suffix, padding);
                    buffer.replace(pos, end - pos, replacement);

                    pos = buffer.find(path_data.prefix_placeholder, pos + new_prefix.size());
                }
#endif
            }

            std::ofstream fo = open_ofstream(dst, std::ios::out | std::ios::binary);
            fo << buffer;
            fo.close();

            std::error_code lec;
            fs::permissions(dst, fs::status(src).permissions(), lec);
            if (lec)
            {
                LOG_WARNING << "Could not set permissions on [" << dst << "]: " << ec.message();
            }

#if defined(__APPLE__)
            if (binary_changed && m_pkg_info.platform == "osx-arm64")
            {
                codesign(dst, m_context->context().output_params.verbosity > 1);
            }
#endif
            return std::make_tuple(std::string(validation::sha256sum(dst)), rel_dst.generic_string());
        }

        if ((path_data.path_type == PathType::HARDLINK) || path_data.no_link)
        {
            bool copy = path_data.no_link || m_context->link_params().always_copy;
            bool softlink = m_context->link_params().always_softlink;

            if (!copy && !softlink)
            {
                std::error_code lec;
                fs::create_hard_link(src, dst, lec);

                if (lec)
                {
                    softlink = m_context->link_params().allow_softlinks;
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
                std::error_code lec;
                fs::create_symlink(src, dst, lec);
                if (lec)
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
            return std::make_tuple("", rel_dst.generic_string());
        }
        else
        {
            throw std::runtime_error(
                std::string("Path type not implemented: ")
                + std::to_string(static_cast<int>(path_data.path_type))
            );
        }
        return std::make_tuple(
            path_data.sha256.empty() ? std::string(validation::sha256sum(dst)) : path_data.sha256,
            rel_dst.generic_string()
        );
    }

    std::vector<fs::u8path> LinkPackage::compile_pyc_files(const std::vector<fs::u8path>& py_files)
    {
        if (py_files.size() == 0)
        {
            return {};
        }

        std::vector<fs::u8path> pyc_files;
        for (auto& f : py_files)
        {
            pyc_files.push_back(pyc_path(f, m_context->python_params().short_python_version));
        }
        if (m_context->link_params().compile_pyc)
        {
            m_context->try_pyc_compilation(py_files);
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
        const auto& context = m_context->context();

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
        if (index_json.find("noarch") != index_json.end()
            && index_json["noarch"].type() != nlohmann::json::value_t::null)
        {
            if (index_json["noarch"].type() == nlohmann::json::value_t::boolean)
            {
                if (index_json["noarch"].get<bool>())
                {
                    noarch_type = NoarchType::GENERIC_V1;
                }
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
            auto [sha256_in_prefix, final_path] = link_path(path, noarch_type == NoarchType::PYTHON);
            files_record.push_back(final_path);

            nlohmann::json json_record = { { "_path", final_path },
                                           { "sha256_in_prefix", sha256_in_prefix } };

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
                auto points_to = fs::canonical(m_context->prefix_params().target_prefix / files_record[i], ec);
                bool found = false;
                if (!ec)
                {
                    for (std::size_t pix = 0; pix < files_record.size(); ++pix)
                    {
                        if ((m_context->prefix_params().target_prefix / files_record[pix]) == points_to)
                        {
                            if (paths_json["paths"][pix].contains("sha256_in_prefix"))
                            {
                                LOG_TRACE << "Found symlink and target " << files_record[i]
                                          << " -> " << files_record[pix];
                                // use already computed value
                                paths_json["paths"][i]["sha256_in_prefix"] = paths_json["paths"][pix]
                                                                                       ["sha256_in_prefix"];
                                found = true;
                                break;
                            }
                        }
                    }
                }
                if (!found)
                {
                    bool exists = fs::exists(m_context->prefix_params().target_prefix / files_record[i], ec);
                    if (ec)
                    {
                        LOG_WARNING << "Could not check existence for " << files_record[i] << ": "
                                    << ec.message();
                        exists = false;
                    }

                    if (exists)
                    {
                        paths_json["paths"][i]["sha256_in_prefix"] = validation::sha256sum(
                            m_context->prefix_params().target_prefix / files_record[i]
                        );
                    }
                    else
                    {
                        // for broken symlinks (that don't yet point anywhere valid) we record the
                        // sha256 for an empty string
                        paths_json["paths"][i]["sha256_in_prefix"] = MAMBA_EMPTY_SHA;
                    }
                }
            }
        }

        LOG_DEBUG << paths_data.size() << " files linked";

        out_json = index_json;
        out_json["paths_data"] = paths_json;
        out_json["files"] = files_record;

        specs::MatchSpec* requested_spec = nullptr;
        for (auto& ms : m_context->requested_specs)
        {
            if (ms.name().contains(m_pkg_info.name))
            {
                requested_spec = &ms;
            }
        }
        out_json["requested_spec"] = requested_spec != nullptr ? requested_spec->to_string() : "";
        out_json["package_tarball_full_path"] = m_source.string() + ".tar.bz2";
        out_json["extracted_package_dir"] = m_source.string();

        // TODO find out what `1` means
        out_json["link"] = { { "source", m_source.string() }, { "type", 1 } };

        if (noarch_type == NoarchType::PYTHON)
        {
            fs::u8path link_json_path = m_source / "info" / "link.json";
            nlohmann::json link_json;
            if (fs::exists(link_json_path))
            {
                std::ifstream link_json_file = open_ifstream(link_json_path);
                link_json_file >> link_json;
            }

            std::vector<fs::u8path> for_compilation;
            static std::regex py_file_re("^site-packages[/\\\\][^\\t\\n\\r\\f\\v]+\\.py$");
            for (auto& sub_path_json : paths_data)
            {
                if (std::regex_match(sub_path_json.path, py_file_re))
                {
                    for_compilation.push_back(
                        get_python_noarch_target_path(sub_path_json.path, m_context->python_params().site_packages_path)
                    );
                }
            }

            std::vector<fs::u8path> pyc_files = compile_pyc_files(for_compilation);
            for (const fs::u8path& pyc_path : pyc_files)
            {
                out_json["paths_data"]["paths"].push_back({ { "_path", pyc_path.generic_string() },
                                                            { "path_type", "pyc_file" } });

                out_json["files"].push_back(pyc_path.generic_string());
            }

            if (link_json.find("noarch") != link_json.end()
                && link_json["noarch"].find("entry_points") != link_json["noarch"].end())
            {
                for (auto& ep : link_json["noarch"]["entry_points"])
                {
                    // install entry points
                    auto entry_point_parsed = parse_entry_point(ep.get<std::string>());
                    auto entry_point_path = get_bin_directory_short_path()
                                            / entry_point_parsed.command;
                    LOG_TRACE << "entry point path: " << entry_point_path << std::endl;
                    auto files = create_python_entry_point(entry_point_path, entry_point_parsed);

#ifdef _WIN32
                    out_json["paths_data"]["paths"].push_back(
                        { { "_path", files[0] }, { "path_type", "windows_python_entry_point_script" } }
                    );
                    out_json["paths_data"]["paths"].push_back(
                        { { "_path", files[1] }, { "path_type", "windows_python_entry_point_exe" } }
                    );
                    out_json["files"].push_back(files[0]);
                    out_json["files"].push_back(files[1]);
#else
                    out_json["paths_data"]["paths"].push_back(
                        { { "_path", files }, { "path_type", "unix_python_entry_point" } }
                    );
                    out_json["files"].push_back(files);
#endif
                }
            }
        }

        // Create all start menu shortcuts if prefix name doesn't start with underscore
        if (util::on_win && context.shortcuts
            && m_context->prefix_params().target_prefix.filename().string()[0] != '_')
        {
            for (auto& path : paths_data)
            {
                if (std::regex_match(path.path, MENU_PATH_REGEX))
                {
                    create_menu_from_json(m_context->prefix_params().target_prefix / path.path, m_context);
                }
            }
        }

        run_script(context, m_context->prefix_params().target_prefix, m_pkg_info, "post-link", "", true);

        fs::u8path prefix_meta = m_context->prefix_params().target_prefix / "conda-meta";
        if (!fs::exists(prefix_meta))
        {
            fs::create_directory(prefix_meta);
        }

        LOG_DEBUG << "Finalizing linking";
        auto meta = prefix_meta / (f_name + ".json");
        LOG_TRACE << "Adding package to prefix metadata at '" << meta.string() << "'";
        std::ofstream out_file = open_ofstream(meta);
        out_file << out_json.dump(4);

        if (!m_clobber_warnings.empty())
        {
            LOG_WARNING << "[" << f_name
                        << "] The following files were already present in the environment:\n- "
                        << util::join("\n- ", m_clobber_warnings);
        }

        return true;
    }

    bool LinkPackage::undo()
    {
        UnlinkPackage ulp(m_pkg_info, m_cache_path, m_context);
        return ulp.execute();
    }
}  // namespace mamba
