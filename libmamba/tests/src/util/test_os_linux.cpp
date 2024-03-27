// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <regex>

#include <doctest/doctest.h>

#include "mamba/util/build.hpp"
#include "mamba/util/os_linux.hpp"

using namespace mamba;
using namespace mamba::util;

TEST_SUITE("util::os_linux")
{
    TEST_CASE("linux_version")
    {
        const auto maybe_version = linux_version();
        if (util::on_win)
        {
            CHECK_FALSE(maybe_version.has_value());
        }
        else
        {
            REQUIRE(maybe_version.has_value());
            static const auto version_regex = std::regex(R"r(\d+\.\d+\.\d+)r");
            CHECK(std ::regex_match(maybe_version.value(), version_regex));
        }
    }
}
