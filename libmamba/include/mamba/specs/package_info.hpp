// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_PACKAGE_INFO
#define MAMBA_CORE_PACKAGE_INFO

#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json_fwd.hpp>

#include "mamba/specs/error.hpp"
#include "mamba/specs/platform.hpp"
#include "mamba/util/tuple_hash.hpp"

namespace mamba::specs
{
    namespace defaulted_key
    {
        inline constexpr const char* initialized = "_initialized";
        inline constexpr const char* name = "name";
        inline constexpr const char* version = "version";
        inline constexpr const char* build = "build";
        inline constexpr const char* build_string = "build_string";
        inline constexpr const char* build_number = "build_number";
        inline constexpr const char* channel = "channel";
        inline constexpr const char* subdir = "subdir";
        inline constexpr const char* fn = "fn";
        inline constexpr const char* license = "license";
        inline constexpr const char* timestamp = "timestamp";
        inline constexpr const char* track_features = "track_features";
        inline constexpr const char* depends = "depends";
        inline constexpr const char* constrains = "constrains";
        inline constexpr const char* md5 = "md5";
        inline constexpr const char* sha256 = "sha256";
        inline constexpr const char* size = "size";
        inline constexpr const char* url = "url";
    }

    enum class PackageType
    {
        Unknown,
        Conda,
        Wheel,
        TarGz,
    };

    class PackageInfo
    {
    public:

        std::string name = {};
        std::string version = {};
        std::string build_string = {};
        std::size_t build_number = 0;
        /**
         * Could contain "conda-forge", "conda-forge/linux-64", or a url.
         *
         * @todo need to use a proper type for channels
         */
        std::string channel = {};
        std::string package_url = {};
        DynamicPlatform platform = {};
        std::string filename = {};
        std::string license = {};
        std::string md5 = {};
        std::string sha256 = {};
        std::string python_site_packages_path = {};
        std::string signatures = {};
        std::vector<std::string> track_features = {};
        std::vector<std::string> dependencies = {};
        std::vector<std::string> constrains = {};
        // Tracks which fields have stub/default values that should be replaced by index.json.
        // Set by from_url() with field names, and by make_package_info() with {"_initialized"}.
        // The "_initialized" sentinel indicates the PackageInfo was properly constructed.
        // See GitHub issue #4095.
        std::vector<std::string> defaulted_keys = {};
        NoArchType noarch = NoArchType::No;
        std::size_t size = 0;
        std::size_t timestamp = 0;
        // FIXME this is a temporary hack to accommodate Python wheels but wheels and conda
        // PackageInfo, should really be split in different types.
        // WARNING Be aware that `package_type` value,
        // if set later to anything other than default (PackageType::Unknown),
        // will not be passed when going through `make_package_info` from libsolv
        PackageType package_type = PackageType::Unknown;

        [[nodiscard]] static auto from_url(std::string_view url) -> expected_parse_t<PackageInfo>;
        [[nodiscard]] auto url_for_channel(std::string_view channel_mirror_url) const -> std::string;
        [[nodiscard]] auto
        url_for_channel_platform(std::string_view channel_mirror_platform_url) const -> std::string;

        PackageInfo() = default;
        explicit PackageInfo(std::string name);
        PackageInfo(std::string name, std::string version, std::string build_string, std::size_t build_number);
        PackageInfo(std::string name, std::string version, std::string build_string, std::string channel);

        [[nodiscard]] auto json_signable() const -> nlohmann::json;
        // TODO: rename to_string, following C++ conventions
        [[nodiscard]] auto str() const -> std::string;
        [[nodiscard]] auto long_str() const -> std::string;

        /**
         * Dynamically get a field (e.g. name, version) as a string.
         */
        [[nodiscard]] auto field(std::string_view name) const -> std::string;
    };

    auto operator==(const PackageInfo& lhs, const PackageInfo& rhs) -> bool;

    auto operator!=(const PackageInfo& lhs, const PackageInfo& rhs) -> bool;

    void to_json(nlohmann::json& j, const PackageInfo& pkg);

    void from_json(const nlohmann::json& j, PackageInfo& pkg);

    /**
     * Sort packages by version (descending), then by build number (descending).
     *
     * This function precomputes parsed versions once per package to avoid repeatedly
     * calling `Version::parse` in sort comparator hot paths.
     */
    void sort_packages_by_version_and_build_desc(std::vector<PackageInfo>& packages);
}

template <>
struct std::hash<mamba::specs::PackageInfo>
{
    auto operator()(const mamba::specs::PackageInfo& pkg) const -> std::size_t
    {
        auto seed = std::size_t(0);
        seed = mamba::util::hash_vals(
            seed,
            pkg.name,
            pkg.version,
            pkg.build_string,
            pkg.build_number,
            pkg.channel,
            pkg.package_url,
            pkg.platform,
            pkg.filename,
            pkg.license,
            pkg.md5,
            pkg.sha256,
            pkg.signatures
        );
        seed = mamba::util::hash_combine_val_range(
            seed,
            pkg.track_features.begin(),
            pkg.track_features.end()
        );
        seed = mamba::util::hash_combine_val_range(
            seed,
            pkg.dependencies.begin(),
            pkg.dependencies.end()
        );
        seed = mamba::util::hash_combine_val_range(seed, pkg.constrains.begin(), pkg.constrains.end());
        seed = mamba::util::hash_combine_val_range(
            seed,
            pkg.defaulted_keys.begin(),
            pkg.defaulted_keys.end()
        );
        seed = mamba::util::hash_vals(seed, pkg.noarch, pkg.size, pkg.timestamp, pkg.package_type);
        return seed;
    }
};

#endif
