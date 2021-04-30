#include <gtest/gtest.h>

#include "mamba/core/validate.hpp"

#include "sodium/crypto_sign.h"
#include "sodium/utils.h"

#include <algorithm>
#include <cstdio>


namespace validate
{
    namespace testing
    {
        class VerifyMsg : public ::testing::Test
        {
        public:
            VerifyMsg()
            {
                crypto_sign_ed25519_keypair(pk, sk);
                sign("Some text.", sk, signature);
            }

        protected:
            unsigned char pk[crypto_sign_PUBLICKEYBYTES];
            unsigned char sk[crypto_sign_SECRETKEYBYTES];
            unsigned char signature[crypto_sign_BYTES];
        };

        TEST_F(VerifyMsg, from_bytes)
        {
            EXPECT_EQ(verify("Some text.", pk, signature), 0);
        }

        TEST_F(VerifyMsg, from_hex)
        {
            char signature_hex[crypto_sign_BYTES * 2U + 1U];
            sodium_bin2hex(
                signature_hex, crypto_sign_BYTES * 2U + 1U, signature, crypto_sign_BYTES);

            char pk_hex[crypto_sign_PUBLICKEYBYTES * 2U + 1U];
            sodium_bin2hex(
                pk_hex, crypto_sign_PUBLICKEYBYTES * 2U + 1U, pk, crypto_sign_PUBLICKEYBYTES);

            EXPECT_EQ(verify("Some text.", pk_hex, signature_hex), 0);
        }

        class VerifyGPGMsg : public ::testing::Test
        {  // Using test/data/2.root.json from conda-content-trust
        protected:
            std::string pk = "c8bd83b3bfc991face417d97b9c0db011b5d256476b602b92fec92849fc2b36c";
            std::string signature
                = "6e7ad181d4dff9ea462fc076041e19f90a765cba6d6c9c9bad28a8b5094145ea7372c5aca74911daf75e2e5efc8dc9b745bd286c6d9787198659f89bfdb7fd01";
            std::string msg_hash
                = "9ddbb1e859bb431409bcfbfe9dc645e598324907c25a9222dde9c1b547f8bcd6";
        };

        TEST_F(VerifyGPGMsg, from_bin)
        {
            unsigned char bin_signature[crypto_sign_BYTES];
            sodium_hex2bin(bin_signature,
                           crypto_sign_BYTES,
                           signature.c_str(),
                           crypto_sign_BYTES * 2U,
                           NULL,
                           NULL,
                           NULL);

            unsigned char bin_pk[crypto_sign_PUBLICKEYBYTES];
            sodium_hex2bin(bin_pk,
                           crypto_sign_PUBLICKEYBYTES,
                           pk.c_str(),
                           crypto_sign_PUBLICKEYBYTES * 2U,
                           NULL,
                           NULL,
                           NULL);

            EXPECT_EQ(verify_gpg_hashed_msg(msg_hash, bin_pk, bin_signature), 0);
        }

        TEST_F(VerifyGPGMsg, from_hex)
        {  // Using test/data/2.root.json from conda-content-trust
            EXPECT_EQ(verify_gpg_hashed_msg(msg_hash, pk, signature), 0);
        }
    }  // namespace testing
}  // namespace mamba
