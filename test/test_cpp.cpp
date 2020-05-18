#include <gtest/gtest.h>

#include "match_spec.hpp"
#include "context.hpp"
#include "link.hpp"
#include "history.hpp"
#include "fsutil.hpp"

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

    TEST(match_spec, parse_version_build)
    {
        std::string v, b;
        // >>> _parse_version_plus_build("=1.2.3 0")
        // ('=1.2.3', '0')
        // >>> _parse_version_plus_build("1.2.3=0")
        // ('1.2.3', '0')
        // >>> _parse_version_plus_build(">=1.0 , < 2.0 py34_0")
        // ('>=1.0,<2.0', 'py34_0')
        // >>> _parse_version_plus_build(">=1.0 , < 2.0 =py34_0")
        // ('>=1.0,<2.0', 'py34_0')
        // >>> _parse_version_plus_build("=1.2.3 ")
        // ('=1.2.3', None)
        // >>> _parse_version_plus_build(">1.8,<2|==1.7")
        // ('>1.8,<2|==1.7', None)
        // >>> _parse_version_plus_build("* openblas_0")
        // ('*', 'openblas_0')
        // >>> _parse_version_plus_build("* *")
        // ('*', '*')
        std::tie(v, b) = MatchSpec::parse_version_and_build("=1.2.3 0");
        EXPECT_EQ(v, "=1.2.3");
        EXPECT_EQ(b, "0");
        std::tie(v, b) = MatchSpec::parse_version_and_build("=1.2.3=0");
        EXPECT_EQ(v, "=1.2.3");
        EXPECT_EQ(b, "0");
        std::tie(v, b) = MatchSpec::parse_version_and_build(">=1.0 , < 2.0 py34_0");
        EXPECT_EQ(v, ">=1.0,<2.0");
        EXPECT_EQ(b, "py34_0");
        std::tie(v, b) = MatchSpec::parse_version_and_build(">=1.0 , < 2.0 =py34_0");
        EXPECT_EQ(v, ">=1.0,<2.0");
        EXPECT_EQ(b, "py34_0");
        std::tie(v, b) = MatchSpec::parse_version_and_build("=1.2.3 ");
        EXPECT_EQ(v, "=1.2.3");
        EXPECT_EQ(b, "");
        std::tie(v, b) = MatchSpec::parse_version_and_build(">1.8,<2|==1.7");
        EXPECT_EQ(v, ">1.8,<2|==1.7");
        EXPECT_EQ(b, "");
        std::tie(v, b) = MatchSpec::parse_version_and_build("* openblas_0");
        EXPECT_EQ(v, "*");
        EXPECT_EQ(b, "openblas_0");
        std::tie(v, b) = MatchSpec::parse_version_and_build("* *");
        EXPECT_EQ(v, "*");
        EXPECT_EQ(b, "*");
    }

    TEST(match_spec, parse)
    {
        {
            MatchSpec ms("xtensor==0.12.3");
            EXPECT_EQ(ms.version, "0.12.3");
            EXPECT_EQ(ms.name, "xtensor");
        }
        {
            MatchSpec ms("ipykernel");
            EXPECT_EQ(ms.version, "");
            EXPECT_EQ(ms.name, "ipykernel");
        }
        {
            MatchSpec ms("ipykernel ");
            EXPECT_EQ(ms.version, "");
            EXPECT_EQ(ms.name, "ipykernel");
        }
        {
            MatchSpec ms("numpy 1.7*");   
            EXPECT_EQ(ms.version, "1.7*");
            EXPECT_EQ(ms.name, "numpy");
            EXPECT_EQ(ms.conda_build_form(), "numpy 1.7*");
            EXPECT_EQ(ms.str(), "numpy=1.7");
        }
        {
            MatchSpec ms("numpy[version='1.7|1.8']");   
            // TODO!
            // EXPECT_EQ(ms.version, "1.7|1.8");
            EXPECT_EQ(ms.name, "numpy");
            EXPECT_EQ(ms.brackets["version"], "1.7|1.8");
            EXPECT_EQ(ms.str(), "numpy[version='1.7|1.8']");
        }
        {
            MatchSpec ms("conda-forge/linux64::xtensor==0.12.3");   
            EXPECT_EQ(ms.version, "0.12.3");
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
        {
            MatchSpec ms("foo=1.0=2");
            EXPECT_EQ(ms.conda_build_form(), "foo 1.0 2");
            EXPECT_EQ(ms.str(), "foo==1.0=2");
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

    TEST(fsutil, starts_with_home)
    {
        if (on_linux)
        {
            auto home = env::expand_user("~");
            EXPECT_EQ(path::starts_with_home(home / "test" / "file.txt" ), true);
            EXPECT_EQ(path::starts_with_home("~"), true);
            EXPECT_EQ(path::starts_with_home("/opt/bin"), false);
        }
    }

    TEST(fsutil, expand_user)
    {
        fs::path pbefore = "/tmp/test/xyz.txt";
        fs::path p = env::expand_user(pbefore);
        EXPECT_EQ(p, pbefore);
    }

    TEST(fsutil, touch)
    {
        if (on_linux)
        {
            path::touch("/tmp/dir/file.txt", true);
            EXPECT_TRUE(fs::exists("/tmp/dir/file.txt"));
        }
    }

    TEST(fsutil, is_writable)
    {
        if (on_linux)
        {
            EXPECT_TRUE(path::is_writable("/tmp/test.txt"));
            EXPECT_TRUE(path::is_writable(env::expand_user("~/hello.txt")));
            // if (env::is_admin())
            // {
            //     EXPECT_TRUE(path::is_writable("/opt/test.txt"));
            // }
            // else
            // {
            //     EXPECT_FALSE(path::is_writable("/opt/test.txt"));
            // }
            EXPECT_THROW(path::is_writable("/tmp/this/path/doesnt/exist"), std::runtime_error);
        }
    }
}