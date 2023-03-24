// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <chrono>
#include <sstream>
#include <tuple>

#include <gtest/gtest.h>

#include "mamba/core/context.hpp"
#include "mamba/core/environment.hpp"
#include "mamba/core/fsutil.hpp"
#include "mamba/core/history.hpp"
#include "mamba/core/link.hpp"
#include "mamba/core/match_spec.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/subdirdata.hpp"

#include "test_data.hpp"

namespace mamba
{
    // TEST(cpp_install, install)
    // {
    //     Context::instance().verbosity = 3;
    //     PackageInfo pkg("wheel", "0.34.2", "py_1", 1);
    //     fs::u8path prefix = "C:\\Users\\wolfv\\miniconda3\\";
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
                "https://conda.anaconda.org/conda-forge/linux-64/_libgcc_mutex-0.1-conda_forge.tar.bz2"
            );
            EXPECT_EQ(ms.name, "_libgcc_mutex");
            EXPECT_EQ(ms.version, "0.1");
            EXPECT_EQ(ms.build_string, "conda_forge");
            EXPECT_EQ(
                ms.url,
                "https://conda.anaconda.org/conda-forge/linux-64/_libgcc_mutex-0.1-conda_forge.tar.bz2"
            );
            EXPECT_EQ(ms.fn, "_libgcc_mutex-0.1-conda_forge.tar.bz2");
        }
        {
            MatchSpec ms("/home/randomguy/Downloads/linux-64/_libgcc_mutex-0.1-conda_forge.tar.bz2");
            EXPECT_EQ(ms.name, "_libgcc_mutex");
            EXPECT_EQ(ms.version, "0.1");
            EXPECT_EQ(ms.build_string, "conda_forge");
#ifdef _WIN32
            std::string driveletter = fs::absolute(fs::u8path("/")).string().substr(0, 1);
            EXPECT_EQ(
                ms.url,
                std::string("file://") + driveletter
                    + ":/home/randomguy/Downloads/linux-64/_libgcc_mutex-0.1-conda_forge.tar.bz2"
            );
#else
            EXPECT_EQ(
                ms.url,
                "file:///home/randomguy/Downloads/linux-64/_libgcc_mutex-0.1-conda_forge.tar.bz2"
            );
#endif
            EXPECT_EQ(ms.fn, "_libgcc_mutex-0.1-conda_forge.tar.bz2");
        }
        {
            MatchSpec ms("xtensor[url=file:///home/wolfv/Downloads/"
                         "xtensor-0.21.4-hc9558a2_0.tar.bz2]");
            EXPECT_EQ(ms.name, "xtensor");
            EXPECT_EQ(
                ms.brackets["url"],
                "file:///home/wolfv/Downloads/xtensor-0.21.4-hc9558a2_0.tar.bz2"
            );
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
            MatchSpec ms("foo=1.0=2[md5=123123123, license=BSD-3, fn='test 123.tar.bz2', url='abcdef']"
            );
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
        {
            MatchSpec ms("numpy=1.20");
            EXPECT_EQ(ms.str(), "numpy=1.20");
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

    TEST(output, hide_secrets)
    {
        auto res = Console::instance().hide_secrets("http://myweb.com/t/my-12345-token/test.repo");
        EXPECT_EQ(res, "http://myweb.com/t/*****/test.repo");

        res = Console::instance().hide_secrets("http://root:secretpassword@myweb.com/test.repo");
        EXPECT_EQ(res, "http://root:*****@myweb.com/test.repo");

        res = Console::instance().hide_secrets(
            "http://root:secretpassword@myweb.com/test.repo http://root:secretpassword@myweb.com/test.repo"
        );
        EXPECT_EQ(res, "http://root:*****@myweb.com/test.repo http://root:*****@myweb.com/test.repo");

        res = Console::instance().hide_secrets(
            "http://root:secretpassword@myweb.com/test.repo\nhttp://myweb.com/t/my-12345-token/test.repo http://myweb.com/t/my-12345-token/test.repo http://root:secretpassword@myweb.com/test.repo"
        );
        EXPECT_EQ(
            res,
            "http://root:*****@myweb.com/test.repo\nhttp://myweb.com/t/*****/test.repo http://myweb.com/t/*****/test.repo http://root:*****@myweb.com/test.repo"
        );

        res = Console::instance().hide_secrets("myweb.com/t/my-12345-token/test.repo");
        EXPECT_EQ(res, "myweb.com/t/*****/test.repo");

        res = Console::instance().hide_secrets("root:secretpassword@myweb.com/test.repo");
        EXPECT_EQ(res, "root:*****@myweb.com/test.repo");
    }


    class OutputPromptTests : public testing::TestWithParam<std::tuple<std::string, char, bool>>
    {
    };

    TEST_P(OutputPromptTests, prompt)
    {
        auto params = GetParam();

        std::stringstream test_stream;
        test_stream << std::get<0>(params) << std::endl;
        EXPECT_EQ(
            Console::instance().prompt("Test prompt", std::get<1>(params), test_stream),
            std::get<2>(params)
        );
    }

    INSTANTIATE_TEST_SUITE_P(
        output,
        OutputPromptTests,
        testing::Values(
            std::make_tuple("y", 'y', true),
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
            std::make_tuple("No", 'n', false)
        )
    );

    TEST(context, env_name)
    {
        if constexpr (on_mac || on_linux)
        {
            auto& ctx = Context::instance();
            ctx.root_prefix = "/home/user/micromamba/";
            ctx.envs_dirs = { ctx.root_prefix / "envs" };
            fs::u8path prefix = "/home/user/micromamba/envs/testprefix";

            EXPECT_EQ(env_name(prefix), "testprefix");
            prefix = "/home/user/micromamba/envs/a.txt";
            EXPECT_EQ(env_name(prefix), "a.txt");
            prefix = "/home/user/micromamba/envs/a.txt";
            EXPECT_EQ(env_name(prefix), "a.txt");
            prefix = "/home/user/micromamba/envs/abc/a.txt";
            EXPECT_EQ(env_name(prefix), "/home/user/micromamba/envs/abc/a.txt");
            prefix = "/home/user/env";
            EXPECT_EQ(env_name(prefix), "/home/user/env");

// Workaround MSVC treating warning C4102 as an error in old version of MSVC,
// here triggered by GTest's macro implementation.
#if defined(_MSC_VER) && _MSC_VER > 1920
            EXPECT_THROW(locate_prefix_by_name("test"), std::runtime_error);
#endif
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
        fs::u8path pbefore = "/tmp/test/xyz.txt";
        fs::u8path p = env::expand_user(pbefore);
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

    TEST(link, replace_long_shebang)
    {
        if (!on_win)
        {
            std::string res = replace_long_shebang(
                "#!/this/is/loooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooong/python -o test -x"
            );
            if (on_linux)
            {
                EXPECT_EQ(res, "#!/usr/bin/env python -o test -x");
            }
            else
            {
                EXPECT_EQ(
                    res,
                    "#!/this/is/loooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooong/python -o test -x"
                );
            }

            if (on_linux)
            {
                res = replace_long_shebang(
                    "#!/this/is/loooooooooooooooooooooooooooooooooooooooooooooooooooo\\ oooooo\\ oooooo\\ oooooooooooooooooooooooooooooooooooong/python -o test -x"
                );
                EXPECT_EQ(res, "#!/usr/bin/env python -o test -x");
                res = replace_long_shebang(
                    "#!/this/is/loooooooooooooooooooooooooooooooooooooooooooooooooooo\\ oooooo\\ oooooo\\ oooooooooooooooooooooooooooooooooooong/pyt hon -o test -x"
                );
                EXPECT_EQ(res, "#!/usr/bin/env pyt hon -o test -x");
                res = replace_long_shebang(
                    "#!/this/is/loooooooooooooooooooooooooooooooooooooooooooooooooooo\\ oooooo\\ oooooo\\ oooooooooooooooooooooooooooooooooooong/pyt\\ hon -o test -x"
                );
                EXPECT_EQ(res, "#!/usr/bin/env pyt\\ hon -o test -x");
                res = replace_long_shebang(
                    "#! /this/is/loooooooooooooooooooooooooooooooooooooooooooooooooooo\\ oooooo\\ oooooo\\ oooooooooooooooooooooooooooooooooooong/pyt\\ hon -o test -x"
                );
                EXPECT_EQ(res, "#!/usr/bin/env pyt\\ hon -o test -x");
                res = replace_long_shebang(
                    "#!    /this/is/looooooooooooooooooooooooooooooooooooooooooooo\\ \\ ooooooo\\ oooooo\\ oooooo\\ ooooooooooooooooo\\ ooooooooooooooooooong/pyt\\ hon -o \"te  st\" -x"
                );
                EXPECT_EQ(res, "#!/usr/bin/env pyt\\ hon -o \"te  st\" -x");
            }

            std::string shebang = fmt::format("#!/{}/bin/python -o test 123 -x", std::string(500, 'a'));
            res = replace_long_shebang(shebang);
            EXPECT_EQ(res, "#!/usr/bin/env python -o test 123 -x");
            shebang = fmt::format("#!/{}/bin/python -o test 123 -x", std::string(500, 'a'));
            shebang[299] = '\\';
            shebang[300] = ' ';
            res = replace_long_shebang(shebang);
            EXPECT_EQ(res, "#!/usr/bin/env python -o test 123 -x");
        }
    }

    TEST(link, python_shebang)
    {
        auto res = python_shebang("/usr/bin/python");
        EXPECT_EQ(res, "#!/usr/bin/python");
        res = python_shebang("/usr/bin/pyth on with spaces");
        EXPECT_EQ(res, "#!/bin/sh\n'''exec' \"/usr/bin/pyth on with spaces\" \"$0\" \"$@\" #'''");
    }

    TEST(link, shebang_regex_matches)
    {
        std::string shebang = "#!/simple/shebang";
        std::smatch s;
        auto match = std::regex_match(shebang, s, shebang_regex);
        EXPECT_TRUE(match);
        EXPECT_EQ(s[0].str(), "#!/simple/shebang");
        EXPECT_EQ(s[1].str(), "#!/simple/shebang");
        EXPECT_EQ(s[2].str(), "/simple/shebang");
        EXPECT_EQ(s[3].str(), "");

        // // with spaces
        shebang = "#!    /simple/shebang";
        match = std::regex_match(shebang, s, shebang_regex);
        EXPECT_TRUE(match);
        EXPECT_EQ(s[0].str(), "#!    /simple/shebang");
        EXPECT_EQ(s[1].str(), "#!    /simple/shebang");
        EXPECT_EQ(s[2].str(), "/simple/shebang");
        EXPECT_EQ(s[3].str(), "");

        // with escaped spaces and flags
        shebang = "#!/simple/shebang/escaped\\ space --and --flags -x";
        match = std::regex_match(shebang, s, shebang_regex);
        EXPECT_TRUE(match);
        EXPECT_EQ(s[0].str(), "#!/simple/shebang/escaped\\ space --and --flags -x");
        EXPECT_EQ(s[1].str(), "#!/simple/shebang/escaped\\ space --and --flags -x");
        EXPECT_EQ(s[2].str(), "/simple/shebang/escaped\\ space");
        EXPECT_EQ(s[3].str(), " --and --flags -x");
    }

    TEST(utils, quote_for_shell)
    {
        if (!on_win)
        {
            std::vector<std::string> args1 = { "python", "-c", "print('is\ngreat')" };
            EXPECT_EQ(quote_for_shell(args1), "python -c 'print('\"'\"'is\ngreat'\"'\"')'");
            std::vector<std::string> args2 = { "python", "-c", "print(\"is great\")" };
            EXPECT_EQ(quote_for_shell(args2), "python -c 'print(\"is great\")'");
            std::vector<std::string> args3 = { "python", "very nice", "print(\"is great\")" };
            EXPECT_EQ(quote_for_shell(args3), "python 'very nice' 'print(\"is great\")'");
            std::vector<std::string> args4 = { "pyt \t tab", "very nice", "print(\"is great\")" };
            EXPECT_EQ(quote_for_shell(args4), "'pyt \t tab' 'very nice' 'print(\"is great\")'");
            std::vector<std::string> args5 = { "echo", "(" };
            EXPECT_EQ(quote_for_shell(args5), "echo '('");
            std::vector<std::string> args6 = { "echo", "foo'bar\nspam" };
            EXPECT_EQ(quote_for_shell(args6), "echo 'foo'\"'\"'bar\nspam'");
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

    TEST(utils, lexists)
    {
        fs::create_symlink("empty_target", "nonexistinglink");
        EXPECT_FALSE(fs::exists("nonexistinglink"));
        EXPECT_TRUE(lexists("nonexistinglink"));
        fs::remove("nonexistinglink");
        EXPECT_FALSE(fs::exists("nonexistinglink"));
        EXPECT_FALSE(lexists("nonexistinglink"));

        path::touch("emptytestfile");
        EXPECT_TRUE(fs::exists("emptytestfile"));
        EXPECT_TRUE(lexists("emptytestfile"));
        fs::create_symlink("emptytestfile", "existinglink");
        EXPECT_TRUE(fs::exists("existinglink"));
        EXPECT_TRUE(lexists("existinglink"));

        fs::remove("existinglink");
        EXPECT_FALSE(fs::exists("existinglink"));
        EXPECT_FALSE(lexists("existinglink"));
        fs::remove("emptytestfile");
        EXPECT_FALSE(fs::exists("emptytestfile"));
        EXPECT_FALSE(lexists("emptytestfile"));

        std::error_code ec;
        EXPECT_FALSE(lexists("completelyinexistent", ec));
        EXPECT_FALSE(ec);

        EXPECT_FALSE(fs::exists("completelyinexistent", ec));
        EXPECT_FALSE(ec);
    }

    namespace detail
    {
        // read the header that contains json like {"_mod": "...", ...}
        tl::expected<subdir_metadata, mamba_error> read_metadata(const fs::u8path& file);
    }

#ifdef _WIN32
    std::chrono::system_clock::time_point filetime_to_unix_test(const fs::file_time_type& filetime)
    {
        // windows filetime is in 100ns intervals since 1601-01-01
        static constexpr auto epoch_offset = std::chrono::seconds(11644473600ULL);
        return std::chrono::system_clock::time_point(
            std::chrono::duration_cast<std::chrono::system_clock::duration>(
                filetime.time_since_epoch() - epoch_offset
            )
        );
    }
#endif

    TEST(subdirdata, parse_mod_etag)
    {
        bool old_value = Context::instance().repodata_use_zst;
        Context::instance().repodata_use_zst = true;
        fs::u8path cache_folder = fs::u8path{ test_data_dir / "repodata_json_cache" };
        auto mq = detail::read_metadata(cache_folder / "test_1.json");
        EXPECT_TRUE(mq.has_value());
        auto j = mq.value();
        EXPECT_EQ(j.mod, "Fri, 11 Feb 2022 13:52:44 GMT");
        EXPECT_EQ(
            j.url,
            "file:///Users/wolfvollprecht/Programs/mamba/mamba/tests/channel_a/linux-64/repodata.json"
        );

        j = detail::read_metadata(cache_folder / "test_2.json").value();
        EXPECT_EQ(j.mod, "Fri, 11 Feb 2022 13:52:44 GMT");
        EXPECT_EQ(
            j.url,
            "file:///Users/wolfvollprecht/Programs/mamba/mamba/tests/channel_a/linux-64/repodata.json"
        );

        j = detail::read_metadata(cache_folder / "test_5.json").value();
        EXPECT_EQ(j.mod, "Fri, 11 Feb 2022 13:52:44 GMT");
        EXPECT_EQ(
            j.url,
            "file:///Users/wolfvollprecht/Programs/mamba/mamba/tests/channel_a/linux-64/repodata.json"
        );

        j = detail::read_metadata(cache_folder / "test_4.json").value();
        EXPECT_EQ(j.cache_control, "{{}}\",,,\"");
        EXPECT_EQ(j.etag, "\n\n\"\"randome ecx,,ssd\n,,\"");
        EXPECT_EQ(j.mod, "Fri, 11 Feb 2022 13:52:44 GMT");
        EXPECT_EQ(
            j.url,
            "file:///Users/wolfvollprecht/Programs/mamba/mamba/tests/channel_a/linux-64/repodata.json"
        );

        mq = detail::read_metadata(cache_folder / "test_3.json");
        EXPECT_TRUE(mq.has_value() == false);

        j = detail::read_metadata(cache_folder / "test_6.json").value();
        EXPECT_EQ(j.mod, "Thu, 02 Apr 2020 20:21:27 GMT");
        EXPECT_EQ(j.url, "https://conda.anaconda.org/intake/osx-arm64");

        auto state_file = cache_folder / "test_7.state.json";
        // set file_mtime


        {
#ifdef _WIN32
            auto file_mtime = filetime_to_unix_test(fs::last_write_time(cache_folder / "test_7.json"));
#else
            auto file_mtime = fs::last_write_time(cache_folder / "test_7.json");
#endif

            // auto file_size = fs::file_size(state_file);
            auto ifs = open_ifstream(state_file, std::ios::in | std::ios::binary);
            auto jstate = nlohmann::json::parse(ifs);
            ifs.close();
            auto nsecs = std::chrono::duration_cast<std::chrono::nanoseconds>(
                file_mtime.time_since_epoch()
            );
            jstate["mtime_ns"] = nsecs.count();

            auto file_size = fs::file_size(cache_folder / "test_7.json");
            jstate["size"] = file_size;

            auto ofs = open_ofstream(state_file);
            ofs << jstate.dump(4);
        }

        j = detail::read_metadata(cache_folder / "test_7.json").value();
        EXPECT_EQ(j.cache_control, "something");
        EXPECT_EQ(j.etag, "something else");
        EXPECT_EQ(j.mod, "Fri, 11 Feb 2022 13:52:44 GMT");
        EXPECT_EQ(j.url, "https://conda.anaconda.org/conda-forge/noarch/repodata.json.zst");
        EXPECT_EQ(j.has_zst.value().value, true);
        EXPECT_EQ(j.has_zst.value().last_checked, parse_utc_timestamp("2023-01-06T16:33:06Z"));

        Context::instance().repodata_use_zst = old_value;
    }
}  // namespace mamba
