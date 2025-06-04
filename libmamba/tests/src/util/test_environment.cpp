// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <catch2/catch_all.hpp>

#include "mamba/util/build.hpp"
#include "mamba/util/encoding.hpp"
#include "mamba/util/environment.hpp"

#include "mambatests.hpp"

using namespace mamba::util;

namespace
{
    TEST_CASE("get_env", "[mamba::util]")
    {
        const auto restore = mambatests::EnvironmentCleaner();

        REQUIRE_FALSE(get_env("VAR_THAT_DOES_NOT_EXIST_XYZ").has_value());
        REQUIRE(get_env("PATH").has_value());
    }

    TEST_CASE("set_env")
    {
        const auto restore = mambatests::EnvironmentCleaner();

        SECTION("ASCII")
        {
            const auto key = to_utf8_std_string(u8"VAR_THAT_DOES_NOT_EXIST_XYZ");
            const auto value1 = to_utf8_std_string(u8"VALUE");
            set_env(key, value1);
            REQUIRE(get_env(key) == value1);
            const auto value2 = to_utf8_std_string(u8"VALUE_NEW");
            set_env(key, value2);
            REQUIRE(get_env(key) == value2);
        }

        SECTION("UTF-8")
        {
            const auto key = to_utf8_std_string(u8"VAR_私のにほんごわへたです");
            const auto value1 = to_utf8_std_string(u8"😀");
            set_env(key, value1);
            REQUIRE(get_env(key) == value1);
            const auto value2 = to_utf8_std_string(u8"🤗");
            set_env(key, value2);
            REQUIRE(get_env(key) == value2);
        }
    }

    TEST_CASE("unset_env", "[mamba::util]")
    {
        const auto restore = mambatests::EnvironmentCleaner();

        const auto key = to_utf8_std_string(u8"VAR_THAT_DOES_NOT_EXIST_ABC_😀");
        REQUIRE_FALSE(get_env(key).has_value());
        unset_env(key);
        REQUIRE_FALSE(get_env(key).has_value());
        set_env(key, "VALUE");
        REQUIRE(get_env(key).has_value());
        unset_env(key);
        REQUIRE_FALSE(get_env(key).has_value());
    }

    TEST_CASE("get_env_map", "[mamba::util]")
    {
        const auto restore = mambatests::EnvironmentCleaner();

        auto env = mamba::util::get_env_map();
        REQUIRE(env.size() > 0);
        REQUIRE(env.count("VAR_THAT_MUST_NOT_EXIST_XYZ") == 0);
        REQUIRE(env.count("PATH") == 1);

        const auto key = to_utf8_std_string(u8"VAR_私のにほHelloわへたです");
        const auto value = to_utf8_std_string(u8"😀");
        set_env(key, value);
        env = get_env_map();
        REQUIRE(env.at(key) == value);
    }

    TEST_CASE("update_env_map", "[mamba::util]")
    {
        const auto restore = mambatests::EnvironmentCleaner();

        const auto key_inexistent = to_utf8_std_string(u8"CONDA😀");
        const auto key_unchanged = to_utf8_std_string(u8"MAMBA😀");
        const auto key_changed = to_utf8_std_string(u8"PIXI😀");

        REQUIRE_FALSE(get_env(key_inexistent).has_value());
        REQUIRE_FALSE(get_env(key_unchanged).has_value());
        REQUIRE_FALSE(get_env(key_changed).has_value());

        const auto val_set_1 = to_utf8_std_string(u8"a😀");
        update_env_map({ { key_changed, val_set_1 }, { key_unchanged, val_set_1 } });
        REQUIRE(get_env(key_inexistent) == std::nullopt);
        REQUIRE(get_env(key_unchanged) == val_set_1);
        REQUIRE(get_env(key_changed) == val_set_1);

        const auto val_set_2 = to_utf8_std_string(u8"b😀");
        update_env_map({ { key_changed, val_set_2 } });
        REQUIRE(get_env(key_inexistent) == std::nullopt);
        REQUIRE(get_env(key_unchanged) == val_set_1);
        REQUIRE(get_env(key_changed) == val_set_2);
    }

    TEST_CASE("set_env_map", "[mamba::util]")
    {
        const auto restore = mambatests::EnvironmentCleaner();

        const auto key_inexistent = to_utf8_std_string(u8"CONDA🤗");
        const auto key_unchanged = to_utf8_std_string(u8"MAMBA🤗");
        const auto key_changed = to_utf8_std_string(u8"PIXI🤗");

        REQUIRE_FALSE(get_env(key_inexistent).has_value());
        REQUIRE_FALSE(get_env(key_unchanged).has_value());
        REQUIRE_FALSE(get_env(key_changed).has_value());

        const auto val_set_1 = to_utf8_std_string(u8"a😀");
        set_env_map({ { key_changed, val_set_1 }, { key_unchanged, val_set_1 } });
        REQUIRE(get_env(key_inexistent) == std::nullopt);
        REQUIRE(get_env(key_unchanged) == val_set_1);
        REQUIRE(get_env(key_changed) == val_set_1);

        const auto val_set_2 = to_utf8_std_string(u8"b😀");
        set_env_map({ { key_changed, val_set_2 } });
        REQUIRE(get_env(key_inexistent) == std::nullopt);
        REQUIRE(get_env(key_unchanged) == std::nullopt);  // Difference with update_env_map
        REQUIRE(get_env(key_changed) == val_set_2);
    }

    TEST_CASE("user_home_dir", "[mamba::util]")
    {
        const auto restore = mambatests::EnvironmentCleaner();

        SECTION("default")
        {
            [[maybe_unused]] const auto home = user_home_dir();  // Must not raise error

            if (!on_win)
            {
                unset_env("HOME");
                REQUIRE(user_home_dir() == home);  // Fallback does not need $HOME
            }
        }

        SECTION("explicit")
        {
            if (on_win)
            {
                set_env("USERPROFILE", R"(D:\user\mamba)");
                REQUIRE(user_home_dir() == R"(D:\user\mamba)");

                unset_env("USERPROFILE");
                set_env("HOMEDRIVE", R"(D:\user\)");
                set_env("HOMEPATH", "mamba");
                REQUIRE(user_home_dir() == R"(D:\user\mamba)");
            }
            else
            {
                set_env("HOME", "/user/mamba");
                REQUIRE(user_home_dir() == "/user/mamba");
            }
        }
    }

    TEST_CASE("user_xdg", "[mamba::util]")
    {
        const auto restore = mambatests::EnvironmentCleaner();

        SECTION("XDG environment variables")
        {
            update_env_map({
                { "XDG_CONFIG_HOME", "xconfig" },
                { "XDG_DATA_HOME", "xdata" },
                { "XDG_CACHE_HOME", "xcache" },
            });
            REQUIRE(user_config_dir() == "xconfig");
            REQUIRE(user_data_dir() == "xdata");
            REQUIRE(user_cache_dir() == "xcache");
        }

        SECTION("Defaults")
        {
            if (!on_win)
            {
                set_env_map({ { "HOME", "/user/mamba" } });
                REQUIRE(user_config_dir() == "/user/mamba/.config");
                REQUIRE(user_data_dir() == "/user/mamba/.local/share");
                REQUIRE(user_cache_dir() == "/user/mamba/.cache");
            }
        }
    }

    TEST_CASE("which_in", "[mamba::util]")
    {
        SECTION("Inexistent search dirs")
        {
            REQUIRE(which_in("echo", "/obviously/does/not/exist") == "");
        }

        SECTION("testing_libmamba_lock")
        {
            const auto test_exe = which_in(
                "testing_libmamba_lock",
                mambatests::testing_libmamba_lock_exe.parent_path()
            );
            REQUIRE(test_exe.stem() == "testing_libmamba_lock");
            REQUIRE(mamba::fs::exists(test_exe));
        }

        SECTION("testing_libmamba_lock.exe")
        {
            if (on_win)
            {
                const auto test_exe = which_in(
                    "testing_libmamba_lock.exe",
                    mambatests::testing_libmamba_lock_exe.parent_path()
                );
                REQUIRE(test_exe.stem() == "testing_libmamba_lock");
                REQUIRE(mamba::fs::exists(test_exe));
            }
        }
    }

    TEST_CASE("which", "[mamba::util]")
    {
        SECTION("echo")
        {
            const auto echo = which("echo");
            REQUIRE(echo.stem() == "echo");
            REQUIRE(mamba::fs::exists(echo));

            if (!on_win)
            {
                const auto dir = echo.parent_path();
                constexpr auto reasonable_locations = std::array{
                    "/bin",
                    "/sbin",
                    "/usr/bin",
                    "/usr/sbin",
                };
                REQUIRE(starts_with_any(echo.string(), reasonable_locations));
            }
        }

        SECTION("echo.exe")
        {
            if (on_win)
            {
                const auto echo = which("echo.exe");
                REQUIRE(echo.stem() == "echo");
                REQUIRE(mamba::fs::exists(echo));
            }
        }

        SECTION("Inexistent path")
        {
            REQUIRE(which("obviously-does-not-exist") == "");
        }
    }
}
