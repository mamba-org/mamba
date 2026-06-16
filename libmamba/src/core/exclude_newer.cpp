// Copyright (c) 2026, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <cctype>
#include <charconv>
#include <ctime>
#include <optional>
#include <stdexcept>
#include <string>

#include "mamba/core/exclude_newer.hpp"

namespace mamba
{
    namespace
    {
        [[nodiscard]] auto trim(std::string_view value) -> std::string_view
        {
            while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())))
            {
                value.remove_prefix(1);
            }
            while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
            {
                value.remove_suffix(1);
            }
            return value;
        }

        [[nodiscard]] auto invalid_exclude_newer(std::string_view value) -> std::invalid_argument
        {
            return std::invalid_argument(
                "Invalid exclude_newer value '" + std::string(value)
                + "'; use e.g. 7d, P7D, 2026-04-01, or 2026-04-01T12:00:00Z"
            );
        }

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

        [[nodiscard]] auto parse_fixed_uint(std::string_view value) -> std::optional<int>
        {
            int number = 0;
            const auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), number);
            if (ec != std::errc() || ptr != value.data() + value.size())
            {
                return std::nullopt;
            }
            return number;
        }

        [[nodiscard]] auto parse_plain_seconds(std::string_view value) -> std::optional<std::uint64_t>
        {
            std::uint64_t seconds = 0;
            const auto* begin = value.data();
            const auto* end = begin + value.size();
            const auto [ptr, ec] = std::from_chars(begin, end, seconds);
            if (ec != std::errc() || ptr != end)
            {
                return std::nullopt;
            }
            return seconds;
        }

        [[nodiscard]] auto parse_compact_duration_seconds(std::string_view value)
            -> std::optional<std::uint64_t>
        {
            auto remaining = value;
            const auto amount = parse_uint_prefix(remaining);
            if (!amount)
            {
                return std::nullopt;
            }
            while (!remaining.empty() && std::isspace(static_cast<unsigned char>(remaining.front())))
            {
                remaining.remove_prefix(1);
            }
            if (remaining.size() != 1)
            {
                return std::nullopt;
            }
            switch (std::tolower(static_cast<unsigned char>(remaining.front())))
            {
                case 'w':
                    return *amount * 604800;
                case 'd':
                    return *amount * 86400;
                case 'h':
                    return *amount * 3600;
                case 'm':
                    return *amount * 60;
                case 's':
                    return *amount;
                default:
                    return std::nullopt;
            }
        }

        [[nodiscard]] auto parse_iso8601_duration_seconds(std::string_view value)
            -> std::optional<std::uint64_t>
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
                total += static_cast<std::uint64_t>(*amount) * multiplier;
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
            return total;
        }

        [[nodiscard]] auto parse_duration_seconds(std::string_view value)
            -> std::optional<std::uint64_t>
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

        [[nodiscard]] auto timegm_utc(std::tm tm) -> std::time_t
        {
#if defined(_WIN32)
            return _mkgmtime(&tm);
#else
            return timegm(&tm);
#endif
        }

        [[nodiscard]] auto parse_date_only_next_utc_day(std::string_view value)
            -> std::optional<std::uint64_t>
        {
            if (value.size() != 10 || value[4] != '-' || value[7] != '-')
            {
                return std::nullopt;
            }

            const auto year = parse_fixed_uint(value.substr(0, 4));
            const auto month = parse_fixed_uint(value.substr(5, 2));
            const auto day = parse_fixed_uint(value.substr(8, 2));
            if (!year || !month || !day)
            {
                return std::nullopt;
            }

            std::tm tm = {};
            tm.tm_year = *year - 1900;
            tm.tm_mon = *month - 1;
            tm.tm_mday = *day + 1;
            tm.tm_hour = 0;
            tm.tm_min = 0;
            tm.tm_sec = 0;
            tm.tm_isdst = 0;
            return static_cast<std::uint64_t>(timegm_utc(tm));
        }

        [[nodiscard]] auto parse_datetime_timestamp(std::string_view value)
            -> std::optional<std::uint64_t>
        {
            if (value.size() < 19 || value[4] != '-' || value[7] != '-' || value[10] != 'T'
                || value[13] != ':' || value[16] != ':')
            {
                return std::nullopt;
            }

            const auto year = parse_fixed_uint(value.substr(0, 4));
            const auto month = parse_fixed_uint(value.substr(5, 2));
            const auto day = parse_fixed_uint(value.substr(8, 2));
            const auto hour = parse_fixed_uint(value.substr(11, 2));
            const auto minute = parse_fixed_uint(value.substr(14, 2));
            const auto second = parse_fixed_uint(value.substr(17, 2));
            if (!year || !month || !day || !hour || !minute || !second)
            {
                return std::nullopt;
            }

            std::tm tm = {};
            tm.tm_year = *year - 1900;
            tm.tm_mon = *month - 1;
            tm.tm_mday = *day;
            tm.tm_hour = *hour;
            tm.tm_min = *minute;
            tm.tm_sec = *second;
            tm.tm_isdst = 0;

            auto timestamp = timegm_utc(tm);
            auto suffix = value.substr(19);

            if (suffix.empty())
            {
                // Naive datetimes are interpreted as UTC.
            }
            else if (suffix.size() == 1
                     && std::tolower(static_cast<unsigned char>(suffix.front())) == 'z')
            {
                // Explicit UTC.
            }
            else if (suffix.size() == 6 && (suffix.front() == '+' || suffix.front() == '-')
                     && suffix[3] == ':')
            {
                const auto offset_hours = parse_fixed_uint(suffix.substr(1, 2));
                const auto offset_minutes = parse_fixed_uint(suffix.substr(4, 2));
                if (!offset_hours || !offset_minutes)
                {
                    return std::nullopt;
                }
                const auto sign = suffix.front() == '+' ? 1 : -1;
                const auto offset_seconds = sign * (*offset_hours * 3600 + *offset_minutes * 60);
                timestamp -= offset_seconds;
            }
            else
            {
                return std::nullopt;
            }

            if (timestamp < 0)
            {
                return std::nullopt;
            }
            return static_cast<std::uint64_t>(timestamp);
        }

        [[nodiscard]] auto
        duration_cutoff(std::uint64_t duration_seconds, std::uint64_t now_seconds) -> std::uint64_t
        {
            if (duration_seconds > now_seconds)
            {
                return 0;
            }
            return now_seconds - duration_seconds;
        }

        [[nodiscard]] auto
        resolve_exclude_newer_cutoff_impl(std::string_view value, std::uint64_t now_seconds)
            -> std::optional<std::uint64_t>
        {
            value = trim(value);
            if (value.empty())
            {
                return std::nullopt;
            }

            if (auto duration_seconds = parse_duration_seconds(value))
            {
                return duration_cutoff(*duration_seconds, now_seconds);
            }

            if (auto timestamp = parse_date_only_next_utc_day(value))
            {
                return *timestamp;
            }

            if (auto timestamp = parse_datetime_timestamp(value))
            {
                return *timestamp;
            }

            throw invalid_exclude_newer(value);
        }

    }  // namespace

    auto resolve_exclude_newer_cutoff(std::string_view value, std::uint64_t now_seconds)
        -> std::optional<std::uint64_t>
    {
        return resolve_exclude_newer_cutoff_impl(value, now_seconds);
    }

    auto ExcludeNewerPolicy::cutoff_for(std::string_view package_name) const
        -> std::optional<std::uint64_t>
    {
        if (const auto it = per_package.find(package_name); it != per_package.end())
        {
            return it->second;
        }
        return global;
    }

    auto ExcludeNewerPolicy::excludes(std::string_view package_name, std::uint64_t pkg_timestamp) const
        -> bool
    {
        if (const auto cutoff = cutoff_for(package_name))
        {
            return pkg_timestamp > *cutoff;
        }
        return false;
    }

    auto resolve_exclude_newer_package_cutoffs(
        const std::map<std::string, std::string>& exclude_newer_package,
        std::uint64_t now_seconds
    ) -> ExcludeNewerPackageCutoffs
    {
        auto out = ExcludeNewerPackageCutoffs{};
        for (const auto& [name, value] : exclude_newer_package)
        {
            const auto trimmed = trim(value);
            if (trimmed.size() == 5 && (trimmed[0] == 'f' || trimmed[0] == 'F')
                && (trimmed[1] == 'a' || trimmed[1] == 'A')
                && (trimmed[2] == 'l' || trimmed[2] == 'L')
                && (trimmed[3] == 's' || trimmed[3] == 'S')
                && (trimmed[4] == 'e' || trimmed[4] == 'E'))
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
