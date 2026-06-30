// Copyright (c) 2026, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <cctype>
#include <charconv>
#include <chrono>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>

#include "mamba/core/exclude_newer.hpp"
#include "mamba/util/string.hpp"

namespace mamba
{
    namespace
    {
        // Disambiguate the char overload for use with strip_if/lstrip_if templates.
        constexpr auto is_space = static_cast<bool (*)(char)>(util::is_space);

        using CutoffInstant = std::chrono::sys_seconds;
        using SysTime = std::chrono::sys_time<std::chrono::seconds>;

        /** Convert an internal UTC instant to Unix epoch seconds for the public API. */
        [[nodiscard]] auto to_unix_seconds(CutoffInstant instant) -> std::uint64_t
        {
            return static_cast<std::uint64_t>(instant.time_since_epoch().count());
        }

        /**
         * Parse ``value`` with ``std::chrono::parse`` using ``fmt``.
         *
         * Returns ``std::nullopt`` when parsing fails or trailing characters remain.
         */
        template <typename T>
        [[nodiscard]] auto parse_chrono(std::string_view value, const char* fmt) -> std::optional<T>
        {
            std::istringstream stream{ std::string(value) };
            T out{};
            stream >> std::chrono::parse(fmt, out);
            if (stream.fail() || stream.peek() != std::istringstream::traits_type::eof())
            {
                return std::nullopt;
            }
            return out;
        }

        /** Case-insensitive equality for ASCII strings. */
        [[nodiscard]] auto equals_ci(std::string_view value, std::string_view expected) -> bool
        {
            return value.size() == expected.size()
                   && std::equal(
                       value.begin(),
                       value.end(),
                       expected.begin(),
                       [](char lhs, char rhs) { return util::to_lower(lhs) == util::to_lower(rhs); }
                   );
        }

        /** Build a ``std::invalid_argument`` for an unparsable ``exclude_newer`` value. */
        [[nodiscard]] auto invalid_exclude_newer(std::string_view value) -> std::invalid_argument
        {
            return std::invalid_argument(
                "Invalid exclude_newer value '" + std::string(value)
                + "'; use e.g. 7d, P7D, 2026-04-01, or 2026-04-01T12:00:00Z"
            );
        }

        /**
         * Parse a leading unsigned integer from ``value`` and advance past the consumed digits.
         *
         * Used by compact duration parsing (e.g. ``7d``).
         */
        [[nodiscard]] auto parse_uint_prefix(std::string_view& value) -> std::optional<std::uint64_t>
        {
            if (value.empty() || !std::isdigit(static_cast<unsigned char>(value.front())))
            {
                return std::nullopt;
            }
            std::uint64_t number = 0;
            const auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), number);
            if (ec != std::errc())
            {
                return std::nullopt;
            }
            value.remove_prefix(static_cast<std::size_t>(ptr - value.data()));
            return number;
        }

        /** Parse ``value`` as an unsigned integer that must consume the entire string. */
        [[nodiscard]] auto parse_fixed_uint(std::string_view value) -> std::optional<std::uint64_t>
        {
            std::uint64_t number = 0;
            const auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), number);
            if (ec != std::errc() || ptr != value.data() + value.size())
            {
                return std::nullopt;
            }
            return number;
        }

        /** Parse a plain integer duration in seconds (e.g. ``3600``). */
        [[nodiscard]] auto parse_plain_seconds(std::string_view value)
            -> std::optional<std::chrono::seconds>
        {
            if (auto seconds = parse_fixed_uint(value))
            {
                return std::chrono::seconds{ static_cast<std::chrono::seconds::rep>(*seconds) };
            }
            return std::nullopt;
        }

        /** Parse a compact duration such as ``7d``, ``3d12h``, ``1w``, or ``30s``. */
        [[nodiscard]] auto parse_compact_duration_seconds(std::string_view value)
            -> std::optional<std::chrono::seconds>
        {
            auto remaining = value;
            std::uint64_t total = 0;
            bool has_component = false;

            while (!remaining.empty())
            {
                const auto amount = parse_uint_prefix(remaining);
                if (!amount)
                {
                    return std::nullopt;
                }
                remaining = util::lstrip_if(remaining, is_space);
                if (remaining.empty())
                {
                    return std::nullopt;
                }

                std::uint64_t multiplier = 0;
                switch (std::tolower(static_cast<unsigned char>(remaining.front())))
                {
                    case 'w':
                        multiplier = 604800;
                        break;
                    case 'd':
                        multiplier = 86400;
                        break;
                    case 'h':
                        multiplier = 3600;
                        break;
                    case 'm':
                        multiplier = 60;
                        break;
                    case 's':
                        multiplier = 1;
                        break;
                    default:
                        return std::nullopt;
                }
                remaining.remove_prefix(1);
                has_component = true;
                total += *amount * multiplier;
            }

            if (!has_component)
            {
                return std::nullopt;
            }
            return std::chrono::seconds{ static_cast<std::chrono::seconds::rep>(total) };
        }

        /**
         * Parse an ISO 8601 duration such as ``P7D`` or ``P1DT12H``.
         *
         * @throws std::invalid_argument when the value starts with ``P`` but has no components.
         */
        [[nodiscard]] auto parse_iso8601_duration_seconds(std::string_view value)
            -> std::optional<std::chrono::seconds>
        {
            if (value.empty() || std::tolower(static_cast<unsigned char>(value.front())) != 'p')
            {
                return std::nullopt;
            }

            auto remaining = value.substr(1);
            std::uint64_t total = 0;
            bool has_component = false;

            const auto consume_if_unit = [&](char unit, std::uint64_t multiplier) -> bool
            {
                std::size_t digits = 0;
                while (digits < remaining.size()
                       && std::isdigit(static_cast<unsigned char>(remaining[digits])))
                {
                    ++digits;
                }
                if (digits == 0)
                {
                    return true;
                }
                if (remaining.size() < digits + 1)
                {
                    return false;
                }
                if (std::tolower(static_cast<unsigned char>(remaining[digits]))
                    != std::tolower(static_cast<unsigned char>(unit)))
                {
                    return true;
                }

                const auto amount = parse_fixed_uint(remaining.substr(0, digits));
                if (!amount)
                {
                    return false;
                }
                remaining.remove_prefix(digits + 1);
                has_component = true;
                total += *amount * multiplier;
                return true;
            };

            if (!consume_if_unit('W', 604800) || !consume_if_unit('D', 86400))
            {
                return std::nullopt;
            }

            if (!remaining.empty()
                && std::tolower(static_cast<unsigned char>(remaining.front())) == 't')
            {
                remaining.remove_prefix(1);
                if (!consume_if_unit('H', 3600) || !consume_if_unit('M', 60)
                    || !consume_if_unit('S', 1))
                {
                    return std::nullopt;
                }
            }

            if (!remaining.empty())
            {
                return std::nullopt;
            }
            if (!has_component)
            {
                throw invalid_exclude_newer(value);
            }
            return std::chrono::seconds{ static_cast<std::chrono::seconds::rep>(total) };
        }

        /**
         * Parse a duration string in any supported format.
         *
         * Tries plain seconds, ISO 8601, then compact notation, in that order.
         */
        [[nodiscard]] auto parse_duration_seconds(std::string_view value)
            -> std::optional<std::chrono::seconds>
        {
            if (auto seconds = parse_plain_seconds(value))
            {
                return seconds;
            }
            if (auto seconds = parse_iso8601_duration_seconds(value))
            {
                return seconds;
            }
            return parse_compact_duration_seconds(value);
        }

        /**
         * Parse a date-only value (``YYYY-MM-DD``) to the start of the next UTC day.
         *
         * Matches conda's exclusive upper-bound semantics for date-only ``exclude_newer``.
         */
        [[nodiscard]] auto parse_date_only(std::string_view value) -> std::optional<CutoffInstant>
        {
            if (value.size() != 10)
            {
                return std::nullopt;
            }

            const auto day = parse_chrono<std::chrono::sys_days>(value, "%F");
            if (!day)
            {
                return std::nullopt;
            }

            return CutoffInstant{ *day + std::chrono::days{ 1 } };
        }

        /**
         * Parse a datetime value to an absolute UTC instant.
         *
         * Supports ``%FT%T%Ez``, ``%FT%TZ``, and naive ``%FT%T`` forms.
         */
        [[nodiscard]] auto parse_datetime(std::string_view value) -> std::optional<CutoffInstant>
        {
            if (value.size() < 19 || value[4] != '-' || value[7] != '-' || value[10] != 'T'
                || value[13] != ':' || value[16] != ':')
            {
                return std::nullopt;
            }

            if (auto instant = parse_chrono<SysTime>(value, "%FT%T%Ez"))
            {
                return CutoffInstant{ instant->time_since_epoch() };
            }
            if (auto instant = parse_chrono<SysTime>(value, "%FT%TZ"))
            {
                return CutoffInstant{ instant->time_since_epoch() };
            }
            if (auto instant = parse_chrono<SysTime>(value, "%FT%T"))
            {
                return CutoffInstant{ instant->time_since_epoch() };
            }
            return std::nullopt;
        }

        /** Compute ``now - duration``, clamping to epoch zero when the duration exceeds ``now``. */
        [[nodiscard]] auto duration_cutoff(std::chrono::seconds duration, CutoffInstant now)
            -> CutoffInstant
        {
            if (duration > now.time_since_epoch())
            {
                return CutoffInstant{};
            }
            return now - duration;
        }

    }  // namespace

    /** Resolve a global ``exclude_newer`` value; see ``resolve_exclude_newer_cutoff`` in the
     * header. */
    auto resolve_exclude_newer_cutoff(std::string_view value, std::uint64_t now_seconds)
        -> std::optional<std::uint64_t>
    {
        value = util::strip_if(value, is_space);
        if (value.empty())
        {
            return std::nullopt;
        }

        const auto now = CutoffInstant{ std::chrono::seconds{ now_seconds } };

        if (auto duration = parse_duration_seconds(value))
        {
            return to_unix_seconds(duration_cutoff(*duration, now));
        }

        if (auto instant = parse_date_only(value))
        {
            return to_unix_seconds(*instant);
        }

        if (auto instant = parse_datetime(value))
        {
            return to_unix_seconds(*instant);
        }

        throw invalid_exclude_newer(value);
    }

    /** Return the per-package or global cutoff for ``package_name``. */
    auto ExcludeNewerPolicy::cutoff_for(std::string_view package_name) const
        -> std::optional<std::uint64_t>
    {
        if (const auto it = per_package.find(std::string(package_name)); it != per_package.end())
        {
            return it->second;
        }
        return global;
    }

    /** Return whether ``pkg_timestamp`` exceeds the effective cutoff for ``package_name``. */
    auto ExcludeNewerPolicy::excludes(std::string_view package_name, std::uint64_t pkg_timestamp) const
        -> bool
    {
        if (const auto cutoff = cutoff_for(package_name))
        {
            return pkg_timestamp > *cutoff;
        }
        return false;
    }

    /** Resolve each entry in ``exclude_newer_package`` to a cutoff or exemption. */
    auto resolve_exclude_newer_package_cutoffs(
        const std::map<std::string, std::string>& exclude_newer_package,
        std::uint64_t now_seconds
    ) -> ExcludeNewerPackageCutoffs
    {
        auto out = ExcludeNewerPackageCutoffs{};
        for (const auto& [name, value] : exclude_newer_package)
        {
            const auto trimmed = util::strip_if(std::string_view{ value }, is_space);
            if (equals_ci(trimmed, "false"))
            {
                out.emplace(name, std::nullopt);
            }
            else
            {
                out.emplace(name, resolve_exclude_newer_cutoff(trimmed, now_seconds));
            }
        }
        return out;
    }

}  // namespace mamba
