// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <array>
#include <cassert>
#include <optional>
#include <regex>
#include <stdexcept>
#include <string>
#include <string_view>

#include <fmt/format.h>
#include <openssl/evp.h>

#include "mamba/core/mamba_fs.hpp"
#include "mamba/util/build.hpp"
#include "mamba/util/path_manip.hpp"
#include "mamba/util/string.hpp"
#include "mamba/util/url_manip.hpp"

namespace mamba::util
{
    namespace
    {
        auto url_is_unreserved_char(char c) -> bool
        {
            // https://github.com/curl/curl/blob/67e9e3cb1ea498cb94071dddb7653ab5169734b2/lib/escape.c#L45
            return util::is_alphanum(c) || (c == '-') || (c == '.') || (c == '_') || (c == '~');
        }

        auto url_encode_char(char c) -> std::array<char, 3>
        {
            // https://github.com/curl/curl/blob/67e9e3cb1ea498cb94071dddb7653ab5169734b2/lib/escape.c#L107
            static constexpr auto hex = std::array{
                '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F',
            };
            return std::array{
                '%',
                hex[static_cast<unsigned char>(c) >> 4],
                hex[c & 0xf],
            };
        }

        auto url_is_hex_char(char c) -> bool
        {
            return util::is_digit(c) || (('A' <= c) && (c <= 'F')) || (('a' <= c) && (c <= 'f'));
        }

        auto url_decode_char(char d10, char d1) -> char
        {
            // https://github.com/curl/curl/blob/67e9e3cb1ea498cb94071dddb7653ab5169734b2/lib/escape.c#L147
            // Offset from char '0', contains '0' padding for incomplete values.
            static constexpr std::array<unsigned char, 55> hex_offset = {
                0, 1,  2,  3,  4,  5,  6,  7, 8, 9, 0, 0, 0, 0, 0, 0, /* 0x30 - 0x3f */
                0, 10, 11, 12, 13, 14, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x40 - 0x4f */
                0, 0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x50 - 0x5f */
                0, 10, 11, 12, 13, 14, 15                             /* 0x60 - 0x66 */
            };
            assert('0' <= d10);
            assert('0' <= d1);
            unsigned char idx10 = static_cast<unsigned char>(d10 - '0');
            unsigned char idx1 = static_cast<unsigned char>(d1 - '0');
            assert(idx10 < hex_offset.size());
            assert(idx1 < hex_offset.size());
            return static_cast<char>((hex_offset[idx10] << 4) | hex_offset[idx1]);
        }

        template <typename Str>
        auto url_encode_impl(std::string_view url, Str exclude) -> std::string
        {
            std::string out = {};
            out.reserve(url.size());
            for (char c : url)
            {
                if (url_is_unreserved_char(c) || contains(exclude, c))
                {
                    out += c;
                }
                else
                {
                    const auto encoding = url_encode_char(c);
                    out += std::string_view(encoding.data(), encoding.size());
                }
            }
            return out;
        }
    }

    auto url_encode(std::string_view url) -> std::string
    {
        return url_encode_impl(url, 'a');  // Already not encoded
    }

    auto url_encode(std::string_view url, std::string_view exclude) -> std::string
    {
        return url_encode_impl(url, exclude);
    }

    auto url_encode(std::string_view url, char exclude) -> std::string
    {
        return url_encode_impl(url, exclude);
    }

    auto url_decode(std::string_view url) -> std::string
    {
        std::string out = {};
        out.reserve(url.size());
        const auto end = url.cend();
        for (auto iter = url.cbegin(); iter < end; ++iter)
        {
            if (((iter + 2) < end) && (iter[0] == '%') && url_is_hex_char(iter[1])
                && url_is_hex_char(iter[2]))
            {
                out.push_back(url_decode_char(iter[1], iter[2]));
                iter += 2;
            }
            else
            {
                out.push_back(*iter);
            }
        }
        return out;
    }

    // proper file scheme on Windows is `file:///C:/blabla`
    // https://blogs.msdn.microsoft.com/ie/2006/12/06/file-uris-in-windows/
    std::string concat_scheme_url(const std::string& scheme, const std::string& location)
    {
        if (scheme == "file" && location.size() > 1 && location[1] == ':')
        {
            return util::concat("file:///", location);
        }
        else
        {
            return util::concat(scheme, "://", location);
        }
    }

    std::string build_url(
        const std::optional<std::string>& auth,
        const std::string& scheme,
        const std::string& base,
        bool with_credential
    )
    {
        if (with_credential && auth)
        {
            return concat_scheme_url(scheme, util::concat(*auth, "@", base));
        }
        else
        {
            return concat_scheme_url(scheme, base);
        }
    }

    void split_platform(
        const std::vector<std::string>& known_platforms,
        const std::string& url,
        const std::string& context_platform,
        std::string& cleaned_url,
        std::string& platform
    )
    {
        platform = "";

        auto check_platform_position = [&url](std::size_t pos, const std::string& lplatform) -> bool
        {
            if (pos == std::string::npos)
            {
                return false;
            }
            if (pos > 0 && url[pos - 1] != '/')
            {
                return false;
            }
            if ((pos + lplatform.size()) < url.size() && url[pos + lplatform.size()] != '/')
            {
                return false;
            }

            return true;
        };

        std::size_t pos = url.find(context_platform);
        if (check_platform_position(pos, context_platform))
        {
            platform = context_platform;
        }
        else
        {
            for (auto it = known_platforms.begin(); it != known_platforms.end(); ++it)
            {
                pos = url.find(*it);
                if (check_platform_position(pos, *it))
                {
                    platform = *it;
                    break;
                }
            }
        }

        cleaned_url = url;
        if (pos != std::string::npos)
        {
            cleaned_url.replace(pos - 1, platform.size() + 1, "");
        }
        cleaned_url = util::rstrip(cleaned_url, "/");
    }

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
                    url_encode(path_to_posix(std::string(path.substr(2))), '/')
                );
            }
            return util::concat(file_scheme, url_encode(path_to_posix(std::string(path)), '/'));
        }
        return util::concat(file_scheme, url_encode(path, '/'));
    }

    auto path_to_url(std::string_view path) -> std::string
    {
        return abs_path_to_url(fs::absolute(path).string());
    }

    auto path_or_url_to_url(std::string_view path) -> std::string
    {
        if (url_has_scheme(path))
        {
            return std::string(path);
        }
        return path_to_url(path);
    }

    std::string file_uri_unc2_to_unc4(std::string_view uri)
    {
        static constexpr std::string_view file_scheme = "file:";

        // Not "file:" scheme
        if (!util::starts_with(uri, file_scheme))
        {
            return std::string(uri);
        }

        // No hostname set in "file://hostname/path/to/data.xml"
        auto [slashes, rest] = util::lstrip_parts(util::remove_prefix(uri, file_scheme), '/');
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

    std::string cache_name_from_url(const std::string& url)
    {
        std::string u = url;
        if (u.empty() || (u.back() != '/' && !util::ends_with(u, ".json")))
        {
            u += '/';
        }

        // mimicking conda's behavior by special handling repodata.json
        // todo support .zst
        if (util::ends_with(u, "/repodata.json"))
        {
            u = u.substr(0, u.size() - 13);
        }

        unsigned char hash[16];

        EVP_MD_CTX* mdctx;
        mdctx = EVP_MD_CTX_create();
        EVP_DigestInit_ex(mdctx, EVP_md5(), nullptr);
        EVP_DigestUpdate(mdctx, u.c_str(), u.size());
        EVP_DigestFinal_ex(mdctx, hash, nullptr);
        EVP_MD_CTX_destroy(mdctx);

        std::string hex_digest = util::hex_string(hash, 16);
        return hex_digest.substr(0u, 8u);
    }
}
