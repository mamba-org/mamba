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

#include "mamba/specs/url.hpp"
#include "mamba/util/string.hpp"

namespace mamba::specs
{
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

        [[nodiscard]] auto find_token_and_prefix(std::string_view path)
            -> std::pair<std::size_t, std::size_t>
        {
            static constexpr auto npos = std::string_view::npos;

            const auto prefix_pos = path.find(CondaURL::token_prefix);
            if (prefix_pos == npos)
            {
                return std::pair{ std::string_view::npos, 0ul };
            }

            const auto token_pos = prefix_pos + CondaURL::token_prefix.size();
            const auto token_end_pos = path.find('/', token_pos);
            assert(token_pos < token_end_pos);
            const auto token_len = (token_end_pos == npos) ? npos : token_end_pos - token_pos;
            if (is_token(path.substr(token_pos, token_len)))
            {
                const auto token_and_prefix_len = (token_end_pos == npos)
                                                      ? npos
                                                      : token_end_pos - token_pos
                                                            + CondaURL::token_prefix.size();
                return std::pair{ prefix_pos, token_and_prefix_len };
            }

            return std::pair{ std::string_view::npos, 0ul };
        }
    }

    CondaURL::CondaURL(URL&& url)
        : Base(std::move(url))
    {
    }

    auto CondaURL::parse(std::string_view url) -> CondaURL
    {
        return CondaURL(URL::parse(url));
    }

    auto CondaURL::token() const -> std::string_view
    {
        static constexpr auto npos = std::string_view::npos;

        const auto& l_path = path(Decode::no);
        const auto [pos, len] = find_token_and_prefix(l_path);
        if ((pos == npos) || (len == 0))
        {
            return "";
        }
        assert(token_prefix.size() < len);
        const auto token_len = (len != npos) ? len - token_prefix.size() : npos;
        return std::string_view(l_path).substr(pos + token_prefix.size(), token_len);
    }

    void CondaURL::set_token(std::string_view token)
    {
        static constexpr auto npos = std::string_view::npos;

        if (!is_token(token))
        {
            throw std::invalid_argument(fmt::format(R"(Invalid CondaURL token "{}")", token));
        }
        const auto [pos, len] = find_token_and_prefix(path(Decode::no));
        if ((pos == npos) || (len == 0))
        {
            throw std::invalid_argument(
                fmt::format(R"(No token template in orignial path "{}")", path(Decode::no))
            );
        }
        assert(token_prefix.size() < len);
        std::string l_path = clear_path();  // percent encoded
        const auto token_len = (len != npos) ? len - token_prefix.size() : npos;
        l_path.replace(pos + token_prefix.size(), token_len, token);
        set_path(std::move(l_path), Encode::no);
    }

    auto CondaURL::clear_token() -> bool
    {
        const auto [pos, len] = find_token_and_prefix(path(Decode::no));
        if ((pos == std::string::npos) || (len == 0))
        {
            return false;
        }
        assert(token_prefix.size() < len);
        std::string l_path = clear_path();  // percent encoded
        l_path.erase(pos, len);
        set_path(std::move(l_path), Encode::no);
        return true;
    }

    namespace
    {
        [[nodiscard]] auto find_slash_and_platform(std::string_view path)
            -> std::tuple<std::size_t, std::size_t, std::optional<Platform>>
        {
            static constexpr auto npos = std::string_view::npos;

            assert(!path.empty() && (path.front() == '/'));
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
    }

    auto CondaURL::platform() const -> std::optional<Platform>
    {
        const auto& l_path = path(Decode::no);
        const auto [pos, count, plat] = find_slash_and_platform(l_path);
        return plat;
    }

    auto CondaURL::platform_name() const -> std::string_view
    {
        static constexpr auto npos = std::string_view::npos;

        const auto& l_path = path(Decode::no);
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
        set_path(std::move(l_path), Encode::no);
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
        const auto [pos, count, plat] = find_slash_and_platform(path(Decode::no));
        if (!plat.has_value())
        {
            return false;
        }
        assert(1 < count);
        std::string l_path = clear_path();  // percent encoded
        l_path.erase(pos, count);
        set_path(std::move(l_path), Encode::no);
        return true;
    }
}
