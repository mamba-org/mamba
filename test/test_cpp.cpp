#include <gtest/gtest.h>

#include "match_spec.hpp"
#include "context.hpp"
#include "link.hpp"
#include "history.hpp"

namespace mamba
{
    // TEST(cpp_install, install)
    // {
    //     Context::instance().set_verbosity(3);
    //     PackageInfo pkg("wheel", "0.34.2", "py_1", 1);
    //     fs::path prefix = "C:\\Users\\wolfv\\miniconda3\\";
    //     TransactionContext tc(prefix, "3.8.x");
    //     // try {
    //         UnlinkPackage up(pkg, &tc);
    //         up.execute();
    //     // } catch (...) { std::cout << "Nothing to delete ... \n"; }
    //     LinkPackage lp(pkg, prefix / "pkgs" , &tc);
    //     lp.execute();
    // }

    TEST(match_spec, parse)
    {
        {
            MatchSpec ms("xtensor==0.12.3");   
            EXPECT_EQ(ms.version, "==0.12.3");
            EXPECT_EQ(ms.name, "xtensor");
        }
        {
            MatchSpec ms("numpy 1.7*");   
            EXPECT_EQ(ms.version, "1.7*");
            EXPECT_EQ(ms.name, "numpy");
        }
        {
            MatchSpec ms("numpy[version='1.7|1.8']");   
            // TODO!
            // EXPECT_EQ(ms.version, "1.7|1.8");
            EXPECT_EQ(ms.name, "numpy");
            EXPECT_EQ(ms.brackets["version"], "1.7|1.8");
        }
        {
            MatchSpec ms("conda-forge/linux64::xtensor==0.12.3");   
            EXPECT_EQ(ms.version, "==0.12.3");
            EXPECT_EQ(ms.name, "xtensor");
            EXPECT_EQ(ms.channel, "conda-forge/linux64");
            EXPECT_EQ(ms.optional, false);
        }
        {
            MatchSpec ms("conda-forge::foo[build=3](target=blarg,optional)");
            EXPECT_EQ(ms.version, "");
            EXPECT_EQ(ms.name, "foo");
            EXPECT_EQ(ms.channel, "conda-forge");
            EXPECT_EQ(ms.brackets["build"], "3");
            EXPECT_EQ(ms.parens["target"], "blarg");
            EXPECT_EQ(ms.optional, true);
        }
        {
            MatchSpec ms("python[build_number=3]");
            EXPECT_EQ(ms.name, "python");
            EXPECT_EQ(ms.brackets["build_number"], "3");
            EXPECT_EQ(ms.build_number, "3");
        }
        {
            MatchSpec ms("python[build_number='<=3']");
            EXPECT_EQ(ms.name, "python");
            EXPECT_EQ(ms.brackets["build_number"], "<=3");
            EXPECT_EQ(ms.build_number, "<=3");
        }
        {
            MatchSpec ms("xtensor[url=file:///home/wolfv/Downloads/xtensor-0.21.4-hc9558a2_0.tar.bz2]");
            EXPECT_EQ(ms.name, "xtensor");
            EXPECT_EQ(ms.brackets["url"], "file:///home/wolfv/Downloads/xtensor-0.21.4-hc9558a2_0.tar.bz2");
            EXPECT_EQ(ms.fn, "file:///home/wolfv/Downloads/xtensor-0.21.4-hc9558a2_0.tar.bz2");
        }
    }

    TEST(history, user_request)
    {
        auto u = History::UserRequest::prefilled();
        // update in 100 years!
        EXPECT_TRUE(u.date[0] == '2' && u.date[1] == '0');
    }

    TEST(output, no_progress_bars)
    {
        Context::instance().no_progress_bars = true;
        testing::internal::CaptureStdout();

        auto proxy = Console::instance().add_progress_bar("conda-forge");
        proxy.set_progress(50);
        proxy.set_postfix("Downloading");
        proxy.mark_as_completed("conda-forge channel downloaded");
        std::string output = testing::internal::GetCapturedStdout();
        EXPECT_TRUE(ends_with(output, "conda-forge channel downloaded\n"));
        Context::instance().no_progress_bars = false;
    }

    TEST(context, env_name)
    {
        if (on_mac || on_linux)
        {
            auto& ctx = Context::instance();
            ctx.root_prefix = "/home/user/micromamba/";
            ctx.envs_dirs = { ctx.root_prefix / "envs" };
            fs::path prefix = "/home/user/micromamba/envs/testprefix";

            EXPECT_EQ(env_name(prefix), "testprefix");
            prefix = "/home/user/micromamba/envs/a.txt";
            EXPECT_EQ(env_name(prefix), "a.txt");
            prefix = "/home/user/micromamba/envs/a.txt";
            EXPECT_EQ(env_name(prefix), "a.txt");
            prefix = "/home/user/micromamba/envs/abc/a.txt";
            EXPECT_EQ(env_name(prefix), "/home/user/micromamba/envs/abc/a.txt");
            prefix = "/home/user/env";
            EXPECT_EQ(env_name(prefix), "/home/user/env");

            EXPECT_THROW(locate_prefix_by_name("test"), std::runtime_error);
            // TODO implement tests for locate_prefix_by_name
        }
    }
}