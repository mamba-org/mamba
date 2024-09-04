// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <fmt/color.h>
#include <fmt/format.h>
#include <fmt/ostream.h>
#include <fmt/ranges.h>
#include <reproc++/run.hpp>
#include <reproc/reproc.h>

#include "mamba/api/install.hpp"
#include "mamba/core/activation.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/util.hpp"
#include "mamba/fs/filesystem.hpp"
#include "mamba/util/environment.hpp"

#include "pip_utils.hpp"

namespace mamba
{
    namespace
    {
        tl::expected<command_args, std::runtime_error> get_pip_install_command(
            const std::string& name,
            const std::string& target_prefix,
            const fs::u8path& spec_file,
            pip::Update update
        )
        {
            const auto get_python_path = [&]
            { return util::which_in("python", get_path_dirs(target_prefix)).string(); };

            command_args cmd = { get_python_path(), "-m", "pip", "install" };
            command_args cmd_extension = { "-r", spec_file, "--no-input", "--quiet" };

            if (update == pip::Update::Yes)
            {
                cmd.push_back("-U");
            }

            if (name == "pip --no-deps")
            {
                cmd.push_back("--no-deps");
            }

            cmd.insert(cmd.end(), cmd_extension.begin(), cmd_extension.end());
            const std::unordered_map<std::string, command_args> pip_install_command{
                { "pip", cmd },
                { "pip --no-deps", cmd }
            };

            auto found_it = pip_install_command.find(name);
            if (found_it != pip_install_command.end())
            {
                return found_it->second;
            }
            else
            {
                return tl::unexpected(std::runtime_error(fmt::format(
                    "no {} instruction found for package manager '{}'",
                    (update == pip::Update::Yes) ? "update" : "install",
                    name
                )));
            }
        }
    }

    bool reproc_killed(int status)
    {
        return status == REPROC_SIGKILL;
    }

    bool reproc_terminated(int status)
    {
        return status == REPROC_SIGTERM;
    }

    void assert_reproc_success(const reproc::options& options, int status, std::error_code ec)
    {
        bool killed_not_an_err = (options.stop.first.action == reproc::stop::kill)
                                 || (options.stop.second.action == reproc::stop::kill)
                                 || (options.stop.third.action == reproc::stop::kill);

        bool terminated_not_an_err = (options.stop.first.action == reproc::stop::terminate)
                                     || (options.stop.second.action == reproc::stop::terminate)
                                     || (options.stop.third.action == reproc::stop::terminate);

        if (ec || (!killed_not_an_err && reproc_killed(status))
            || (!terminated_not_an_err && reproc_terminated(status)))
        {
            if (ec)
            {
                LOG_ERROR << "Subprocess call failed: " << ec.message();
            }
            else if (reproc_killed(status))
            {
                LOG_ERROR << "Subprocess call failed (killed)";
            }
            else
            {
                LOG_ERROR << "Subprocess call failed (terminated)";
            }
            throw std::runtime_error("Subprocess call failed. Aborting.");
        }
    }

    tl::expected<command_args, std::runtime_error> install_for_other_pkgmgr(
        const Context& ctx,
        const detail::other_pkg_mgr_spec& other_spec,
        pip::Update update
    )
    {
        const auto& pkg_mgr = other_spec.pkg_mgr;
        const auto& deps = other_spec.deps;
        const auto& cwd = other_spec.cwd;

        LOG_WARNING << fmt::format(
            "You are using '{}' as an additional package manager.\nBe aware that packages installed with '{}' are managed independently from 'conda-forge' channel.",
            pkg_mgr,
            pkg_mgr
        );

        TemporaryFile specs("mambaf", "", cwd);
        {
            std::ofstream specs_f = open_ofstream(specs.path());
            for (auto& d : deps)
            {
                specs_f << d.c_str() << '\n';
            }
        }

        command_args command = [&]
        {
            const auto maybe_command = get_pip_install_command(
                pkg_mgr,
                ctx.prefix_params.target_prefix.string(),
                specs.path(),
                update
            );
            if (maybe_command)
            {
                return maybe_command.value();
            }
            else
            {
                throw maybe_command.error();
            }
        }();

        auto [wrapped_command, tmpfile] = prepare_wrapped_call(
            ctx,
            ctx.prefix_params.target_prefix,
            command
        );

        reproc::options options;
        options.redirect.parent = true;
        options.working_directory = cwd.c_str();

        Console::stream() << fmt::format(
            ctx.graphics_params.palette.external,
            "\n{} {} packages: {}",
            (update == pip::Update::Yes) ? "Updating" : "Installing",
            pkg_mgr,
            fmt::join(deps, ", ")
        );

        fmt::print(LOG_INFO, "Calling: {}", fmt::join(command, " "));

        auto [status, ec] = reproc::run(wrapped_command, options);
        assert_reproc_success(options, status, ec);
        if (status != 0)
        {
            throw std::runtime_error(fmt::format(
                "pip failed to {} packages",
                (update == pip::Update::Yes) ? "update" : "install"
            ));
        }

        return command;
    }
}
