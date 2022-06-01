#include <gtest/gtest.h>

#include <vector>

#include "mamba/core/mamba_fs.hpp"
#include "mamba/core/util.hpp"
#include "mamba/core/util_scope.hpp"

namespace mamba
{
    TEST(u8path, normalized_separators)
    {
        static constexpr auto value = u8"a/b/c";
        std::filesystem::path x{ value };
        const auto y = fs::normalized_separators(x);
#if defined(_WIN32)
        ASSERT_EQ(y.u8string(), u8R"(a\b\c)");
#else
        ASSERT_EQ(y.u8string(), value);
#endif
    }

    TEST(u8path, normalized_separators_unicode)
    {
        static constexpr auto value = u8"日本語";
        const std::filesystem::path x = fs::from_utf8(value);
        ASSERT_EQ(x.u8string(), u8"日本語");  // check assumption
        const auto y = fs::normalized_separators(x);
        ASSERT_EQ(y.u8string(), u8"日本語");
    }

    TEST(u8path, consistent_encoding)
    {
        const auto utf8_string = u8"日本語";
        const fs::u8path filename(utf8_string);
        const auto str = filename.string();
        EXPECT_EQ(str, utf8_string);

        const fs::u8path file_path = fs::temp_directory_path() / filename;
        EXPECT_EQ(file_path.filename().string(), utf8_string);

        const auto std_path = file_path.std_path();
        EXPECT_EQ(std_path.filename().u8string(), utf8_string);
    }

    TEST(u8path, string_stream_encoding)
    {
        const auto utf8_string = u8"日本語";
        const std::string quoted_utf8_string = std::string("\"") + utf8_string + std::string("\"");
        const fs::u8path filename(utf8_string);
        std::stringstream stream;
        stream << filename;
        EXPECT_EQ(stream.str(), quoted_utf8_string);

        fs::u8path path_read;
        stream.seekg(0);
        stream >> path_read;
        EXPECT_EQ(path_read.string(), utf8_string);
    }

    TEST(u8path, directory_iteration)
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
            EXPECT_EQ(first_entry.path(), file_path.parent_path());
            auto secibd_entry = *(++it);
            EXPECT_EQ(secibd_entry.path(), file_path);
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
            EXPECT_EQ(entries_found, expected_entries);
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
            EXPECT_EQ(entries_found, expected_entries);
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
            EXPECT_EQ(entries_found, expected_entries);
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
            EXPECT_EQ(entries_found, expected_entries);
        }
    }

    TEST(u8path, long_paths)
    {
        const auto tmp_dir = fs::temp_directory_path() / "mamba_fs_long_path";

        fs::u8path long_path = tmp_dir;
        for (int i = 0; i < 42; ++i)
            long_path /= u8"some_very_long_prefix";

        mamba::on_scope_exit _([&] { fs::remove_all(long_path); });
        fs::create_directories(long_path);
    }

#if defined(_WIN32)
    TEST(u8path, append_maintains_slash_type)
    {
        const fs::u8path path = u8R"(a/b/c/d)";
        const auto path_1 = path / u8R"(e\f\g)";
        EXPECT_EQ(path_1.string(), u8R"(a\b\c\d\e\f\g)");
    }
#endif

}
