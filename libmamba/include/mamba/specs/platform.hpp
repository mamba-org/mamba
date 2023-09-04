// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.


#ifndef MAMBA_SPECS_PLATFORM_HPP
#define MAMBA_SPECS_PLATFORM_HPP

#include <array>
#include <optional>
#include <string_view>

#include <nlohmann/json_fwd.hpp>

namespace mamba::specs
{
    /**
     * All platforms known to Conda.
     *
     * When one platform name is the substring of another, the longest appears first so that
     * it makes it easier to use in a parser.
     */
    enum class Platform
    {
        noarch = 0,
        linux_32,
        linux_64,
        linux_armv6l,
        linux_armv7l,
        linux_aarch64,
        linux_ppc64le,
        linux_ppc64,
        linux_s390x,
        linux_riscv32,
        linux_riscv64,
        osx_64,
        osx_arm64,
        win_32,
        win_64,
        win_arm64,

        // For reflexion purposes only
        count_,
    };

    constexpr auto known_platforms_count() -> std::size_t
    {
        return static_cast<std::size_t>(Platform::count_);
    }

    constexpr auto known_platforms() -> std::array<Platform, known_platforms_count()>;

    constexpr auto known_platform_names() -> std::array<std::string_view, known_platforms_count()>;

    /**
     * Convert the enumeration to its conda string.
     */
    constexpr auto platform_name(Platform p) -> std::string_view;

    /**
     * Return the enum matching the platform name.
     */
    auto platform_parse(std::string_view str) -> std::optional<Platform>;

    /**
     * Detect the platform on which mamba was built.
     */
    auto build_platform() -> Platform;
    auto build_platform_name() -> std::string_view;

    /**
     * Serialize to JSON string.
     */
    void to_json(nlohmann::json& j, const Platform& p);

    /**
     * Deserialize from JSON string.
     */
    void from_json(const nlohmann::json& j, Platform& p);

    /********************
     *  Implementation  *
     ********************/

    constexpr auto platform_name(Platform p) -> std::string_view
    {
        switch (p)
        {
            case Platform::noarch:
                return "noarch";
            case Platform::linux_32:
                return "linux-32";
            case Platform::linux_64:
                return "linux-64";
            case Platform::linux_armv6l:
                return "linux-armv6l";
            case Platform::linux_armv7l:
                return "linux-armv7l";
            case Platform::linux_aarch64:
                return "linux-aarch64";
            case Platform::linux_ppc64:
                return "linux-ppc64";
            case Platform::linux_ppc64le:
                return "linux-ppc64le";
            case Platform::linux_s390x:
                return "linux-s390x";
            case Platform::linux_riscv32:
                return "linux-riscv32";
            case Platform::linux_riscv64:
                return "linux-riscv64";
            case Platform::osx_64:
                return "osx-64";
            case Platform::osx_arm64:
                return "osx-arm64";
            case Platform::win_32:
                return "win-32";
            case Platform::win_64:
                return "win-64";
            case Platform::win_arm64:
                return "win-arm64";
            default:
                return "";
        }
    }

    constexpr auto known_platforms() -> std::array<Platform, known_platforms_count()>
    {
        auto out = std::array<Platform, known_platforms_count()>{};
        for (std::size_t idx = 0; idx < out.size(); ++idx)
        {
            out[idx] = static_cast<Platform>(idx);
        }
        return out;
    }

    constexpr auto known_platform_names() -> std::array<std::string_view, known_platforms_count()>
    {
        auto out = std::array<std::string_view, known_platforms_count()>{};
        auto iter = out.begin();
        for (auto p : known_platforms())
        {
            *(iter++) = platform_name(p);
        }
        return out;
    }

}
#endif
