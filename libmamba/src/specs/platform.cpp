// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <string>

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include "mamba/specs/platform.hpp"
#include "mamba/util/string.hpp"

namespace mamba::specs
{
    auto platform_parse(std::string_view str) -> std::optional<KnownPlatform>
    {
        const std::string str_clean = util::to_lower(util::strip(str));
        for (const auto p : known_platforms())
        {
            if (str_clean == platform_name(p))
            {
                return { p };
            }
        }
        return {};
    }

    auto platform_is_linux(KnownPlatform plat) -> bool
    {
        return (plat == KnownPlatform::linux_32)          //
               || (plat == KnownPlatform::linux_64)       //
               || (plat == KnownPlatform::linux_armv6l)   //
               || (plat == KnownPlatform::linux_armv7l)   //
               || (plat == KnownPlatform::linux_aarch64)  //
               || (plat == KnownPlatform::linux_ppc64le)  //
               || (plat == KnownPlatform::linux_ppc64)    //
               || (plat == KnownPlatform::linux_s390x)    //
               || (plat == KnownPlatform::linux_riscv32)  //
               || (plat == KnownPlatform::linux_riscv64)  //
               || (plat == KnownPlatform::linux_loongarch64);
    }

    auto platform_is_linux(DynamicPlatform plat) -> bool
    {
        static constexpr auto repr = std::string_view("linux");
        return (plat.size() >= repr.size()) && util::starts_with(util::to_lower(plat), repr);
    }

    auto platform_is_osx(KnownPlatform plat) -> bool
    {
        return (plat == KnownPlatform::osx_64) || (plat == KnownPlatform::osx_arm64);
    }

    auto platform_is_osx(DynamicPlatform plat) -> bool
    {
        static constexpr auto repr = std::string_view("osx");
        return (plat.size() >= repr.size()) && util::starts_with(util::to_lower(plat), repr);
    }

    auto platform_is_win(KnownPlatform plat) -> bool
    {
        return (plat == KnownPlatform::win_32)     //
               || (plat == KnownPlatform::win_64)  //
               || (plat == KnownPlatform::win_arm64);
    }

    auto platform_is_win(DynamicPlatform plat) -> bool
    {
        static constexpr auto repr = std::string_view("win");
        return (plat.size() >= repr.size())
               && util::starts_with(util::to_lower(std::move(plat)), repr);
    }

    auto platform_is_noarch(KnownPlatform plat) -> bool
    {
        return plat == KnownPlatform::noarch;
    }

    auto platform_is_noarch(DynamicPlatform plat) -> bool
    {
        static constexpr auto repr = std::string_view("noarch");
        return util::starts_with(util::to_lower(std::move(plat)), repr);
    }

    /**
     * Detect the platform on which mamba was built.
     */
    auto build_platform() -> KnownPlatform
    {
#if defined(__linux__)
#if __x86_64__
        return KnownPlatform::linux_64;
#elif defined(i386)
        return Platform::linux_32;
#elif defined(__arm__) || defined(__thumb__)
#ifdef ___ARM_ARCH_6__
        return KnownPlatform::linux_armv6l;
#elif __ARM_ARCH_7__
        return KnownPlatform::linux_armv7l;
#else
#error "Unknown Linux arm platform"
#endif
#elif _M_ARM == 6
        return KnownPlatform::linux_armv6l;
#elif _M_ARM == 7
        return KnownPlatform::linux_armv7l;
#elif defined(__aarch64__)
        return KnownPlatform::linux_aarch64;
#elif defined(__ppc64__) || defined(__powerpc64__)
#if (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
        return KnownPlatform::linux_ppc64;
#else
        return KnownPlatform::linux_ppc64le;
#endif
#elif defined(__s390x__)
        return KnownPlatform::linux_s390x;
#elif defined(__riscv) && defined(__riscv_xlen) && (__riscv_xlen == 32)
        return KnownPlatform::linux_riscv32;
#elif defined(__riscv) && defined(__riscv_xlen) && (__riscv_xlen == 64)
        return KnownPlatform::linux_riscv64;
#elif defined(__loongarch64)
        return KnownPlatform::linux_loongarch64;
#else
#error "Unknown Linux platform"
#endif

#elif defined(__APPLE__) || defined(__MACH__)
#if __x86_64__
        return KnownPlatform::osx_64;
#elif __arm64__
        return KnownPlatform::osx_arm64;
#else
#error "Unknown OSX platform"
#endif

#elif defined(_WIN64)
#if defined(_M_AMD64)
        return KnownPlatform::win_64;
#elif defined(_M_ARM64)
        return KnownPlatform::win_arm64;
#else
#error "Unknown Windows platform"
#endif
#elif defined(_WIN32)
        return KnownPlatform::win_32;

#else
#error "Unknown platform"
#endif
    }

    auto build_platform_name() -> std::string_view
    {
        return platform_name(build_platform());
    }

    void to_json(nlohmann::json& j, const KnownPlatform& p)
    {
        j = platform_name(p);
    }

    void from_json(const nlohmann::json& j, KnownPlatform& p)
    {
        const auto j_str = j.get<std::string_view>();
        if (const auto maybe = platform_parse(j_str))
        {
            p = *maybe;
        }
        else
        {
            throw std::invalid_argument(fmt::format("Invalid platform: {}", j_str));
        }
    }

    auto noarch_parse(std::string_view str) -> std::optional<NoArchType>
    {
        const std::string str_clean = util::to_lower(util::strip(str));
        for (const auto p : known_noarch())
        {
            if (str_clean == noarch_name(p))
            {
                return { p };
            }
        }
        return {};
    }

    void to_json(nlohmann::json& j, const NoArchType& noarch)
    {
        if (noarch != NoArchType::No)
        {
            j = noarch_name(noarch);
        }
        else
        {
            j = nullptr;
        }
    }

    void from_json(const nlohmann::json& j, NoArchType& noarch)
    {
        // Legacy deserilization
        if (j.is_null())
        {
            noarch = NoArchType::No;
            return;
        }
        if (j.is_boolean())
        {
            noarch = j.get<bool>() ? NoArchType::Generic : NoArchType::No;
            return;
        }

        const auto str = j.get<std::string_view>();
        if (const auto maybe = noarch_parse(str))
        {
            noarch = *maybe;
        }
        else
        {
            throw std::invalid_argument(fmt::format("Invalid noarch: {}", str));
        }
    }
}
