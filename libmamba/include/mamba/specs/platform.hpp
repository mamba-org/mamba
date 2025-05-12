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
    enum class KnownPlatform
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
        zos_z,

        // For reflexion purposes only
        count_,
    };

    using DynamicPlatform = std::string;

    [[nodiscard]] constexpr auto known_platforms_count() -> std::size_t
    {
        return static_cast<std::size_t>(KnownPlatform::count_);
    }

    [[nodiscard]] constexpr auto known_platforms()
        -> std::array<KnownPlatform, known_platforms_count()>;

    [[nodiscard]] constexpr auto known_platform_names()
        -> std::array<std::string_view, known_platforms_count()>;

    /**
     * Convert the enumeration to its conda string.
     */
    [[nodiscard]] constexpr auto platform_name(KnownPlatform p) -> std::string_view;

    /**
     * Return the enum matching the platform name.
     */
    [[nodiscard]] auto platform_parse(std::string_view str) -> std::optional<KnownPlatform>;

    [[nodiscard]] auto platform_is_linux(KnownPlatform plat) -> bool;
    [[nodiscard]] auto platform_is_linux(DynamicPlatform plat) -> bool;

    [[nodiscard]] auto platform_is_osx(KnownPlatform plat) -> bool;
    [[nodiscard]] auto platform_is_osx(DynamicPlatform plat) -> bool;

    [[nodiscard]] auto platform_is_win(KnownPlatform plat) -> bool;
    [[nodiscard]] auto platform_is_win(DynamicPlatform plat) -> bool;

    [[nodiscard]] auto platform_is_noarch(KnownPlatform plat) -> bool;
    [[nodiscard]] auto platform_is_noarch(DynamicPlatform plat) -> bool;

    /**
     * Detect the platform on which mamba was built.
     */
    [[nodiscard]] auto build_platform() -> KnownPlatform;
    [[nodiscard]] auto build_platform_name() -> std::string_view;

    /**
     * Serialize to JSON string.
     */
    void to_json(nlohmann::json& j, const KnownPlatform& p);

    /**
     * Deserialize from JSON string.
     */
    void from_json(const nlohmann::json& j, KnownPlatform& p);

    /**
     * Noarch packages are packages that are not architecture specific.
     *
     * Noarch packages only have to be built once.
     */
    enum struct NoArchType
    {
        /** Not a noarch type. */
        No,

        /** Noarch generic packages allow users to distribute docs, datasets, and source code. */
        Generic,

        /**
         * A noarch python package is a python package without any precompiled python files.
         *
         * Normally, precompiled files (`.pyc` or `__pycache__`) are bundled with the package.
         * However, these files are tied to a specific version of Python and must therefore be
         * generated for every target platform and architecture.
         * This complicates the build process.
         * For noarch Python packages these files are generated when installing the package by
         * invoking the compilation process through the python binary that is installed in the
         * same environment.
         *
         * @see https://www.anaconda.com/blog/condas-new-noarch-packages
         * @see
         * https://docs.conda.io/projects/conda/en/latest/user-guide/concepts/packages.html#noarch-python
         */
        Python,

        // For reflexion purposes only
        count_,
    };

    constexpr auto known_noarch_count() -> std::size_t
    {
        return static_cast<std::size_t>(NoArchType::count_);
    }

    constexpr auto known_noarch() -> std::array<NoArchType, known_noarch_count()>;

    constexpr auto known_noarch_names() -> std::array<std::string_view, known_noarch_count()>;

    /**
     * Convert the enumeration to its conda string.
     */
    constexpr auto noarch_name(NoArchType noarch) -> std::string_view;

    /**
     * Return the enum matching the noarch name.
     */
    auto noarch_parse(std::string_view str) -> std::optional<NoArchType>;

    /**
     * Serialize to JSON string.
     */
    void to_json(nlohmann::json& j, const NoArchType& noarch);

    /**
     * Deserialize from JSON string.
     */
    void from_json(const nlohmann::json& j, NoArchType& noarch);

    /********************
     *  Implementation  *
     ********************/

    constexpr auto platform_name(KnownPlatform p) -> std::string_view
    {
        switch (p)
        {
            case KnownPlatform::noarch:
                return "noarch";
            case KnownPlatform::linux_32:
                return "linux-32";
            case KnownPlatform::linux_64:
                return "linux-64";
            case KnownPlatform::linux_armv6l:
                return "linux-armv6l";
            case KnownPlatform::linux_armv7l:
                return "linux-armv7l";
            case KnownPlatform::linux_aarch64:
                return "linux-aarch64";
            case KnownPlatform::linux_ppc64:
                return "linux-ppc64";
            case KnownPlatform::linux_ppc64le:
                return "linux-ppc64le";
            case KnownPlatform::linux_s390x:
                return "linux-s390x";
            case KnownPlatform::linux_riscv32:
                return "linux-riscv32";
            case KnownPlatform::linux_riscv64:
                return "linux-riscv64";
            case KnownPlatform::osx_64:
                return "osx-64";
            case KnownPlatform::osx_arm64:
                return "osx-arm64";
            case KnownPlatform::win_32:
                return "win-32";
            case KnownPlatform::win_64:
                return "win-64";
            case KnownPlatform::win_arm64:
                return "win-arm64";
            case KnownPlatform::zos_z:
                return "zos-z";
            default:
                return "";
        }
    }

    constexpr auto known_platforms() -> std::array<KnownPlatform, known_platforms_count()>
    {
        auto out = std::array<KnownPlatform, known_platforms_count()>{};
        for (std::size_t idx = 0; idx < out.size(); ++idx)
        {
            out[idx] = static_cast<KnownPlatform>(idx);
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

    constexpr auto noarch_name(NoArchType noarch) -> std::string_view
    {
        switch (noarch)
        {
            case NoArchType::No:
                return "no";
            case NoArchType::Generic:
                return "generic";
            case NoArchType::Python:
                return "python";
            default:
                return "";
        }
    }

    constexpr auto known_noarch() -> std::array<NoArchType, known_noarch_count()>
    {
        auto out = std::array<NoArchType, known_noarch_count()>{};
        for (std::size_t idx = 0; idx < out.size(); ++idx)
        {
            out[idx] = static_cast<NoArchType>(idx);
        }
        return out;
    }

    constexpr auto known_noarch_names() -> std::array<std::string_view, known_noarch_count()>
    {
        auto out = std::array<std::string_view, known_noarch_count()>{};
        auto iter = out.begin();
        for (auto p : known_noarch())
        {
            *(iter++) = noarch_name(p);
        }
        return out;
    }
}
#endif
