// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_VALIDATION_UPDATE_FRAMEWORK_V0_6_HPP
#define MAMBA_VALIDATION_UPDATE_FRAMEWORK_V0_6_HPP

#include <memory>
#include <set>
#include <string>

#include <nlohmann/json_fwd.hpp>

#include "mamba/validation/keys.hpp"
#include "mamba/validation/update_framework.hpp"

namespace mamba::validation::v0_6
{
    /**
     * The Update Frameworkd ``conda-content-trust`` v0.6.0 specific implementation.
     *
     * This is a variation of TUF specification.
     */
    class SpecImpl final : public SpecBase
    {
    public:

        SpecImpl(std::string sv = "0.6.0");

        [[nodiscard]] auto json_key() const -> std::string override;
        [[nodiscard]] auto expiration_json_key() const -> std::string override;

        [[nodiscard]] auto signatures(const nlohmann::json& j) const
            -> std::set<RoleSignature> override;

        [[nodiscard]] auto canonicalize(const nlohmann::json& j) const -> std::string override;
        [[nodiscard]] auto upgradable() const -> bool override;
    };

    class V06RoleBaseExtension
    {
    public:

        void set_timestamp(const std::string& ts);

        [[nodiscard]] auto timestamp() const -> std::string;

    protected:

        std::string m_timestamp;

        void check_timestamp_format() const;
    };

    // Forward declaration of KeyMgrRole.
    class KeyMgrRole;

    /**
     * 'root' role implementation.
     */
    class RootImpl final
        : public RootRole
        , public V06RoleBaseExtension
    {
    public:

        RootImpl(const fs::u8path& p);
        RootImpl(const nlohmann::json& j);
        RootImpl(const std::string& json_str);

        /**
         * Return a ``RepoIndexChecker`` implementation (derived class) from repository base URL.
         */
        auto build_index_checker(
            const Context& context,
            const TimeRef& time_reference,
            const std::string& url,
            const fs::u8path& cache_path
        ) const -> std::unique_ptr<RepoIndexChecker> override;

        [[nodiscard]] auto self_keys() const -> RoleFullKeys override;

        [[nodiscard]] auto upgraded_signable() const -> nlohmann::json;
        auto
        upgraded_signature(const nlohmann::json& j, const std::string& pk, const std::byte* sk) const
            -> RoleSignature;

        [[nodiscard]] auto create_key_mgr(const fs::u8path& p) const -> KeyMgrRole;
        [[nodiscard]] auto create_key_mgr(const nlohmann::json& j) const -> KeyMgrRole;

        friend void to_json(nlohmann::json& j, const RootImpl& r);
        friend void from_json(const nlohmann::json& j, RootImpl& r);

    private:

        void load_from_json(const nlohmann::json& j);

        auto create_update(const nlohmann::json& j) -> std::unique_ptr<RootRole> override;

        [[nodiscard]] auto mandatory_defined_roles() const -> std::set<std::string> override;
        [[nodiscard]] auto optionally_defined_roles() const -> std::set<std::string> override;

        void set_defined_roles(std::map<std::string, RolePubKeys> keys);
    };


    class PkgMgrRole;

    /**
     * The Update Framework 'key_mgr' role implementation.
     */
    class KeyMgrRole final
        : public RoleBase
        , public V06RoleBaseExtension
    {
    public:

        KeyMgrRole(const fs::u8path& p, RoleFullKeys keys, const std::shared_ptr<SpecBase> spec);
        KeyMgrRole(const nlohmann::json& j, RoleFullKeys keys, const std::shared_ptr<SpecBase> spec);
        KeyMgrRole(const std::string& json_str, RoleFullKeys keys, const std::shared_ptr<SpecBase> spec);

        // std::set<std::string> roles() const override;
        [[nodiscard]] auto self_keys() const -> RoleFullKeys override;

        [[nodiscard]] auto create_pkg_mgr(const fs::u8path& p) const -> PkgMgrRole;
        [[nodiscard]] auto create_pkg_mgr(const nlohmann::json& j) const -> PkgMgrRole;

        /**
         * Return a ``RepoIndexChecker`` implementation (derived class) from repository base URL.
         */
        auto build_index_checker(
            const Context& context,
            const TimeRef& time_reference,
            const std::string& url,
            const fs::u8path& cache_path
        ) const -> std::unique_ptr<RepoIndexChecker>;

        friend void to_json(nlohmann::json& j, const KeyMgrRole& r);
        friend void from_json(const nlohmann::json& j, KeyMgrRole& r);

    private:

        void load_from_json(const nlohmann::json& j);

        RoleFullKeys m_keys;
        std::map<std::string, RolePubKeys> m_delegations;

        [[nodiscard]] auto mandatory_defined_roles() const -> std::set<std::string> override;
        [[nodiscard]] auto optionally_defined_roles() const -> std::set<std::string> override;

        void set_defined_roles(std::map<std::string, RolePubKeys> keys);
    };

    /**
     * The Update Framework 'pkg_mgr' role implementation.
     *
     * This role inherits from ``RepoIndexChecker`` and will be used by ``RepoChecker`` to
     * perform the repository index verification.
     */
    class PkgMgrRole final
        : public RoleBase
        , public V06RoleBaseExtension
        , public RepoIndexChecker
    {
    public:

        PkgMgrRole(RoleFullKeys keys, const std::shared_ptr<SpecBase> spec);
        PkgMgrRole(const fs::u8path& p, RoleFullKeys keys, const std::shared_ptr<SpecBase> spec);
        PkgMgrRole(const nlohmann::json& j, RoleFullKeys keys, const std::shared_ptr<SpecBase> spec);
        PkgMgrRole(const std::string& json_str, RoleFullKeys keys, const std::shared_ptr<SpecBase> spec);

        void verify_index(const fs::u8path& p) const override;
        void verify_index(const nlohmann::json& j) const override;
        void
        verify_package(const nlohmann::json& signed_data, const nlohmann::json& signatures) const override;

        friend void to_json(nlohmann::json& j, const PkgMgrRole& r);
        friend void from_json(const nlohmann::json& j, PkgMgrRole& r);

    private:

        void load_from_json(const nlohmann::json& j);

        [[nodiscard]] auto self_keys() const -> RoleFullKeys override;
        [[nodiscard]] auto pkg_signatures(const nlohmann::json& j) const -> std::set<RoleSignature>;
        void
        check_pkg_signatures(const nlohmann::json& signed_data, const nlohmann::json& signatures) const;

        void set_defined_roles(std::map<std::string, RolePubKeys> keys);

        RoleFullKeys m_keys;

        friend class KeyMgrRole;
    };
}
#endif
