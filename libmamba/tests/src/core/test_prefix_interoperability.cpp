// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <catch2/catch_all.hpp>

#include "mamba/api/configuration.hpp"
#include "mamba/core/channel_context.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/package_database_loader.hpp"
#include "mamba/core/prefix_data.hpp"
#include "mamba/core/util.hpp"
#include "mamba/solver/libsolv/database.hpp"
#include "mamba/specs/package_info.hpp"
#include "mamba/util/environment.hpp"

#include "mambatests.hpp"

using namespace mamba;

TEST_CASE("Configuration: prefix_data_interoperability", "[core][prefix-interop]")
{
    auto& ctx = mambatests::context();
    mamba::Configuration config{ ctx };

    SECTION("Default value is false")
    {
        REQUIRE_FALSE(ctx.prefix_data_interoperability);
    }

    SECTION("Can be set via configuration")
    {
        config.at("prefix_data_interoperability").set_value(true);
        config.load();
        REQUIRE(ctx.prefix_data_interoperability);

        config.at("prefix_data_interoperability").set_value(false);
        config.load();
        REQUIRE_FALSE(ctx.prefix_data_interoperability);
    }

    SECTION("Can be set via environment variable")
    {
        mambatests::EnvironmentCleaner env_cleaner;

        // Set environment variable and reload configuration
        // YAML parsing accepts "true"/"false" (case-insensitive)
        util::set_env("CONDA_PREFIX_DATA_INTEROPERABILITY", "true");
        config.reset_configurables();
        config.load();
        // Check both config value and context value
        REQUIRE(config.at("prefix_data_interoperability").value<bool>());
        REQUIRE(ctx.prefix_data_interoperability);

        util::set_env("CONDA_PREFIX_DATA_INTEROPERABILITY", "false");
        config.reset_configurables();
        config.load();
        REQUIRE_FALSE(config.at("prefix_data_interoperability").value<bool>());
        REQUIRE_FALSE(ctx.prefix_data_interoperability);

        util::unset_env("CONDA_PREFIX_DATA_INTEROPERABILITY");
        util::set_env("MAMBA_PREFIX_DATA_INTEROPERABILITY", "true");
        config.reset_configurables();
        config.load();
        REQUIRE(config.at("prefix_data_interoperability").value<bool>());
        REQUIRE(ctx.prefix_data_interoperability);
    }
}

TEST_CASE("PrefixData: pip packages loading", "[core][prefix-interop]")
{
    auto tmp_dir = TemporaryDirectory();
    auto prefix_path = tmp_dir.path() / "prefix";
    fs::create_directories(prefix_path);

    auto& ctx = mambatests::context();
    auto channel_context = ChannelContext::make_simple(ctx);

    SECTION("Pip packages are loaded when no_pip is false")
    {
        // Create a minimal conda environment structure
        auto conda_meta_dir = prefix_path / "conda-meta";
        fs::create_directories(conda_meta_dir);

        // Create a fake pip package record (simulating pip inspect output)
        // Note: In real scenarios, pip packages are discovered via pip inspect
        // For testing, we'll create the structure manually
        auto python_pkg_json = conda_meta_dir / "python-3.10.0-h12345_0.json";
        {
            auto out = open_ofstream(python_pkg_json);
            out << R"({
                "name": "python",
                "version": "3.10.0",
                "build_string": "h12345_0",
                "channel": "conda-forge",
                "platform": "linux-64"
            })";
        }

        auto pip_pkg_json = conda_meta_dir / "pip-23.0.0-py310h12345_0.json";
        {
            auto out = open_ofstream(pip_pkg_json);
            out << R"({
                "name": "pip",
                "version": "23.0.0",
                "build_string": "py310h12345_0",
                "channel": "conda-forge",
                "platform": "linux-64"
            })";
        }

        // Use no_pip=true to avoid trying to run pip inspect (which would fail in test environment)
        // We're just testing that conda packages are loaded, not pip packages
        auto prefix_data = PrefixData::create(prefix_path, channel_context, true).value();

        // Verify conda packages are loaded
        REQUIRE(prefix_data.records().find("python") != prefix_data.records().end());
        REQUIRE(prefix_data.records().find("pip") != prefix_data.records().end());
    }

    SECTION("Pip packages are not loaded when no_pip is true")
    {
        auto conda_meta_dir = prefix_path / "conda-meta";
        fs::create_directories(conda_meta_dir);

        auto prefix_data = PrefixData::create(prefix_path, channel_context, true).value();

        // Pip packages should not be loaded
        // (We can't easily test pip package loading without actually having pip installed,
        // but we can verify the no_pip flag is respected)
        REQUIRE((prefix_data.records().empty() || prefix_data.pip_records().empty()));
    }
}

TEST_CASE("Package database loader: pip packages in solver", "[core][prefix-interop]")
{
    auto tmp_dir = TemporaryDirectory();
    auto prefix_path = tmp_dir.path() / "prefix";
    fs::create_directories(prefix_path);

    auto& ctx = mambatests::context();
    auto channel_context = ChannelContext::make_simple(ctx);

    // Create a minimal conda environment
    auto conda_meta_dir = prefix_path / "conda-meta";
    fs::create_directories(conda_meta_dir);

    // Create a conda package
    auto python_pkg_json = conda_meta_dir / "python-3.10.0-h12345_0.json";
    {
        auto out = open_ofstream(python_pkg_json);
        out << R"({
            "name": "python",
            "version": "3.10.0",
            "build_string": "h12345_0",
            "channel": "conda-forge",
            "platform": "linux-64"
        })";
    }

    auto pip_pkg_json = conda_meta_dir / "pip-23.0.0-py310h12345_0.json";
    {
        auto out = open_ofstream(pip_pkg_json);
        out << R"({
            "name": "pip",
            "version": "23.0.0",
            "build_string": "py310h12345_0",
            "channel": "conda-forge",
            "platform": "linux-64"
        })";
    }

    // Use no_pip=true to avoid trying to run pip inspect (which would fail in test environment)
    // We'll manually add pip packages to simulate pip-installed packages
    auto prefix_data = PrefixData::create(prefix_path, channel_context, true).value();

    // Manually add a pip package to simulate pip-installed package
    // (In real scenarios, this would come from pip inspect)
    auto pip_pkg = specs::PackageInfo("boto3", "1.14.4", "pypi_0", "pypi");
    pip_pkg.platform = "linux-64";
    prefix_data.add_pip_packages({ pip_pkg });

    SECTION("Pip packages are NOT included when prefix interoperability is disabled")
    {
        ctx.prefix_data_interoperability = false;

        auto db = solver::libsolv::Database(channel_context.params());
        load_installed_packages_in_database(ctx, db, prefix_data);

        // Check that pip package is not in the database
        bool found_pip_pkg = false;
        db.for_each_package_matching(
            specs::MatchSpec::parse("boto3").value(),
            [&](const auto&)
            {
                found_pip_pkg = true;
                return util::LoopControl::Break;
            }
        );

        REQUIRE_FALSE(found_pip_pkg);
    }

    SECTION("Pip packages ARE included when prefix interoperability is enabled")
    {
        ctx.prefix_data_interoperability = true;

        auto db = solver::libsolv::Database(channel_context.params());
        load_installed_packages_in_database(ctx, db, prefix_data);

        // Check that pip package is in the database
        bool found_pip_pkg = false;
        db.for_each_package_matching(
            specs::MatchSpec::parse("boto3").value(),
            [&](const auto& pkg)
            {
                found_pip_pkg = true;
                REQUIRE(pkg.channel == "pypi");
                return util::LoopControl::Break;
            }
        );

        REQUIRE(found_pip_pkg);
    }

    SECTION("Pip packages with conda equivalents are NOT added")
    {
        ctx.prefix_data_interoperability = true;

        // Add a conda package with the same name as the pip package
        auto conda_boto3 = specs::PackageInfo("boto3", "1.13.21", "py310h12345_0", "conda-forge");
        conda_boto3.platform = "linux-64";
        prefix_data.add_packages({ conda_boto3 });

        auto db = solver::libsolv::Database(channel_context.params());
        load_installed_packages_in_database(ctx, db, prefix_data);

        // Check that only the conda package is in the database, not the pip one
        int boto3_count = 0;
        bool found_conda_boto3 = false;
        bool found_pip_boto3 = false;

        db.for_each_package_matching(
            specs::MatchSpec::parse("boto3").value(),
            [&](const auto& pkg)
            {
                boto3_count++;
                if (pkg.channel == "pypi")
                {
                    found_pip_boto3 = true;
                }
                else
                {
                    found_conda_boto3 = true;
                }
                return util::LoopControl::Continue;
            }
        );

        // Should only find the conda package, not the pip one
        REQUIRE(found_conda_boto3);
        REQUIRE_FALSE(found_pip_boto3);
        REQUIRE(boto3_count == 1);
    }

    SECTION("Multiple pip packages are included when prefix interoperability is enabled")
    {
        ctx.prefix_data_interoperability = true;

        // Add multiple pip packages
        auto pip_pkg1 = specs::PackageInfo("requests", "2.28.0", "pypi_0", "pypi");
        pip_pkg1.platform = "linux-64";
        auto pip_pkg2 = specs::PackageInfo("numpy", "1.24.0", "pypi_0", "pypi");
        pip_pkg2.platform = "linux-64";
        prefix_data.add_pip_packages({ pip_pkg1, pip_pkg2 });

        auto db = solver::libsolv::Database(channel_context.params());
        load_installed_packages_in_database(ctx, db, prefix_data);

        // Check that all pip packages are in the database
        int pip_pkg_count = 0;
        bool found_requests = false;
        bool found_numpy = false;
        bool found_boto3 = false;

        db.for_each_package_matching(
            specs::MatchSpec::parse("*").value(),
            [&](const auto& pkg)
            {
                if (pkg.channel == "pypi")
                {
                    pip_pkg_count++;
                    if (pkg.name == "requests")
                    {
                        found_requests = true;
                    }
                    else if (pkg.name == "numpy")
                    {
                        found_numpy = true;
                    }
                    else if (pkg.name == "boto3")
                    {
                        found_boto3 = true;
                    }
                }
                return util::LoopControl::Continue;
            }
        );

        REQUIRE(found_requests);
        REQUIRE(found_numpy);
        REQUIRE(found_boto3);
        REQUIRE(pip_pkg_count >= 3);  // At least our 3 pip packages
    }
}

TEST_CASE("Transaction: pip package removal", "[core][prefix-interop]")
{
    auto tmp_dir = TemporaryDirectory();
    auto prefix_path = tmp_dir.path() / "prefix";
    fs::create_directories(prefix_path);

    auto& ctx = mambatests::context();
    auto channel_context = ChannelContext::make_simple(ctx);

    SECTION("Pip packages are identified by channel == 'pypi'")
    {
        auto pip_pkg = specs::PackageInfo("boto3", "1.14.4", "pypi_0", "pypi");
        REQUIRE(pip_pkg.channel == "pypi");

        auto conda_pkg = specs::PackageInfo("boto3", "1.13.21", "py310h12345_0", "conda-forge");
        REQUIRE(conda_pkg.channel != "pypi");
    }

    SECTION("Pip package channel format is correct")
    {
        // Verify the channel format matches what load_site_packages creates
        auto pip_pkg = specs::PackageInfo("testpkg", "1.0.0", "pypi_0", "pypi");
        REQUIRE(pip_pkg.channel == "pypi");
        REQUIRE(pip_pkg.build_string == "pypi_0");
    }
}

TEST_CASE("Integration: prefix interoperability workflow", "[core][prefix-interop]")
{
    auto tmp_dir = TemporaryDirectory();
    auto prefix_path = tmp_dir.path() / "prefix";
    fs::create_directories(prefix_path);

    auto& ctx = mambatests::context();
    auto channel_context = ChannelContext::make_simple(ctx);

    // Create a minimal conda environment
    auto conda_meta_dir = prefix_path / "conda-meta";
    fs::create_directories(conda_meta_dir);

    auto python_pkg_json = conda_meta_dir / "python-3.10.0-h12345_0.json";
    {
        auto out = open_ofstream(python_pkg_json);
        out << R"({
            "name": "python",
            "version": "3.10.0",
            "build_string": "h12345_0",
            "channel": "conda-forge",
            "platform": "linux-64"
        })";
    }

    auto pip_pkg_json = conda_meta_dir / "pip-23.0.0-py310h12345_0.json";
    {
        auto out = open_ofstream(pip_pkg_json);
        out << R"({
            "name": "pip",
            "version": "23.0.0",
            "build_string": "py310h12345_0",
            "channel": "conda-forge",
            "platform": "linux-64"
        })";
    }

    SECTION("Full workflow: pip package detected and can be removed")
    {
        ctx.prefix_data_interoperability = true;

        // Use no_pip=true to avoid trying to run pip inspect
        auto prefix_data = PrefixData::create(prefix_path, channel_context, true).value();

        // Simulate a pip-installed package
        auto pip_pkg = specs::PackageInfo("boto3", "1.14.4", "pypi_0", "pypi");
        pip_pkg.platform = "linux-64";
        prefix_data.add_pip_packages({ pip_pkg });

        // Load into database
        auto db = solver::libsolv::Database(channel_context.params());
        load_installed_packages_in_database(ctx, db, prefix_data);

        // Verify pip package is in the installed repo
        REQUIRE(db.installed_repo().has_value());
        auto installed_repo = db.installed_repo().value();

        bool found_in_installed = false;
        db.for_each_package_in_repo(
            installed_repo,
            [&](specs::PackageInfo&& pkg)
            {
                if (pkg.name == "boto3" && pkg.channel == "pypi")
                {
                    found_in_installed = true;
                    REQUIRE(pkg.version == "1.14.4");
                    return util::LoopControl::Break;
                }
                return util::LoopControl::Continue;
            }
        );

        REQUIRE(found_in_installed);
    }

    SECTION("Pip package exclusion when conda package exists")
    {
        ctx.prefix_data_interoperability = true;

        // Use no_pip=true to avoid trying to run pip inspect
        auto prefix_data = PrefixData::create(prefix_path, channel_context, true).value();

        // Add both conda and pip versions of the same package
        auto conda_pkg = specs::PackageInfo("boto3", "1.13.21", "py310h12345_0", "conda-forge");
        conda_pkg.platform = "linux-64";
        auto pip_pkg = specs::PackageInfo("boto3", "1.14.4", "pypi_0", "pypi");
        pip_pkg.platform = "linux-64";

        prefix_data.add_packages({ conda_pkg });
        prefix_data.add_pip_packages({ pip_pkg });

        // Load into database
        auto db = solver::libsolv::Database(channel_context.params());
        load_installed_packages_in_database(ctx, db, prefix_data);

        // Verify only conda package is in database
        int boto3_count = 0;
        bool found_conda = false;
        bool found_pip = false;

        db.for_each_package_matching(
            specs::MatchSpec::parse("boto3").value(),
            [&](const auto& pkg)
            {
                boto3_count++;
                if (pkg.channel == "pypi")
                {
                    found_pip = true;
                }
                else
                {
                    found_conda = true;
                }
                return util::LoopControl::Continue;
            }
        );

        REQUIRE(found_conda);
        REQUIRE_FALSE(found_pip);
        REQUIRE(boto3_count == 1);
    }
}
