// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <catch2/catch_all.hpp>

#include "mamba/solver/solution.hpp"
#include "mamba/specs/package_info.hpp"
#include "mamba/util/string.hpp"

using namespace mamba;
using namespace mamba::solver;

namespace
{
    using PackageInfo = specs::PackageInfo;

    TEST_CASE("Create a Solution")
    {
        auto solution = Solution{ {
            Solution::Omit{ PackageInfo("omit") },
            Solution::Upgrade{ PackageInfo("upgrade_remove"), PackageInfo("upgrade_install") },
            Solution::Downgrade{ PackageInfo("downgrade_remove"), PackageInfo("downgrade_install") },
            Solution::Change{ PackageInfo("change_remove"), PackageInfo("change_install") },
            Solution::Reinstall{ PackageInfo("reinstall") },
            Solution::Remove{ PackageInfo("remove") },
            Solution::Install{ PackageInfo("install") },
        } };

        SECTION("Iterate over packages")
        {
            auto remove_count = std::size_t(0);
            for_each_to_remove(
                solution.actions,
                [&](const PackageInfo& pkg)
                {
                    remove_count++;
                    const auto has_remove = util::ends_with(pkg.name, "remove")
                                            || (pkg.name == "reinstall");
                    REQUIRE(has_remove);
                }
            );
            REQUIRE(remove_count == 5);

            auto install_count = std::size_t(0);
            for_each_to_install(
                solution.actions,
                [&](const PackageInfo& pkg)
                {
                    install_count++;
                    const auto has_install = util::ends_with(pkg.name, "install")
                                             || (pkg.name == "reinstall");
                    REQUIRE(has_install);
                }
            );
            REQUIRE(install_count == 5);

            auto omit_count = std::size_t(0);
            for_each_to_omit(
                solution.actions,
                [&](const PackageInfo& pkg)
                {
                    omit_count++;
                    REQUIRE(util::ends_with(pkg.name, "omit"));
                }
            );
            REQUIRE(omit_count == 1);
        }

        SECTION("Iterate over packages and break")
        {
            auto remove_count = std::size_t(0);
            for_each_to_remove(
                solution.actions,
                [&](const PackageInfo&)
                {
                    remove_count++;
                    return util::LoopControl::Break;
                }
            );
            REQUIRE(remove_count == 1);

            auto install_count = std::size_t(0);
            for_each_to_install(
                solution.actions,
                [&](const PackageInfo&)
                {
                    install_count++;
                    return util::LoopControl::Break;
                }
            );
            REQUIRE(install_count == 1);

            auto omit_count = std::size_t(0);
            for_each_to_omit(
                solution.actions,
                [&](const PackageInfo&)
                {
                    omit_count++;
                    return util::LoopControl::Break;
                }
            );
            REQUIRE(omit_count == 1);
        }
    }
}
