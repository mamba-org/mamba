// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <vector>

#include <catch2/catch_all.hpp>

#include "mamba/core/subdir_index.hpp"
#include "mamba/core/util.hpp"
#include "mamba/core/util_scope.hpp"
#include "mamba/fs/filesystem.hpp"
#include "mamba/util/build.hpp"

namespace mamba
{
    namespace
    {
        TEST_CASE("normalized_separators")
        {
            static constexpr auto value = u8"a/b/c";
            std::filesystem::path x{ value };
            const auto y = fs::normalized_separators(x);
#if defined(_WIN32)
            REQUIRE(y.u8string() == u8R"(a\b\c)");
#else
            REQUIRE(y.u8string() == value);
#endif
        }

        TEST_CASE("normalized_separators_unicode")
        {
            static constexpr auto value = u8"日本語";
            const std::filesystem::path x = fs::from_utf8(value);
            REQUIRE(x.u8string() == u8"日本語");  // check assumption
            const auto y = fs::normalized_separators(x);
            REQUIRE(y.u8string() == u8"日本語");
        }

        TEST_CASE("to_utf8_check_separators")
        {
            static constexpr auto some_path_str = u8"a/b/c";
            std::filesystem::path some_path = std::filesystem::u8path(some_path_str);

            REQUIRE(fs::to_utf8(some_path, { /*normalize_sep=*/false }) == some_path_str);
#if defined(_WIN32)
            REQUIRE(fs::to_utf8(some_path, { /*normalize_sep=*/true }) == u8"a\\b\\c");
#else
            REQUIRE(fs::to_utf8(some_path, { /*normalize_sep=*/true }) == some_path_str);
#endif
        }

        TEST_CASE("to_utf8_check_separators_unicode")
        {
            static constexpr auto some_path_str = u8"日/本/語";
            std::filesystem::path some_path = std::filesystem::u8path(some_path_str);

            REQUIRE(fs::to_utf8(some_path, { /*normalize_sep=*/false }) == some_path_str);
#if defined(_WIN32)
            REQUIRE(fs::to_utf8(some_path, { /*normalize_sep=*/true }) == u8"日\\本\\語");
#else
            REQUIRE(fs::to_utf8(some_path, { /*normalize_sep=*/true }) == some_path_str);
#endif
        }

        TEST_CASE("from_utf8_check_separators")
        {
            static constexpr auto some_path_str = u8"a/b/c";

#if defined(_WIN32)
            REQUIRE(fs::from_utf8(some_path_str) == std::filesystem::u8path(u8"a\\b\\c"));
#else
            REQUIRE(fs::from_utf8(some_path_str) == std::filesystem::u8path(u8"a/b/c"));
#endif
        }

        TEST_CASE("from_utf8_check_separators_unicode")
        {
            static constexpr auto some_path_str = u8"日/本/語";

#if defined(_WIN32)
            REQUIRE(fs::from_utf8(some_path_str) == std::filesystem::u8path(u8"日\\本\\語"));
#else
            REQUIRE(fs::from_utf8(some_path_str) == std::filesystem::u8path(u8"日/本/語"));
#endif
        }

        TEST_CASE("u8path_separators_formatting")
        {
            static constexpr auto some_path_str = u8"a/b/c";
            std::filesystem::path some_path = std::filesystem::u8path(some_path_str);
            const fs::u8path u8_path(some_path);

#if defined(_WIN32)
            REQUIRE(u8_path.string() == u8"a\\b\\c");
#else
            REQUIRE(u8_path.string() == some_path_str);
#endif
            REQUIRE(u8_path.generic_string() == some_path_str);
        }

        TEST_CASE("consistent_encoding")
        {
            const auto utf8_string = u8"日本語";
            const fs::u8path filename(utf8_string);
            const auto str = filename.string();
            REQUIRE(str == utf8_string);

            const fs::u8path file_path = fs::temp_directory_path() / filename;
            REQUIRE(file_path.filename().string() == utf8_string);

            const auto std_path = file_path.std_path();
            REQUIRE(std_path.filename().u8string() == utf8_string);
        }

        TEST_CASE("string_stream_encoding")
        {
            const auto utf8_string = u8"日本語";
            const std::string quoted_utf8_string = std::string("\"") + utf8_string
                                                   + std::string("\"");
            const fs::u8path filename(utf8_string);
            std::stringstream stream;
            stream << filename;
            REQUIRE(stream.str() == quoted_utf8_string);

            fs::u8path path_read;
            stream.seekg(0);
            stream >> path_read;
            REQUIRE(path_read.string() == utf8_string);
        }

        TEST_CASE("directory_iteration")
        {
            const auto tmp_dir = fs::temp_directory_path() / "mamba_fs_iteration";
            mamba::on_scope_exit _([&] { fs::remove_all(tmp_dir); });  // Cleanup if not debugging.
            const auto file_dir = tmp_dir / "kikoo" / "lol" / u8"日本語";
            const auto file_path = file_dir / u8"joël";

            fs::remove_all(tmp_dir);  // Make sure it's not existing from the start.
            fs::create_directories(file_dir);

            {
                std::ofstream file(file_path.std_path(), std::ios::binary | std::ios::trunc);
                file << u8"日本語";
            }

            {
                const auto path_to_search_from = file_dir.parent_path();
                fs::recursive_directory_iterator it{ path_to_search_from };
                auto first_entry = *it;
                REQUIRE(first_entry.path() == file_path.parent_path());
                auto secibd_entry = *(++it);
                REQUIRE(secibd_entry.path() == file_path);
            }

            {
                const std::vector<fs::u8path> expected_entries{
                    tmp_dir / "kikoo",
                    tmp_dir / "kikoo" / "lol",
                    tmp_dir / "kikoo" / "lol" / u8"日本語",
                    tmp_dir / "kikoo" / "lol" / u8"日本語" / u8"joël",
                };

                std::vector<fs::u8path> entries_found;
                for (const auto& entry : fs::recursive_directory_iterator(tmp_dir))
                {
                    static_assert(std::is_same_v<decltype(entry.path()), fs::u8path>);
                    entries_found.push_back(entry.path());
                }
                REQUIRE(entries_found == expected_entries);
            }

            {
                const std::vector<std::string> expected_entries{
                    (tmp_dir / "kikoo").string(),
                    (tmp_dir / "kikoo" / "lol").string(),
                    (tmp_dir / "kikoo" / "lol" / u8"日本語").string(),
                    (tmp_dir / "kikoo" / "lol" / u8"日本語" / u8"joël").string(),
                };

                std::vector<std::string> entries_found;
                for (const auto& entry : fs::recursive_directory_iterator(tmp_dir))
                {
                    static_assert(std::is_same_v<decltype(entry.path()), fs::u8path>);
                    entries_found.push_back(entry.path().string());
                }
                REQUIRE(entries_found == expected_entries);
            }

            {
                const std::vector<fs::u8path> expected_entries{
                    tmp_dir / "kikoo" / "lol" / u8"日本語" / u8"joël",
                };

                std::vector<fs::u8path> entries_found;
                for (const auto& entry : fs::directory_iterator(file_dir))
                {
                    static_assert(std::is_same_v<decltype(entry.path()), fs::u8path>);
                    entries_found.push_back(entry.path());
                }
                REQUIRE(entries_found == expected_entries);
            }

            {
                const std::vector<std::string> expected_entries{
                    (tmp_dir / "kikoo" / "lol" / u8"日本語" / u8"joël").string(),
                };

                std::vector<std::string> entries_found;
                for (const auto& entry : fs::directory_iterator(file_dir))
                {
                    static_assert(std::is_same_v<decltype(entry.path()), fs::u8path>);
                    entries_found.push_back(entry.path().string());
                }
                REQUIRE(entries_found == expected_entries);
            }
        }

        TEST_CASE("long_paths")
        {
            const auto tmp_dir = fs::temp_directory_path() / "mamba_fs_long_path";

            fs::u8path long_path = tmp_dir;
            for (int i = 0; i < 42; ++i)
            {
                long_path /= u8"some_very_long_prefix";
            }

            mamba::on_scope_exit _([&] { fs::remove_all(long_path); });
            fs::create_directories(long_path);
        }

#if defined(_WIN32)
        TEST_CASE("append_maintains_slash_type")
        {
            const fs::u8path path = u8R"(a/b/c/d)";
            const auto path_1 = path / u8R"(e\f\g)";
            REQUIRE(path_1.string() == u8R"(a\b\c\d\e\f\g)");
        }
#endif
    }

    namespace
    {
        TEST_CASE("remove_readonly_file")
        {
            const auto tmp_dir = fs::temp_directory_path() / "mamba-fs-delete-me";
            // NOTE: the following will FAIL if the test doesnt pass (it relies on the tested
            // function)
            mamba::on_scope_exit _([&] { fs::remove_all(tmp_dir); });  // Cleanup if not debugging.
            fs::create_directories(tmp_dir);

            const auto readonly_file_path = tmp_dir / "fs-readonly-file";
            {
                std::ofstream readonly_file{ readonly_file_path.std_path(),
                                             readonly_file.binary | readonly_file.trunc };
                readonly_file << "delete me" << std::endl;
            }

            // set to read-only
            fs::permissions(
                readonly_file_path,
                fs::perms::owner_read | fs::perms::group_read,
                fs::perm_options::replace
            );
            REQUIRE(
                (fs::status(readonly_file_path).permissions() & fs::perms::owner_write)
                == fs::perms::none
            );
            REQUIRE(
                (fs::status(readonly_file_path).permissions() & fs::perms::group_write)
                == fs::perms::none
            );

            // removing should still work.
            REQUIRE(fs::exists(readonly_file_path));
            fs::remove(readonly_file_path);
            REQUIRE_FALSE(fs::exists(readonly_file_path));
        }

        TEST_CASE("remove_all_readonly_files")
        {
            const auto tmp_dir = fs::temp_directory_path() / "mamba-fs-delete-me";
            // NOTE: the following will FAIL if the test doesnt pass (it relies on the tested
            // function)
            mamba::on_scope_exit _([&] { fs::remove_all(tmp_dir); });  // Cleanup if not debugging.
            fs::create_directories(tmp_dir);

            static constexpr int file_count_per_directory = 3;
            static constexpr int subdir_count_per_directory = 3;
            static constexpr int tree_depth = 3;

            const auto create_readonly_files = [](const fs::u8path& dir_path)
            {
                assert(fs::is_directory(dir_path));
                for (int file_idx = 0; file_idx < file_count_per_directory; ++file_idx)
                {
                    const auto readonly_file_path = dir_path
                                                    / fmt::format("readonly-file-{}", file_idx);
                    {
                        std::ofstream readonly_file{ readonly_file_path.std_path(),
                                                     readonly_file.binary | readonly_file.trunc };
                        readonly_file << "delete me" << std::endl;
                    }

                    // set to read-only
                    fs::permissions(
                        readonly_file_path,
                        fs::perms::owner_read | fs::perms::group_read,
                        fs::perm_options::replace
                    );
                    REQUIRE(
                        (fs::status(readonly_file_path).permissions() & fs::perms::owner_write)
                        == fs::perms::none
                    );
                    REQUIRE(
                        (fs::status(readonly_file_path).permissions() & fs::perms::group_write)
                        == fs::perms::none
                    );
                }
            };

            const auto create_subdirs = [&](const std::vector<fs::u8path>& dirs)
            {
                auto subdirs = dirs;
                for (const auto& dir_path : dirs)
                {
                    for (int subdir_idx = 0; subdir_idx < subdir_count_per_directory; ++subdir_idx)
                    {
                        subdirs.push_back(dir_path / fmt::format("{}", subdir_idx));
                    }
                }
                return subdirs;
            };

            std::vector<fs::u8path> dirs{ tmp_dir };
            for (int depth = 0; depth < tree_depth; ++depth)
            {
                dirs = create_subdirs(dirs);
            }

            for (const auto& dir_path : dirs)
            {
                fs::create_directories(dir_path);
                create_readonly_files(dir_path);
            }

            REQUIRE(fs::exists(tmp_dir));
            fs::remove_all(tmp_dir);
            REQUIRE_FALSE(fs::exists(tmp_dir));
        }

        TEST_CASE("create_cache_dir")
        {
            // `create_cache_dir` create a `cache` subdirectory at a given path given as an
            // argument.
            const auto cache_path = fs::temp_directory_path() / "mamba-fs-cache-path";
            const auto cache_dir = cache_path / "cache";

            mamba::on_scope_exit _([&] { fs::remove_all(cache_path); });

            // Get the information about whether the filesystem supports the `set_gid` bit.
            bool supports_setgid_bit = false;
            fs::create_directories(cache_path);

            std::error_code ec;
            fs::permissions(cache_path, fs::perms::set_gid, fs::perm_options::add, ec);

            if (!ec)
            {
                supports_setgid_bit = (fs::status(cache_path).permissions() & fs::perms::set_gid)
                                      == fs::perms::set_gid;
            }

            // Check that `cache_dir` does not exist before calling `create_cache_dir`
            REQUIRE_FALSE(fs::exists(cache_dir));

            create_cache_dir(cache_path);

            REQUIRE(fs::exists(cache_dir));
            REQUIRE(fs::is_directory(cache_dir));

            // Check that the permissions of `cache_dir` are _at least_ `rwxr-xr-x` because
            // `std::fs::temp_directory_path` might not have `rwxrwxr-x` permissions.
            auto cache_dir_permissions = fs::status(cache_dir).permissions();
            auto expected_min_owner_perm = fs::perms::owner_all;
            auto expected_min_group_perm = fs::perms::group_read | fs::perms::group_exec;
            auto expected_min_others_perm = fs::perms::others_read | fs::perms::others_exec;

            REQUIRE((cache_dir_permissions & expected_min_owner_perm) == expected_min_owner_perm);
            REQUIRE((cache_dir_permissions & expected_min_group_perm) == expected_min_group_perm);
            REQUIRE((cache_dir_permissions & expected_min_others_perm) == expected_min_others_perm);

            if (supports_setgid_bit)
            {
                REQUIRE((cache_dir_permissions & fs::perms::set_gid) == fs::perms::set_gid);
            }
        }

    }

}
