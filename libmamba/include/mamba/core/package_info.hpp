// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_PACKAGE_INFO
#define MAMBA_CORE_PACKAGE_INFO

#include <string>
#include <vector>

#include <nlohmann/json_fwd.hpp>

namespace mamba
{
    class PackageInfo
    {
    public:

        std::string noarch = {};
        std::size_t build_number = 0;
        /**
         * Could contain "conda-forge", "conda-forge/linux-64", or a url.
         *
         * @todo need to use a proper type for channels
         */
        std::string channel = {};
        std::string url = {};
        std::string subdir = {};
        std::string filename = {};
        std::string license = {};
        std::size_t size = 0;
        std::size_t timestamp = 0;
        std::string md5 = {};
        std::string sha256 = {};
        std::vector<std::string> track_features = {};
        std::vector<std::string> depends = {};
        std::vector<std::string> constrains = {};
        std::string signatures = {};
        std::vector<std::string> defaulted_keys = {};

        PackageInfo() = default;
        explicit PackageInfo(std::string name);
        PackageInfo(std::string name, std::string version, std::string build_string, std::size_t build_number);

        [[nodiscard]] auto name() const -> const std::string&;

        void set_name(std::string name);

        [[nodiscard]] auto version() const -> const std::string&;

        void set_version(std::string ver);

        [[nodiscard]] auto build_string() const -> const std::string&;

        void set_build_string(std::string bld);

        [[nodiscard]] auto json_signable() const -> nlohmann::json;
        [[nodiscard]] auto str() const -> std::string;
        [[nodiscard]] auto long_str() const -> std::string;

        /**
         * Dynamically get a field (e.g. name, version) as a string.
         */
        [[nodiscard]] auto field(std::string_view name) const -> std::string;

    private:

        std::string m_name = {};
        std::string m_version = {};
        std::string m_build_string = {};
    };

    auto operator==(const PackageInfo& lhs, const PackageInfo& rhs) -> bool;

    auto operator!=(const PackageInfo& lhs, const PackageInfo& rhs) -> bool;

    void to_json(nlohmann::json& j, const PackageInfo& pkg);

    void from_json(const nlohmann::json& j, PackageInfo& pkg);
}
#endif
