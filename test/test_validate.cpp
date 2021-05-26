#include <gtest/gtest.h>

#include "mamba/core/validate.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/util.hpp"

#include <nlohmann/json.hpp>

#include <openssl/sha.h>

#include <algorithm>
#include <cstdio>
#include <map>


namespace validate
{
    namespace testing
    {
        using nlohmann::json;

        TEST(Validate, ed25519_key_hex_to_bytes)
        {
            std::array<unsigned char, MAMBA_ED25519_KEYSIZE_BYTES> pk, sk;
            generate_ed25519_keypair(pk.data(), sk.data());

            auto pk_hex = ::mamba::hex_string(pk);
            auto pk_bytes = ed25519_key_hex_to_bytes(pk_hex);
            EXPECT_EQ(pk_hex, ::mamba::hex_string(pk_bytes));

            mamba::MessageLogger::global_log_severity() = mamba::LogSeverity::kDebug;

            std::array<unsigned char, 5> not_even_key;
            pk_hex = ::mamba::hex_string(not_even_key);
            pk_bytes = ed25519_key_hex_to_bytes(pk_hex);
            EXPECT_FALSE(pk_hex == ::mamba::hex_string(pk_bytes));

            std::array<unsigned char, 6> wrong_size_key;
            pk_hex = ::mamba::hex_string(wrong_size_key);
            pk_bytes = ed25519_key_hex_to_bytes(pk_hex);
            EXPECT_FALSE(pk_hex == ::mamba::hex_string(pk_bytes));

            mamba::MessageLogger::global_log_severity() = mamba::LogSeverity::kInfo;
        }

        TEST(Validate, ed25519_sig_hex_to_bytes)
        {
            std::array<unsigned char, MAMBA_ED25519_KEYSIZE_BYTES> pk, sk;
            generate_ed25519_keypair(pk.data(), sk.data());

            std::array<unsigned char, MAMBA_ED25519_SIGSIZE_BYTES> sig;
            sign("Some text.", sk.data(), sig.data());

            auto sig_hex = ::mamba::hex_string(sig);
            auto sig_bytes = ed25519_sig_hex_to_bytes(sig_hex);
            EXPECT_EQ(sig_hex, ::mamba::hex_string(sig_bytes));

            mamba::MessageLogger::global_log_severity() = mamba::LogSeverity::kDebug;

            std::array<unsigned char, 5> not_even_sig;
            sig_hex = ::mamba::hex_string(not_even_sig);
            sig_bytes = ed25519_sig_hex_to_bytes(sig_hex);
            EXPECT_FALSE(sig_hex == ::mamba::hex_string(sig_bytes));

            std::array<unsigned char, 6> wrong_size_sig;
            sig_hex = ::mamba::hex_string(wrong_size_sig);
            sig_bytes = ed25519_sig_hex_to_bytes(sig_hex);
            EXPECT_FALSE(sig_hex == ::mamba::hex_string(sig_bytes));

            mamba::MessageLogger::global_log_severity() = mamba::LogSeverity::kInfo;
        }


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
            auto signature_hex = ::mamba::hex_string(signature, MAMBA_ED25519_SIGSIZE_BYTES);
            auto pk_hex = ::mamba::hex_string(pk, MAMBA_ED25519_KEYSIZE_BYTES);

            EXPECT_EQ(verify("Some text.", pk_hex, signature_hex), 1);
        }

        TEST_F(VerifyMsg, wrong_signature)
        {
            mamba::MessageLogger::global_log_severity() = mamba::LogSeverity::kDebug;
            auto pk_hex = ::mamba::hex_string(pk, MAMBA_ED25519_KEYSIZE_BYTES);

            EXPECT_EQ(verify("Some text.", pk_hex, "signature_hex"), 0);
            mamba::MessageLogger::global_log_severity() = mamba::LogSeverity::kInfo;
        }

        TEST_F(VerifyMsg, wrong_public_key)
        {
            mamba::MessageLogger::global_log_severity() = mamba::LogSeverity::kDebug;
            auto signature_hex = ::mamba::hex_string(signature, MAMBA_ED25519_SIGSIZE_BYTES);

            EXPECT_EQ(verify("Some text.", "pk_hex", signature_hex), 0);
            mamba::MessageLogger::global_log_severity() = mamba::LogSeverity::kInfo;
        }

        class VerifyGPGMsg : public ::testing::Test
        {
        public:
            VerifyGPGMsg()
            {
                json j = R"({
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
            std::string signature
                = "d891de3fc102a2ff7b96559ff2f4d81a8e25b5d51a44e10a9fbc5bdc3febf22120582f30e26f6dfe9450ca8100566af7cbc286bf7f52c700d074acd3d4a01603";
            std::string trailer
                = "04001608001d1621040673d781a8b80bcb7b002040ac7bc8bcf821360d050260a52453";
            std::string hash = "5ad6a0995a537a5fc728ead2dda546972607c5ac235945f7c6c66f90eae1b326";
            std::string data;
        };

        TEST_F(VerifyGPGMsg, verify_gpg_hashed_msg_from_bin)
        {
            auto bin_signature = ed25519_sig_hex_to_bytes(signature);
            auto bin_pk = ed25519_key_hex_to_bytes(pk);

            EXPECT_EQ(verify_gpg_hashed_msg(hash, bin_pk.data(), bin_signature.data()), 1);
        }

        TEST_F(VerifyGPGMsg, verify_gpg_hashed_msg_from_hex)
        {
            EXPECT_EQ(verify_gpg_hashed_msg(hash, pk, signature), 1);
        }

        TEST_F(VerifyGPGMsg, verify_gpg)
        {
            EXPECT_EQ(verify_gpg(data, trailer, pk, signature), 1);
        }
    }  // namespace testing

    namespace v06
    {
        namespace testing
        {
            using namespace mamba;

            class RootRoleV06 : public ::testing::Test
            {
            public:
                using role_secrets_type
                    = std::map<std::string, std::array<unsigned char, MAMBA_ED25519_KEYSIZE_BYTES>>;
                using secrets_type = std::map<std::string, role_secrets_type>;

                RootRoleV06()
                {
                    channel_dir = std::make_unique<TemporaryDirectory>();

                    generate_secrets();
                    sign_root();
                }

                fs::path trusted_root_file(const json& j)
                {
                    fs::path p = channel_dir->path() / "root.json";

                    std::ofstream out_file(p, std::ofstream::out | std::ofstream::trunc);
                    out_file << j;
                    out_file.close();

                    return p;
                }

                fs::path trusted_root_file_raw_key()
                {
                    return trusted_root_file(root1_json);
                }

                fs::path trusted_root_file_pgp()
                {
                    return trusted_root_file(root1_pgp_json);
                }

                fs::path create_test_update(const fs::path& name, const json& patch = json())
                {
                    fs::path p = channel_dir->path() / name;

                    std::ofstream out_file(p, std::ofstream::out | std::ofstream::trunc);

                    json new_root = root1_json;

                    if (!patch.empty())
                        new_root = new_root.patch(patch);

                    json sig_patch
                        = json::parse(R"([
                                        { "op": "replace", "path": "/signatures", "value":)"
                                      + sign_root_meta(new_root.at("signed")).dump() + R"( }
                                        ])");
                    out_file << new_root.patch(sig_patch);
                    out_file.close();

                    return p;
                }

                void generate_secrets(int root = 1, int key_mgr = 1, int pkg_mgr = 1)
                {
                    secrets.insert({ "root", generate_role_secrets(root) });
                    secrets.insert({ "key_mgr", generate_role_secrets(key_mgr) });
                    secrets.insert({ "pkg_mgr", generate_role_secrets(pkg_mgr) });
                }

                void sign_root()
                {
                    std::vector<std::string> mandatory_roles({ "root", "key_mgr" });
                    for (auto& r : mandatory_roles)
                    {
                        std::vector<std::string> role_public_keys;
                        for (auto& secret : secrets.at(r))
                        {
                            role_public_keys.push_back(secret.first);
                        }
                        root1_json["signed"]["delegations"][r]
                            = RolePubKeys({ role_public_keys, 1 });
                    }

                    root1_json["signed"]["version"] = 1;
                    root1_json["signed"]["metadata_spec_version"] = "0.6.0";
                    root1_json["signed"]["type"] = "root";
                    root1_json["signed"]["expiration"] = timestamp(utc_time_now() + 3600);
                    root1_json["signatures"] = sign_root_meta(root1_json["signed"]);

                    std::ifstream i(root1_pgp);
                    i >> root1_pgp_json;
                }

                json sign_root_meta(const json& root_meta)
                {
                    std::map<std::string, std::map<std::string, std::string>> signatures;

                    unsigned char sig_bin[MAMBA_ED25519_SIGSIZE_BYTES];

                    for (auto& secret : secrets.at("root"))
                    {
                        sign(root_meta.dump(2), secret.second.data(), sig_bin);

                        auto sig_hex = ::mamba::hex_string(sig_bin, MAMBA_ED25519_SIGSIZE_BYTES);
                        signatures[secret.first].insert({ "signature", sig_hex });
                    }

                    return signatures;
                }

                json upgrade_to_v1(const RootImpl& root, const json& patch = json())
                {
                    auto root_meta = root.upgraded_signable();
                    if (!patch.empty())
                        root_meta = root_meta.patch(patch);

                    std::vector<RoleSignature> signatures;
                    for (auto& secret : secrets.at("root"))
                    {
                        signatures.push_back(
                            root.upgraded_signature(root_meta, secret.first, secret.second.data()));
                    }

                    json upgraded_root;
                    upgraded_root["signed"] = root_meta;
                    upgraded_root["signatures"] = signatures;

                    return upgraded_root;
                }

            protected:
                fs::path root1_pgp = "validation_data/1.sv0.6.root.json";
                json root1_json, root1_pgp_json;

                secrets_type secrets;

                std::unique_ptr<TemporaryDirectory> channel_dir;

                std::map<std::string, std::array<unsigned char, MAMBA_ED25519_KEYSIZE_BYTES>>
                generate_role_secrets(int count)
                {
                    std::map<std::string, std::array<unsigned char, MAMBA_ED25519_KEYSIZE_BYTES>>
                        secrets;

                    unsigned char pk[MAMBA_ED25519_KEYSIZE_BYTES];
                    std::array<unsigned char, MAMBA_ED25519_KEYSIZE_BYTES> sk;

                    for (int i = 0; i < count; ++i)
                    {
                        generate_ed25519_keypair(pk, sk.data());
                        auto pk_hex = ::mamba::hex_string(pk, MAMBA_ED25519_KEYSIZE_BYTES);

                        secrets.insert({ pk_hex, sk });
                    }
                    return secrets;
                }
            };

            TEST_F(RootRoleV06, ctor_from_path)
            {
                RootImpl root(trusted_root_file_raw_key());

                EXPECT_EQ(root.type(), "root");
                EXPECT_EQ(root.file_ext(), "json");
                EXPECT_EQ(root.spec_version(), SpecImpl("0.6.0"));
                EXPECT_EQ(root.version(), 1);
            }

            TEST_F(RootRoleV06, ctor_from_path_pgp_signed)
            {
                RootImpl root(trusted_root_file_pgp());

                EXPECT_EQ(root.type(), "root");
                EXPECT_EQ(root.file_ext(), "json");
                EXPECT_EQ(root.spec_version(), SpecImpl("0.6.0"));
                EXPECT_EQ(root.version(), 1);
            }

            TEST_F(RootRoleV06, ctor_from_json)
            {
                RootImpl root(root1_json);

                EXPECT_EQ(root.type(), "root");
                EXPECT_EQ(root.file_ext(), "json");
                EXPECT_EQ(root.spec_version(), SpecImpl("0.6.0"));
                EXPECT_EQ(root.version(), 1);
            }

            TEST_F(RootRoleV06, ctor_from_json_pgp_signed)
            {
                RootImpl root(root1_pgp_json);

                EXPECT_EQ(root.type(), "root");
                EXPECT_EQ(root.file_ext(), "json");
                EXPECT_EQ(root.spec_version(), SpecImpl("0.6.0"));
                EXPECT_EQ(root.version(), 1);
            }

            TEST_F(RootRoleV06, ctor_wrong_filename_spec_version)
            {
                fs::path p = channel_dir->path() / "2.sv1.root.json";

                std::ofstream out_file(p, std::ofstream::out | std::ofstream::trunc);
                out_file << root1_json;
                out_file.close();

                // "2.sv1.root.json" is not compatible spec version (spec version N)
                EXPECT_THROW(RootImpl root(p), role_file_error);
            }

            TEST_F(RootRoleV06, update_from_path)
            {
                using namespace mamba;

                auto f = trusted_root_file_raw_key();
                RootImpl root(f);

                json patch = R"([
                    { "op": "replace", "path": "/signed/version", "value": 2 }
                    ])"_json;
                auto updated_root = root.update(create_test_update("2.root.json", patch));

                auto testing_root = static_cast<RootImpl*>(updated_root.get());
                EXPECT_EQ(testing_root->type(), "root");
                EXPECT_EQ(testing_root->file_ext(), "json");
                EXPECT_EQ(testing_root->spec_version(), SpecImpl("0.6.0"));
                EXPECT_EQ(testing_root->version(), 2);
            }

            TEST_F(RootRoleV06, wrong_version)
            {
                RootImpl root(root1_json);

                json patch = R"([
                    { "op": "replace", "path": "/signed/version", "value": 3 }
                    ])"_json;

                EXPECT_THROW(root.update(create_test_update("2.root.json", patch)),
                             role_metadata_error);
            }

            TEST_F(RootRoleV06, spec_version)
            {
                RootImpl root(root1_json);

                json patch = R"([
                    { "op": "replace", "path": "/signed/version", "value": 2 },
                    { "op": "replace", "path": "/signed/metadata_spec_version", "value": "0.6.1" }
                    ])"_json;
                auto updated_root = root.update(create_test_update("2.root.json", patch));

                auto testing_root = static_cast<RootImpl*>(updated_root.get());
                EXPECT_EQ(testing_root->spec_version(), SpecImpl("0.6.1"));
                EXPECT_EQ(testing_root->version(), 2);
                EXPECT_EQ(testing_root->expires(), root.expires());
            }

            TEST_F(RootRoleV06, upgraded_spec_version)
            {
                RootImpl root(root1_json);

                json patch = R"([
                    { "op": "replace", "path": "/signed/version", "value": 2 },
                    { "op": "replace", "path": "/signed/metadata_spec_version", "value": "1.0.0" }
                    ])"_json;

                EXPECT_THROW(root.update(create_test_update("2.root.json", patch)),
                             spec_version_error);

                json signable_patch
                    = json::parse(R"([
                    { "op": "replace", "path": "/version", "value": 2 },
                    { "op": "replace", "path": "/expires", "value": ")"
                                  + timestamp(utc_time_now() + 1) /* force +1s */ + R"(" },
                    { "op": "add", "path": "/keys/dummy_value", "value": { "keytype": "ed25519", "scheme": "ed25519", "keyval": "dummy_value" } },
                    { "op": "add", "path": "/roles/snapshot/keyids", "value": ["dummy_value"] },
                    { "op": "add", "path": "/roles/timestamp/keyids", "value": ["dummy_value"] }
                    ])");
                auto updated_root = root.update(upgrade_to_v1(root, signable_patch));

                auto testing_root = static_cast<RootImpl*>(updated_root.get());
                EXPECT_EQ(testing_root->spec_version(), SpecImpl("1.0.17"));
                EXPECT_EQ(testing_root->version(), 2);
                EXPECT_LT(testing_root->expires(), root.expires());
            }

            TEST_F(RootRoleV06, equivalent_upgraded_spec_version)
            {
                RootImpl root(root1_json);

                json signable_patch = R"([
                    { "op": "add", "path": "/keys/dummy_value", "value": { "keytype": "ed25519", "scheme": "ed25519", "keyval": "dummy_value" } },
                    { "op": "add", "path": "/roles/snapshot/keyids", "value": ["dummy_value"] },
                    { "op": "add", "path": "/roles/timestamp/keyids", "value": ["dummy_value"] }
                    ])"_json;
                v1::RootImpl updated_root(upgrade_to_v1(root, signable_patch));

                EXPECT_EQ(updated_root.spec_version(), v1::SpecImpl("1.0.17"));
                EXPECT_EQ(updated_root.version(), 1);
            }

            TEST_F(RootRoleV06, wrong_spec_version)
            {
                RootImpl root(root1_json);

                json patch = R"([
                    { "op": "replace", "path": "/signed/version", "value": 2 },
                    { "op": "replace", "path": "/signed/metadata_spec_version", "value": "1.0.0" }
                    ])"_json;

                EXPECT_THROW(root.update(create_test_update("2.root.json", patch)),
                             spec_version_error);

                patch = R"([
                    { "op": "replace", "path": "/signed/version", "value": 2 },
                    { "op": "replace", "path": "/signed/metadata_spec_version", "value": "wrong" }
                    ])"_json;

                EXPECT_THROW(root.update(create_test_update("2.root.json", patch)),
                             spec_version_error);
            }

            TEST_F(RootRoleV06, wrong_filename_role)
            {
                RootImpl root(root1_json);

                EXPECT_THROW(root.update(create_test_update("2.rooot.json")), role_file_error);
            }

            TEST_F(RootRoleV06, wrong_filename_version)
            {
                RootImpl root(root1_json);

                EXPECT_THROW(root.update(create_test_update("3.root.json")), role_file_error);
            }

            TEST_F(RootRoleV06, wrong_filename_spec_version)
            {
                RootImpl root(root1_json);

                // "2.sv1.root.json" is upgradable spec version (spec version N+1)
                EXPECT_THROW(root.update(create_test_update("3.sv1.root.json")), role_file_error);
            }

            TEST_F(RootRoleV06, hillformed_filename_version)
            {
                RootImpl root(root1_json);

                EXPECT_THROW(root.update(create_test_update("wrong.root.json")), role_file_error);
                EXPECT_THROW(root.update(create_test_update("2..root.json")), role_file_error);
                EXPECT_THROW(root.update(create_test_update("2.sv04.root.json")), role_file_error);
            }

            TEST_F(RootRoleV06, rollback_attack)
            {
                RootImpl root(root1_json);

                json patch = R"([
                    { "op": "replace", "path": "/signed/version", "value": 1 }
                    ])"_json;

                EXPECT_THROW(root.update(create_test_update("2.root.json", patch)), rollback_error);
            }

            TEST_F(RootRoleV06, wrong_type)
            {
                RootImpl root(root1_json);

                json patch = R"([
                    { "op": "replace", "path": "/signed/type", "value": "timestamp" },
                    { "op": "replace", "path": "/signed/version", "value": 2 }
                    ])"_json;

                EXPECT_THROW(root.update(create_test_update("2.root.json", patch)),
                             role_metadata_error);
            }

            TEST_F(RootRoleV06, missing_type)
            {
                RootImpl root(root1_json);

                json patch = R"([
                    { "op": "remove", "path": "/signed/type" },
                    { "op": "replace", "path": "/signed/version", "value": 2 }
                    ])"_json;

                EXPECT_THROW(root.update(create_test_update("2.root.json", patch)),
                             role_metadata_error);
            }

            TEST_F(RootRoleV06, missing_delegations)
            {
                RootImpl root(root1_json);

                json patch = R"([
                    { "op": "remove", "path": "/signed/delegations" },
                    { "op": "replace", "path": "/signed/version", "value": 2 }
                    ])"_json;

                EXPECT_THROW(root.update(create_test_update("2.root.json", patch)),
                             role_metadata_error);
            }

            TEST_F(RootRoleV06, missing_delegation)
            {
                RootImpl root(root1_json);

                json patch = R"([
                                { "op": "remove", "path": "/signed/delegations/root" },
                                { "op": "replace", "path": "/signed/version", "value": 2 }
                                ])"_json;

                EXPECT_THROW(root.update(create_test_update("2.root.json", patch)),
                             role_metadata_error);
            }

            TEST_F(RootRoleV06, empty_delegation_pubkeys)
            {
                RootImpl root(root1_json);

                json patch = R"([
                                { "op": "replace", "path": "/signed/delegations/root/pubkeys", "value": [] },
                                { "op": "replace", "path": "/signed/version", "value": 2 }
                                ])"_json;

                EXPECT_THROW(root.update(create_test_update("2.root.json", patch)),
                             role_metadata_error);
            }

            TEST_F(RootRoleV06, null_role_threshold)
            {
                RootImpl root(root1_json);

                json patch = R"([
                                { "op": "replace", "path": "/signed/delegations/root/threshold", "value": 0 },
                                { "op": "replace", "path": "/signed/version", "value": 2 }
                                ])"_json;

                EXPECT_THROW(root.update(create_test_update("2.root.json", patch)),
                             role_metadata_error);
            }

            TEST_F(RootRoleV06, extra_roles)
            {
                RootImpl root(root1_json);

                json patch = R"([
                                { "op": "add", "path": "/signed/delegations/some_wrong_role",
                                    "value": { "pubkeys": ["c"], "threshold": 1 } },
                                { "op": "replace", "path": "/signed/version", "value": 2 }
                                ])"_json;

                EXPECT_THROW(root.update(create_test_update("2.root.json", patch)),
                             role_metadata_error);
            }
            /*
            TEST_F(RootRoleV06, mirrors_role)
            {
                json patch = R"([
                                                        { "op": "add", "path":
               "/signed/roles/mirrors", "value": { "keyids":
                                       ["c"], "threshold": 1 } }, { "op": "replace", "path":
               "/signed/version", "value": 2 }
                                                        ])"_json;

                RootImpl root(create_test_update("2.root.json", patch));
                bool mirrors_role_found = (root.roles().find("mirrors") != root.roles().end());
                EXPECT_TRUE(mirrors_role_found);
            }
            */
            TEST_F(RootRoleV06, threshold_not_met)
            {
                RootImpl root(root1_json);

                json patch = R"([
                    { "op": "replace", "path": "/signed/version", "value": 2 },
                    { "op": "replace", "path": "/signed/delegations/root/threshold", "value": 2 }
                    ])"_json;

                EXPECT_THROW(root.update(create_test_update("2.root.json", patch)),
                             threshold_error);
            }

            TEST_F(RootRoleV06, expires)
            {
                RootImpl root(root1_json);

                // expiration is set to now+3600s in 'sign_root'
                TimeRef::instance().set(utc_time_now());
                EXPECT_FALSE(root.expired());

                TimeRef::instance().set(utc_time_now() + 7200);
                EXPECT_TRUE(root.expired());

                json patch = json::parse(R"([
                    { "op": "replace", "path": "/signed/expiration", "value": ")"
                                         + timestamp(utc_time_now() + 10800) + R"(" },
                    { "op": "replace", "path": "/signed/version", "value": 2 }
                    ])");
                auto updated_root = root.update(create_test_update("2.root.json", patch));

                auto testing_root = static_cast<RootImpl*>(updated_root.get());
                EXPECT_FALSE(testing_root->expired());
            }

            class KeyMgr : public RootRoleV06
            {
            public:
                KeyMgr()
                    : RootRoleV06()
                {
                    sign_key_mgr();
                }

                void sign_key_mgr()
                {
                    std::vector<std::string> pkg_mgr_pks;
                    for (auto& secret : secrets.at("pkg_mgr"))
                    {
                        pkg_mgr_pks.push_back(secret.first);
                    }
                    key_mgr_json["signed"]["delegations"]["pkg_mgr"]
                        = RolePubKeys({ pkg_mgr_pks, 1 });

                    key_mgr_json["signed"]["version"] = 1;
                    key_mgr_json["signed"]["metadata_spec_version"] = "0.6.0";
                    key_mgr_json["signed"]["type"] = "key_mgr";

                    key_mgr_json["signed"]["expiration"] = timestamp(utc_time_now() + 3600);
                    key_mgr_json["signatures"] = sign_key_mgr_meta(key_mgr_json["signed"]);

                    std::ifstream i(root1_pgp);
                    i >> root1_pgp_json;
                }

                json sign_patched(const json& patch = json())
                {
                    json update_key_mgr = key_mgr_json;

                    if (!patch.empty())
                        update_key_mgr = update_key_mgr.patch(patch);

                    json sig_patch = json::parse(
                        R"([
                            { "op": "replace", "path": "/signatures", "value": )"
                        + sign_key_mgr_meta(update_key_mgr.at("signed")).dump() + R"( }
                            ])");
                    return update_key_mgr.patch(sig_patch);
                }

                fs::path trusted_file(const json& j, const std::string& filename = "key_mgr.json")
                {
                    fs::path p = channel_dir->path() / filename;

                    std::ofstream out_file(p, std::ofstream::out | std::ofstream::trunc);
                    out_file << j;
                    out_file.close();

                    return p;
                }

            protected:
                json key_mgr_json;

                json sign_key_mgr_meta(const json& meta)
                {
                    std::map<std::string, std::map<std::string, std::string>> signatures;

                    unsigned char sig_bin[MAMBA_ED25519_SIGSIZE_BYTES];

                    for (auto& secret : secrets.at("key_mgr"))
                    {
                        sign(meta.dump(2), secret.second.data(), sig_bin);

                        auto sig_hex = ::mamba::hex_string(sig_bin, MAMBA_ED25519_SIGSIZE_BYTES);
                        signatures[secret.first].insert({ "signature", sig_hex });
                    }

                    return signatures;
                }
            };

            TEST_F(KeyMgr, ctor_from_json)
            {
                RootImpl root(root1_json);
                auto key_mgr = root.create_key_mgr(key_mgr_json);

                EXPECT_EQ(key_mgr.spec_version(), SpecImpl("0.6.0"));
                EXPECT_EQ(key_mgr.version(), 1);
            }

            TEST_F(KeyMgr, version)
            {
                RootImpl root(root1_json);

                {
                    json key_mgr_patch = R"([
                    { "op": "replace", "path": "/signed/version", "value": 2 }
                    ])"_json;
                    auto key_mgr = root.create_key_mgr(sign_patched(key_mgr_patch));

                    EXPECT_EQ(key_mgr.spec_version(), SpecImpl("0.6.0"));
                    EXPECT_EQ(key_mgr.version(), 2);
                }

                {  // Any version is valid, without chaining required
                    json key_mgr_patch = R"([
                    { "op": "replace", "path": "/signed/version", "value": 20 }
                    ])"_json;
                    auto key_mgr = root.create_key_mgr(sign_patched(key_mgr_patch));

                    EXPECT_EQ(key_mgr.spec_version(), SpecImpl("0.6.0"));
                    EXPECT_EQ(key_mgr.version(), 20);
                }
            }

            TEST_F(KeyMgr, spec_version)
            {  // spec version as to match exactly 'root' spec version
                RootImpl root(root1_json);

                {
                    json key_mgr_patch = R"([
                    { "op": "replace", "path": "/signed/metadata_spec_version", "value": "0.6.0" }
                    ])"_json;
                    auto key_mgr = root.create_key_mgr(sign_patched(key_mgr_patch));

                    EXPECT_EQ(key_mgr.spec_version(), SpecImpl("0.6.0"));
                    EXPECT_EQ(key_mgr.version(), 1);
                }

                {  // is compatible but not strictly the same as 'root' one
                    json key_mgr_patch = R"([
                    { "op": "replace", "path": "/signed/metadata_spec_version", "value": "0.6.1" }
                    ])"_json;

                    EXPECT_THROW(root.create_key_mgr(sign_patched(key_mgr_patch)),
                                 spec_version_error);
                }

                {  // wrong type
                    json key_mgr_patch = R"([
                    { "op": "replace", "path": "/signed/metadata_spec_version", "value": 0.6 }
                    ])"_json;

                    EXPECT_THROW(root.create_key_mgr(sign_patched(key_mgr_patch)),
                                 role_metadata_error);
                }
            }

            TEST_F(KeyMgr, ctor_from_path)
            {
                RootImpl root(root1_json);

                auto key_mgr = root.create_key_mgr(trusted_file(key_mgr_json));
                EXPECT_EQ(key_mgr.spec_version(), SpecImpl("0.6.0"));
                EXPECT_EQ(key_mgr.version(), 1);

                // TODO: enforce consistency between spec version in filename and metadata
                key_mgr = root.create_key_mgr(trusted_file(key_mgr_json, "20.sv0.6.key_mgr.json"));
                EXPECT_EQ(key_mgr.spec_version(), SpecImpl("0.6.0"));
                EXPECT_EQ(key_mgr.version(), 1);


                EXPECT_THROW(root.create_key_mgr(fs::path("not_existing")), role_file_error);

                EXPECT_THROW(root.create_key_mgr(trusted_file(key_mgr_json, "wrong.json")),
                             role_file_error);

                EXPECT_THROW(root.create_key_mgr(trusted_file(key_mgr_json, "sv1.key_mgr.json")),
                             role_file_error);

                EXPECT_THROW(
                    root.create_key_mgr(trusted_file(key_mgr_json, "wrong.sv0.6.key_mgr.json")),
                    role_file_error);
            }

            TEST_F(KeyMgr, expires)
            {
                RootImpl root(root1_json);
                auto key_mgr = root.create_key_mgr(key_mgr_json);

                // expiration is set to now+3600s in 'sign_key_mgr'
                TimeRef::instance().set(utc_time_now());
                EXPECT_FALSE(key_mgr.expired());
                EXPECT_FALSE(root.expired());

                TimeRef::instance().set(utc_time_now() + 7200);
                EXPECT_TRUE(key_mgr.expired());
                EXPECT_TRUE(root.expired());

                json patch = json::parse(R"([
                    { "op": "replace", "path": "/signed/expiration", "value": ")"
                                         + timestamp(utc_time_now() + 10800) + R"(" }
                    ])");

                key_mgr = root.create_key_mgr(sign_patched(patch));
                EXPECT_FALSE(key_mgr.expired());
                EXPECT_TRUE(root.expired());
            }


            class PkgMgr : public KeyMgr
            {
            public:
                PkgMgr()
                    : KeyMgr()
                {
                    init_repodata();
                    root = std::make_unique<RootImpl>(root1_json);
                };

                json sign_repodata(const json& patch = json())
                {
                    json updated_repodata = repodata_json;

                    if (!patch.empty())
                        updated_repodata = updated_repodata.patch(patch);

                    for (auto& it : updated_repodata.at("packages").get<json::object_t>())
                    {
                        json sig_patch = json::parse(
                            R"({
                                "signatures": { ")"
                            + it.first + "\":" + sign_repodata_meta(it.second).dump() + R"(
                                    }
                                })");
                        updated_repodata.merge_patch(sig_patch);
                    }
                    return updated_repodata;
                }

            protected:
                json repodata_json, signed_repodata_json;

                std::unique_ptr<RootImpl> root;

                void init_repodata()
                {
                    repodata_json = R"({
                                        "info": {
                                            "subdir": "noarch"
                                        },
                                        "packages": {
                                            "test-package1-0.1-0.tar.bz2": {
                                            "build": "0",
                                            "build_number": 0,
                                            "depends": [],
                                            "license": "BSD",
                                            "license_family": "BSD",
                                            "md5": "2a8595f37faa2950e1b433acbe91d481",
                                            "name": "test-package",
                                            "noarch": "generic",
                                            "sha256": "b908ffce2d26d94c58c968abf286568d4bcf87d1cfe6c994958351724a6f6988",
                                            "size": 5719,
                                            "subdir": "noarch",
                                            "timestamp": 1613117294885,
                                            "version": "0.1"
                                            },
                                            "test-package2-0.1-0.tar.bz2": {
                                            "build": "0"
                                            }
                                        }
                                    })"_json;

                    signed_repodata_json = sign_repodata();
                }

                json sign_repodata_meta(const json& meta)
                {
                    std::map<std::string, std::map<std::string, std::string>> signatures;

                    unsigned char sig_bin[MAMBA_ED25519_SIGSIZE_BYTES];

                    for (auto& secret : secrets.at("pkg_mgr"))
                    {
                        sign(meta.dump(2), secret.second.data(), sig_bin);

                        auto sig_hex = ::mamba::hex_string(sig_bin, MAMBA_ED25519_SIGSIZE_BYTES);
                        signatures[secret.first].insert({ "signature", sig_hex });
                    }

                    return signatures;
                }
            };

            TEST_F(PkgMgr, verify_index)
            {
                auto key_mgr = root->create_key_mgr(key_mgr_json);
                auto pkg_mgr = key_mgr.create_pkg_mgr();

                pkg_mgr.verify_index(signed_repodata_json);
            }

            TEST_F(PkgMgr, corrupted_repodata)
            {
                auto key_mgr = root->create_key_mgr(key_mgr_json);
                auto pkg_mgr = key_mgr.create_pkg_mgr();

                json wrong_pkg_patch = R"([
                            { "op": "replace", "path": "/packages/test-package1-0.1-0.tar.bz2/version", "value": "0.1.1" }
                            ])"_json;
                EXPECT_THROW(pkg_mgr.verify_index(signed_repodata_json.patch(wrong_pkg_patch)),
                             package_error);
            }

            TEST_F(PkgMgr, hillformed_repodata)
            {
                auto key_mgr = root->create_key_mgr(key_mgr_json);
                auto pkg_mgr = key_mgr.create_pkg_mgr();

                json hillformed_pkg_patch = R"([
                            { "op": "remove", "path": "/signatures"}
                            ])"_json;
                EXPECT_THROW(pkg_mgr.verify_index(signed_repodata_json.patch(hillformed_pkg_patch)),
                             package_error);
            }
        }  // namespace testing
    }      // namespace v06

    namespace v1
    {
        namespace testing
        {
            using namespace mamba;

            class RootRoleV1 : public ::testing::Test
            {
            public:
                using role_secrets_type
                    = std::map<std::string, std::array<unsigned char, MAMBA_ED25519_KEYSIZE_BYTES>>;
                using secrets_type = std::map<std::string, role_secrets_type>;

                RootRoleV1()
                {
                    channel_dir = std::make_unique<TemporaryDirectory>();

                    generate_secrets();
                    sign_root();
                }

                fs::path trusted_root_file()
                {
                    fs::path p = channel_dir->path() / "root.json";

                    std::ofstream out_file(p, std::ofstream::out | std::ofstream::trunc);
                    out_file << root1_json;
                    out_file.close();

                    return p;
                }

                fs::path create_test_update(const fs::path& name, const json& patch = json())
                {
                    fs::path p = channel_dir->path() / name;

                    std::ofstream out_file(p, std::ofstream::out | std::ofstream::trunc);

                    json new_root = root1_json;

                    if (!patch.empty())
                        new_root = new_root.patch(patch);

                    json sig_patch
                        = json::parse(R"([
                                        { "op": "replace", "path": "/signatures", "value":)"
                                      + sign_root_meta(new_root.at("signed")).dump() + R"(}
                                        ])");
                    out_file << new_root.patch(sig_patch);
                    out_file.close();

                    return p;
                }

                void generate_secrets(int root = 1,
                                      int targets = 1,
                                      int snapshot = 1,
                                      int timestamp = 1)
                {
                    secrets.insert({ "root", generate_role_secrets(root) });
                    secrets.insert({ "targets", generate_role_secrets(targets) });
                    secrets.insert({ "snapshot", generate_role_secrets(snapshot) });
                    secrets.insert({ "timestamp", generate_role_secrets(timestamp) });
                }

                void sign_root()
                {
                    std::ifstream i(root1);
                    i >> root1_json;

                    std::map<std::string, RoleKeys> all_roles;
                    std::map<std::string, Key> all_keys;

                    for (auto& it : secrets)
                    {
                        auto& r = it.first;
                        std::vector<std::string> r_keys;
                        for (auto& s : it.second)
                        {
                            r_keys.push_back(s.first);
                            all_keys.insert({ s.first, Key::from_ed25519(s.first) });
                        }
                        all_roles[r] = { r_keys, 1 };
                    }
                    root1_json.at("signed").at("roles") = all_roles;
                    root1_json.at("signed").at("keys") = all_keys;
                    root1_json.at("signed")["expires"] = timestamp(utc_time_now() + 3600);

                    root1_json["signatures"] = sign_root_meta(root1_json["signed"]);
                }

                json sign_root_meta(const json& root_meta)
                {
                    std::vector<RoleSignature> signatures;
                    unsigned char sig_bin[MAMBA_ED25519_SIGSIZE_BYTES];

                    for (auto& secret : secrets.at("root"))
                    {
                        sign(root_meta.dump(), secret.second.data(), sig_bin);

                        auto sig_hex = ::mamba::hex_string(sig_bin, MAMBA_ED25519_SIGSIZE_BYTES);
                        signatures.push_back({ secret.first, sig_hex });
                    }

                    return signatures;
                }

            protected:
                fs::path root1 = "validation_data/root.json";
                json root1_json;

                std::unique_ptr<TemporaryDirectory> channel_dir;

                secrets_type secrets;

                std::map<std::string, std::array<unsigned char, MAMBA_ED25519_KEYSIZE_BYTES>>
                generate_role_secrets(int count)
                {
                    std::map<std::string, std::array<unsigned char, MAMBA_ED25519_KEYSIZE_BYTES>>
                        secrets;

                    unsigned char pk[MAMBA_ED25519_KEYSIZE_BYTES];
                    std::array<unsigned char, MAMBA_ED25519_KEYSIZE_BYTES> sk;

                    for (int i = 0; i < count; ++i)
                    {
                        generate_ed25519_keypair(pk, sk.data());

                        auto pk_hex = ::mamba::hex_string(pk, MAMBA_ED25519_KEYSIZE_BYTES);
                        secrets.insert({ pk_hex, sk });
                    }
                    return secrets;
                }
            };

            TEST_F(RootRoleV1, ctor_from_path)
            {
                RootImpl root(trusted_root_file());

                EXPECT_EQ(root.type(), "root");
                EXPECT_EQ(root.file_ext(), "json");
                EXPECT_EQ(root.spec_version(), SpecImpl("1.0.17"));
                EXPECT_EQ(root.version(), 1);
            }

            TEST_F(RootRoleV1, ctor_from_json)
            {
                RootImpl root(root1_json);

                EXPECT_EQ(root.type(), "root");
                EXPECT_EQ(root.file_ext(), "json");
                EXPECT_EQ(root.spec_version(), SpecImpl("1.0.17"));
                EXPECT_EQ(root.version(), 1);
            }

            TEST_F(RootRoleV1, update_from_path)
            {
                using namespace mamba;

                RootImpl root(trusted_root_file());

                json patch = R"([
                    { "op": "replace", "path": "/signed/version", "value": 2 }
                    ])"_json;
                auto updated_root = root.update(create_test_update("2.root.json", patch));

                auto testing_root = static_cast<RootImpl*>(updated_root.get());
                EXPECT_EQ(testing_root->type(), "root");
                EXPECT_EQ(testing_root->file_ext(), "json");
                EXPECT_EQ(testing_root->spec_version(), SpecImpl("1.0.17"));
                EXPECT_EQ(testing_root->version(), 2);
            }

            TEST_F(RootRoleV1, ctor_wrong_filename_spec_version)
            {
                fs::path p = channel_dir->path() / "2.sv0.6.root.json";

                std::ofstream out_file(p, std::ofstream::out | std::ofstream::trunc);
                out_file << root1_json;
                out_file.close();

                // "2.sv0.6.root.json" is not compatible spec version (spec version N)
                EXPECT_THROW(RootImpl root(p), role_file_error);
            }

            TEST_F(RootRoleV1, wrong_version)
            {
                RootImpl root(root1_json);

                json patch = R"([
                    { "op": "replace", "path": "/signed/version", "value": 3 }
                    ])"_json;

                EXPECT_THROW(root.update(create_test_update("2.root.json", patch)),
                             role_metadata_error);
            }

            TEST_F(RootRoleV1, spec_version)
            {
                RootImpl root(root1_json);

                json patch = R"([
                    { "op": "replace", "path": "/signed/version", "value": 2 },
                    { "op": "replace", "path": "/signed/spec_version", "value": "1.30.10" }
                    ])"_json;

                auto updated_root = root.update(create_test_update("2.root.json", patch));

                auto testing_root = static_cast<RootImpl*>(updated_root.get());
                EXPECT_EQ(testing_root->spec_version(), SpecImpl("1.30.10"));
                EXPECT_EQ(testing_root->version(), 2);
            }

            TEST_F(RootRoleV1, wrong_spec_version)
            {
                RootImpl root(root1_json);

                json patch = R"([
                    { "op": "replace", "path": "/signed/spec_version", "value": "2.0.0" }
                    ])"_json;

                EXPECT_THROW(root.update(create_test_update("2.root.json", patch)),
                             spec_version_error);
            }

            TEST_F(RootRoleV1, wrong_filename_role)
            {
                RootImpl root(root1_json);

                json patch = R"([])"_json;

                EXPECT_THROW(root.update(create_test_update("2.rooot.json", patch)),
                             role_file_error);
            }

            TEST_F(RootRoleV1, wrong_filename_version)
            {
                RootImpl root(root1_json);

                json patch = R"([])"_json;

                EXPECT_THROW(root.update(create_test_update("3.root.json", patch)),
                             role_file_error);
            }

            TEST_F(RootRoleV1, wrong_filename_spec_version)
            {
                RootImpl root(root1_json);

                // "2.sv2.root.json" is upgradable spec version (spec version N+1)
                // but v2 is NOT implemented yet, so v1::RootImpl is not upgradable
                EXPECT_THROW(root.update(create_test_update("2.sv2.root.json")),
                             spec_version_error);
                // "2.sv3.root.json" is NOT upgradable spec version (spec version N+1)
                EXPECT_THROW(root.update(create_test_update("2.sv3.root.json")), role_file_error);
                EXPECT_THROW(root.update(create_test_update("2.sv0.6.root.json")), role_file_error);
            }

            TEST_F(RootRoleV1, hillformed_filename_version)
            {
                RootImpl root(root1_json);

                json patch = R"([])"_json;

                EXPECT_THROW(root.update(create_test_update("wrong.root.json", patch)),
                             role_file_error);
            }

            TEST_F(RootRoleV1, rollback_attack)
            {
                RootImpl root(root1_json);

                json patch = R"([
                    { "op": "replace", "path": "/signed/version", "value": 1 }
                    ])"_json;

                EXPECT_THROW(root.update(create_test_update("2.root.json", patch)), rollback_error);
            }

            TEST_F(RootRoleV1, wrong_type)
            {
                RootImpl root(root1_json);

                json patch = R"([
                    { "op": "replace", "path": "/signed/_type", "value": "timestamp" },
                    { "op": "replace", "path": "/signed/version", "value": 2 }
                    ])"_json;

                EXPECT_THROW(root.update(create_test_update("2.root.json", patch)),
                             role_metadata_error);
            }

            TEST_F(RootRoleV1, missing_type)
            {
                RootImpl root(root1_json);

                json patch = R"([
                    { "op": "remove", "path": "/signed/_type" },
                    { "op": "replace", "path": "/signed/version", "value": 2 }
                    ])"_json;

                EXPECT_THROW(root.update(create_test_update("2.root.json", patch)),
                             role_metadata_error);
            }

            TEST_F(RootRoleV1, missing_keys)
            {
                RootImpl root(root1_json);

                json patch = R"([
                    { "op": "remove", "path": "/signed/keys" },
                    { "op": "replace", "path": "/signed/version", "value": 2 }
                    ])"_json;

                EXPECT_THROW(root.update(create_test_update("2.root.json", patch)),
                             role_metadata_error);
            }

            TEST_F(RootRoleV1, missing_roles)
            {
                RootImpl root(root1_json);

                json patch = R"([
                    { "op": "remove", "path": "/signed/roles" },
                    { "op": "replace", "path": "/signed/version", "value": 2 }
                    ])"_json;

                EXPECT_THROW(root.update(create_test_update("2.root.json", patch)),
                             role_metadata_error);
            }

            TEST_F(RootRoleV1, missing_role)
            {
                RootImpl root(root1_json);

                json patch = R"([
                    { "op": "remove", "path": "/signed/roles/timestamp" },
                    { "op": "replace", "path": "/signed/version", "value": 2 }
                    ])"_json;

                EXPECT_THROW(root.update(create_test_update("2.root.json", patch)),
                             role_metadata_error);
            }

            TEST_F(RootRoleV1, empty_role_keyids)
            {
                RootImpl root(root1_json);

                json patch = R"([
                                { "op": "replace", "path": "/signed/roles/snapshot/keyids", "value": [] },
                                { "op": "replace", "path": "/signed/version", "value": 2 }
                                ])"_json;

                EXPECT_THROW(root.update(create_test_update("2.root.json", patch)),
                             role_metadata_error);
            }

            TEST_F(RootRoleV1, null_role_threshold)
            {
                RootImpl root(root1_json);

                json patch = R"([
                                { "op": "replace", "path": "/signed/roles/snapshot/threshold", "value": 0 },
                                { "op": "replace", "path": "/signed/version", "value": 2 }
                                ])"_json;

                EXPECT_THROW(root.update(create_test_update("2.root.json", patch)),
                             role_metadata_error);
            }

            TEST_F(RootRoleV1, extra_roles)
            {
                RootImpl root(root1_json);

                json patch = R"([
                    { "op": "add", "path": "/signed/roles/some_wrong_role", "value": { "keyids": ["c"], "threshold": 1 } },
                    { "op": "replace", "path": "/signed/version", "value": 2 }
                    ])"_json;

                EXPECT_THROW(root.update(create_test_update("2.root.json", patch)),
                             role_metadata_error);
            }

            TEST_F(RootRoleV1, key_not_found)
            {
                RootImpl root(root1_json);

                json patch = R"([
                    { "op": "add", "path": "/signed/roles/snapshot/keyids/-", "value": "c" },
                    { "op": "replace", "path": "/signed/version", "value": 2 }
                    ])"_json;

                EXPECT_THROW(root.update(create_test_update("2.root.json", patch)),
                             role_metadata_error);
            }

            TEST_F(RootRoleV1, mirrors_role)
            {
                json patch = R"([
                    { "op": "add", "path": "/signed/roles/mirrors", "value": { "keyids": ["c"], "threshold": 1 } },
                    { "op": "add", "path": "/signed/keys/c", "value": { "scheme": "ed25519", "keytype": "ed25519", "keyval": "c"} },
                    { "op": "replace", "path": "/signed/version", "value": 2 }
                    ])"_json;

                RootImpl root(create_test_update("2.root.json", patch));
                bool mirrors_role_found = (root.roles().find("mirrors") != root.roles().end());
                EXPECT_TRUE(mirrors_role_found);
            }

            TEST_F(RootRoleV1, threshold_not_met)
            {
                RootImpl root(root1_json);

                json patch = R"([
                    { "op": "replace", "path": "/signed/version", "value": 2 },
                    { "op": "replace", "path": "/signed/roles/root/threshold", "value": 2 }
                    ])"_json;

                EXPECT_THROW(root.update(create_test_update("2.root.json", patch)),
                             threshold_error);
            }

            TEST_F(RootRoleV1, expires)
            {
                RootImpl root(root1_json);

                // expiration is set to now+3600s in 'sign_root'
                TimeRef::instance().set(utc_time_now());
                EXPECT_FALSE(root.expired());

                TimeRef::instance().set(utc_time_now() + 7200);
                EXPECT_TRUE(root.expired());

                json patch = json::parse(R"([
                    { "op": "replace", "path": "/signed/expires", "value": ")"
                                         + timestamp(utc_time_now() + 10800) + R"(" },
                    { "op": "replace", "path": "/signed/version", "value": 2 }
                    ])");
                auto updated_root = root.update(create_test_update("2.root.json", patch));

                auto testing_root = static_cast<RootImpl*>(updated_root.get());
                EXPECT_FALSE(testing_root->expired());
            }

            TEST(RoleSignature, to_json)
            {
                RoleSignature s{ "some_key_id", "some_signature", "" };
                json j = R"({"keyid": "some_key_id", "sig": "some_signature"})"_json;
                EXPECT_EQ(j, json(s));

                s = { "some_key_id", "some_signature", "some_pgp_trailer" };
                j = R"({"keyid": "some_key_id", "other_headers": "some_pgp_trailer", "sig": "some_signature"})"_json;
                EXPECT_EQ(j, json(s));
            }
        }  // namespace testing
    }      // namespace v1
}  // namespace validate
