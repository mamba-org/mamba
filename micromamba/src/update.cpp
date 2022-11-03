// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <fmt/color.h>
#include <fmt/format.h>

#include "mamba/api/configuration.hpp"
#include "mamba/api/channel_loader.hpp"
#include "mamba/api/shell.hpp"
#include "mamba/api/update.hpp"

#include "mamba/core/context.hpp"
#include "mamba/core/transaction.hpp"
#include "mamba/core/util_os.hpp"

#include "common_options.hpp"
#include "version.hpp"

using namespace mamba;  // NOLINT(build/namespaces)

int
update_self(const std::optional<std::string>& version)
{
    auto& config = mamba::Configuration::instance();
    auto& ctx = mamba::Context::instance();
    config.load();

    // set target_prefix to root_prefix (irrelevant, but transaction tries to lock
    // the conda-meta folder of the target_prefix)
    ctx.target_prefix = ctx.root_prefix;

    mamba::MPool pool;
    mamba::MultiPackageCache package_caches(ctx.pkgs_dirs);

    auto exp_loaded = load_channels(pool, package_caches, 0);
    if (!exp_loaded)
    {
        throw exp_loaded.error();
    }

    pool.create_whatprovides();
    std::string matchspec = version ? fmt::format("micromamba={}", version.value())
                                    : fmt::format("micromamba>{}", umamba::version());

    auto solvable_ids = pool.select_solvables(pool.matchspec2id(matchspec), true);

    if (solvable_ids.empty())
    {
        if (pool.select_solvables(pool.matchspec2id("micromamba")).empty())
        {
            throw mamba::mamba_error(
                "No micromamba found in the loaded channels. Add 'conda-forge' to your config file.",
                mamba_error_code::selfupdate_failure);
        }
        else
        {
            Console::instance().print(fmt::format(
                "\nYour micromamba version ({}) is already up to date.", umamba::version()));
            return 0;
        }
    }

    std::optional<PackageInfo> latest_micromamba = pool.id2pkginfo(solvable_ids[0]);
    if (!latest_micromamba)
    {
        throw mamba::mamba_error("Could not convert solvable to PackageInfo",
                                 mamba_error_code::internal_failure);
    }
    Console::instance().stream() << fmt::format(
        fg(fmt::color::green),
        "\n  Installing micromamba version: {} (currently installed {})",
        latest_micromamba.value().version,
        umamba::version());

    Console::instance().print(
        fmt::format("  Fetching micromamba from {}\n", latest_micromamba.value().url));

    ctx.download_only = true;
    MTransaction t(pool, { latest_micromamba.value() }, package_caches);
    auto exp_prefix_data = PrefixData::create(ctx.root_prefix);
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
        if (on_win)
        {
            fs::copy_file(cache_path / "Library" / "bin" / "micromamba.exe",
                          mamba_exe,
                          fs::copy_options::overwrite_existing);
        }
        else
        {
            fs::copy_file(
                cache_path / "bin" / "micromamba", mamba_exe, fs::copy_options::overwrite_existing);
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

    Console::instance().print("\nReinitializing all previously initialized shells\n");
    std::string shell_type = "";
    mamba::shell("reinit", shell_type, ctx.root_prefix, false);

    return 0;
}


void
set_update_command(CLI::App* subcom)
{
    Configuration::instance();

    init_install_options(subcom);

    static bool prune = true;
    static bool update_all = false;
    subcom->add_flag("--prune,!--no-prune", prune, "Prune dependencies (default)");

    subcom->get_option("specs")->description("Specs to update in the environment");
    subcom->add_flag("-a,--all", update_all, "Update all packages in the environment");

    subcom->callback([&]() { update(update_all, prune); });
}

void
set_self_update_command(CLI::App* subcom)
{
    Configuration::instance();

    init_install_options(subcom);

    static std::optional<std::string> version;
    subcom->add_option("--version", version, "Install specific micromamba version");

    subcom->callback([&]() { return update_self(version); });
}
