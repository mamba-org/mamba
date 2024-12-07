// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <catch2/catch_all.hpp>

#include "mamba/specs/authentication_info.hpp"

using namespace mamba::specs;

TEST_CASE("URLWeakener")
{
    const auto weakener = URLWeakener();

    SECTION("mamba.org/private/chan")
    {
        REQUIRE(weakener.make_first_key("mamba.org/private/chan") == "mamba.org/private/chan/");

        auto maybe_key = weakener.weaken_key("mamba.org/private/chan/");
        REQUIRE(maybe_key == "mamba.org/private/chan");
        maybe_key = weakener.weaken_key(maybe_key.value());
        REQUIRE(maybe_key == "mamba.org/private/");
        maybe_key = weakener.weaken_key(maybe_key.value());
        REQUIRE(maybe_key == "mamba.org/private");
        maybe_key = weakener.weaken_key(maybe_key.value());
        REQUIRE(maybe_key == "mamba.org/");
        maybe_key = weakener.weaken_key(maybe_key.value());
        REQUIRE(maybe_key == "mamba.org");
        maybe_key = weakener.weaken_key(maybe_key.value());
        REQUIRE(maybe_key == std::nullopt);
    }

    SECTION("mamba.org/private/chan/")
    {
        REQUIRE(weakener.make_first_key("mamba.org/private/chan") == "mamba.org/private/chan/");
    }
}

TEST_CASE("AuthticationDataBase")
{
    SECTION("mamba.org")
    {
        auto db = AuthenticationDataBase{ { "mamba.org", BearerToken{ "mytoken" } } };

        REQUIRE(db.contains("mamba.org"));
        REQUIRE_FALSE(db.contains("mamba.org/"));

        REQUIRE(db.contains_weaken("mamba.org"));
        REQUIRE(db.contains_weaken("mamba.org/"));
        REQUIRE(db.contains_weaken("mamba.org/channel"));
        REQUIRE_FALSE(db.contains_weaken("repo.mamba.org"));
        REQUIRE_FALSE(db.contains_weaken("/folder"));
    }

    SECTION("mamba.org/")
    {
        auto db = AuthenticationDataBase{ { "mamba.org/", BearerToken{ "mytoken" } } };

        REQUIRE(db.contains("mamba.org/"));
        REQUIRE_FALSE(db.contains("mamba.org"));

        REQUIRE(db.contains_weaken("mamba.org"));
        REQUIRE(db.contains_weaken("mamba.org/"));
        REQUIRE(db.contains_weaken("mamba.org/channel"));
        REQUIRE_FALSE(db.contains_weaken("repo.mamba.org/"));
        REQUIRE_FALSE(db.contains_weaken("/folder"));
    }

    SECTION("mamba.org/channel")
    {
        auto db = AuthenticationDataBase{ { "mamba.org/channel", BearerToken{ "mytoken" } } };

        REQUIRE(db.contains("mamba.org/channel"));
        REQUIRE_FALSE(db.contains("mamba.org"));

        REQUIRE_FALSE(db.contains_weaken("mamba.org"));
        REQUIRE_FALSE(db.contains_weaken("mamba.org/"));
        REQUIRE(db.contains_weaken("mamba.org/channel"));
        REQUIRE_FALSE(db.contains_weaken("repo.mamba.org/"));
        REQUIRE_FALSE(db.contains_weaken("/folder"));
    }
}
