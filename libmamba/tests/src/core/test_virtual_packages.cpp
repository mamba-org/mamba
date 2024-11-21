// Copyright (c) 2022, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <catch2/catch_all.hpp>

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

        namespace
        {
            TEST_CASE("make_virtual_package")
            {
                const auto& context = mambatests::context();
                const auto pkg = detail::make_virtual_package("test", context.platform, "0.1.5", "abcd");

                REQUIRE(pkg.name == "test");
                REQUIRE(pkg.version == "0.1.5");
                REQUIRE(pkg.build_string == "abcd");
                REQUIRE(pkg.build_number == 0);
                REQUIRE(pkg.channel == "@");
                REQUIRE(pkg.platform == context.platform);
                REQUIRE(pkg.md5 == "12345678901234567890123456789012");
                REQUIRE(pkg.filename == pkg.name);
            }

            TEST_CASE("dist_packages")
            {
                using Version = specs::Version;

                auto& ctx = mambatests::context();
                auto pkgs = detail::dist_packages(ctx.platform);

                if (util::on_win)
                {
                    REQUIRE(pkgs.size() == 2);
                    REQUIRE(pkgs[0].name == "__win");
                    REQUIRE(Version::parse(pkgs[0].version).value() > Version());
                }
                if (util::on_linux)
                {
                    REQUIRE(pkgs.size() == 4);
                    REQUIRE(pkgs[0].name == "__unix");
                    REQUIRE(pkgs[1].name == "__linux");
                    REQUIRE(Version::parse(pkgs[1].version).value() > Version());
                    REQUIRE(pkgs[2].name == "__glibc");
                    REQUIRE(Version::parse(pkgs[2].version).value() > Version());
                }
                if (util::on_mac)
                {
                    REQUIRE(pkgs.size() == 3);
                    REQUIRE(pkgs[0].name == "__unix");
                    REQUIRE(pkgs[1].name == "__osx");
                    REQUIRE(Version::parse(pkgs[1].version).value() > Version());
                }
#if __x86_64__ || defined(_WIN64)
                REQUIRE(pkgs.back().name == "__archspec");
                REQUIRE(pkgs.back().build_string.find("x86_64") == 0);
#endif

                // This is bad design, tests should not interfere
                // Will get rid of that when implementing context as not a singleton
                auto restore_ctx = [&ctx, old_plat = ctx.platform]() { ctx.platform = old_plat; };
                auto finally = Finally<decltype(restore_ctx)>{ restore_ctx };

                util::set_env("CONDA_OVERRIDE_OSX", "12.1");
                pkgs = detail::dist_packages("osx-arm");
                REQUIRE(pkgs.size() == 3);
                REQUIRE(pkgs[0].name == "__unix");
                REQUIRE(pkgs[1].name == "__osx");
                REQUIRE(pkgs[1].version == "12.1");
                REQUIRE(pkgs[2].name == "__archspec");
                REQUIRE(pkgs[2].build_string == "arm");

                util::unset_env("CONDA_OVERRIDE_OSX");
                util::set_env("CONDA_OVERRIDE_LINUX", "5.7");
                util::set_env("CONDA_OVERRIDE_GLIBC", "2.15");
                pkgs = detail::dist_packages("linux-32");
                REQUIRE(pkgs.size() == 4);
                REQUIRE(pkgs[0].name == "__unix");
                REQUIRE(pkgs[1].name == "__linux");
                REQUIRE(pkgs[1].version == "5.7");
                REQUIRE(pkgs[2].name == "__glibc");
                REQUIRE(pkgs[2].version == "2.15");
                REQUIRE(pkgs[3].name == "__archspec");
                REQUIRE(pkgs[3].build_string == "x86");
                util::unset_env("CONDA_OVERRIDE_GLIBC");
                util::unset_env("CONDA_OVERRIDE_LINUX");

                pkgs = detail::dist_packages("lin-850");
                REQUIRE(pkgs.size() == 1);
                REQUIRE(pkgs[0].name == "__archspec");
                REQUIRE(pkgs[0].build_string == "850");
                util::unset_env("CONDA_SUBDIR");

                pkgs = detail::dist_packages("linux");
                REQUIRE(pkgs.size() == 0);

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
                REQUIRE(pkgs.size() == pkgs_count);
                REQUIRE(pkgs.back().name == "__cuda");
                REQUIRE(pkgs.back().version == "9.0");

                util::unset_env("CONDA_OVERRIDE_CUDA");
                pkgs = get_virtual_packages(context.platform);

                if (!detail::cuda_version().empty())
                {
                    REQUIRE(pkgs.size() == pkgs_count);
                }
                else
                {
                    REQUIRE(pkgs.size() == pkgs_count - 1);
                }
            }
        }
    }
}
