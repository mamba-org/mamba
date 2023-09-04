// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <algorithm>
#include <cassert>
#include <string>

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include "mamba/specs/platform.hpp"
#include "mamba/util/string.hpp"

namespace mamba::specs
{
    auto platform_parse(std::string_view str) -> std::optional<Platform>
    {
        std::string const str_clean = util::to_lower(util::strip(str));
        for (const auto p : known_platforms())
        {
            if (str_clean == platform_name(p))
            {
                return { p };
            }
        }
        return {};
    }

    /**
     * Detect the platform on which mamba was built.
     */
    auto build_platform() -> Platform
    {
#if defined(__linux__)
#if __x86_64__
        return Platform::linux_64;
#elif defined(i386)
        return Platform::linux_32;
#elif defined(__arm__) || defined(__thumb__)
#ifdef ___ARM_ARCH_6__
        return Platform::linux_armv6l;
#elif __ARM_ARCH_7__
        return Platform::linux_armv7l;
#else
#error "Unknown Linux arm platform"
#endif
#elif _M_ARM == 6
        return Platform::linux_armv6l;
#elif _M_ARM == 7
        return Platform::linux_armv7l;
#elif defined(__aarch64__)
        return Platform::linux_aarch64;
#elif defined(__ppc64__) || defined(__powerpc64__)
#if (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
        return Platform::linux_ppc64;
#else
        return Platform::linux_ppc64le;
#endif
#elif defined(__s390x__)
        return Platform::linux_s390x;
#elif defined(__riscv) && defined(__riscv_xlen) && (__riscv_xlen == 32)
        return Platform::linux_riscv32;
#elif defined(__riscv) && defined(__riscv_xlen) && (__riscv_xlen == 64)
        return Platform::linux_riscv64;
#else
#error "Unknown Linux platform"
#endif

#elif defined(__APPLE__) || defined(__MACH__)
#if __x86_64__
        return Platform::osx_64;
#elif __arm64__
        return Platform::osx_arm64;
#else
#error "Unknown OSX platform"
#endif

#elif defined(_WIN64)
#if defined(_M_AMD64)
        return Platform::win_64;
#elif defined(_M_ARM64)
        return Platform::win_arm64;
#else
#error "Unknown Windows platform"
#endif
#elif defined(_WIN32)
        return Platform::win_32;

#else
#error "Unknown platform"
#endif
    }

    auto build_platform_name() -> std::string_view
    {
        return platform_name(build_platform());
    }

    void to_json(nlohmann::json& j, const Platform& p)
    {
        j = platform_name(p);
    }

    void from_json(const nlohmann::json& j, Platform& p)
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
}
