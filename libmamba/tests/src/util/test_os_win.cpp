// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.


#include <regex>
#include <string>

#include <doctest/doctest.h>

#include "mamba/util/build.hpp"
#include "mamba/util/os_win.hpp"

using namespace mamba;
using namespace mamba::util;

TEST_SUITE("util::os_win")
{
    TEST_CASE("utf8" * doctest::skip(!util::on_win))
    {
        const std::wstring text_utf16 = L"Hello, I am Joël. 私のにほんごわへたです";
        const std::string text_utf8 = u8"Hello, I am Joël. 私のにほんごわへたです";

        SUBCASE("utf8_to_windows_encoding")
        {
            CHECK_EQ(utf8_to_windows_encoding(""), L"");
            CHECK_EQ(utf8_to_windows_encoding(text_utf8), text_utf16);
        }

        SUBCASE("windows_encoding_to_utf8")
        {
            CHECK_EQ(windows_encoding_to_utf8(L""), "");
            CHECK_EQ(windows_encoding_to_utf8(text_utf16), text_utf8);
        }
    }

    TEST_CASE("windows_version")
    {
        const auto maybe_version = windows_version();
        if (util::on_win)
        {
            REQUIRE(maybe_version.has_value());
            static const auto version_regex = std::regex(R"r(\d+\.\d+\.\d+)r");
            CHECK(std::regex_match(maybe_version.value(), version_regex));
        }
        else
        {
            CHECK_FALSE(maybe_version.has_value());
        }
    }
}
