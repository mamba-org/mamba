// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <regex>

#include <fmt/format.h>

#include "mamba/util/os_linux.hpp"
#include "mamba/util/os_unix.hpp"

namespace mamba::util
{
    auto linux_version() -> tl::expected<std::string, OSError>
    {
        return unix_name_version().and_then(
            [](auto&& name_version) -> tl::expected<std::string, OSError>
            {
                if (name_version.first != "Linux")
                {
                    return tl::make_unexpected(OSError{
                        fmt::format(R"(OS "{}" is not Linux)", name_version.first),
                    });
                }
                return { std::forward<decltype(name_version)>(name_version).second };
            }
        );
    }
}
