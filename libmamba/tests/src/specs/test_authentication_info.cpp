// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <doctest/doctest.h>

#include "mamba/specs/authentication_info.hpp"

using namespace mamba::specs;

TEST_SUITE("specs::AuthticationDataBase")
{
    TEST_CASE("mamba.org")
    {
        auto db = AuthenticationDataBase{ { "mamba.org", BearerToken{ "mytoken" } } };

        CHECK(db.contains("mamba.org"));
        CHECK_FALSE(db.contains("mamba.org/"));

        CHECK(db.contains_compatible("mamba.org"));
        CHECK(db.contains_compatible("mamba.org/"));
        CHECK(db.contains_compatible("mamba.org/channel"));
        CHECK_FALSE(db.contains_compatible("repo.mamba.org"));
        CHECK_FALSE(db.contains_compatible("/folder"));
    }

    TEST_CASE("mamba.org/")
    {
        auto db = AuthenticationDataBase{ { "mamba.org/", BearerToken{ "mytoken" } } };

        CHECK(db.contains("mamba.org/"));
        CHECK_FALSE(db.contains("mamba.org"));

        CHECK(db.contains_compatible("mamba.org"));
        CHECK(db.contains_compatible("mamba.org/"));
        CHECK(db.contains_compatible("mamba.org/channel"));
        CHECK_FALSE(db.contains_compatible("repo.mamba.org/"));
        CHECK_FALSE(db.contains_compatible("/folder"));
    }

    TEST_CASE("mamba.org/channel")
    {
        auto db = AuthenticationDataBase{ { "mamba.org/channel", BearerToken{ "mytoken" } } };

        CHECK(db.contains("mamba.org/channel"));
        CHECK_FALSE(db.contains("mamba.org"));

        CHECK_FALSE(db.contains_compatible("mamba.org"));
        CHECK_FALSE(db.contains_compatible("mamba.org/"));
        CHECK(db.contains_compatible("mamba.org/channel"));
        CHECK_FALSE(db.contains_compatible("repo.mamba.org/"));
        CHECK_FALSE(db.contains_compatible("/folder"));
    }
}
