// Copyright (c) 2026, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <cctype>
#include <charconv>
#include <ctime>
#include <optional>
#include <regex>
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

        [[nodiscard]] auto parse_iso8601_duration_seconds(std::string_view value)
            -> std::optional<std::uint64_t>
        {
            static const std::regex iso_duration_re(
                R"re(^P(?:(\d+)W)?(?:(\d+)D)?(?:T(?:(\d+)H)?(?:(\d+)M)?(?:(\d+)S)?)?$)re",
                std::regex::icase
            );
            std::cmatch match;
            if (!std::regex_match(value.begin(), value.end(), match, iso_duration_re))
            {
                return std::nullopt;
            }

            bool has_component = false;
            std::uint64_t total = 0;
            const auto add = [&](const std::csub_match& group, std::uint64_t unit)
            {
                if (!group.matched)
                {
                    return;
                }
                has_component = true;
                total += static_cast<std::uint64_t>(std::stoull(std::string(group))) * unit;
            };

            add(match[1], 604800);
            add(match[2], 86400);
            add(match[3], 3600);
            add(match[4], 60);
            add(match[5], 1);

            if (!has_component)
            {
                throw invalid_exclude_newer(value);
            }
            return total;
        }

        [[nodiscard]] auto parse_compact_duration_seconds(std::string_view value)
            -> std::optional<std::uint64_t>
        {
            static const std::regex compact_duration_re(R"re(^(\d+)\s*([wdhms])$)re", std::regex::icase);
            std::cmatch match;
            if (!std::regex_match(value.begin(), value.end(), match, compact_duration_re))
            {
                return std::nullopt;
            }

            const auto amount = static_cast<std::uint64_t>(std::stoull(std::string(match[1])));
            const auto unit = std::tolower(static_cast<unsigned char>(match[2].first[0]));
            switch (unit)
            {
                case 'w':
                    return amount * 604800;
                case 'd':
                    return amount * 86400;
                case 'h':
                    return amount * 3600;
                case 'm':
                    return amount * 60;
                case 's':
                    return amount;
                default:
                    return std::nullopt;
            }
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
            static const std::regex date_only_re(R"re(^(\d{4})-(\d{2})-(\d{2})$)re");
            std::cmatch match;
            if (!std::regex_match(value.begin(), value.end(), match, date_only_re))
            {
                return std::nullopt;
            }

            std::tm tm = {};
            tm.tm_year = std::stoi(std::string(match[1])) - 1900;
            tm.tm_mon = std::stoi(std::string(match[2])) - 1;
            tm.tm_mday = std::stoi(std::string(match[3])) + 1;
            tm.tm_hour = 0;
            tm.tm_min = 0;
            tm.tm_sec = 0;
            tm.tm_isdst = 0;
            return static_cast<std::uint64_t>(timegm_utc(tm));
        }

        [[nodiscard]] auto parse_datetime_timestamp(std::string_view value)
            -> std::optional<std::uint64_t>
        {
            static const std::regex datetime_re(
                R"re(^(\d{4})-(\d{2})-(\d{2})T(\d{2}):(\d{2}):(\d{2})(Z|([+-])(\d{2}):(\d{2}))?$)re"
            );
            std::cmatch match;
            if (!std::regex_match(value.begin(), value.end(), match, datetime_re))
            {
                return std::nullopt;
            }

            std::tm tm = {};
            tm.tm_year = std::stoi(std::string(match[1])) - 1900;
            tm.tm_mon = std::stoi(std::string(match[2])) - 1;
            tm.tm_mday = std::stoi(std::string(match[3]));
            tm.tm_hour = std::stoi(std::string(match[4]));
            tm.tm_min = std::stoi(std::string(match[5]));
            tm.tm_sec = std::stoi(std::string(match[6]));
            tm.tm_isdst = 0;

            auto timestamp = timegm_utc(tm);

            if (match[7].matched && match[7].str() != "Z")
            {
                const auto sign = match[8].str() == "+" ? 1 : -1;
                const auto offset_hours = std::stoi(std::string(match[9]));
                const auto offset_minutes = std::stoi(std::string(match[10]));
                const auto offset_seconds = sign * (offset_hours * 3600 + offset_minutes * 60);
                timestamp -= offset_seconds;
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

    auto ExcludeNewerCutoffPolicy::cutoff_for(std::string_view package_name) const
        -> std::optional<std::uint64_t>
    {
        if (const auto it = per_package.find(package_name); it != per_package.end())
        {
            return it->second;
        }
        return global;
    }

    auto
    ExcludeNewerCutoffPolicy::excludes(std::string_view package_name, std::uint64_t pkg_timestamp) const
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
