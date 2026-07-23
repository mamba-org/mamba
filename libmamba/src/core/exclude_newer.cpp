// Copyright (c) 2026, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <cctype>
#include <charconv>
#include <chrono>
#include <optional>
#include <regex>
#include <stdexcept>
#include <string>

#include "mamba/core/detail/chrono_parse.hpp"
#include "mamba/core/error_handling.hpp"
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

        /** Throw when an ``exclude_newer`` value could not be parsed. */
        [[noreturn]] void
        throw_invalid_exclude_newer(std::string_view value, std::string_view package_name = {})
        {
            constexpr auto duration_hint = "expected a compact duration (e.g. 7d, 3d12h, 1w, 1y, 6M), an ISO 8601 duration "
                                           "(e.g. P7D, PT24H, P1DT12H), a plain integer in seconds (e.g. 3600), a date "
                                           "(e.g. 2026-04-01), or a datetime (e.g. 2026-04-01T12:00:00Z)";

            std::string message;
            if (package_name.empty())
            {
                message = "Could not parse exclude_newer value '" + std::string(value) + "'; "
                          + duration_hint;
            }
            else
            {
                message = "Could not parse exclude_newer_package value for package '"
                          + std::string(package_name) + "' ('" + std::string(value) + "'); "
                          + duration_hint + ", or false";
            }
            throw mamba_error(std::move(message), mamba_error_code::incorrect_usage);
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
            if (auto seconds = detail::parse_iso8601_duration_seconds(value))
            {
                return seconds;
            }
            return detail::parse_compact_duration_seconds(value);
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

            const auto day = detail::parse_chrono<std::chrono::sys_days>(value, "%F");
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

            if (auto instant = detail::parse_chrono<SysTime>(value, "%FT%T%Ez"))
            {
                return CutoffInstant{ instant->time_since_epoch() };
            }
            if (auto instant = detail::parse_chrono<SysTime>(value, "%FT%TZ"))
            {
                return CutoffInstant{ instant->time_since_epoch() };
            }
            if (auto instant = detail::parse_chrono<SysTime>(value, "%FT%T"))
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

    namespace detail
    {
        auto parse_iso8601_duration_seconds(std::string_view value)
            -> std::optional<std::chrono::seconds>
        {
            // For a definition of the ISO 8601 duration format, see:
            // https://docs.digi.com/resources/documentation/digidocs/90001488-13/reference/r_iso_8601_duration_format.htm
            // but mind the missing "(n)W" segment for weeks! Weeks are supported here folloiwing:
            //
            //     P(n)Y(n)M(n)W(n)DT(n)H(n)M(n)S
            //
            static const std::regex iso8601_duration{
                R"(^P(?:(\d+)Y)?(?:(\d+)M)?(?:(\d+)W)?(?:(\d+)D)?(?:T(?:(\d+)H)?(?:(\d+)M)?(?:(\d+)S)?)?$)",
                std::regex_constants::icase,
            };

            constexpr std::uint64_t seconds_per_minute = 60;
            constexpr std::uint64_t seconds_per_hour = 3600;
            constexpr std::uint64_t seconds_per_day = 86400;
            constexpr std::uint64_t seconds_per_week = 604800;
            constexpr std::uint64_t seconds_per_month = 30 * seconds_per_day;
            constexpr std::uint64_t seconds_per_year = 365 * seconds_per_day;

            if (value.empty())
            {
                return std::nullopt;
            }

            std::match_results<std::string_view::const_iterator> match;
            if (!std::regex_match(value.begin(), value.end(), match, iso8601_duration))
            {
                return std::nullopt;
            }

            std::uint64_t total = 0;
            bool has_component = false;

            const auto accumulate_component = [&](std::size_t group, std::uint64_t multiplier) -> bool
            {
                if (!match[group].matched)
                {
                    return true;
                }
                const auto amount = parse_fixed_uint(match[group].str());
                if (!amount)
                {
                    return false;
                }
                total += *amount * multiplier;
                has_component = true;
                return true;
            };

            if (!accumulate_component(1, seconds_per_year)
                || !accumulate_component(2, seconds_per_month)
                || !accumulate_component(3, seconds_per_week)
                || !accumulate_component(4, seconds_per_day)
                || !accumulate_component(5, seconds_per_hour)
                || !accumulate_component(6, seconds_per_minute) || !accumulate_component(7, 1))
            {
                return std::nullopt;
            }
            if (!has_component)
            {
                throw_invalid_exclude_newer(value);
            }
            return std::chrono::seconds{ static_cast<std::chrono::seconds::rep>(total) };
        }

        auto parse_compact_duration_seconds(std::string_view value)
            -> std::optional<std::chrono::seconds>
        {
            // (n)y(n)M(n)w(n)d(n)h(n)m(n)s — lowercase units except M for months.
            static const std::regex compact_duration{ R"(^(\d+[yMwdhms])+$)" };
            static const std::regex compact_segment{ R"((\d+)([yMwdhms]))" };

            constexpr std::uint64_t seconds_per_minute = 60;
            constexpr std::uint64_t seconds_per_hour = 3600;
            constexpr std::uint64_t seconds_per_day = 86400;
            constexpr std::uint64_t seconds_per_week = 604800;
            constexpr std::uint64_t seconds_per_month = 30 * seconds_per_day;
            constexpr std::uint64_t seconds_per_year = 365 * seconds_per_day;

            if (value.empty() || !std::regex_match(value.begin(), value.end(), compact_duration))
            {
                return std::nullopt;
            }

            std::uint64_t total = 0;
            const std::string input(value);
            const std::sregex_iterator end;
            for (std::sregex_iterator it(input.begin(), input.end(), compact_segment); it != end; ++it)
            {
                const auto amount = parse_fixed_uint((*it)[1].str());
                if (!amount)
                {
                    return std::nullopt;
                }

                std::uint64_t multiplier = 0;
                switch ((*it)[2].str().front())
                {
                    case 'y':
                        multiplier = seconds_per_year;
                        break;
                    case 'M':
                        multiplier = seconds_per_month;
                        break;
                    case 'w':
                        multiplier = seconds_per_week;
                        break;
                    case 'd':
                        multiplier = seconds_per_day;
                        break;
                    case 'h':
                        multiplier = seconds_per_hour;
                        break;
                    case 'm':
                        multiplier = seconds_per_minute;
                        break;
                    case 's':
                        multiplier = 1;
                        break;
                    default:
                        return std::nullopt;
                }
                total += *amount * multiplier;
            }

            return std::chrono::seconds{ static_cast<std::chrono::seconds::rep>(total) };
        }
    }  // namespace detail

    /** Resolve a global ``exclude_newer`` value; see ``resolve_exclude_newer_cutoff`` in the
     * header. */
    auto resolve_exclude_newer_cutoff(
        std::string_view value,
        std::uint64_t now_seconds,
        std::string_view package_name
    ) -> std::optional<std::uint64_t>
    {
        const auto raw = value;
        value = util::strip_if(value, is_space);
        if (value.empty())
        {
            if (!raw.empty())
            {
                throw_invalid_exclude_newer(raw, package_name);
            }
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

        throw_invalid_exclude_newer(value, package_name);
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
        const std::vector<std::pair<std::string, std::string>>& exclude_newer_package,
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
                out.emplace(name, resolve_exclude_newer_cutoff(trimmed, now_seconds, name));
            }
        }
        return out;
    }

    auto resolve_exclude_newer_policy(
        std::string_view exclude_newer,
        const std::vector<std::pair<std::string, std::string>>& exclude_newer_package,
        std::uint64_t now_seconds
    ) -> ExcludeNewerPolicy
    {
        return {
            /* .global= */ exclude_newer.empty()
                ? std::nullopt
                : resolve_exclude_newer_cutoff(exclude_newer, now_seconds),
            /* .per_package= */ resolve_exclude_newer_package_cutoffs(exclude_newer_package, now_seconds),
        };
    }

}  // namespace mamba
