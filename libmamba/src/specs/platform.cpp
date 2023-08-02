// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <algorithm>
#include <cassert>
#include <string>

#include "mamba/core/util_string.hpp"
#include "mamba/specs/platform.hpp"

namespace mamba::specs
{
    /**
     * Convert the enumeration to its conda string.
     */
    auto platform_name(Platform p) -> std::string_view
    {
        switch (p)
        {
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
            default:
                // All enum cases must be handled
                assert(false);
                return "";
        }
    }

    auto platform_parse(std::string_view str) -> std::optional<Platform>
    {
        std::string const str_clean = to_lower(strip(str));
        for (const auto p : {
                 Platform::linux_32,
                 Platform::linux_64,
                 Platform::linux_armv6l,
                 Platform::linux_armv7l,
                 Platform::linux_aarch64,
                 Platform::linux_ppc64,
                 Platform::linux_ppc64le,
                 Platform::linux_s390x,
                 Platform::linux_riscv32,
                 Platform::linux_riscv64,
                 Platform::osx_64,
                 Platform::osx_arm64,
                 Platform::win_32,
                 Platform::win_64,
             })
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
        return Platform::win_64;
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
}
