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

    TEST_CASE("Create a Solution", "[mamba::solver]")
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

        SECTION("Const iterate over packages")
        {
            SECTION("Packages to remove")
            {
                auto remove_count = std::size_t(0);
                for (const PackageInfo& pkg : solution.packages_to_remove())
                {
                    remove_count++;
                    const auto has_remove = util::ends_with(pkg.name, "remove")
                                            || (pkg.name == "reinstall");
                    REQUIRE(has_remove);
                }
                REQUIRE(remove_count == 5);
            }

            SECTION("Packages to install")
            {
                auto install_count = std::size_t(0);
                for (const PackageInfo& pkg : solution.packages_to_install())
                {
                    install_count++;
                    const auto has_install = util::ends_with(pkg.name, "install")
                                             || (pkg.name == "reinstall");
                    REQUIRE(has_install);
                }
                REQUIRE(install_count == 5);
            }

            SECTION("Packages to omit")
            {
                auto omit_count = std::size_t(0);
                for (const PackageInfo& pkg : solution.packages_to_omit())
                {
                    omit_count++;
                    REQUIRE(util::ends_with(pkg.name, "omit"));
                }
                REQUIRE(omit_count == 1);
            }
        }

        SECTION("Ref iterate over packages")
        {
            SECTION("Packages to remove")
            {
                for (PackageInfo& pkg : solution.packages_to_remove())
                {
                    pkg.name = "";
                }
                for (const PackageInfo& pkg : solution.packages_to_remove())
                {
                    CHECK(pkg.name == "");
                }
            }

            SECTION("Packages to install")
            {
                for (PackageInfo& pkg : solution.packages_to_install())
                {
                    pkg.name = "";
                }
                for (const PackageInfo& pkg : solution.packages_to_install())
                {
                    CHECK(pkg.name == "");
                }
            }

            SECTION("Packages to omit")
            {
                for (PackageInfo& pkg : solution.packages_to_omit())
                {
                    pkg.name = "";
                }
                for (const PackageInfo& pkg : solution.packages_to_omit())
                {
                    CHECK(pkg.name == "");
                }
            }
        }
    }
}
