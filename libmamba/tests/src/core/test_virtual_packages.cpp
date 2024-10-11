// Copyright (c) 2022, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <doctest/doctest.h>

#include "mamba/core/context.hpp"
#include "mamba/core/virtual_packages.hpp"
#include "mamba/specs/version.hpp"
#include "mamba/util/build.hpp"
#include "mamba/util/environment.hpp"

#include "mambatests.hpp"

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
                const auto& context = mambatests::context();
                const auto pkg = detail::make_virtual_package("test", context.platform, "0.1.5", "abcd");

                CHECK_EQ(pkg.name, "test");
                CHECK_EQ(pkg.version, "0.1.5");
                CHECK_EQ(pkg.build_string, "abcd");
                CHECK_EQ(pkg.build_number, 0);
                CHECK_EQ(pkg.channel, "@");
                CHECK_EQ(pkg.platform, context.platform);
                CHECK_EQ(pkg.md5, "12345678901234567890123456789012");
                CHECK_EQ(pkg.filename, pkg.name);
            }

            TEST_CASE("dist_packages")
            {
                using Version = specs::Version;

                auto& ctx = mambatests::context();
                auto pkgs = detail::dist_packages(ctx.platform);

                if (util::on_win)
                {
                    REQUIRE_EQ(pkgs.size(), 2);
                    CHECK_EQ(pkgs[0].name, "__win");
                    CHECK_GT(Version::parse(pkgs[0].version).value(), Version());
                }
                if (util::on_linux)
                {
                    REQUIRE_EQ(pkgs.size(), 4);
                    CHECK_EQ(pkgs[0].name, "__unix");
                    CHECK_EQ(pkgs[1].name, "__linux");
                    CHECK_GT(Version::parse(pkgs[1].version).value(), Version());
                    CHECK_EQ(pkgs[2].name, "__glibc");
                    CHECK_GT(Version::parse(pkgs[2].version).value(), Version());
                }
                if (util::on_mac)
                {
                    REQUIRE_EQ(pkgs.size(), 3);
                    CHECK_EQ(pkgs[0].name, "__unix");
                    CHECK_EQ(pkgs[1].name, "__osx");
                    CHECK_GT(Version::parse(pkgs[1].version).value(), Version());
                }
#if __x86_64__ || defined(_WIN64)
                CHECK_EQ(pkgs.back().name, "__archspec");
                CHECK_EQ(pkgs.back().build_string.find("x86_64"), 0);
#endif

                // This is bad design, tests should not interfere
                // Will get rid of that when implementing context as not a singleton
                auto restore_ctx = [&ctx, old_plat = ctx.platform]() { ctx.platform = old_plat; };
                auto finally = Finally<decltype(restore_ctx)>{ restore_ctx };

                util::set_env("CONDA_OVERRIDE_OSX", "12.1");
                pkgs = detail::dist_packages("osx-arm");
                REQUIRE_EQ(pkgs.size(), 3);
                CHECK_EQ(pkgs[0].name, "__unix");
                CHECK_EQ(pkgs[1].name, "__osx");
                CHECK_EQ(pkgs[1].version, "12.1");
                CHECK_EQ(pkgs[2].name, "__archspec");
                CHECK_EQ(pkgs[2].build_string, "arm");

                util::unset_env("CONDA_OVERRIDE_OSX");
                util::set_env("CONDA_OVERRIDE_LINUX", "5.7");
                util::set_env("CONDA_OVERRIDE_GLIBC", "2.15");
                pkgs = detail::dist_packages("linux-32");
                REQUIRE_EQ(pkgs.size(), 4);
                CHECK_EQ(pkgs[0].name, "__unix");
                CHECK_EQ(pkgs[1].name, "__linux");
                CHECK_EQ(pkgs[1].version, "5.7");
                CHECK_EQ(pkgs[2].name, "__glibc");
                CHECK_EQ(pkgs[2].version, "2.15");
                CHECK_EQ(pkgs[3].name, "__archspec");
                CHECK_EQ(pkgs[3].build_string, "x86");
                util::unset_env("CONDA_OVERRIDE_GLIBC");
                util::unset_env("CONDA_OVERRIDE_LINUX");

                pkgs = detail::dist_packages("lin-850");
                REQUIRE_EQ(pkgs.size(), 1);
                CHECK_EQ(pkgs[0].name, "__archspec");
                CHECK_EQ(pkgs[0].build_string, "850");
                util::unset_env("CONDA_SUBDIR");

                pkgs = detail::dist_packages("linux");
                REQUIRE_EQ(pkgs.size(), 0);

                ctx.platform = ctx.host_platform;
            }

            TEST_CASE("get_virtual_packages")
            {
                util::set_env("CONDA_OVERRIDE_CUDA", "9.0");
                const auto& context = mambatests::context();
                auto pkgs = get_virtual_packages(context.platform);
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

                util::unset_env("CONDA_OVERRIDE_CUDA");
                pkgs = get_virtual_packages(context.platform);

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
