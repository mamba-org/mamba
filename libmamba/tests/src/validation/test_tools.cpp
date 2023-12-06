// Copyright (c) 2022, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <doctest/doctest.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "mamba/core/util.hpp"
#include "mamba/util/encoding.hpp"
#include "mamba/validation/tools.hpp"

using namespace mamba;
using namespace mamba::validation;
namespace nl = nlohmann;

template <std::size_t size>
auto
hex_str(const std::array<std::byte, size>& bytes)
{
    return util::bytes_to_hex_str(bytes.data(), bytes.data() + bytes.size());
}

TEST_SUITE("validation::tools")
{
    TEST_CASE("sha256sum")
    {
        auto tmp = TemporaryFile();
        auto f = mamba::open_ofstream(tmp.path());
        f << "test";
        f.close();
        auto sha256 = sha256sum(tmp.path());
        CHECK_EQ(sha256, "9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08");

        auto md5 = md5sum(tmp.path());
        CHECK_EQ(md5, "098f6bcd4621d373cade4e832627b4f6");
    }

    TEST_CASE("ed25519_key_hex_to_bytes")
    {
        std::array<std::byte, MAMBA_ED25519_KEYSIZE_BYTES> pk, sk;
        generate_ed25519_keypair(pk.data(), sk.data());

        auto pk_hex = hex_str(pk);
        int error = 0;
        auto pk_bytes = ed25519_key_hex_to_bytes(pk_hex, error);
        REQUIRE_EQ(error, 0);
        CHECK_EQ(pk_hex, hex_str(pk_bytes));

        spdlog::set_level(spdlog::level::debug);

        std::array<std::byte, 5> not_even_key;
        pk_hex = hex_str(not_even_key);
        pk_bytes = ed25519_key_hex_to_bytes(pk_hex, error);
        REQUIRE_EQ(error, 0);
        CHECK_FALSE(pk_hex == hex_str(pk_bytes));

        std::array<std::byte, 6> wrong_size_key;
        pk_hex = hex_str(wrong_size_key);
        pk_bytes = ed25519_key_hex_to_bytes(pk_hex, error);
        REQUIRE_EQ(error, 0);
        CHECK_FALSE(pk_hex == hex_str(pk_bytes));

        spdlog::set_level(spdlog::level::info);
    }

    TEST_CASE("ed25519_sig_hex_to_bytes")
    {
        std::array<std::byte, MAMBA_ED25519_KEYSIZE_BYTES> pk, sk;
        generate_ed25519_keypair(pk.data(), sk.data());

        std::array<std::byte, MAMBA_ED25519_SIGSIZE_BYTES> sig;
        sign("Some text.", sk.data(), sig.data());

        int error = 0;
        auto sig_hex = hex_str(sig);
        auto sig_bytes = ed25519_sig_hex_to_bytes(sig_hex, error);
        REQUIRE_EQ(error, 0);
        CHECK_EQ(sig_hex, hex_str(sig_bytes));

        spdlog::set_level(spdlog::level::debug);

        std::array<std::byte, 5> not_even_sig;
        sig_hex = hex_str(not_even_sig);
        sig_bytes = ed25519_sig_hex_to_bytes(sig_hex, error);
        REQUIRE_EQ(error, 0);
        CHECK_FALSE(sig_hex == hex_str(sig_bytes));

        std::array<std::byte, 6> wrong_size_sig;
        sig_hex = hex_str(wrong_size_sig);
        sig_bytes = ed25519_sig_hex_to_bytes(sig_hex, error);
        REQUIRE_EQ(error, 0);
        CHECK_FALSE(sig_hex == hex_str(sig_bytes));

        spdlog::set_level(spdlog::level::info);
    }
}

class VerifyMsg
{
public:

    VerifyMsg()
    {
        generate_ed25519_keypair(pk.data(), sk.data());
        sign("Some text.", sk.data(), signature.data());
    }

protected:

    std::array<std::byte, MAMBA_ED25519_KEYSIZE_BYTES> pk;
    std::array<std::byte, MAMBA_ED25519_KEYSIZE_BYTES> sk;
    std::array<std::byte, MAMBA_ED25519_SIGSIZE_BYTES> signature;
};

TEST_SUITE("validation::VerifyMsg")
{
    TEST_CASE_FIXTURE(VerifyMsg, "from_bytes")
    {
        CHECK_EQ(verify("Some text.", pk.data(), signature.data()), 1);
    }

    TEST_CASE_FIXTURE(VerifyMsg, "from_hex")
    {
        auto signature_hex = hex_str(signature);
        auto pk_hex = hex_str(pk);

        CHECK_EQ(verify("Some text.", pk_hex, signature_hex), 1);
    }

    TEST_CASE_FIXTURE(VerifyMsg, "wrong_signature")
    {
        spdlog::set_level(spdlog::level::debug);
        auto pk_hex = hex_str(pk);

        CHECK_EQ(verify("Some text.", pk_hex, "signature_hex"), 0);
        spdlog::set_level(spdlog::level::info);
    }

    TEST_CASE_FIXTURE(VerifyMsg, "wrong_public_key")
    {
        spdlog::set_level(spdlog::level::debug);
        auto signature_hex = hex_str(signature);

        CHECK_EQ(verify("Some text.", "pk_hex", signature_hex), 0);
        spdlog::set_level(spdlog::level::info);
    }
}

class VerifyGPGMsg
{
public:

    VerifyGPGMsg()
    {
        nl::json j = R"({
                            "delegations": {
                            "key_mgr": {
                                "pubkeys": [
                                "013ddd714962866d12ba5bae273f14d48c89cf0773dee2dbf6d4561e521c83f7"
                                ],
                                "threshold": 1
                            },
                            "root": {
                                "pubkeys": [
                                "2b920f88531576643ada0a632915d1dcdd377557647093f29cbe251ba8c33724"
                                ],
                                "threshold": 1
                            }
                            },
                            "expiration": "2022-05-19T14:44:35Z",
                            "metadata_spec_version": "0.6.0",
                            "timestamp": "2021-05-19T14:44:35Z",
                            "type": "root",
                            "version": 1
                        })"_json;
        data = j.dump(2);
    }

protected:

    std::string pk = "2b920f88531576643ada0a632915d1dcdd377557647093f29cbe251ba8c33724";
    std::string signature = "d891de3fc102a2ff7b96559ff2f4d81a8e25b5d51a44e10a9fbc5bdc3febf22120582f30e26f6dfe9450ca8100566af7cbc286bf7f52c700d074acd3d4a01603";
    std::string trailer = "04001608001d1621040673d781a8b80bcb7b002040ac7bc8bcf821360d050260a52453";
    std::string hash = "5ad6a0995a537a5fc728ead2dda546972607c5ac235945f7c6c66f90eae1b326";
    std::string data;
};

TEST_SUITE("validation::VerifyGPGMsg")
{
    TEST_CASE_FIXTURE(VerifyGPGMsg, "verify_gpg_hashed_msg_from_bin")
    {
        int error = 0;
        auto bin_signature = ed25519_sig_hex_to_bytes(signature, error);
        REQUIRE_EQ(error, 0);
        auto bin_pk = ed25519_key_hex_to_bytes(pk, error);
        REQUIRE_EQ(error, 0);

        CHECK_EQ(verify_gpg_hashed_msg(hash, bin_pk.data(), bin_signature.data()), 1);
    }

    TEST_CASE_FIXTURE(VerifyGPGMsg, "verify_gpg_hashed_msg_from_hex")
    {
        CHECK_EQ(verify_gpg_hashed_msg(hash, pk, signature), 1);
    }

    TEST_CASE_FIXTURE(VerifyGPGMsg, "verify_gpg")
    {
        CHECK_EQ(verify_gpg(data, trailer, pk, signature), 1);
    }
}
