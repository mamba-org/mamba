#ifndef MAMBA_ENVIRONMENT
#define MAMBA_ENVIRONMENT

#include <map>
#include <cstdlib>
#include <cassert>
#include <string_view>

#include "thirdparty/filesystem.hpp"

namespace fs = ghc::filesystem;

#ifndef _WIN32
#include <sys/utsname.h>

extern "C"
{
    extern char **environ;
}
#endif

namespace mamba
{

namespace env
{
    constexpr inline const char* pathsep()
    {
    #ifdef _WIN32
        return ";";
    #else
        return ":";
    #endif
    }

    inline std::string get(const std::string& key)
    {
        const char* value = std::getenv(key.c_str());
        if (!value) return "";
        return value;
    }

    inline bool set(const std::string& key, const std::string& value)
    {
        #ifdef _WIN32
        return SetEnvironmentVariable(key.c_str(), value.c_str());
        #else
        return setenv(key.c_str(), value.c_str(), 1) == 0;
        #endif
    }

    inline fs::path which(const std::string& exe)
    {
        // TODO maybe add a cache?
        if (std::getenv("PATH"))
        {
            std::string path = std::getenv("PATH");
            auto parts = split(path, pathsep());
            for (auto& p : parts)
            {
                if (!fs::exists(p) || !fs::is_directory(p))
                {
                    continue;
                }
                for (const auto & entry : fs::directory_iterator(p))
                {
                    if (entry.path().filename() == exe)
                    {
                        return entry.path();
                    }
                }
            }
        }
        return ""; // empty path
    }

    inline std::map<std::string, std::string> copy()
    {
        std::map<std::string, std::string> m;
        #ifndef _WIN32
        int i = 1;
        std::string_view s = *environ;
        for (; s.size(); i++)
        {
            auto pos = s.find("=");
            m[std::string(s.substr(0, pos))] = (pos != s.npos) ? std::string(s.substr(pos + 1)) : "";
            s = *(environ + i);
        }
        #else

        // inspired by https://github.com/gennaroprota/breath/blob/0709a9f0fe4e745b1d9fc44ab65d92853820b515
        //                    /breath/environment/brt/dep/syst/windows/get_environment_map.cpp#L38-L80
        char* start = GetEnvironmentStrings();
        if (start == nullptr)
        {
            throw std::runtime_error("GetEnvironmentStrings() failed") ;
        }

        char* current = start;
        while (*current != '\0')
        {
            std::string_view s = current;
            auto pos = s.find("=");
            assert (pos != std::string_view::npos);
            std::string_view key = s.substr(0, pos) ;

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


    inline std::string platform()
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
}

}

#endif