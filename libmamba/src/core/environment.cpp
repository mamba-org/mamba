// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <cstdlib>

#ifdef _WIN32
#include <mutex>

#include <Shlobj.h>

#include "mamba/core/output.hpp"
#include "mamba/core/util_os.hpp"
#include "mamba/util/os_win.hpp"
#else
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
    fs::u8path which(const std::string& exe, const std::string& override_path)
    {
        // TODO maybe add a cache?
        auto env_path = override_path == "" ? util::get_env("PATH") : override_path;
        if (env_path)
        {
            std::string path = env_path.value();
            const auto parts = util::split(path, pathsep());
            const std::vector<fs::u8path> search_paths(parts.begin(), parts.end());
            return which(exe, search_paths);
        }


#ifndef _WIN32
        if (override_path == "")
        {
            char* pathbuf;
            size_t n = confstr(_CS_PATH, NULL, static_cast<size_t>(0));
            pathbuf = static_cast<char*>(malloc(n));
            if (pathbuf != NULL)
            {
                confstr(_CS_PATH, pathbuf, n);
                return which(exe, pathbuf);
            }
        }
#endif

        return "";  // empty path
    }

    fs::u8path which(const std::string& exe, const std::vector<fs::u8path>& search_paths)
    {
        for (auto& p : search_paths)
        {
            std::error_code _ec;  // ignore
            if (!fs::exists(p, _ec) || !fs::is_directory(p, _ec))
            {
                continue;
            }

#ifdef _WIN32
            const auto exe_with_extension = exe + ".exe";
#endif
            for (const auto& entry : fs::directory_iterator(p, _ec))
            {
                const auto filename = entry.path().filename();
                if (filename == exe

#ifdef _WIN32
                    || filename == exe_with_extension
#endif
                )
                {
                    return entry.path();
                }
            }
        }

        return "";  // empty path
    }

    fs::u8path expand_user(const fs::u8path& path)
    {
        auto p = path.string();
        if (p[0] == '~')
        {
            p.replace(0, 1, util::user_home_dir().string());
        }
        return p;
    }

    fs::u8path shrink_user(const fs::u8path& path)
    {
        auto p = path.string();
        auto home = util::user_home_dir().string();
        if (util::starts_with(p, home))
        {
            p.replace(0, home.size(), "~");
        }
        return p;
    }
}
