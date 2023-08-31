// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/specs/archive.hpp"
#include "mamba/util/string.hpp"

namespace mamba::specs
{

    auto has_archive_extension(std::string_view path) -> bool
    {
        for (const auto& ext : ARCHIVE_EXTENSIONS)
        {
            if (util::ends_with(path, ext))
            {
                return true;
            }
        }
        return false;
    }

    auto has_archive_extension(const fs::u8path& path) -> bool
    {
        if (path.has_filename() && path.has_extension())
        {
            const auto filename = path.filename().string();
            return has_archive_extension(std::string_view(filename));
        }
        return false;
    }

    auto strip_archive_extension(std::string_view path) -> std::string_view
    {
        for (const auto& ext : ARCHIVE_EXTENSIONS)
        {
            const auto stem = util::remove_suffix(path, ext);
            if (stem.size() != path.size())
            {
                return stem;
            }
        }
        return path;
    }

    auto strip_archive_extension(fs::u8path path) -> fs::u8path
    {
        if (path.has_filename() && path.has_extension())
        {
            const auto filename = path.filename().string();
            auto new_filename = strip_archive_extension(std::string_view(filename));
            path.replace_filename(new_filename);
        }
        return path;
    }
}
