// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.


#ifdef _WIN32

#include <mutex>
#include <stdexcept>

#include <fmt/format.h>
#include <Shlobj.h>

#include "mamba/util/environment.hpp"
#include "mamba/util/os_win.hpp"

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
        setenv(key, "");
    }
}

#else  // #ifdef _WIN32

#include <cstdlib>
#include <stdexcept>

#include <fmt/format.h>

#include "mamba/util/environment.hpp"

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
}

#endif  // #ifdef _WIN32
