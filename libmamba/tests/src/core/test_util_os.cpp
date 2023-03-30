// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <gtest/gtest.h>

#include <string>

#include "mamba/core/util_os.hpp"


#ifdef _WIN32

namespace
{
    const std::wstring text_utf16 = L"Hello, I am Joël. 私のにほんごわへたです";
    const std::string text_utf8 = u8"Hello, I am Joël. 私のにほんごわへたです";
}

TEST(to_utf8, basic_unicode_conversion)
{
    auto result = mamba::to_utf8(text_utf16);
    EXPECT_EQ(text_utf8, result);
}

TEST(to_windows_unicode, basic_unicode_conversion)
{
    auto result = mamba::to_windows_unicode(text_utf8);
    EXPECT_EQ(text_utf16, result);
}

#endif

