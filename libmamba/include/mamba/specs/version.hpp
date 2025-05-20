// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_SPECS_VERSION_HPP
#define MAMBA_SPECS_VERSION_HPP

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <fmt/format.h>

#include "mamba/specs/error.hpp"
#include "mamba/util/charconv.hpp"

namespace mamba::specs
{

    /**
     * A succession of a number and a lowercase literal.
     *
     * Comparison is done lexicographically, with the number first and the literal second.
     * Certain literals have special meaning:
     * "*" < "dev" < "_"< any other literal < "" < "post"
     */
    class VersionPartAtom
    {
    public:

        VersionPartAtom() noexcept = default;
        VersionPartAtom(std::size_t numeral) noexcept;
        VersionPartAtom(std::size_t numeral, std::string_view literal);
        // The use of a template is only meant to prevent ambiguous conversions
        template <typename Char>
        VersionPartAtom(std::size_t numeral, std::basic_string<Char> literal);

        [[nodiscard]] auto numeral() const noexcept -> std::size_t;
        [[nodiscard]] auto literal() const& noexcept -> const std::string&;
        auto literal() && noexcept -> std::string;

        [[nodiscard]] auto to_string() const -> std::string;

    private:

        // Stored in decreasing size order for performance
        std::string m_literal = "";
        std::size_t m_numeral = 0;
    };

    auto operator==(const VersionPartAtom& left, const VersionPartAtom& right) -> bool;
    auto operator!=(const VersionPartAtom& left, const VersionPartAtom& right) -> bool;
    auto operator<(const VersionPartAtom& left, const VersionPartAtom& right) -> bool;
    auto operator<=(const VersionPartAtom& left, const VersionPartAtom& right) -> bool;
    auto operator>(const VersionPartAtom& left, const VersionPartAtom& right) -> bool;
    auto operator>=(const VersionPartAtom& left, const VersionPartAtom& right) -> bool;

    extern template VersionPartAtom::VersionPartAtom(std::size_t, std::string);

    /**
     * A sequence of VersionPartAtom meant to represent a part of a version (e.g. major, minor).
     *
     * In a version like ``1.3.0post1dev``, the parts are ``1``, ``3``, and ``0post1dev``.
     * Version parts can have a arbitrary number of atoms, such as {0, "post"} {1, "dev"}
     * in ``0post1dev``.
     *
     * @see  Version::parse for how this is computed from strings.
     * @todo Use a small vector of expected size 1 if performance ar not good enough.
     */
    struct VersionPart
    {
        /** The atoms of the version part */
        std::vector<VersionPartAtom> atoms = {};

        /**
         * Whether a potential leading zero in the first atom should be considered implicit.
         *
         * During parsing of ``Version``, if a part starts with a literal atom, it is considered
         * the same as if it started with a leading ``0``.
         * For instance ``0post1dev`` is parsed in the same way as ``post1dev``.
         * Marking it as implicit enables the possibility to remove it when reconstructing a string
         * representation.
         * This is desirable for compatibility with other version formats, such as Python, where
         * a version modifier might be expressed as ``1.3.0.dev3``.
         */
        bool implicit_leading_zero = false;

        VersionPart();
        VersionPart(std::initializer_list<VersionPartAtom> init);
        VersionPart(std::vector<VersionPartAtom> atoms, bool implicit_leading_zero);

        [[nodiscard]] auto to_string() const -> std::string;
    };

    auto operator==(const VersionPart& left, const VersionPart& other) -> bool;
    auto operator!=(const VersionPart& left, const VersionPart& other) -> bool;
    auto operator<(const VersionPart& left, const VersionPart& other) -> bool;
    auto operator<=(const VersionPart& left, const VersionPart& other) -> bool;
    auto operator>(const VersionPart& left, const VersionPart& other) -> bool;
    auto operator>=(const VersionPart& left, const VersionPart& other) -> bool;

    /**
     * A sequence of VersionPart meant to represent all parts of a version.
     *
     * CommonVersion are composed of an arbitrary positive number parts, such as major, minor.
     * They are typically separated by dots, for instance the three parts in 3.0post1dev.4 are
     * {{3, ""}}, {{0, "post"}, {1, "dev"}}, and {{4, ""}}.
     *
     * @see  Version::parse for how this is computed from strings.
     * @todo Use a small vector of expected size 4 if performance ar not good enough.
     */
    using CommonVersion = std::vector<VersionPart>;

    /**
     * A version according to Conda specifications.
     *
     * A version is composed of
     * - A epoch number, usually 0;
     * - A regular version,
     * - An optional local.
     * These elements are used to lexicographicaly compare two versions.
     *
     * @see https://github.com/conda/conda/blob/main/conda/models/version.py
     */
    class Version
    {
    public:

        static constexpr char epoch_delim = '!';
        static constexpr char local_delim = '+';
        static constexpr char part_delim = '.';
        static constexpr char part_delim_alt = '-';
        static constexpr char part_delim_special = '_';

        static auto parse(std::string_view str) -> expected_parse_t<Version>;

        /** Construct version ``0.0``. */
        Version() noexcept = default;
        Version(std::size_t epoch, CommonVersion version, CommonVersion local = {}) noexcept;

        [[nodiscard]] auto epoch() const noexcept -> std::size_t;
        [[nodiscard]] auto version() const noexcept -> const CommonVersion&;
        [[nodiscard]] auto local() const noexcept -> const CommonVersion&;

        /**
         * A string representation of the version.
         *
         * May not always be the same as the parsed string (due to reconstruction) but reparsing
         * this string will give the same version.
         * ``v == Version::parse(v.to_string())``.
         */
        [[nodiscard]] auto to_string() const -> std::string;

        /**
         * A string truncated of extended representation of the version.
         *
         * Represent the string with the desired number of parts.
         * If the actual number of parts is larger, then the string is truncated.
         * If the actual number of parts is smalle, then the string is expanded with zeros.
         */
        [[nodiscard]] auto to_string(std::size_t level) const -> std::string;

        /**
         * String representation that treats ``*`` as glob pattern.
         *
         * Instead of printing them as ``0*`` (as a special literal), it formats them as ``*``.
         * In full, a version like ``*.1.*`` will print as such instead of ``0*.1.0*``.
         */
        [[nodiscard]] auto to_string_glob() const -> std::string;

        /**
         * Return true if this version starts with the other prefix.
         *
         * For instance 1.2.3 starts with 1.2 but not the opposite.
         * Because Conda versions can contain an arbitrary number of segments, some of which
         * with alpha releases, this function cannot be written as a comparison.
         * One would need to comoare with a version with infinitely pre-release segments.
         */
        [[nodiscard]] auto starts_with(const Version& prefix) const -> bool;

        /**
         * Return true if this version is a compatible upgrade to the given one.
         *
         * For instance 1.3.1 is compatible with 1.2.1 at level 0 (first component `1 == 1``),
         * at level 1 (second component `` 3 >= 2``), but not at level two (because the second
         * component is strictly larger ``3 > 2``).
         * Compatible versions are always smaller than the current version.
         */
        [[nodiscard]] auto compatible_with(const Version& older, std::size_t level) const -> bool;

    private:

        // Stored in decreasing size order for performance
        CommonVersion m_version = {};
        CommonVersion m_local = {};
        std::size_t m_epoch = 0;
    };

    auto operator==(const Version& left, const Version& other) -> bool;
    auto operator!=(const Version& left, const Version& other) -> bool;
    auto operator<(const Version& left, const Version& other) -> bool;
    auto operator<=(const Version& left, const Version& other) -> bool;
    auto operator>(const Version& left, const Version& other) -> bool;
    auto operator>=(const Version& left, const Version& other) -> bool;

    namespace version_literals
    {
        auto operator""_v(const char* str, std::size_t len) -> Version;
    }
}

template <>
struct fmt::formatter<mamba::specs::VersionPartAtom>
{
    constexpr auto parse(format_parse_context& ctx) -> format_parse_context::iterator
    {
        // make sure that range is empty
        if (ctx.begin() != ctx.end() && *ctx.begin() != '}')
        {
            throw fmt::format_error("Invalid format");
        }
        return ctx.begin();
    }

    auto format(const ::mamba::specs::VersionPartAtom atom, format_context& ctx) const
        -> format_context::iterator;
};

template <>
struct fmt::formatter<mamba::specs::VersionPart>
{
    constexpr auto parse(format_parse_context& ctx) -> format_parse_context::iterator
    {
        // make sure that range is empty
        if (ctx.begin() != ctx.end() && *ctx.begin() != '}')
        {
            throw fmt::format_error("Invalid format");
        }
        return ctx.begin();
    }

    auto format(const ::mamba::specs::VersionPart atom, format_context& ctx) const
        -> format_context::iterator;
};

template <>
struct fmt::formatter<mamba::specs::Version>
{
    enum struct FormatType
    {
        Normal,
        /**
         * The Glob pattern, as used internally ``VersionPredicate``, lets you treat ``*`` as a
         * glob pattern instead of the special character.
         * It lets you format ``*.*`` as such instead of ``0*.0*``.
         */
        Glob,
    };

    std::optional<std::size_t> m_level;
    FormatType m_type = FormatType::Normal;

    constexpr auto parse(format_parse_context& ctx) -> format_parse_context::iterator
    {
        const auto end = ctx.end();
        const auto start = ctx.begin();

        // Make sure that range is not empty
        if (start == end || *start == '}')
        {
            return start;
        }

        // Check for restricted number of segments at beginning
        std::size_t val = 0;
        auto [ptr, ec] = mamba::util::constexpr_from_chars(start, end, val);
        if (ec == std::errc())
        {
            m_level = val;
        }

        // Check for end of format spec
        if (ptr == end || *ptr == '}')
        {
            return ptr;
        }

        // Check the custom format type
        if (*ptr == 'g')
        {
            m_type = FormatType::Glob;
            ++ptr;
        }

        return ptr;
    }

    auto format(const ::mamba::specs::Version v, format_context& ctx) const
        -> format_context::iterator;
};

#endif
