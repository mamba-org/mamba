// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/core/validate.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/util.hpp"

#include "openssl/md5.h"
#include "openssl/sha.h"
#include "openssl/evp.h"

#include <vector>
#include <stdexcept>
#include <iostream>
#include <string>
#include <set>


namespace validate
{
    trust_error::trust_error(const std::string& message) noexcept
        : m_message(message)
    {
        std::cout << this->m_message << std::endl;
    }

    const char* trust_error::what() const noexcept
    {
        return this->m_message.c_str();
    }

    threshold_error::threshold_error() noexcept
        : trust_error("Signatures threshold not met. Aborting.")
    {
    }

    role_metadata_error::role_metadata_error() noexcept
        : trust_error("Invalid role metadata. Aborting.")
    {
    }

    rollback_error::rollback_error() noexcept
        : trust_error("Possible rollback attack. Aborting.")
    {
    }

    role_file_error::role_file_error() noexcept
        : trust_error("Invalid role file. Aborting.")
    {
    }

    std::string sha256sum(const std::string& path)
    {
        std::array<unsigned char, SHA256_DIGEST_LENGTH> hash;

        SHA256_CTX sha256;
        SHA256_Init(&sha256);

        std::ifstream infile(path, std::ios::binary);

        constexpr std::size_t BUFSIZE = 32768;
        std::vector<char> buffer(BUFSIZE);

        while (infile)
        {
            infile.read(buffer.data(), BUFSIZE);
            size_t count = infile.gcount();
            if (!count)
                break;
            SHA256_Update(&sha256, buffer.data(), count);
        }

        SHA256_Final(hash.data(), &sha256);

        return ::mamba::hex_string(hash);
    }

    std::string md5sum(const std::string& path)
    {
        std::array<unsigned char, MD5_DIGEST_LENGTH> hash;

        MD5_CTX md5;
        MD5_Init(&md5);

        std::ifstream infile(path, std::ios::binary);

        constexpr std::size_t BUFSIZE = 32768;
        std::vector<char> buffer(BUFSIZE);

        while (infile)
        {
            infile.read(buffer.data(), BUFSIZE);
            size_t count = infile.gcount();
            if (!count)
                break;
            MD5_Update(&md5, buffer.data(), count);
        }

        MD5_Final(hash.data(), &md5);

        return ::mamba::hex_string(hash);
    }

    bool sha256(const std::string& path, const std::string& validation)
    {
        return sha256sum(path) == validation;
    }

    bool md5(const std::string& path, const std::string& validation)
    {
        return md5sum(path) == validation;
    }

    bool file_size(const fs::path& path, std::uintmax_t validation)
    {
        return fs::file_size(path) == validation;
    }

    int generate_ed25519_keypair(unsigned char* pk, unsigned char* sk)
    {
        std::size_t key_len = MAMBA_ED25519_KEYSIZE_BYTES;
        EVP_PKEY* pkey = NULL;
        EVP_PKEY_CTX* pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, NULL);

        int gen_status;
        gen_status = EVP_PKEY_keygen_init(pctx);
        if (gen_status != 1)
        {
            LOG_DEBUG << "Failed to initialize ED25519 key pair generation";
            return gen_status;
        }

        gen_status = EVP_PKEY_keygen(pctx, &pkey);
        if (gen_status != 1)
        {
            LOG_DEBUG << "Failed to generate ED25519 key pair";
            return gen_status;
        }

        int storage_status;
        storage_status = EVP_PKEY_get_raw_public_key(pkey, pk, &key_len);
        if (storage_status != 1)
        {
            LOG_DEBUG << "Failed to store public key of generated ED25519 key pair";
            return storage_status;
        }
        storage_status = EVP_PKEY_get_raw_private_key(pkey, sk, &key_len);
        if (storage_status != 1)
        {
            LOG_DEBUG << "Failed to store private key of generated ED25519 key pair";
            return storage_status;
        }

        EVP_PKEY_CTX_free(pctx);
        return 1;
    }

    int sign(const std::string& data, const unsigned char* sk, unsigned char* signature)
    {
        std::size_t msg_len = data.size();
        std::size_t sig_len = MAMBA_ED25519_SIGSIZE_BYTES;
        auto msg = (const unsigned char*) data.c_str();

        EVP_PKEY* ed_key
            = EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, NULL, sk, MAMBA_ED25519_KEYSIZE_BYTES);
        EVP_MD_CTX* md_ctx = EVP_MD_CTX_new();

        if (ed_key == nullptr)
        {
            LOG_DEBUG << "Failed to read secret key raw buffer during signing step";
            return 0;
        }

        int init_status, sign_status;
        init_status = EVP_DigestSignInit(md_ctx, NULL, NULL, NULL, ed_key);
        if (init_status != 1)
        {
            LOG_DEBUG << "Failed to init signing step";
            return init_status;
        }

        sign_status = EVP_DigestSign(md_ctx, signature, &sig_len, msg, msg_len);
        if (sign_status != 1)
        {
            LOG_DEBUG << "Failed to sign the data";
            return sign_status;
        }

        EVP_MD_CTX_free(md_ctx);
        return 1;
    }

    int verify(const unsigned char* data,
               std::size_t data_len,
               unsigned char* pk,
               unsigned char* signature)
    {
        std::size_t sig_len = MAMBA_ED25519_SIGSIZE_BYTES;

        EVP_PKEY* ed_key
            = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, NULL, pk, MAMBA_ED25519_KEYSIZE_BYTES);
        EVP_MD_CTX* md_ctx = EVP_MD_CTX_new();

        if (ed_key == nullptr)
        {
            LOG_DEBUG << "Failed to read public key raw buffer during verification step";
            return 0;
        }

        int init_status, verif_status;
        init_status = EVP_DigestVerifyInit(md_ctx, NULL, NULL, NULL, ed_key);
        if (init_status != 1)
        {
            LOG_DEBUG << "Failed to init verification step";
            return init_status;
        }

        verif_status = EVP_DigestVerify(md_ctx, signature, sig_len, data, data_len);
        if (verif_status != 1)
        {
            LOG_DEBUG << "Failed to verify the data signature";
            return verif_status;
        }

        EVP_MD_CTX_free(md_ctx);
        return 1;
    }

    int verify(const std::string& data, unsigned char* pk, unsigned char* signature)
    {
        unsigned long long data_len = data.size();
        auto raw_data = (const unsigned char*) data.c_str();

        return verify(raw_data, data_len, pk, signature);
    }

    int verify(const std::string& data, const std::string& pk, const std::string& signature)
    {
        unsigned char bin_signature[MAMBA_ED25519_SIGSIZE_BYTES];
        int ret1 = hex2bin(bin_signature,
                           MAMBA_ED25519_SIGSIZE_BYTES,
                           signature.c_str(),
                           MAMBA_ED25519_SIGSIZE_HEX,
                           NULL,
                           NULL,
                           NULL);

        unsigned char bin_pk[MAMBA_ED25519_KEYSIZE_BYTES];
        int ret2 = hex2bin(bin_pk,
                           MAMBA_ED25519_KEYSIZE_BYTES,
                           pk.c_str(),
                           MAMBA_ED25519_KEYSIZE_HEX,
                           NULL,
                           NULL,
                           NULL);

        if ((ret1 != 0) || (ret2 != 0))
        {
            throw std::runtime_error("Conversions from hex to bin format failed.");
        }

        return verify(data, bin_pk, bin_signature);
    }

    int verify_gpg_hashed_msg(const std::string& data, unsigned char* pk, unsigned char* signature)
    {
        unsigned char msg_bin[MAMBA_SHA256_SIZE_BYTES];
        hex2bin(msg_bin,
                MAMBA_SHA256_SIZE_BYTES,
                data.c_str(),
                MAMBA_SHA256_SIZE_HEX,
                NULL,
                NULL,
                NULL);

        return verify(msg_bin, MAMBA_SHA256_SIZE_BYTES, pk, signature);
    }

    int verify_gpg_hashed_msg(const std::string& data,
                              const std::string& pk,
                              const std::string& signature)
    {
        unsigned char bin_signature[MAMBA_ED25519_SIGSIZE_BYTES];
        hex2bin(bin_signature,
                MAMBA_ED25519_SIGSIZE_BYTES,
                signature.c_str(),
                MAMBA_ED25519_SIGSIZE_HEX,
                NULL,
                NULL,
                NULL);

        unsigned char bin_pk[MAMBA_ED25519_KEYSIZE_BYTES];
        hex2bin(bin_pk,
                MAMBA_ED25519_KEYSIZE_BYTES,
                pk.c_str(),
                MAMBA_ED25519_KEYSIZE_HEX,
                NULL,
                NULL,
                NULL);

        return verify_gpg_hashed_msg(data, bin_pk, bin_signature);
    }

    /**
     * Binary to hexadecimal converter.
     * from https://github.com/jedisct1/libsodium
     */
    char* bin2hex(char* const hex,
                  const size_t hex_maxlen,
                  const unsigned char* const bin,
                  const size_t bin_len)
    {
        size_t i = (size_t) 0U;
        unsigned int x;
        int b;
        int c;

        if (bin_len >= SIZE_MAX / 2 || hex_maxlen < bin_len * 2U)
        {
            throw std::runtime_error("Invalid size for binary to hexadecimal conversion.");
        }
        while (i < bin_len)
        {
            c = bin[i] & 0xf;
            b = bin[i] >> 4;
            x = (unsigned char) (87U + c + (((c - 10U) >> 8) & ~38U)) << 8
                | (unsigned char) (87U + b + (((b - 10U) >> 8) & ~38U));
            hex[i * 2U] = (char) x;
            x >>= 8;
            hex[i * 2U + 1U] = (char) x;
            i++;
        }
        if (hex_maxlen > bin_len * 2U)
        {
            hex[i * 2U] = 0U;
        }

        return hex;
    }

    /**
     * Hexadecimal to decimal converter.
     * from https://github.com/jedisct1/libsodium
     */
    int hex2bin(unsigned char* const bin,
                const size_t bin_maxlen,
                const char* const hex,
                const size_t hex_len,
                const char* const ignore,
                size_t* const bin_len,
                const char** const hex_end)
    {
        size_t bin_pos = (size_t) 0U;
        size_t hex_pos = (size_t) 0U;
        int ret = 0;
        unsigned char c;
        unsigned char c_acc = 0U;
        unsigned char c_alpha0, c_alpha;
        unsigned char c_num0, c_num;
        unsigned char c_val;
        unsigned char state = 0U;

        while (hex_pos < hex_len)
        {
            c = (unsigned char) hex[hex_pos];
            c_num = c ^ 48U;
            c_num0 = (c_num - 10U) >> 8;
            c_alpha = (c & ~32U) - 55U;
            c_alpha0 = ((c_alpha - 10U) ^ (c_alpha - 16U)) >> 8;
            if ((c_num0 | c_alpha0) == 0U)
            {
                if (ignore != NULL && state == 0U && strchr(ignore, c) != NULL)
                {
                    hex_pos++;
                    continue;
                }
                break;
            }
            c_val = (c_num0 & c_num) | (c_alpha0 & c_alpha);
            if (bin_pos >= bin_maxlen)
            {
                ret = -1;
                errno = ERANGE;
                break;
            }
            if (state == 0U)
            {
                c_acc = c_val * 16U;
            }
            else
            {
                bin[bin_pos++] = c_acc | c_val;
            }
            state = ~state;
            hex_pos++;
        }
        if (state != 0U)
        {
            hex_pos--;
            errno = EINVAL;
            ret = -1;
        }
        if (ret != 0)
        {
            bin_pos = (size_t) 0U;
        }
        if (hex_end != NULL)
        {
            *hex_end = &hex[hex_pos];
        }
        else if (hex_pos != hex_len)
        {
            errno = EINVAL;
            ret = -1;
        }
        if (bin_len != NULL)
        {
            *bin_len = bin_pos;
        }
        return ret;
    }

    RoleBase::~RoleBase(){};

    std::string RoleBase::name() const
    {
        return m_type;
    };
    std::string RoleBase::type() const
    {
        return m_type;
    };
    std::string RoleBase::spec_version() const
    {
        return m_spec_version;
    };
    std::size_t RoleBase::version() const
    {
        return m_version;
    };
    std::string RoleBase::filename() const
    {
        return m_type + m_ext;
    };

    void RoleBase::set_spec_version(const std::string& spec_version)
    {
        m_spec_version = spec_version;
    };

    void RoleBase::set_version(const std::size_t& version)
    {
        m_version = version;
    };

    RootRole::RootRole()
        : RoleBase("root")
    {
        set_version(1);
    };

    RootRole::RootRole(const json& j)
        : RoleBase("root")
    {
        from_json(j, *this);
    };

    RootRole::RootRole(const fs::path& path)
        : RoleBase("root")
    {
        using namespace mamba;

        if (!fs::exists(path))
        {
            LOG_ERROR << "'root' metadata file not found at: " << path.string();
            throw role_file_error();
        }

        if (!ends_with(path.filename().string(), filename()))
        {
            LOG_ERROR << "'root' metadata file name does not end with '" << filename()
                      << "': " << path.filename().string();
            throw role_file_error();
        }

        std::ifstream i(path);
        json j;
        i >> j;
        from_json(j["signed"], *this);
    };

    void RootRole::update(fs::path path)
    {
        using namespace mamba;

        if (!fs::exists(path))
        {
            LOG_ERROR << "File not found for 'root' update: " << path.string();
            throw role_file_error();
        }

        // Update file should be named as VERSION_NUMBER.FILENAME.EXT
        // TODO: use regex here?
        if (!ends_with(path.filename().string(), filename()))
        {
            LOG_ERROR << "'root' metadata file name does not end with '" << filename()
                      << "': " << path.filename().string();
            throw role_file_error();
        }
        // Check version number in filename is N+1
        unsigned long new_version;
        auto split_filename = split(path.filename().string(), ".", 1);
        try
        {
            new_version = std::stoul(split_filename[0]);
        }
        catch (...)
        {
            LOG_ERROR << "Invalid version in file name for 'root' update: "
                      << path.filename().string();
            throw role_file_error();
        }
        if (new_version != (version() + 1))
        {
            LOG_ERROR << "'root' metadata file name does not start with '" << version() + 1
                      << "': " << filename();
            throw role_file_error();
        }

        std::ifstream i(path);
        json j;
        i >> j;

        update(j);
    }

    void RootRole::update(json j)
    {
        RootRole new_role(j["signed"]);
        auto new_signatures = j["signatures"].get<std::vector<RoleSignature>>();
        std::string signed_data = j["signed"].dump();

        // TUF spec 5.3.4 - Check for an arbitrary software attack
        // Check signatures against current keyids and threshold
        check_role_signatures(signed_data, new_signatures, *this);
        // Check signatures against new keyids and threshold
        check_role_signatures(signed_data, new_signatures, new_role);

        // TUF spec 5.3.5 - Check for a rollback attack
        // Version number has to be N+1
        if (new_role.version() != (version() + 1))
        {
            if (new_role.version() > (version() + 1))
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

        from_json(j["signed"], *this);
    }


    void RootRole::check_role_signatures(const std::string& signed_data,
                                         const std::vector<RoleSignature>& signatures,
                                         const RootRole& role)
    {
        std::map<std::string, Key> accepted_keys;
        auto& threshold = role.roles().at("root").threshold;

        for (auto& keyid : roles().at("root").keyids)
        {
            accepted_keys.insert({ keyid, keys().at(keyid) });
        }

        check_signatures(signed_data, signatures, accepted_keys, threshold);
    }

    void RootRole::check_signatures(const std::string& signed_data,
                                    const std::vector<RoleSignature>& signatures,
                                    const std::map<std::string, Key>& accepted_keys,
                                    const int threshold)
    {
        int valid_sig = 0;

        for (auto& s : signatures)
        {
            auto it = accepted_keys.find(s.keyid);
            if (it != accepted_keys.end())
            {
                auto& pk = it->second.keyval;
                if (verify(signed_data, pk, s.sig) == 1)
                {
                    ++valid_sig;
                }
                else
                {
                    LOG_WARNING << "Invalid signature of 'root' metadata for keyid: " << s.keyid;
                }
            }
            else
            {
                LOG_WARNING << "Invalid keyid: " << s.keyid;
            }
            if (valid_sig >= threshold)
                break;
        }

        if (valid_sig < threshold)
        {
            LOG_ERROR << "Threshold of valid signatures defined in 'root' metadata is not met ("
                      << valid_sig << "/" << threshold << ")";
            throw threshold_error();
        }
    }

    const std::map<std::string, Key>& RootRole::keys() const
    {
        return m_keys;
    }

    std::map<std::string, Key>& RootRole::keys()
    {
        return m_keys;
    }

    const std::map<std::string, RoleKeys>& RootRole::roles() const
    {
        return m_roles;
    }

    std::map<std::string, RoleKeys>& RootRole::roles()
    {
        return m_roles;
    }

    void RootRole::set_keys(const std::map<std::string, Key>& keys)
    {
        m_keys = keys;
    }

    void RootRole::set_roles(const std::map<std::string, RoleKeys>& roles)
    {
        m_roles = roles;
    }

    void to_json(json& j, const Key& key)
    {
        j = json{ { "keytype", key.keytype }, { "scheme", key.scheme }, { "keyval", key.keyval } };
    }

    void to_json(json& j, const RoleKeys& role_keys)
    {
        j = json{ { "keyids", role_keys.keyids }, { "threshold", role_keys.threshold } };
    }

    void to_json(json& j, const RoleSignature& role_sig)
    {
        j = json{ { "keyid", role_sig.keyid }, { "sig", role_sig.sig } };
    }

    void to_json_base(json& j, const RoleBase& role)
    {
        j = json{ { "_type", role.name() },
                  { "spec_version", role.spec_version() },
                  { "version", role.version() } };
    }

    void to_json(json& j, const RootRole& role)
    {
        to_json_base(j, role);
        j["keys"] = role.keys();
        j["roles"] = role.roles();
    }

    void from_json(const json& j, Key& key)
    {
        j.at("keytype").get_to(key.keytype);
        j.at("scheme").get_to(key.scheme);
        j.at("keyval").get_to(key.keyval);
    }

    void from_json(const json& j, RoleKeys& role_keys)
    {
        j.at("keyids").get_to(role_keys.keyids);
        j.at("threshold").get_to(role_keys.threshold);
    }

    void from_json(const json& j, RoleSignature& role_sig)
    {
        j.at("keyid").get_to(role_sig.keyid);
        j.at("sig").get_to(role_sig.sig);
    }

    void from_json_base(const json& j, RoleBase& role)
    {
        role.set_spec_version(j.at("spec_version"));
        role.set_version(j.at("version"));
    }

    void from_json(const json& j, RootRole& role)
    {
        std::string meta_type;
        try
        {
            meta_type = j.at("_type");
        }
        catch (...)
        {
            LOG_ERROR << "'_type' not found in 'root' metadata";
            throw role_metadata_error();
        }
        if (meta_type != role.type())
        {
            LOG_ERROR << "Wrong '_type' found in 'root' metadata, should be 'root': '"
                      << j.at("_type") << "'";
            throw role_metadata_error();
        }

        try
        {
            from_json_base(j, role);
            role.set_keys(j.at("keys").get<std::map<std::string, Key>>());
            role.set_roles(j.at("roles").get<std::map<std::string, RoleKeys>>());
        }
        catch (const json::exception& e)
        {
            LOG_ERROR << "Error while loading 'root' metadata from JSON file: " << e.what();
            throw role_metadata_error();
        }
        catch (...)
        {
            LOG_ERROR << "Error while loading 'root' metadata from JSON file";
            throw role_metadata_error();
        }

        // Did not managed to use Role enum as map key
        // TODO: replace those checks with JSON schema?
        for (const auto& r : role.roles())
        {
            json jr = r.first;
            auto role = jr.get<Role>();
            if (role == Role::kInvalid)
            {
                LOG_ERROR << "Invalid role while loading 'root' metadata from JSON file: '"
                          << r.first << "'";
                throw role_metadata_error();
            }
        }

        std::set<std::string> current_roles;
        for (const auto& r : role.roles())
        {
            current_roles.insert(r.first);
        }
        std::set<std::string> mandatory_roles = { "root", "snapshot", "targets", "timestamp" };
        if (!std::includes(current_roles.begin(),
                           current_roles.end(),
                           mandatory_roles.begin(),
                           mandatory_roles.end()))
        {
            std::vector<std::string> diff;
            std::set_difference(mandatory_roles.begin(),
                                mandatory_roles.end(),
                                current_roles.begin(),
                                current_roles.end(),
                                std::inserter(diff, diff.end()));
            LOG_ERROR << "Missing roles while loading 'root' metdata: " << mamba::join(", ", diff);
            throw role_metadata_error();
        }
    }
}  // namespace validate
