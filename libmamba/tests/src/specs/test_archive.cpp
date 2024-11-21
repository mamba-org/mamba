// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <catch2/catch_all.hpp>

#include "mamba/specs/archive.hpp"

using namespace mamba;
using namespace mamba::specs;

namespace
{
    TEST_CASE("has_archive_extension")
    {
        for (const auto& no_ext_path : {
                 "",
                 "hello",
                 "soupsieve-2.3.2.post1-pyhd8ed1ab_0.tar",
                 "soupsieve-2.3.2.post1-pyhd8ed1ab_0.bz2",
                 "/folder.tar.bz2/filename.txt",
             })
        {
            CAPTURE(std::string_view(no_ext_path));
            REQUIRE_FALSE(has_archive_extension(no_ext_path));
            REQUIRE_FALSE(has_archive_extension(fs::u8path(no_ext_path)));
        }

        for (const auto& ext_path : {
                 "soupsieve-2.3.2.post1-pyhd8ed1ab_0.tar.bz2",
                 "folder/soupsieve-2.3.2.post1-pyhd8ed1ab_0.tar.bz2",
             })
        {
            CAPTURE(std::string_view(ext_path));
            REQUIRE(has_archive_extension(ext_path));
            REQUIRE(has_archive_extension(fs::u8path(ext_path)));
        }
    }

    TEST_CASE("strip_archive_extension")
    {
        for (const auto& no_ext_path : {
                 "",
                 "hello",
                 "soupsieve-2.3.2.post1-pyhd8ed1ab_0.tar",
                 "soupsieve-2.3.2.post1-pyhd8ed1ab_0.bz2",
                 "/folder.tar.bz2/filename.txt",
             })
        {
            CAPTURE(std::string_view(no_ext_path));
            REQUIRE(strip_archive_extension(no_ext_path) == no_ext_path);
            REQUIRE(strip_archive_extension(fs::u8path(no_ext_path)) == no_ext_path);
        }

        REQUIRE(
            strip_archive_extension("soupsieve-2.3.2.post1-pyhd8ed1ab_0.tar.bz2")
            == "soupsieve-2.3.2.post1-pyhd8ed1ab_0"
        );
        REQUIRE(
            strip_archive_extension("folder/soupsieve-2.3.2.post1-pyhd8ed1ab_0.tar.bz2")
            == "folder/soupsieve-2.3.2.post1-pyhd8ed1ab_0"
        );

        REQUIRE(
            strip_archive_extension(fs::u8path("soupsieve-2.3.2.post1-pyhd8ed1ab_0.tar.bz2"))
            == "soupsieve-2.3.2.post1-pyhd8ed1ab_0"
        );
        REQUIRE(
            strip_archive_extension(fs::u8path("folder/soupsieve-2.3.2.post1-pyhd8ed1ab_0.tar.bz2"))
            == "folder/soupsieve-2.3.2.post1-pyhd8ed1ab_0"
        );
    }
}
