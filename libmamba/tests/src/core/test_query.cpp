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
