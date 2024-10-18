// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <array>
#include <regex>
#include <stdexcept>
#include <string>

#include <fmt/format.h>
#include <fmt/ranges.h>
#include <reproc++/run.hpp>

#include "mamba/util/environment.hpp"
#include "mamba/util/os_win.hpp"
#include "mamba/util/string.hpp"

#ifdef _WIN32
#include <cassert>
#include <limits>

#include <Shlobj.h>
#endif

namespace mamba::util
{

#ifdef _WIN32

    namespace
    {
        auto get_windows_known_user_folder_raw(WindowsKnowUserFolder dir) -> KNOWNFOLDERID
        {
            switch (dir)
            {
                case (WindowsKnowUserFolder::Documents):
                    return ::FOLDERID_Documents;
                case (WindowsKnowUserFolder::Profile):
                    return ::FOLDERID_Profile;
                case (WindowsKnowUserFolder::Programs):
                    return ::FOLDERID_Programs;
                case (WindowsKnowUserFolder::ProgramData):
                    return ::FOLDERID_ProgramData;
                case (WindowsKnowUserFolder::LocalAppData):
                    return ::FOLDERID_LocalAppData;
                case (WindowsKnowUserFolder::RoamingAppData):
                    return ::FOLDERID_RoamingAppData;
            }
            throw std::invalid_argument("Invalid enum");
        }

        struct COMwstr
        {
            ~COMwstr()
            {
                ::CoTaskMemFree(str);
            }

            wchar_t* str = nullptr;
        };
    }

    auto get_windows_known_user_folder(WindowsKnowUserFolder dir) -> std::string
    {
        auto localAppData = COMwstr{ nullptr };

        const auto hres = SHGetKnownFolderPath(
            get_windows_known_user_folder_raw(dir),
            KF_FLAG_DONT_VERIFY,
            nullptr,
            &(localAppData.str)
        );

        if (FAILED(hres) || (localAppData.str == nullptr))
        {
            throw std::runtime_error("Could not retrieve known user folder");
        }

        return windows_encoding_to_utf8(localAppData.str);
    }

    auto utf8_to_windows_encoding(const std::string_view utf8_text) -> std::wstring
    {
        if (utf8_text.empty())
        {
            return {};
        }

        assert(utf8_text.size() <= std::numeric_limits<int>::max());
        const int size = ::MultiByteToWideChar(CP_UTF8, 0, utf8_text.data(), utf8_text.size(), nullptr, 0);
        if (size <= 0)
        {
            throw std::runtime_error(fmt::format(
                R"(Failed to convert UTF-8 string "{}" to UTF-16: {})",
                utf8_text,
                std::system_category().message(static_cast<int>(::GetLastError()))
            ));
        }

        auto output = std::wstring(static_cast<std::size_t>(size), char(0));
        [[maybe_unused]] const int res_size = ::MultiByteToWideChar(
            CP_UTF8,
            0,
            utf8_text.data(),
            utf8_text.size(),
            output.data(),
            output.size()
        );
        assert(res_size == size);
        return output;
    }

    auto windows_encoding_to_utf8(std::wstring_view str) -> std::string
    {
        if (str.empty())
        {
            return {};
        }

        assert(str.size() <= std::numeric_limits<int>::max());
        const int size = ::WideCharToMultiByte(
            CP_UTF8,
            0,
            str.data(),
            static_cast<int>(str.size()),
            nullptr,
            0,
            nullptr,
            nullptr
        );
        if (size <= 0)
        {
            throw std::runtime_error(fmt::format(
                R"(Failed to convert UTF-16 string to UTF-8: {})",
                std::system_category().message(static_cast<int>(::GetLastError()))
            ));
        }

        auto output = std::string(static_cast<std::size_t>(size), char(0));
        [[maybe_unused]] const int res_size = ::WideCharToMultiByte(
            CP_UTF8,
            0,
            str.data(),
            static_cast<int>(str.size()),
            output.data(),
            static_cast<int>(size),
            nullptr,
            nullptr
        );
        assert(res_size == size);

        return output;
    }

#else  // #ifdef _WIN32

    namespace
    {
        [[noreturn]] void throw_not_implemented(std::string_view name)
        {
            throw std::invalid_argument(fmt::format(R"(Function "{}" only available on Windows)", name)
            );
        }
    }

    auto get_windows_known_user_folder(WindowsKnowUserFolder) -> std::string
    {
        throw_not_implemented("get_windows_known_user_folder");
    }

    auto utf8_to_windows_encoding(const std::string_view) -> std::wstring
    {
        throw_not_implemented("utf8_to_windows_unicode");
    }

    auto windows_encoding_to_utf8(std::wstring_view) -> std::string
    {
        throw_not_implemented("windows_encoding_to_utf8");
    }

#endif

    auto windows_version() -> tl::expected<std::string, OSError>
    {
        auto comspec = util::get_env("COMSPEC");
        if (!comspec.has_value() || comspec->empty())
        {
            return tl::make_unexpected(OSError{ fmt::format(
                "Cannot find command line interpreter, environment variable COMSPEC not defined."
            ) });
        }

        const auto args = std::array<std::string, 3>{ std::move(comspec).value(), "/c", "ver" };

        auto out = std::string();
        auto err = std::string();

        auto [status, ec] = reproc::run(
            args,
            reproc::options{},
            reproc::sink::string(out),
            reproc::sink::string(err)
        );

        if (ec)
        {
            return tl::make_unexpected(OSError{ fmt::format(
                R"(Could not find Windows version by calling "{}": {})",
                fmt::join(args, " "),
                ec.message()
            ) });
        }

        out = util::strip(out);

        // from python
        static const auto ver_output_regex = std::regex(R"((?:([\w ]+) ([\w.]+) .*\[.* ([\d.]+)\]))");

        // The output of the command could contain multiple unrelated lines, so we need to check
        // every lines, which is why we need to search in a loop until reaching the end of the output.
        std::smatch rmatch;
        auto start_it = out.cbegin();
        while (std::regex_search(start_it, out.cend(), rmatch, ver_output_regex))
        {
            std::string full_version = rmatch[3];
            auto version_elems = util::split(full_version, ".");
            return { util::concat(version_elems[0], ".", version_elems[1], ".", version_elems[2]) };
        }

        return tl::make_unexpected(OSError{ fmt::format(
            R"(Could not parse Windows version in command "{}" output "{}")",
            fmt::join(args, " "),
            out
        ) });
    }
}
