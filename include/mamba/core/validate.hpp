// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_VALIDATE_HPP
#define MAMBA_CORE_VALIDATE_HPP

#include "mamba_fs.hpp"

#include <nlohmann/json.hpp>

#include <string>
#include <vector>
#include <stdexcept>


namespace validate
{
    using nlohmann::json;

    std::string sha256sum(const std::string& path);
    std::string md5sum(const std::string& path);
    bool sha256(const std::string& path, const std::string& validation);
    bool md5(const std::string& path, const std::string& validation);
    bool file_size(const fs::path& path, std::uintmax_t validation);

    int hex2bin(unsigned char* const bin,
                const size_t bin_maxlen,
                const char* const hex,
                const size_t hex_len,
                const char* const ignore,
                size_t* const bin_len,
                const char** const hex_end);
    char* bin2hex(char* const hex,
                  const size_t hex_maxlen,
                  const unsigned char* const bin,
                  const size_t bin_len);

    const std::size_t MAMBA_SHA256_SIZE_HEX = 64;
    const std::size_t MAMBA_SHA256_SIZE_BYTES = 32;
    const std::size_t MAMBA_ED25519_KEYSIZE_HEX = 64;
    const std::size_t MAMBA_ED25519_KEYSIZE_BYTES = 32;
    const std::size_t MAMBA_ED25519_SIGSIZE_HEX = 128;
    const std::size_t MAMBA_ED25519_SIGSIZE_BYTES = 64;

    int generate_ed25519_keypair(unsigned char* pk, unsigned char* sk);

    int sign(const std::string& data, const unsigned char* sk, unsigned char* signature);

    int verify(const unsigned char* data,
               std::size_t data_len,
               unsigned char* pk,
               unsigned char* signature);
    int verify(const std::string& data, unsigned char* pk, unsigned char* signature);
    int verify(const std::string& data, const std::string& pk, const std::string& signature);

    int verify_gpg_hashed_msg(const std::string& data, unsigned char* pk, unsigned char* signature);
    int verify_gpg_hashed_msg(const std::string& data,
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

    struct GPGKey : public Key
    {
        std::string other_headers = "";
    };

    struct RoleSignature
    {
        std::string keyid = "";
        std::string sig = "";
    };

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

    struct RoleKeys
    {
        std::vector<std::string> keyids;
        std::size_t threshold;
    };

    class RoleBase
    {
    public:
        RoleBase() = default;
        RoleBase(const std::string& type)
            : m_type(type){};

        virtual ~RoleBase() = 0;

        virtual void update(fs::path path) = 0;
        virtual void update(json j) = 0;

        std::string name() const;
        std::string type() const;
        std::string spec_version() const;
        std::size_t version() const;
        std::string filename() const;

        void set_spec_version(const std::string& spec_version);
        void set_version(const std::size_t& version);

    private:
        std::string m_type;
        std::size_t m_version;
        std::string m_spec_version;
        std::string m_expires;
        std::string m_ext = ".json";
    };

    class RootRole : public RoleBase
    {
    public:
        RootRole();
        RootRole(const fs::path& p);
        RootRole(const json& j);

        void update(fs::path path);
        void update(json j);

        std::map<std::string, Key>& keys();
        const std::map<std::string, Key>& keys() const;
        std::map<std::string, RoleKeys>& roles();
        const std::map<std::string, RoleKeys>& roles() const;

        void set_keys(const std::map<std::string, Key>& keys);
        void set_roles(const std::map<std::string, RoleKeys>& roles);

    private:
        void check_role_signatures(const std::string& signed_data,
                                   const std::vector<RoleSignature>& signatures,
                                   const RootRole& role);

        void check_signatures(const std::string& signed_data,
                              const std::vector<RoleSignature>& signatures,
                              const std::map<std::string, Key>& accepted_keys,
                              const int threshold);

        std::map<std::string, Key> m_keys;
        std::map<std::string, RoleKeys> m_roles;
    };

    void to_json(json& j, const Key& k);
    void to_json(json& j, const RoleKeys& rk);
    void to_json(json& j, const RoleSignature& rs);
    void to_json_base(json& j, const RoleBase& r);
    void to_json(json& j, const RootRole& r);

    void from_json(const json& j, Key& k);
    void from_json(const json& j, RoleKeys& rk);
    void from_json(const json& j, RoleSignature& rs);
    void from_json_base(const json& j, RoleBase& r);
    void from_json(const json& j, RootRole& r);
}  // namespace validate

#endif  // MAMBA_VALIDATE_HPP
