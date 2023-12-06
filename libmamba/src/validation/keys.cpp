// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <utility>

#include <nlohmann/json.hpp>

#include "mamba/validation/keys.hpp"

namespace mamba::validation
{
    auto Key::from_ed25519(std::string keyval) -> Key
    {
        return { "ed25519", "ed25519", std::move(keyval) };
    }

    void to_json(nlohmann::json& j, const Key& key)
    {
        j = { { "keytype", key.keytype }, { "scheme", key.scheme }, { "keyval", key.keyval } };
    }

    void from_json(const nlohmann::json& j, Key& key)
    {
        j.at("keytype").get_to(key.keytype);
        j.at("scheme").get_to(key.scheme);
        j.at("keyval").get_to(key.keyval);
    }

    void to_json(nlohmann::json& j, const RoleSignature& role_sig)
    {
        j = { { "keyid", role_sig.keyid }, { "sig", role_sig.sig } };
        if (!role_sig.pgp_trailer.empty())
        {
            j["other_headers"] = role_sig.pgp_trailer;
        }
    }

    void from_json(const nlohmann::json& j, RoleSignature& role_sig)
    {
        j.at("keyid").get_to(role_sig.keyid);
        j.at("sig").get_to(role_sig.sig);
        if (j.find("other_headers") != j.end())
        {
            j.at("other_headers").get_to(role_sig.pgp_trailer);
        }
    }

    auto operator<(const RoleSignature& rs1, const RoleSignature& rs2) -> bool
    {
        return rs1.keyid < rs2.keyid;
    };

    void to_json(nlohmann::json& j, const RoleKeys& role_keys)
    {
        j = { { "keyids", role_keys.keyids }, { "threshold", role_keys.threshold } };
    }

    void from_json(const nlohmann::json& j, RoleKeys& role_keys)
    {
        j.at("keyids").get_to(role_keys.keyids);
        j.at("threshold").get_to(role_keys.threshold);
    }

    auto RolePubKeys::to_role_keys() const -> RoleKeys
    {
        return { pubkeys, threshold };
    }

    void to_json(nlohmann::json& j, const RolePubKeys& role_keys)
    {
        j = { { "pubkeys", role_keys.pubkeys }, { "threshold", role_keys.threshold } };
    }

    void from_json(const nlohmann::json& j, RolePubKeys& role_keys)
    {
        j.at("pubkeys").get_to(role_keys.pubkeys);
        j.at("threshold").get_to(role_keys.threshold);
    }

    RoleFullKeys::RoleFullKeys(const std::map<std::string, Key>& keys_, const std::size_t& threshold_)
        : keys(keys_)
        , threshold(threshold_){};

    auto RoleFullKeys::to_keys() const -> std::map<std::string, Key>
    {
        return keys;
    }

    auto RoleFullKeys::to_roles() const -> RoleKeys
    {
        std::vector<std::string> keyids;
        for (auto& k : keys)
        {
            keyids.push_back(k.first);
        }
        return { keyids, threshold };
    }

    void to_json(nlohmann::json& j, const RoleFullKeys& k)
    {
        j = { { "keys", k.keys }, { "threshold", k.threshold } };
    }

    void from_json(const nlohmann::json& j, RoleFullKeys& k)
    {
        j.at("keys").get_to(k.keys);
        j.at("threshold").get_to(k.threshold);
    }
}
