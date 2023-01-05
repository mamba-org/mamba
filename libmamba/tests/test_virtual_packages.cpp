#include <gtest/gtest.h>

#include "mamba/core/context.hpp"
#include "mamba/core/environment.hpp"
#include "mamba/core/util.hpp"
#include "mamba/core/virtual_packages.hpp"


namespace mamba
{
    namespace testing
    {
        TEST(virtual_packages, make_virtual_package)
        {
            auto pkg = make_virtual_package("test", "0.1.5", "abcd");

            EXPECT_EQ(pkg.name, "test");
            EXPECT_EQ(pkg.version, "0.1.5");
            EXPECT_EQ(pkg.build_string, "abcd");
            EXPECT_EQ(pkg.build_number, 0);
            EXPECT_EQ(pkg.channel, "@");
            // EXPECT_EQ(pkg.subdir, "osx-64");  // TODO: fix this
            EXPECT_EQ(pkg.md5, "12345678901234567890123456789012");
            EXPECT_EQ(pkg.fn, pkg.name);
        }

        TEST(virtual_packages, dist_packages)
        {
            auto pkgs = detail::dist_packages();
            auto& ctx = Context::instance();

            if (on_win)
            {
                ASSERT_EQ(pkgs.size(), 2);
                EXPECT_EQ(pkgs[0].name, "__win");
            }
            if (on_linux)
            {
                ASSERT_EQ(pkgs.size(), 4);
                EXPECT_EQ(pkgs[0].name, "__unix");
                EXPECT_EQ(pkgs[1].name, "__linux");
                EXPECT_EQ(pkgs[2].name, "__glibc");
            }
            if (on_mac)
            {
                ASSERT_EQ(pkgs.size(), 3);
                EXPECT_EQ(pkgs[0].name, "__unix");
                EXPECT_EQ(pkgs[1].name, "__osx");
            }
#if __x86_64__ || defined(_WIN64)
            EXPECT_EQ(pkgs.back().name, "__archspec");
            EXPECT_EQ(pkgs.back().build_string, "x86_64");
#endif

            ctx.platform = "osx-arm";
            env::set("CONDA_OVERRIDE_OSX", "12.1");
            pkgs = detail::dist_packages();
            ASSERT_EQ(pkgs.size(), 3);
            EXPECT_EQ(pkgs[0].name, "__unix");
            EXPECT_EQ(pkgs[1].name, "__osx");
            EXPECT_EQ(pkgs[1].version, "12.1");
            EXPECT_EQ(pkgs[2].name, "__archspec");
            EXPECT_EQ(pkgs[2].build_string, "arm");

            env::unset("CONDA_OVERRIDE_OSX");
            ctx.platform = "linux-32";
            env::set("CONDA_OVERRIDE_LINUX", "5.7");
            env::set("CONDA_OVERRIDE_GLIBC", "2.15");
            pkgs = detail::dist_packages();
            ASSERT_EQ(pkgs.size(), 4);
            EXPECT_EQ(pkgs[0].name, "__unix");
            EXPECT_EQ(pkgs[1].name, "__linux");
            EXPECT_EQ(pkgs[1].version, "5.7");
            EXPECT_EQ(pkgs[2].name, "__glibc");
            EXPECT_EQ(pkgs[2].version, "2.15");
            EXPECT_EQ(pkgs[3].name, "__archspec");
            EXPECT_EQ(pkgs[3].build_string, "x86");
            env::unset("CONDA_OVERRIDE_GLIBC");
            env::unset("CONDA_OVERRIDE_LINUX");

            ctx.platform = "lin-850";
            pkgs = detail::dist_packages();
            ASSERT_EQ(pkgs.size(), 1);
            EXPECT_EQ(pkgs[0].name, "__archspec");
            EXPECT_EQ(pkgs[0].build_string, "850");
            env::unset("CONDA_SUBDIR");

            ctx.platform = "linux";
            pkgs = detail::dist_packages();
            ASSERT_EQ(pkgs.size(), 0);

            ctx.platform = ctx.host_platform;
        }

        TEST(virtual_packages, get_virtual_packages)
        {
            env::set("CONDA_OVERRIDE_CUDA", "9.0");

            auto pkgs = get_virtual_packages();
            int pkgs_count;

            if (on_win)
            {
                pkgs_count = 2;
            }
            if (on_linux)
            {
                pkgs_count = 4;
            }
            if (on_mac)
            {
                pkgs_count = 3;
            }

            ++pkgs_count;
            ASSERT_EQ(pkgs.size(), pkgs_count);
            EXPECT_EQ(pkgs.back().name, "__cuda");
            EXPECT_EQ(pkgs.back().version, "9.0");

            env::unset("CONDA_OVERRIDE_CUDA");
            pkgs = get_virtual_packages();

            if (!detail::cuda_version().empty())
            {
                ASSERT_EQ(pkgs.size(), pkgs_count);
            }
            else
            {
                ASSERT_EQ(pkgs.size(), pkgs_count - 1);
            }
        }
    }
}
