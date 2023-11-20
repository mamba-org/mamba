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
#include "mamba/util/build.hpp"
#include "mamba/util/environment.hpp"
#include "mamba/util/string.hpp"

namespace mamba::env
{
    namespace
    {
#ifdef _WIN32
        auto which_system(std::string_view exe) -> fs::u8path
        {
            return "";
        }
#else
        auto which_system(std::string_view exe) -> fs::u8path
        {
            const auto n = ::confstr(_CS_PATH, nullptr, static_cast<std::size_t>(0));
            auto pathbuf = std::vector<char>(n, '\0');
            ::confstr(_CS_PATH, pathbuf.data(), n);
            return which_in(exe, std::string_view(pathbuf.data(), n));
        }
#endif

    }

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

        if (override_path.empty())
        {
            return which_system(exe);
        }

        return "";  // empty path
    }

    namespace
    {

        [[nodiscard]] constexpr auto exec_extension() -> std::string_view
        {
            if (util::on_win)
            {
                return ".exe";
            }
            return "";
        };

        [[nodiscard]] auto
        which_in_one_impl(const fs::u8path& exe, const fs::u8path& dir, const fs::u8path& extension)
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

        [[nodiscard]] auto which_in_split_impl(
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
