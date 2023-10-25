// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <algorithm>
#include <cassert>
#include <string_view>
#include <tuple>

#include <fmt/format.h>

#include "mamba/specs/archive.hpp"
#include "mamba/specs/conda_url.hpp"
#include "mamba/util/string.hpp"
#include "mamba/util/url_manip.hpp"

namespace mamba::specs
{
    /**
     * Find the location of "/os-arch"-like subsring.
     *
     * Not a static function, it is needed in "channel_spec.cpp".
     */
    auto find_slash_and_platform(std::string_view path)
        -> std::tuple<std::size_t, std::size_t, std::optional<Platform>>
    {
        static constexpr auto npos = std::string_view::npos;

        auto start = std::size_t(0);
        auto end = path.find('/', start + 1);
        while (start != npos)
        {
            assert(start < end);
            const auto count = (end == npos) ? npos : end - start;
            const auto count_minus_1 = (end == npos) ? npos : end - start - 1;
            if (auto plat = platform_parse(path.substr(start + 1, count_minus_1)))
            {
                return { start, count, plat };
            }
            start = end;
            end = path.find('/', start + 1);
        }
        return { npos, 0, std::nullopt };
    }

    namespace
    {
        [[nodiscard]] auto is_token_char(char c) -> bool
        {
            return util::is_alphanum(c) || (c == '-');
        }

        [[nodiscard]] auto is_token_first_char(char c) -> bool
        {
            return is_token_char(c) || (c == '_');
        }

        [[nodiscard]] auto is_token(std::string_view str) -> bool
        {
            // usernames on anaconda.org can have a underscore, which influences the first two
            // characters
            static constexpr std::size_t token_start_size = 2;
            if (str.size() < token_start_size)
            {
                return false;
            }
            const auto token_first = str.substr(0, token_start_size);
            const auto token_rest = str.substr(token_start_size);
            return std::all_of(token_first.cbegin(), token_first.cend(), &is_token_first_char)
                   && std::all_of(token_rest.cbegin(), token_rest.cend(), &is_token_char);
        }

        [[nodiscard]] auto token_and_prefix_len(std::string_view path) -> std::size_t
        {
            static constexpr auto npos = std::string_view::npos;
            static constexpr auto prefix = CondaURL::token_prefix;

            if ((path.size() <= prefix.size()) || !util::starts_with(path, prefix))
            {
                return 0;
            }

            const auto token_end_pos = path.find('/', prefix.size());
            assert(prefix.size() < token_end_pos);
            const auto token_len = (token_end_pos == npos) ? npos : token_end_pos - prefix.size();
            if (is_token(path.substr(prefix.size(), token_len)))
            {
                return token_end_pos;
            }

            return 0;
        }
    }

    void CondaURL::ensure_path_without_token_leading_slash()
    {
        if (path_without_token().empty())
        {
            set_path_without_token("/", Encode::no);
        }
    }

    CondaURL::CondaURL(URL&& url)
        : Base(std::move(url))
    {
        ensure_path_without_token_leading_slash();
    }

    CondaURL::CondaURL(const util::URL& url)
        : CondaURL(URL(url))
    {
    }

    auto CondaURL::base() const -> const util::URL&
    {
        return static_cast<const util::URL&>(*this);
    }

    auto CondaURL::parse(std::string_view url) -> CondaURL
    {
        return CondaURL(URL::parse(url));
    }

    void CondaURL::set_path(std::string_view path, Encode::yes_type)
    {
        Base::set_path(path, Encode::yes);
        ensure_path_without_token_leading_slash();
    }

    void CondaURL::set_path(std::string path, Encode::no_type)
    {
        Base::set_path(path, Encode::no);
        ensure_path_without_token_leading_slash();
    }

    void CondaURL::append_path(std::string_view path, Encode::yes_type)
    {
        Base::append_path(path, Encode::yes);
        ensure_path_without_token_leading_slash();
    }

    void CondaURL::append_path(std::string_view path, Encode::no_type)
    {
        Base::append_path(path, Encode::no);
        ensure_path_without_token_leading_slash();
    }

    auto CondaURL::has_token() const -> bool
    {
        const auto& p = path(Decode::no);
        return
            // Fast return for easy cases
            (p.size() > token_prefix.size())
            // The actual check
            && util::starts_with(p, token_prefix);
    }

    auto CondaURL::token() const -> std::string_view
    {
        static constexpr auto npos = std::string_view::npos;

        const auto& l_path = path(Decode::no);
        const auto len = token_and_prefix_len(l_path);
        if (len == 0)
        {
            return "";
        }
        assert(token_prefix.size() < len);
        const auto token_len = (len != npos) ? len - token_prefix.size() : npos;
        return std::string_view(l_path).substr(token_prefix.size(), token_len);
    }

    namespace
    {
        void set_token_no_check_input_impl(
            std::string& path,
            std::size_t pos,
            std::size_t len,
            std::string_view new_token
        )
        {
            static constexpr auto npos = std::string_view::npos;

            assert(CondaURL::token_prefix.size() < len);
            const auto token_len = (len != npos) ? len - CondaURL::token_prefix.size() : npos;
            path.replace(pos + CondaURL::token_prefix.size(), token_len, new_token);
        }
    }

    void CondaURL::set_token(std::string_view token)
    {
        if (!is_token(token))
        {
            throw std::invalid_argument(fmt::format(R"(Invalid CondaURL token "{}")", token));
        }
        const auto len = token_and_prefix_len(path(Decode::no));
        if (len == 0)
        {
            std::string l_path = clear_path();  // percent encoded
            assert(util::starts_with(l_path, '/'));
            set_path(util::concat("/t/", token, l_path), Encode::no);
        }
        else
        {
            std::string l_path = clear_path();  // percent encoded
            set_token_no_check_input_impl(l_path, 0, len, token);
            set_path(std::move(l_path), Encode::no);
        }
    }

    auto CondaURL::clear_token() -> bool
    {
        const auto len = token_and_prefix_len(path(Decode::no));
        if (len == 0)
        {
            return false;
        }
        assert(token_prefix.size() < len);
        std::string l_path = clear_path();  // percent encoded
        l_path.erase(0, len);
        Base::set_path(std::move(l_path), Encode::no);
        return true;
    }

    auto CondaURL::path_without_token(Decode::no_type) const -> std::string_view
    {
        const auto& full_path = path(Decode::no);
        if (const auto len = token_and_prefix_len(full_path); len > 0)
        {
            return std::string_view(full_path).substr(std::min(len, full_path.size()));
        }
        return full_path;
    }

    auto CondaURL::path_without_token(Decode::yes_type) const -> std::string
    {
        return util::url_decode(path_without_token(Decode::no));
    }

    void CondaURL::set_path_without_token(std::string_view new_path, Encode::no_type)
    {
        if (const auto len = token_and_prefix_len(path(Decode::no)); len > 0)
        {
            auto old_path = clear_path();
            old_path.erase(std::min(len, old_path.size()));
            Base::set_path(std::move(old_path), Encode::no);
            Base::append_path(new_path.empty() ? "/" : new_path);
        }
        else
        {
            Base::set_path(std::string(new_path), Encode::no);
        }
    }

    void CondaURL::set_path_without_token(std::string_view new_path, Encode::yes_type)
    {
        clear_path_without_token();
        Base::append_path(new_path.empty() ? "/" : new_path, Encode::yes);
    }

    auto CondaURL::clear_path_without_token() -> bool
    {
        const std::size_t old_len = path(Decode::no).size();
        set_path_without_token("", Encode::no);
        return path(Decode::no).size() != old_len;
    }

    auto CondaURL::platform() const -> std::optional<Platform>
    {
        const auto& l_path = path(Decode::no);
        assert(!l_path.empty() && (l_path.front() == '/'));
        const auto [pos, count, plat] = find_slash_and_platform(l_path);
        return plat;
    }

    auto CondaURL::platform_name() const -> std::string_view
    {
        static constexpr auto npos = std::string_view::npos;

        const auto& l_path = path(Decode::no);
        assert(!l_path.empty() && (l_path.front() == '/'));
        const auto [pos, len, plat] = find_slash_and_platform(l_path);
        if (!plat.has_value())
        {
            return "";
        }
        assert(1 < len);
        const auto plat_len = (len != npos) ? len - 1 : npos;
        return std::string_view(l_path).substr(pos + 1, plat_len);
    }

    void CondaURL::set_platform_no_check_input(std::string_view platform)
    {
        static constexpr auto npos = std::string_view::npos;

        assert(!path(Decode::no).empty() && (path(Decode::no).front() == '/'));
        const auto [pos, len, plat] = find_slash_and_platform(path(Decode::no));
        if (!plat.has_value())
        {
            throw std::invalid_argument(
                fmt::format(R"(No platform in orignial path "{}")", path(Decode::no))
            );
        }
        assert(1 < len);
        std::string l_path = clear_path();  // percent encoded
        const auto plat_len = (len != npos) ? len - 1 : npos;
        l_path.replace(pos + 1, plat_len, platform);
        Base::set_path(std::move(l_path), Encode::no);
    }

    void CondaURL::set_platform(std::string_view platform)
    {
        if (!platform_parse(platform).has_value())
        {
            throw std::invalid_argument(fmt::format(R"(Invalid CondaURL platform "{}")", platform));
        }
        return set_platform_no_check_input(platform);
    }

    void CondaURL::set_platform(Platform platform)
    {
        return set_platform_no_check_input(specs::platform_name(platform));
    }

    auto CondaURL::clear_platform() -> bool
    {
        assert(!path(Decode::no).empty() && (path(Decode::no).front() == '/'));
        const auto [pos, count, plat] = find_slash_and_platform(path(Decode::no));
        if (!plat.has_value())
        {
            return false;
        }
        assert(1 < count);
        std::string l_path = clear_path();  // percent encoded
        l_path.erase(pos, count);
        Base::set_path(std::move(l_path), Encode::no);
        return true;
    }

    auto CondaURL::package(Decode::yes_type) const -> std::string
    {
        return util::url_decode(package(Decode::no));
    }

    auto CondaURL::package(Decode::no_type) const -> std::string_view
    {
        // Must not decode to find the meaningful '/' spearators
        const auto& l_path = path(Decode::no);
        if (has_archive_extension(l_path))
        {
            auto [head, pkg] = util::rstrip_if_parts(l_path, [](char c) { return c != '/'; });
            return pkg;
        }
        return "";
    }

    void CondaURL::set_package(std::string_view pkg, Encode::yes_type)
    {
        return set_package(util::url_encode(pkg), Encode::no);
    }

    void CondaURL::set_package(std::string_view pkg, Encode::no_type)
    {
        if (!has_archive_extension(pkg))
        {
            throw std::invalid_argument(
                fmt::format(R"(Invalid CondaURL package "{}", use path_append instead)", pkg)
            );
        }
        // Must not decode to find the meaningful '/' spearators
        if (has_archive_extension(path(Decode::no)))
        {
            auto l_path = clear_path();
            const auto pos = std::min(std::min(l_path.rfind('/'), l_path.size()) + 1ul, l_path.size());
            l_path.replace(pos, std::string::npos, pkg);
            Base::set_path(std::move(l_path), Encode::no);
        }
        else
        {
            Base::append_path(pkg, Encode::no);
        }
    }

    auto CondaURL::clear_package() -> bool
    {
        // Must not decode to find the meaningful '/' spearators
        if (has_archive_extension(path(Decode::no)))
        {
            auto l_path = clear_path();
            l_path.erase(l_path.rfind('/'));
            Base::set_path(std::move(l_path), Encode::no);
            return true;
        }
        return false;
    }

    auto CondaURL::str(Credentials credentials) const -> std::string
    {
        std::string_view l_path = "";
        std::string_view l_token = "";
        switch (credentials)
        {
            case (Credentials::Show):
            {
                l_path = path(Decode::no);
                break;
            }
            case (Credentials::Hide):
            {
                if (token().empty())
                {
                    l_path = path(Decode::no);
                }
                else
                {
                    l_path = path_without_token(Decode::no);
                    l_token = "*****";
                }
                break;
            }
            case (Credentials::Remove):
            {
                l_path = path_without_token(Decode::no);
                break;
            }
        }
        std::array<std::string_view, 7> authority = authority_elems(credentials, Decode::no);
        return util::concat(
            scheme(),
            "://",
            authority[0],
            authority[1],
            authority[2],
            authority[3],
            authority[4],
            authority[5],
            authority[6],
            l_token.empty() ? "" : token_prefix,
            l_token,
            l_path,
            query().empty() ? "" : "?",
            query(),
            fragment().empty() ? "" : "#",
            fragment()
        );
    }

    auto
    CondaURL::pretty_str(StripScheme strip_scheme, char rstrip_path, Credentials credentials) const
        -> std::string
    {
        std::string l_path = {};
        switch (credentials)
        {
            case (Credentials::Show):
            {
                l_path = pretty_str_path(strip_scheme, rstrip_path);
                break;
            }
            case (Credentials::Hide):
            {
                if (token().empty())
                {
                    l_path = pretty_str_path(strip_scheme, rstrip_path);
                }
                else
                {
                    l_path = util::concat("/t/*****", path_without_token(Decode::yes));
                }
                break;
            }
            case (Credentials::Remove):
            {
                if (token().empty())
                {
                    l_path = pretty_str_path(strip_scheme, rstrip_path);
                }
                else
                {
                    l_path = path_without_token(Decode::yes);
                }
                break;
            }
        }

        std::array<std::string, 7> authority = authority_elems(credentials, Decode::yes);
        return util::concat(
            (strip_scheme == StripScheme::no) ? scheme() : "",
            (strip_scheme == StripScheme::no) ? "://" : "",
            authority[0],
            authority[1],
            authority[2],
            authority[3],
            authority[4],
            authority[5],
            authority[6],
            l_path,
            query().empty() ? "" : "?",
            query(),
            fragment().empty() ? "" : "#",
            fragment()
        );
    }

    auto operator==(const CondaURL& a, const CondaURL& b) -> bool
    {
        return a.base() == b.base();
    }

    auto operator!=(const CondaURL& a, const CondaURL& b) -> bool
    {
        return !(a == b);
    }

    auto operator/(const CondaURL& url, std::string_view subpath) -> CondaURL
    {
        return CondaURL(url) / subpath;
    }

    auto operator/(CondaURL&& url, std::string_view subpath) -> CondaURL
    {
        url.append_path(subpath);
        return std::move(url);
    }
}

auto
std::hash<mamba::specs::CondaURL>::operator()(const mamba::specs::CondaURL& u) const -> std::size_t
{
    return std::hash<mamba::util::URL>()(u.base());
}
