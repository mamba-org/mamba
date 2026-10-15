// Copyright (c) 2022, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <catch2/catch_all.hpp>

#include "mamba/core/util.hpp"
#include "mamba/validation/tools.hpp"

using namespace mamba;
using namespace mamba::validation;

namespace
{
    TEST_CASE("sha256sum and md5sum")
    {
        auto tmp = TemporaryFile();
        auto f = mamba::open_ofstream(tmp.path());
        f << "test";
        f.close();
        auto sha256 = sha256sum(tmp.path());
        REQUIRE(sha256 == "9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08");

        auto md5 = md5sum(tmp.path());
        REQUIRE(md5 == "098f6bcd4621d373cade4e832627b4f6");
    }
}
