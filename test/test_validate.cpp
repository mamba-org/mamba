#include <gtest/gtest.h>

#include "mamba/core/validate.hpp"

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
                generate_ed25519_keypair(pk, sk);
                sign("Some text.", sk, signature);
            }

        protected:
            unsigned char pk[MAMBA_ED25519_KEYSIZE_BYTES];
            unsigned char sk[MAMBA_ED25519_KEYSIZE_BYTES];
            unsigned char signature[MAMBA_ED25519_SIGSIZE_BYTES];
        };

        TEST_F(VerifyMsg, from_bytes)
        {
            EXPECT_EQ(verify("Some text.", pk, signature), 1);
        }

        TEST_F(VerifyMsg, from_hex)
        {
            char signature_hex[MAMBA_ED25519_SIGSIZE_HEX];
            bin2hex(
                signature_hex, MAMBA_ED25519_SIGSIZE_HEX, signature, MAMBA_ED25519_SIGSIZE_BYTES);

            char pk_hex[MAMBA_ED25519_KEYSIZE_HEX];
            bin2hex(pk_hex, MAMBA_ED25519_KEYSIZE_HEX, pk, MAMBA_ED25519_KEYSIZE_BYTES);

            EXPECT_EQ(verify("Some text.", pk_hex, signature_hex), 1);
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

            EXPECT_EQ(verify_gpg_hashed_msg(msg_hash, bin_pk, bin_signature), 1);
        }

        TEST_F(VerifyGPGMsg, from_hex)
        {  // Using test/data/2.root.json from conda-content-trust
            EXPECT_EQ(verify_gpg_hashed_msg(msg_hash, pk, signature), 1);
        }
    }  // namespace testing
}  // namespace mamba
