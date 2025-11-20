// Copyright (c) 2025, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

#include <fmt/format.h>

#include "mamba/specs/match_spec.hpp"
#include "mamba/specs/match_spec_condition.hpp"
#include "mamba/specs/package_info.hpp"
#include "mamba/util/string.hpp"

namespace mamba::specs
{
    namespace
    {
        /**
         * Skip whitespace at the beginning of the string.
         */
        [[nodiscard]] auto skip_whitespace(std::string_view str) -> std::string_view
        {
            while (!str.empty() && std::isspace(static_cast<unsigned char>(str[0])))
            {
                str.remove_prefix(1);
            }
            return str;
        }

        /**
         * Check if the string starts with a word (case-insensitive).
         * Returns the length of the matched word if found, or 0 if not found.
         */
        [[nodiscard]] auto find_word_at_start(std::string_view str, std::string_view word)
            -> std::size_t
        {
            if (str.size() < word.size())
            {
                return 0;
            }
            // Check if it's a word boundary (start of string or whitespace before)
            // Check if it's a word boundary after the word
            if (str.size() > word.size())
            {
                auto after = str[word.size()];
                if (std::isalnum(static_cast<unsigned char>(after)) || after == '_')
                {
                    return 0;  // Not a word boundary
                }
            }
            // Case-insensitive comparison
            bool matches = std::equal(
                word.begin(),
                word.end(),
                str.begin(),
                [](char a, char b) { return std::tolower(a) == std::tolower(b); }
            );
            return matches ? word.size() : 0;
        }

        /**
         * Find the next delimiter (and, or, parentheses) in the string.
         * Returns the position and the delimiter type.
         * Case-insensitive for "and" and "or".
         */
        [[nodiscard]] auto find_next_delimiter(std::string_view str)
            -> std::pair<std::size_t, std::string_view>
        {
            constexpr std::string_view delimiters[] = { "and", "or", "(", ")" };
            std::size_t min_pos = std::string_view::npos;
            std::string_view found_delimiter = "";

            for (auto delimiter : delimiters)
            {
                if (delimiter == "and" || delimiter == "or")
                {
                    // For "and"/"or", do case-insensitive search
                    if (str.size() >= delimiter.size())
                    {
                        for (std::size_t pos = 0; pos <= str.size() - delimiter.size(); ++pos)
                        {
                            // Case-insensitive comparison
                            bool matches = std::equal(
                                delimiter.begin(),
                                delimiter.end(),
                                str.begin() + pos,
                                [](char a, char b) { return std::tolower(a) == std::tolower(b); }
                            );
                            if (matches)
                            {
                                // Check if it's a word boundary
                                bool is_start_boundary = (pos == 0) || std::isspace(str[pos - 1]);
                                bool is_end_boundary = (pos + delimiter.size() >= str.size())
                                                       || std::isspace(str[pos + delimiter.size()]);
                                if (is_start_boundary && is_end_boundary)
                                {
                                    if (pos < min_pos)
                                    {
                                        min_pos = pos;
                                        found_delimiter = delimiter;
                                    }
                                    break;  // Found the first match, move to next delimiter
                                }
                            }
                        }
                    }
                }
                else
                {
                    // For parentheses, just find the character
                    auto pos = str.find(delimiter);
                    if (pos != std::string_view::npos && pos < min_pos)
                    {
                        min_pos = pos;
                        found_delimiter = delimiter;
                    }
                }
            }

            return { min_pos, found_delimiter };
        }

        /**
         * Parse a primary condition (matchspec or parenthesized expression).
         */
        [[nodiscard]] auto parse_primary_condition(std::string_view input)
            -> expected_parse_t<std::pair<MatchSpecCondition, std::string_view>>
        {
            input = skip_whitespace(input);
            if (input.empty())
            {
                return make_unexpected_parse("Empty condition");
            }

            // Check for parenthesized expression
            if (input[0] == '(')
            {
                // Find matching closing parenthesis
                std::size_t depth = 0;
                std::size_t pos = 0;
                for (; pos < input.size(); ++pos)
                {
                    if (input[pos] == '(')
                    {
                        ++depth;
                    }
                    else if (input[pos] == ')')
                    {
                        --depth;
                        if (depth == 0)
                        {
                            break;
                        }
                    }
                }

                if (depth != 0)
                {
                    return make_unexpected_parse("Unmatched opening parenthesis");
                }

                // Parse the content inside parentheses
                auto inner = input.substr(1, pos - 1);
                auto result = MatchSpecCondition::parse(inner);
                if (!result.has_value())
                {
                    return make_unexpected_parse(result.error().what());
                }

                // Return the condition and the remaining string
                auto remaining = skip_whitespace(input.substr(pos + 1));
                return std::make_pair(std::move(result.value()), remaining);
            }

            // Parse a matchspec (everything up to the next delimiter)
            auto [delim_pos, delimiter] = find_next_delimiter(input);
            std::string_view matchspec_str;
            std::string_view remaining;

            if (delim_pos == std::string_view::npos)
            {
                // No delimiter found, consume the entire string
                matchspec_str = util::strip(input);
                remaining = "";
            }
            else
            {
                matchspec_str = util::strip(input.substr(0, delim_pos));
                remaining = input.substr(delim_pos);
            }

            if (matchspec_str.empty())
            {
                return make_unexpected_parse("Empty matchspec in condition");
            }

            // Parse the matchspec
            auto matchspec = MatchSpec::parse(matchspec_str);
            if (!matchspec.has_value())
            {
                return make_unexpected_parse(
                    fmt::format(
                        "Failed to parse matchspec '{}': {}",
                        matchspec_str,
                        matchspec.error().what()
                    )
                );
            }

            MatchSpecCondition::MatchSpecCondition_ cond{ std::move(matchspec.value()) };
            return std::make_pair(MatchSpecCondition(std::move(cond)), remaining);
        }

        /**
         * Parse AND expressions (higher precedence).
         */
        [[nodiscard]] auto parse_and_condition(std::string_view input)
            -> expected_parse_t<std::pair<MatchSpecCondition, std::string_view>>
        {
            // Parse the first primary condition
            auto first_result = parse_primary_condition(input);
            if (!first_result.has_value())
            {
                return make_unexpected_parse(first_result.error().what());
            }

            auto [first_cond, remaining] = std::move(first_result.value());
            remaining = skip_whitespace(remaining);

            // Parse any additional AND conditions
            auto and_len = find_word_at_start(remaining, "and");
            while (!remaining.empty() && and_len > 0)
            {
                remaining = skip_whitespace(remaining.substr(and_len));  // Skip "and"/"AND"/"And"
                if (remaining.empty())
                {
                    return make_unexpected_parse("Expected condition after 'and'");
                }

                auto next_result = parse_primary_condition(remaining);
                if (!next_result.has_value())
                {
                    return make_unexpected_parse(next_result.error().what());
                }

                auto [next_cond, next_remaining] = std::move(next_result.value());
                // Combine with AND
                auto and_cond = std::make_unique<MatchSpecCondition::And>(
                    std::make_unique<MatchSpecCondition>(std::move(first_cond)),
                    std::make_unique<MatchSpecCondition>(std::move(next_cond))
                );
                first_cond = MatchSpecCondition(std::move(and_cond));
                remaining = skip_whitespace(next_remaining);
                and_len = find_word_at_start(remaining, "and");
            }

            return std::make_pair(std::move(first_cond), remaining);
        }

        /**
         * Parse OR expressions (lower precedence).
         */
        [[nodiscard]] auto parse_or_condition(std::string_view input)
            -> expected_parse_t<std::pair<MatchSpecCondition, std::string_view>>
        {
            // Parse the first AND condition
            auto first_result = parse_and_condition(input);
            if (!first_result.has_value())
            {
                return make_unexpected_parse(first_result.error().what());
            }

            auto [first_cond, remaining] = std::move(first_result.value());
            remaining = skip_whitespace(remaining);

            // Parse any additional OR conditions
            auto or_len = find_word_at_start(remaining, "or");
            while (!remaining.empty() && or_len > 0)
            {
                remaining = skip_whitespace(remaining.substr(or_len));  // Skip "or"/"OR"/"Or"
                if (remaining.empty())
                {
                    return make_unexpected_parse("Expected condition after 'or'");
                }

                auto next_result = parse_and_condition(remaining);
                if (!next_result.has_value())
                {
                    return make_unexpected_parse(next_result.error().what());
                }

                auto [next_cond, next_remaining] = std::move(next_result.value());
                // Combine with OR
                auto or_cond = std::make_unique<MatchSpecCondition::Or>(
                    std::make_unique<MatchSpecCondition>(std::move(first_cond)),
                    std::make_unique<MatchSpecCondition>(std::move(next_cond))
                );
                first_cond = MatchSpecCondition(std::move(or_cond));
                remaining = skip_whitespace(next_remaining);
                or_len = find_word_at_start(remaining, "or");
            }

            return std::make_pair(std::move(first_cond), remaining);
        }
    }

    // MatchSpecCondition implementation

    MatchSpecCondition::MatchSpecCondition(MatchSpecCondition_ cond)
        : value(std::move(cond))
    {
    }

    MatchSpecCondition::MatchSpecCondition(std::unique_ptr<And> cond)
        : value(std::move(cond))
    {
    }

    MatchSpecCondition::MatchSpecCondition(std::unique_ptr<Or> cond)
        : value(std::move(cond))
    {
    }

    MatchSpecCondition::MatchSpecCondition(const MatchSpecCondition& other)
        : value(
              std::visit(
                  [](const auto& val) -> variant_type
                  {
                      using T = std::decay_t<decltype(val)>;
                      if constexpr (std::is_same_v<T, MatchSpecCondition_>)
                      {
                          return val;  // Copy the struct directly
                      }
                      else if constexpr (std::is_same_v<T, std::unique_ptr<And>>)
                      {
                          return std::make_unique<And>(
                              std::make_unique<MatchSpecCondition>(*val->left),
                              std::make_unique<MatchSpecCondition>(*val->right)
                          );
                      }
                      else if constexpr (std::is_same_v<T, std::unique_ptr<Or>>)
                      {
                          return std::make_unique<Or>(
                              std::make_unique<MatchSpecCondition>(*val->left),
                              std::make_unique<MatchSpecCondition>(*val->right)
                          );
                      }
                  },
                  other.value
              )
          )
    {
    }

    MatchSpecCondition& MatchSpecCondition::operator=(const MatchSpecCondition& other)
    {
        if (this != &other)
        {
            value = std::visit(
                [](const auto& val) -> variant_type
                {
                    using T = std::decay_t<decltype(val)>;
                    if constexpr (std::is_same_v<T, MatchSpecCondition_>)
                    {
                        return val;  // Copy the struct directly
                    }
                    else if constexpr (std::is_same_v<T, std::unique_ptr<And>>)
                    {
                        return std::make_unique<And>(
                            std::make_unique<MatchSpecCondition>(*val->left),
                            std::make_unique<MatchSpecCondition>(*val->right)
                        );
                    }
                    else if constexpr (std::is_same_v<T, std::unique_ptr<Or>>)
                    {
                        return std::make_unique<Or>(
                            std::make_unique<MatchSpecCondition>(*val->left),
                            std::make_unique<MatchSpecCondition>(*val->right)
                        );
                    }
                },
                other.value
            );
        }
        return *this;
    }

    auto MatchSpecCondition::parse(std::string_view condition_str)
        -> expected_parse_t<MatchSpecCondition>
    {
        auto trimmed = util::strip(condition_str);
        if (trimmed.empty())
        {
            return make_unexpected_parse("Empty condition string");
        }

        auto result = parse_or_condition(trimmed);
        if (!result.has_value())
        {
            return make_unexpected_parse(result.error().what());
        }

        auto [cond, remaining] = std::move(result.value());
        remaining = skip_whitespace(remaining);

        if (!remaining.empty())
        {
            return make_unexpected_parse(
                fmt::format("Unexpected characters after condition: '{}'", remaining)
            );
        }

        return { std::move(cond) };
    }

    auto MatchSpecCondition::contains(const PackageInfo& pkg) const -> bool
    {
        return std::visit(
            [&](const auto& cond) -> bool
            {
                using T = std::decay_t<decltype(cond)>;
                if constexpr (std::is_same_v<T, MatchSpecCondition_>)
                {
                    return cond.spec.contains_except_channel(pkg);
                }
                else if constexpr (std::is_same_v<T, std::unique_ptr<And>>)
                {
                    return cond->left->contains(pkg) && cond->right->contains(pkg);
                }
                else if constexpr (std::is_same_v<T, std::unique_ptr<Or>>)
                {
                    return cond->left->contains(pkg) || cond->right->contains(pkg);
                }
            },
            value
        );
    }

    auto MatchSpecCondition::to_string() const -> std::string
    {
        return std::visit(
            [](const auto& cond) -> std::string
            {
                using T = std::decay_t<decltype(cond)>;
                if constexpr (std::is_same_v<T, MatchSpecCondition_>)
                {
                    return cond.spec.to_string();
                }
                else if constexpr (std::is_same_v<T, std::unique_ptr<And>>)
                {
                    return fmt::format("({} and {})", cond->left->to_string(), cond->right->to_string());
                }
                else if constexpr (std::is_same_v<T, std::unique_ptr<Or>>)
                {
                    return fmt::format("({} or {})", cond->left->to_string(), cond->right->to_string());
                }
            },
            value
        );
    }

    auto MatchSpecCondition::operator==(const MatchSpecCondition& other) const -> bool
    {
        return std::visit(
            [&](const auto& this_cond) -> bool
            {
                return std::visit(
                    [&](const auto& other_cond) -> bool
                    {
                        using ThisT = std::decay_t<decltype(this_cond)>;
                        using OtherT = std::decay_t<decltype(other_cond)>;

                        if constexpr (std::is_same_v<ThisT, OtherT>)
                        {
                            if constexpr (std::is_same_v<ThisT, MatchSpecCondition_>)
                            {
                                return this_cond.spec == other_cond.spec;
                            }
                            else if constexpr (std::is_same_v<ThisT, std::unique_ptr<And>>)
                            {
                                return *this_cond->left == *other_cond->left
                                       && *this_cond->right == *other_cond->right;
                            }
                            else if constexpr (std::is_same_v<ThisT, std::unique_ptr<Or>>)
                            {
                                return *this_cond->left == *other_cond->left
                                       && *this_cond->right == *other_cond->right;
                            }
                        }
                        return false;
                    },
                    other.value
                );
            },
            value
        );
    }

    // MatchSpecCondition_ equality
    auto MatchSpecCondition::MatchSpecCondition_::operator==(const MatchSpecCondition_& other) const
        -> bool
    {
        return spec == other.spec;
    }

    // And equality
    auto MatchSpecCondition::And::operator==(const And& other) const -> bool
    {
        return *left == *other.left && *right == *other.right;
    }

    // Or equality
    auto MatchSpecCondition::Or::operator==(const Or& other) const -> bool
    {
        return *left == *other.left && *right == *other.right;
    }
}

// Hash implementations (must be in std namespace)
auto
std::hash<mamba::specs::MatchSpecCondition>::operator()(const mamba::specs::MatchSpecCondition& cond) const
    -> std::size_t
{
    return std::hash<std::string>{}(cond.to_string());
}

auto
std::hash<mamba::specs::MatchSpecCondition::MatchSpecCondition_>::operator()(
    const mamba::specs::MatchSpecCondition::MatchSpecCondition_& cond
) const -> std::size_t
{
    return std::hash<mamba::specs::MatchSpec>{}(cond.spec);
}

auto
std::hash<mamba::specs::MatchSpecCondition::And>::operator()(
    const mamba::specs::MatchSpecCondition::And& cond
) const -> std::size_t
{
    auto h1 = std::hash<mamba::specs::MatchSpecCondition>{}(*cond.left);
    auto h2 = std::hash<mamba::specs::MatchSpecCondition>{}(*cond.right);
    return h1 ^ (h2 << 1);
}

auto
std::hash<mamba::specs::MatchSpecCondition::Or>::operator()(
    const mamba::specs::MatchSpecCondition::Or& cond
) const -> std::size_t
{
    auto h1 = std::hash<mamba::specs::MatchSpecCondition>{}(*cond.left);
    auto h2 = std::hash<mamba::specs::MatchSpecCondition>{}(*cond.right);
    return h1 ^ (h2 << 1);
}
