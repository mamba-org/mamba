// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <algorithm>
#include <cassert>
#include <charconv>
#include <compare>
#include <iterator>
#include <optional>
#include <tuple>

#include "mamba/specs/version.hpp"
#include "mamba/util/cast.hpp"
#include "mamba/util/string.hpp"

#include "specs/version_spec_impl.hpp"

namespace mamba::specs
{
    namespace
    {
        /**
         * Compare two ranges where some trailing elements can be considered as empty.
         *
         * If ``0`` is considered "empty" then all the ranges ``[1, 2] and ``[1, 2, 0]``,
         * ``[1, 2, 0, 0]`` are considered equal, however ``[1, 2]`` and ``[1, 0, 2]`` are not.
         * Similarly ``[1, 1] is less than ``[1, 2, 0]`` but more than ``[1, 1, -1]``
         * because ``-1 < 0``.
         *
         * @return The comparison between the two sequences
         * @return The first index where the two sequences diverge.
         */
        template <typename Iter1, typename Iter2, typename Empty1, typename Empty2, typename Cmp>
        constexpr auto lexicographical_compare_three_way_trailing(
            Iter1 first1,
            Iter1 last1,
            Iter2 first2,
            Iter2 last2,
            const Empty1& empty1,
            const Empty2& empty2,
            Cmp comp
        ) -> std::pair<std::strong_ordering, std::size_t>
        {
            assert(std::distance(first1, last1) >= 0);
            assert(std::distance(first2, last2) >= 0);

            auto iter1 = first1;
            auto iter2 = first2;
            for (; (iter1 != last1) && (iter2 != last2); ++iter1, ++iter2)
            {
                if (auto c = comp(*iter1, *iter2); c != std::strong_ordering::equal)
                {
                    return { c, static_cast<std::size_t>(std::distance(first1, iter1)) };
                }
            }

            // They have the same leading elements but 1 has more elements
            // We do a lexicographic comparison with an infinite sequence of empties
            if ((iter1 != last1))
            {
                for (; iter1 != last1; ++iter1)
                {
                    if (auto c = comp(*iter1, empty2); c != std::strong_ordering::equal)
                    {
                        return { c, static_cast<std::size_t>(std::distance(first1, iter1)) };
                    }
                }
            }
            // first2 != last2
            // They have the same leading elements but 2 has more elements
            // We do a lexicographic comparison with an infinite sequence of empties
            if ((iter2 != last2))
            {
                for (; iter2 != last2; ++iter2)
                {
                    if (auto c = comp(empty1, *iter2); c != std::strong_ordering::equal)
                    {
                        return { c, static_cast<std::size_t>(std::distance(first2, iter2)) };
                    }
                }
            }
            // They have the same elements
            return { std::strong_ordering::equal,
                     static_cast<std::size_t>(std::distance(first1, iter1)) };
        }

        template <typename Iter1, typename Iter2, typename Empty, typename Cmp>
        constexpr auto lexicographical_compare_three_way_trailing(
            Iter1 first1,
            Iter1 last1,
            Iter2 first2,
            Iter2 last2,
            const Empty& empty,
            Cmp comp
        ) -> std::pair<std::strong_ordering, std::size_t>
        {
            return lexicographical_compare_three_way_trailing(
                first1,
                last1,
                first2,
                last2,
                empty,
                empty,
                comp
            );
        }

    }

    /***************************************
     *  Implementation of VersionPartAtom  *
     ***************************************/

    VersionPartAtom::VersionPartAtom(std::size_t numeral) noexcept
        : m_numeral{ numeral }
    {
    }

    VersionPartAtom::VersionPartAtom(std::size_t numeral, std::string_view literal)
        : m_literal{ util::to_lower(literal) }
        , m_numeral{ numeral }
    {
    }

    template <typename Char>
    VersionPartAtom::VersionPartAtom(std::size_t numeral, std::basic_string<Char> literal)
        : m_literal{ util::to_lower(std::move(literal)) }
        , m_numeral{ numeral }
    {
    }

    template VersionPartAtom::VersionPartAtom(std::size_t, std::string);

    auto VersionPartAtom::numeral() const noexcept -> std::size_t
    {
        return m_numeral;
    }

    auto VersionPartAtom::literal() const& noexcept -> const std::string&
    {
        return m_literal;
    }

    auto VersionPartAtom::literal() && noexcept -> std::string
    {
        return std::move(m_literal);
    }

    auto VersionPartAtom::to_string() const -> std::string
    {
        return fmt::format("{}", *this);
    }

    std::strong_ordering operator<=>(const VersionPartAtom& a, const VersionPartAtom& b)
    {
        const auto num_ord = a.numeral() <=> b.numeral();
        if (num_ord != std::strong_ordering::equal)
        {
            return num_ord;
        }

        // Certain literals have special meaning we map then to a priority
        // 0 meaning regular string
        auto lit_priority = [](const auto& l) -> int
        {
            if (l == "*")
            {
                return -3;
            }
            if (l == "dev")
            {
                return -2;
            }
            if (l == "_")
            {
                return -1;
            }
            if (l == "")
            {
                return 1;
            }
            if (l == "post")
            {
                return 2;
            }
            return 0;
        };
        const auto a_lit_val = lit_priority(a.literal());
        const auto b_lit_val = lit_priority(b.literal());
        // If two regular string, we need to use string comparison
        if ((a_lit_val == 0) && (b_lit_val == 0))
        {
            return a.literal() <=> b.literal();
        }
        return a_lit_val <=> b_lit_val;
    }
}

auto
fmt::formatter<mamba::specs::VersionPartAtom>::format(
    const ::mamba::specs::VersionPartAtom atom,
    format_context& ctx
) const -> format_context::iterator
{
    return fmt::format_to(ctx.out(), "{}{}", atom.numeral(), atom.literal());
}

namespace mamba::specs
{

    /***********************************
     *  Implementation of VersionPart  *
     ***********************************/

    VersionPart::VersionPart()
        : atoms()
        , implicit_leading_zero(false)
    {
    }

    VersionPart::VersionPart(std::initializer_list<VersionPartAtom> init)
        : atoms(init)
    {
    }

    VersionPart::VersionPart(std::vector<VersionPartAtom> p_atoms, bool p_implicit_leading_zero)
        : atoms(std::move(p_atoms))
        , implicit_leading_zero(p_implicit_leading_zero)
    {
    }

    auto VersionPart::to_string() const -> std::string
    {
        return fmt::format("{}", *this);
    }

    auto operator==(const VersionPart& lhs, const VersionPart& rhs) -> bool
    {
        return (lhs <=> rhs) == std::strong_ordering::equal;
    }

    auto operator<=>(const VersionPart& lhs, const VersionPart& rhs) -> std::strong_ordering
    {
        return lexicographical_compare_three_way_trailing(
                   lhs.atoms.cbegin(),
                   lhs.atoms.cend(),
                   rhs.atoms.cbegin(),
                   rhs.atoms.cend(),
                   VersionPartAtom{},
                   [](const auto& x, const auto& y) { return x <=> y; }
        ).first;
    }
}

auto
fmt::formatter<mamba::specs::VersionPart>::format(
    const ::mamba::specs::VersionPart part,
    format_context& ctx
) const -> format_context::iterator
{
    auto out = ctx.out();
    if (part.atoms.empty())
    {
        return out;
    }

    const auto& first = part.atoms.front();
    if (part.implicit_leading_zero && (first.numeral() == 0) && (!first.literal().empty()))
    {
        // The implicit leading zero is omitted
        out = fmt::format_to(out, "{}", first.literal());
    }
    else
    {
        out = fmt::format_to(out, "{}", first);
    }

    const auto n_atoms = part.atoms.size();
    for (std::size_t i = 1; i < n_atoms; ++i)
    {
        out = fmt::format_to(out, "{}", part.atoms[i]);
    }
    return out;
}

namespace mamba::specs
{

    /*******************************
     *  Implementation of Version  *
     *******************************/

    Version::Version(std::size_t epoch, CommonVersion version, CommonVersion local) noexcept
        : m_version{ std::move(version) }
        , m_local{ std::move(local) }
        , m_epoch{ epoch }
    {
    }

    auto Version::epoch() const noexcept -> std::size_t
    {
        return m_epoch;
    }

    auto Version::version() const noexcept -> const CommonVersion&
    {
        return m_version;
    }

    auto Version::local() const noexcept -> const CommonVersion&
    {
        return m_local;
    }

    auto Version::to_string() const -> std::string
    {
        return fmt::format("{}", *this);
    }

    auto Version::to_string(std::size_t level) const -> std::string
    {
        // We should be able to do, as it works with numbers but it is not clear how this works
        // with the custom parser
        // return fmt::format("{:{}}", *this, level);
        auto fmt = fmt::format("{{:{}}}", level);
        return fmt::format(fmt::runtime(fmt), *this);
    }

    auto Version::to_string_glob() const -> std::string
    {
        return fmt::format("{:g}", *this);
    }

    auto operator==(const Version& left, const Version& right) -> bool
    {
        return (left <=> right) == std::strong_ordering::equal;
    }

    auto operator<=>(const CommonVersion& lhs, const CommonVersion& rhs) -> std::strong_ordering
    {
        return lexicographical_compare_three_way_trailing(
                   lhs.cbegin(),
                   lhs.cend(),
                   rhs.cbegin(),
                   rhs.cend(),
                   VersionPart{},
                   [](const auto& x, const auto& y) { return x <=> y; }
        ).first;
    }

    auto operator<=>(const Version& lhs, const Version& rhs) -> std::strong_ordering
    {
        if (auto c = lhs.epoch() <=> rhs.epoch(); c != std::strong_ordering::equal)
        {
            return c;
        }
        if (auto c = lhs.version() <=> rhs.version(); c != std::strong_ordering::equal)
        {
            return c;
        }
        return lhs.local() <=> rhs.local();
    }

    namespace
    {
        struct AlwaysEqual
        {
        };

        [[maybe_unused]] auto starts_with_three_way(const AlwaysEqual&, const AlwaysEqual&)
            -> std::strong_ordering
        {
            // This comparison should not happen with the current usage.
            assert(false);
            return std::strong_ordering::equal;
        }

        template <typename T>
        auto starts_with_three_way(const AlwaysEqual&, const T&) -> std::strong_ordering
        {
            return std::strong_ordering::equal;
        }

        template <typename T>
        auto starts_with_three_way(const T&, const AlwaysEqual&) -> std::strong_ordering
        {
            return std::strong_ordering::equal;
        }

        auto starts_with_three_way(const VersionPartAtom& a, const VersionPartAtom& b)
            -> std::strong_ordering
        {
            if ((a.numeral() == b.numeral()) && b.literal().empty())
            {
                return std::strong_ordering::equal;
            }
            return a <=> b;
        }

        auto starts_with_three_way(const VersionPart& a, const VersionPart& b) -> std::strong_ordering
        {
            return lexicographical_compare_three_way_trailing(
                       a.atoms.cbegin(),
                       a.atoms.cend(),
                       b.atoms.cbegin(),
                       b.atoms.cend(),
                       VersionPartAtom{},
                       AlwaysEqual{},
                       [](const auto& x, const auto& y) { return starts_with_three_way(x, y); }
            ).first;
        }

        auto starts_with_three_way(const CommonVersion& a, const CommonVersion& b)
            -> std::strong_ordering
        {
            return lexicographical_compare_three_way_trailing(
                       a.cbegin(),
                       a.cend(),
                       b.cbegin(),
                       b.cend(),
                       VersionPart{},
                       AlwaysEqual{},
                       [](const auto& x, const auto& y) { return starts_with_three_way(x, y); }
            ).first;
        }

        auto starts_with_three_way(const Version& a, const Version& b) -> std::strong_ordering
        {
            if (auto c = a.epoch() <=> b.epoch(); c != std::strong_ordering::equal)
            {
                return c;
            }
            if (b.local().empty())
            {
                return starts_with_three_way(a.version(), b.version());
            }
            if (auto c = a.version() <=> b.version(); c != std::strong_ordering::equal)
            {
                return c;
            }
            return starts_with_three_way(a.local(), b.local());
        }
    }

    auto Version::starts_with(const Version& prefix) const -> bool
    {
        return starts_with_three_way(*this, prefix) == std::strong_ordering::equal;
    }

    namespace
    {
        auto
        compatible_with_impl(const CommonVersion& newer, const CommonVersion& older, std::size_t level)
            -> bool
        {
            auto [cmp, idx] = lexicographical_compare_three_way_trailing(
                newer.cbegin(),
                newer.cend(),
                older.cbegin(),
                older.cend(),
                VersionPart{},
                [](const auto& x, const auto& y) { return x <=> y; }
            );

            return (cmp == std::strong_ordering::equal)
                   || ((cmp == std::strong_ordering::greater) && (idx >= level));
        }
    }

    auto Version::compatible_with(const Version& older, std::size_t level) const -> bool
    {
        return (epoch() == older.epoch()) && compatible_with_impl(version(), older.version(), level)
               && compatible_with_impl(local(), older.local(), level);
    }

    namespace
    {
        template <typename Int>
        auto to_int(std::string_view str) -> std::optional<Int>
        {
            Int val = 0;
            const auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), val);
            if ((ec != std::errc()) || (ptr != (str.data() + str.size()))) [[unlikely]]
            {
                return {};
            }
            return val;
        }

        template <typename Int>
        auto parse_leading_epoch(std::string_view str)
            -> expected_parse_t<std::pair<Int, std::string_view>>
        {
            const auto delim_pos = str.find(Version::epoch_delim);
            // No epoch is specified
            if (delim_pos == std::string_view::npos) [[likely]]
            {
                return { { Int(0), str } };
            }
            if (delim_pos == 0)
            {
                return make_unexpected_parse(
                    fmt::format("Empty epoch delimited by '{}'.", Version::epoch_delim)
                );
            }

            const auto epoch_str = str.substr(0, delim_pos);
            const auto maybe_int = to_int<Int>(epoch_str);
            // Epoch is not a number (or empty)
            if (!maybe_int.has_value())
            {
                return make_unexpected_parse(
                    fmt::format("Epoch should be a number, got '{}'.", epoch_str)
                );
            }
            // Found an epoch
            return { { maybe_int.value(), str.substr(delim_pos + 1) } };
        }

        template <typename Int>
        auto parse_leading_integer(std::string_view str) -> std::pair<Int, std::string_view>
        {
            const auto [integer_str, rest] = util::lstrip_if_parts(
                str,
                [](char c) { return util::is_digit(c); }
            );
            auto maybe_integer = to_int<Int>(integer_str);
            assert(maybe_integer.has_value());
            return { maybe_integer.value(), rest };
        }

        auto parse_leading_literal(std::string_view str)
            -> std::pair<std::string_view, std::string_view>
        {
            const auto [literal, rest] = util::lstrip_if_parts(
                str,
                [](char c) { return !util::is_digit(c); }
            );
            return { literal, rest };
        }

        auto parse_leading_part_atom(std::string_view str)
            -> std::pair<VersionPartAtom, std::string_view>
        {
            assert(!str.empty());

            std::size_t numeral = 0;
            std::string_view literal = {};
            auto tail = std::string_view{};
            if (util::is_digit(str.front()))
            {
                std::tie(numeral, tail) = parse_leading_integer<std::size_t>(str);
            }
            else
            {
                tail = str;
            }
            std::tie(literal, tail) = parse_leading_literal(tail);
            return { { numeral, literal }, tail };
        }

        auto parse_part(std::string_view str) -> VersionPart
        {
            assert(!str.empty());

            auto atoms = VersionPart();
            atoms.implicit_leading_zero = !util::is_digit(str.front());

            while (!str.empty())
            {
                atoms.atoms.emplace_back();
                std::tie(atoms.atoms.back(), str) = parse_leading_part_atom(str);
            }
            return atoms;
        }

        auto check_common_version(std::string_view str) -> expected_parse_t<void>
        {
            // `_` and `-` delimiter cannot be used together.
            // Special meaning for `_` at the end of the string.
            if ((str.find(Version::part_delim_alt) < str.size())
                && (str.find(Version::part_delim_special) < str.size() - 1)) [[unlikely]]
            {
                return make_unexpected_parse(
                    fmt::format(
                        "Cannot use both '{}' and '{}' delimiters in {}'.",
                        Version::part_delim_alt,
                        Version::part_delim_special,
                        str
                    )
                );
            }

            auto allowed_char = [](char c) -> bool
            {
                return util::is_alphanum(c)                   //
                       || (c == Version::part_delim)          //
                       || (c == Version::part_delim_alt)      //
                       || (c == Version::part_delim_special)  //
                       || (c == '*');                         // Weirdly but it is in conda tests
            };
            if (std::find_if_not(str.cbegin(), str.cend(), allowed_char) != str.cend())
            {
                return make_unexpected_parse(
                    fmt::format("Version contains invalid characters in {}.", str)
                );
            }
            return {};
        }

        auto parse_common_version(std::string_view str) -> expected_parse_t<CommonVersion>
        {
            assert(!str.empty());
            if (auto outcome = check_common_version(str); !outcome)
            {
                return make_unexpected_parse(outcome.error());
            }

            static constexpr auto delims_buf = std::array{
                Version::part_delim,
                Version::part_delim_alt,
                Version::part_delim_special,
            };
            static constexpr auto delims = std::string_view{ delims_buf.data(), delims_buf.size() };

            CommonVersion parts = {};
            auto tail = str;
            std::size_t tail_delim_pos = 0;
            while (true)
            {
                tail_delim_pos = tail.find_first_of(delims);
                // `_` is both a delimiter and has special meaning.
                // We need to check if it is at the end and omit it
                if ((tail_delim_pos == tail.size() - 1)
                    && tail[tail_delim_pos] == Version::part_delim_special)
                {
                    tail_delim_pos = std::string_view::npos;
                }
                // One of the part is empty
                if ((tail_delim_pos == 0) || (tail_delim_pos == tail.size() - 1)) [[unlikely]]
                {
                    return make_unexpected_parse(fmt::format("Empty part in '{}'.", str));
                }
                parts.push_back(parse_part(tail.substr(0, tail_delim_pos)));
                if (tail_delim_pos == std::string_view::npos)
                {
                    break;
                }
                tail = tail.substr(tail_delim_pos + 1);
            }
            return { std::move(parts) };
        }

        auto parse_trailing_local_version(std::string_view str)
            -> expected_parse_t<std::pair<std::string_view, CommonVersion>>
        {
            const auto delim_pos = str.rfind(Version::local_delim);
            // No local is specified
            if (delim_pos == std::string_view::npos) [[likely]]
            {
                return { { str, {} } };
            }
            // local specified but empty
            if (delim_pos + 1 == str.size())
            {
                return make_unexpected_parse(
                    fmt::format("Empty local version delimited by '{}'.", Version::local_delim)
                );
            }
            return parse_common_version(str.substr(delim_pos + 1))
                .transform([&](CommonVersion&& version)
                           { return std::pair{ str.substr(0, delim_pos), std::move(version) }; });
        }

        auto parse_version(std::string_view str) -> expected_parse_t<CommonVersion>
        {
            if (str.empty())
            {
                return make_unexpected_parse("Empty version.");
            }
            return parse_common_version(str);
        }
    }

    auto Version::parse(std::string_view str) -> expected_parse_t<Version>
    {
        str = util::strip(str);

        auto make_unexpected = [&str](const ParseError& error)
        {
            return make_unexpected_parse(
                fmt::format(R"(Error parsing version "{}". {})", str, error.what())
            );
        };

        auto epoch_rest = parse_leading_epoch<std::size_t>(str);
        if (!epoch_rest)
        {
            return make_unexpected(epoch_rest.error());
        }

        auto version_and_local = parse_trailing_local_version(epoch_rest->second);
        if (!version_and_local)
        {
            return make_unexpected(version_and_local.error());
        }

        auto version = parse_version(version_and_local->first);
        if (!version)
        {
            return make_unexpected(version.error());
        }

        return { {
            /* .epoch= */ epoch_rest->first,
            /* .version= */ std::move(version).value(),
            /* .local= */ std::move(version_and_local)->second,
        } };
    }

    namespace version_literals
    {
        auto operator""_v(const char* str, std::size_t len) -> Version
        {
            return Version::parse(std::literals::string_view_literals::operator""sv(str, len))
                .or_else([](ParseError&& error) { throw std::move(error); })
                .value();
        }
    }
}

auto
fmt::formatter<mamba::specs::Version>::format(const ::mamba::specs::Version v, format_context& ctx) const
    -> format_context::iterator
{
    auto out = ctx.out();
    if (v.epoch() != 0)
    {
        out = fmt::format_to(ctx.out(), "{}!", v.epoch());
    }


    auto format_version_to = [this](auto l_out, const auto& version)
    {
        const auto n_levels = m_level.value_or(version.size());
        for (std::size_t i = 0; i < n_levels; ++i)
        {
            if (i != 0)
            {
                l_out = fmt::format_to(l_out, ".");
            }
            if (i < version.size())
            {
                if (m_type == FormatType::Glob && version[i] == mamba::specs::VERSION_GLOB_SEGMENT)
                {
                    l_out = fmt::format_to(l_out, "{}", mamba::specs::GLOB_PATTERN_STR);
                }
                else
                {
                    l_out = fmt::format_to(l_out, "{}", version[i]);
                }
            }
            else
            {
                l_out = fmt::format_to(l_out, "0");
            }
        }
        return l_out;
    };
    out = format_version_to(out, v.version());
    if (!v.local().empty())
    {
        out = fmt::format_to(out, "+");
        out = format_version_to(out, v.local());
    }
    return out;
}
