#include <gtest/gtest.h>

#include <vector>

#include "mamba/core/mamba_fs.hpp"

namespace mamba
{
    TEST(u8path, consistent_encoding)
    {
        const auto utf8_string = u8"日本語";
        const fs::u8path filename(utf8_string);
        EXPECT_EQ(filename.string(), utf8_string);

        const fs::u8path file_path = fs::temp_directory_path() / filename;
        EXPECT_EQ(file_path.filename().string(), utf8_string);
    }

    TEST(u8path, string_stream_encoding)
    {
        const auto utf8_string = u8"日本語";
        const fs::u8path filename(utf8_string);
        std::stringstream stream;
        stream << filename;
        EXPECT_EQ(stream.str(), utf8_string);

        fs::u8path path_read;
        stream.seekg(0);
        stream >> path_read;
        EXPECT_EQ(path_read.string(), utf8_string);
    }

    TEST(u8path, directory_iteration)
    {
        const auto tmp_dir = fs::temp_directory_path() / "mamba_fs_iteration";
        const auto file_dir = tmp_dir / "kikoo" / "lol" / u8"日本語";
        const auto file_path = file_dir / u8"joël";
        fs::create_directories(file_dir);
        {
            std::ofstream file(file_path.std_path(), std::ios::binary | std::ios::trunc);
            file << u8"日本語";
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
    }
}
