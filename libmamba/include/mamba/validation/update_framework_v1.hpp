// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_VALIDATION_UPDATE_FRAMEWORK_V1_HPP
#define MAMBA_VALIDATION_UPDATE_FRAMEWORK_V1_HPP

#include <set>
#include <string>

#include <nlohmann/json_fwd.hpp>

#include "mamba/validation/keys.hpp"
#include "mamba/validation/update_framework.hpp"

namespace mamba::fs
{
    class u8path;
}

namespace mamba::validation::v1
{
    /**
     * TUF v1 specific implementation.
     */
    class SpecImpl final : public SpecBase
    {
    public:

        SpecImpl(std::string sv = "1.0.17");

        [[nodiscard]] auto json_key() const -> std::string override;
        [[nodiscard]] auto expiration_json_key() const -> std::string override;

        [[nodiscard]] auto signatures(const nlohmann::json& j) const
            -> std::set<RoleSignature> override;
    };

    /**
     * The Update Frameworkd 'root' role implementation.
     *
     * TUF v1.0.17 §2.1.1
     * https://theupdateframework.github.io/specification/latest/#root
     */
    class RootImpl final : public RootRole
    {
    public:

        RootImpl(const fs::u8path& p);
        RootImpl(const nlohmann::json& j);

        [[nodiscard]] auto self_keys() const -> RoleFullKeys override;

        auto build_index_checker(
            const Context& context,
            const TimeRef& time_reference,
            const std::string& url,
            const fs::u8path& cache_path
        ) const -> std::unique_ptr<RepoIndexChecker> override;

        friend void to_json(nlohmann::json& j, const RootImpl& r);
        friend void from_json(const nlohmann::json& j, RootImpl& r);

    private:

        void load_from_json(const nlohmann::json& j);

        auto create_update(const nlohmann::json& j) -> std::unique_ptr<RootRole> override;

        [[nodiscard]] auto mandatory_defined_roles() const -> std::set<std::string> override;
        [[nodiscard]] auto optionally_defined_roles() const -> std::set<std::string> override;

        void set_defined_roles(
            const std::map<std::string, Key>& keys,
            const std::map<std::string, RoleKeys>& roles
        );
    };
}
#endif
