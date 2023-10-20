// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <cstdlib>

#include "mamba/core/environment.hpp"
#include "mamba/util/string.hpp"

#ifdef _WIN32
#include <mutex>

#include <Shlobj.h>

#include "mamba/core/output.hpp"
#include "mamba/core/util_os.hpp"
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

namespace mamba::env
{
#ifdef _WIN32
    namespace
    {
        const std::map<std::string, KNOWNFOLDERID> knownfolders = {
            { "programs", FOLDERID_Programs },       { "profile", FOLDERID_Profile },
            { "documents", FOLDERID_Documents },     { "roamingappdata", FOLDERID_RoamingAppData },
            { "programdata", FOLDERID_ProgramData }, { "localappdata", FOLDERID_LocalAppData },
        };

        auto get_known_folder(const std::string& id) -> fs::u8path
        {
            wchar_t* localAppData;
            HRESULT hres;

            hres = SHGetKnownFolderPath(knownfolders.at(id), KF_FLAG_DONT_VERIFY, nullptr, &localAppData);

            if (FAILED(hres))
            {
                throw std::runtime_error("Could not retrieve known folder");
            }

            std::wstring tmp(localAppData);
            fs::u8path res(tmp);
            CoTaskMemFree(localAppData);
            return res;
        }
    }
#endif

    std::optional<std::string> get(const std::string& key)
    {
#ifdef _WIN32
        // See:
        // https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/getenv-s-wgetenv-s?view=msvc-170
        static std::mutex call_mutex;
        std::scoped_lock ready_to_execute{ call_mutex };  // Calls to getenv_s kinds of
                                                          // functions are not thread-safe, this
                                                          // is to prevent related issues.

        const auto on_failed = [&](errno_t error_code)
        {
            LOG_ERROR << fmt::format(
                "Failed to acquire environment variable '{}' : errcode = {}",
                key,
                error_code
            );
        };

        const std::wstring unicode_key = to_windows_unicode(key);
        size_t required_size = 0;
        if (auto error_code = _wgetenv_s(&required_size, nullptr, 0, unicode_key.c_str());
            error_code == 0)
        {
            if (required_size == 0)  // The value doesn't exist.
            {
                return {};
            }

            std::wstring value(required_size, L'?');  // Note: The required size implies a `\0`
                                                      // but basic_string doesn't.
            if (error_code = _wgetenv_s(&required_size, value.data(), value.size(), unicode_key.c_str());
                error_code == 0)
            {
                value.pop_back();  // Remove the `\0` that was written in, otherwise any future
                                   // concatenation will fail.
                return mamba::to_utf8(value);
            }
            else
            {
                on_failed(error_code);
            }
        }
        else
        {
            on_failed(error_code);
        }
#else
        const char* value = std::getenv(key.c_str());
        if (value)
        {
            return value;
        }
#endif
        return {};
    }

    bool set(const std::string& key, const std::string& value)
    {
#ifdef _WIN32
        // See:
        // https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/getenv-s-wgetenv-s?view=msvc-170
        static std::mutex call_mutex;
        std::scoped_lock ready_to_execute{ call_mutex };  // Calls to getenv_s kinds of
                                                          // functions are not thread-safe, this
                                                          // is to prevent related issues.

        const std::wstring unicode_key = to_windows_unicode(key);
        const std::wstring unicode_value = to_windows_unicode(value);
        auto res = _wputenv_s(unicode_key.c_str(), unicode_value.c_str());
        if (res != 0)
        {
            LOG_ERROR << fmt::format(
                "Could not set environment variable '{}' to '{}' : {}",
                key,
                value,
                GetLastError()
            );
        }
        return res == 0;
#else
        return setenv(key.c_str(), value.c_str(), 1) == 0;
#endif
    }

    void unset(const std::string& key)
    {
#ifdef _WIN32
        set(key, "");
#else
        unsetenv(key.c_str());
#endif
    }

    fs::u8path which(const std::string& exe, const std::string& override_path)
    {
        // TODO maybe add a cache?
        auto env_path = override_path == "" ? env::get("PATH") : override_path;
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
        std::string maybe_home = env::get("USERPROFILE").value_or("");
        if (maybe_home.empty())
        {
            maybe_home = util::concat(
                env::get("HOMEDRIVE").value_or(""),
                env::get("HOMEPATH").value_or("")
            );
        }
        if (maybe_home.empty())
        {
            throw std::runtime_error(
                "Cannot determine HOME (checked USERPROFILE, HOMEDRIVE and HOMEPATH env vars)"
            );
        }
#else
        std::string maybe_home = env::get("HOME").value_or("");
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
        std::string maybe_user_config_dir = env::get("XDG_CONFIG_HOME").value_or("");
        if (maybe_user_config_dir.empty())
        {
#ifdef _WIN32
            maybe_user_config_dir = get_known_folder("roamingappdata");
#else
            maybe_user_config_dir = home_directory() / ".config";
#endif
        }
        return fs::u8path(maybe_user_config_dir) / "mamba";
    }

    fs::u8path user_data_dir()
    {
        std::string maybe_user_data_dir = env::get("XDG_DATA_HOME").value_or("");
        if (maybe_user_data_dir.empty())
        {
#ifdef _WIN32
            maybe_user_data_dir = get_known_folder("roamingappdata");
#else
            maybe_user_data_dir = home_directory() / ".local" / "share";
#endif
        }
        return fs::u8path(maybe_user_data_dir) / "mamba";
    }

    fs::u8path user_cache_dir()
    {
        std::string maybe_user_cache_dir = env::get("XDG_CACHE_HOME").value_or("");
        if (maybe_user_cache_dir.empty())
        {
#ifdef _WIN32
            maybe_user_cache_dir = get_known_folder("localappdata");
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
