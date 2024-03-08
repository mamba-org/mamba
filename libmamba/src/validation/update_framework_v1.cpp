// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <nlohmann/json.hpp>

#include "mamba/core/output.hpp"
#include "mamba/fs/filesystem.hpp"
#include "mamba/validation/errors.hpp"
#include "mamba/validation/update_framework_v1.hpp"

namespace mamba::validation::v1
{
    SpecImpl::SpecImpl(std::string sv)
        : SpecBase(std::move(sv))
    {
    }

    auto SpecImpl::json_key() const -> std::string
    {
        return "spec_version";
    }

    auto SpecImpl::expiration_json_key() const -> std::string
    {
        return "expires";
    }

    auto SpecImpl::signatures(const nlohmann::json& j) const -> std::set<RoleSignature>
    {
        auto sigs = j.at("signatures").get<std::vector<RoleSignature>>();
        std::set<RoleSignature> unique_sigs(sigs.cbegin(), sigs.cend());

        return unique_sigs;
    }

    RootImpl::RootImpl(const nlohmann::json& j)
        : RootRole(std::make_shared<SpecImpl>())

    {
        load_from_json(j);
    }

    RootImpl::RootImpl(const fs::u8path& path)
        : RootRole(std::make_shared<SpecImpl>())
    {
        auto j = read_json_file(path);
        load_from_json(j);
    }

    auto RootImpl::create_update(const nlohmann::json& j) -> std::unique_ptr<RootRole>
    {
        if (v1::SpecImpl().is_compatible(j))
        {
            return std::make_unique<RootImpl>(j);
        }
        else
        {
            LOG_ERROR << "Invalid spec version for 'root' update";
            throw spec_version_error();
        }
    }

    void RootImpl::load_from_json(const nlohmann::json& j)
    {
        from_json(j, *this);

        // TUF spec 5.3.4 - Check for an arbitrary software attack
        // Check signatures against current keyids and threshold
        check_role_signatures(j, *this);
    }

    auto RootImpl::self_keys() const -> RoleFullKeys
    {
        return m_defined_roles.at("root");
    }

    auto RootImpl::mandatory_defined_roles() const -> std::set<std::string>
    {
        return { "root", "snapshot", "targets", "timestamp" };
    }

    auto RootImpl::optionally_defined_roles() const -> std::set<std::string>
    {
        return { "mirrors" };
    }

    void RootImpl::set_defined_roles(
        const std::map<std::string, Key>& keys,
        const std::map<std::string, RoleKeys>& roles
    )
    {
        m_defined_roles.clear();

        for (auto& it : roles)
        {
            std::map<std::string, Key> role_keys;
            for (auto& keyid : it.second.keyids)
            {
                try
                {
                    role_keys.insert({ keyid, keys.at(keyid) });
                }
                catch (const std::out_of_range&)
                {
                    LOG_ERROR << "Missing key in 'keys' is used in '" << it.first
                              << "' delegation: '" << keyid << "'";
                    throw role_metadata_error();
                }
            }
            m_defined_roles.insert({ it.first, { role_keys, it.second.threshold } });
        }
    }

    auto RootImpl::build_index_checker(
        const Context&,
        const TimeRef& /*time_reference*/,
        const std::string& /*url*/,
        const fs::u8path& /*cache_path*/
    ) const -> std::unique_ptr<RepoIndexChecker>
    {
        std::unique_ptr<RepoIndexChecker> ptr;
        return ptr;
    }

    void to_json(nlohmann::json& j, const RootImpl& r)
    {
        to_json(j, static_cast<const RoleBase&>(r));
    }

    void from_json(const nlohmann::json& j, RootImpl& role)
    {
        auto j_signed = j.at("signed");
        try
        {
            from_json(j_signed, static_cast<RoleBase&>(role));

            auto type = j_signed.at("_type").get<std::string>();
            if (type != role.type())
            {
                LOG_ERROR << "Wrong '_type' found in 'root' metadata, should be 'root': '" << type
                          << "'";
                throw role_metadata_error();
            }

            role.set_spec_version(
                std::make_shared<SpecImpl>(j_signed.at("spec_version").get<std::string>())
            );

            auto keys = j_signed.at("keys").get<std::map<std::string, Key>>();
            auto roles = j_signed.at("roles").get<std::map<std::string, RoleKeys>>();
            role.set_defined_roles(keys, roles);
        }
        catch (const nlohmann::json::exception& e)
        {
            LOG_ERROR << "Invalid 'root' metadata: " << e.what();
            throw role_metadata_error();
        }

        role.check_expiration_format();
        role.check_defined_roles();
    }
}
