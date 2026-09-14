// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cstdint>
#include <exception>
#include <iostream>
#include <iterator>
#include <numeric>
#include <regex>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#if !defined(_WIN32)
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <fmt/format.h>
#include <reproc++/reproc.hpp>
#include <reproc++/run.hpp>

#include "mamba/core/error_handling.hpp"
#include "mamba/core/menuinst.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/thread_utils.hpp"
#include "mamba/specs/match_spec.hpp"
#include "mamba/util/build.hpp"
#include "mamba/util/cryptography.hpp"
#include "mamba/util/encoding.hpp"
#include "mamba/util/environment.hpp"
#include "mamba/util/path_manip.hpp"
#include "mamba/util/string.hpp"
#include "mamba/validation/tools.hpp"

#include "./link.hpp"
#include "./transaction_context.hpp"

#ifdef __APPLE__
#include "mamba/core/util_os.hpp"
#endif

#if _WIN32
#include "../data/conda_exe.hpp"
#endif

namespace mamba
{
    namespace
    {
        using namespace std::literals::string_view_literals;

        const std::regex MENU_PATH_REGEX("^menu[/\\\\].*\\.json$", std::regex_constants::icase);

        const std::regex
            python_identifier_chain_regex(R"(^[A-Za-z_][A-Za-z0-9_]*(\.[A-Za-z_][A-Za-z0-9_]*)*$)");

        constexpr std::array forbidden_entry_point_command_substrings = {
            "/"sv, "\\"sv, "\n"sv, "\r"sv, "\t"sv, " "sv,
        };

        auto check_python_identifier_chain(std::string_view value, std::string_view field_name)
            -> tl::expected<void, mamba_error>
        {
            if (!std::regex_match(std::string(value), python_identifier_chain_regex))
            {
                return make_unexpected(
                    fmt::format(
                        "Invalid entry point {} '{}': expected a dotted chain of Python identifiers",
                        field_name,
                        value
                    ),
                    mamba_error_code::invalid_spec
                );
            }
            return {};
        }

        auto check_entry_point_command(std::string_view command) -> tl::expected<void, mamba_error>
        {
            if (command.empty() || command == "." || command == "..")
            {
                return make_unexpected(
                    fmt::format(
                        "Invalid entry point command name '{}': must be a simple file name",
                        command
                    ),
                    mamba_error_code::invalid_spec
                );
            }

            if (util::contains_any(command, forbidden_entry_point_command_substrings)
                || std::any_of(
                    command.begin(),
                    command.end(),
                    [](char c) { return util::is_control(c); }
                ))
            {
                return make_unexpected(
                    fmt::format(
                        "Invalid entry point command name '{}': path separators, whitespace, and control characters are not allowed",
                        command
                    ),
                    mamba_error_code::invalid_spec
                );
            }

            if (util::starts_with(command, ".."))
            {
                return make_unexpected(
                    fmt::format("Invalid entry point command name '{}': must not start with '..'", command),
                    mamba_error_code::invalid_spec
                );
            }

            const fs::u8path command_path(command);
            if (command_path.is_absolute())
            {
                return make_unexpected(
                    fmt::format(
                        "Invalid entry point command name '{}': absolute paths are not allowed",
                        command
                    ),
                    mamba_error_code::invalid_spec
                );
            }

            if (util::path_has_drive_letter(command)
                || (command.size() >= 2 && std::isalpha(static_cast<unsigned char>(command[0]))
                    && command[1] == ':'))
            {
                return make_unexpected(
                    fmt::format(
                        "Invalid entry point command name '{}': Windows drive paths are not allowed",
                        command
                    ),
                    mamba_error_code::invalid_spec
                );
            }

            return {};
        }

        auto check_path_within_prefix(
            const fs::u8path& path,
            const fs::u8path& prefix,
            std::string_view description
        ) -> tl::expected<void, mamba_error>
        {
            std::error_code ec;
            const fs::u8path rel = fs::relative(path, prefix, ec);
            if (ec)
            {
                return make_unexpected(
                    fmt::format(
                        "Cannot resolve {} path '{}' relative to environment prefix '{}': {}",
                        description,
                        path.string(),
                        prefix.string(),
                        ec.message()
                    ),
                    mamba_error_code::internal_failure
                );
            }

            const std::string rel_str = rel.generic_string();
            if (rel_str.empty() || rel_str == "." || util::starts_with(rel_str, "..")
                || util::contains(rel_str, "/../") || util::contains(rel_str, "\\..\\"))
            {
                return make_unexpected(
                    fmt::format(
                        "Refusing to write {} outside environment prefix: '{}'",
                        description,
                        path.string()
                    ),
                    mamba_error_code::invalid_spec
                );
            }

            return {};
        }

    }

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
            // Free-threaded installs use `lib/pythonX.Yt/site-packages` (Unix) or
            // `Lib/site-packages` (Windows conda layout), but bytecode names still follow
            // `sys.implementation.cache_tag` (e.g. `cpython-313` for `python 3.13t`
            // (free-threaded)) i.e. the `t` suffix is not included in the cache tag.
            util::replace_all(py_ver_nodot, "t", "");
            return directory / fs::u8path("__pycache__")
                   / util::concat(py_file_stem.string(), ".cpython-", py_ver_nodot, ".pyc");
        }
    }

    auto parse_entry_point(const std::string& ep_def)
        -> expected_t<python_entry_point_parsed, mamba_aggregated_error>
    {
        // def looks like: "wheel = wheel.cli:main"
        // Same approach as conda's parse_entry_point_def (conda/conda#16340):
        //   1. split on the first '=' into command and RHS
        //   2. strip whitespace and surrounding quotes from the RHS
        //      (some packages, e.g. findpython, ship `cmd = "mod:func"`)
        //   3. rsplit the stripped RHS on the last ':' into module and callable
        // Always return mamba_aggregated_error (even for a single failure) so the error is not
        // sliced when stored in expected_t (mamba-org/mamba#4352).
        auto make_parse_error = [](auto&&... errs)
        {
            return tl::unexpected(mamba_aggregated_error(
                std::vector<mamba_error>{ std::forward<decltype(errs)>(errs)... },
                /*with_bug_report_info=*/false
            ));
        };

        const auto [command, defn] = util::split_once(ep_def, '=');
        if (!defn)
        {
            return make_parse_error(
                mamba_error{ fmt::format("Invalid entry point definition '{}': missing '='", ep_def),
                             mamba_error_code::invalid_spec }
            );
        }

        // Step 2: strip whitespace/quotes from the module:callable side (conda#16340).
        constexpr std::string_view entry_point_rhs_strip_chars = " \t\r\n\"'";
        const auto module_func = util::strip(*defn, entry_point_rhs_strip_chars);
        // Step 3: rsplit the stripped RHS on the last ':' into module and callable.
        const auto [module, func] = util::rsplit_once(module_func, ':');
        if (!module)
        {
            return make_parse_error(
                mamba_error{ fmt::format("Invalid entry point definition '{}': missing ':'", ep_def),
                             mamba_error_code::invalid_spec }
            );
        }

        python_entry_point_parsed result;
        result.command = util::strip(command);
        result.module = util::strip(*module);
        result.func = util::strip(func);

        std::vector<mamba_error> errors;
        auto record_error = [&](const tl::expected<void, mamba_error>& check)
        {
            if (!check)
            {
                errors.push_back(check.error());
            }
        };

        record_error(check_entry_point_command(result.command));
        record_error(check_python_identifier_chain(result.module, "module"));
        record_error(check_python_identifier_chain(result.func, "callable"));

        if (!errors.empty())
        {
            return tl::unexpected(
                mamba_aggregated_error(std::move(errors), /*with_bug_report_info=*/false)
            );
        }

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
        const std::string win_script = path.string() + "-script.py";
        const std::string win_script_gen_str = path.generic_string() + "-script.py";
        const fs::u8path script_path = target_prefix / win_script;
        fs::u8path script_exe = path;
        script_exe.replace_extension("exe");
        const fs::u8path script_exe_path = target_prefix / script_exe;
#else
        const fs::u8path script_path = target_prefix / path;
#endif

        // The wheel package ships a console script also named "wheel". Prefix containment is
        // validated at parse time; the path guard false-positives for this known layout.
        if (m_pkg_info.name != "wheel")
        {
#ifdef _WIN32
            if (auto path_check = check_path_within_prefix(
                    script_path,
                    target_prefix,
                    "python entry point script"
                );
                !path_check)
            {
                throw path_check.error();
            }
            if (auto path_check = check_path_within_prefix(
                    script_exe_path,
                    target_prefix,
                    "python entry point executable"
                );
                !path_check)
            {
                throw path_check.error();
            }
#else
            if (auto path_check = check_path_within_prefix(
                    script_path,
                    target_prefix,
                    "python entry point script"
                );
                !path_check)
            {
                throw path_check.error();
            }
#endif
        }

        if (fs::exists(script_path))
        {
            m_clobber_warnings->push_back(fs::relative(script_path, target_prefix).string());
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
            python_path = m_context->prefix_params().relocate_prefix
                          / m_context->python_params().python_path;
        }
        if (!python_path.empty())
        {
            out_file << python_shebang(python_path.string()) << "\n";
        }

        python_entry_point_template(out_file, entry_point);
        out_file.close();

#ifdef _WIN32
        if (fs::exists(script_exe_path))
        {
            m_clobber_warnings->push_back(fs::relative(script_exe_path, target_prefix).string());
            fs::remove(script_exe_path);
        }

        std::ofstream conda_exe_f = open_ofstream(script_exe_path, std::ios::binary);
        conda_exe_f.write(reinterpret_cast<char*>(conda_exe), conda_exe_len);
        conda_exe_f.close();
        make_executable(script_exe_path);
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
            m_clobber_warnings->push_back(target_full_path.string());
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
        const TransactionParams& transaction_params,
        const PrefixParams& prefix_params,
        const fs::u8path& script_prefix,
        const specs::PackageInfo& pkg_info,
        const std::string& action = "post-link",
        const std::string& env_prefix = "",
        bool activate = false
    )
    {
        fs::u8path path;
        if (util::on_win)
        {
            path = script_prefix / get_bin_directory_short_path()
                   / util::concat(".", pkg_info.name, "-", action, ".bat");
        }
        else
        {
            path = script_prefix / get_bin_directory_short_path()
                   / util::concat(".", pkg_info.name, "-", action, ".sh");
        }

        if (!fs::exists(path))
        {
            LOG_DEBUG << action << " script for '" << pkg_info.name << "' does not exist ('"
                      << path.string() << "')";
            return true;
        }

        if (transaction_params.link_params.skip_run_link_scripts)
        {
            LOG_DEBUG << "Skipping " << action << " script for '" << pkg_info.name
                      << "' (disabled via config)";
            return true;
        }

        std::unordered_map<std::string, std::string> envmap;
        if (action == "pre-link")
        {
            LOG_WARNING
                << "Special Note: Pre-link scripts are particularly high-risk as they can "
                << "modify the package cache, potentially affecting all environments on this system.";
            envmap["SOURCE_DIR"] = script_prefix.string();
        }

        if (action == "post-unlink")
        {
            LOG_WARNING << "post-unlink scripts are deprecated and therefore won't be executed!";
            return true;
        }

        LOG_WARNING << "Executing " << action << " script for package '" << pkg_info.name << "'.";

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
                    prefix_params.root_prefix,
                    prefix_params.target_prefix,
                    { "@CALL", path.string() },
                    transaction_params.is_mamba_exe
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
                    prefix_params.root_prefix.string(),
                    prefix_params.target_prefix,
                    { ".", path.string() },
                    transaction_params.is_mamba_exe
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

        envmap["ROOT_PREFIX"] = prefix_params.root_prefix.string();
        envmap["PREFIX"] = env_prefix.size() ? env_prefix : prefix_params.target_prefix.string();
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
        if (transaction_params.json_output)
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
        std::string subtarget = path_data["_path"].get<std::string>();
        const fs::u8path& target_prefix = m_context->prefix_params().target_prefix;
        fs::u8path dst = target_prefix / subtarget;

        // Conda metadata for noarch Python packages can store short paths like
        // `site-packages/...` or `python-scripts/...`. Resolve those to the
        // actual prefix-relative destination before unlinking.
        if (!fs::exists(dst))
        {
            const auto resolved_subtarget = get_python_noarch_target_path(
                subtarget,
                m_context->python_params().site_packages_path
            );
            fs::u8path resolved_dst = target_prefix / resolved_subtarget;
            if (resolved_dst != dst)
            {
                dst = std::move(resolved_dst);
            }
        }

        LOG_TRACE << "Unlinking '" << dst.string() << "'";
        std::error_code err;

        if (remove_or_rename(target_prefix, dst) == 0)
        {
            LOG_DEBUG << "Error when removing file '" << dst.string() << "' will be ignored";
        }

        // Release any cross-package clobber claim so a later relink can rewrite the path.
        {
            const std::string rel = fs::relative(dst, target_prefix).generic_string();
            auto registry = m_context->clobber_registry().synchronize();
            registry->erase(rel);
            // Also erase the short path from conda-meta if it differed (noarch resolution).
            if (path_data.contains("_path"))
            {
                registry->erase(path_data["_path"].get<std::string>());
            }
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
                    remove_or_rename(target_prefix, parent_path);
                }
                else
                {
                    break;
                }
            }
            parent_path = parent_path.parent_path();
            if (parent_path == target_prefix)
            {
                break;
            }
        }
        return true;
    }

    bool UnlinkPackage::execute()
    {
        // find the recorded JSON file
        fs::u8path json = m_context->prefix_params().target_prefix / "conda-meta"
                          / (m_specifier + ".json");
        LOG_INFO << "Unlinking package '" << m_specifier << "'";
        LOG_DEBUG << "Use metadata found at '" << json.string() << "'";

        // Check if this is a pip package (pip packages don't have conda-meta JSON files)
        // Pip packages are handled by pip uninstall in transaction.cpp, so we can skip
        // the unlinking process here if the JSON file doesn't exist for a pip package
        if (m_pkg_info.channel == "pypi")
        {
            // For pip packages, the conda-meta JSON file may not exist
            // They are uninstalled via pip uninstall in transaction.cpp
            if (!fs::exists(json))
            {
                LOG_DEBUG << "Skipping unlinking for pip package '" << m_specifier
                          << "' (no conda-meta JSON file, handled by pip uninstall)";
                return true;
            }
        }

        // Check if the JSON file exists before trying to read it
        if (!fs::exists(json))
        {
            LOG_WARNING << "conda-meta JSON file not found for package '" << m_specifier << "' at '"
                        << json.string() << "'";
            // Return true to avoid failing the transaction, but log the warning
            return true;
        }

        run_script(
            m_context->transaction_params(),
            m_context->prefix_params(),
            m_context->prefix_params().target_prefix,
            m_pkg_info,
            "pre-unlink",
            "",
            false
        );

        std::ifstream json_file = open_ifstream(json);
        if (!json_file.good())
        {
            LOG_WARNING << "Failed to open conda-meta JSON file for package '" << m_specifier
                        << "' at '" << json.string() << "'";
            return true;
        }

        nlohmann::json json_record;
        json_file >> json_record;

        nlohmann::json paths_to_unlink = nlohmann::json::array();
        if (json_record.contains("paths_data") && json_record["paths_data"].is_object()
            && json_record["paths_data"].contains("paths")
            && json_record["paths_data"]["paths"].is_array())
        {
            paths_to_unlink = json_record["paths_data"]["paths"];
        }
        else if (json_record.contains("files") && json_record["files"].is_array())
        {
            LOG_DEBUG << "Using legacy `files` list from metadata for package '" << m_specifier
                      << "'";
            for (const auto& file : json_record["files"])
            {
                if (file.is_string())
                {
                    paths_to_unlink.push_back({ { "_path", file.get<std::string>() } });
                }
            }
        }
        else
        {
            LOG_WARNING << "No unlinkable file list found in conda-meta JSON for package '"
                        << m_specifier << "'";
        }

        for (auto& path : paths_to_unlink)
        {
            std::string fpath = path["_path"];
            if (std::regex_match(fpath, MENU_PATH_REGEX))
            {
                remove_menu_from_json(m_context->prefix_params().target_prefix / fpath, *m_context);
            }

            unlink_path(path);
        }

        json_file.close();

        run_script(
            m_context->transaction_params(),
            m_context->prefix_params(),
            m_context->prefix_params().target_prefix,
            m_pkg_info,
            "post-unlink",
            "",
            true
        );

        fs::remove(json);

        return true;
    }

    bool UnlinkPackage::undo()
    {
        LinkPackage lp(m_pkg_info, m_cache_path, m_context);
        return lp.execute();
    }

    namespace
    {
        auto sha256_hex(std::string_view data) -> std::string
        {
            thread_local util::Sha256Hasher hasher;
            return hasher.str_hex_str(data);
        }

        constexpr std::size_t large_prefix_rewrite_threshold = 256u * 1024u;

#if !defined(_WIN32)
        struct MappedFile
        {
            const char* data = nullptr;
            std::size_t size = 0;
            bool needs_unmap = false;
            bool valid = false;

            ~MappedFile()
            {
                if (needs_unmap && data != nullptr && size > 0)
                {
                    ::munmap(const_cast<char*>(data), size);
                }
            }

            MappedFile() = default;
            MappedFile(const MappedFile&) = delete;
            MappedFile& operator=(const MappedFile&) = delete;

            MappedFile(MappedFile&& other) noexcept
                : data(other.data)
                , size(other.size)
                , needs_unmap(other.needs_unmap)
                , valid(other.valid)
            {
                other.data = nullptr;
                other.size = 0;
                other.needs_unmap = false;
                other.valid = false;
            }

            MappedFile& operator=(MappedFile&& other) noexcept
            {
                if (this != &other)
                {
                    if (needs_unmap && data != nullptr && size > 0)
                    {
                        ::munmap(const_cast<char*>(data), size);
                    }
                    data = other.data;
                    size = other.size;
                    needs_unmap = other.needs_unmap;
                    valid = other.valid;
                    other.data = nullptr;
                    other.size = 0;
                    other.needs_unmap = false;
                    other.valid = false;
                }
                return *this;
            }

            explicit operator bool() const
            {
                return valid;
            }
        };

        /** Memory-map ``path`` read-only. Empty files succeed with ``data == nullptr``. */
        auto mmap_file_readonly(const fs::u8path& path) -> MappedFile
        {
            const int fd = ::open(path.string().c_str(), O_RDONLY | O_CLOEXEC);
            if (fd < 0)
            {
                return {};
            }
            struct ::stat st{};
            if (::fstat(fd, &st) != 0 || st.st_size < 0)
            {
                ::close(fd);
                return {};
            }
            const auto size = static_cast<std::size_t>(st.st_size);
            if (size == 0)
            {
                ::close(fd);
                MappedFile empty;
                empty.valid = true;
                return empty;
            }
            void* mapped = ::mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
            ::close(fd);
            if (mapped == MAP_FAILED)
            {
                return {};
            }
            MappedFile result;
            result.data = static_cast<const char*>(mapped);
            result.size = size;
            result.needs_unmap = true;
            result.valid = true;
            return result;
        }

        /**
         * Stream prefix replacement from an already-mapped source into ``dst``, hashing as we go.
         * Handles text and binary (null-padded) replacement without holding a second full copy.
         */
        auto stream_prefix_replace_from_memory(
            std::string_view source,
            const std::string& placeholder,
            const std::string& new_prefix,
            FileMode file_mode,
            const fs::u8path& dst,
            bool* binary_changed
        ) -> std::string
        {
            std::ofstream fo = open_ofstream(dst, std::ios::out | std::ios::binary);
            util::Sha256Digester digester;
            digester.digest_start();

            auto write_and_hash = [&](std::string_view chunk)
            {
                if (chunk.empty())
                {
                    return;
                }
                fo.write(chunk.data(), static_cast<std::streamsize>(chunk.size()));
                auto* iter = reinterpret_cast<const std::byte*>(chunk.data());
                auto remaining = chunk.size();
                while (remaining > 0)
                {
                    const auto taken = std::min(remaining, util::Sha256Digester::digest_size);
                    digester.digest_update(iter, taken);
                    remaining -= taken;
                    iter += taken;
                }
            };

            auto write_with_text_replacements = [&](std::string_view region)
            {
                std::size_t cursor = 0;
                std::size_t pos = region.find(placeholder);
                while (pos != std::string_view::npos)
                {
                    write_and_hash(region.substr(cursor, pos - cursor));
                    write_and_hash(new_prefix);
                    cursor = pos + placeholder.size();
                    pos = region.find(placeholder, cursor);
                }
                write_and_hash(region.substr(cursor));
            };

            if (file_mode != FileMode::BINARY)
            {
                std::size_t body_offset = 0;
                if constexpr (!util::on_win)
                {
                    if (source.size() >= 2 && source[0] == '#' && source[1] == '!')
                    {
                        const std::size_t end_of_line = source.find_first_of('\n');
                        std::string first_line(source.substr(0, end_of_line));
                        util::replace_all(first_line, placeholder, new_prefix);
                        if (first_line.size() > MAX_SHEBANG_LENGTH)
                        {
                            first_line = replace_long_shebang(first_line);
                        }
                        write_and_hash(first_line);
                        if (end_of_line != std::string_view::npos)
                        {
                            write_and_hash(source.substr(end_of_line, 1));
                            body_offset = end_of_line + 1;
                        }
                        else
                        {
                            body_offset = source.size();
                        }
                    }
                }
                write_with_text_replacements(source.substr(body_offset));
            }
            else
            {
                const std::size_t padding_size = (placeholder.size() > new_prefix.size())
                                                     ? placeholder.size() - new_prefix.size()
                                                     : 0;
                const std::string padding(padding_size, '\0');

                std::size_t cursor = 0;
                std::size_t pos = source.find(placeholder);
                while (pos != std::string_view::npos)
                {
                    if (binary_changed != nullptr)
                    {
                        *binary_changed = true;
                    }
                    write_and_hash(source.substr(cursor, pos - cursor));

                    std::size_t end = pos + placeholder.size();
                    while (end < source.size() && source[end] != '\0')
                    {
                        ++end;
                    }
                    const auto suffix = source.substr(
                        pos + placeholder.size(),
                        end - (pos + placeholder.size())
                    );
                    const std::string replacement = util::concat(new_prefix, std::string(suffix), padding);
                    write_and_hash(replacement);

                    cursor = end;
                    pos = source.find(placeholder, cursor);
                }
                write_and_hash(source.substr(cursor));
            }

            fo.close();
            std::array<std::byte, util::Sha256Digester::bytes_size> hash{};
            digester.digest_finalize_to(hash.data());
            return util::bytes_to_hex_str(hash.data(), hash.data() + hash.size());
        }
#endif

        auto destination_relative_path(
            const PathData& path_data,
            bool noarch_python,
            const fs::u8path& site_packages_path
        ) -> fs::u8path
        {
            if (noarch_python)
            {
                return get_python_noarch_target_path(path_data.path, site_packages_path);
            }
            return path_data.path;
        }
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
            rel_dst = get_python_noarch_target_path(
                subtarget,
                m_context->python_params().site_packages_path
            );
            dst = m_context->prefix_params().target_prefix / rel_dst;
        }
        else
        {
            rel_dst = subtarget;
            dst = m_context->prefix_params().target_prefix / rel_dst;
        }

        fs::u8path src = m_source / subtarget;

        std::error_code ec;
        bool claimed_first = false;
        {
            auto registry = m_context->clobber_registry().synchronize();
            auto [it, inserted] = registry->emplace(rel_dst.string(), m_pkg_info.str());
            claimed_first = inserted;
            (void) it;
        }

        if (!claimed_first)
        {
            // Another package in this transaction already claimed the path — first writer wins
            // while the file is still present. If the file is gone (e.g. after unlink), take over.
            if (lexists(dst, ec) && !ec)
            {
                m_clobber_warnings->push_back(rel_dst.string());
                try
                {
                    return std::make_tuple(validation::sha256sum(dst), rel_dst.generic_string());
                }
                catch (...)
                {
                    std::string empty_sha = MAMBA_EMPTY_SHA;
                    return std::make_tuple(std::move(empty_sha), rel_dst.generic_string());
                }
            }
            {
                auto registry = m_context->clobber_registry().synchronize();
                (*registry)[rel_dst.string()] = m_pkg_info.str();
            }
        }

        if (lexists(dst, ec) && !ec)
        {
            // Sometimes we might want to raise here ...
            m_clobber_warnings->push_back(rel_dst.string());
#ifdef _WIN32
            // Try to compute SHA256 of existing file, but if it fails (e.g., file is locked
            // or from a pip package), fall back to removing it like on other platforms
            try
            {
                return std::make_tuple(
                    std::string(validation::sha256sum(dst)),
                    rel_dst.generic_string()
                );
            }
            catch (const std::exception& e)
            {
                LOG_DEBUG << "Could not compute SHA256 of existing file " << dst
                          << ", will remove it: " << e.what();
            }
            catch (...)
            {
                LOG_DEBUG << "Could not compute SHA256 of existing file " << dst
                          << ", will remove it (unknown error)";
            }
            fs::remove(dst);
#else
            fs::remove(dst);
#endif
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

#if !defined(_WIN32)
            // Large files: mmap + stream rewrite/hash to avoid a second full-buffer copy.
            // Keep the buffered path for small files and for macOS codesign (needs re-hash on
            // disk).
            const bool try_stream = path_data.size_in_bytes >= large_prefix_rewrite_threshold
#if defined(__APPLE__)
                                    && m_pkg_info.platform != "osx-arm64"
#endif
                ;
            if (try_stream)
            {
                MappedFile mapped = mmap_file_readonly(src);
                if (mapped)
                {
                    bool binary_changed_stream = false;
                    const std::string_view view(mapped.data, mapped.size);
                    std::string hash = stream_prefix_replace_from_memory(
                        view,
                        path_data.prefix_placeholder,
                        new_prefix,
                        path_data.file_mode,
                        dst,
                        path_data.file_mode == FileMode::BINARY ? &binary_changed_stream : nullptr
                    );
                    std::error_code lec;
                    fs::permissions(dst, fs::status(src).permissions(), lec);
                    if (lec)
                    {
                        LOG_WARNING << "Could not set permissions on [" << dst
                                    << "]: " << lec.message();
                    }
                    return std::tuple(std::move(hash), rel_dst.generic_string());
                }
            }
#endif

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
                codesign(dst, m_context->transaction_params().verbosity > 1);
                return std::tuple(validation::sha256sum(dst), rel_dst.generic_string());
            }
#endif
            return std::tuple(sha256_hex(buffer), rel_dst.generic_string());
        }

        if ((path_data.path_type == PathType::HARDLINK) || path_data.no_link)
        {
            bool copy = path_data.no_link || m_context->link_params().always_copy;
            bool softlink = m_context->link_params().always_softlink && !path_data.no_link;

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
                if (path_data.no_link && m_context->link_params().always_softlink)
                {
                    LOG_WARNING
                        << "File '" << subtarget
                        << "' is marked as `no_link`, ignoring --always-softlink and forcing copy.";
                }
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
        return std::tuple(
            path_data.sha256.empty() ? validation::sha256sum(dst) : path_data.sha256,
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

    void
    LinkPackage::create_parent_directories(const std::vector<PathData>& paths_data, bool noarch_python)
    {
        const auto& prefix = m_context->prefix_params().target_prefix;
        const auto& site_packages = m_context->python_params().site_packages_path;

        std::unordered_set<std::string> seen;
        std::vector<fs::u8path> dirs;
        dirs.reserve(paths_data.size());

        for (const auto& path : paths_data)
        {
            const fs::u8path rel_dst = destination_relative_path(path, noarch_python, site_packages);
            fs::u8path parent = (prefix / rel_dst).parent_path();
            if (parent.empty() || parent == prefix)
            {
                continue;
            }
            if (seen.insert(parent.string()).second)
            {
                dirs.push_back(std::move(parent));
            }
        }

        std::sort(
            dirs.begin(),
            dirs.end(),
            [](const fs::u8path& a, const fs::u8path& b)
            { return a.string().size() < b.string().size(); }
        );

        for (const auto& dir : dirs)
        {
            std::error_code ec;
            fs::create_directories(dir, ec);
            if (ec)
            {
                LOG_WARNING << "Could not create directory " << dir << ": " << ec.message();
            }
        }
    }

    enum class NoarchType
    {
        NOT_A_NOARCH,
        GENERIC_V1,
        GENERIC_V2,
        PYTHON
    };

    auto LinkPackage::paths_data() const -> const std::vector<PathData>&
    {
        return m_paths_data;
    }

    auto LinkPackage::package_info() const -> const specs::PackageInfo&
    {
        return m_pkg_info;
    }

    auto LinkPackage::estimated_link_cost(const PathData& p) -> std::uint64_t
    {
        if (p.prefix_placeholder.empty())
        {
            return 1;
        }
        const std::uint64_t size = p.size_in_bytes == 0 ? 4096ull : p.size_in_bytes;
        return size * (p.file_mode == FileMode::BINARY ? 8ull : 4ull);
    }

    bool LinkPackage::prepare()
    {
        LOG_TRACE << "Preparing linking from '" << m_source.string() << "'";

        run_script(
            m_context->transaction_params(),
            m_context->prefix_params(),
            m_source,
            m_pkg_info,
            "pre-link",
            "",
            false
        );

        LOG_TRACE << "Opening: " << m_source / "info" / "paths.json";
        m_paths_data = read_paths(m_source);

        LOG_TRACE << "Opening: " << m_source / "info" / "repodata_record.json";
        std::ifstream repodata_f = open_ifstream(m_source / "info" / "repodata_record.json");
        repodata_f >> m_index_json;

        LOG_DEBUG << "Linking package '" << m_pkg_info.str() << "' from '" << m_source.string()
                  << "'";

        m_noarch_type = static_cast<int>(NoarchType::NOT_A_NOARCH);
        if (m_index_json.find("noarch") != m_index_json.end()
            && m_index_json["noarch"].type() != nlohmann::json::value_t::null)
        {
            if (m_index_json["noarch"].type() == nlohmann::json::value_t::boolean)
            {
                if (m_index_json["noarch"].get<bool>())
                {
                    m_noarch_type = static_cast<int>(NoarchType::GENERIC_V1);
                }
            }
            else
            {
                std::string na_t(m_index_json["noarch"].get<std::string>());
                if (na_t == "python")
                {
                    m_noarch_type = static_cast<int>(NoarchType::PYTHON);
                }
                else if (na_t == "generic")
                {
                    m_noarch_type = static_cast<int>(NoarchType::GENERIC_V2);
                }
            }
        }

        const bool noarch_python = m_noarch_type == static_cast<int>(NoarchType::PYTHON);
        create_parent_directories(m_paths_data, noarch_python);
        m_linked.assign(m_paths_data.size(), {});
        m_prepared = true;
        m_files_linked = false;
        return true;
    }

    void LinkPackage::link_file_at(std::size_t index)
    {
        assert(m_prepared);
        assert(index < m_paths_data.size());
        const bool noarch_python = m_noarch_type == static_cast<int>(NoarchType::PYTHON);
        interruption_point();
        m_linked[index] = link_path(m_paths_data[index], noarch_python);
    }

    void LinkPackage::mark_files_linked()
    {
        m_files_linked = true;
    }

    bool LinkPackage::files_linked() const
    {
        return m_files_linked;
    }

    void LinkPackage::link_files()
    {
        assert(m_prepared);
        const auto n_link_threads = normalize_to_affinity_concurrency(
            static_cast<std::ptrdiff_t>(m_context->transaction_params().threads_params.link_threads)
        );

        std::vector<std::size_t> order(m_paths_data.size());
        std::iota(order.begin(), order.end(), std::size_t{ 0 });
        std::stable_sort(
            order.begin(),
            order.end(),
            [&](std::size_t a, std::size_t b)
            { return estimated_link_cost(m_paths_data[a]) > estimated_link_cost(m_paths_data[b]); }
        );

        if (n_link_threads <= 1 || order.size() <= 1)
        {
            for (const std::size_t i : order)
            {
                link_file_at(i);
            }
        }
        else
        {
            const std::size_t n_items = order.size();
            const std::size_t n_workers = std::min(n_link_threads, n_items);
            std::atomic<std::size_t> next{ 0 };
            std::atomic<bool> stop{ false };
            util::synchronized_value<std::exception_ptr> first_exception;
            {
                // Use std::thread (not jthread): Apple libc++ does not provide std::jthread yet.
                std::vector<std::thread> workers;
                workers.reserve(n_workers);
                for (std::size_t t = 0; t < n_workers; ++t)
                {
                    workers.emplace_back(
                        [&]()
                        {
                            try
                            {
                                while (!stop.load(std::memory_order_relaxed))
                                {
                                    const auto k = next.fetch_add(1, std::memory_order_relaxed);
                                    if (k >= n_items)
                                    {
                                        break;
                                    }
                                    if (is_sig_interrupted())
                                    {
                                        stop.store(true, std::memory_order_relaxed);
                                        break;
                                    }
                                    link_file_at(order[k]);
                                }
                            }
                            catch (...)
                            {
                                stop.store(true, std::memory_order_relaxed);
                                auto exception = first_exception.synchronize();
                                if (!*exception)
                                {
                                    *exception = std::current_exception();
                                }
                            }
                        }
                    );
                }
                for (auto& worker : workers)
                {
                    worker.join();
                }
            }

            if (auto exception = first_exception.value())
            {
                std::rethrow_exception(exception);
            }
            interruption_point();
        }
        m_files_linked = true;
    }

    void link_packages_files_parallel(std::vector<LinkPackage>& packages, std::size_t link_threads)
    {
        struct Job
        {
            std::size_t pkg_index;
            std::size_t path_index;
            std::uint64_t cost;
        };

        std::vector<Job> jobs;
        for (std::size_t p = 0; p < packages.size(); ++p)
        {
            const auto& paths = packages[p].paths_data();
            jobs.reserve(jobs.size() + paths.size());
            for (std::size_t i = 0; i < paths.size(); ++i)
            {
                jobs.push_back(Job{ p, i, LinkPackage::estimated_link_cost(paths[i]) });
            }
        }

        if (jobs.empty())
        {
            for (auto& pkg : packages)
            {
                pkg.mark_files_linked();
            }
            return;
        }

        std::stable_sort(
            jobs.begin(),
            jobs.end(),
            [](const Job& a, const Job& b) { return a.cost > b.cost; }
        );

        const auto n_link_threads = normalize_to_affinity_concurrency(
            static_cast<std::ptrdiff_t>(link_threads)
        );

        auto run_one = [&](const Job& job) { packages[job.pkg_index].link_file_at(job.path_index); };

        if (n_link_threads <= 1 || jobs.size() <= 1)
        {
            for (const auto& job : jobs)
            {
                run_one(job);
            }
        }
        else
        {
            const std::size_t n_items = jobs.size();
            const std::size_t n_workers = std::min(n_link_threads, n_items);
            std::atomic<std::size_t> next{ 0 };
            std::atomic<bool> stop{ false };
            util::synchronized_value<std::exception_ptr> first_exception;
            {
                // Use std::thread (not jthread): Apple libc++ does not provide std::jthread yet.
                std::vector<std::thread> workers;
                workers.reserve(n_workers);
                for (std::size_t t = 0; t < n_workers; ++t)
                {
                    workers.emplace_back(
                        [&]()
                        {
                            try
                            {
                                while (!stop.load(std::memory_order_relaxed))
                                {
                                    const auto k = next.fetch_add(1, std::memory_order_relaxed);
                                    if (k >= n_items)
                                    {
                                        break;
                                    }
                                    if (is_sig_interrupted())
                                    {
                                        stop.store(true, std::memory_order_relaxed);
                                        break;
                                    }
                                    run_one(jobs[k]);
                                }
                            }
                            catch (...)
                            {
                                stop.store(true, std::memory_order_relaxed);
                                auto exception = first_exception.synchronize();
                                if (!*exception)
                                {
                                    *exception = std::current_exception();
                                }
                            }
                        }
                    );
                }
                for (auto& worker : workers)
                {
                    worker.join();
                }
            }

            if (auto exception = first_exception.value())
            {
                std::rethrow_exception(exception);
            }
            interruption_point();
        }

        if (!is_sig_interrupted())
        {
            for (auto& pkg : packages)
            {
                pkg.mark_files_linked();
            }
        }
    }

    bool LinkPackage::finalize()
    {
        assert(m_prepared);
        assert(m_files_linked);

        const auto& paths_data = m_paths_data;
        const auto& linked = m_linked;
        const auto noarch_type = static_cast<NoarchType>(m_noarch_type);
        const std::string f_name = m_pkg_info.str();

        std::vector<std::string> files_record;
        files_record.reserve(paths_data.size());

        nlohmann::json paths_json = nlohmann::json::object();
        paths_json["paths"] = nlohmann::json::array();
        paths_json["paths_version"] = 1;

        for (std::size_t i = 0; i < paths_data.size(); ++i)
        {
            const auto& [sha256_in_prefix, final_path] = linked[i];
            files_record.push_back(final_path);

            nlohmann::json json_record = { { "_path", final_path },
                                           { "sha256_in_prefix", sha256_in_prefix } };

            const auto& path = paths_data[i];
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
                json_record["size_in_bytes"] = path.size_in_bytes;
            }

            paths_json["paths"].push_back(json_record);
        }

        const auto& prefix = m_context->prefix_params().target_prefix;
        std::unordered_map<std::string, std::string> sha_by_abs_path;
        sha_by_abs_path.reserve(paths_data.size());
        for (std::size_t i = 0; i < paths_data.size(); ++i)
        {
            if (paths_data[i].path_type == PathType::SOFTLINK)
            {
                continue;
            }
            const auto& rec = paths_json["paths"][i];
            if (rec.contains("sha256_in_prefix"))
            {
                sha_by_abs_path.emplace(
                    (prefix / files_record[i]).string(),
                    rec["sha256_in_prefix"].get<std::string>()
                );
            }
        }

        for (std::size_t i = 0; i < paths_data.size(); ++i)
        {
            if (paths_data[i].path_type != PathType::SOFTLINK)
            {
                continue;
            }

            std::error_code ec;
            auto points_to = fs::canonical(prefix / files_record[i], ec);
            bool found = false;
            if (!ec)
            {
                if (auto it = sha_by_abs_path.find(points_to.string()); it != sha_by_abs_path.end())
                {
                    LOG_TRACE << "Found symlink and target " << files_record[i] << " -> "
                              << points_to;
                    paths_json["paths"][i]["sha256_in_prefix"] = it->second;
                    found = true;
                }
            }
            if (!found)
            {
                bool exists = fs::exists(prefix / files_record[i], ec);
                if (ec)
                {
                    LOG_WARNING << "Could not check existence for " << files_record[i] << ": "
                                << ec.message();
                    exists = false;
                }

                if (exists)
                {
                    paths_json["paths"][i]["sha256_in_prefix"] = validation::sha256sum(
                        prefix / files_record[i]
                    );
                }
                else
                {
                    paths_json["paths"][i]["sha256_in_prefix"] = MAMBA_EMPTY_SHA;
                }
            }
        }

        LOG_DEBUG << paths_data.size() << " files linked";

        nlohmann::json out_json = m_index_json;
        out_json["paths_data"] = paths_json;
        out_json["files"] = files_record;

        const specs::MatchSpec* requested_spec = nullptr;
        for (auto& ms : m_context->requested_specs())
        {
            if (ms.name().contains(m_pkg_info.name))
            {
                requested_spec = &ms;
            }
        }
        out_json["requested_spec"] = requested_spec != nullptr ? requested_spec->to_string() : "";
        out_json["package_tarball_full_path"] = m_source.string() + ".tar.bz2";
        out_json["extracted_package_dir"] = m_source.string();
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
                    for_compilation.push_back(get_python_noarch_target_path(
                        sub_path_json.path,
                        m_context->python_params().site_packages_path
                    ));
                }
            }

            std::vector<fs::u8path> pyc_files = compile_pyc_files(for_compilation);
            for (const fs::u8path& pyc_path : pyc_files)
            {
                out_json["paths_data"]["paths"].push_back(
                    { { "_path", pyc_path.generic_string() }, { "path_type", "pyc_file" } }
                );

                out_json["files"].push_back(pyc_path.generic_string());
            }

            if (link_json.find("noarch") != link_json.end()
                && link_json["noarch"].find("entry_points") != link_json["noarch"].end())
            {
                for (auto& ep : link_json["noarch"]["entry_points"])
                {
                    const auto ep_def = ep.get<std::string>();
                    auto entry_point_parsed = parse_entry_point(ep_def);
                    if (!entry_point_parsed)
                    {
                        throw mamba_error(
                            fmt::format(
                                "Invalid noarch:python entry point '{}' in package '{}' ({}): {}\n"
                                "The package distributor must correct the entry_points declared in "
                                "info/link.json.",
                                ep_def,
                                m_pkg_info.name,
                                m_pkg_info.build_string,
                                entry_point_parsed.error().what()
                            ),
                            mamba_error_code::invalid_spec
                        );
                    }
                    auto entry_point_path = get_bin_directory_short_path()
                                            / entry_point_parsed->command;
                    LOG_TRACE << "entry point path: " << entry_point_path << std::endl;
                    auto files = create_python_entry_point(entry_point_path, *entry_point_parsed);

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

        if (util::on_win && m_context->transaction_params().shortcuts
            && m_context->prefix_params().target_prefix.filename().string()[0] != '_')
        {
            for (auto& path : paths_data)
            {
                if (std::regex_match(path.path, MENU_PATH_REGEX))
                {
                    create_menu_from_json(
                        m_context->prefix_params().target_prefix / path.path,
                        *m_context
                    );
                }
            }
        }

        run_script(
            m_context->transaction_params(),
            m_context->prefix_params(),
            m_context->prefix_params().target_prefix,
            m_pkg_info,
            "post-link",
            "",
            true
        );

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

        {
            const auto warnings = m_clobber_warnings.synchronize();
            if (!warnings->empty())
            {
                LOG_WARNING << "[" << f_name
                            << "] The following files were already present in the environment:\n- "
                            << util::join("\n- ", *warnings);
            }
        }

        return true;
    }

    bool LinkPackage::execute()
    {
        prepare();
        link_files();
        return finalize();
    }

    bool LinkPackage::undo()
    {
        UnlinkPackage ulp(m_pkg_info, m_cache_path, m_context);
        return ulp.execute();
    }
}  // namespace mamba
