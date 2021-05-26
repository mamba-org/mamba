// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_VALIDATE_HPP
#define MAMBA_CORE_VALIDATE_HPP

#include "mamba/core/mamba_fs.hpp"
#include "mamba/core/util.hpp"

#include <nlohmann/json.hpp>

#include <string>
#include <vector>
#include <set>
#include <stdexcept>


namespace validate
{
    using nlohmann::json;

    std::string sha256sum(const std::string& path);
    std::string md5sum(const std::string& path);
    bool sha256(const std::string& path, const std::string& validation);
    bool md5(const std::string& path, const std::string& validation);
    bool file_size(const fs::path& path, std::uintmax_t validation);

    const std::size_t MAMBA_SHA256_SIZE_HEX = 64;
    const std::size_t MAMBA_SHA256_SIZE_BYTES = 32;
    const std::size_t MAMBA_ED25519_KEYSIZE_HEX = 64;
    const std::size_t MAMBA_ED25519_KEYSIZE_BYTES = 32;
    const std::size_t MAMBA_ED25519_SIGSIZE_HEX = 128;
    const std::size_t MAMBA_ED25519_SIGSIZE_BYTES = 64;

    int generate_ed25519_keypair(unsigned char* pk, unsigned char* sk);

    int sign(const std::string& data, const unsigned char* sk, unsigned char* signature);

    std::array<unsigned char, MAMBA_ED25519_SIGSIZE_BYTES> ed25519_sig_hex_to_bytes(
        const std::string& sig_hex) noexcept;

    std::array<unsigned char, MAMBA_ED25519_SIGSIZE_BYTES> ed25519_sig_hex_to_bytes(
        const std::string& sig_hex, int& error_code) noexcept;

    std::array<unsigned char, MAMBA_ED25519_KEYSIZE_BYTES> ed25519_key_hex_to_bytes(
        const std::string& key_hex) noexcept;

    std::array<unsigned char, MAMBA_ED25519_KEYSIZE_BYTES> ed25519_key_hex_to_bytes(
        const std::string& key_hex, int& error_code) noexcept;

    int verify(const unsigned char* data,
               std::size_t data_len,
               const unsigned char* pk,
               const unsigned char* signature);
    int verify(const std::string& data, const unsigned char* pk, const unsigned char* signature);
    int verify(const std::string& data,
               const std::string& pk_hex,
               const std::string& signature_hex);

    /**
     * Verify a GPG/PGP signature against the hash of the binary data and
     * the additional trailer added in V4 signature.
     * See RFC4880, section 5.2.4 https://datatracker.ietf.org/doc/html/rfc4880#section-5.2.4
     * This method assumes hash function to be SHA-256
     */
    int verify_gpg_hashed_msg(const unsigned char* data,
                              const unsigned char* pk,
                              const unsigned char* signature);
    int verify_gpg_hashed_msg(const std::string& data,
                              const unsigned char* pk,
                              const unsigned char* signature);
    int verify_gpg_hashed_msg(const std::string& data,
                              const std::string& pk,
                              const std::string& signature);

    /**
     * Verify a GPG/PGP signature against the binary data and
     * the additional trailer added in V4 signature.
     * See RFC4880, section 5.2.4 https://datatracker.ietf.org/doc/html/rfc4880#section-5.2.4
     * This method assumes hash function to be SHA-256
     */
    int verify_gpg(const std::string& data,
                   const std::string& gpg_v4_trailer,
                   const std::string& pk,
                   const std::string& signature);

    class trust_error : public std::exception
    {
    public:
        trust_error(const std::string& message) noexcept;
        virtual ~trust_error() = default;
        virtual const char* what() const noexcept override;

    private:
        std::string m_message;
    };

    class threshold_error : public trust_error
    {
    public:
        threshold_error() noexcept;
        virtual ~threshold_error() = default;
    };

    class role_metadata_error : public trust_error
    {
    public:
        role_metadata_error() noexcept;
        virtual ~role_metadata_error() = default;
    };

    class role_file_error : public trust_error
    {
    public:
        role_file_error() noexcept;
        virtual ~role_file_error() = default;
    };

    class rollback_error : public trust_error
    {
    public:
        rollback_error() noexcept;
        virtual ~rollback_error() = default;
    };

    class spec_version_error : public trust_error
    {
    public:
        spec_version_error() noexcept;
        virtual ~spec_version_error() = default;
    };

    struct Key
    {
        std::string keytype = "";
        std::string scheme = "";
        std::string keyval = "";

        static Key from_ed25519(std::string keyval)
        {
            return { "ed25519", "ed25519", keyval };
        }
    };

    struct RoleSignature
    {
        std::string keyid = "";
        std::string sig = "";
        std::string pgp_trailer = "";
    };

    bool operator<(const RoleSignature& rs1, const RoleSignature& rs2);

    enum Role
    {
        kRoot,
        kSnapshot,
        kTargets,
        kTimestamp,
        kMirrors,
        kInvalid = -1,
    };

    NLOHMANN_JSON_SERIALIZE_ENUM(Role,
                                 { { kInvalid, "invalid" },
                                   { kRoot, "root" },
                                   { kSnapshot, "snapshot" },
                                   { kTargets, "targets" },
                                   { kTimestamp, "timestamp" },
                                   { kMirrors, "mirrors" } });

    /**
     * Store key IDs and threshold for role.
     */
    struct RoleKeys
    {
        std::vector<std::string> keyids;
        std::size_t threshold;
    };

    /**
     * Store key values and threshold for role.
     * Assumes key scheme/type is `ed25519`.
     */
    struct RolePubKeys
    {
        std::vector<std::string> pubkeys;
        std::size_t threshold;

        RoleKeys to_role_keys() const;
    };

    /**
     * Store full keys and threshold for role.
     */
    struct RoleFullKeys
    {
        std::map<std::string, Key> keys;
        std::size_t threshold;
    };

    void to_json(json& j, const RoleFullKeys& r);
    void from_json(const json& j, RoleFullKeys& r);

    enum SpecVersion
    {
        kV06 = 0,
        kV1 = 1
    };

    class TimeRef
    {
    public:
        static TimeRef& instance();

        void set(const std::time_t& time);
        void set_now();
        std::string timestamp();

    private:
        TimeRef();
        ~TimeRef() = default;

        std::time_t m_time_ref;
    };

    class SpecVersionBase
    {
    public:
        virtual ~SpecVersionBase() = default;

        std::string version_str() const;

        virtual std::string canonicalize(const json& j) const;
        virtual bool upgradable() const;

        std::string compatible_starts_with() const;
        std::string upgradable_starts_with() const;

        bool is_compatible(const json& j) const;
        bool is_compatible(const std::string& version) const;

        bool is_upgradable(const json& j) const;
        bool is_upgradable(const std::string& version) const;

        virtual std::string json_key() const = 0;
        virtual std::string expiration_json_key() const = 0;

        virtual std::set<RoleSignature> signatures(const json& j) const = 0;

    protected:
        SpecVersionBase(const std::string& spec_version);
        SpecVersionBase() = delete;

        std::string get_json_value(const json& j) const;

    private:
        std::string m_spec_version;
    };

    bool operator==(const SpecVersionBase& sv1, const SpecVersionBase& sv2);
    bool operator!=(const SpecVersionBase& sv1, const SpecVersionBase& sv2);

    class RoleBase
    {
    public:
        RoleBase(const std::string& type, std::unique_ptr<SpecVersionBase> sv);

        virtual ~RoleBase() = 0;

        std::string type() const;
        SpecVersionBase& spec_version() const;
        std::size_t version() const;
        std::string file_ext() const;
        std::string expires() const;

        bool expired() const;

        std::set<RoleSignature> signatures(const json& j) const;
        virtual RoleFullKeys self_keys() const = 0;

        friend void to_json(json& j, const RoleBase* r);
        friend void from_json(const json& j, RoleBase* r);

    protected:
        json read_file(const fs::path& p, bool update = false) const;

        void check_role_signatures(const json& data, const RoleBase& role);

        void set_spec_version(std::unique_ptr<SpecVersionBase> sv);

        void set_expiration(const std::string& expires);

        std::string canonicalize(const json& j) const;

    private:
        std::string m_internal_type;
        std::string m_type;
        std::unique_ptr<SpecVersionBase> p_spec_version;
        std::size_t m_version = 1;
        std::string m_expires;
        std::string m_ext = "json";

        void check_signatures(const std::string& signed_data,
                              const std::set<RoleSignature>& signatures,
                              const RoleFullKeys& keyring);
    };

    class RootRoleBase : public RoleBase
    {
    public:
        std::unique_ptr<RootRoleBase> update(fs::path path);
        std::unique_ptr<RootRoleBase> update(json j);

        // Set of valid top-level roles (incl. 'root')
        virtual std::set<std::string> roles() const = 0;

        // Map of full-keys for valid top-level roles (incl. 'root')
        virtual std::map<std::string, RoleFullKeys> all_keys() const = 0;

    protected:
        RootRoleBase(std::unique_ptr<SpecVersionBase> sv);

    private:
        virtual std::unique_ptr<RootRoleBase> create_update(const json& j) = 0;
    };

    /*
    class RepoTrust
    {
    public:
        RepoTrust(const std::string& url,
                  const fs::path& local_trusted_root,
                  const SpecVersion& spec_version);

    private:
        std::string m_base_url;
        std::unique_ptr<RootRoleBase> p_root;
    };
    */

    void to_json(json& j, const Key& k);
    void to_json(json& j, const RoleKeys& rk);
    void to_json(json& j, const RolePubKeys& rk);
    void to_json(json& j, const RoleSignature& rs);

    void from_json(const json& j, Key& k);
    void from_json(const json& j, RoleKeys& rk);
    void from_json(const json& j, RolePubKeys& rk);
    void from_json(const json& j, RoleSignature& rs);

    namespace v1
    {
        class SpecVersion final : public SpecVersionBase
        {
        public:
            SpecVersion(const std::string& sv = "1.0.17");

            std::string json_key() const override;
            std::string expiration_json_key() const override;

            std::set<RoleSignature> signatures(const json& j) const override;
        };

        class RootRole final : public RootRoleBase
        {
        public:
            RootRole(const fs::path& p);
            RootRole(const json& j);

            std::set<std::string> roles() const override;
            RoleFullKeys self_keys() const override;
            std::map<std::string, RoleFullKeys> all_keys() const override;

            friend void to_json(json& j, const RootRole& r);
            friend void from_json(const json& j, RootRole& r);

        private:
            RootRole() = delete;

            void load_from_json(const json& j);

            std::unique_ptr<RootRoleBase> create_update(const json& j) override;

            std::map<std::string, Key> m_keys;
            std::map<std::string, RoleKeys> m_roles;
        };
    }

    namespace v06
    {
        std::string json_canonicalize(const json& version);

        class SpecVersion final : public SpecVersionBase
        {
        public:
            SpecVersion(const std::string& sv = "0.6.0");

            std::string json_key() const override;
            std::string expiration_json_key() const override;

            std::set<RoleSignature> signatures(const json& j) const override;

        protected:
            std::string canonicalize(const json& j) const override;
            bool upgradable() const override;
        };

        class RootRole final : public RootRoleBase
        {
        public:
            RootRole(const fs::path& p);
            RootRole(const json& j);

            std::set<std::string> roles() const override;
            RoleFullKeys self_keys() const override;
            std::map<std::string, RoleFullKeys> all_keys() const override;

            json upgraded_signable() const;
            RoleSignature upgraded_signature(const json& j,
                                             const std::string& pk,
                                             const unsigned char* sk) const;

            friend void to_json(json& j, const RootRole& r);
            friend void from_json(const json& j, RootRole& r);

        private:
            RootRole() = delete;

            void load_from_json(const json& j);

            std::unique_ptr<RootRoleBase> create_update(const json& j) override;

            std::map<std::string, RolePubKeys> m_delegations;
        };


        class KeyMgrRole final : public RoleBase
        {
        public:
            KeyMgrRole(const fs::path& p, const RoleFullKeys& keys);
            KeyMgrRole(const json& j, const RoleFullKeys& keys);

            // std::set<std::string> roles() const override;
            RoleFullKeys self_keys() const override;


            friend void to_json(json& j, const KeyMgrRole& r);
            friend void from_json(const json& j, KeyMgrRole& r);

        private:
            KeyMgrRole() = delete;

            void load_from_json(const json& j);

            RoleFullKeys m_keys;
            std::map<std::string, RolePubKeys> m_delegations;
        };
    }
}  // namespace validate

#endif  // MAMBA_VALIDATE_HPP
