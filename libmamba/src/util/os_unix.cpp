// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <regex>

#include <fmt/format.h>

#include "mamba/util/os_unix.hpp"

#ifndef _WIN32
#include <sys/utsname.h>
#endif

namespace mamba::util
{
#ifndef _WIN32

    auto unix_name_version() -> tl::expected<std::pair<std::string, std::string>, OSError>
    {
        struct ::utsname uname_result = {};
        const auto ret = ::uname(&uname_result);
        if (ret != 0)
        {
            return tl::make_unexpected(
                OSError{ fmt::format(
                    "Error calling uname: {}",
                    std::system_error(errno, std::generic_category()).what()
                ) }
            );
        }

        static const auto re = std::regex(R"r(([0-9]+\.[0-9]+\.[0-9]+)(?:-.*)?)r");

        if (auto m = std::cmatch(); std::regex_search(uname_result.release, m, re) && (m.size() == 2))
        {
            return { { uname_result.sysname, std::move(m)[1].str() } };
        }

        return tl::make_unexpected(
            OSError{ fmt::format(
                R"(Could not parse Linux version in uname output "{}")",
                uname_result.release
            ) }
        );
    }

#else

    auto unix_name_version() -> tl::expected<std::pair<std::string, std::string>, OSError>
    {
        return tl::make_unexpected(OSError{ "Cannot get Linux version on this system" });
    }

#endif
}
