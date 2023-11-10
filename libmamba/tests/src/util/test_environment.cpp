// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <doctest/doctest.h>

#include "mamba/util/environment.hpp"

using namespace mamba::util;

TEST_SUITE("util::environment")
{
    TEST_CASE("get_env")
    {
        CHECK_FALSE(get_env("VAR_THAT_DOES_NOT_EXIST_XYZ").has_value());
        CHECK(get_env("PATH").has_value());
    }

    TEST_CASE("set_env")
    {
        SUBCASE("ASCII")
        {
            const auto key = std::string(u8"VAR_THAT_DOES_NOT_EXIST_XYZ");
            const auto value1 = std::string(u8"VALUE");
            set_env(key, value1);
            CHECK_EQ(get_env(key), value1);
            const auto value2 = std::string(u8"VALUE_NEW");
            set_env(key, value2);
            CHECK_EQ(get_env(key), value2);
        }

        SUBCASE("UTF-8")
        {
            const auto key = std::string(u8"VAR_私のにほんごわへたです");
            const auto value1 = std::string(u8"😀");
            set_env(key, value1);
            CHECK_EQ(get_env(key), value1);
            const auto value2 = std::string(u8"🤗");
            set_env(key, value2);
            CHECK_EQ(get_env(key), value2);
        }
    }

    TEST_CASE("unset_env")
    {
        const auto key = std::string(u8"VAR_THAT_DOES_NOT_EXIST_ABC_😀");
        CHECK_FALSE(get_env(key).has_value());
        unset_env(key);
        CHECK_FALSE(get_env(key).has_value());
        set_env(key, "VALUE");
        CHECK(get_env(key).has_value());
        unset_env(key);
        CHECK_FALSE(get_env(key).has_value());
    }

    TEST_CASE("get_env_map")
    {
        auto environ = get_env_map();
        CHECK_GT(environ.size(), 0);
        CHECK_EQ(environ.count("VAR_THAT_MUST_NOT_EXIST_XYZ"), 0);
        CHECK_EQ(environ.count("PATH"), 1);

        const auto key = std::string(u8"VAR_私のにほHelloわへたです");
        const auto value = std::string(u8"😀");
        set_env(key, value);
        environ = get_env_map();
        CHECK_EQ(environ.at(key), value);
    }

    TEST_CASE("update_env_map")
    {
        const auto key_inexistant = std::string(u8"CONDA😀");
        const auto key_unchanged = std::string(u8"MAMBA😀");
        const auto key_changed = std::string(u8"PIXI😀");

        CHECK_FALSE(get_env(key_inexistant).has_value());
        CHECK_FALSE(get_env(key_unchanged).has_value());
        CHECK_FALSE(get_env(key_changed).has_value());

        const auto val_set_1 = std::string(u8"a😀");
        update_env_map({ { key_changed, val_set_1 }, { key_unchanged, val_set_1 } });
        CHECK_EQ(get_env(key_inexistant), std::nullopt);
        CHECK_EQ(get_env(key_unchanged), val_set_1);
        CHECK_EQ(get_env(key_changed), val_set_1);

        const auto val_set_2 = std::string(u8"b😀");
        update_env_map({ { key_changed, val_set_2 } });
        CHECK_EQ(get_env(key_inexistant), std::nullopt);
        CHECK_EQ(get_env(key_unchanged), val_set_1);
        CHECK_EQ(get_env(key_changed), val_set_2);
    }

    TEST_CASE("set_env_map")
    {
        const auto key_inexistant = std::string(u8"CONDA🤗");
        const auto key_unchanged = std::string(u8"MAMBA🤗");
        const auto key_changed = std::string(u8"PIXI🤗");

        CHECK_FALSE(get_env(key_inexistant).has_value());
        CHECK_FALSE(get_env(key_unchanged).has_value());
        CHECK_FALSE(get_env(key_changed).has_value());

        const auto val_set_1 = std::string(u8"a😀");
        set_env_map({ { key_changed, val_set_1 }, { key_unchanged, val_set_1 } });
        CHECK_EQ(get_env(key_inexistant), std::nullopt);
        CHECK_EQ(get_env(key_unchanged), val_set_1);
        CHECK_EQ(get_env(key_changed), val_set_1);

        const auto val_set_2 = std::string(u8"b😀");
        set_env_map({ { key_changed, val_set_2 } });
        CHECK_EQ(get_env(key_inexistant), std::nullopt);
        CHECK_EQ(get_env(key_unchanged), std::nullopt);  // Difference with update_env_map
        CHECK_EQ(get_env(key_changed), val_set_2);
    }
}
