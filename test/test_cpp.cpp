#include <gtest/gtest.h>

#include <sstream>
#include <tuple>

#include "mamba/core/context.hpp"
#include "mamba/core/fsutil.hpp"
#include "mamba/core/history.hpp"
#include "mamba/core/link.hpp"
#include "mamba/core/match_spec.hpp"

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
            MatchSpec ms(
                "https://conda.anaconda.org/conda-forge/linux-64/_libgcc_mutex-0.1-conda_forge.tar.bz2");
            EXPECT_EQ(ms.name, "_libgcc_mutex");
            EXPECT_EQ(ms.version, "0.1");
            EXPECT_EQ(ms.build, "conda_forge");
            EXPECT_EQ(
                ms.url,
                "https://conda.anaconda.org/conda-forge/linux-64/_libgcc_mutex-0.1-conda_forge.tar.bz2");
            EXPECT_EQ(ms.fn, "_libgcc_mutex-0.1-conda_forge.tar.bz2");
        }
        {
            MatchSpec ms(
                "/home/randomguy/Downloads/linux-64/_libgcc_mutex-0.1-conda_forge.tar.bz2");
            EXPECT_EQ(ms.name, "_libgcc_mutex");
            EXPECT_EQ(ms.version, "0.1");
            EXPECT_EQ(ms.build, "conda_forge");
#ifdef _WIN32
            std::string driveletter = fs::absolute(fs::path("/")).string().substr(0, 1);
            EXPECT_EQ(
                ms.url,
                std::string("file://") + driveletter
                    + ":/home/randomguy/Downloads/linux-64/_libgcc_mutex-0.1-conda_forge.tar.bz2");
#else
            EXPECT_EQ(
                ms.url,
                "file:///home/randomguy/Downloads/linux-64/_libgcc_mutex-0.1-conda_forge.tar.bz2");
#endif
            EXPECT_EQ(ms.fn, "_libgcc_mutex-0.1-conda_forge.tar.bz2");
        }
        {
            MatchSpec ms("xtensor[url=file:///home/wolfv/Downloads/"
                         "xtensor-0.21.4-hc9558a2_0.tar.bz2]");
            EXPECT_EQ(ms.name, "xtensor");
            EXPECT_EQ(ms.brackets["url"],
                      "file:///home/wolfv/Downloads/xtensor-0.21.4-hc9558a2_0.tar.bz2");
            EXPECT_EQ(ms.url, "file:///home/wolfv/Downloads/xtensor-0.21.4-hc9558a2_0.tar.bz2");
        }
        {
            MatchSpec ms("foo=1.0=2");
            EXPECT_EQ(ms.conda_build_form(), "foo 1.0 2");
            EXPECT_EQ(ms.str(), "foo==1.0=2");
        }
        {
            MatchSpec ms("foo=1.0=2[md5=123123123, license=BSD-3, fn='test 123.tar.bz2']");
            EXPECT_EQ(ms.conda_build_form(), "foo 1.0 2");
            EXPECT_EQ(ms.str(), "foo==1.0=2[md5=123123123,license=BSD-3,fn='test 123.tar.bz2']");
        }
        {
            MatchSpec ms(
                "foo=1.0=2[md5=123123123, license=BSD-3, fn='test 123.tar.bz2', url='abcdef']");
            EXPECT_EQ(ms.conda_build_form(), "foo 1.0 2");
            EXPECT_EQ(ms.str(), "foo==1.0=2[url=abcdef,md5=123123123,license=BSD-3]");
        }
        {
            MatchSpec ms("libblas=*=*mkl");
            EXPECT_EQ(ms.conda_build_form(), "libblas * *mkl");
            // EXPECT_EQ(ms.str(), "foo==1.0=2");
        }
        {
            MatchSpec ms("libblas=0.15*");
            EXPECT_EQ(ms.conda_build_form(), "libblas 0.15*");
        }
        {
            MatchSpec ms("xtensor =0.15*");
            EXPECT_EQ(ms.conda_build_form(), "xtensor 0.15*");
            EXPECT_EQ(ms.str(), "xtensor=0.15");
        }
    }

    TEST(match_spec, is_simple)
    {
        {
            MatchSpec ms("libblas");
            EXPECT_TRUE(ms.is_simple());
        }
        {
            MatchSpec ms("libblas=12.9=abcdef");
            EXPECT_FALSE(ms.is_simple());
        }
        {
            MatchSpec ms("libblas=0.15*");
            EXPECT_FALSE(ms.is_simple());
        }
        {
            MatchSpec ms("libblas[version=12.2]");
            EXPECT_FALSE(ms.is_simple());
        }
        {
            MatchSpec ms("xtensor =0.15*");
            EXPECT_FALSE(ms.is_simple());
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
        proxy.set_progress(50, 100);
        proxy.set_postfix("Downloading");
        proxy.mark_as_completed("conda-forge channel downloaded");
        std::string output = testing::internal::GetCapturedStdout();
        EXPECT_TRUE(ends_with(output, "conda-forge channel downloaded\n"));
        Context::instance().no_progress_bars = false;
    }

    class OutputPromptTests : public testing::TestWithParam<std::tuple<std::string, char, bool>>
    {
    };

    TEST_P(OutputPromptTests, prompt)
    {
        auto params = GetParam();

        std::stringstream test_stream;
        test_stream << std::get<0>(params) << std::endl;
        EXPECT_EQ(Console::instance().prompt("Test prompt", std::get<1>(params), test_stream),
                  std::get<2>(params));
    }

    INSTANTIATE_TEST_CASE_P(output,
                            OutputPromptTests,
                            testing::Values(std::make_tuple("y", 'y', true),
                                            std::make_tuple("yes", 'y', true),
                                            std::make_tuple("Y", 'y', true),
                                            std::make_tuple("Yes", 'y', true),
                                            std::make_tuple("", 'y', true),
                                            std::make_tuple("n", 'y', false),
                                            std::make_tuple("no", 'y', false),
                                            std::make_tuple("N", 'y', false),
                                            std::make_tuple("No", 'y', false),

                                            std::make_tuple("y", 'n', true),
                                            std::make_tuple("yes", 'n', true),
                                            std::make_tuple("Y", 'n', true),
                                            std::make_tuple("Yes", 'n', true),
                                            std::make_tuple("", 'n', false),
                                            std::make_tuple("n", 'n', false),
                                            std::make_tuple("no", 'n', false),
                                            std::make_tuple("N", 'n', false),
                                            std::make_tuple("No", 'n', false)));

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
            EXPECT_EQ(path::starts_with_home(home / "test" / "file.txt"), true);
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

    TEST(link, replace_long_shebang)
    {
        if (!on_win)
        {
            std::string res = replace_long_shebang(
                "#!/this/is/loooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooong/python -o test -x");
            EXPECT_EQ(res, "#!/usr/bin/env python -o test -x");
            res = replace_long_shebang(
                "#!/this/is/loooooooooooooooooooooooooooooooooooooooooooooooooooo oooooo oooooo oooooooooooooooooooooooooooooooooooong/python -o test -x");
            EXPECT_EQ(res, "#!/usr/bin/env python -o test -x");
            res = replace_long_shebang(
                "#!/this/is/loooooooooooooooooooooooooooooooooooooooooooooooooooo oooooo oooooo oooooooooooooooooooooooooooooooooooong/pyt hon -o test -x");
            EXPECT_EQ(res, "#!/usr/bin/env pyt hon -o test -x");
            res = replace_long_shebang(
                "#!/this/is/loooooooooooooooooooooooooooooooooooooooooooooooooooo oooooo oooooo oooooooooooooooooooooooooooooooooooong/pyt\\ hon -o test -x");
            EXPECT_EQ(res, "#!/usr/bin/env pyt\\ hon -o test -x");
            res = replace_long_shebang(
                "#! /this/is/loooooooooooooooooooooooooooooooooooooooooooooooooooo oooooo oooooo oooooooooooooooooooooooooooooooooooong/pyt\\ hon -o test -x");
            EXPECT_EQ(res, "#!/usr/bin/env pyt\\ hon -o test -x");
            res = replace_long_shebang(
                "#!    /this/is/looooooooooooooooooooooooooooooooooooooooooooo  ooooooo oooooo oooooo ooooooooooooooooo ooooooooooooooooooong/pyt\\ hon -o \"te  st\" -x");
            EXPECT_EQ(res, "#!/usr/bin/env pyt\\ hon -o \"te  st\" -x");
        }
    }

    TEST(utils, quote_for_shell)
    {
        if (!on_win)
        {
            std::vector<std::string> args1 = { "python", "-c", "print('is great')" };
            EXPECT_EQ(quote_for_shell(args1), "python -c \"print('is great')\"");
            std::vector<std::string> args2 = { "python", "-c", "print(\"is great\")" };
            EXPECT_EQ(quote_for_shell(args2), "python -c 'print(\"is great\")'");
            std::vector<std::string> args3 = { "python", "very nice", "print(\"is great\")" };
            EXPECT_EQ(quote_for_shell(args3), "python \"very nice\" 'print(\"is great\")'");
            std::vector<std::string> args4 = { "pyt \t tab", "very nice", "print(\"is great\")" };
            EXPECT_EQ(quote_for_shell(args4), "\"pyt \t tab\" \"very nice\" 'print(\"is great\")'");
        }

        std::vector<std::string> args1 = { "a b c", "d", "e" };
        EXPECT_EQ(quote_for_shell(args1, "cmdexe"), "\"a b c\" d e");
        std::vector<std::string> args2 = { "ab\"c", "\\", "d" };
        EXPECT_EQ(quote_for_shell(args2, "cmdexe"), "ab\\\"c \\ d");
        std::vector<std::string> args3 = { "ab\"c", " \\", "d" };
        EXPECT_EQ(quote_for_shell(args3, "cmdexe"), "ab\\\"c \" \\\\\" d");
        std::vector<std::string> args4 = { "a\\\\\\b", "de fg", "h" };
        EXPECT_EQ(quote_for_shell(args4, "cmdexe"), "a\\\\\\b \"de fg\" h");
        std::vector<std::string> args5 = { "a\\\"b", "c", "d" };
        EXPECT_EQ(quote_for_shell(args5, "cmdexe"), "a\\\\\\\"b c d");
        std::vector<std::string> args6 = { "a\\\\b c", "d", "e" };
        EXPECT_EQ(quote_for_shell(args6, "cmdexe"), "\"a\\\\b c\" d e");
        std::vector<std::string> args7 = { "a\\\\b\\ c", "d", "e" };
        EXPECT_EQ(quote_for_shell(args7, "cmdexe"), "\"a\\\\b\\ c\" d e");
        std::vector<std::string> args8 = { "ab", "" };
        EXPECT_EQ(quote_for_shell(args8, "cmdexe"), "ab \"\"");
    }

    TEST(utils, strip)
    {
        {
            std::string x(strip("   testwhitespacestrip  "));
            EXPECT_EQ(x, "testwhitespacestrip");
            std::string y(rstrip("   testwhitespacestrip  "));
            EXPECT_EQ(y, "   testwhitespacestrip");
            std::string z(lstrip("   testwhitespacestrip  "));
            EXPECT_EQ(z, "testwhitespacestrip  ");
        }
        {
            std::string x(strip("    "));
            EXPECT_EQ(x, "");
            std::string y(rstrip("    "));
            EXPECT_EQ(y, "");
            std::string z(lstrip("    "));
            EXPECT_EQ(z, "");
        }
        {
            std::string x(strip("a"));
            EXPECT_EQ(x, "a");
            std::string y(rstrip("a"));
            EXPECT_EQ(y, "a");
            std::string z(lstrip("a"));
            EXPECT_EQ(z, "a");
        }
        {
            std::string x(strip("  a   "));
            EXPECT_EQ(x, "a");
            std::string y(rstrip(" a  "));
            EXPECT_EQ(y, " a");
            std::string z(lstrip("  a   "));
            EXPECT_EQ(z, "a   ");
        }
        {
            std::string x(strip("abc"));
            EXPECT_EQ(x, "abc");
            std::string y(rstrip("abc"));
            EXPECT_EQ(y, "abc");
            std::string z(lstrip("abc"));
            EXPECT_EQ(z, "abc");
        }
        {
            std::string x(strip(" \r \t  \n   "));
            EXPECT_EQ(x, "");
            std::string y(rstrip("  \r \t  \n  "));
            EXPECT_EQ(y, "");
            std::string z(lstrip("   \r \t  \n "));
            EXPECT_EQ(z, "");
        }
        {
            std::string x(strip("\r \t  \n testwhitespacestrip  \r \t  \n"));
            EXPECT_EQ(x, "testwhitespacestrip");
            std::string y(rstrip("  \r \t  \n testwhitespacestrip  \r \t  \n"));
            EXPECT_EQ(y, "  \r \t  \n testwhitespacestrip");
            std::string z(lstrip("  \r \t  \n testwhitespacestrip \r \t  \n "));
            EXPECT_EQ(z, "testwhitespacestrip \r \t  \n ");
        }
    }
}  // namespace mamba
