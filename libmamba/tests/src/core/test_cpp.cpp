// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <chrono>
#include <sstream>
#include <tuple>

#include <catch2/catch_all.hpp>

#include "mamba/core/channel_context.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/fsutil.hpp"
#include "mamba/core/history.hpp"
#include "mamba/core/link.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/subdirdata.hpp"
#include "mamba/util/build.hpp"
#include "mamba/util/path_manip.hpp"

#include "mambatests.hpp"

namespace mamba
{

    TEST_CASE("cache_name_from_url")
    {
        REQUIRE(cache_name_from_url("http://test.com/1234/") == "302f0a61");
        REQUIRE(cache_name_from_url("http://test.com/1234/repodata.json") == "302f0a61");
        REQUIRE(cache_name_from_url("http://test.com/1234/current_repodata.json") == "78a8cce9");
    }

    // TEST(cpp_install, install)
    // {
    //     mambatests::context().output_params.verbosity = 3;
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

    namespace
    {
        TEST_CASE("user_request")
        {
            auto u = History::UserRequest::prefilled(mambatests::context());
            // update in 100 years!
            REQUIRE(u.date[0] == '2');
            REQUIRE(u.date[1] == '0');
        }
    }

    namespace
    {
        TEST_CASE("hide_secrets")
        {
            auto res = Console::instance().hide_secrets("http://myweb.com/t/my-12345-token/test.repo");
            REQUIRE(res == "http://myweb.com/t/*****/test.repo");

            res = Console::instance().hide_secrets("http://root:secretpassword@myweb.com/test.repo");
            REQUIRE(res == "http://root:*****@myweb.com/test.repo");

            res = Console::instance().hide_secrets(
                "http://root:secretpassword@myweb.com/test.repo http://root:secretpassword@myweb.com/test.repo"
            );
            REQUIRE(
                res == "http://root:*****@myweb.com/test.repo http://root:*****@myweb.com/test.repo"
            );

            res = Console::instance().hide_secrets(
                "http://root:secretpassword@myweb.com/test.repo\nhttp://myweb.com/t/my-12345-token/test.repo http://myweb.com/t/my-12345-token/test.repo http://root:secretpassword@myweb.com/test.repo"
            );
            REQUIRE(
                res == "http://root:*****@myweb.com/test.repo\nhttp://myweb.com/t/*****/test.repo http://myweb.com/t/*****/test.repo http://root:*****@myweb.com/test.repo"
            );

            res = Console::instance().hide_secrets("myweb.com/t/my-12345-token/test.repo");
            REQUIRE(res == "myweb.com/t/*****/test.repo");

            res = Console::instance().hide_secrets("root:secretpassword@myweb.com/test.repo");
            REQUIRE(res == "root:*****@myweb.com/test.repo");
        }

        // Note: this was initially a value-parametrized test; unfortunately,
        // doctest does not support this feature yet.
        TEST_CASE("prompt")
        {
            using param_type = std::tuple<std::string, char, bool>;
            std::vector<param_type> param_values = {
                std::make_tuple("y", 'y', true),   std::make_tuple("yes", 'y', true),
                std::make_tuple("Y", 'y', true),   std::make_tuple("Yes", 'y', true),
                std::make_tuple("", 'y', true),    std::make_tuple("n", 'y', false),
                std::make_tuple("no", 'y', false), std::make_tuple("N", 'y', false),
                std::make_tuple("No", 'y', false),

                std::make_tuple("y", 'n', true),   std::make_tuple("yes", 'n', true),
                std::make_tuple("Y", 'n', true),   std::make_tuple("Yes", 'n', true),
                std::make_tuple("", 'n', false),   std::make_tuple("n", 'n', false),
                std::make_tuple("no", 'n', false), std::make_tuple("N", 'n', false),
                std::make_tuple("No", 'n', false)
            };

            for (const auto& p : param_values)
            {
                CAPTURE(p);
                std::stringstream test_stream;
                test_stream << std::get<0>(p) << std::endl;
                REQUIRE(
                    Console::instance().prompt("Test prompt", std::get<1>(p), test_stream)
                    == std::get<2>(p)
                );
            }
        }
    }

    namespace
    {
        TEST_CASE("env_name")
        {
            if constexpr (util::on_mac || util::on_linux)
            {
                auto& ctx = mambatests::context();
                ctx.prefix_params.root_prefix = "/home/user/micromamba/";
                ctx.envs_dirs = { ctx.prefix_params.root_prefix / "envs" };
                fs::u8path prefix = "/home/user/micromamba/envs/testprefix";

                REQUIRE(env_name(ctx, prefix) == "testprefix");
                prefix = "/home/user/micromamba/envs/a.txt";
                REQUIRE(env_name(ctx, prefix) == "a.txt");
                prefix = "/home/user/micromamba/envs/a.txt";
                REQUIRE(env_name(ctx, prefix) == "a.txt");
                prefix = "/home/user/micromamba/envs/abc/a.txt";
                REQUIRE(env_name(ctx, prefix) == "/home/user/micromamba/envs/abc/a.txt");
                prefix = "/home/user/env";
                REQUIRE(env_name(ctx, prefix) == "/home/user/env");
            }
        }
    }

    namespace
    {
        TEST_CASE("starts_with_home")
        {
            if (util::on_linux)
            {
                auto home = fs::u8path(util::expand_home("~"));
                REQUIRE(path::starts_with_home(home / "test" / "file.txt") == true);
                REQUIRE(path::starts_with_home("~") == true);
                REQUIRE(path::starts_with_home("/opt/bin") == false);
            }
        }

        TEST_CASE("touch")
        {
            if (util::on_linux)
            {
                path::touch("/tmp/dir/file.txt", true);
                REQUIRE(fs::exists("/tmp/dir/file.txt"));
            }
        }
    }

    namespace
    {
        TEST_CASE("replace_long_shebang")
        {
            if (!util::on_win)
            {
                std::string res = replace_long_shebang(
                    "#!/this/is/loooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooong/python -o test -x"
                );
                if (util::on_linux)
                {
                    REQUIRE(res == "#!/usr/bin/env python -o test -x");
                }
                else
                {
                    REQUIRE(
                        res == "#!/this/is/loooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooong/python -o test -x"
                    );
                }

                if (util::on_linux)
                {
                    res = replace_long_shebang(
                        "#!/this/is/loooooooooooooooooooooooooooooooooooooooooooooooooooo\\ oooooo\\ oooooo\\ oooooooooooooooooooooooooooooooooooong/python -o test -x"
                    );
                    REQUIRE(res == "#!/usr/bin/env python -o test -x");
                    res = replace_long_shebang(
                        "#!/this/is/loooooooooooooooooooooooooooooooooooooooooooooooooooo\\ oooooo\\ oooooo\\ oooooooooooooooooooooooooooooooooooong/pyt hon -o test -x"
                    );
                    REQUIRE(res == "#!/usr/bin/env pyt hon -o test -x");
                    res = replace_long_shebang(
                        "#!/this/is/loooooooooooooooooooooooooooooooooooooooooooooooooooo\\ oooooo\\ oooooo\\ oooooooooooooooooooooooooooooooooooong/pyt\\ hon -o test -x"
                    );
                    REQUIRE(res == "#!/usr/bin/env pyt\\ hon -o test -x");
                    res = replace_long_shebang(
                        "#! /this/is/loooooooooooooooooooooooooooooooooooooooooooooooooooo\\ oooooo\\ oooooo\\ oooooooooooooooooooooooooooooooooooong/pyt\\ hon -o test -x"
                    );
                    REQUIRE(res == "#!/usr/bin/env pyt\\ hon -o test -x");
                    res = replace_long_shebang(
                        "#!    /this/is/looooooooooooooooooooooooooooooooooooooooooooo\\ \\ ooooooo\\ oooooo\\ oooooo\\ ooooooooooooooooo\\ ooooooooooooooooooong/pyt\\ hon -o \"te  st\" -x"
                    );
                    REQUIRE(res == "#!/usr/bin/env pyt\\ hon -o \"te  st\" -x");
                }

                std::string shebang = fmt::format(
                    "#!/{}/bin/python -o test 123 -x",
                    std::string(500, 'a')
                );
                res = replace_long_shebang(shebang);
                REQUIRE(res == "#!/usr/bin/env python -o test 123 -x");
                shebang = fmt::format("#!/{}/bin/python -o test 123 -x", std::string(500, 'a'));
                shebang[299] = '\\';
                shebang[300] = ' ';
                res = replace_long_shebang(shebang);
                REQUIRE(res == "#!/usr/bin/env python -o test 123 -x");
            }
        }

        TEST_CASE("python_shebang")
        {
            auto res = python_shebang("/usr/bin/python");
            REQUIRE(res == "#!/usr/bin/python");
            res = python_shebang("/usr/bin/pyth on with spaces");
            REQUIRE(res == "#!/bin/sh\n'''exec' \"/usr/bin/pyth on with spaces\" \"$0\" \"$@\" #'''");
        }

        TEST_CASE("shebang_regex_matches")
        {
            std::string shebang = "#!/simple/shebang";
            std::smatch s;
            auto match = std::regex_match(shebang, s, shebang_regex);
            REQUIRE(match);
            REQUIRE(s[0].str() == "#!/simple/shebang");
            REQUIRE(s[1].str() == "#!/simple/shebang");
            REQUIRE(s[2].str() == "/simple/shebang");
            REQUIRE(s[3].str() == "");

            // // with spaces
            shebang = "#!    /simple/shebang";
            match = std::regex_match(shebang, s, shebang_regex);
            REQUIRE(match);
            REQUIRE(s[0].str() == "#!    /simple/shebang");
            REQUIRE(s[1].str() == "#!    /simple/shebang");
            REQUIRE(s[2].str() == "/simple/shebang");
            REQUIRE(s[3].str() == "");

            // with escaped spaces and flags
            shebang = "#!/simple/shebang/escaped\\ space --and --flags -x";
            match = std::regex_match(shebang, s, shebang_regex);
            REQUIRE(match);
            REQUIRE(s[0].str() == "#!/simple/shebang/escaped\\ space --and --flags -x");
            REQUIRE(s[1].str() == "#!/simple/shebang/escaped\\ space --and --flags -x");
            REQUIRE(s[2].str() == "/simple/shebang/escaped\\ space");
            REQUIRE(s[3].str() == " --and --flags -x");
        }
    }

    namespace
    {
        TEST_CASE("quote_for_shell")
        {
            if (!util::on_win)
            {
                std::vector<std::string> args1 = { "python", "-c", "print('is\ngreat')" };
                REQUIRE(quote_for_shell(args1) == "python -c 'print('\"'\"'is\ngreat'\"'\"')'");
                std::vector<std::string> args2 = { "python", "-c", "print(\"is great\")" };
                REQUIRE(quote_for_shell(args2) == "python -c 'print(\"is great\")'");
                std::vector<std::string> args3 = { "python", "very nice", "print(\"is great\")" };
                REQUIRE(quote_for_shell(args3) == "python 'very nice' 'print(\"is great\")'");
                std::vector<std::string> args4 = { "pyt \t tab", "very nice", "print(\"is great\")" };
                REQUIRE(quote_for_shell(args4) == "'pyt \t tab' 'very nice' 'print(\"is great\")'");
                std::vector<std::string> args5 = { "echo", "(" };
                REQUIRE(quote_for_shell(args5) == "echo '('");
                std::vector<std::string> args6 = { "echo", "foo'bar\nspam" };
                REQUIRE(quote_for_shell(args6) == "echo 'foo'\"'\"'bar\nspam'");
            }

            std::vector<std::string> args1 = { "a b c", "d", "e" };
            REQUIRE(quote_for_shell(args1, "cmdexe") == "\"a b c\" d e");
            std::vector<std::string> args2 = { "ab\"c", "\\", "d" };
            REQUIRE(quote_for_shell(args2, "cmdexe") == "ab\\\"c \\ d");
            std::vector<std::string> args3 = { "ab\"c", " \\", "d" };
            REQUIRE(quote_for_shell(args3, "cmdexe") == "ab\\\"c \" \\\\\" d");
            std::vector<std::string> args4 = { "a\\\\\\b", "de fg", "h" };
            REQUIRE(quote_for_shell(args4, "cmdexe") == "a\\\\\\b \"de fg\" h");
            std::vector<std::string> args5 = { "a\\\"b", "c", "d" };
            REQUIRE(quote_for_shell(args5, "cmdexe") == "a\\\\\\\"b c d");
            std::vector<std::string> args6 = { "a\\\\b c", "d", "e" };
            REQUIRE(quote_for_shell(args6, "cmdexe") == "\"a\\\\b c\" d e");
            std::vector<std::string> args7 = { "a\\\\b\\ c", "d", "e" };
            REQUIRE(quote_for_shell(args7, "cmdexe") == "\"a\\\\b\\ c\" d e");
            std::vector<std::string> args8 = { "ab", "" };
            REQUIRE(quote_for_shell(args8, "cmdexe") == "ab \"\"");
        }

        TEST_CASE("lexists")
        {
            fs::create_symlink("empty_target", "nonexistinglink");
            REQUIRE_FALSE(fs::exists("nonexistinglink"));
            REQUIRE(lexists("nonexistinglink"));
            fs::remove("nonexistinglink");
            REQUIRE_FALSE(fs::exists("nonexistinglink"));
            REQUIRE_FALSE(lexists("nonexistinglink"));

            path::touch("emptytestfile");
            REQUIRE(fs::exists("emptytestfile"));
            REQUIRE(lexists("emptytestfile"));
            fs::create_symlink("emptytestfile", "existinglink");
            REQUIRE(fs::exists("existinglink"));
            REQUIRE(lexists("existinglink"));

            fs::remove("existinglink");
            REQUIRE_FALSE(fs::exists("existinglink"));
            REQUIRE_FALSE(lexists("existinglink"));
            fs::remove("emptytestfile");
            REQUIRE_FALSE(fs::exists("emptytestfile"));
            REQUIRE_FALSE(lexists("emptytestfile"));

            std::error_code ec;
            REQUIRE_FALSE(lexists("completelyinexistent", ec));
            REQUIRE_FALSE(ec);

            REQUIRE_FALSE(fs::exists("completelyinexistent", ec));
            REQUIRE_FALSE(ec);
        }
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

    namespace
    {
        TEST_CASE("parse_last_modified_etag")
        {
            fs::u8path cache_folder = fs::u8path{ mambatests::test_data_dir / "repodata_json_cache" };
            auto mq = SubdirMetadata::read(cache_folder / "test_1.json");
            REQUIRE(mq.has_value());
            auto j = mq.value();
            REQUIRE(j.last_modified(), "Fri == 11 Feb 2022 13:52:44 GMT");
            REQUIRE(
                j.url()
                == "file:///Users/wolfvollprecht/Programs/mamba/mamba/tests/channel_a/linux-64/repodata.json"
            );

            j = SubdirMetadata::read(cache_folder / "test_2.json").value();
            REQUIRE(j.last_modified(), "Fri == 11 Feb 2022 13:52:44 GMT");
            REQUIRE(
                j.url()
                == "file:///Users/wolfvollprecht/Programs/mamba/mamba/tests/channel_a/linux-64/repodata.json"
            );

            j = SubdirMetadata::read(cache_folder / "test_5.json").value();
            REQUIRE(j.last_modified(), "Fri == 11 Feb 2022 13:52:44 GMT");
            REQUIRE(
                j.url()
                == "file:///Users/wolfvollprecht/Programs/mamba/mamba/tests/channel_a/linux-64/repodata.json"
            );

            j = SubdirMetadata::read(cache_folder / "test_4.json").value();
            REQUIRE(j.cache_control() == "{{}}\",,,\"");
            REQUIRE(j.etag() == "\n\n\"\"randome ecx,,ssd\n,,\"");
            REQUIRE(j.last_modified(), "Fri == 11 Feb 2022 13:52:44 GMT");
            REQUIRE(
                j.url()
                == "file:///Users/wolfvollprecht/Programs/mamba/mamba/tests/channel_a/linux-64/repodata.json"
            );

            mq = SubdirMetadata::read(cache_folder / "test_3.json");
            REQUIRE(mq.has_value() == false);

            j = SubdirMetadata::read(cache_folder / "test_6.json").value();
            REQUIRE(j.last_modified(), "Thu == 02 Apr 2020 20:21:27 GMT");
            REQUIRE(j.url() == "https://conda.anaconda.org/intake/osx-arm64");

            auto state_file = cache_folder / "test_7.state.json";
            // set file_mtime


            {
#ifdef _WIN32
                auto file_mtime = filetime_to_unix_test(
                    fs::last_write_time(cache_folder / "test_7.json")
                );
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

            j = SubdirMetadata::read(cache_folder / "test_7.json").value();
            REQUIRE(j.cache_control() == "something");
            REQUIRE(j.etag() == "something else");
            REQUIRE(j.last_modified(), "Fri == 11 Feb 2022 13:52:44 GMT");
            REQUIRE(j.url() == "https://conda.anaconda.org/conda-forge/noarch/repodata.json.zst");
            REQUIRE(j.has_zst() == false);
        }
    }
}  // namespace mamba
