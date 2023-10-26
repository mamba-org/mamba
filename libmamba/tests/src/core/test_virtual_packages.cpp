// Copyright (c) 2022, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <iostream>

#include <doctest/doctest.h>

#include "mamba/core/context.hpp"
#include "mamba/core/environment.hpp"
#include "mamba/core/virtual_packages.hpp"
#include "mamba/util/build.hpp"

namespace mamba
{
    namespace testing
    {

        template <typename Callback>
        struct Finally
        {
            Callback callback;

            ~Finally()
            {
                callback();
            }
        };

        TEST_SUITE("virtual_packages")
        {
            TEST_CASE("make_virtual_package")
            {
                auto pkg = detail::make_virtual_package("test", "0.1.5", "abcd");

                CHECK_EQ(pkg.name, "test");
                CHECK_EQ(pkg.version, "0.1.5");
                CHECK_EQ(pkg.build_string, "abcd");
                CHECK_EQ(pkg.build_number, 0);
                CHECK_EQ(pkg.channel, "@");
                // CHECK_EQ(pkg.subdir, "osx-64");  // TODO: fix this
                CHECK_EQ(pkg.md5, "12345678901234567890123456789012");
                CHECK_EQ(pkg.fn, pkg.name);
            }

            TEST_CASE("dist_packages")
            {
                auto pkgs = detail::dist_packages();
                auto& ctx = Context::instance();

                if (util::on_win)
                {
                    REQUIRE_EQ(pkgs.size(), 2);
                    CHECK_EQ(pkgs[0].name, "__win");
                }
                if (util::on_linux)
                {
                    REQUIRE_EQ(pkgs.size(), 4);
                    CHECK_EQ(pkgs[0].name, "__unix");
                    CHECK_EQ(pkgs[1].name, "__linux");
                    CHECK_EQ(pkgs[2].name, "__glibc");
                }
                if (util::on_mac)
                {
                    REQUIRE_EQ(pkgs.size(), 3);
                    CHECK_EQ(pkgs[0].name, "__unix");
                    CHECK_EQ(pkgs[1].name, "__osx");
                }
#if __x86_64__ || defined(_WIN64)
                CHECK_EQ(pkgs.back().name, "__archspec");
                CHECK_EQ(pkgs.back().build_string.find("x86_64"), 0);
#endif

                // This is bad design, tests should not interfer
                // Will get rid of that when implementing context as not a singleton
                auto restore_ctx = [&ctx, old_plat = ctx.platform]() { ctx.platform = old_plat; };
                auto finally = Finally<decltype(restore_ctx)>{ restore_ctx };

                ctx.platform = "osx-arm";
                env::set("CONDA_OVERRIDE_OSX", "12.1");
                pkgs = detail::dist_packages();
                REQUIRE_EQ(pkgs.size(), 3);
                CHECK_EQ(pkgs[0].name, "__unix");
                CHECK_EQ(pkgs[1].name, "__osx");
                CHECK_EQ(pkgs[1].version, "12.1");
                CHECK_EQ(pkgs[2].name, "__archspec");
                CHECK_EQ(pkgs[2].build_string, "arm");

                env::unset("CONDA_OVERRIDE_OSX");
                ctx.platform = "linux-32";
                env::set("CONDA_OVERRIDE_LINUX", "5.7");
                env::set("CONDA_OVERRIDE_GLIBC", "2.15");
                pkgs = detail::dist_packages();
                REQUIRE_EQ(pkgs.size(), 4);
                CHECK_EQ(pkgs[0].name, "__unix");
                CHECK_EQ(pkgs[1].name, "__linux");
                CHECK_EQ(pkgs[1].version, "5.7");
                CHECK_EQ(pkgs[2].name, "__glibc");
                CHECK_EQ(pkgs[2].version, "2.15");
                CHECK_EQ(pkgs[3].name, "__archspec");
                CHECK_EQ(pkgs[3].build_string, "x86");
                env::unset("CONDA_OVERRIDE_GLIBC");
                env::unset("CONDA_OVERRIDE_LINUX");

                ctx.platform = "lin-850";
                pkgs = detail::dist_packages();
                REQUIRE_EQ(pkgs.size(), 1);
                CHECK_EQ(pkgs[0].name, "__archspec");
                CHECK_EQ(pkgs[0].build_string, "850");
                env::unset("CONDA_SUBDIR");

                ctx.platform = "linux";
                pkgs = detail::dist_packages();
                REQUIRE_EQ(pkgs.size(), 0);

                ctx.platform = ctx.host_platform;
            }

            TEST_CASE("get_virtual_packages")
            {
                env::set("CONDA_OVERRIDE_CUDA", "9.0");

                auto pkgs = get_virtual_packages();
                int pkgs_count;

                if (util::on_win)
                {
                    pkgs_count = 2;
                }
                if (util::on_linux)
                {
                    pkgs_count = 4;
                }
                if (util::on_mac)
                {
                    pkgs_count = 3;
                }

                ++pkgs_count;
                REQUIRE_EQ(pkgs.size(), pkgs_count);
                CHECK_EQ(pkgs.back().name, "__cuda");
                CHECK_EQ(pkgs.back().version, "9.0");

                env::unset("CONDA_OVERRIDE_CUDA");
                pkgs = get_virtual_packages();

                if (!detail::cuda_version().empty())
                {
                    REQUIRE_EQ(pkgs.size(), pkgs_count);
                }
                else
                {
                    REQUIRE_EQ(pkgs.size(), pkgs_count - 1);
                }
            }
        }
    }
}
