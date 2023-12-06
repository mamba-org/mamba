// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <array>
#include <iostream>
#include <regex>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>
#include <openssl/evp.h>

#include "mamba/core/context.hpp"
#include "mamba/core/download.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/validate.hpp"
#include "mamba/util/cryptography.hpp"
#include "mamba/util/string.hpp"
#include "mamba/validation/errors.hpp"
#include "mamba/validation/update_framework_v0_6.hpp"
#include "mamba/validation/update_framework_v1.hpp"


namespace mamba::validation
{
    auto sha256sum(const fs::u8path& path) -> std::string_view
    {
        std::ifstream infile = mamba::open_ifstream(path);
        thread_local auto hasher = util::Sha256Hasher();
        thread_local auto hash = hasher.file_hex(infile);
        return { hash.data(), hash.size() };
    }

    auto md5sum(const fs::u8path& path) -> std::string_view
    {
        std::ifstream infile = mamba::open_ifstream(path);
        thread_local auto hasher = util::Md5Hasher();
        thread_local auto hash = hasher.file_hex(infile);
        return { hash.data(), hash.size() };
    }

    auto file_size(const fs::u8path& path, std::uintmax_t validation) -> bool
    {
        return fs::file_size(path) == validation;
    }

    namespace
    {
        template <size_t S, class B>
        [[nodiscard]] auto hex_to_bytes_arr(const B& buffer, int& error_code) noexcept
            -> std::array<std::byte, S>
        {
            auto out = std::array<std::byte, S>{};
            auto err = util::EncodingError::Ok;
            util::hex_to_bytes_to(buffer, out.data(), err);
            error_code = err != util::EncodingError::Ok;
            return out;
        }

        template <class B>
        [[nodiscard]] auto hex_to_bytes_vec(const B& buffer, int& error_code) noexcept
            -> std::vector<std::byte>
        {
            auto out = std::vector<std::byte>(buffer.size() / 2);
            auto err = util::EncodingError::Ok;
            util::hex_to_bytes_to(buffer, out.data(), err);
            error_code = err != util::EncodingError::Ok;
            return out;
        }
    }

    auto ed25519_sig_hex_to_bytes(const std::string& sig_hex, int& error_code) noexcept
        -> std::array<std::byte, MAMBA_ED25519_SIGSIZE_BYTES>

    {
        return hex_to_bytes_arr<MAMBA_ED25519_SIGSIZE_BYTES>(sig_hex, error_code);
    }

    auto ed25519_key_hex_to_bytes(const std::string& key_hex, int& error_code) noexcept
        -> std::array<std::byte, MAMBA_ED25519_KEYSIZE_BYTES>

    {
        return hex_to_bytes_arr<MAMBA_ED25519_KEYSIZE_BYTES>(key_hex, error_code);
    }

    auto generate_ed25519_keypair(std::byte* pk, std::byte* sk) -> int
    {
        std::size_t key_len = MAMBA_ED25519_KEYSIZE_BYTES;
        EVP_PKEY* pkey = nullptr;
        struct EVPContext
        {
            EVP_PKEY_CTX* pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, nullptr);

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

        int storage_status = EVP_PKEY_get_raw_public_key(
            pkey,
            reinterpret_cast<unsigned char*>(pk),
            &key_len
        );
        if (storage_status != 1)
        {
            LOG_DEBUG << "Failed to store public key of generated ED25519 key pair";
            return storage_status;
        }
        storage_status = EVP_PKEY_get_raw_private_key(
            pkey,
            reinterpret_cast<unsigned char*>(sk),
            &key_len
        );
        if (storage_status != 1)
        {
            LOG_DEBUG << "Failed to store private key of generated ED25519 key pair";
            return storage_status;
        }

        return 1;
    }

    auto generate_ed25519_keypair() -> std::pair<
        std::array<std::byte, MAMBA_ED25519_KEYSIZE_BYTES>,
        std::array<std::byte, MAMBA_ED25519_KEYSIZE_BYTES>>
    {
        std::array<std::byte, MAMBA_ED25519_KEYSIZE_BYTES> pk, sk;
        generate_ed25519_keypair(pk.data(), sk.data());
        return { pk, sk };
    }

    auto generate_ed25519_keypair_hex() -> std::pair<std::string, std::string>
    {
        auto [first, second] = generate_ed25519_keypair();
        // TODO change function signature to use std::byte
        const auto first_data = reinterpret_cast<const std::byte*>(first.data());
        const auto second_data = reinterpret_cast<const std::byte*>(second.data());
        return {
            util::bytes_to_hex_str(first_data, first_data + first.size()),
            util::bytes_to_hex_str(second_data, second_data + second.size()),
        };
    }

    auto sign(const std::string& data, const std::byte* sk, std::byte* signature) -> int
    {
        EVP_PKEY* ed_key = EVP_PKEY_new_raw_private_key(
            EVP_PKEY_ED25519,
            nullptr,
            reinterpret_cast<const unsigned char*>(sk),
            MAMBA_ED25519_KEYSIZE_BYTES
        );
        EVP_MD_CTX* md_ctx = EVP_MD_CTX_new();

        if (ed_key == nullptr)
        {
            LOG_DEBUG << "Failed to read secret key raw buffer during signing step";
            return 0;
        }

        int init_status, sign_status;
        init_status = EVP_DigestSignInit(md_ctx, nullptr, nullptr, nullptr, ed_key);
        if (init_status != 1)
        {
            LOG_DEBUG << "Failed to init signing step";
            return init_status;
        }

        std::size_t sig_len = MAMBA_ED25519_SIGSIZE_BYTES;
        sign_status = EVP_DigestSign(
            md_ctx,
            reinterpret_cast<unsigned char*>(signature),
            &sig_len,
            reinterpret_cast<const unsigned char*>(data.data()),
            data.size()
        );
        if (sign_status != 1)
        {
            LOG_DEBUG << "Failed to sign the data";
            return sign_status;
        }

        EVP_MD_CTX_free(md_ctx);
        return 1;
    }

    auto sign(const std::string& data, const std::string& sk, std::string& signature) -> int
    {
        int error_code = 0;

        auto bin_sk = ed25519_key_hex_to_bytes(sk, error_code);
        if (error_code != 0)
        {
            LOG_DEBUG << "Invalid secret key";
            return 0;
        }

        std::array<std::byte, MAMBA_ED25519_SIGSIZE_BYTES> sig;

        error_code = sign(data, bin_sk.data(), sig.data());

        const auto sig_data = reinterpret_cast<const std::byte*>(sig.data());
        signature = util::bytes_to_hex_str(sig_data, sig_data + sig.size());

        return error_code;
    }

    auto
    verify(const std::byte* data, std::size_t data_len, const std::byte* pk, const std::byte* signature)
        -> int
    {
        EVP_PKEY* ed_key = EVP_PKEY_new_raw_public_key(
            EVP_PKEY_ED25519,
            nullptr,
            reinterpret_cast<const unsigned char*>(pk),
            MAMBA_ED25519_KEYSIZE_BYTES
        );
        EVP_MD_CTX* md_ctx = EVP_MD_CTX_new();

        if (ed_key == nullptr)
        {
            LOG_DEBUG << "Failed to read public key raw buffer during verification step";
            return 0;
        }

        int init_status, verif_status;
        init_status = EVP_DigestVerifyInit(md_ctx, nullptr, nullptr, nullptr, ed_key);
        if (init_status != 1)
        {
            LOG_DEBUG << "Failed to init verification step";
            return init_status;
        }

        std::size_t sig_len = MAMBA_ED25519_SIGSIZE_BYTES;
        verif_status = EVP_DigestVerify(
            md_ctx,
            reinterpret_cast<const unsigned char*>(signature),
            sig_len,
            reinterpret_cast<const unsigned char*>(data),
            data_len
        );
        if (verif_status != 1)
        {
            LOG_DEBUG << "Failed to verify the data signature";
            return verif_status;
        }

        EVP_MD_CTX_free(md_ctx);
        return 1;
    }

    auto verify(const std::string& data, const std::byte* pk, const std::byte* signature) -> int
    {
        unsigned long long data_len = data.size();
        auto raw_data = reinterpret_cast<const std::byte*>(data.data());

        return verify(raw_data, data_len, pk, signature);
    }

    auto verify(const std::string& data, const std::string& pk, const std::string& signature) -> int
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

    auto verify_gpg_hashed_msg(const std::byte* data, const std::byte* pk, const std::byte* signature)
        -> int
    {
        return verify(data, MAMBA_SHA256_SIZE_BYTES, pk, signature);
    }


    auto
    verify_gpg_hashed_msg(const std::string& data, const std::byte* pk, const std::byte* signature)
        -> int
    {
        int error = 0;
        auto data_bin = hex_to_bytes_arr<MAMBA_SHA256_SIZE_BYTES>(data, error);

        return verify(data_bin.data(), MAMBA_SHA256_SIZE_BYTES, pk, signature) + error;
    }

    auto
    verify_gpg_hashed_msg(const std::string& data, const std::string& pk, const std::string& signature)
        -> int
    {
        int error = 0;
        auto signature_bin = ed25519_sig_hex_to_bytes(signature, error);
        if (error)
        {
            return error;
        }
        auto pk_bin = ed25519_key_hex_to_bytes(pk, error);
        if (error)
        {
            return error;
        }

        return verify_gpg_hashed_msg(data, pk_bin.data(), signature_bin.data());
    }

    auto verify_gpg(
        const std::string& data,
        const std::string& pgp_v4_trailer,
        const std::string& pk,
        const std::string& signature
    ) -> int
    {
        unsigned long long data_len = data.size();
        auto data_bin = reinterpret_cast<const std::byte*>(data.data());

        int error = 0;
        auto signature_bin = ed25519_sig_hex_to_bytes(signature, error);
        if (error)
        {
            return error;
        }
        auto pk_bin = ed25519_key_hex_to_bytes(pk, error);
        if (error)
        {
            return error;
        }

        std::size_t trailer_hex_size = pgp_v4_trailer.size();
        if (trailer_hex_size % 2 != 0)
        {
            LOG_DEBUG << "PGP V4 trailer size is not even: " << pgp_v4_trailer;
            return 0;
        }

        auto pgp_trailer_bin = hex_to_bytes_vec(pgp_v4_trailer, error);
        if (error)
        {
            return error;
        }
        auto final_trailer_bin = hex_to_bytes_arr<2>(std::string_view("04ff"), error);
        assert(!error);

        auto trailer_bin_len_big_endian = static_cast<uint32_t>(pgp_trailer_bin.size());

#ifdef _WIN32
        trailer_bin_len_big_endian = _byteswap_ulong(trailer_bin_len_big_endian);
#else
        trailer_bin_len_big_endian = __builtin_bswap32(trailer_bin_len_big_endian);
#endif

        std::array<std::byte, MAMBA_SHA256_SIZE_BYTES> hash;

        auto digester = util::Sha256Digester();
        digester.digest_start();
        digester.digest_update(data_bin, data_len);
        digester.digest_update(pgp_trailer_bin.data(), pgp_trailer_bin.size());
        digester.digest_update(final_trailer_bin.data(), final_trailer_bin.size());
        digester.digest_update(reinterpret_cast<const std::byte*>(&trailer_bin_len_big_endian), 4);
        digester.digest_finalize_to(hash.data());

        return verify_gpg_hashed_msg(hash.data(), pk_bin.data(), signature_bin.data()) + error;
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

    auto SpecBase::upgrade_prefix() const -> std::vector<std::string>
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
        return ::mamba::util::starts_with(version, compatible_prefix() + ".");
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

        return ::mamba::util::starts_with_any(version, possible_upgrades);
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


    void to_json(nlohmann::json& j, const RoleBase* role)
    {
        j = { { "version", role->version() }, { "expires", role->expires() } };
    }

    void from_json(const nlohmann::json& j, RoleBase* role)
    {
        role->m_version = j.at("version");
        role->set_expiration(j.at(role->spec_version().expiration_json_key()));
    }

    RepoChecker::RepoChecker(Context& context, std::string base_url, fs::u8path ref_path, fs::u8path cache_path)
        : m_base_url(std::move(base_url))
        , m_ref_path(std::move(ref_path))
        , m_cache_path(std::move(cache_path))
        , m_context(context)
    {
    }

    auto RepoChecker::cache_path() -> const fs::u8path&
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

    auto RepoChecker::root_version() -> std::size_t
    {
        return m_root_version;
    }

    auto RepoChecker::ref_root() -> fs::u8path
    {
        return m_ref_path / "root.json";
    }

    auto RepoChecker::cached_root() -> fs::u8path
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

    auto RepoChecker::initial_trusted_root() -> fs::u8path
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

    auto RepoChecker::get_root_role(const TimeRef& time_reference) -> std::unique_ptr<RootRole>
    {
        // TUF spec 5.3 - Update the root role
        // https://theupdateframework.github.io/specification/latest/#update-root

        std::unique_ptr<RootRole> updated_root;

        LOG_DEBUG << "Loading 'root' metadata for repo '" << m_base_url << "'";
        auto trusted_root = initial_trusted_root();

        if (v0_6::SpecImpl().is_compatible(trusted_root))
        {
            updated_root = std::make_unique<v0_6::RootImpl>(trusted_root);
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
