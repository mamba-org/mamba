// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <doctest/doctest.h>

#include "mamba/solver/solution.hpp"
#include "mamba/specs/package_info.hpp"
#include "mamba/util/string.hpp"

using namespace mamba;
using namespace mamba::solver;

TEST_SUITE("solver::solution")
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

        SUBCASE("Iterate over packages")
        {
            auto remove_count = std::size_t(0);
            for_each_to_remove(
                solution.actions,
                [&](const PackageInfo& pkg)
                {
                    remove_count++;
                    const auto has_remove = util::ends_with(pkg.name, "remove")
                                            || (pkg.name == "reinstall");
                    CHECK(has_remove);
                }
            );
            CHECK_EQ(remove_count, 5);

            auto install_count = std::size_t(0);
            for_each_to_install(
                solution.actions,
                [&](const PackageInfo& pkg)
                {
                    install_count++;
                    const auto has_install = util::ends_with(pkg.name, "install")
                                             || (pkg.name == "reinstall");
                    CHECK(has_install);
                }
            );
            CHECK_EQ(install_count, 5);

            auto omit_count = std::size_t(0);
            for_each_to_omit(
                solution.actions,
                [&](const PackageInfo& pkg)
                {
                    omit_count++;
                    CHECK(util::ends_with(pkg.name, "omit"));
                }
            );
            CHECK_EQ(omit_count, 1);
        }

        SUBCASE("Iterate over packages and break")
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
            CHECK_EQ(remove_count, 1);

            auto install_count = std::size_t(0);
            for_each_to_install(
                solution.actions,
                [&](const PackageInfo&)
                {
                    install_count++;
                    return util::LoopControl::Break;
                }
            );
            CHECK_EQ(install_count, 1);

            auto omit_count = std::size_t(0);
            for_each_to_omit(
                solution.actions,
                [&](const PackageInfo&)
                {
                    omit_count++;
                    return util::LoopControl::Break;
                }
            );
            CHECK_EQ(omit_count, 1);
        }
    }
}
