// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_VALIDATION_UPDATE_FRAMEWORK_ROLES_HPP
#define MAMBA_VALIDATION_UPDATE_FRAMEWORK_ROLES_HPP

#include <map>
#include <string>

#include <nlohmann/json_fwd.hpp>

namespace mamba::validation
{
    /**
     * Representation of the public part of a cryptographic key pair.
     */
    struct Key
    {
        std::string keytype = "";
        std::string scheme = "";
        std::string keyval = "";

        [[nodiscard]] static auto from_ed25519(std::string keyval) -> Key;
    };

    void to_json(nlohmann::json& j, const Key& k);
    void from_json(const nlohmann::json& j, Key& k);

    /**
     * Representation of a role signature.
     *
     * Optional 'pgp_trailer' will trigger special handling during verification to conform to
     * OpenPGP RFC4880.
     */
    struct RoleSignature
    {
        std::string keyid = "";
        std::string sig = "";
        std::string pgp_trailer = "";
    };

    void to_json(nlohmann::json& j, const RoleSignature& rs);
    void from_json(const nlohmann::json& j, RoleSignature& rs);

    [[nodiscard]] auto operator<(const RoleSignature& rs1, const RoleSignature& rs2) -> bool;

    /**
     * Store key IDs and threshold for a role.
     *
     * Key ID can be a hash of Key, or just its public key value.
     */
    struct RoleKeys
    {
        std::vector<std::string> keyids;
        std::size_t threshold;
    };

    void to_json(nlohmann::json& j, const RoleKeys& rk);
    void from_json(const nlohmann::json& j, RoleKeys& rk);

    /**
     * Store key values and threshold for role. Assumes key scheme/type is `ed25519`.
     */
    struct RolePubKeys
    {
        std::vector<std::string> pubkeys;
        std::size_t threshold;

        [[nodiscard]] auto to_role_keys() const -> RoleKeys;
    };

    void to_json(nlohmann::json& j, const RolePubKeys& rk);
    void from_json(const nlohmann::json& j, RolePubKeys& rk);

    /**
     * Store full keys and threshold for role.
     */
    struct RoleFullKeys
    {
        RoleFullKeys() = default;
        RoleFullKeys(const std::map<std::string, Key>& keys_, const std::size_t& threshold_);

        std::map<std::string, Key> keys;
        std::size_t threshold;

        [[nodiscard]] auto to_keys() const -> std::map<std::string, Key>;
        [[nodiscard]] auto to_roles() const -> RoleKeys;
    };

    void to_json(nlohmann::json& j, const RoleFullKeys& r);
    void from_json(const nlohmann::json& j, RoleFullKeys& r);
}
#endif
