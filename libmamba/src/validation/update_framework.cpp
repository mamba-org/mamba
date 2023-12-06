// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <fstream>
#include <regex>
#include <set>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "mamba/core/output.hpp"
#include "mamba/fs/filesystem.hpp"
#include "mamba/util/string.hpp"
#include "mamba/validation/errors.hpp"
#include "mamba/validation/tools.hpp"
#include "mamba/validation/update_framework.hpp"

namespace mamba::validation
{
    SpecBase::SpecBase(std::string spec_version)
        : m_spec_version(std::move(spec_version))
    {
    }

    auto SpecBase::version_str() const -> std::string
    {
        return m_spec_version;
    }

    auto SpecBase::compatible_prefix() const -> std::string
    {
        auto split_spec_version = util::split(m_spec_version, ".", 2);
        auto spec_version_major = std::stoi(split_spec_version[0]);
        if (spec_version_major == 0)
        {
            return split_spec_version[0] + "." + split_spec_version[1];
        }
        else
        {
            return split_spec_version[0];
        }
    }

    auto SpecBase::upgrade_prefix() const -> std::vector<std::string>
    {
        auto split_spec_version = util::split(m_spec_version, ".", 2);
        auto spec_version_major = std::stoi(split_spec_version[0]);
        auto spec_version_minor = std::stoi(split_spec_version[1]);
        if (spec_version_major == 0)
        {
            // Return the most recent possible upgrade first
            return { "1", split_spec_version[0] + "." + std::to_string(spec_version_minor + 1) };
        }
        else
        {
            return { std::to_string(spec_version_major + 1) };
        }
    }

    auto SpecBase::is_compatible(const fs::u8path& p) const -> bool
    {
        std::regex name_re;
        std::smatch matches;
        std::size_t min_match_size;

        std::string f_name = p.filename().string();
        std::string f_spec_version_str, f_version_str, f_type, f_ext;

        name_re = R"(^(?:[1-9]+\d*.)?(?:sv([1-9]\d*|0\.[1-9]\d*).)?(\w+)\.(\w+)$)";
        min_match_size = 3;

        if (std::regex_search(f_name, matches, name_re) && (min_match_size <= matches.size()))
        {
            f_spec_version_str = matches[1].str();
            if (!f_spec_version_str.empty())
            {
                return is_compatible(matches[1].str() + ".");
            }
            else
            {
                std::ifstream i(p.std_path());
                nlohmann::json j;
                i >> j;
                return is_compatible(j);
            }
        }
        else
        {
            return false;
        }
    }

    auto SpecBase::is_compatible(const std::string& version) const -> bool
    {
        return util::starts_with(version, compatible_prefix() + ".");
    }

    auto SpecBase::is_compatible(const nlohmann::json& j) const -> bool
    {
        auto spec_version = get_json_value(j);
        if (!spec_version.empty())
        {
            return is_compatible(spec_version);
        }
        else
        {
            return false;
        }
    }

    auto SpecBase::is_upgrade(const std::string& version) const -> bool
    {
        auto upgrade_prefixes = upgrade_prefix();
        std::vector<std::string_view> possible_upgrades;
        for (auto& s : upgrade_prefixes)
        {
            s += '.';
            possible_upgrades.push_back(s);
        }

        return util::starts_with_any(version, possible_upgrades);
    }

    auto SpecBase::is_upgrade(const nlohmann::json& j) const -> bool
    {
        auto spec_version = get_json_value(j);
        if (!spec_version.empty())
        {
            return is_upgrade(spec_version);
        }
        else
        {
            return false;
        }
    }

    auto SpecBase::get_json_value(const nlohmann::json& j) const -> std::string
    {
        try
        {
            return j.at("signed").at(json_key()).get<std::string>();
        }
        catch (const nlohmann::json::exception& e)
        {
            LOG_DEBUG << "Invalid 'root' metadata, impossible to check spec version compatibility: "
                      << e.what();
            return "";
        }
    }

    auto SpecBase::canonicalize(const nlohmann::json& j) const -> std::string
    {
        return j.dump();
    }

    auto SpecBase::upgradable() const -> bool
    {
        return false;
    };

    auto operator==(const SpecBase& sv1, const SpecBase& sv2) -> bool
    {
        return sv1.version_str() == sv2.version_str();
    }

    auto operator!=(const SpecBase& sv1, const SpecBase& sv2) -> bool
    {
        return sv1.version_str() != sv2.version_str();
    }

    RoleBase::RoleBase(std::string type, std::shared_ptr<SpecBase> sv)
        : m_type(std::move(type))
    {
        p_spec = std::move(sv);
    };

    RoleBase::~RoleBase() = default;

    auto RoleBase::type() const -> std::string
    {
        return m_type;
    }

    auto RoleBase::version() const -> std::size_t
    {
        return m_version;
    }

    auto RoleBase::expires() const -> std::string
    {
        return m_expires;
    }

    auto RoleBase::file_ext() const -> std::string
    {
        return m_ext;
    }

    auto RoleBase::spec_version() const -> SpecBase&
    {
        return *p_spec;
    }

    auto RoleBase::spec_impl() const -> std::shared_ptr<SpecBase>
    {
        return p_spec;
    }

    auto RoleBase::expired(const TimeRef& time_reference) const -> bool
    {
        return time_reference.timestamp().compare(m_expires) < 0 ? false : true;
    }

    auto RoleBase::canonicalize(const nlohmann::json& j) const -> std::string
    {
        return p_spec->canonicalize(j);
    }

    auto RoleBase::signatures(const nlohmann::json& j) const -> std::set<RoleSignature>
    {
        return p_spec->signatures(j);
    }

    auto RoleBase::roles() const -> std::set<std::string>
    {
        std::set<std::string> r;
        for (auto& it : m_defined_roles)
        {
            r.insert(it.first);
        }
        return r;
    }

    auto RoleBase::mandatory_defined_roles() const -> std::set<std::string>
    {
        return {};
    }

    auto RoleBase::optionally_defined_roles() const -> std::set<std::string>
    {
        return {};
    }

    void RoleBase::check_expiration_format() const
    {
        check_timestamp_metadata_format(m_expires);
    }

    void RoleBase::check_defined_roles(bool allow_any) const
    {
        auto mandatory_roles = mandatory_defined_roles();
        auto optional_roles = optionally_defined_roles();
        auto all_roles = mandatory_roles;
        all_roles.insert(optional_roles.cbegin(), optional_roles.cend());

        if (!allow_any)
        {
            for (const auto& r : roles())
            {
                if (all_roles.find(r) == all_roles.end())
                {
                    LOG_ERROR << "Invalid role defined in '" << type() << "' metadata: '" << r << "'";
                    throw role_metadata_error();
                }
            }
        }

        auto current_roles = roles();
        if (!std::includes(
                current_roles.begin(),
                current_roles.end(),
                mandatory_roles.begin(),
                mandatory_roles.end()
            ))
        {
            std::vector<std::string> diff;
            std::set_difference(
                mandatory_roles.begin(),
                mandatory_roles.end(),
                current_roles.begin(),
                current_roles.end(),
                std::inserter(diff, diff.end())
            );
            LOG_ERROR << "Missing roles while loading '" << type() << "' metadata: '"
                      << ::mamba::util::join(", ", diff) << "'";
            throw role_metadata_error();
        }

        for (const auto& r : all_keys())
        {
            auto r_keys = r.second;
            if (r_keys.keys.empty())
            {
                LOG_ERROR << "'" << type()
                          << "' metadata should declare at least one key ID for role: '" << r.first
                          << "'";
                throw role_metadata_error();
            }
            if (r_keys.threshold == 0)
            {
                LOG_ERROR << "'" << type()
                          << "' metadata should declare at least a 'threshold' of 1 for role: '"
                          << r.first << "'";
                throw role_metadata_error();
            }
        }
    }

    auto RoleBase::all_keys() const -> std::map<std::string, RoleFullKeys>
    {
        return m_defined_roles;
    }

    void RoleBase::set_spec_version(std::shared_ptr<SpecBase> new_sv)
    {
        auto& current_sv = spec_version();

        if (!current_sv.is_compatible(new_sv->version_str()))
        {
            LOG_ERROR << "Incompatible 'spec_version' found in 'root' metadata, should start with '"
                      << current_sv.compatible_prefix() << "' but is: '" << new_sv->version_str()
                      << "'";

            throw spec_version_error();
        }

        p_spec = std::move(new_sv);
    }

    void RoleBase::set_expiration(const std::string& expires)
    {
        m_expires = expires;
    }

    auto RoleBase::read_json_file(const fs::u8path& p, bool update) const -> nlohmann::json
    {
        if (!fs::exists(p))
        {
            LOG_ERROR << "File not found for '" << type() << "' role: " << p.string();
            throw role_file_error();
        }

        std::regex name_re;
        std::smatch matches;
        std::size_t min_match_size;

        std::string f_name = p.filename().string();
        std::string f_spec_version_str, f_version_str, f_type, f_ext;

        // Files should be named using one of the following structures:
        // - Trusted (reference) file:
        //   - FILENAME.EXT
        //   - svSPEC_VERSION_MAJOR.FILENAME.EXT
        // - Update file:
        //   - VERSION_NUMBER.FILENAME.EXT
        //   - VERSION_NUMBER.svSPEC_VERSION_MAJOR.FILENAME.EXT
        if (update)
        {
            name_re = R"(^(?:([1-9]+\d*).)(?:sv([1-9]\d*|0\.[1-9]\d*).)?(\w+)\.(\w+)$)";
            min_match_size = 4;
        }
        else
        {
            name_re = R"(^(?:[1-9]+\d*.)?(?:sv([1-9]\d*|0\.[1-9]\d*).)?(\w+)\.(\w+)$)";
            min_match_size = 3;
        }

        if (std::regex_search(f_name, matches, name_re) && (min_match_size <= matches.size()))
        {
            auto match_size = matches.size();

            if (update)
            {
                f_version_str = matches[1].str();
            }

            f_type = matches[match_size - 2].str();
            f_ext = matches[match_size - 1].str();

            if ((min_match_size + 1) == match_size)
            {
                f_spec_version_str = matches[match_size - 3].str();
            }
        }
        else
        {
            LOG_ERROR << "Invalid file name for 'root' metadata update: " << f_name;
            throw role_file_error();
        }

        if (f_ext != file_ext())
        {
            LOG_ERROR << "'root' metadata file should have 'json' extension, not: '" << f_ext << "'";
            throw role_file_error();
        }
        if (f_type != type())
        {
            LOG_ERROR << "'root' metadata file should have 'root' type, not: '" << f_type << "'";
            throw role_file_error();
        }
        if (!f_spec_version_str.empty())
        {
            auto new_spec_version_str = f_spec_version_str + ".";

            if (update && spec_version().is_upgrade(new_spec_version_str)
                && !spec_version().upgradable())
            {
                LOG_ERROR << "Please check for a client update, unsupported spec version: '"
                          << f_spec_version_str << "'";
                throw spec_version_error();
            }
            else if (!((!update && spec_version().is_compatible(new_spec_version_str))
                       || (update && spec_version().is_upgrade(new_spec_version_str))))
            {
                LOG_ERROR << "Invalid spec version specified in file name: '" << f_spec_version_str
                          << "'";
                throw role_file_error();
            }
        }

        if (update)
        {
            // Check version number in filename is N+1
            unsigned long f_version;
            try
            {
                f_version = std::stoul(f_version_str);
            }
            catch (...)
            {
                LOG_ERROR << "Invalid version in file name for 'root' metadata update: "
                          << f_version_str;
                throw role_file_error();
            }
            if (f_version != (version() + 1))
            {
                LOG_ERROR << "'root' metadata file name should start with N+1 version ("
                          << version() + 1 << "), but starts with: " << f_version;
                throw role_file_error();
            }
        }

        std::ifstream i(p.std_path());
        nlohmann::json j;
        i >> j;

        return j;
    }

    void RoleBase::check_role_signatures(const nlohmann::json& data, const RoleBase& role)
    {
        std::string signed_data = role.canonicalize(data["signed"]);
        auto signatures = role.signatures(data);
        auto k = self_keys();

        try
        {
            check_signatures(signed_data, signatures, k);
        }
        catch (const threshold_error& e)
        {
            LOG_ERROR << "Validation failed on role '" << type() << "' : " << e.what();
            throw role_error();
        }
    }

    void RoleBase::check_signatures(
        const std::string& signed_data,
        const std::set<RoleSignature>& signatures,
        const RoleFullKeys& keyring
    ) const
    {
        std::size_t valid_sig = 0;

        for (auto& s : signatures)
        {
            auto it = keyring.keys.find(s.keyid);
            if (it != keyring.keys.end())
            {
                auto& pk = it->second.keyval;
                int status;

                if (s.pgp_trailer.empty())
                {
                    status = verify(signed_data, pk, s.sig);
                }
                else
                {
                    status = verify_gpg(signed_data, s.pgp_trailer, pk, s.sig);
                }

                if (status == 1)
                {
                    ++valid_sig;
                }
                else
                {
                    LOG_WARNING << "Invalid signature of metadata using keyid: " << s.keyid;
                }
            }
            else
            {
                LOG_WARNING << "Invalid keyid: " << s.keyid;
            }
            if (valid_sig >= keyring.threshold)
            {
                break;
            }
        }

        if (valid_sig < keyring.threshold)
        {
            LOG_ERROR << "Threshold of valid signatures is not met (" << valid_sig << "/"
                      << keyring.threshold << ")";
            throw threshold_error();
        }
    }

    void to_json(nlohmann::json& j, const RoleBase& role)
    {
        j = { { "version", role.version() }, { "expires", role.expires() } };
    }

    void from_json(const nlohmann::json& j, RoleBase& role)
    {
        role.m_version = j.at("version");
        role.set_expiration(j.at(role.spec_version().expiration_json_key()));
    }

    RootRole::RootRole(std::shared_ptr<SpecBase> spec)
        : RoleBase("root", spec)
    {
    }

    auto RootRole::possible_update_files() -> std::vector<fs::u8path>
    {
        auto new_v = std::to_string(version() + 1);
        auto compat_spec = spec_impl()->compatible_prefix();
        auto upgrade_spec = spec_impl()->upgrade_prefix();

        std::vector<fs::u8path> files;
        // upgrade first
        for (auto& s : upgrade_spec)
        {
            files.emplace_back(
                ::mamba::util::join(".", std::vector<std::string>({ new_v, "sv" + s, "root.json" }))
            );
        }
        // compatible next
        files.emplace_back(::mamba::util::join(
            ".",
            std::vector<std::string>({ new_v, "sv" + compat_spec, "root.json" })
        ));
        // then finally undefined spec
        files.emplace_back(::mamba::util::join(".", std::vector<std::string>({ new_v, "root.json" })));

        return files;
    }

    auto RootRole::update(fs::u8path path) -> std::unique_ptr<RootRole>
    {
        auto j = read_json_file(path, true);
        return update(j);
    }

    // `create_update` currently catch a possible spec version update by testing
    // and extracting spec version from JSON. It could be done upstream (in
    // `update(fs::u8path)`) if we decide to specify the spec version in the file name.
    // The filename would take the form VERSION_NUMBER.SPECVERSION.FILENAME.EXT
    // To disambiguate version and spec version: 1.sv0.6.root.json or 1.sv1.root.json
    auto RootRole::update(nlohmann::json j) -> std::unique_ptr<RootRole>
    {
        // TUF spec 5.3.4 - Check for an arbitrary software attack
        // Check signatures against current keyids and threshold in 'RootImpl' constructor
        auto root_update = create_update(j);

        // Check signatures against new keyids and threshold
        // check_role_signatures(j, *root_update);

        // TUF spec 5.3.5 - Check for a rollback attack
        // Version number has to be N+1
        if (root_update->version() != (version() + 1))
        {
            if (root_update->version() > (version() + 1))
            {
                LOG_ERROR << "Invalid 'root' metadata version, should be exactly N+1";
                throw role_metadata_error();
            }
            else
            {
                LOG_ERROR << "Possible rollback attack of 'root' metadata";
                throw rollback_error();
            }
        }
        return root_update;
    }
}
