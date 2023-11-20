// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <cstdlib>
#include <optional>

#ifdef _WIN32
#include <mutex>

#include <Shlobj.h>

#include "mamba/core/output.hpp"
#include "mamba/core/util_os.hpp"
#include "mamba/util/os_win.hpp"
#else
#include <vector>

#include <pwd.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <wordexp.h>

extern "C"
{
    extern char** environ;
}
#endif

#include "mamba/core/environment.hpp"
#include "mamba/util/environment.hpp"
#include "mamba/util/string.hpp"

namespace mamba::env
{
    auto which(std::string_view exe, std::string_view override_path) -> fs::u8path
    {
        // TODO maybe add a cache?
        auto env_path = std::string(
            override_path == "" ? util::get_env("PATH").value_or("") : override_path
        );
        if (!env_path.empty())
        {
            const auto parts = util::split(env_path, util::pathsep());
            const std::vector<fs::u8path> search_paths(parts.begin(), parts.end());
            return which_in(exe, search_paths);
        }


#ifndef _WIN32
        if (override_path == "")
        {
            const auto n = ::confstr(_CS_PATH, nullptr, static_cast<std::size_t>(0));
            auto pathbuf = std::vector<char>(n, '\0');
            ::confstr(_CS_PATH, pathbuf.data(), n);
            return which(exe, std::string_view(pathbuf.data(), n));
        }
#endif

        return "";  // empty path
    }

    namespace detail
    {
        auto which_in_impl(const fs::u8path& exe, const fs::u8path& dir, const fs::u8path& extension)
            -> fs::u8path
        {
            std::error_code _ec;  // ignore
            if (!fs::exists(dir, _ec) || !fs::is_directory(dir, _ec))
            {
                return "";  // Not found
            }

            for (const auto& entry : fs::directory_iterator(dir, _ec))
            {
                auto p = entry.path();
                if ((p.stem() == exe.stem()) && (p.extension().empty() || p.extension() == extension))
                {
                    return entry.path();
                }
            }
            return "";  // Not found
        }
    }
}
