// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "common_options.hpp"

#include "mamba/api/configuration.hpp"
#include "mamba/api/update.hpp"
#include "mamba/api/channel_loader.hpp"

#include "mamba/core/transaction.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/util_os.hpp"
#include "version.hpp"

extern "C"
{
#include "solv/evr.h"
#include "solv/pool.h"
#include "solv/conda.h"
#include "solv/repo.h"
#include "solv/selection.h"
#include "solv/solver.h"
}

using namespace mamba;  // NOLINT(build/namespaces)

void
update_self(const std::optional<std::string>& version)
{
    auto& config = mamba::Configuration::instance();
    auto& ctx = mamba::Context::instance();
    config.load();

    mamba::MPool pool;
    mamba::MultiPackageCache package_caches(ctx.pkgs_dirs);

    auto exp_loaded = load_channels(pool, package_caches, 0);
    if (!exp_loaded)
    {
        throw std::runtime_error(exp_loaded.error().what());
    }

    pool.create_whatprovides();
    std::string matchspec;
    if (!version)
    {
        matchspec = fmt::format("micromamba>{}", umamba::version());
    }
    else
    {
        matchspec = fmt::format("micromamba={}", version.value());
    }

    auto solvable_ids = pool.select_solvables(pool.matchspec2id(matchspec));

    if (solvable_ids.empty())
    {
        if (pool.select_solvables(pool.matchspec2id("micromamba")).empty())
        {
            throw std::runtime_error(
                "micromamba not found in the loaded channels. Add 'conda-forge' to your config file.");
        }
        else
        {
            throw std::runtime_error(
                "No newer version of micromamba found in the loaded channels.");
        }
    }

    std::sort(solvable_ids.begin(),
              solvable_ids.end(),
              [&pool](Id a, Id b)
              {
                  Solvable* sa;
                  Solvable* sb;
                  sa = pool_id2solvable(pool, a);
                  sb = pool_id2solvable(pool, b);
                  return (pool_evrcmp(pool, sa->evr, sb->evr, EVRCMP_COMPARE) > 0);
              });

    std::optional<PackageInfo> latest_micromamba = pool.id2pkginfo(solvable_ids[0]);
    LOG_WARNING << latest_micromamba.value().url;

    ctx.download_only = true;
    MTransaction t(pool, { latest_micromamba.value() }, package_caches);
    auto exp_prefix_data = PrefixData::create(ctx.root_prefix);
    if (!exp_prefix_data)
    {
        // TODO: propagate tl::expected mechanism
        throw std::runtime_error(exp_prefix_data.error().what());
    }
    PrefixData& prefix_data = exp_prefix_data.value();
    t.execute(prefix_data);

    fs::u8path mamba_exe = get_self_exe_path();
    fs::u8path mamba_exe_bkup = mamba_exe;
    mamba_exe_bkup.replace_extension(".bkup");

    fs::u8path cache_path = package_caches.get_extracted_dir_path(latest_micromamba.value())
                            / latest_micromamba.value().str();

    if (on_win)
    {
        fs::rename(mamba_exe, mamba_exe_bkup);
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
    }

    fs::remove(mamba_exe_bkup);
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

    static bool update_self_in = false;
    subcom->add_flag("--self", update_self_in, "Update micromamba");
    static std::optional<std::string> version;
    subcom->add_option("--version", version, "Update micromamba");

    subcom->callback(
        [&]()
        {
            if (update_self_in)
                return update_self(version);
            update(update_all, prune);
        });
}
