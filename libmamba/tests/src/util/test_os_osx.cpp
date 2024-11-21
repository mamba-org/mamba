// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <regex>

#include <catch2/catch_all.hpp>

#include "mamba/util/build.hpp"
#include "mamba/util/os_osx.hpp"

using namespace mamba;
using namespace mamba::util;

namespace
{
    TEST_CASE("osx_version")
    {
        const auto maybe_version = osx_version();
        if (util::on_mac)
        {
            REQUIRE(maybe_version.has_value());
            // The version would be a sequence:
            // 'x.x' or 'x.x.x'
            // with 'x' matching one or more digits
            static const auto version_regex = std::regex(R"r(\d+\.\d+(\.\d+)?)r");
            REQUIRE(std ::regex_match(maybe_version.value(), version_regex));
        }
        else
        {
            REQUIRE_FALSE(maybe_version.has_value());
        }
    }
}
