// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <doctest/doctest.h>

#include "mamba/util/build.hpp"
#include "mamba/util/environment.hpp"

#include "mambatests.hpp"

using namespace mamba::util;

TEST_SUITE("util::environment")
{
    TEST_CASE("get_env")
    {
        const auto restore = mambatests::EnvironmentCleaner();

        CHECK_FALSE(get_env("VAR_THAT_DOES_NOT_EXIST_XYZ").has_value());
        CHECK(get_env("PATH").has_value());
    }

    TEST_CASE("set_env")
    {
        const auto restore = mambatests::EnvironmentCleaner();

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
        const auto restore = mambatests::EnvironmentCleaner();

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
        const auto restore = mambatests::EnvironmentCleaner();

        auto env = mamba::util::get_env_map();
        CHECK_GT(env.size(), 0);
        CHECK_EQ(env.count("VAR_THAT_MUST_NOT_EXIST_XYZ"), 0);
        CHECK_EQ(env.count("PATH"), 1);

        const auto key = std::string(u8"VAR_私のにほHelloわへたです");
        const auto value = std::string(u8"😀");
        set_env(key, value);
        env = get_env_map();
        CHECK_EQ(env.at(key), value);
    }

    TEST_CASE("update_env_map")
    {
        const auto restore = mambatests::EnvironmentCleaner();

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
        const auto restore = mambatests::EnvironmentCleaner();

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

    TEST_CASE("user_home_dir")
    {
        const auto restore = mambatests::EnvironmentCleaner();

        SUBCASE("default")
        {
            [[maybe_unused]] const auto home = user_home_dir();  // Must not raise error

            if (!on_win)
            {
                unset_env("HOME");
                CHECK_EQ(user_home_dir(), home);  // Fallback does not need $HOME
            }
        }

        SUBCASE("explicit")
        {
            if (on_win)
            {
                set_env("USERPROFILE", R"(D:\user\mamba)");
                CHECK_EQ(user_home_dir(), R"(D:\user\mamba)");

                unset_env("USERPROFILE");
                set_env("HOMEDRIVE", R"(D:\user\)");
                set_env("HOMEPATH", "mamba");
                CHECK_EQ(user_home_dir(), R"(D:\user\mamba)");
            }
            else
            {
                set_env("HOME", "/user/mamba");
                CHECK_EQ(user_home_dir(), "/user/mamba");
            }
        }
    }

    TEST_CASE("user_xdg")
    {
        const auto restore = mambatests::EnvironmentCleaner();

        SUBCASE("XDG environment variables")
        {
            update_env_map({
                { "XDG_CONFIG_HOME", "xconfig" },
                { "XDG_DATA_HOME", "xdata" },
                { "XDG_CACHE_HOME", "xcache" },
            });
            CHECK_EQ(user_config_dir(), mamba::fs::u8path("xconfig"));
            CHECK_EQ(user_data_dir(), mamba::fs::u8path("xdata"));
            CHECK_EQ(user_cache_dir(), mamba::fs::u8path("xcache"));
        }

        SUBCASE("Defaults")
        {
            if (!on_win)
            {
                set_env_map({ { "HOME", "/user/mamba" } });
                CHECK_EQ(user_config_dir(), mamba::fs::u8path("/user/mamba/.config"));
                CHECK_EQ(user_data_dir(), mamba::fs::u8path("/user/mamba/.local/share"));
                CHECK_EQ(user_cache_dir(), mamba::fs::u8path("/user/mamba/.cache"));
            }
        }
    }
}
