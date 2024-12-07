// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <regex>

#include <catch2/catch_all.hpp>

#include "mamba/util/build.hpp"
#include "mamba/util/os_unix.hpp"

using namespace mamba;
using namespace mamba::util;

TEST_CASE("unix_name_version")
{
    const auto maybe_name_version = unix_name_version();
    if (util::on_linux)
    {
        REQUIRE(maybe_name_version.has_value());
        REQUIRE(maybe_name_version.value().first == "Linux");
        static const auto version_regex = std::regex(R"r(\d+\.\d+\.\d+)r");
        REQUIRE(std ::regex_match(maybe_name_version.value().second, version_regex));
    }
    else if (util::on_mac)
    {
        REQUIRE(maybe_name_version.has_value());
        REQUIRE(maybe_name_version.value().first == "Darwin");
        static const auto version_regex = std::regex(R"r(\d+\.\d+\.\d+)r");
        REQUIRE(std ::regex_match(maybe_name_version.value().second, version_regex));
    }
    else
    {
        REQUIRE_FALSE(maybe_name_version.has_value());
    }
}
