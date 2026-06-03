// Copyright (c) 2025, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <chrono>
#include <fstream>
#include <map>
#include <sstream>

#include <catch2/catch_all.hpp>
#include <nlohmann/json.hpp>

#include "mamba/api/channel_loader.hpp"
#include "mamba/core/channel_context.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/package_cache.hpp"
#include "mamba/core/subdir_index.hpp"
#include "mamba/core/util.hpp"
#include "mamba/solver/libsolv/database.hpp"
#include "mamba/solver/libsolv/repo_info.hpp"

#include "mambatests.hpp"
#include "mambatests_utils.hpp"

using namespace mamba;
using mambatests::make_simple_channel;

namespace
{
    [[nodiscard]] auto make_subdir_loader(
        const specs::Channel& channel,
        const fs::u8path& cache_root,
        const std::string& platform = "linux-64"
    ) -> SubdirIndexLoader
    {
        auto caches = MultiPackageCache({ cache_root }, ValidationParams{});
        return SubdirIndexLoader::create({}, channel, platform, caches).value();
    }

    void write_chain_repodata(const fs::u8path& repodata_path, std::size_t package_count)
    {
        nlohmann::json repodata;
        repodata["info"]["subdir"] = "linux-64";
        repodata["repodata_version"] = 1;
        repodata["packages"] = nlohmann::json::object();

        for (std::size_t i = 0; i < package_count; ++i)
        {
            nlohmann::json record;
            record["name"] = "pkg-" + std::to_string(i);
            record["version"] = "1.0";
            record["build"] = "0";
            record["build_number"] = 0;
            record["subdir"] = "linux-64";
            record["depends"] = nlohmann::json::array();
            if (i > 0)
            {
                record["depends"].push_back("pkg-" + std::to_string(i - 1) + " >=1.0");
            }
            repodata["packages"]["pkg-" + std::to_string(i) + "-1.0-0.tar.bz2"] = std::move(record);
        }

        nlohmann::json python;
        python["name"] = "python";
        python["version"] = "3.11";
        python["build"] = "0";
        python["build_number"] = 0;
        python["subdir"] = "linux-64";
        python["depends"] = nlohmann::json::array(
            { "pkg-" + std::to_string(package_count - 1) + " >=1.0" }
        );
        repodata["packages"]["python-3.11-0.tar.bz2"] = std::move(python);

        std::ofstream out(repodata_path.string());
        out << repodata.dump();
    }
}

TEST_CASE("init_channels", "[mamba::api][channel_loader]")
{
    auto ctx = Context();
    auto channel_context = ChannelContext::make_conda_compatible(ctx);

    SECTION("Empty channels and mirrored_channels does not add channel mirrors")
    {
        ctx.channels = {};
        ctx.mirrored_channels = {};

        init_channels(ctx, channel_context);

        // mirror_map always has a default PassThroughMirror(""); no channel-specific
        // mirrors should be added when channels are empty.
        REQUIRE_FALSE(ctx.mirrors.has_mirrors("conda-forge"));
    }

    SECTION("Single channel registers mirrors")
    {
        ctx.channels = { "conda-forge" };
        ctx.mirrored_channels = {};

        init_channels(ctx, channel_context);

        for (const auto& location : ctx.channels)
        {
            for (const specs::Channel& channel : channel_context.make_channel(location))
            {
                REQUIRE(ctx.mirrors.has_mirrors(channel.id()));
            }
        }
    }

    SECTION("Mirrored channel registers mirrors")
    {
        ctx.channels = {};
        ctx.mirrored_channels = { { "conda-forge", { "https://conda.anaconda.org/conda-forge" } } };

        init_channels(ctx, channel_context);

        for (const auto& [name, urls] : ctx.mirrored_channels)
        {
            for (const specs::Channel& channel : channel_context.make_channel(name, urls))
            {
                REQUIRE(ctx.mirrors.has_mirrors(channel.id()));
            }
        }
    }

    SECTION("Regular channel skipped when in mirrored_channels")
    {
        ctx.channels = { "conda-forge" };
        ctx.mirrored_channels = { { "conda-forge", { "https://conda.anaconda.org/conda-forge" } } };

        init_channels(ctx, channel_context);

        // conda-forge is in mirrored_channels so it was processed there; still should have mirrors
        REQUIRE(ctx.mirrors.has_mirrors("conda-forge"));
    }
}

TEST_CASE("init_channels_from_package_urls", "[mamba::api][channel_loader]")
{
    auto ctx = Context();
    auto channel_context = ChannelContext::make_conda_compatible(ctx);

    SECTION("Package URL spec registers mirrors for package channel")
    {
        // Valid conda package URL; channel will be conda-forge
        std::vector<std::string> specs = {
            "https://conda.anaconda.org/conda-forge/linux-64/python-3.11.0-h1234567_0.conda"
        };

        init_channels_from_package_urls(ctx, channel_context, specs);

        REQUIRE(ctx.mirrors.has_mirrors("conda-forge"));
    }
}

TEST_CASE("load_channels", "[mamba::api][channel_loader]")
{
    // Use test singletons so Console/progress bar are initialized (avoids SIGABRT)
    Context& ctx = mambatests::context();

    // Save and restore context so we don't affect other tests (e.g. test_configuration
    // expects default ssl_verify / config state)
    struct ContextGuard
    {
        Context& ctx;
        std::vector<std::string> channels;
        std::map<std::string, std::vector<std::string>> mirrored_channels;
        std::vector<fs::u8path> pkgs_dirs;
        bool offline;
        std::string ssl_verify;
        std::string channel_alias;
        std::map<std::string, std::string> proxy_servers;

        explicit ContextGuard(Context& c)
            : ctx(c)
            , channels(c.channels)
            , mirrored_channels(c.mirrored_channels)
            , pkgs_dirs(c.pkgs_dirs)
            , offline(c.offline)
            , ssl_verify(c.remote_fetch_params.ssl_verify)
            , channel_alias(c.channel_alias)
            , proxy_servers(c.remote_fetch_params.proxy_servers)
        {
        }

        ~ContextGuard()
        {
            ctx.channels = std::move(channels);
            ctx.mirrored_channels = std::move(mirrored_channels);
            ctx.pkgs_dirs = std::move(pkgs_dirs);
            ctx.offline = offline;
            ctx.remote_fetch_params.ssl_verify = std::move(ssl_verify);
            ctx.channel_alias = std::move(channel_alias);
            ctx.remote_fetch_params.proxy_servers = std::move(proxy_servers);
        }
    };

    ContextGuard guard(ctx);

    ctx.channels = {};
    ctx.mirrored_channels = {};
    ctx.pkgs_dirs = {};
    ctx.offline = true;

    auto channel_context = ChannelContext::make_conda_compatible(ctx);
    auto database = solver::libsolv::Database(channel_context.params());
    const auto tmp_dir = TemporaryDirectory();
    auto package_caches = MultiPackageCache({ tmp_dir.path() }, ValidationParams{});

    auto result = load_channels(ctx, channel_context, database, package_caches);

    REQUIRE(result.has_value());
    REQUIRE(database.repo_count() == 0);
}

TEST_CASE("load_channels with root_packages", "[mamba::core][mamba::api::channel_loader]")
{
    auto& ctx = mambatests::context();
    ctx.channels = { "conda-forge" };
    ctx.use_sharded_repodata = true;

    auto channel_context = ChannelContext::make_conda_compatible(ctx);
    solver::libsolv::Database db{ channel_context.params() };
    MultiPackageCache package_caches(ctx.pkgs_dirs, ctx.validation_params);

    SECTION("Empty root_packages")
    {
        auto result = load_channels(ctx, channel_context, db, package_caches, {});
        REQUIRE(result.has_value());
    }

    SECTION("With root_packages")
    {
        std::vector<std::string> root_packages = { "python" };
        auto result = load_channels(ctx, channel_context, db, package_caches, root_packages);
        REQUIRE(result.has_value());
    }
}

TEST_CASE("should_shard_then_expand_roots", "[mamba::api][channel_loader][issue_4277]")
{
    const auto tmp_dir = TemporaryDirectory();
    const auto channel = make_simple_channel("conda-forge");
    auto subdir = make_subdir_loader(channel, tmp_dir.path());

    SECTION("false when sharded repodata is disabled")
    {
        REQUIRE_FALSE(should_shard_then_expand_roots(false, { "python" }, { subdir }, 0));
    }

    SECTION("false when root_packages is empty")
    {
        subdir.set_shards_availability(true);
        REQUIRE_FALSE(should_shard_then_expand_roots(true, {}, { subdir }, 86400));
    }

    SECTION("false when no subdir has shards (pure-flat channel set)")
    {
        REQUIRE_FALSE(should_shard_then_expand_roots(true, { "python" }, { subdir }, 86400));
    }

    SECTION("true when at least one subdir has up-to-date shards")
    {
        subdir.set_shards_availability(true);
        REQUIRE(should_shard_then_expand_roots(true, { "python" }, { subdir }, 86400));
    }

    SECTION("true when only one of several subdirs has shards")
    {
        auto flat_subdir = make_subdir_loader(channel, tmp_dir.path(), "noarch");
        subdir.set_shards_availability(true);
        REQUIRE(should_shard_then_expand_roots(true, { "python" }, { flat_subdir, subdir }, 86400));
    }
}

TEST_CASE("expand_shard_root_packages_from_full_repodata_repos", "[mamba::api][channel_loader][issue_4277]")
{
    const auto resolve_params = ChannelContext::ChannelResolveParams{
        { "linux-64" },
        specs::CondaURL::parse("https://conda.anaconda.org").value()
    };
    solver::libsolv::Database db{ resolve_params };
    std::vector<specs::PackageInfo> packages;
    packages.reserve(5);

    auto python = specs::PackageInfo("python", "3.11", "test", 0);
    python.dependencies = { "libzlib >=1.0" };
    packages.push_back(std::move(python));

    auto libzlib = specs::PackageInfo("libzlib", "1.0", "test", 0);
    packages.push_back(std::move(libzlib));

    const auto repo = db.add_repo_from_packages(
        packages,
        "flat",
        solver::libsolv::PipAsPythonDependency::No
    );

    std::vector<std::string> roots = { "python" };
    expand_shard_root_packages_from_full_repodata_repos(db, { repo }, roots);

    REQUIRE(std::find(roots.begin(), roots.end(), "python") != roots.end());
    REQUIRE(std::find(roots.begin(), roots.end(), "libzlib") != roots.end());
}

TEST_CASE(
    "expand_shard_root_packages merges deps from all records with the same name",
    "[mamba::api][channel_loader][issue_4277]"
)
{
    const auto resolve_params = ChannelContext::ChannelResolveParams{
        { "linux-64" },
        specs::CondaURL::parse("https://conda.anaconda.org").value()
    };
    solver::libsolv::Database db{ resolve_params };

    auto root = specs::PackageInfo("root", "1.0", "test", 0);
    auto root_other_build = specs::PackageInfo("root", "2.0", "test", 0);
    root_other_build.dependencies = { "only-in-2.0 >=1.0" };

    std::vector<specs::PackageInfo> packages;
    packages.push_back(std::move(root));
    packages.push_back(std::move(root_other_build));
    const auto repo = db.add_repo_from_packages(
        packages,
        "flat",
        solver::libsolv::PipAsPythonDependency::No
    );

    std::vector<std::string> roots = { "root" };
    expand_shard_root_packages_from_full_repodata_repos(db, { repo }, roots);

    REQUIRE(std::find(roots.begin(), roots.end(), "only-in-2.0") != roots.end());
}

TEST_CASE(
    "flat file channel subdirs do not request shard_then_expand",
    "[mamba::api][channel_loader][issue_4277]"
)
{
    const auto tmp_dir = TemporaryDirectory();
    const auto channel_root = tmp_dir.path() / "flat-channel";
    const auto subdir_path = channel_root / "linux-64";
    fs::create_directories(subdir_path);
    write_chain_repodata(subdir_path / "repodata.json", 8);

    const auto channel = make_simple_channel("file://" + channel_root.string() + "[linux-64]");
    auto subdir = make_subdir_loader(channel, tmp_dir.path());

    REQUIRE_FALSE(subdir.metadata().has_shards());
    REQUIRE_FALSE(should_shard_then_expand_roots(true, { "python" }, { subdir }, 86400));
}

TEST_CASE(
    "expand_shard_root_packages_from_full_repodata_repos chain repodata",
    "[mamba::api][channel_loader][issue_4277]"
)
{
    const auto resolve_params = ChannelContext::ChannelResolveParams{
        { "linux-64" },
        specs::CondaURL::parse("https://conda.anaconda.org").value()
    };
    solver::libsolv::Database db{ resolve_params };

    const auto tmp_dir = TemporaryDirectory();
    const auto repodata_path = tmp_dir.path() / "repodata.json";
    write_chain_repodata(repodata_path, 1500);

    const auto repo = db.add_repo_from_repodata_json(
        repodata_path,
        "file://flat/linux-64",
        "flat",
        solver::libsolv::PipAsPythonDependency::No,
        solver::libsolv::PackageTypes::CondaOrElseTarBz2,
        solver::libsolv::VerifyPackages::No,
        solver::libsolv::RepodataParser::Mamba
    );
    REQUIRE(repo.has_value());

    std::vector<std::string> roots = { "python" };
    const auto started = std::chrono::steady_clock::now();
    expand_shard_root_packages_from_full_repodata_repos(db, { repo.value() }, roots);
    const auto elapsed = std::chrono::steady_clock::now() - started;

    // Regression guard: indexed BFS over ~1500 records must stay well under multi-minute wall time.
    REQUIRE(roots.size() >= 1500);
    REQUIRE(elapsed < std::chrono::seconds(2));
}
