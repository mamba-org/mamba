// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.


#ifdef _WIN32

#include <cassert>
#include <mutex>
#include <stdexcept>

#include <fmt/format.h>
#include <Shlobj.h>
#include <Windows.h>

#include "mamba/util/environment.hpp"
#include "mamba/util/os_win.hpp"
#include "mamba/util/string.hpp"

namespace mamba::util
{
    namespace
    {

        // See:
        // https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/getenv-s-wgetenv-s?view=msvc-170
        std::mutex env_mutex = {};

    }

    auto get_env(const std::string& key) -> std::optional<std::string>
    {
        const auto on_failed = [&](auto error_code)
        {
            throw std::runtime_error(
                fmt::format(R"(Failed to acquire environment variable "{}" : errcode = {})", key, error_code)

            );
        };

        std::scoped_lock ready_to_execute{ env_mutex };  // Calls to getenv_s kinds of
                                                         // functions are not thread-safe, this
                                                         // is to prevent related issues.

        const std::wstring unicode_key = utf8_to_windows_encoding(key);

        std::size_t required_size = 0;
        auto error_code = ::_wgetenv_s(&required_size, nullptr, 0, unicode_key.c_str());
        if (error_code != 0)
        {
            on_failed(error_code);
        }

        if (required_size == 0)  // The value doesn't exist.
        {
            return {};
        }

        std::wstring value(required_size, L'?');  // Note: The required size implies a `\0`
                                                  // but basic_string doesn't.
        error_code = ::_wgetenv_s(&required_size, value.data(), value.size(), unicode_key.c_str());
        if (error_code != 0)
        {
            on_failed(error_code);
        }

        value.pop_back();  // Remove the `\0` that was written in, otherwise any future
                           // concatenation will fail.
        return { windows_encoding_to_utf8(value) };
    }

    void set_env(const std::string& key, const std::string& value)
    {
        std::scoped_lock ready_to_execute{ env_mutex };  // Calls to getenv_s kinds of
                                                         // functions are not thread-safe, this
                                                         // is to prevent related issues.

        const std::wstring unicode_key = utf8_to_windows_encoding(key);
        const std::wstring unicode_value = utf8_to_windows_encoding(value);
        const auto res = ::_wputenv_s(unicode_key.c_str(), unicode_value.c_str());
        if (res != 0)
        {
            throw std::runtime_error(fmt::format(
                R"(Could not set environment variable "{}" to "{}" : {})",
                key,
                value,
                ::GetLastError()
            ));
        }
    }

    void unset_env(const std::string& key)
    {
        set_env(key, "");
    }

    namespace
    {
        struct Environ
        {
            Environ()
            {
                ptr = GetEnvironmentStringsW();
                if (ptr == nullptr)
                {
                    throw std::runtime_error("Fail to get environment");
                }
            }

            ~Environ()
            {
                FreeEnvironmentStringsW(ptr);
            }

            wchar_t* ptr = nullptr;
        };
    }

    auto get_env_map() -> environment_map
    {
        static constexpr auto npos = std::wstring_view::npos;

        auto env = environment_map();

        auto raw_env = Environ();

        wchar_t* current = raw_env.ptr;
        while (*current != L'\0')
        {
            const auto expr = std::wstring_view(current);
            const auto pos = expr.find(L'=');
            assert(pos != npos);
            std::string key = windows_encoding_to_utf8(expr.substr(0, pos));
            if (!key.empty())
            {
                std::string value = windows_encoding_to_utf8(
                    (pos != npos) ? expr.substr(pos + 1) : L""
                );
                env.emplace(std::move(key), std::move(value));
            }
            current += expr.size() + 1;
        }

        return env;
    }

    auto user_home_dir() -> std::string
    {
        if (auto maybe_home = get_env("USERPROFILE").value_or(""); !maybe_home.empty())
        {
            return maybe_home;
        }

        auto maybe_home = util::concat(
            get_env("HOMEDRIVE").value_or(""),
            get_env("HOMEPATH").value_or("")
        );

        if (maybe_home.empty())
        {
            throw std::runtime_error(
                "Cannot determine HOME (checked USERPROFILE, HOMEDRIVE and HOMEPATH env vars)"
            );
        }

        return maybe_home;
    }

    auto user_config_dir() -> std::string
    {
        if (auto maybe_dir = get_env("XDG_CONFIG_HOME").value_or(""); !maybe_dir.empty())
        {
            return maybe_dir;
        }
        return util::get_windows_known_user_folder(WindowsKnowUserFolder::RoamingAppData);
    }

    auto user_data_dir() -> std::string
    {
        if (auto maybe_dir = get_env("XDG_DATA_HOME").value_or(""); !maybe_dir.empty())
        {
            return maybe_dir;
        }
        return util::get_windows_known_user_folder(WindowsKnowUserFolder::RoamingAppData);
    }

    auto user_cache_dir() -> std::string
    {
        if (auto maybe_dir = get_env("XDG_CACHE_HOME").value_or(""); !maybe_dir.empty())
        {
            return maybe_dir;
        }
        return util::get_windows_known_user_folder(WindowsKnowUserFolder::LocalAppData);
    }

    auto which_system(std::string_view exe) -> fs::u8path
    {
        return "";
    }

    constexpr auto exec_extension() -> std::string_view
    {
        return ".exe";
    };
}

#else  // #ifdef _WIN32

#include <cstdlib>
#include <stdexcept>
#include <vector>

#include <fmt/format.h>
#include <pwd.h>
#include <unistd.h>

#include "mamba/util/environment.hpp"
#include "mamba/util/path_manip.hpp"

extern "C"
{
    extern char** environ;  // Unix defined
}

namespace mamba::util
{
    auto get_env(const std::string& key) -> std::optional<std::string>
    {
        if (const char* val = std::getenv(key.c_str()))
        {
            return val;
        }
        return {};
    }

    void set_env(const std::string& key, const std::string& value)
    {
        const auto result = ::setenv(key.c_str(), value.c_str(), 1);
        if (result != 0)
        {
            throw std::runtime_error(
                fmt::format(R"(Could not set environment variable "{}" to "{}")", key, value)
            );
        }
    }

    void unset_env(const std::string& key)
    {
        const auto res = ::unsetenv(key.c_str());
        if (res != 0)
        {
            throw std::runtime_error(fmt::format(R"(Could not unset environment variable "{}")", key));
        }
    }

    auto get_env_map() -> environment_map
    {
        // To keep the same signature between Unix and Windows (which is more convenient
        // to work with), we have to copy strings.
        // A more advanced slution could wrap a platform specific solution in a common return type.
        auto env = environment_map();
        for (std::size_t i = 0; environ[i]; ++i)
        {
            const auto expr = std::string_view(environ[i]);
            const auto pos = expr.find('=');
            env.emplace(
                expr.substr(0, pos),
                (pos != expr.npos) ? std::string(expr.substr(pos + 1)) : ""

            );
        }
        return env;
    }

    auto user_home_dir() -> std::string
    {
        if (auto maybe_home = get_env("HOME").value_or(""); !maybe_home.empty())
        {
            return maybe_home;
        }
        if (const auto* user = ::getpwuid(::getuid()))
        {
            if (const char* maybe_home = user->pw_dir)
            {
                return maybe_home;
            }
        }
        throw std::runtime_error("HOME not set.");
    }

    auto user_config_dir() -> std::string
    {
        if (auto maybe_dir = get_env("XDG_CONFIG_HOME").value_or(""); !maybe_dir.empty())
        {
            return maybe_dir;
        }
        return path_concat(user_home_dir(), ".config");
    }

    auto user_data_dir() -> std::string
    {
        if (auto maybe_dir = get_env("XDG_DATA_HOME").value_or(""); !maybe_dir.empty())
        {
            return maybe_dir;
        }
        return path_concat(user_home_dir(), ".local/share");
    }

    auto user_cache_dir() -> std::string
    {
        if (auto maybe_dir = get_env("XDG_CACHE_HOME").value_or(""); !maybe_dir.empty())
        {
            return maybe_dir;
        }
        return path_concat(user_home_dir(), ".cache");
    }

    auto which_system(std::string_view exe) -> fs::u8path
    {
        const auto n = ::confstr(_CS_PATH, nullptr, static_cast<std::size_t>(0));
        auto pathbuf = std::vector<char>(n, '\0');
        ::confstr(_CS_PATH, pathbuf.data(), n);
        return which_in(exe, std::string_view(pathbuf.data(), n));
    }

    constexpr auto exec_extension() -> std::string_view
    {
        return "";
    };
}

#endif  // #ifdef _WIN32

#include "mamba/util/environment.hpp"
#include "mamba/util/string.hpp"

namespace mamba::util
{
    void update_env_map(const environment_map& env)
    {
        for (const auto& [name, val] : env)
        {
            set_env(name, val);
        }
    }

    void set_env_map(const environment_map& env)
    {
        for (const auto& [name, val] : get_env_map())
        {
            unset_env(name);
        }
        update_env_map(env);
    }

    namespace
    {
        auto
        which_in_one_impl(const fs::u8path& exe, const fs::u8path& dir, const fs::u8path& extension)
            -> fs::u8path
        {
            std::error_code _ec;  // ignore
            if (!fs::exists(dir, _ec) || !fs::is_directory(dir, _ec))
            {
                return "";  // Not found
            }

            const auto strip_ext = [&extension](const fs::u8path& p)
            {
                if (p.extension() == extension)
                {
                    return p.stem();
                }
                return p.filename();
            };

            const auto exe_striped = strip_ext(exe);
            for (const auto& entry : fs::directory_iterator(dir, _ec))
            {
                if (auto p = entry.path(); strip_ext(p) == exe_striped)
                {
                    return p;
                }
            }
            return "";  // Not found
        }

        auto which_in_split_impl(
            const fs::u8path& exe,
            std::string_view paths,
            const fs::u8path& extension,
            char pathsep
        ) -> fs::u8path
        {
            auto elem = std::string_view();
            auto rest = std::optional<std::string_view>(paths);
            while (rest.has_value())
            {
                std::tie(elem, rest) = util::split_once(rest.value(), pathsep);
                if (auto p = which_in_one_impl(exe, elem, extension); !p.empty())
                {
                    return p;
                };
            }
            return "";
        }
    }

    auto which(std::string_view exe) -> fs::u8path
    {
        if (auto paths = get_env("PATH"))
        {
            if (auto p = which_in(exe, paths.value()); !p.empty())
            {
                return p;
            }
        }
        return which_system(exe);
    }

    namespace detail
    {
        auto which_in_one(const fs::u8path& exe, const fs::u8path& dir) -> fs::u8path
        {
            return which_in_one_impl(exe, dir, exec_extension());
        }

        auto which_in_split(const fs::u8path& exe, std::string_view paths) -> fs::u8path
        {
            return which_in_split_impl(exe, paths, exec_extension(), util::pathsep());
        }
    }
}
