// Copyright (c) 2026, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/core/util.hpp"

#include "utils.hpp"

namespace mambatests
{
    auto read_explicit_urls(const mamba::fs::u8path& path) -> std::vector<std::string>
    {
        std::vector<std::string> urls;
        for (const auto& line : mamba::read_lines(path))
        {
            if (line.starts_with("http://") || line.starts_with("https://"))
            {
                urls.push_back(line);
            }
        }
        return urls;
    }
}
