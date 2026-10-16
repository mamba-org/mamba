// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <functional>
#include <sstream>
#include <string>
#include <vector>

#include <catch2/catch_all.hpp>
#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include "mamba/core/channel_context.hpp"
#include "mamba/core/query.hpp"
#include "mamba/solver/libsolv/database.hpp"
#include "mamba/specs/package_info.hpp"
#include "mamba/specs/version.hpp"

#include "mambatests.hpp"

using namespace mamba;

namespace
{
    /**
     * Helper function to create a PackageInfo with version and build number.
     */
    auto mkpkg(
        std::string name,
        std::string version,
        std::string build_string = "",
        std::size_t build_number = 0
    ) -> specs::PackageInfo
    {
        auto pkg = specs::PackageInfo(std::move(name));
        pkg.version = std::move(version);
        pkg.build_string = std::move(build_string);
        pkg.build_number = build_number;
        pkg.channel = "conda-forge";
        pkg.platform = "linux-64";
        // Set required fields for pretty output
        if (pkg.build_string.empty())
        {
            pkg.filename = fmt::format("{}-{}.tar.bz2", pkg.name, pkg.version);
        }
        else
        {
            pkg.filename = fmt::format("{}-{}-{}.tar.bz2", pkg.name, pkg.version, pkg.build_string);
        }
        pkg.package_url = fmt::format(
            "https://conda.anaconda.org/{}/{}/{}",
            pkg.channel,
            pkg.platform,
            pkg.filename
        );
        // Set unique sha256 for each package (used for grouping)
        // Use a combination that ensures uniqueness: name + version + build_string + build_number
        std::string unique_id = pkg.name + pkg.version + pkg.build_string
                                + std::to_string(build_number);
        pkg.sha256 = fmt::format("{:064x}", std::hash<std::string>{}(unique_id));
        return pkg;
    }
}

TEST_CASE("QueryResult version sorting", "[mamba::core][mamba::core::query]")
{
    auto& ctx = mambatests::context();
    auto channel_context = ChannelContext::make_conda_compatible(ctx);

    SECTION("Search results sorted by version and build number")
    {
        // Create a database with multiple versions of "mamba" package
        // to simulate the real-world scenario
        auto packages = std::vector<specs::PackageInfo>{
            mkpkg("mamba", "1.1.0", "py310h51d5547_2", 2),
            mkpkg("mamba", "2.5.0", "h9835478_0", 0),
            mkpkg("mamba", "2.4.0", "h7ae174a_1", 1),
            mkpkg("mamba", "2.4.0", "h7ae174a_0", 0),
            mkpkg("mamba", "1.0.0", "py310h51d5547_1", 1),
            mkpkg("mamba", "0.25.0", "h1234567_0", 0),
        };

        auto db = solver::libsolv::Database(channel_context.params());
        db.add_repo_from_packages(packages, "test-repo");

        // Perform search query
        auto result = Query::find(db, { "mamba" });
        REQUIRE_FALSE(result.empty());

        // Group by name to get all versions together
        result.groupby("name");

        // Get JSON output to verify sorting
        auto json_output = result.json();
        REQUIRE(json_output.contains("result"));
        REQUIRE(json_output["result"].contains("pkgs"));

        auto pkgs = json_output["result"]["pkgs"];
        REQUIRE(pkgs.is_array());
        REQUIRE(pkgs.size() >= 2);

        // Filter to only "mamba" packages
        std::vector<nlohmann::json> mamba_pkgs;
        for (const auto& pkg : pkgs)
        {
            if (pkg.contains("name") && pkg["name"] == "mamba")
            {
                mamba_pkgs.push_back(pkg);
            }
        }

        REQUIRE(mamba_pkgs.size() >= 2);

        // Verify versions are sorted in descending order (newest first)
        for (std::size_t i = 0; i < mamba_pkgs.size() - 1; ++i)
        {
            const auto& pkg_i = mamba_pkgs[i];
            const auto& pkg_j = mamba_pkgs[i + 1];

            REQUIRE(pkg_i.contains("version"));
            REQUIRE(pkg_j.contains("version"));

            const std::string version_i = pkg_i["version"];
            const std::string version_j = pkg_j["version"];

            // Parse versions for comparison
            auto version_obj_i = specs::Version::parse(version_i);
            auto version_obj_j = specs::Version::parse(version_j);

            if (version_obj_i.has_value() && version_obj_j.has_value())
            {
                // Version i should be >= version j (descending order)
                REQUIRE(version_obj_i.value() >= version_obj_j.value());

                // If versions are equal, check build numbers
                if (version_obj_i.value() == version_obj_j.value())
                {
                    REQUIRE(pkg_i.contains("build_number"));
                    REQUIRE(pkg_j.contains("build_number"));

                    const std::size_t build_i = pkg_i["build_number"];
                    const std::size_t build_j = pkg_j["build_number"];

                    // Build number i should be >= build number j (descending order)
                    REQUIRE(build_i >= build_j);
                }
            }
        }

        // Verify the first result is the latest version (2.5.0)
        const auto& first_pkg = mamba_pkgs[0];
        REQUIRE(first_pkg.contains("version"));
        const std::string first_version = first_pkg["version"];

        auto first_version_obj = specs::Version::parse(first_version);
        REQUIRE(first_version_obj.has_value());

        // The first version should be 2.5.0 (or the highest version in our test data)
        auto expected_latest = specs::Version::parse("2.5.0");
        REQUIRE(expected_latest.has_value());
        REQUIRE(first_version_obj.value() == expected_latest.value());
    }

    SECTION("Pretty output sorted by version and build number")
    {
        // Create a database with multiple versions
        auto packages = std::vector<specs::PackageInfo>{
            mkpkg("mamba", "1.1.0", "py310h51d5547_2", 2),
            mkpkg("mamba", "2.5.0", "h9835478_0", 0),
            mkpkg("mamba", "2.4.0", "h7ae174a_1", 1),
        };

        auto db = solver::libsolv::Database(channel_context.params());
        db.add_repo_from_packages(packages, "test-repo");

        // Perform search query
        auto result = Query::find(db, { "mamba" });
        REQUIRE_FALSE(result.empty());

        // Get pretty output
        std::ostringstream oss;
        result.pretty(oss, false);

        std::string output = oss.str();

        // The output should contain the latest version first
        // Find the first occurrence of "mamba" followed by a version
        std::size_t mamba_pos = output.find("mamba");
        REQUIRE(mamba_pos != std::string::npos);

        // Extract the version that appears first in the output
        std::size_t version_start = output.find(" ", mamba_pos + 5);
        REQUIRE(version_start != std::string::npos);
        version_start++;  // Skip the space

        std::size_t version_end = output.find(" ", version_start);
        if (version_end == std::string::npos)
        {
            version_end = output.find("\n", version_start);
        }
        REQUIRE(version_end != std::string::npos);

        std::string first_version_str = output.substr(version_start, version_end - version_start);

        // Verify the first version is 2.5.0 (latest)
        auto first_version = specs::Version::parse(first_version_str);
        auto expected_latest = specs::Version::parse("2.5.0");
        REQUIRE(first_version.has_value());
        REQUIRE(expected_latest.has_value());
        REQUIRE(first_version.value() == expected_latest.value());
    }

    SECTION("Table output sorted by version and build number")
    {
        // Create a database with multiple versions
        auto packages = std::vector<specs::PackageInfo>{
            mkpkg("mamba", "1.1.0", "py310h51d5547_2", 2),
            mkpkg("mamba", "2.5.0", "h9835478_0", 0),
            mkpkg("mamba", "2.4.0", "h7ae174a_1", 1),
        };

        auto db = solver::libsolv::Database(channel_context.params());
        db.add_repo_from_packages(packages, "test-repo");

        // Perform search query
        auto result = Query::find(db, { "mamba" });
        REQUIRE_FALSE(result.empty());

        // Group by name
        result.groupby("name");

        // Get table output
        std::ostringstream oss;
        result.table(oss);

        std::string output = oss.str();

        // Parse the table output to extract versions
        // Table format: "Name Version Build Channel Subdir"
        std::vector<std::string> lines;
        std::istringstream iss(output);
        std::string line;
        while (std::getline(iss, line))
        {
            if (line.find("mamba") != std::string::npos && line.find("Version") == std::string::npos)
            {
                // This is a data line, not a header
                lines.push_back(line);
            }
        }

        REQUIRE(lines.size() >= 2);

        // Extract versions from lines
        std::vector<specs::Version> versions;
        for (const auto& data_line : lines)
        {
            std::istringstream line_stream(data_line);
            std::string name, version, build;
            line_stream >> name >> version >> build;
            if (name == "mamba")
            {
                auto version_obj = specs::Version::parse(version);
                if (version_obj.has_value())
                {
                    versions.push_back(version_obj.value());
                }
            }
        }

        REQUIRE(versions.size() >= 2);

        // Verify versions are in descending order
        for (std::size_t i = 0; i < versions.size() - 1; ++i)
        {
            REQUIRE(versions[i] >= versions[i + 1]);
        }

        // Verify first version is the latest (2.5.0)
        auto expected_latest = specs::Version::parse("2.5.0");
        REQUIRE(expected_latest.has_value());
        REQUIRE(versions[0] == expected_latest.value());
    }
}

// Regression: https://github.com/mamba-org/mamba/issues/4286
TEST_CASE(
    "Query::find python_site_packages_path respects package platform #4286",
    "[mamba::core][mamba::core::query][regression][4286]"
)
{
    auto& ctx = mambatests::context();
    auto channel_context = ChannelContext::make_conda_compatible(ctx);

    auto linux_python = mkpkg("python", "3.14.1", "h4724d56_1_cp313t", 1);
    linux_python.python_site_packages_path = "lib/python3.13t/site-packages";

    auto win_python = mkpkg("python", "3.14.1", "h7c1dbca_0_cp314t", 0);
    win_python.platform = "win-64";
    win_python.python_site_packages_path = "Lib/site-packages";
    win_python.filename = "python-3.14.1-h7c1dbca_0_cp314t.conda";
    win_python.package_url = fmt::format(
        "https://conda.anaconda.org/{}/{}/{}",
        win_python.channel,
        win_python.platform,
        win_python.filename
    );

    const std::vector<specs::PackageInfo> packages{ linux_python, win_python };
    auto db = solver::libsolv::Database(channel_context.params());
    db.add_repo_from_packages(packages, "test-repo");

    SECTION("linux-64 search keeps unix repodata path on any host")
    {
        auto result = Query::find(db, { "python=3.14.1=h4724d56_1_cp313t" });
        REQUIRE_FALSE(result.empty());

        const auto json_output = result.json();
        REQUIRE(json_output["result"]["pkgs"].size() == 1);
        REQUIRE(
            json_output["result"]["pkgs"][0]["python_site_packages_path"]
            == "lib/python3.13t/site-packages"
        );
    }

    SECTION("win-64 search keeps windows repodata path on any host")
    {
        auto result = Query::find(db, { "python=3.14.1=h7c1dbca_0_cp314t" });
        REQUIRE_FALSE(result.empty());

        const auto json_output = result.json();
        REQUIRE(json_output["result"]["pkgs"].size() == 1);
        REQUIRE(json_output["result"]["pkgs"][0]["python_site_packages_path"] == "Lib/site-packages");
    }
}

TEST_CASE("Query::depends constraint intersection", "[mamba::core][mamba::core::query]")
{
    auto& ctx = mambatests::context();
    auto channel_context = ChannelContext::make_conda_compatible(ctx);

    SECTION("Unconstrained and constrained dep resolve to single package")
    {
        auto root = mkpkg("conda", "26.5.3", "py310ha69dea2_2");
        root.dependencies = { "python", "python >=3.10,<3.11" };

        auto packages = std::vector<specs::PackageInfo>{
            root,
            mkpkg("python", "3.10.20", "hac0b6dc_1_cpython"),
            mkpkg("python", "3.14.6", "h156bc91_100_cp314"),
        };

        auto db = solver::libsolv::Database(channel_context.params());
        db.add_repo_from_packages(packages, "test-repo");

        auto result = Query::depends(db, "conda=26.5.3=py310ha69dea2_2", false);
        auto json_output = result.json();

        // Should resolve to exactly one python package (3.10.20), not two
        auto pkgs = json_output["result"]["pkgs"];
        int python_count = 0;
        std::string resolved_version;
        for (const auto& pkg : pkgs)
        {
            if (pkg.contains("name") && pkg["name"] == "python")
            {
                python_count++;
                resolved_version = pkg["version"].get<std::string>();
            }
        }
        REQUIRE(python_count == 1);
        REQUIRE(resolved_version == "3.10.20");
    }

    SECTION("Unconstrained dep with build string constraint resolves correctly")
    {
        auto root = mkpkg("conda", "26.5.3", "py312h20c3967_2");
        root.dependencies = { "python", "python 3.12.* *_cpython" };

        auto packages = std::vector<specs::PackageInfo>{
            root,
            mkpkg("python", "3.12.5", "h1234567_0_cpython"),
            mkpkg("python", "3.14.6", "habeac84_100_cp314"),
        };

        auto db = solver::libsolv::Database(channel_context.params());
        db.add_repo_from_packages(packages, "test-repo");

        auto result = Query::depends(db, "conda=26.5.3=py312h20c3967_2", false);
        auto json_output = result.json();

        // Should resolve to python 3.12.5, not 3.14.6
        auto pkgs = json_output["result"]["pkgs"];
        int python_count = 0;
        std::string resolved_version;
        for (const auto& pkg : pkgs)
        {
            if (pkg.contains("name") && pkg["name"] == "python")
            {
                python_count++;
                resolved_version = pkg["version"].get<std::string>();
            }
        }
        REQUIRE(python_count == 1);
        REQUIRE(resolved_version == "3.12.5");
    }

    SECTION("Conflicting constraints result in NOT FOUND")
    {
        auto root = mkpkg("broken-pkg", "1.0.0");
        root.dependencies = { "python >=3.10,<3.11", "python >=3.12" };

        auto packages = std::vector<specs::PackageInfo>{
            root,
            mkpkg("python", "3.10.20", "hac0b6dc_1_cpython"),
            mkpkg("python", "3.12.5", "h1234567_0"),
        };

        auto db = solver::libsolv::Database(channel_context.params());
        db.add_repo_from_packages(packages, "test-repo");


        auto result = Query::depends(db, "broken-pkg=1.0.0", false);
        auto json_output = result.json();

        // Should have NOT FOUND for python, no actual python package
        auto pkgs = json_output["result"]["pkgs"];
        bool found_python = false;
        bool found_not_found = false;
        for (const auto& pkg : pkgs)
        {
            if (pkg.contains("name") && pkg["name"] == "python")
            {
                found_python = true;
            }
            if (pkg.contains("name")
                && pkg["name"].get<std::string>().find("NOT FOUND") != std::string::npos)
            {
                found_not_found = true;
            }
        }
        REQUIRE_FALSE(found_python);
        REQUIRE(found_not_found);
    }

    SECTION("Single unconstrained dep resolves to latest")
    {
        auto root = mkpkg("my-pkg", "1.0.0");
        root.dependencies = { "numpy >=1.20", "numpy" };

        auto packages = std::vector<specs::PackageInfo>{
            root,
            mkpkg("numpy", "1.21.0", "py310h20c3968_0"),
            mkpkg("numpy", "1.24.0", "py310h20c3967_1"),
        };

        auto db = solver::libsolv::Database(channel_context.params());
        db.add_repo_from_packages(packages, "test-repo");

        auto result = Query::depends(db, "my-pkg=1.0.0", false);
        auto json_output = result.json();

        auto pkgs = json_output["result"]["pkgs"];
        int numpy_count = 0;
        std::string resolved_version;
        for (const auto& pkg : pkgs)
        {
            if (pkg.contains("name") && pkg["name"] == "numpy")
            {
                numpy_count++;
                resolved_version = pkg["version"].get<std::string>();
            }
        }
        REQUIRE(numpy_count == 1);
        REQUIRE(resolved_version == "1.24.0");
    }

    SECTION("Different packages are resolved independently")
    {
        auto root = mkpkg("my-pkg", "1.0.0");
        root.dependencies = { "python >=3.12", "numpy >=1.20" };

        auto packages = std::vector<specs::PackageInfo>{
            root,
            mkpkg("python", "3.12.5", "h1234567_0"),
            mkpkg("numpy", "1.24.0", "py312h20c3967_0"),
        };

        auto db = solver::libsolv::Database(channel_context.params());
        db.add_repo_from_packages(packages, "test-repo");

        auto result = Query::depends(db, "my-pkg=1.0.0", false);
        auto json_output = result.json();

        auto pkgs = json_output["result"]["pkgs"];
        bool found_python = false;
        bool found_numpy = false;
        for (const auto& pkg : pkgs)
        {
            if (pkg.contains("name") && pkg["name"] == "python")
            {
                found_python = true;
                REQUIRE(pkg["version"].get<std::string>() == "3.12.5");
            }
            if (pkg.contains("name") && pkg["name"] == "numpy")
            {
                found_numpy = true;
                REQUIRE(pkg["version"].get<std::string>() == "1.24.0");
            }
        }
        REQUIRE(found_python);
        REQUIRE(found_numpy);
    }
}
