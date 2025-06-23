// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <array>
#include <string>
#include <string_view>

#include "mamba/fs/filesystem.hpp"
#include "mamba/util/build.hpp"
#include "mamba/util/encoding.hpp"
#include "mamba/util/path_manip.hpp"
#include "mamba/util/string.hpp"
#include "mamba/util/url_manip.hpp"

namespace mamba::util
{
    auto url_get_scheme(std::string_view url) -> std::string_view
    {
        static constexpr auto is_scheme_char = [](char c) -> bool
        { return util::is_alphanum(c) || (c == '.') || (c == '-') || (c == '_'); };

        const auto sep = url.find("://");
        if ((0 < sep) && (sep < std::string_view::npos))
        {
            auto scheme = url.substr(0, sep);
            if (std::all_of(scheme.cbegin(), scheme.cend(), is_scheme_char))
            {
                return scheme;
            }
        }
        return "";
    }

    auto is_file_uri(std::string_view url) -> bool
    {
        return url_get_scheme(url) == "file";
    }

    auto url_has_scheme(std::string_view url) -> bool
    {
        return !url_get_scheme(url).empty();
    }

    auto abs_path_to_url(std::string_view path) -> std::string
    {
        static constexpr std::string_view file_scheme = "file://";
        if (on_win)
        {
            if ((path.size() >= 2) && path_has_drive_letter(path))
            {
                return concat(
                    file_scheme,
                    path.substr(0, 2),
                    encode_percent(path_to_posix(std::string(path.substr(2))), '/')
                );
            }
            return util::concat(file_scheme, encode_percent(path_to_posix(std::string(path)), '/'));
        }
        return util::concat(file_scheme, encode_percent(path, '/'));
    }

    auto abs_path_or_url_to_url(std::string_view path) -> std::string
    {
        if (url_has_scheme(path))
        {
            return std::string(path);
        }
        return abs_path_to_url(path);
    }

    auto path_to_url(std::string_view path) -> std::string
    {
        return abs_path_to_url(fs::absolute(expand_home(path)).lexically_normal().string());
    }

    auto path_or_url_to_url(std::string_view path) -> std::string
    {
        if (url_has_scheme(path))
        {
            return std::string(path);
        }
        return path_to_url(path);
    }

    auto check_file_scheme_and_slashes(std::string_view uri)
        -> std::tuple<bool, std::string_view, std::string_view>
    {
        static constexpr std::string_view file_scheme = "file:";

        // Not "file:" scheme
        if (!util::starts_with(uri, file_scheme))
        {
            return { false, {}, {} };
        }

        auto [slashes, rest] = util::lstrip_parts(util::remove_prefix(uri, file_scheme), '/');
        return { true, slashes, rest };
    }

    auto make_curl_compatible(std::string uri) -> std::string
    {
        // Convert `file://` and `file:///` to `file:////`
        // when followed with a drive letter
        // to make it compatible with libcurl on unix
        auto [is_file_scheme, slashes, rest] = check_file_scheme_and_slashes(uri);
        if constexpr (!on_win)
        {
            if (is_file_scheme && path_has_drive_letter(rest)
                && ((slashes.size() == 2) || (slashes.size() == 3)))
            {
                return util::concat("file:////", rest);
            }
            return uri;
        }
        else
        {
            return uri;
        }
    }

    auto file_uri_unc2_to_unc4(std::string_view uri) -> std::string
    {
        auto [is_file_scheme, slashes, rest] = check_file_scheme_and_slashes(uri);
        if (!is_file_scheme)
        {
            return std::string(uri);
        }

        // No hostname set in "file://hostname/path/to/data.xml"
        if (slashes.size() != 2)
        {
            return std::string(uri);
        }

        const auto s_idx = rest.find('/');
        const auto c_idx = rest.find(':');

        // ':' found before '/', a Windows drive is specified in "file://C:/path/to/data.xml" (not
        // really URI compliant, they should have "file:///" or "file:/"). Otherwise no path in
        // "file://hostname", also not URI compliant.
        if (c_idx < s_idx)
        {
            return std::string(uri);
        }

        const auto hostname = rest.substr(0, s_idx);

        // '\' are used as path separator in "file://\\hostname\path\to\data.xml" (also not RFC
        // compliant)
        if (util::starts_with(hostname, R"(\\)"))
        {
            return std::string(uri);
        }

        // Things that means localhost are kept for some reason in ``url_to_path``
        // in ``conda.common.path``
        if ((hostname == "localhost") || (hostname == "127.0.0.1") || (hostname == "::1"))
        {
            return std::string(uri);
        }

        return util::concat("file:////", rest);
    }
}
