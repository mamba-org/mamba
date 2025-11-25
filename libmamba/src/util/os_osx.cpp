// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <array>

#include <fmt/format.h>
#include <reproc++/run.hpp>

#include "mamba/util/os_osx.hpp"
#include "mamba/util/string.hpp"

namespace mamba::util
{
    auto osx_version() -> tl::expected<std::string, OSError>
    {
        // Note: we could also inspect /System/Library/CoreServices/SystemVersion.plist which is
        // an XML file that contains the same information.
        // However, then we'd either need an xml parser or some other crude method to read the data

        static constexpr auto args = std::array<std::string_view, 2>{ "sw_vers", "-productVersion" };

        auto out = std::string();
        auto err = std::string();

        auto [status, ec] = reproc::run(
            args,
            reproc::options{},
            reproc::sink::string(out),
            reproc::sink::string(err)
        );

        if (ec)
        {
            return tl::make_unexpected(
                OSError{ fmt::format(
                    R"(Could not find macOS version by calling "sw_vers -productVersion": {})",
                    ec.message()
                ) }
            );
        }

        return { std::string(util::strip(out)) };
    }
}
