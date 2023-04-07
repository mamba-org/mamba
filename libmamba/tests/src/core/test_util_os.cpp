﻿// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <string>

#include <doctest/doctest.h>

#include "mamba/core/util_os.hpp"

#ifdef _WIN32

namespace
{
    const std::wstring text_utf16 = L"Hello, I am Joël. 私のにほんごわへたです";
    const std::string text_utf8 = u8"Hello, I am Joël. 私のにほんごわへたです";
}

TEST_SUITE("basic_unicode_conversion")
{
    TEST_CASE("to_utf8")
    {
        auto result = mamba::to_utf8(text_utf16);
        CHECK_EQ(text_utf8, result);
    }

    TEST_CASE("to_windows_unicode")
    {
        auto result = mamba::to_windows_unicode(text_utf8);
        CHECK_EQ(text_utf16, result);
    }
}

#endif
