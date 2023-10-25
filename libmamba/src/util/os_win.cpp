// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifdef _WIN32

#include <stdexcept>

#include <Shlobj.h>

#include "mamba/util/os_win.hpp"

namespace mamba::util
{

    namespace
    {
        auto get_windows_known_user_folder_raw(WindowsKnowUserFolder dir) -> KNOWNFOLDERID
        {
            switch (dir)
            {
                case (WindowsKnowUserFolder::Documents):
                    return FOLDERID_Documents;
                case (WindowsKnowUserFolder::Profile):
                    return FOLDERID_Profile;
                case (WindowsKnowUserFolder::Programs):
                    return FOLDERID_Programs;
                case (WindowsKnowUserFolder::ProgramData):
                    return FOLDERID_ProgramData;
                case (WindowsKnowUserFolder::LocalAppData):
                    return FOLDERID_LocalAppData;
                case (WindowsKnowUserFolder::RoamingAppData):
                    return FOLDERID_RoamingAppData;
            }
            throw std::invalid_argument("Invalid enum");
        }

        struct COMwstr
        {
            ~COMwstr()
            {
                CoTaskMemFree(str);
            }

            wchar_t* str = nullptr;
        };
    }

    auto get_windows_known_user_folder(WindowsKnowUserFolder dir) -> fs::u8path
    {
        auto localAppData = COMwstr{ nullptr };

        HRESULT const hres = SHGetKnownFolderPath(
            get_windows_known_user_folder_raw(dir),
            KF_FLAG_DONT_VERIFY,
            nullptr,
            &(localAppData.str)
        );

        if (FAILED(hres) || (localAppData.str == nullptr))
        {
            throw std::runtime_error("Could not retrieve known user folder");
        }

        return fs::u8path(localAppData.str);
    }
}

#else  // #ifdef _WIN32

#include <stdexcept>

#include <fmt/format.h>

#include "mamba/util/os_win.hpp"

namespace mamba::util
{
    namespace
    {
        [[noreturn]] void throw_not_implemented(std::string_view name)
        {
            throw std::invalid_argument(fmt::format(R"(Function "{}" only available on Windows)", name)
            );
        }
    }

    auto get_windows_known_user_folder(WindowsKnowUserFolder) -> fs::u8path
    {
        throw_not_implemented("get_windows_known_user_folder");
    }
}

#endif  // #ifdef _WIN32
