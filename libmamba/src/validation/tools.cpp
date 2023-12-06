// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <regex>
#include <utility>

#include <openssl/evp.h>

#include "mamba/core/output.hpp"
#include "mamba/core/util.hpp"
#include "mamba/fs/filesystem.hpp"
#include "mamba/util/cryptography.hpp"
#include "mamba/validation/errors.hpp"
#include "mamba/validation/tools.hpp"

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
        ::EVP_PKEY* pkey = nullptr;
        struct EVPContext
        {
            ::EVP_PKEY_CTX* pctx = ::EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, nullptr);

            ~EVPContext()
            {
                ::EVP_PKEY_CTX_free(pctx);
            }

        } evp_context;


        int gen_status = ::EVP_PKEY_keygen_init(evp_context.pctx);
        if (gen_status != 1)
        {
            LOG_DEBUG << "Failed to initialize ED25519 key pair generation";
            return gen_status;
        }

        gen_status = ::EVP_PKEY_keygen(evp_context.pctx, &pkey);
        if (gen_status != 1)
        {
            LOG_DEBUG << "Failed to generate ED25519 key pair";
            return gen_status;
        }

        int storage_status = ::EVP_PKEY_get_raw_public_key(
            pkey,
            reinterpret_cast<unsigned char*>(pk),
            &key_len
        );
        if (storage_status != 1)
        {
            LOG_DEBUG << "Failed to store public key of generated ED25519 key pair";
            return storage_status;
        }
        storage_status = ::EVP_PKEY_get_raw_private_key(
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
        ::EVP_PKEY* ed_key = ::EVP_PKEY_new_raw_private_key(
            EVP_PKEY_ED25519,
            nullptr,
            reinterpret_cast<const unsigned char*>(sk),
            MAMBA_ED25519_KEYSIZE_BYTES
        );
        ::EVP_MD_CTX* md_ctx = ::EVP_MD_CTX_new();

        if (ed_key == nullptr)
        {
            LOG_DEBUG << "Failed to read secret key raw buffer during signing step";
            return 0;
        }

        int init_status, sign_status;
        init_status = ::EVP_DigestSignInit(md_ctx, nullptr, nullptr, nullptr, ed_key);
        if (init_status != 1)
        {
            LOG_DEBUG << "Failed to init signing step";
            return init_status;
        }

        std::size_t sig_len = MAMBA_ED25519_SIGSIZE_BYTES;
        sign_status = ::EVP_DigestSign(
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

        ::EVP_MD_CTX_free(md_ctx);
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
        ::EVP_PKEY* ed_key = ::EVP_PKEY_new_raw_public_key(
            EVP_PKEY_ED25519,
            nullptr,
            reinterpret_cast<const unsigned char*>(pk),
            MAMBA_ED25519_KEYSIZE_BYTES
        );
        ::EVP_MD_CTX* md_ctx = ::EVP_MD_CTX_new();

        if (ed_key == nullptr)
        {
            LOG_DEBUG << "Failed to read public key raw buffer during verification step";
            return 0;
        }

        int init_status, verif_status;
        init_status = ::EVP_DigestVerifyInit(md_ctx, nullptr, nullptr, nullptr, ed_key);
        if (init_status != 1)
        {
            LOG_DEBUG << "Failed to init verification step";
            return init_status;
        }

        std::size_t sig_len = MAMBA_ED25519_SIGSIZE_BYTES;
        verif_status = ::EVP_DigestVerify(
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

        ::EVP_MD_CTX_free(md_ctx);
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
}
