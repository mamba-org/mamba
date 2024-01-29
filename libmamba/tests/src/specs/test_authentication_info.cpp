// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <doctest/doctest.h>

#include "mamba/specs/authentication_info.hpp"

using namespace mamba::specs;

TEST_SUITE("specs::authentication_info")
{
    TEST_CASE("URLWeakener")
    {
        const auto weakener = URLWeakener();

        SUBCASE("mamba.org/private/chan")
        {
            CHECK_EQ(weakener.make_first_key("mamba.org/private/chan"), "mamba.org/private/chan/");

            auto maybe_key = weakener.weaken_key("mamba.org/private/chan/");
            CHECK_EQ(maybe_key, "mamba.org/private/chan");
            maybe_key = weakener.weaken_key(maybe_key.value());
            CHECK_EQ(maybe_key, "mamba.org/private/");
            maybe_key = weakener.weaken_key(maybe_key.value());
            CHECK_EQ(maybe_key, "mamba.org/private");
            maybe_key = weakener.weaken_key(maybe_key.value());
            CHECK_EQ(maybe_key, "mamba.org/");
            maybe_key = weakener.weaken_key(maybe_key.value());
            CHECK_EQ(maybe_key, "mamba.org");
            maybe_key = weakener.weaken_key(maybe_key.value());
            CHECK_EQ(maybe_key, std::nullopt);
        }

        SUBCASE("mamba.org/private/chan/")
        {
            CHECK_EQ(weakener.make_first_key("mamba.org/private/chan"), "mamba.org/private/chan/");
        }
    }

    TEST_CASE("AuthticationDataBase")
    {
        SUBCASE("mamba.org")
        {
            auto db = AuthenticationDataBase{ { "mamba.org", BearerToken{ "mytoken" } } };

            CHECK(db.contains("mamba.org"));
            CHECK_FALSE(db.contains("mamba.org/"));

            CHECK(db.contains_weaken("mamba.org"));
            CHECK(db.contains_weaken("mamba.org/"));
            CHECK(db.contains_weaken("mamba.org/channel"));
            CHECK_FALSE(db.contains_weaken("repo.mamba.org"));
            CHECK_FALSE(db.contains_weaken("/folder"));
        }

        SUBCASE("mamba.org/")
        {
            auto db = AuthenticationDataBase{ { "mamba.org/", BearerToken{ "mytoken" } } };

            CHECK(db.contains("mamba.org/"));
            CHECK_FALSE(db.contains("mamba.org"));

            CHECK(db.contains_weaken("mamba.org"));
            CHECK(db.contains_weaken("mamba.org/"));
            CHECK(db.contains_weaken("mamba.org/channel"));
            CHECK_FALSE(db.contains_weaken("repo.mamba.org/"));
            CHECK_FALSE(db.contains_weaken("/folder"));
        }

        SUBCASE("mamba.org/channel")
        {
            auto db = AuthenticationDataBase{ { "mamba.org/channel", BearerToken{ "mytoken" } } };

            CHECK(db.contains("mamba.org/channel"));
            CHECK_FALSE(db.contains("mamba.org"));

            CHECK_FALSE(db.contains_weaken("mamba.org"));
            CHECK_FALSE(db.contains_weaken("mamba.org/"));
            CHECK(db.contains_weaken("mamba.org/channel"));
            CHECK_FALSE(db.contains_weaken("repo.mamba.org/"));
            CHECK_FALSE(db.contains_weaken("/folder"));
        }
    }
}
