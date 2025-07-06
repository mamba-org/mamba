// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <fmt/color.h>
#include <fmt/format.h>
#include <reproc++/run.hpp>

#include "mamba/api/channel_loader.hpp"
#include "mamba/api/configuration.hpp"
#include "mamba/api/update.hpp"
#include "mamba/core/channel_context.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/package_database_loader.hpp"
#include "mamba/core/transaction.hpp"
#include "mamba/core/util_os.hpp"
#include "mamba/solver/database_utils.hpp"
#include "mamba/solver/solver_factory.hpp"
#include "mamba/util/build.hpp"

#ifdef __APPLE__
#include "mamba/core/util_os.hpp"
#endif

#include "common_options.hpp"
#include "version.hpp"

using namespace mamba;  // NOLINT(build/namespaces)

void
set_update_command(CLI::App* subcom, Configuration& config)
{
    init_install_options(subcom, config);

    static bool prune_deps = true;
    static bool update_all = false;
    subcom->add_flag("--prune-deps,!--no-prune-deps", prune_deps, "Prune dependencies (default)");

    subcom->get_option("specs")->description("Specs to update in the environment");
    subcom->add_flag("-a,--all", update_all, "Update all packages in the environment");

    subcom->callback(
        [&]
        {
            auto update_params = UpdateParams{
                update_all ? UpdateAll::Yes : UpdateAll::No,
                prune_deps ? PruneDeps::Yes : PruneDeps::No,
                EnvUpdate::No,
                RemoveNotSpecified::No,
            };
            return update(config, update_params);
        }
    );
}

#ifdef BUILDING_MICROMAMBA
namespace
{
    auto database_latest_package(solver::DatabaseVariant& database, specs::MatchSpec spec)
        -> std::optional<specs::PackageInfo>
    {
        auto out = std::optional<specs::PackageInfo>();
        if (auto* libsolv_db = std::get_if<solver::libsolv::Database>(&database))
        {
            libsolv_db->for_each_package_matching(
                spec,
                [&](auto pkg)
                {
                    if (!out
                        || (specs::Version::parse(pkg.version).value_or(specs::Version())
                            > specs::Version::parse(out->version).value_or(specs::Version())))
                    {
                        out = std::move(pkg);
                    }
                }
            );
        }
        else if (auto* resolvo_db = std::get_if<solver::resolvo::Database>(&database))
        {
            // For resolvo, we need to get all candidates for the package and find the latest
            // version
            auto candidates = resolvo_db->get_candidates(
                resolvo_db->name_pool.alloc(resolvo::String(spec.name().to_string()))
            );
            if (candidates.candidates.empty())
            {
                return std::nullopt;
            }

            // Sort candidates by version
            resolvo_db->sort_candidates(candidates.candidates);

            // Get the latest version (last in the sorted list)
            auto latest_solvable = candidates.candidates[candidates.candidates.size() - 1];
            return resolvo_db->solvable_pool[latest_solvable];
        }
        return out;
    };
}

int
update_self(Configuration& config, const std::optional<std::string>& version)
{
    auto& ctx = config.context();
    config.load();

    // set target_prefix to root_prefix (irrelevant, but transaction tries to lock
    // the conda-meta folder of the target_prefix)
    ctx.prefix_params.target_prefix = ctx.prefix_params.root_prefix;

    auto channel_context = ChannelContext::make_conda_compatible(ctx);

    auto db_variant = [&]() -> solver::DatabaseVariant
    {
        if (ctx.experimental_resolvo_solver)
        {
            return solver::resolvo::Database{ channel_context.params() };
        }
        else
        {
            solver::libsolv::Database database{ channel_context.params() };
            add_spdlog_logger_to_database(database);
            return database;
        }
    }();

    mamba::MultiPackageCache package_caches(ctx.pkgs_dirs, ctx.validation_params);

    auto exp_loaded = load_channels(ctx, channel_context, db_variant, package_caches);
    if (!exp_loaded)
    {
        throw exp_loaded.error();
    }

    auto matchspec = specs::MatchSpec::parse(
                         version ? fmt::format("micromamba={}", version.value())
                                 : fmt::format("micromamba>{}", umamba::version())
    )
                         .or_else([](specs::ParseError&& err) { throw std::move(err); })
                         .value();

    auto latest_micromamba = database_latest_package(db_variant, matchspec);

    if (!latest_micromamba.has_value())
    {
        if (mamba::solver::database_has_package(
                db_variant,
                specs::MatchSpec::parse("micromamba").value()
            ))
        {
            Console::instance().print(
                fmt::format("\nYour micromamba version ({}) is already up to date.", umamba::version())
            );
            return 0;
        }
        else
        {
            throw mamba::mamba_error(
                "No micromamba found in the loaded channels. Add 'conda-forge' to your config file.",
                mamba_error_code::selfupdate_failure
            );
        }
    }

    Console::stream() << fmt::format(
        fg(fmt::terminal_color::green),
        "\n  Installing micromamba version: {} (currently installed {})",
        latest_micromamba.value().version,
        umamba::version()
    );

    Console::instance().print(
        fmt::format("  Fetching micromamba from {}\n", latest_micromamba.value().package_url)
    );

    ctx.download_only = true;
    MTransaction t(ctx, db_variant, { latest_micromamba.value() }, package_caches);
    auto exp_prefix_data = PrefixData::create(ctx.prefix_params.root_prefix, channel_context);
    if (!exp_prefix_data)
    {
        throw exp_prefix_data.error();
    }

    PrefixData& prefix_data = exp_prefix_data.value();
    t.execute(ctx, channel_context, prefix_data);

    fs::u8path mamba_exe = get_self_exe_path();
    fs::u8path mamba_exe_bkup = mamba_exe;
    mamba_exe_bkup.replace_extension(mamba_exe.extension().string() + ".bkup");

    fs::u8path cache_path = package_caches.get_extracted_dir_path(latest_micromamba.value())
                            / latest_micromamba.value().str();

    fs::rename(mamba_exe, mamba_exe_bkup);

    try
    {
        if (util::on_win)
        {
            fs::copy_file(
                cache_path / "Library" / "bin" / "micromamba.exe",
                mamba_exe,
                fs::copy_options::overwrite_existing
            );
        }
        else
        {
            fs::copy_file(
                cache_path / "bin" / "micromamba",
                mamba_exe,
                fs::copy_options::overwrite_existing
            );
#ifdef __APPLE__
            codesign(mamba_exe, false);
#endif
            fs::remove(mamba_exe_bkup);
        }
    }
    catch (std::exception& e)
    {
        LOG_ERROR << "Error while updating micromamba: " << e.what();
        LOG_ERROR << "Restoring backup";
        fs::remove(mamba_exe);
        fs::rename(mamba_exe_bkup, mamba_exe);
        throw;
    }

    // Command to reinit shell from the new executable.
    std::vector<std::string> command = { mamba_exe, "shell", "reinit" };

    // The options for the process
    reproc::options options;
    options.redirect.parent = true;  // Redirect output
    options.deadline = reproc::milliseconds(5000);

    // Run the command in a redirected process
    int status = -1;
    std::error_code ec;
    std::tie(status, ec) = reproc::run(command, options);

    if (ec)
    {
        std::cerr << "error: " << ec.message() << std::endl;
    }

    return ec ? ec.value() : status;
}

void
set_self_update_command(CLI::App* subcom, Configuration& config)
{
    init_install_options(subcom, config);

    static std::optional<std::string> version;
    subcom->add_option("--version", version, "Install specific micromamba version")
        ->option_text("VERSION");

    subcom->callback([&] { return update_self(config, version); });
}
#endif
