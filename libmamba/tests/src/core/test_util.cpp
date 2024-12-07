// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <catch2/catch_all.hpp>

#include "mamba/core/context.hpp"
#include "mamba/core/fsutil.hpp"
#include "mamba/core/util.hpp"
#include "mamba/core/util_scope.hpp"
#include "mamba/fs/filesystem.hpp"
#include "mamba/util/path_manip.hpp"

#include "mambatests.hpp"

namespace mamba
{
    TEST_CASE("basics")
    {
        bool executed = false;
        {
            on_scope_exit _{ [&] { executed = true; } };
            REQUIRE_FALSE(executed);
        }
        REQUIRE(executed);
    }

    TEST_CASE("is_yaml_file_name")
    {
        REQUIRE(is_yaml_file_name("something.yaml"));
        REQUIRE(is_yaml_file_name("something.yml"));
        REQUIRE(is_yaml_file_name("something-lock.yaml"));
        REQUIRE(is_yaml_file_name("something-lock.yml"));
        REQUIRE(is_yaml_file_name("/some/dir/something.yaml"));
        REQUIRE(is_yaml_file_name("/some/dir/something.yaml"));
        REQUIRE(is_yaml_file_name("../../some/dir/something.yml"));
        REQUIRE(is_yaml_file_name("../../some/dir/something.yml"));

        REQUIRE(is_yaml_file_name(fs::u8path{ "something.yaml" }.string()));
        REQUIRE(is_yaml_file_name(fs::u8path{ "something.yml" }.string()));
        REQUIRE(is_yaml_file_name(fs::u8path{ "something-lock.yaml" }.string()));
        REQUIRE(is_yaml_file_name(fs::u8path{ "something-lock.yml" }.string()));
        REQUIRE(is_yaml_file_name(fs::u8path{ "/some/dir/something.yaml" }.string()));
        REQUIRE(is_yaml_file_name(fs::u8path{ "/some/dir/something.yml" }.string()));
        REQUIRE(is_yaml_file_name(fs::u8path{ "../../some/dir/something.yaml" }.string()));
        REQUIRE(is_yaml_file_name(fs::u8path{ "../../some/dir/something.yml" }.string()));

        REQUIRE_FALSE(is_yaml_file_name("something"));
        REQUIRE_FALSE(is_yaml_file_name("something-lock"));
        REQUIRE_FALSE(is_yaml_file_name("/some/dir/something"));
        REQUIRE_FALSE(is_yaml_file_name("../../some/dir/something"));

        REQUIRE_FALSE(is_yaml_file_name(fs::u8path{ "something" }.string()));
        REQUIRE_FALSE(is_yaml_file_name(fs::u8path{ "something-lock" }.string()));
        REQUIRE_FALSE(is_yaml_file_name(fs::u8path{ "/some/dir/something" }.string()));
        REQUIRE_FALSE(is_yaml_file_name(fs::u8path{ "../../some/dir/something" }.string()));
    }

    TEST_CASE("is_writable")
    {
        const auto test_dir_path = fs::temp_directory_path() / "libmamba" / "writable_tests";
        fs::create_directories(test_dir_path);
        on_scope_exit _{ [&]
                         {
                             fs::permissions(test_dir_path, fs::perms::all);
                             fs::remove_all(test_dir_path);
                         } };

        REQUIRE(path::is_writable(test_dir_path));
        fs::permissions(test_dir_path, fs::perms::none);
        REQUIRE_FALSE(path::is_writable(test_dir_path));
        fs::permissions(test_dir_path, fs::perms::all);
        REQUIRE(path::is_writable(test_dir_path));

        REQUIRE(path::is_writable(test_dir_path / "non-existing-writable-test-delete-me.txt"));
        REQUIRE(path::is_writable(
            util::expand_home("~/.libmamba-non-existing-writable-test-delete-me.txt")
        ));

        REQUIRE(path::is_writable(test_dir_path / "non-existing-subfolder"));
        REQUIRE_FALSE(fs::exists(test_dir_path / "non-existing-subfolder"));

        {
            const auto existing_file_path = test_dir_path / "existing-writable-test-delete-me.txt";
            {
                std::ofstream temp_file{ existing_file_path.std_path() };
                REQUIRE(temp_file.is_open());
                temp_file << "delete me" << std::endl;
            }
            REQUIRE(path::is_writable(existing_file_path));
            fs::permissions(existing_file_path, fs::perms::none);
            REQUIRE_FALSE(path::is_writable(existing_file_path));
            fs::permissions(existing_file_path, fs::perms::all);
            REQUIRE(path::is_writable(existing_file_path));
        }
    }

    TEST_CASE("proxy_match")
    {
        auto& context = mambatests::singletons().context;
        context.remote_fetch_params.proxy_servers = { { "http", "foo" },
                                                      { "https", "bar" },
                                                      { "https://example.net", "foobar" },
                                                      { "all://example.net", "baz" },
                                                      { "all", "other" } };

        auto proxy_match_with_context = [&](const char* url)
        { return proxy_match(url, context.remote_fetch_params.proxy_servers); };

        REQUIRE(*proxy_match_with_context("http://example.com/channel") == "foo");
        REQUIRE(*proxy_match_with_context("http://example.net/channel") == "foo");
        REQUIRE(*proxy_match_with_context("https://example.com/channel") == "bar");
        REQUIRE(*proxy_match_with_context("https://example.com:8080/channel") == "bar");
        REQUIRE(*proxy_match_with_context("https://example.net/channel") == "foobar");
        REQUIRE(*proxy_match_with_context("ftp://example.net/channel") == "baz");
        REQUIRE(*proxy_match_with_context("ftp://example.org") == "other");

        context.remote_fetch_params.proxy_servers = { { "http", "foo" },
                                                      { "https", "bar" },
                                                      { "https://example.net", "foobar" },
                                                      { "all://example.net", "baz" } };

        REQUIRE_FALSE(proxy_match_with_context("ftp://example.org").has_value());

        context.remote_fetch_params.proxy_servers = {};

        REQUIRE_FALSE(proxy_match_with_context("http://example.com/channel").has_value());
    }
}
