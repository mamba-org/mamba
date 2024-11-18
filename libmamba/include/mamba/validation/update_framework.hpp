// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_VALIDATION_UPDATE_FRAMEWORK_HPP
#define MAMBA_VALIDATION_UPDATE_FRAMEWORK_HPP

#include <memory>
#include <set>
#include <string>
#include <vector>

#include <nlohmann/json_fwd.hpp>

#include "mamba/core/timeref.hpp"
#include "mamba/validation/keys.hpp"

namespace mamba
{
    class Context;

    namespace fs
    {
        class u8path;
    }
}

namespace mamba::validation
{
    class RepoIndexChecker;

    /**
     * Base class for spec implementations.
     */
    class SpecBase
    {
    public:

        virtual ~SpecBase() = default;

        [[nodiscard]] auto version_str() const -> std::string;

        [[nodiscard]] virtual auto canonicalize(const nlohmann::json& j) const -> std::string;

        [[nodiscard]] auto compatible_prefix() const -> std::string;
        [[nodiscard]] auto upgrade_prefix() const -> std::vector<std::string>;

        [[nodiscard]] auto is_compatible(const fs::u8path& p) const -> bool;
        [[nodiscard]] auto is_compatible(const nlohmann::json& j) const -> bool;
        [[nodiscard]] auto is_compatible(const std::string& version) const -> bool;

        [[nodiscard]] auto is_upgrade(const nlohmann::json& j) const -> bool;
        [[nodiscard]] auto is_upgrade(const std::string& version) const -> bool;

        [[nodiscard]] virtual auto upgradable() const -> bool;

        [[nodiscard]] virtual auto json_key() const -> std::string = 0;
        [[nodiscard]] virtual auto expiration_json_key() const -> std::string = 0;

        [[nodiscard]] virtual auto signatures(const nlohmann::json& j) const
            -> std::set<RoleSignature>
            = 0;

    protected:

        SpecBase(std::string spec_version);

        [[nodiscard]] auto get_json_value(const nlohmann::json& j) const -> std::string;

    private:

        std::string m_spec_version;
    };

    auto operator==(const SpecBase& sv1, const SpecBase& sv2) -> bool;
    auto operator!=(const SpecBase& sv1, const SpecBase& sv2) -> bool;

    /**
     * Base class for role implementation.
     */
    class RoleBase
    {
    public:

        RoleBase(std::string type, std::shared_ptr<SpecBase> sv);

        virtual ~RoleBase() = 0;

        [[nodiscard]] auto type() const -> std::string;
        [[nodiscard]] auto spec_version() const -> SpecBase&;
        [[nodiscard]] auto version() const -> std::size_t;
        [[nodiscard]] auto file_ext() const -> std::string;
        [[nodiscard]] auto expires() const -> std::string;

        [[nodiscard]] auto expired(const TimeRef& time_reference) const -> bool;

        [[nodiscard]] auto roles() const -> std::set<std::string>;
        [[nodiscard]] auto signatures(const nlohmann::json& j) const -> std::set<RoleSignature>;

        [[nodiscard]] virtual auto self_keys() const -> RoleFullKeys = 0;
        [[nodiscard]] auto all_keys() const -> std::map<std::string, RoleFullKeys>;

    protected:

        [[nodiscard]] auto read_json_file(const fs::u8path& p, bool update = false) const
            -> nlohmann::json;

        /**
         * Check that a threshold of valid signatures is met
         * for the signed metadata of a role, using another
         * role keys (possibly the same).
         * Both signed and signatures metadata are contained
         * in 'data'.
         */
        void check_role_signatures(const nlohmann::json& data, const RoleBase& role);
        /**
         * Check that a threshold of valid signatures is met
         * for the signed metadata, using a set of keys.
         */
        void check_signatures(
            const std::string& signed_data,
            const std::set<RoleSignature>& signatures,
            const RoleFullKeys& keyring
        ) const;

        void set_spec_version(std::shared_ptr<SpecBase> sv);
        void set_expiration(const std::string& expires);

        // Forwarding to spec implementation
        [[nodiscard]] auto canonicalize(const nlohmann::json& j) const -> std::string;
        // Return the spec implementation
        [[nodiscard]] auto spec_impl() const -> std::shared_ptr<SpecBase>;

        // Mandatory roles defined by the current role
        [[nodiscard]] virtual auto mandatory_defined_roles() const -> std::set<std::string>;
        // Optional roles defined by the current role
        [[nodiscard]] virtual auto optionally_defined_roles() const -> std::set<std::string>;

        // Check role
        void check_expiration_format() const;
        void check_defined_roles(bool allow_any = false) const;

        std::map<std::string, RoleFullKeys> m_defined_roles;

    private:

        std::string m_internal_type;
        std::string m_type;
        std::shared_ptr<SpecBase> p_spec;
        std::size_t m_version = 1;
        std::string m_expires;
        std::string m_ext = "json";

        friend void to_json(nlohmann::json& j, const RoleBase& r);
        friend void from_json(const nlohmann::json& j, RoleBase& r);
    };

    void to_json(nlohmann::json& j, const RoleBase& role);
    void from_json(const nlohmann::json& j, RoleBase& role);

    /**
     * 'root' role interface.
     */
    class RootRole : public RoleBase
    {
    public:

        ~RootRole() override = default;

        auto update(fs::u8path path) -> std::unique_ptr<RootRole>;
        auto update(nlohmann::json j) -> std::unique_ptr<RootRole>;

        auto possible_update_files() -> std::vector<fs::u8path>;

        virtual auto build_index_checker(
            const Context& context,
            const TimeRef& time_reference,
            const std::string& url,
            const fs::u8path& cache_path
        ) const -> std::unique_ptr<RepoIndexChecker>
            = 0;

    protected:

        RootRole(std::shared_ptr<SpecBase> spec);

    private:

        virtual auto create_update(const nlohmann::json& j) -> std::unique_ptr<RootRole> = 0;
    };

    /**
     * Interface that performs validity checks
     * on a repository packages index.
     */
    class RepoIndexChecker
    {
    public:

        virtual ~RepoIndexChecker() = default;
        virtual void verify_index(const nlohmann::json& j) const = 0;
        virtual void verify_index(const fs::u8path& p) const = 0;
        virtual void verify_package(const nlohmann::json& signed_data, const nlohmann::json& signatures) const = 0;

    protected:

        RepoIndexChecker() = default;
    };
}
#endif
