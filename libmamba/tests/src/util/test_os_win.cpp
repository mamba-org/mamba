// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifdef _WIN32

#include <string>

#include <doctest/doctest.h>

#include "mamba/util/os_win.hpp"

using namespace mamba::util;

TEST_SUITE("uti::os_win")
{
    TEST_CASE("")
    {
        const std::wstring text_utf16 = L"Hello, I am Joël. 私のにほんごわへたです";
        const std::string text_utf8 = u8"Hello, I am Joël. 私のにほんごわへたです";

        SUBCASE("utf8_to_windows_encoding")
        {
            CHECK_EQ(utf8_to_windows_encoding(text_utf8), text_utf16);
        }
    }
}

#endif
