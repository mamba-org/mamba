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
#include <regex>


namespace validate
{
    trust_error::trust_error(const std::string& message) noexcept
        : m_message("Content trust error. " + message + ". Aborting.")
    {
        std::cout << this->m_message << std::endl;
    }

    const char* trust_error::what() const noexcept
    {
        return this->m_message.c_str();
    }

    threshold_error::threshold_error() noexcept
        : trust_error("Signatures threshold not met")
    {
    }

    role_metadata_error::role_metadata_error() noexcept
        : trust_error("Invalid role metadata")
    {
    }

    rollback_error::rollback_error() noexcept
        : trust_error("Possible rollback attack")
    {
    }

    role_file_error::role_file_error() noexcept
        : trust_error("Invalid role file")
    {
    }

    spec_version_error::spec_version_error() noexcept
        : trust_error("Unsupported specification version")
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

    std::array<unsigned char, MAMBA_ED25519_SIGSIZE_BYTES> ed25519_sig_hex_to_bytes(
        const std::string& sig_hex) noexcept

    {
        return ::mamba::hex_to_bytes<MAMBA_ED25519_SIGSIZE_BYTES>(sig_hex);
    }

    std::array<unsigned char, MAMBA_ED25519_SIGSIZE_BYTES> ed25519_sig_hex_to_bytes(
        const std::string& sig_hex, int& error_code) noexcept

    {
        return ::mamba::hex_to_bytes<MAMBA_ED25519_SIGSIZE_BYTES>(sig_hex, error_code);
    }

    std::array<unsigned char, MAMBA_ED25519_KEYSIZE_BYTES> ed25519_key_hex_to_bytes(
        const std::string& key_hex) noexcept

    {
        return ::mamba::hex_to_bytes<MAMBA_ED25519_KEYSIZE_BYTES>(key_hex);
    }

    std::array<unsigned char, MAMBA_ED25519_KEYSIZE_BYTES> ed25519_key_hex_to_bytes(
        const std::string& key_hex, int& error_code) noexcept

    {
        return ::mamba::hex_to_bytes<MAMBA_ED25519_KEYSIZE_BYTES>(key_hex, error_code);
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
               const unsigned char* pk,
               const unsigned char* signature)
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

    int verify(const std::string& data, const unsigned char* pk, const unsigned char* signature)
    {
        unsigned long long data_len = data.size();
        auto raw_data = (const unsigned char*) data.c_str();

        return verify(raw_data, data_len, pk, signature);
    }

    int verify(const std::string& data, const std::string& pk, const std::string& signature)
    {
        int error_code = 0;
        auto bin_signature = ed25519_sig_hex_to_bytes(signature, error_code);
        if (error_code != 0)
        {
            LOG_DEBUG << "Invalid signature '" << signature << "' for public key '" << pk << "'";
            return 0;
        }

        auto bin_pk = ed25519_key_hex_to_bytes(pk, error_code);
        if (error_code != 0)
        {
            LOG_DEBUG << "Invalid public key '" << pk << "'";
            return 0;
        }

        return verify(data, bin_pk.data(), bin_signature.data());
    }

    int verify_gpg_hashed_msg(const unsigned char* data,
                              const unsigned char* pk,
                              const unsigned char* signature)
    {
        return verify(data, MAMBA_SHA256_SIZE_BYTES, pk, signature);
    }


    int verify_gpg_hashed_msg(const std::string& data,
                              const unsigned char* pk,
                              const unsigned char* signature)
    {
        auto data_bin = ::mamba::hex_to_bytes<MAMBA_SHA256_SIZE_BYTES>(data);

        return verify(data_bin.data(), MAMBA_SHA256_SIZE_BYTES, pk, signature);
    }

    int verify_gpg_hashed_msg(const std::string& data,
                              const std::string& pk,
                              const std::string& signature)
    {
        auto signature_bin = ed25519_sig_hex_to_bytes(signature);
        auto pk_bin = ed25519_key_hex_to_bytes(pk);

        return verify_gpg_hashed_msg(data, pk_bin.data(), signature_bin.data());
    }

    int verify_gpg(const std::string& data,
                   const std::string& pgp_v4_trailer,
                   const std::string& pk,
                   const std::string& signature)
    {
        unsigned long long data_len = data.size();
        auto data_bin = (const unsigned char*) data.c_str();

        auto signature_bin = ed25519_sig_hex_to_bytes(signature);
        auto pk_bin = ed25519_key_hex_to_bytes(pk);

        std::size_t trailer_hex_size = pgp_v4_trailer.size();
        if (trailer_hex_size % 2 != 0)
        {
            LOG_DEBUG << "PGP V4 trailer size is not even: " << pgp_v4_trailer;
            return 0;
        }

        auto pgp_trailer_bin = ::mamba::hex_to_bytes(pgp_v4_trailer);
        auto final_trailer_bin = ::mamba::hex_to_bytes<2>(std::string("04ff"));

        uint32_t trailer_bin_len_big_endian = pgp_trailer_bin.size();

#ifdef _WIN32
        trailer_bin_len_big_endian = _byteswap_ulong(trailer_bin_len_big_endian);
#else
        trailer_bin_len_big_endian = __builtin_bswap32(trailer_bin_len_big_endian);
#endif

        std::array<unsigned char, SHA256_DIGEST_LENGTH> hash;

        SHA256_CTX sha256;
        SHA256_Init(&sha256);
        SHA256_Update(&sha256, data_bin, data_len);
        SHA256_Update(&sha256, pgp_trailer_bin.data(), pgp_trailer_bin.size());
        SHA256_Update(&sha256, final_trailer_bin.data(), final_trailer_bin.size());
        SHA256_Update(&sha256, (unsigned char*) &trailer_bin_len_big_endian, 4);
        SHA256_Final(hash.data(), &sha256);

        return verify_gpg_hashed_msg(hash.data(), pk_bin.data(), signature_bin.data());
    }

    bool operator<(const RoleSignature& rs1, const RoleSignature& rs2)
    {
        return rs1.keyid < rs2.keyid;
    };

    RoleKeys RolePubKeys::to_role_keys() const
    {
        return { pubkeys, threshold };
    }

    RoleBase::~RoleBase(){};

    std::string RoleBase::type() const
    {
        return m_type;
    }

    std::string RoleBase::spec_version() const
    {
        return m_spec_version;
    }

    std::size_t RoleBase::version() const
    {
        return m_version;
    }

    std::string RoleBase::file_ext() const
    {
        return m_ext;
    }

    bool RoleBase::is_spec_version_compatible(const std::string& version) const
    {
        using namespace mamba;

        auto spec_version_major = std::stoi(split(m_spec_version, ".", 1)[0]);
        auto new_spec_version_major = std::stoi(split(version, ".", 1)[0]);
        return new_spec_version_major == spec_version_major;
    }

    bool RoleBase::is_spec_version_upgradable(const std::string& version) const
    {
        using namespace mamba;

        auto spec_version_major = std::stoi(split(m_spec_version, ".", 1)[0]);
        auto new_spec_version_major = std::stoi(split(version, ".", 1)[0]);
        return new_spec_version_major == (spec_version_major + 1);
    }

    SpecVersion RoleBase::major_spec_version() const
    {
        using namespace mamba;

        auto spec_version_major = std::stoi(split(m_spec_version, ".", 1)[0]);
        if (spec_version_major == 0)
        {
            return SpecVersion::kV06;
        }
        else if (spec_version_major == 1)
        {
            return SpecVersion::kV1;
        }
        else
        {
            LOG_ERROR << "Unsupported spec version: '" << m_spec_version << "'";
            throw role_metadata_error();
        }
    }

    void RoleBase::set_spec_version(const std::string& version)
    {
        m_spec_version = version;
    }

    std::string RoleBase::canonicalize(const json& j) const
    {
        return j.dump();
    }

    bool RoleBase::upgradable() const
    {
        return false;
    };

    json RoleBase::read_file(const fs::path& p, bool update) const
    {
        if (!fs::exists(p))
        {
            LOG_ERROR << "File not found for 'root' update: " << p.string();
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
            name_re = "^(?:([1-9]+\\d*).)(?:sv([1-9]\\d*|0\\.[1-9]\\d*).)?(\\w+)\\.(\\w+)$";
            min_match_size = 4;
        }
        else
        {
            name_re = "^(?:[1-9]+\\d*.)?(?:sv([1-9]\\d*|0\\.[1-9]\\d*).)?(\\w+)\\.(\\w+)$";
            min_match_size = 3;
        }

        if (std::regex_search(f_name, matches, name_re) && (min_match_size <= matches.size()))
        {
            auto match_size = matches.size();

            if (update)
                f_version_str = matches[1].str();

            f_type = matches[match_size - 2].str();
            f_ext = matches[match_size - 1].str();

            if ((min_match_size + 1) == match_size)
                f_spec_version_str = matches[match_size - 3].str();
        }
        else
        {
            LOG_ERROR << "Invalid file name for 'root' metadata update: " << f_name;
            throw role_file_error();
        }

        if (f_ext != file_ext())
        {
            LOG_ERROR << "'root' metadata file should have 'json' extension, not: '" << f_ext
                      << "'";
            throw role_file_error();
        }
        if (f_type != type())
        {
            LOG_ERROR << "'root' metadata file should have 'root' type, not: '" << f_type << "'";
            throw role_file_error();
        }
        if (!f_spec_version_str.empty())
        {
            if (update && is_spec_version_upgradable(f_spec_version_str) && !upgradable())
            {
                LOG_ERROR << "Please check for a client update, unsupported spec version: '"
                          << f_spec_version_str << "'";
                throw spec_version_error();
            }
            else if (!((!update && is_spec_version_compatible(f_spec_version_str))
                       || (update && is_spec_version_upgradable(f_spec_version_str))))
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

        std::ifstream i(p);
        json j;
        i >> j;

        return j;
    }

    RootRoleBase::RootRoleBase(const std::string& spec_version)
        : RoleBase("root", spec_version)
    {
    }

    std::unique_ptr<RootRoleBase> RootRoleBase::update(fs::path path)
    {
        auto j = read_file(path, true);
        return update(j);
    }

    // `create_update` currently catch a possible spec version update by testing
    // and extracting spec version from JSON. It could be done upstream (in
    // `update(fs::path)`) if we decide to specify the spec version in the file name.
    // The filename would take the form VERSION_NUMBER.SPECVERSION.FILENAME.EXT
    // To disambiguate version and spec version: 1.sv0.6.root.json or 1.sv1.root.json
    std::unique_ptr<RootRoleBase> RootRoleBase::update(json j)
    {
        // TUF spec 5.3.4 - Check for an arbitrary software attack
        // Check signatures against current keyids and threshold in 'RootRole' constructor
        auto root_update = create_update(j);

        // Check signatures against new keyids and threshold
        check_role_signatures(j, *root_update);

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

    void RootRoleBase::check_role_signatures(const json& data, const RootRoleBase& role)
    {
        std::string signed_data = role.canonicalize(data["signed"]);
        auto signatures = role.signatures(data);

        auto k = keys();
        RoleFullKeys root_keys = k.at("root");

        check_signatures(signed_data, signatures, root_keys);
    }

    void RootRoleBase::check_signatures(const std::string& signed_data,
                                        const std::set<RoleSignature>& signatures,
                                        const RoleFullKeys& keyring)
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
                    LOG_WARNING << "Invalid signature of 'root' metadata for keyid: " << s.keyid;
                }
            }
            else
            {
                LOG_WARNING << "Invalid keyid: " << s.keyid;
            }
            if (valid_sig >= keyring.threshold)
                break;
        }

        if (valid_sig < keyring.threshold)
        {
            LOG_ERROR << "Threshold of valid signatures defined in 'root' metadata is not met ("
                      << valid_sig << "/" << keyring.threshold << ")";
            throw threshold_error();
        }
    }

    namespace v1
    {
        RootRole::RootRole()
            : RootRoleBase("1.0.17")
        {
        }

        RootRole::RootRole(const json& j)
            : RootRoleBase("1.0.17")
        {
            load_from_json(j);
        }

        RootRole::RootRole(const fs::path& path)
            : RootRoleBase("1.0.17")
        {
            auto j = read_file(path);
            load_from_json(j);
        }

        std::unique_ptr<RootRoleBase> RootRole::create_update(const json& j)
        {
            if (v1::is_spec_version_compatible(j))
            {
                return std::make_unique<RootRole>(j);
            }
            else
            {
                LOG_ERROR << "Invalid spec version for 'root' update";
                throw spec_version_error();
            }
        }

        void RootRole::load_from_json(const json& j)
        {
            from_json(j, *this);

            // TUF spec 5.3.4 - Check for an arbitrary software attack
            // Check signatures against current keyids and threshold
            check_role_signatures(j, *this);
        }

        std::set<std::string> RootRole::roles() const
        {
            std::set<std::string> r;
            for (auto& it : m_roles)
            {
                r.insert(it.first);
            }
            return r;
        }

        std::set<RoleSignature> RootRole::signatures(const json& j) const
        {
            auto sigs = j.at("signatures").get<std::vector<RoleSignature>>();
            std::set<RoleSignature> unique_sigs(sigs.cbegin(), sigs.cend());

            return unique_sigs;
        }

        std::map<std::string, RoleFullKeys> RootRole::keys() const
        {
            std::map<std::string, RoleFullKeys> res;

            for (auto& it : m_roles)
            {
                std::map<std::string, Key> role_keys;
                for (auto& keyid : it.second.keyids)
                {
                    role_keys.insert({ keyid, m_keys.at(keyid) });
                }
                res.insert({ it.first, { role_keys, it.second.threshold } });
            }
            return res;
        }

        void to_json(json& j, const RootRole& r)
        {
            to_json(j, static_cast<const RoleBase*>(&r));
        }

        void from_json(const json& j, RootRole& role)
        {
            auto j_signed = j.at("signed");
            try
            {
                from_json(j_signed, static_cast<RoleBase*>(&role));

                auto type = j_signed.at("_type").get<std::string>();
                if (type != role.type())
                {
                    LOG_ERROR << "Wrong '_type' found in 'root' metadata, should be 'root': '"
                              << type << "'";
                    throw role_metadata_error();
                }

                if (!v1::is_spec_version_compatible(j))
                {
                    using namespace mamba;

                    auto spec_version_major = split(role.spec_version(), ".", 1)[0];

                    LOG_ERROR
                        << "Incompatible 'spec_version' found in 'root' metadata, should be '1' but is: '"
                        << spec_version_major << "'";
                    throw role_metadata_error();
                }
                role.set_spec_version(j_signed.at("spec_version").get<std::string>());

                role.m_keys = j_signed.at("keys").get<std::map<std::string, Key>>();
                role.m_roles = j_signed.at("roles").get<std::map<std::string, RoleKeys>>();
            }
            catch (const json::exception& e)
            {
                LOG_ERROR << "Invalid 'root' metadata: " << e.what();
                throw role_metadata_error();
            }

            // TODO: replace those checks with JSON schema?
            for (const auto& r : role.m_roles)
            {
                json json_role = r.first;
                auto role = json_role.get<Role>();
                if (role == Role::kInvalid)
                {
                    LOG_ERROR << "Invalid role while loading 'root' metadata from JSON file: '"
                              << r.first << "'";
                    throw role_metadata_error();
                }
            }

            std::set<std::string> current_roles;
            for (const auto& r : role.m_roles)
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
                LOG_ERROR << "Missing roles while loading 'root' metadata: "
                          << mamba::join(", ", diff);
                throw role_metadata_error();
            }

            for (const auto& r : role.m_roles)
            {
                auto r_keys = r.second;
                if (r_keys.keyids.empty())
                {
                    LOG_ERROR
                        << "'root' metadata should declare at least one key ID in 'keyids' for role: '"
                        << r.first << "'";
                    throw role_metadata_error();
                }
                if (r_keys.threshold == 0)
                {
                    LOG_ERROR
                        << "'root' metadata should declare at least a 'threshold' of 1 for role: '"
                        << r.first << "'";
                    throw role_metadata_error();
                }

                for (auto& rk : r_keys.keyids)
                {
                    if (role.m_keys.find(rk) == role.m_keys.end())
                    {
                        LOG_ERROR
                            << "Invalid 'root' metadata, a key ID is used in 'roles' but not declared in 'keys': '"
                            << rk << "'";
                        throw role_metadata_error();
                    }
                }
            }
        }

        bool is_spec_version_compatible(const json& j)
        {
            try
            {
                auto spec_version = j.at("signed").at("spec_version").get<std::string>();
                return mamba::starts_with(spec_version, "1.");
            }
            catch (const json::exception& e)
            {
                LOG_DEBUG
                    << "Invalid 'root' metadata, impossible to check spec version compatibility: "
                    << e.what();
                return false;
            }
        }
    }  // namespace v1


    namespace v06
    {
        RootRole::RootRole()
            : RootRoleBase("0.6.0")
        {
        }

        RootRole::RootRole(const json& j)
            : RootRoleBase("0.6.0")
        {
            load_from_json(j);
        }

        RootRole::RootRole(const fs::path& path)
            : RootRoleBase("0.6.0")
        {
            auto j = read_file(path);
            load_from_json(j);
        }

        bool RootRole::upgradable() const
        {
            return true;
        };

        std::unique_ptr<RootRoleBase> RootRole::create_update(const json& j)
        {
            if (v06::is_spec_version_compatible(j))
            {
                return std::make_unique<RootRole>(j);
            }
            else if (v1::is_spec_version_compatible(j))
            {
                LOG_DEBUG << "Updating 'root' role spec version";
                return std::make_unique<v1::RootRole>(j);
            }
            {
                LOG_ERROR << "Invalid spec version for 'root' update";
                throw spec_version_error();
            }
        }

        void RootRole::load_from_json(const json& j)
        {
            from_json(j, *this);

            // TUF spec 5.3.4 - Check for an arbitrary software attack
            // Check signatures against current keyids and threshold
            check_role_signatures(j, *this);
        }

        std::set<std::string> RootRole::roles() const
        {
            std::set<std::string> r;
            for (auto& it : m_delegations)
            {
                r.insert(it.first);
            }
            return r;
        }

        std::set<RoleSignature> RootRole::signatures(const json& j) const
        {
            auto sigs = j.at("signatures")
                            .get<std::map<std::string, std::map<std::string, std::string>>>();
            std::set<RoleSignature> unique_sigs;

            for (auto& s : sigs)
            {
                std::string pgp_trailer = "";
                if (s.second.find("other_headers") != s.second.end())
                    pgp_trailer = s.second["other_headers"];

                unique_sigs.insert(
                    RoleSignature({ s.first, s.second.at("signature"), pgp_trailer }));
            }

            return unique_sigs;
        }

        std::string RootRole::canonicalize(const json& j) const
        {
            return j.dump(2);
        }

        json RootRole::upgraded_signable() const
        {
            json v1_equivalent_root;

            v1_equivalent_root["roles"]["root"] = m_delegations.at("root").to_role_keys();
            v1_equivalent_root["roles"]["targets"] = m_delegations.at("key_mgr").to_role_keys();
            v1_equivalent_root["roles"]["snapshot"] = RoleKeys({ std::vector<std::string>(), 1 });
            v1_equivalent_root["roles"]["timestamp"] = RoleKeys({ std::vector<std::string>(), 1 });

            auto allkeys = keys();
            std::map<std::string, Key> v1_keys = allkeys.at("root").keys;
            v1_keys.insert(allkeys.at("key_mgr").keys.cbegin(), allkeys.at("key_mgr").keys.cend());

            v1_equivalent_root["keys"] = v1_keys;
            v1_equivalent_root["_type"] = "root";
            v1_equivalent_root["version"] = version();
            v1_equivalent_root["spec_version"] = "1.0.17";

            return v1_equivalent_root;
        }

        RoleSignature RootRole::upgraded_signature(const json& j,
                                                   const std::string& pk,
                                                   const unsigned char* sk) const
        {
            std::array<unsigned char, MAMBA_ED25519_SIGSIZE_BYTES> sig_bin;
            sign(j.dump(), sk, sig_bin.data());
            auto sig_hex = ::mamba::hex_string(sig_bin);

            return { pk, sig_hex };
        }


        std::map<std::string, RoleFullKeys> RootRole::keys() const
        {
            std::map<std::string, RoleFullKeys> res;
            for (auto& it : m_delegations)
            {
                std::map<std::string, Key> role_keys;
                for (auto& key : it.second.pubkeys)
                {
                    role_keys.insert({ key, Key::from_ed25519(key) });
                }
                res.insert({ it.first, { role_keys, it.second.threshold } });
            }
            return res;
        }

        void to_json(json& j, const RootRole& r)
        {
            to_json(j, static_cast<const RoleBase*>(&r));
        }

        void from_json(const json& j, RootRole& role)
        {
            auto j_signed = j.at("signed");
            try
            {
                from_json(j_signed, static_cast<RoleBase*>(&role));

                auto type = j_signed.at("type").get<std::string>();
                if (type != role.type())
                {
                    LOG_ERROR << "Wrong 'type' found in 'root' metadata, should be 'root': '"
                              << type << "'";
                    throw role_metadata_error();
                }

                if (!v06::is_spec_version_compatible(j))
                {
                    using namespace mamba;

                    auto spec_version_major = split(role.spec_version(), ".", 1)[0];

                    LOG_ERROR
                        << "Incompatible 'spec_version' found in 'root' metadata, should be '0.6.x' but is: '"
                        << spec_version_major << "'";
                    throw role_metadata_error();
                }

                role.set_spec_version(j_signed.at("metadata_spec_version").get<std::string>());

                role.m_delegations
                    = j_signed.at("delegations").get<std::map<std::string, RolePubKeys>>();
            }
            catch (const json::exception& e)
            {
                LOG_ERROR << "Invalid 'root' metadata: " << e.what();
                throw role_metadata_error();
            }

            // TODO: replace those checks with JSON schema?
            std::set<std::string> current_roles;
            for (const auto& r : role.m_delegations)
            {
                current_roles.insert(r.first);

                auto r_keys = r.second;
                if (r_keys.pubkeys.empty())
                {
                    LOG_ERROR
                        << "'root' metadata should declare at least one public key in 'pubkeys' for delegation: '"
                        << r.first << "'";
                    throw role_metadata_error();
                }
                if (r_keys.threshold == 0)
                {
                    LOG_ERROR
                        << "'root' metadata should declare at least a 'threshold' of 1 for role: '"
                        << r.first << "'";
                    throw role_metadata_error();
                }
            }
            std::set<std::string> mandatory_roles = { "root", "key_mgr" };
            if (mandatory_roles != current_roles)
            {
                LOG_ERROR << "Invalid delegations in 'root' metadata";
                throw role_metadata_error();
            }
        }

        bool is_spec_version_compatible(const json& j)
        {
            try
            {
                auto spec_version = j.at("signed").at("metadata_spec_version").get<std::string>();
                return mamba::starts_with(spec_version, "0.6.");
            }
            catch (const json::exception& e)
            {
                LOG_DEBUG
                    << "Invalid 'root' metadata, impossible to check spec version compatibility: "
                    << e.what();
                return false;
            }
        }
    }  // namespace v06

    void to_json(json& j, const Key& key)
    {
        j = json{ { "keytype", key.keytype }, { "scheme", key.scheme }, { "keyval", key.keyval } };
    }

    void to_json(json& j, const RoleKeys& role_keys)
    {
        j = json{ { "keyids", role_keys.keyids }, { "threshold", role_keys.threshold } };
    }

    void to_json(json& j, const RolePubKeys& role_keys)
    {
        j = json{ { "pubkeys", role_keys.pubkeys }, { "threshold", role_keys.threshold } };
    }

    void to_json(json& j, const RoleSignature& role_sig)
    {
        j = json{ { "keyid", role_sig.keyid }, { "sig", role_sig.sig } };
        if (!role_sig.pgp_trailer.empty())
            j["other_headers"] = role_sig.pgp_trailer;
    }

    void to_json(json& j, const RoleBase* role)
    {
        j = json{ { "version", role->version() } };
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

    void from_json(const json& j, RolePubKeys& role_keys)
    {
        j.at("pubkeys").get_to(role_keys.pubkeys);
        j.at("threshold").get_to(role_keys.threshold);
    }

    void from_json(const json& j, RoleSignature& role_sig)
    {
        j.at("keyid").get_to(role_sig.keyid);
        j.at("sig").get_to(role_sig.sig);
        if (j.find("other_headers") != j.end())
            j.at("other_headers").get_to(role_sig.pgp_trailer);
    }

    void from_json(const json& j, RoleBase* role)
    {
        role->m_version = j.at("version");
    }

    RepoTrust::RepoTrust(const std::string& url,
                         const fs::path& local_trusted_root,
                         const SpecVersion& spec_version)
        : m_base_url(url)
    {
        if (spec_version == SpecVersion::kV06)
        {
            p_root = std::make_unique<v1::RootRole>(local_trusted_root);
        }
        else if (spec_version == SpecVersion::kV1)
        {
            p_root = std::make_unique<v1::RootRole>(local_trusted_root);
        }
        else
        {
            throw spec_version_error();
        }
    }
}  // namespace validate
