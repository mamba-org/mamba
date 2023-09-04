// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <algorithm>
#include <cassert>
#include <string_view>

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
            const auto prefix_pos = path.find(CondaURL::token_prefix);
            if (prefix_pos == std::string_view::npos)
            {
                return std::pair{ std::string_view::npos, 0ul };
            }

            const auto token_pos = prefix_pos + CondaURL::token_prefix.size();
            const auto token_end_pos = path.find('/', token_pos);
            auto candidate_token = path.substr(token_pos);
            if (token_end_pos != std::string_view::npos)
            {
                candidate_token = candidate_token.substr(0, token_end_pos - token_pos);
            }

            if (is_token(candidate_token))
            {
                return std::pair{ prefix_pos, token_end_pos };
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
        const auto& l_path = path(Decode::no);
        const auto [pos, count] = find_token_and_prefix(l_path);
        if ((pos == std::string_view::npos) || (count == 0))
        {
            return "";
        }
        assert(token_prefix.size() < count);
        return std::string_view(l_path).substr(pos + token_prefix.size(), count - token_prefix.size());
    }

    void CondaURL::set_token(std::string_view token)
    {
        if (!is_token(token))
        {
            throw std::invalid_argument(fmt::format(R"(Invalid CondaURL token "{}")", token));
        }
        std::string l_path = clear_path();  // percent encoded
        const auto [pos, count] = find_token_and_prefix(l_path);
        if ((pos == std::string::npos) || (count == 0))
        {
            set_path(std::move(l_path), Encode::no);
            throw std::invalid_argument(
                fmt::format(R"(No token template in orignial path "{}")", path(Decode::no))
            );
        }
        assert(token_prefix.size() < count);
        l_path.replace(pos + token_prefix.size(), count - token_prefix.size(), token);
        set_path(std::move(l_path), Encode::no);
    }

    auto CondaURL::clear_token() -> bool
    {
        std::string l_path = clear_path();  // percent encoded
        const auto [pos, count] = find_token_and_prefix(l_path);
        if ((pos == std::string::npos) || (count == 0))
        {
            return false;
        }
        assert(token_prefix.size() < count);
        l_path.erase(pos, count);
        set_path(std::move(l_path), Encode::no);
        return true;
    }
}
