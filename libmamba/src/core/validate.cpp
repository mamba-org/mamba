// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <iostream>
#include <regex>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>
#include <openssl/evp.h>

#include "mamba/core/context.hpp"
#include "mamba/core/download.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/validate.hpp"
#include "mamba/util/string.hpp"
#include "mamba/util/url.hpp"

namespace mamba::validation
{

    trust_error::trust_error(const std::string& message) noexcept
        : m_message("Content trust error. " + message + ". Aborting.")
    {
        mamba::Console::stream() << this->m_message << '\n';
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

    freeze_error::freeze_error() noexcept
        : trust_error("Possible freeze attack")
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

    fetching_error::fetching_error() noexcept
        : trust_error("Failed to fetch role metadata")
    {
    }

    package_error::package_error() noexcept
        : trust_error("Invalid package")
    {
    }

    role_error::role_error() noexcept
        : trust_error("Invalid role")
    {
    }

    index_error::index_error() noexcept
        : trust_error("Invalid package index metadata")
    {
    }

    namespace
    {
        struct EVPContextDeleter
        {
            using value_type = ::EVP_MD_CTX;
            using pointer_type = value_type*;

            void operator()(pointer_type ptr) const
            {
                if (ptr)
                {
                    ::EVP_MD_CTX_destroy(ptr);
                }
            }
        };

        auto make_EVP_context()
        {
            auto ptr = std::unique_ptr<::EVP_MD_CTX, EVPContextDeleter>();
            ptr.reset(::EVP_MD_CTX_create());
            return ptr;
        }
    }

    std::string sha256sum(const fs::u8path& path)
    {
        unsigned char hash[MAMBA_SHA256_SIZE_BYTES];
        auto mdctx = make_EVP_context();
        EVP_DigestInit_ex(mdctx.get(), EVP_sha256(), nullptr);

        std::ifstream infile = mamba::open_ifstream(path);

        static constexpr std::size_t BUFSIZE = 32768;
        std::vector<char> buffer(BUFSIZE);

        while (infile)
        {
            infile.read(buffer.data(), BUFSIZE);
            auto count = static_cast<std::size_t>(infile.gcount());
            if (!count)
            {
                break;
            }
            EVP_DigestUpdate(mdctx.get(), buffer.data(), count);
        }

        EVP_DigestFinal_ex(mdctx.get(), hash, nullptr);

        return ::mamba::util::hex_string(hash, MAMBA_SHA256_SIZE_BYTES);
    }

    std::string md5sum(const fs::u8path& path)
    {
        unsigned char hash[MAMBA_MD5_SIZE_BYTES];

        auto mdctx = make_EVP_context();
        EVP_DigestInit_ex(mdctx.get(), EVP_md5(), nullptr);

        std::ifstream infile = mamba::open_ifstream(path);

        constexpr std::size_t BUFSIZE = 32768;
        std::vector<char> buffer(BUFSIZE);

        while (infile)
        {
            infile.read(buffer.data(), BUFSIZE);
            auto count = static_cast<std::size_t>(infile.gcount());
            if (!count)
            {
                break;
            }
            EVP_DigestUpdate(mdctx.get(), buffer.data(), count);
        }

        EVP_DigestFinal_ex(mdctx.get(), hash, nullptr);

        return ::mamba::util::hex_string(hash, MAMBA_MD5_SIZE_BYTES);
    }

    bool sha256(const fs::u8path& path, const std::string& validation)
    {
        return sha256sum(path) == validation;
    }

    bool md5(const fs::u8path& path, const std::string& validation)
    {
        return md5sum(path) == validation;
    }

    bool file_size(const fs::u8path& path, std::uintmax_t validation)
    {
        return fs::file_size(path) == validation;
    }

    namespace
    {
        template <class B>
        std::vector<unsigned char> hex_to_bytes(const B& buffer, std::size_t size)
        {
            std::vector<unsigned char> res;
            if (size % 2 != 0)
            {
                return res;
            }

            std::string extract;
            for (auto pos = buffer.cbegin(); pos < buffer.cend(); pos += 2)
            {
                extract.assign(pos, pos + 2);
                res.push_back(static_cast<unsigned char>(std::stoi(extract, nullptr, 16)));
            }
            return res;
        }

        template <class B>
        std::vector<unsigned char> hex_to_bytes(const B& buffer)
        {
            return hex_to_bytes(buffer, buffer.size());
        }

        template <size_t S, class B>
        std::array<unsigned char, S> hex_to_bytes(const B& buffer, int& error_code)
        {
            std::array<unsigned char, S> res{};
            if (buffer.size() != (S * 2))
            {
                LOG_DEBUG << "Wrong size for hexadecimal buffer, expected " << S * 2 << " but is "
                          << buffer.size();
                error_code = 1;
                return res;
            }

            std::string extract;
            std::size_t i = 0;
            for (auto pos = buffer.cbegin(); pos < buffer.cend(); pos += 2)
            {
                extract.assign(pos, pos + 2);
                res[i] = static_cast<unsigned char>(std::stoi(extract, nullptr, 16));
                ++i;
            }
            return res;
        }

        template <size_t S, class B>
        std::array<unsigned char, S> hex_to_bytes(const B& buffer)
        {
            int ec;
            return hex_to_bytes<S>(buffer, ec);
        }
    }

    std::array<unsigned char, MAMBA_ED25519_SIGSIZE_BYTES>
    ed25519_sig_hex_to_bytes(const std::string& sig_hex) noexcept
    {
        return hex_to_bytes<MAMBA_ED25519_SIGSIZE_BYTES>(sig_hex);
    }

    std::array<unsigned char, MAMBA_ED25519_SIGSIZE_BYTES>
    ed25519_sig_hex_to_bytes(const std::string& sig_hex, int& error_code) noexcept

    {
        return hex_to_bytes<MAMBA_ED25519_SIGSIZE_BYTES>(sig_hex, error_code);
    }

    std::array<unsigned char, MAMBA_ED25519_KEYSIZE_BYTES>
    ed25519_key_hex_to_bytes(const std::string& key_hex) noexcept

    {
        return hex_to_bytes<MAMBA_ED25519_KEYSIZE_BYTES>(key_hex);
    }

    std::array<unsigned char, MAMBA_ED25519_KEYSIZE_BYTES>
    ed25519_key_hex_to_bytes(const std::string& key_hex, int& error_code) noexcept

    {
        return hex_to_bytes<MAMBA_ED25519_KEYSIZE_BYTES>(key_hex, error_code);
    }

    int generate_ed25519_keypair(unsigned char* pk, unsigned char* sk)
    {
        std::size_t key_len = MAMBA_ED25519_KEYSIZE_BYTES;
        EVP_PKEY* pkey = NULL;
        struct EVPContext
        {
            EVP_PKEY_CTX* pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, NULL);

            ~EVPContext()
            {
                EVP_PKEY_CTX_free(pctx);
            }

        } evp_context;


        int gen_status = EVP_PKEY_keygen_init(evp_context.pctx);
        if (gen_status != 1)
        {
            LOG_DEBUG << "Failed to initialize ED25519 key pair generation";
            return gen_status;
        }

        gen_status = EVP_PKEY_keygen(evp_context.pctx, &pkey);
        if (gen_status != 1)
        {
            LOG_DEBUG << "Failed to generate ED25519 key pair";
            return gen_status;
        }

        int storage_status = EVP_PKEY_get_raw_public_key(pkey, pk, &key_len);
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

        return 1;
    }

    std::pair<
        std::array<unsigned char, MAMBA_ED25519_KEYSIZE_BYTES>,
        std::array<unsigned char, MAMBA_ED25519_KEYSIZE_BYTES>>
    generate_ed25519_keypair()
    {
        std::array<unsigned char, MAMBA_ED25519_KEYSIZE_BYTES> pk, sk;
        generate_ed25519_keypair(pk.data(), sk.data());
        return { pk, sk };
    }

    std::pair<std::string, std::string> generate_ed25519_keypair_hex()
    {
        auto pair = generate_ed25519_keypair();
        return { ::mamba::util::hex_string(pair.first), ::mamba::util::hex_string(pair.second) };
    }

    int sign(const std::string& data, const unsigned char* sk, unsigned char* signature)
    {
        std::size_t msg_len = data.size();
        std::size_t sig_len = MAMBA_ED25519_SIGSIZE_BYTES;
        auto msg = reinterpret_cast<const unsigned char*>(data.c_str());

        EVP_PKEY* ed_key = EVP_PKEY_new_raw_private_key(
            EVP_PKEY_ED25519,
            NULL,
            sk,
            MAMBA_ED25519_KEYSIZE_BYTES
        );
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

    int sign(const std::string& data, const std::string& sk, std::string& signature)
    {
        int error_code = 0;

        auto bin_sk = ed25519_key_hex_to_bytes(sk, error_code);
        if (error_code != 0)
        {
            LOG_DEBUG << "Invalid secret key";
            return 0;
        }

        std::array<unsigned char, MAMBA_ED25519_SIGSIZE_BYTES> sig;

        error_code = sign(data, bin_sk.data(), sig.data());
        signature = ::mamba::util::hex_string(sig, MAMBA_ED25519_SIGSIZE_BYTES);

        return error_code;
    }

    int verify(
        const unsigned char* data,
        std::size_t data_len,
        const unsigned char* pk,
        const unsigned char* signature
    )
    {
        std::size_t sig_len = MAMBA_ED25519_SIGSIZE_BYTES;

        EVP_PKEY* ed_key = EVP_PKEY_new_raw_public_key(
            EVP_PKEY_ED25519,
            NULL,
            pk,
            MAMBA_ED25519_KEYSIZE_BYTES
        );
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
        auto raw_data = reinterpret_cast<const unsigned char*>(data.c_str());

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

    int
    verify_gpg_hashed_msg(const unsigned char* data, const unsigned char* pk, const unsigned char* signature)
    {
        return verify(data, MAMBA_SHA256_SIZE_BYTES, pk, signature);
    }


    int
    verify_gpg_hashed_msg(const std::string& data, const unsigned char* pk, const unsigned char* signature)
    {
        auto data_bin = hex_to_bytes<MAMBA_SHA256_SIZE_BYTES>(data);

        return verify(data_bin.data(), MAMBA_SHA256_SIZE_BYTES, pk, signature);
    }

    int
    verify_gpg_hashed_msg(const std::string& data, const std::string& pk, const std::string& signature)
    {
        auto signature_bin = ed25519_sig_hex_to_bytes(signature);
        auto pk_bin = ed25519_key_hex_to_bytes(pk);

        return verify_gpg_hashed_msg(data, pk_bin.data(), signature_bin.data());
    }

    int verify_gpg(
        const std::string& data,
        const std::string& pgp_v4_trailer,
        const std::string& pk,
        const std::string& signature
    )
    {
        unsigned long long data_len = data.size();
        auto data_bin = reinterpret_cast<const unsigned char*>(data.c_str());

        auto signature_bin = ed25519_sig_hex_to_bytes(signature);
        auto pk_bin = ed25519_key_hex_to_bytes(pk);

        std::size_t trailer_hex_size = pgp_v4_trailer.size();
        if (trailer_hex_size % 2 != 0)
        {
            LOG_DEBUG << "PGP V4 trailer size is not even: " << pgp_v4_trailer;
            return 0;
        }

        auto pgp_trailer_bin = hex_to_bytes(pgp_v4_trailer);
        auto final_trailer_bin = hex_to_bytes<2>(std::string("04ff"));

        uint32_t trailer_bin_len_big_endian = static_cast<uint32_t>(pgp_trailer_bin.size());

#ifdef _WIN32
        trailer_bin_len_big_endian = _byteswap_ulong(trailer_bin_len_big_endian);
#else
        trailer_bin_len_big_endian = __builtin_bswap32(trailer_bin_len_big_endian);
#endif

        std::array<unsigned char, MAMBA_SHA256_SIZE_BYTES> hash;

        auto mdctx = make_EVP_context();
        EVP_DigestInit_ex(mdctx.get(), EVP_sha256(), nullptr);

        EVP_DigestUpdate(mdctx.get(), data_bin, data_len);
        EVP_DigestUpdate(mdctx.get(), pgp_trailer_bin.data(), pgp_trailer_bin.size());
        EVP_DigestUpdate(mdctx.get(), final_trailer_bin.data(), final_trailer_bin.size());
        EVP_DigestUpdate(mdctx.get(), reinterpret_cast<unsigned char*>(&trailer_bin_len_big_endian), 4);

        EVP_DigestFinal_ex(mdctx.get(), hash.data(), nullptr);

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

    RoleFullKeys::RoleFullKeys(const std::map<std::string, Key>& keys_, const std::size_t& threshold_)
        : keys(keys_)
        , threshold(threshold_){};

    std::map<std::string, Key> RoleFullKeys::to_keys() const
    {
        return keys;
    }

    RoleKeys RoleFullKeys::to_roles() const
    {
        std::vector<std::string> keyids;
        for (auto& k : keys)
        {
            keyids.push_back(k.first);
        }
        return { keyids, threshold };
    }

    void check_timestamp_metadata_format(const std::string& ts)
    {
        std::regex timestamp_re("^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}Z$");

        if (!std::regex_match(ts, timestamp_re))
        {
            mamba::Console::stream() << "Invalid timestamp in content trust metadata";
            LOG_ERROR << "Invalid timestamp format '" << ts
                      << "', should be UTC ISO8601 ('<YYYY>-<MM>-<DD>T<HH>:<MM>:<SS>Z')";
            throw role_metadata_error();
        }
    }

    SpecBase::SpecBase(const std::string& spec_version)
        : m_spec_version(spec_version)
    {
    }

    std::string SpecBase::version_str() const
    {
        return m_spec_version;
    }

    std::string SpecBase::compatible_prefix() const
    {
        auto split_spec_version = ::mamba::util::split(m_spec_version, ".", 2);
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

    std::vector<std::string> SpecBase::upgrade_prefix() const
    {
        auto split_spec_version = ::mamba::util::split(m_spec_version, ".", 2);
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

    bool SpecBase::is_compatible(const fs::u8path& p) const
    {
        std::regex name_re;
        std::smatch matches;
        std::size_t min_match_size;

        std::string f_name = p.filename().string();
        std::string f_spec_version_str, f_version_str, f_type, f_ext;

        name_re = "^(?:[1-9]+\\d*.)?(?:sv([1-9]\\d*|0\\.[1-9]\\d*).)?(\\w+)\\.(\\w+)$";
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

    bool SpecBase::is_compatible(const std::string& version) const
    {
        return ::mamba::util::starts_with(version, compatible_prefix() + ".");
    }

    bool SpecBase::is_compatible(const nlohmann::json& j) const
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

    bool SpecBase::is_upgrade(const std::string& version) const
    {
        auto upgrade_prefixes = upgrade_prefix();
        std::vector<std::string_view> possible_upgrades;
        for (auto& s : upgrade_prefixes)
        {
            s += '.';
            possible_upgrades.push_back(s);
        }

        return ::mamba::util::starts_with_any(version, possible_upgrades);
    }

    bool SpecBase::is_upgrade(const nlohmann::json& j) const
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

    std::string SpecBase::get_json_value(const nlohmann::json& j) const
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

    std::string SpecBase::canonicalize(const nlohmann::json& j) const
    {
        return j.dump();
    }

    bool SpecBase::upgradable() const
    {
        return false;
    };

    bool operator==(const SpecBase& sv1, const SpecBase& sv2)
    {
        return sv1.version_str() == sv2.version_str();
    }

    bool operator!=(const SpecBase& sv1, const SpecBase& sv2)
    {
        return sv1.version_str() != sv2.version_str();
    }

    RoleBase::RoleBase(const std::string& type, std::shared_ptr<SpecBase> sv)
        : m_type(type)
    {
        p_spec = std::move(sv);
    };

    RoleBase::~RoleBase(){};

    std::string RoleBase::type() const
    {
        return m_type;
    }

    std::size_t RoleBase::version() const
    {
        return m_version;
    }

    std::string RoleBase::expires() const
    {
        return m_expires;
    }

    std::string RoleBase::file_ext() const
    {
        return m_ext;
    }

    SpecBase& RoleBase::spec_version() const
    {
        return *p_spec;
    }

    std::shared_ptr<SpecBase> RoleBase::spec_impl() const
    {
        return p_spec;
    }

    bool RoleBase::expired(const TimeRef& time_reference) const
    {
        return time_reference.timestamp().compare(m_expires) < 0 ? false : true;
    }

    std::string RoleBase::canonicalize(const nlohmann::json& j) const
    {
        return p_spec->canonicalize(j);
    }

    std::set<RoleSignature> RoleBase::signatures(const nlohmann::json& j) const
    {
        return p_spec->signatures(j);
    }

    std::set<std::string> RoleBase::roles() const
    {
        std::set<std::string> r;
        for (auto& it : m_defined_roles)
        {
            r.insert(it.first);
        }
        return r;
    }

    std::set<std::string> RoleBase::mandatory_defined_roles() const
    {
        return {};
    }

    std::set<std::string> RoleBase::optionally_defined_roles() const
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

    std::map<std::string, RoleFullKeys> RoleBase::all_keys() const
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

    nlohmann::json RoleBase::read_json_file(const fs::u8path& p, bool update) const
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

    RootRole::RootRole(std::shared_ptr<SpecBase> spec)
        : RoleBase("root", spec)
    {
    }

    std::vector<fs::u8path> RootRole::possible_update_files()
    {
        auto new_v = std::to_string(version() + 1);
        auto compat_spec = spec_impl()->compatible_prefix();
        auto upgrade_spec = spec_impl()->upgrade_prefix();

        std::vector<fs::u8path> files;
        // upgrade first
        for (auto& s : upgrade_spec)
        {
            files.push_back(
                ::mamba::util::join(".", std::vector<std::string>({ new_v, "sv" + s, "root.json" }))
            );
        }
        // compatible next
        files.push_back(::mamba::util::join(
            ".",
            std::vector<std::string>({ new_v, "sv" + compat_spec, "root.json" })
        ));
        // then finally undefined spec
        files.push_back(::mamba::util::join(".", std::vector<std::string>({ new_v, "root.json" })));

        return files;
    }

    std::unique_ptr<RootRole> RootRole::update(fs::u8path path)
    {
        auto j = read_json_file(path, true);
        return update(j);
    }

    // `create_update` currently catch a possible spec version update by testing
    // and extracting spec version from JSON. It could be done upstream (in
    // `update(fs::u8path)`) if we decide to specify the spec version in the file name.
    // The filename would take the form VERSION_NUMBER.SPECVERSION.FILENAME.EXT
    // To disambiguate version and spec version: 1.sv0.6.root.json or 1.sv1.root.json
    std::unique_ptr<RootRole> RootRole::update(nlohmann::json j)
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

    namespace v1
    {
        SpecImpl::SpecImpl(const std::string& sv)
            : SpecBase(sv)
        {
        }

        std::string SpecImpl::json_key() const
        {
            return "spec_version";
        }

        std::string SpecImpl::expiration_json_key() const
        {
            return "expires";
        }

        std::set<RoleSignature> SpecImpl::signatures(const nlohmann::json& j) const
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

        std::unique_ptr<RootRole> RootImpl::create_update(const nlohmann::json& j)
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

        RoleFullKeys RootImpl::self_keys() const
        {
            return m_defined_roles.at("root");
        }

        std::set<std::string> RootImpl::mandatory_defined_roles() const
        {
            return { "root", "snapshot", "targets", "timestamp" };
        }

        std::set<std::string> RootImpl::optionally_defined_roles() const
        {
            return { "mirrors" };
        }

        void
        RootImpl::set_defined_roles(std::map<std::string, Key> keys, std::map<std::string, RoleKeys> roles)
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

        std::unique_ptr<RepoIndexChecker> RootImpl::build_index_checker(
            Context&,
            const TimeRef& /*time_reference*/,
            const std::string& /*url*/,
            const fs::u8path& /*cache_path*/
        ) const
        {
            std::unique_ptr<RepoIndexChecker> ptr;
            return ptr;
        }

        void to_json(nlohmann::json& j, const RootImpl& r)
        {
            to_json(j, static_cast<const RoleBase*>(&r));
        }

        void from_json(const nlohmann::json& j, RootImpl& role)
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
    }  // namespace v1


    namespace v06
    {
        SpecImpl::SpecImpl(const std::string& sv)
            : SpecBase(sv)
        {
        }

        std::string SpecImpl::json_key() const
        {
            return "metadata_spec_version";
        }

        std::string SpecImpl::expiration_json_key() const
        {
            return "expiration";
        }

        std::set<RoleSignature> SpecImpl::signatures(const nlohmann::json& j) const
        {
            auto sigs = j.at("signatures")
                            .get<std::map<std::string, std::map<std::string, std::string>>>();
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

        bool SpecImpl::upgradable() const
        {
            return true;
        };

        std::string SpecImpl::canonicalize(const nlohmann::json& j) const
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

        std::string V06RoleBaseExtension::timestamp() const
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

        std::unique_ptr<RootRole> RootImpl::create_update(const nlohmann::json& j)
        {
            if (v06::SpecImpl().is_compatible(j))
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

        nlohmann::json RootImpl::upgraded_signable() const
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

        RoleSignature RootImpl::upgraded_signature(
            const nlohmann::json& j,
            const std::string& pk,
            const unsigned char* sk
        ) const
        {
            std::array<unsigned char, MAMBA_ED25519_SIGSIZE_BYTES> sig_bin;
            sign(j.dump(), sk, sig_bin.data());
            auto sig_hex = ::mamba::util::hex_string(sig_bin);

            return { pk, sig_hex };
        }

        RoleFullKeys RootImpl::self_keys() const
        {
            return m_defined_roles.at("root");
        }

        std::set<std::string> RootImpl::mandatory_defined_roles() const
        {
            return { "root", "key_mgr" };
        }

        std::set<std::string> RootImpl::optionally_defined_roles() const
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

        std::unique_ptr<RepoIndexChecker> RootImpl::build_index_checker(
            Context& context,
            const TimeRef& time_reference,
            const std::string& base_url,
            const fs::u8path& cache_path
        ) const
        {
            fs::u8path metadata_path = cache_path / "key_mgr.json";

            auto tmp_dir = std::make_unique<mamba::TemporaryDirectory>();
            auto tmp_metadata_path = tmp_dir->path() / "key_mgr.json";

            const auto url = mamba::util::URL::parse(base_url + "/key_mgr.json");

            if (check_resource_exists(url.pretty_str(), context))
            {
                DownloadRequest request("key_mgr.json", url.pretty_str(), tmp_metadata_path.string());
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

        KeyMgrRole RootImpl::create_key_mgr(const fs::u8path& p) const
        {
            return KeyMgrRole(p, all_keys()["key_mgr"], spec_impl());
        }

        KeyMgrRole RootImpl::create_key_mgr(const nlohmann::json& j) const
        {
            return KeyMgrRole(j, all_keys()["key_mgr"], spec_impl());
        }

        void to_json(nlohmann::json& j, const RootImpl& r)
        {
            to_json(j, static_cast<const RoleBase*>(&r));
        }

        void from_json(const nlohmann::json& j, RootImpl& role)
        {
            auto j_signed = j.at("signed");
            try
            {
                from_json(j_signed, static_cast<RoleBase*>(&role));

                role.set_timestamp(j_signed.at("timestamp").get<std::string>());

                auto type = j_signed.at("type").get<std::string>();
                if (type != role.type())
                {
                    LOG_ERROR << "Wrong 'type' found in 'root' metadata, should be 'root': '"
                              << type << "'";
                    throw role_metadata_error();
                }

                role.set_spec_version(std::make_shared<SpecImpl>(
                    j_signed.at("metadata_spec_version").get<std::string>()
                ));

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

        KeyMgrRole::KeyMgrRole(
            const fs::u8path& p,
            const RoleFullKeys& keys,
            const std::shared_ptr<SpecBase> spec
        )
            : RoleBase("key_mgr", spec)
            , m_keys(keys)
        {
            auto j = read_json_file(p);
            load_from_json(j);
        }

        KeyMgrRole::KeyMgrRole(
            const nlohmann::json& j,
            const RoleFullKeys& keys,
            const std::shared_ptr<SpecBase> spec
        )
            : RoleBase("key_mgr", spec)
            , m_keys(keys)
        {
            load_from_json(j);
        }

        KeyMgrRole::KeyMgrRole(
            const std::string& json_str,
            const RoleFullKeys& keys,
            const std::shared_ptr<SpecBase> spec
        )
            : RoleBase("key_mgr", spec)
            , m_keys(keys)
        {
            load_from_json(nlohmann::json::parse(json_str));
        }

        void KeyMgrRole::load_from_json(const nlohmann::json& j)
        {
            from_json(j, *this);
            // Check signatures against keyids and threshold
            check_role_signatures(j, *this);
        }

        RoleFullKeys KeyMgrRole::self_keys() const
        {
            return m_keys;
        }

        PkgMgrRole KeyMgrRole::create_pkg_mgr(const fs::u8path& p) const
        {
            return PkgMgrRole(p, all_keys()["pkg_mgr"], spec_impl());
        }

        PkgMgrRole KeyMgrRole::create_pkg_mgr(const nlohmann::json& j) const
        {
            return PkgMgrRole(j, all_keys()["pkg_mgr"], spec_impl());
        }

        std::unique_ptr<RepoIndexChecker> KeyMgrRole::build_index_checker(
            Context& context,
            const TimeRef& time_reference,
            const std::string& base_url,
            const fs::u8path& cache_path
        ) const
        {
            fs::u8path metadata_path = cache_path / "pkg_mgr.json";

            auto tmp_dir = std::make_unique<mamba::TemporaryDirectory>();
            auto tmp_metadata_path = tmp_dir->path() / "pkg_mgr.json";

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

        std::set<std::string> KeyMgrRole::mandatory_defined_roles() const
        {
            return { "pkg_mgr" };
        }

        std::set<std::string> KeyMgrRole::optionally_defined_roles() const
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
            to_json(j, static_cast<const RoleBase*>(&r));
        }

        void from_json(const nlohmann::json& j, KeyMgrRole& role)
        {
            auto j_signed = j.at("signed");
            try
            {
                from_json(j_signed, static_cast<RoleBase*>(&role));

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

        PkgMgrRole::PkgMgrRole(const RoleFullKeys& keys, const std::shared_ptr<SpecBase> spec)
            : RoleBase("pkg_mgr", spec)
            , m_keys(keys)
        {
        }

        PkgMgrRole::PkgMgrRole(
            const fs::u8path& p,
            const RoleFullKeys& keys,
            const std::shared_ptr<SpecBase> spec
        )
            : RoleBase("pkg_mgr", spec)
            , m_keys(keys)
        {
            auto j = read_json_file(p);
            load_from_json(j);
        }

        PkgMgrRole::PkgMgrRole(
            const nlohmann::json& j,
            const RoleFullKeys& keys,
            const std::shared_ptr<SpecBase> spec
        )
            : RoleBase("pkg_mgr", spec)
            , m_keys(keys)
        {
            load_from_json(j);
        }

        PkgMgrRole::PkgMgrRole(
            const std::string& json_str,
            const RoleFullKeys& keys,
            const std::shared_ptr<SpecBase> spec
        )
            : RoleBase("pkg_mgr", spec)
            , m_keys(keys)
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
            to_json(j, static_cast<const RoleBase*>(&r));
        }

        void from_json(const nlohmann::json& j, PkgMgrRole& role)
        {
            auto j_signed = j.at("signed");
            try
            {
                from_json(j_signed, static_cast<RoleBase*>(&role));

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

        RoleFullKeys PkgMgrRole::self_keys() const
        {
            return m_keys;
        }

        std::set<RoleSignature> PkgMgrRole::pkg_signatures(const nlohmann::json& j) const
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

        void PkgMgrRole::check_pkg_signatures(
            const nlohmann::json& metadata,
            const nlohmann::json& signatures
        ) const
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
                        LOG_ERROR << "Validation failed on package: '" << pkg_name
                                  << "' : " << e.what();
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
                LOG_ERROR << "Validation failed on package index: '" << p.string()
                          << "' : " << e.what();
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

    void to_json(nlohmann::json& j, const Key& key)
    {
        j = { { "keytype", key.keytype }, { "scheme", key.scheme }, { "keyval", key.keyval } };
    }

    void to_json(nlohmann::json& j, const RoleKeys& role_keys)
    {
        j = { { "keyids", role_keys.keyids }, { "threshold", role_keys.threshold } };
    }

    void to_json(nlohmann::json& j, const RolePubKeys& role_keys)
    {
        j = { { "pubkeys", role_keys.pubkeys }, { "threshold", role_keys.threshold } };
    }

    void to_json(nlohmann::json& j, const RoleFullKeys& k)
    {
        j = { { "keys", k.keys }, { "threshold", k.threshold } };
    }

    void to_json(nlohmann::json& j, const RoleSignature& role_sig)
    {
        j = { { "keyid", role_sig.keyid }, { "sig", role_sig.sig } };
        if (!role_sig.pgp_trailer.empty())
        {
            j["other_headers"] = role_sig.pgp_trailer;
        }
    }

    void to_json(nlohmann::json& j, const RoleBase* role)
    {
        j = { { "version", role->version() }, { "expires", role->expires() } };
    }

    void from_json(const nlohmann::json& j, Key& key)
    {
        j.at("keytype").get_to(key.keytype);
        j.at("scheme").get_to(key.scheme);
        j.at("keyval").get_to(key.keyval);
    }

    void from_json(const nlohmann::json& j, RoleKeys& role_keys)
    {
        j.at("keyids").get_to(role_keys.keyids);
        j.at("threshold").get_to(role_keys.threshold);
    }

    void from_json(const nlohmann::json& j, RolePubKeys& role_keys)
    {
        j.at("pubkeys").get_to(role_keys.pubkeys);
        j.at("threshold").get_to(role_keys.threshold);
    }

    void from_json(const nlohmann::json& j, RoleFullKeys& k)
    {
        j.at("keys").get_to(k.keys);
        j.at("threshold").get_to(k.threshold);
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

    void from_json(const nlohmann::json& j, RoleBase* role)
    {
        role->m_version = j.at("version");
        role->set_expiration(j.at(role->spec_version().expiration_json_key()));
    }

    RepoChecker::RepoChecker(
        Context& context,
        const std::string& base_url,
        const fs::u8path& ref_path,
        const fs::u8path& cache_path
    )
        : m_base_url(base_url)
        , m_ref_path(ref_path)
        , m_cache_path(cache_path)
        , m_context(context)
    {
    }

    const fs::u8path& RepoChecker::cache_path()
    {
        return m_cache_path;
    }

    void RepoChecker::generate_index_checker()
    {
        if (p_index_checker == nullptr)
        {
            // TUF spec 5.1 - Record fixed update start time
            // Expiration computations will be done against
            // this reference
            // https://theupdateframework.github.io/specification/latest/#fix-time
            const TimeRef time_reference;

            auto root = get_root_role(time_reference);
            p_index_checker = root->build_index_checker(
                m_context,
                time_reference,
                m_base_url,
                cache_path()
            );

            LOG_INFO << "Index checker successfully generated for '" << m_base_url << "'";
        }
    }

    void RepoChecker::verify_index(const nlohmann::json& j) const
    {
        p_index_checker->verify_index(j);
    }

    void RepoChecker::verify_index(const fs::u8path& p) const
    {
        p_index_checker->verify_index(p);
    }

    void
    RepoChecker::verify_package(const nlohmann::json& signed_data, const nlohmann::json& signatures) const
    {
        p_index_checker->verify_package(signed_data, signatures);
    }

    std::size_t RepoChecker::root_version()
    {
        return m_root_version;
    }

    fs::u8path RepoChecker::ref_root()
    {
        return m_ref_path / "root.json";
    }

    fs::u8path RepoChecker::cached_root()
    {
        if (cache_path().empty())
        {
            return "";
        }
        else
        {
            return cache_path() / "root.json";
        }
    }

    void RepoChecker::persist_file(const fs::u8path& file_path)
    {
        if (fs::exists(cached_root()))
        {
            fs::remove(cached_root());
        }
        if (!cached_root().empty())
        {
            fs::copy(file_path, cached_root());
        }
    }

    fs::u8path RepoChecker::initial_trusted_root()
    {
        if (fs::exists(cached_root()))
        {
            LOG_DEBUG << "Using cache for 'root' initial trusted file";
            return cached_root();
        }

        if (!fs::exists(m_ref_path))
        {
            LOG_ERROR << "'root' initial trusted file not found at '" << m_ref_path.string()
                      << "' for repo '" << m_base_url << "'";
            throw role_file_error();
        }
        else
        {
            return ref_root();
        }
    }

    std::unique_ptr<RootRole> RepoChecker::get_root_role(const TimeRef& time_reference)
    {
        // TUF spec 5.3 - Update the root role
        // https://theupdateframework.github.io/specification/latest/#update-root

        std::unique_ptr<RootRole> updated_root;

        LOG_DEBUG << "Loading 'root' metadata for repo '" << m_base_url << "'";
        auto trusted_root = initial_trusted_root();

        if (v06::SpecImpl().is_compatible(trusted_root))
        {
            updated_root = std::make_unique<v06::RootImpl>(trusted_root);
        }
        else if (v1::SpecImpl().is_compatible(trusted_root))
        {
            updated_root = std::make_unique<v1::RootImpl>(trusted_root);
        }
        else
        {
            LOG_ERROR << "Invalid 'root' initial trusted file '" << trusted_root.string()
                      << "' for repo '" << m_base_url << "'";
            throw role_file_error();
        }

        if (trusted_root != cached_root())
        {
            persist_file(trusted_root);
        }

        auto update_files = updated_root->possible_update_files();
        auto tmp_dir = std::make_unique<mamba::TemporaryDirectory>();
        auto tmp_dir_path = tmp_dir->path();

        // do chained updates
        LOG_DEBUG << "Starting updates of 'root' metadata";
        do
        {
            fs::u8path tmp_file_path;

            // Update from the most recent spec supported by this client
            for (auto& f : update_files)
            {
                auto url = ::mamba::util::concat(m_base_url, "/", f.string());
                tmp_file_path = tmp_dir_path / f;

                if (check_resource_exists(url, m_context))
                {
                    DownloadRequest request(f.string(), url, tmp_file_path.string());
                    DownloadResult res = download(std::move(request), m_context);

                    if (res)
                    {
                        break;
                    }
                }
                tmp_file_path = "";
            }

            if (tmp_file_path.empty())
            {
                break;
            }

            updated_root = updated_root->update(tmp_file_path);
            // TUF spec 5.3.8 - Persist root metadata
            // Updated 'root' metadata are persisted in a cache directory
            persist_file(tmp_file_path);

            // Set the next possible files
            update_files = updated_root->possible_update_files();
        }
        // TUF spec 5.3.9 - Repeat steps 5.3.2 to 5.3.9
        while (true);

        m_root_version = updated_root->version();
        LOG_DEBUG << "Latest 'root' metadata has version " << m_root_version;

        // TUF spec 5.3.10 - Check for a freeze attack
        // Updated 'root' role should not be expired
        // https://theupdateframework.github.io/specification/latest/#update-root
        if (updated_root->expired(time_reference))
        {
            LOG_ERROR << "Possible freeze attack of 'root' metadata.\nExpired: "
                      << updated_root->expires();
            throw freeze_error();
        }

        return updated_root;
    };
}  // namespace validate
