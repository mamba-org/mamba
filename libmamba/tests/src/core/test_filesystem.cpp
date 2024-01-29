// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <vector>

#include <doctest/doctest.h>

#include "mamba/core/util.hpp"
#include "mamba/core/util_scope.hpp"
#include "mamba/fs/filesystem.hpp"

namespace mamba
{
    TEST_SUITE("u8path")
    {
        TEST_CASE("normalized_separators")
        {
            static constexpr auto value = u8"a/b/c";
            std::filesystem::path x{ value };
            const auto y = fs::normalized_separators(x);
#if defined(_WIN32)
            REQUIRE_EQ(y.u8string(), u8R"(a\b\c)");
#else
            REQUIRE_EQ(y.u8string(), value);
#endif
        }

        TEST_CASE("normalized_separators_unicode")
        {
            static constexpr auto value = u8"日本語";
            const std::filesystem::path x = fs::from_utf8(value);
            REQUIRE_EQ(x.u8string(), u8"日本語");  // check assumption
            const auto y = fs::normalized_separators(x);
            REQUIRE_EQ(y.u8string(), u8"日本語");
        }

        TEST_CASE("consistent_encoding")
        {
            const auto utf8_string = u8"日本語";
            const fs::u8path filename(utf8_string);
            const auto str = filename.string();
            CHECK_EQ(str, utf8_string);

            const fs::u8path file_path = fs::temp_directory_path() / filename;
            CHECK_EQ(file_path.filename().string(), utf8_string);

            const auto std_path = file_path.std_path();
            CHECK_EQ(std_path.filename().u8string(), utf8_string);
        }

        TEST_CASE("string_stream_encoding")
        {
            const auto utf8_string = u8"日本語";
            const std::string quoted_utf8_string = std::string("\"") + utf8_string
                                                   + std::string("\"");
            const fs::u8path filename(utf8_string);
            std::stringstream stream;
            stream << filename;
            CHECK_EQ(stream.str(), quoted_utf8_string);

            fs::u8path path_read;
            stream.seekg(0);
            stream >> path_read;
            CHECK_EQ(path_read.string(), utf8_string);
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
                CHECK_EQ(first_entry.path(), file_path.parent_path());
                auto secibd_entry = *(++it);
                CHECK_EQ(secibd_entry.path(), file_path);
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
                CHECK_EQ(entries_found, expected_entries);
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
                CHECK_EQ(entries_found, expected_entries);
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
                CHECK_EQ(entries_found, expected_entries);
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
                CHECK_EQ(entries_found, expected_entries);
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
            CHECK_EQ(path_1.string(), u8R"(a\b\c\d\e\f\g)");
        }
#endif
    }

    TEST_SUITE("filesystem")
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
            CHECK_EQ(
                (fs::status(readonly_file_path).permissions() & fs::perms::owner_write),
                fs::perms::none
            );
            CHECK_EQ(
                (fs::status(readonly_file_path).permissions() & fs::perms::group_write),
                fs::perms::none
            );

            // removing should still work.
            CHECK(fs::exists(readonly_file_path));
            fs::remove(readonly_file_path);
            CHECK_FALSE(fs::exists(readonly_file_path));
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
                    CHECK_EQ(
                        (fs::status(readonly_file_path).permissions() & fs::perms::owner_write),
                        fs::perms::none
                    );
                    CHECK_EQ(
                        (fs::status(readonly_file_path).permissions() & fs::perms::group_write),
                        fs::perms::none
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

            CHECK(fs::exists(tmp_dir));
            fs::remove_all(tmp_dir);
            CHECK_FALSE(fs::exists(tmp_dir));
        }
    }

}
