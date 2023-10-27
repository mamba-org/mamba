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
        auto env_path = override_path == "" ? util::getenv("PATH") : override_path;
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

    std::map<std::string, std::string> copy()
    {
        std::map<std::string, std::string> m;
#ifndef _WIN32
        int i = 1;
        const char* c = *environ;
        for (; c; i++)
        {
            std::string_view s(c);
            auto pos = s.find("=");
            m[std::string(s.substr(0, pos))] = (pos != s.npos) ? std::string(s.substr(pos + 1)) : "";
            c = *(environ + i);
        }
#else

        // inspired by
        // https://github.com/gennaroprota/breath/blob/0709a9f0fe4e745b1d9fc44ab65d92853820b515
        //                    /breath/environment/brt/dep/syst/windows/get_environment_map.cpp#L38-L80
        char* start = GetEnvironmentStrings();
        if (start == nullptr)
        {
            throw std::runtime_error("GetEnvironmentStrings() failed");
        }

        char* current = start;
        while (*current != '\0')
        {
            std::string_view s = current;
            auto pos = s.find("=");
            assert(pos != std::string_view::npos);
            std::string key = util::to_upper(s.substr(0, pos));
            if (!key.empty())
            {
                std::string_view value = (pos != s.npos) ? s.substr(pos + 1) : "";
                m[std::string(key)] = value;
            }
            current += s.size() + 1;
        }
#endif
        return m;
    }

    std::string platform()
    {
#ifndef _WIN32
        utsname un;
        int ret = uname(&un);

        if (ret == -1)
        {
            throw std::runtime_error("uname() failed");
        }

        return std::string(un.sysname);
#else
        return "win32";
#endif
    }

    fs::u8path home_directory()
    {
#ifdef _WIN32
        std::string maybe_home = util::getenv("USERPROFILE").value_or("");
        if (maybe_home.empty())
        {
            maybe_home = util::concat(
                util::getenv("HOMEDRIVE").value_or(""),
                util::getenv("HOMEPATH").value_or("")
            );
        }
        if (maybe_home.empty())
        {
            throw std::runtime_error(
                "Cannot determine HOME (checked USERPROFILE, HOMEDRIVE and HOMEPATH env vars)"
            );
        }
#else
        std::string maybe_home = util::getenv("HOME").value_or("");
        if (maybe_home.empty())
        {
            maybe_home = getpwuid(getuid())->pw_dir;
        }
        if (maybe_home.empty())
        {
            throw std::runtime_error("HOME not set.");
        }
#endif
        return fs::u8path(maybe_home);
    }

    fs::u8path user_config_dir()
    {
        std::string maybe_user_config_dir = util::getenv("XDG_CONFIG_HOME").value_or("");
        if (maybe_user_config_dir.empty())
        {
#ifdef _WIN32
            maybe_user_config_dir = util::get_windows_known_user_folder(
                util::WindowsKnowUserFolder::RoamingAppData
            );
#else
            maybe_user_config_dir = home_directory() / ".config";
#endif
        }
        return fs::u8path(maybe_user_config_dir) / "mamba";
    }

    fs::u8path user_data_dir()
    {
        std::string maybe_user_data_dir = util::getenv("XDG_DATA_HOME").value_or("");
        if (maybe_user_data_dir.empty())
        {
#ifdef _WIN32
            maybe_user_data_dir = util::get_windows_known_user_folder(
                util::WindowsKnowUserFolder::RoamingAppData
            );
#else
            maybe_user_data_dir = home_directory() / ".local" / "share";
#endif
        }
        return fs::u8path(maybe_user_data_dir) / "mamba";
    }

    fs::u8path user_cache_dir()
    {
        std::string maybe_user_cache_dir = util::getenv("XDG_CACHE_HOME").value_or("");
        if (maybe_user_cache_dir.empty())
        {
#ifdef _WIN32
            maybe_user_cache_dir = util::get_windows_known_user_folder(
                util::WindowsKnowUserFolder::LocalAppData
            );
#else
            maybe_user_cache_dir = home_directory() / ".cache";
#endif
        }
        return fs::u8path(maybe_user_cache_dir) / "mamba";
    }

    fs::u8path expand_user(const fs::u8path& path)
    {
        auto p = path.string();
        if (p[0] == '~')
        {
            p.replace(0, 1, home_directory().string());
        }
        return p;
    }

    fs::u8path shrink_user(const fs::u8path& path)
    {
        auto p = path.string();
        auto home = home_directory().string();
        if (util::starts_with(p, home))
        {
            p.replace(0, home.size(), "~");
        }
        return p;
    }
}
