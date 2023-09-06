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
#include "mamba/core/channel.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/transaction.hpp"
#include "mamba/core/util_os.hpp"
#include "mamba/util/build.hpp"

#include "common_options.hpp"
#include "version.hpp"

using namespace mamba;  // NOLINT(build/namespaces)

int
update_self(Configuration& config, const std::optional<std::string>& version)
{
    auto& ctx = config.context();
    config.load();

    // set target_prefix to root_prefix (irrelevant, but transaction tries to lock
    // the conda-meta folder of the target_prefix)
    ctx.prefix_params.target_prefix = ctx.prefix_params.root_prefix;

    mamba::ChannelContext channel_context{ ctx };
    mamba::MPool pool{ channel_context };
    mamba::MultiPackageCache package_caches(ctx.pkgs_dirs, ctx.validation_params);

    auto exp_loaded = load_channels(pool, package_caches, 0);
    if (!exp_loaded)
    {
        throw exp_loaded.error();
    }

    pool.create_whatprovides();
    std::string matchspec = version ? fmt::format("micromamba={}", version.value())
                                    : fmt::format("micromamba>{}", umamba::version());

    auto solvable_ids = pool.select_solvables(pool.matchspec2id({ matchspec, channel_context }), true);

    if (solvable_ids.empty())
    {
        if (pool.select_solvables(pool.matchspec2id({ "micromamba", channel_context })).empty())
        {
            throw mamba::mamba_error(
                "No micromamba found in the loaded channels. Add 'conda-forge' to your config file.",
                mamba_error_code::selfupdate_failure
            );
        }
        else
        {
            Console::instance().print(
                fmt::format("\nYour micromamba version ({}) is already up to date.", umamba::version())
            );
            return 0;
        }
    }

    std::optional<PackageInfo> latest_micromamba = pool.id2pkginfo(solvable_ids[0]);
    if (!latest_micromamba)
    {
        throw mamba::mamba_error(
            "Could not convert solvable to PackageInfo",
            mamba_error_code::internal_failure
        );
    }
    Console::stream() << fmt::format(
        fg(fmt::terminal_color::green),
        "\n  Installing micromamba version: {} (currently installed {})",
        latest_micromamba.value().version,
        umamba::version()
    );

    Console::instance().print(
        fmt::format("  Fetching micromamba from {}\n", latest_micromamba.value().url)
    );

    ctx.download_only = true;
    MTransaction t(pool, { latest_micromamba.value() }, package_caches);
    auto exp_prefix_data = PrefixData::create(ctx.prefix_params.root_prefix, channel_context);
    if (!exp_prefix_data)
    {
        throw exp_prefix_data.error();
    }

    PrefixData& prefix_data = exp_prefix_data.value();
    t.execute(prefix_data);

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
set_update_command(CLI::App* subcom, Configuration& config)
{
    init_install_options(subcom, config);

    static bool prune = true;
    static bool update_all = false;
    subcom->add_flag("--prune,!--no-prune", prune, "Prune dependencies (default)");

    subcom->get_option("specs")->description("Specs to update in the environment");
    subcom->add_flag("-a,--all", update_all, "Update all packages in the environment");

    subcom->callback([&] { return update(config, update_all, prune); });
}

void
set_self_update_command(CLI::App* subcom, Configuration& config)
{
    init_install_options(subcom, config);

    static std::optional<std::string> version;
    subcom->add_option("--version", version, "Install specific micromamba version");

    subcom->callback([&] { return update_self(config, version); });
}
