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

#include "tl/expected.hpp"

#include "pip_utils.hpp"

namespace mamba
{
    namespace
    {
        tl::expected<command_args, std::runtime_error> get_pip_install_command(
            const std::string& name,
            const std::string& target_prefix,
            const fs::u8path& spec_file,
            bool update
        )
        {
            const auto get_python_path = [&]
            { return util::which_in("python", get_path_dirs(target_prefix)).string(); };

            const std::unordered_map<std::string, command_args> pip_install_command{
                { "pip",
                  { get_python_path(), "-m", "pip", "install", "-r", spec_file, "--no-input" } },
                { "pip --no-deps",
                  { get_python_path(), "-m", "pip", "install", "--no-deps", "-r", spec_file, "--no-input" } }
            };

            const std::unordered_map<std::string, command_args> pip_update_command{
                { "pip",
                  { get_python_path(), "-m", "pip", "install", "-U", "-r", spec_file, "--no-input" } },
                { "pip --no-deps",
                  { get_python_path(), "-m", "pip", "install", "-U", "--no-deps", "-r", spec_file, "--no-input" } }
            };

            auto found_it = update ? pip_update_command.find(name) : pip_install_command.find(name);
            if (found_it != pip_install_command.end())
            {
                return found_it->second;
            }
            else
            {
                return tl::unexpected(std::runtime_error(fmt::format(
                    "no {} instruction found for package manager '{}'",
                    update ? "update" : "install",
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

    tl::expected<command_args, std::runtime_error>
    install_for_other_pkgmgr(const Context& ctx, const detail::other_pkg_mgr_spec& other_spec, bool update)
    {
        const auto& pkg_mgr = other_spec.pkg_mgr;
        const auto& deps = other_spec.deps;
        const auto& cwd = other_spec.cwd;

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
            update ? "Updating" : "Installing",
            pkg_mgr,
            fmt::join(deps, ", ")
        );

        fmt::print(LOG_INFO, "Calling: {}", fmt::join(command, " "));

        auto [status, ec] = reproc::run(wrapped_command, options);
        assert_reproc_success(options, status, ec);
        if (status != 0)
        {
            throw std::runtime_error(
                fmt::format("pip failed to {} packages", update ? "update" : "install")
            );
        }

        return command;
    }
}
