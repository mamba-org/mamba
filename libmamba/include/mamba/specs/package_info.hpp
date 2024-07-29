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

namespace mamba::specs
{
    enum class PackageType
    {
        Unknown,
        Conda,
        Wheel,
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
        std::string signatures = {};
        std::vector<std::string> track_features = {};
        std::vector<std::string> dependencies = {};
        std::vector<std::string> constrains = {};
        std::vector<std::string> defaulted_keys = {};
        NoArchType noarch = NoArchType::No;
        std::size_t size = 0;
        std::size_t timestamp = 0;
        // FIXME this is a temporary hack to accommodate Python wheels but wheels and conda
        // PackageInfo, should really be split in different types.
        PackageType package_type = PackageType::Unknown;

        [[nodiscard]] static auto from_url(std::string_view url) -> expected_parse_t<PackageInfo>;

        PackageInfo() = default;
        explicit PackageInfo(std::string name);
        PackageInfo(std::string name, std::string version, std::string build_string, std::size_t build_number);

        [[nodiscard]] auto json_signable() const -> nlohmann::json;
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
}
#endif
