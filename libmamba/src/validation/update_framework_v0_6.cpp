// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <fstream>
#include <utility>

#include "mamba/core/download.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/util.hpp"
#include "mamba/specs/conda_url.hpp"
#include "mamba/util/encoding.hpp"
#include "mamba/validation/errors.hpp"
#include "mamba/validation/tools.hpp"
#include "mamba/validation/update_framework_v0_6.hpp"
#include "mamba/validation/update_framework_v1.hpp"

namespace mamba::validation::v0_6
{
    SpecImpl::SpecImpl(std::string sv)
        : SpecBase(std::move(sv))
    {
    }

    auto SpecImpl::json_key() const -> std::string
    {
        return "metadata_spec_version";
    }

    auto SpecImpl::expiration_json_key() const -> std::string
    {
        return "expiration";
    }

    auto SpecImpl::signatures(const nlohmann::json& j) const -> std::set<RoleSignature>
    {
        auto sigs = j.at("signatures").get<std::map<std::string, std::map<std::string, std::string>>>();
        std::set<RoleSignature> unique_sigs;

        for (auto& s : sigs)
        {
            std::string pgp_trailer = "";
            if (s.second.find("other_headers") != s.second.end())
            {
                pgp_trailer = s.second["other_headers"];
            }

            unique_sigs.insert(RoleSignature({ s.first, s.second.at("signature"), pgp_trailer }));
        }

        return unique_sigs;
    }

    auto SpecImpl::upgradable() const -> bool
    {
        return true;
    };

    auto SpecImpl::canonicalize(const nlohmann::json& j) const -> std::string
    {
        return j.dump(2);
    }

    void V06RoleBaseExtension::check_timestamp_format() const
    {
        check_timestamp_metadata_format(m_timestamp);
    }

    void V06RoleBaseExtension::set_timestamp(const std::string& ts)
    {
        m_timestamp = ts;
    }

    auto V06RoleBaseExtension::timestamp() const -> std::string
    {
        return m_timestamp;
    }

    RootImpl::RootImpl(const fs::u8path& path)
        : RootRole(std::make_shared<SpecImpl>())
    {
        auto j = read_json_file(path);
        load_from_json(j);
    }

    RootImpl::RootImpl(const nlohmann::json& j)
        : RootRole(std::make_shared<SpecImpl>())
    {
        load_from_json(j);
    }

    RootImpl::RootImpl(const std::string& json_str)
        : RootRole(std::make_shared<SpecImpl>())
    {
        load_from_json(nlohmann::json::parse(json_str));
    }

    auto RootImpl::create_update(const nlohmann::json& j) -> std::unique_ptr<RootRole>
    {
        if (SpecImpl().is_compatible(j))
        {
            return std::make_unique<RootImpl>(j);
        }
        else if (v1::SpecImpl().is_compatible(j))
        {
            LOG_DEBUG << "Updating 'root' role spec version";
            return std::make_unique<v1::RootImpl>(j);
        }
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

    auto RootImpl::upgraded_signable() const -> nlohmann::json
    {
        nlohmann::json v1_equivalent_root;

        v1_equivalent_root["roles"]["root"] = m_defined_roles.at("root").to_roles();
        v1_equivalent_root["roles"]["targets"] = m_defined_roles.at("key_mgr").to_roles();
        v1_equivalent_root["roles"]["snapshot"] = RoleKeys({ std::vector<std::string>(), 1 });
        v1_equivalent_root["roles"]["timestamp"] = RoleKeys({ std::vector<std::string>(), 1 });

        std::map<std::string, Key> v1_keys = m_defined_roles.at("root").to_keys();
        auto key_mgr_keys = m_defined_roles.at("key_mgr").to_keys();
        v1_keys.insert(key_mgr_keys.cbegin(), key_mgr_keys.cend());

        v1_equivalent_root["keys"] = v1_keys;
        v1_equivalent_root["_type"] = "root";
        v1_equivalent_root["version"] = version();
        v1_equivalent_root["spec_version"] = "1.0.17";
        v1_equivalent_root["expires"] = expires();

        return v1_equivalent_root;
    }

    auto
    RootImpl::upgraded_signature(const nlohmann::json& j, const std::string& pk, const std::byte* sk) const
        -> RoleSignature
    {
        std::array<std::byte, MAMBA_ED25519_SIGSIZE_BYTES> sig_bin;
        sign(j.dump(), sk, sig_bin.data());

        const auto sig_bin_data = sig_bin.data();
        auto sig_hex = util::bytes_to_hex_str(sig_bin_data, sig_bin_data + sig_bin.size());

        return { pk, sig_hex };
    }

    auto RootImpl::self_keys() const -> RoleFullKeys
    {
        return m_defined_roles.at("root");
    }

    auto RootImpl::mandatory_defined_roles() const -> std::set<std::string>
    {
        return { "root", "key_mgr" };
    }

    auto RootImpl::optionally_defined_roles() const -> std::set<std::string>
    {
        return {};
    }

    void RootImpl::set_defined_roles(std::map<std::string, RolePubKeys> keys)
    {
        m_defined_roles.clear();
        for (auto& it : keys)
        {
            std::map<std::string, Key> role_keys;
            for (auto& key : it.second.pubkeys)
            {
                role_keys.insert({ key, Key::from_ed25519(key) });
            }
            m_defined_roles.insert({ it.first, { role_keys, it.second.threshold } });
        }
    }

    auto RootImpl::build_index_checker(
        Context& context,
        const TimeRef& time_reference,
        const std::string& base_url,
        const fs::u8path& cache_path
    ) const -> std::unique_ptr<RepoIndexChecker>
    {
        fs::u8path metadata_path = cache_path / "key_mgr.json";

        auto tmp_dir = TemporaryDirectory();
        auto tmp_metadata_path = tmp_dir.path() / "key_mgr.json";

        const auto url = specs::CondaURL::parse(base_url) / "key_mgr.json";

        if (check_resource_exists(url.pretty_str(), context))
        {
            DownloadRequest request(
                "key_mgr.json",
                url.str(util::URL::Credentials::Show),
                tmp_metadata_path.string()
            );
            DownloadResult res = download(std::move(request), context);
            if (res)
            {
                KeyMgrRole key_mgr = create_key_mgr(tmp_metadata_path);

                // TUF spec 5.6.5 - Check for a freeze attack
                // 'key_mgr' (equivalent of 'targets') role should not be expired
                // https://theupdateframework.github.io/specification/latest/#update-targets
                if (key_mgr.expired(time_reference))
                {
                    LOG_ERROR << "Possible freeze attack of 'key_mgr' metadata.\nExpired: "
                              << key_mgr.expires();
                    throw freeze_error();
                }

                // TUF spec 5.6.6 - Persist targets metadata
                if (!cache_path.empty())
                {
                    if (fs::exists(metadata_path))
                    {
                        fs::remove(metadata_path);
                    }
                    fs::copy(tmp_metadata_path, metadata_path);
                }

                return key_mgr.build_index_checker(context, time_reference, base_url, cache_path);
            }
        }

        // Fallback to local cached-copy if existing
        if (fs::exists(metadata_path))
        {
            KeyMgrRole key_mgr = create_key_mgr(metadata_path);
            return key_mgr.build_index_checker(context, time_reference, base_url, cache_path);
        }

        LOG_ERROR << "Error while fetching 'key_mgr' metadata";
        throw fetching_error();
    }

    auto RootImpl::create_key_mgr(const fs::u8path& p) const -> KeyMgrRole
    {
        return { p, all_keys()["key_mgr"], spec_impl() };
    }

    auto RootImpl::create_key_mgr(const nlohmann::json& j) const -> KeyMgrRole
    {
        return { j, all_keys()["key_mgr"], spec_impl() };
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

            role.set_timestamp(j_signed.at("timestamp").get<std::string>());

            auto type = j_signed.at("type").get<std::string>();
            if (type != role.type())
            {
                LOG_ERROR << "Wrong 'type' found in 'root' metadata, should be 'root': '" << type
                          << "'";
                throw role_metadata_error();
            }

            role.set_spec_version(
                std::make_shared<SpecImpl>(j_signed.at("metadata_spec_version").get<std::string>())
            );

            role.set_defined_roles(
                j_signed.at("delegations").get<std::map<std::string, RolePubKeys>>()
            );
        }
        catch (const nlohmann::json::exception& e)
        {
            LOG_ERROR << "Invalid 'root' metadata: " << e.what();
            throw role_metadata_error();
        }

        role.check_expiration_format();
        role.check_timestamp_format();
        role.check_defined_roles();
    }

    KeyMgrRole::KeyMgrRole(const fs::u8path& p, RoleFullKeys keys, const std::shared_ptr<SpecBase> spec)
        : RoleBase("key_mgr", spec)
        , m_keys(std::move(keys))
    {
        auto j = read_json_file(p);
        load_from_json(j);
    }

    KeyMgrRole::KeyMgrRole(const nlohmann::json& j, RoleFullKeys keys, const std::shared_ptr<SpecBase> spec)
        : RoleBase("key_mgr", spec)
        , m_keys(std::move(keys))
    {
        load_from_json(j);
    }

    KeyMgrRole::KeyMgrRole(
        const std::string& json_str,
        RoleFullKeys keys,
        const std::shared_ptr<SpecBase> spec
    )
        : RoleBase("key_mgr", spec)
        , m_keys(std::move(keys))
    {
        load_from_json(nlohmann::json::parse(json_str));
    }

    void KeyMgrRole::load_from_json(const nlohmann::json& j)
    {
        from_json(j, *this);
        // Check signatures against keyids and threshold
        check_role_signatures(j, *this);
    }

    auto KeyMgrRole::self_keys() const -> RoleFullKeys
    {
        return m_keys;
    }

    auto KeyMgrRole::create_pkg_mgr(const fs::u8path& p) const -> PkgMgrRole
    {
        return { p, all_keys()["pkg_mgr"], spec_impl() };
    }

    auto KeyMgrRole::create_pkg_mgr(const nlohmann::json& j) const -> PkgMgrRole
    {
        return { j, all_keys()["pkg_mgr"], spec_impl() };
    }

    auto KeyMgrRole::build_index_checker(
        Context& context,
        const TimeRef& time_reference,
        const std::string& base_url,
        const fs::u8path& cache_path
    ) const -> std::unique_ptr<RepoIndexChecker>
    {
        fs::u8path metadata_path = cache_path / "pkg_mgr.json";

        auto tmp_dir = TemporaryDirectory();
        auto tmp_metadata_path = tmp_dir.path() / "pkg_mgr.json";

        const auto url = mamba::util::URL::parse(base_url + "/pkg_mgr.json");

        if (check_resource_exists(url.pretty_str(), context))
        {
            DownloadRequest request("pkg_mgr.json", url.pretty_str(), tmp_metadata_path.string());
            DownloadResult res = download(std::move(request), context);

            if (res)
            {
                PkgMgrRole pkg_mgr = create_pkg_mgr(tmp_metadata_path);

                // TUF spec 5.6.5 - Check for a freeze attack
                // 'pkg_mgr' (equivalent of delegated 'targets') role should not be expired
                // https://theupdateframework.github.io/specification/latest/#update-targets
                if (pkg_mgr.expired(time_reference))
                {
                    LOG_ERROR << "Possible freeze attack of 'pkg_mgr' metadata.\nExpired: "
                              << pkg_mgr.expires();
                    throw freeze_error();
                }

                // TUF spec 5.6.6 - Persist targets metadata
                if (!cache_path.empty())
                {
                    if (fs::exists(metadata_path))
                    {
                        fs::remove(metadata_path);
                    }
                    fs::copy(tmp_metadata_path, metadata_path);
                }

                return std::make_unique<PkgMgrRole>(pkg_mgr);
            }
        }

        // Fallback to local cached-copy if existing
        if (fs::exists(metadata_path))
        {
            return std::make_unique<PkgMgrRole>(create_pkg_mgr(metadata_path));
        }

        LOG_ERROR << "Error while fetching 'pkg_mgr' metadata";
        throw fetching_error();
    }

    auto KeyMgrRole::mandatory_defined_roles() const -> std::set<std::string>
    {
        return { "pkg_mgr" };
    }

    auto KeyMgrRole::optionally_defined_roles() const -> std::set<std::string>
    {
        return {};
    }

    void KeyMgrRole::set_defined_roles(std::map<std::string, RolePubKeys> keys)
    {
        m_defined_roles.clear();
        for (auto& it : keys)
        {
            std::map<std::string, Key> role_keys;
            for (auto& key : it.second.pubkeys)
            {
                role_keys.insert({ key, Key::from_ed25519(key) });
            }
            m_defined_roles.insert({ it.first, { role_keys, it.second.threshold } });
        }
    }

    void to_json(nlohmann::json& j, const KeyMgrRole& r)
    {
        to_json(j, static_cast<const RoleBase&>(r));
    }

    void from_json(const nlohmann::json& j, KeyMgrRole& role)
    {
        auto j_signed = j.at("signed");
        try
        {
            from_json(j_signed, static_cast<RoleBase&>(role));

            role.set_timestamp(j_signed.at("timestamp").get<std::string>());

            auto type = j_signed.at("type").get<std::string>();
            if (type != role.type())
            {
                LOG_ERROR << "Wrong 'type' found in 'key_mgr' metadata, should be 'key_mgr': '"
                          << type << "'";
                throw role_metadata_error();
            }

            auto new_spec_version = j_signed.at(role.spec_version().json_key()).get<std::string>();
            if (role.spec_version() != SpecImpl(new_spec_version))
            {
                LOG_ERROR << "Invalid spec version '" << new_spec_version
                          << "' in 'key_mgr' metadata, it should match exactly 'root' spec version: '"
                          << role.spec_version().version_str() << "'";
                throw spec_version_error();
            }

            role.set_defined_roles(
                j_signed.at("delegations").get<std::map<std::string, RolePubKeys>>()
            );
        }
        catch (const nlohmann::json::exception& e)
        {
            LOG_ERROR << "Invalid 'key_mgr' metadata: " << e.what();
            throw role_metadata_error();
        }

        role.check_expiration_format();
        role.check_timestamp_format();
        role.check_defined_roles();
    }

    PkgMgrRole::PkgMgrRole(RoleFullKeys keys, const std::shared_ptr<SpecBase> spec)
        : RoleBase("pkg_mgr", spec)
        , m_keys(std::move(keys))
    {
    }

    PkgMgrRole::PkgMgrRole(const fs::u8path& p, RoleFullKeys keys, const std::shared_ptr<SpecBase> spec)
        : RoleBase("pkg_mgr", spec)
        , m_keys(std::move(keys))
    {
        auto j = read_json_file(p);
        load_from_json(j);
    }

    PkgMgrRole::PkgMgrRole(const nlohmann::json& j, RoleFullKeys keys, const std::shared_ptr<SpecBase> spec)
        : RoleBase("pkg_mgr", spec)
        , m_keys(std::move(keys))
    {
        load_from_json(j);
    }

    PkgMgrRole::PkgMgrRole(
        const std::string& json_str,
        RoleFullKeys keys,
        const std::shared_ptr<SpecBase> spec
    )
        : RoleBase("pkg_mgr", spec)
        , m_keys(std::move(keys))
    {
        load_from_json(nlohmann::json::parse(json_str));
    }

    void PkgMgrRole::load_from_json(const nlohmann::json& j)
    {
        from_json(j, *this);
        // Check signatures against keyids and threshold
        check_role_signatures(j, *this);
    }

    void PkgMgrRole::set_defined_roles(std::map<std::string, RolePubKeys> keys)
    {
        m_defined_roles.clear();
        for (auto& it : keys)
        {
            std::map<std::string, Key> role_keys;
            for (auto& key : it.second.pubkeys)
            {
                role_keys.insert({ key, Key::from_ed25519(key) });
            }
            m_defined_roles.insert({ it.first, { role_keys, it.second.threshold } });
        }
    }

    void to_json(nlohmann::json& j, const PkgMgrRole& r)
    {
        to_json(j, static_cast<const RoleBase&>(r));
    }

    void from_json(const nlohmann::json& j, PkgMgrRole& role)
    {
        auto j_signed = j.at("signed");
        try
        {
            from_json(j_signed, static_cast<RoleBase&>(role));

            role.set_timestamp(j_signed.at("timestamp").get<std::string>());

            auto type = j_signed.at("type").get<std::string>();
            if (type != role.type())
            {
                LOG_ERROR << "Wrong 'type' found in 'pkg_mgr' metadata, should be 'pkg_mgr': '"
                          << type << "'";
                throw role_metadata_error();
            }

            auto new_spec_version = j_signed.at(role.spec_version().json_key()).get<std::string>();
            if (role.spec_version() != SpecImpl(new_spec_version))
            {
                LOG_ERROR << "Invalid spec version '" << new_spec_version
                          << "' in 'pkg_mgr' metadata, it should match exactly 'root' spec version: '"
                          << role.spec_version().version_str() << "'";
                throw spec_version_error();
            }

            role.set_defined_roles(
                j_signed.at("delegations").get<std::map<std::string, RolePubKeys>>()
            );
        }
        catch (const nlohmann::json::exception& e)
        {
            LOG_ERROR << "Invalid 'pkg_mgr' metadata: " << e.what();
            throw role_metadata_error();
        }

        role.check_expiration_format();
        role.check_timestamp_format();
        role.check_defined_roles();
    }

    auto PkgMgrRole::self_keys() const -> RoleFullKeys
    {
        return m_keys;
    }

    auto PkgMgrRole::pkg_signatures(const nlohmann::json& j) const -> std::set<RoleSignature>
    {
        auto sigs = j.get<std::map<std::string, std::map<std::string, std::string>>>();
        std::set<RoleSignature> unique_sigs;

        for (auto& s : sigs)
        {
            std::string pgp_trailer = "";
            if (s.second.find("other_headers") != s.second.end())
            {
                pgp_trailer = s.second["other_headers"];
            }

            unique_sigs.insert(RoleSignature({ s.first, s.second.at("signature"), pgp_trailer }));
        }

        return unique_sigs;
    }

    void
    PkgMgrRole::check_pkg_signatures(const nlohmann::json& metadata, const nlohmann::json& signatures) const
    {
        std::string signed_data = canonicalize(metadata);
        auto sigs = pkg_signatures(signatures);
        auto k = self_keys();

        check_signatures(signed_data, sigs, k);
    }

    void PkgMgrRole::verify_index(const nlohmann::json& j) const
    {
        try
        {
            auto packages = j.at("packages").get<nlohmann::json::object_t>();
            auto sigs = j.at("signatures").get<nlohmann::json::object_t>();

            for (auto& it : packages)
            {
                auto pkg_name = it.first;
                auto pkg_meta = it.second;
                auto pkg_sigs = sigs.at(pkg_name).get<nlohmann::json::object_t>();

                try
                {
                    check_pkg_signatures(pkg_meta, pkg_sigs);
                }
                catch (const threshold_error& e)
                {
                    LOG_ERROR << "Validation failed on package: '" << pkg_name << "' : " << e.what();
                    throw package_error();
                }
            }
        }
        catch (const nlohmann::json::exception& e)
        {
            LOG_ERROR << "Invalid package index metadata: " << e.what();
            throw index_error();
        }
    }

    void PkgMgrRole::verify_index(const fs::u8path& p) const
    {
        if (!fs::exists(p))
        {
            LOG_ERROR << "'repodata' file not found at: " << p.string();
            throw index_error();
        }

        std::ifstream i(p.std_path());
        nlohmann::json j;
        i >> j;

        try
        {
            verify_index(j);
        }
        catch (const package_error& e)
        {
            LOG_ERROR << "Validation failed on package index: '" << p.string() << "' : " << e.what();
            throw index_error();
        }
    }

    void
    PkgMgrRole::verify_package(const nlohmann::json& signed_data, const nlohmann::json& signatures) const
    {
        try
        {
            check_pkg_signatures(signed_data, signatures);
        }
        catch (const threshold_error& e)
        {
            LOG_ERROR << "Validation failed on package: '" << signed_data.at("name")
                      << "' : " << e.what();
            throw package_error();
        }
    }
}  // namespace v06
