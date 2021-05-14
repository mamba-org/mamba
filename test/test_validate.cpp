#include <gtest/gtest.h>

#include "mamba/core/validate.hpp"
#include "mamba/core/util.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdio>
#include <map>


namespace validate
{
    namespace testing
    {
        using nlohmann::json;

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
                    generate_secrets();
                    sign_root();
                }

                std::unique_ptr<TemporaryFile> create_test_initial_root()
                {
                    std::unique_ptr<TemporaryFile> root_file
                        = std::make_unique<mamba::TemporaryFile>("1.", ".root.json");

                    auto p = root_file->path();
                    std::ofstream out_file(p, std::ofstream::out | std::ofstream::trunc);
                    out_file << root1_json;
                    out_file.close();
                    return root_file;
                }

                std::unique_ptr<TemporaryFile> create_test_update(const std::string& prefix,
                                                                  const std::string& suffix,
                                                                  json patch)
                {
                    std::unique_ptr<TemporaryFile> root_update
                        = std::make_unique<mamba::TemporaryFile>(prefix, suffix);

                    auto p = root_update->path();
                    std::ofstream out_file(p, std::ofstream::out | std::ofstream::trunc);

                    auto new_root = root1_json.patch(patch);

                    json sig_patch = R"([
                            { "op": "replace", "path": "/signatures", "value": 2 }
                            ])"_json;
                    sig_patch[0]["value"] = sign_root_meta(new_root.at("signed"));

                    out_file << new_root.patch(sig_patch);
                    out_file.close();

                    return root_update;
                }

                void generate_secrets(int root = 1, int key_mgr = 1)
                {
                    secrets.insert({ "root", generate_role_secrets(root) });
                    secrets.insert({ "key_mgr", generate_role_secrets(key_mgr) });
                }

                void sign_root()
                {
                    std::vector<std::string> root_pks;
                    for (auto& secret : secrets.at("root"))
                    {
                        root_pks.push_back(secret.first);
                    }
                    root1_json["signed"]["delegations"]["root"] = RolePubKeys({ root_pks, 1 });

                    std::vector<std::string> key_mgr_pks;
                    for (auto& secret : secrets.at("key_mgr"))
                    {
                        key_mgr_pks.push_back(secret.first);
                    }
                    root1_json["signed"]["delegations"]["key_mgr"]
                        = RolePubKeys({ key_mgr_pks, 1 });
                    root1_json["signed"]["version"] = 1;
                    root1_json["signed"]["metadata_spec_version"] = "0.6.0";
                    root1_json["signed"]["type"] = "root";
                    root1_json["signatures"] = sign_root_meta(root1_json["signed"]);
                }

                json sign_root_meta(const json& root_meta)
                {
                    std::map<std::string, std::map<std::string, std::string>> signatures;

                    char sig_hex[MAMBA_ED25519_SIGSIZE_HEX];
                    unsigned char sig_bin[MAMBA_ED25519_SIGSIZE_BYTES];

                    for (auto& secret : secrets.at("root"))
                    {
                        sign(root_meta.dump(), secret.second.data(), sig_bin);

                        bin2hex(sig_hex,
                                MAMBA_ED25519_SIGSIZE_HEX,
                                sig_bin,
                                MAMBA_ED25519_SIGSIZE_BYTES);
                        signatures[secret.first].insert(
                            { "signature",
                              std::string(sig_hex, sig_hex + MAMBA_ED25519_SIGSIZE_HEX) });
                    }

                    return signatures;
                }

                json upgrade_to_v1(const RootRole& root, const json& patch = json())
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
                fs::path root1 = "validation_data/root.json";
                json root1_json;

                secrets_type secrets;

                std::map<std::string, std::array<unsigned char, MAMBA_ED25519_KEYSIZE_BYTES>>
                generate_role_secrets(int count)
                {
                    std::map<std::string, std::array<unsigned char, MAMBA_ED25519_KEYSIZE_BYTES>>
                        secrets;

                    unsigned char pk[MAMBA_ED25519_KEYSIZE_BYTES];
                    std::array<unsigned char, MAMBA_ED25519_KEYSIZE_BYTES> sk;
                    char pk_hex[MAMBA_ED25519_KEYSIZE_HEX];

                    for (int i = 0; i < count; ++i)
                    {
                        generate_ed25519_keypair(pk, sk.data());
                        bin2hex(pk_hex, MAMBA_ED25519_KEYSIZE_HEX, pk, MAMBA_ED25519_KEYSIZE_BYTES);

                        secrets.insert(
                            { std::string(pk_hex, pk_hex + MAMBA_ED25519_KEYSIZE_HEX), sk });
                    }
                    return secrets;
                }
            };

            TEST_F(RootRoleV06, ctor_from_path)
            {
                auto tmp_file = create_test_initial_root();
                RootRole root(tmp_file->path());

                EXPECT_EQ(root.type(), "root");
                EXPECT_EQ(root.file_ext(), "json");
                EXPECT_EQ(root.spec_version(), "0.6.0");
                EXPECT_EQ(root.version(), 1);
            }

            TEST_F(RootRoleV06, ctor_from_json)
            {
                RootRole root(root1_json);

                EXPECT_EQ(root.type(), "root");
                EXPECT_EQ(root.file_ext(), "json");
                EXPECT_EQ(root.spec_version(), "0.6.0");
                EXPECT_EQ(root.version(), 1);
            }

            TEST_F(RootRoleV06, update_from_path)
            {
                using namespace mamba;

                auto tmp_file = create_test_initial_root();
                RootRole root(tmp_file->path());

                json patch = R"([
                    { "op": "replace", "path": "/signed/version", "value": 2 }
                    ])"_json;
                tmp_file = create_test_update("2.", ".root.json", patch);

                auto updated_root = root.update(tmp_file->path());

                EXPECT_EQ(updated_root->type(), "root");
                EXPECT_EQ(updated_root->file_ext(), "json");
                EXPECT_EQ(updated_root->spec_version(), "0.6.0");
                EXPECT_EQ(updated_root->version(), 2);
            }

            TEST_F(RootRoleV06, wrong_version)
            {
                RootRole root(root1_json);

                json patch = R"([
                    { "op": "replace", "path": "/signed/version", "value": 3 }
                    ])"_json;
                auto tmp_file = create_test_update("2.", ".root.json", patch);

                EXPECT_THROW(root.update(tmp_file->path()), role_metadata_error);
            }

            TEST_F(RootRoleV06, spec_version)
            {
                RootRole root(root1_json);

                json patch = R"([
                    { "op": "replace", "path": "/signed/version", "value": 2 },
                    { "op": "replace", "path": "/signed/metadata_spec_version", "value": "0.6.1" }
                    ])"_json;
                auto tmp_file = create_test_update("2.", ".root.json", patch);

                auto updated_root = root.update(tmp_file->path());
                EXPECT_EQ(updated_root->spec_version(), "0.6.1");
                EXPECT_EQ(updated_root->version(), 2);
            }

            TEST_F(RootRoleV06, upgraded_spec_version)
            {
                RootRole root(root1_json);

                json patch = R"([
                    { "op": "replace", "path": "/signed/version", "value": 2 },
                    { "op": "replace", "path": "/signed/metadata_spec_version", "value": "1.0.0" }
                    ])"_json;
                auto tmp_file = create_test_update("2.", ".root.json", patch);

                EXPECT_THROW(root.update(tmp_file->path()), spec_version_error);

                json signable_patch = R"([
                    { "op": "replace", "path": "/version", "value": 2 },
                    { "op": "add", "path": "/keys/dummy_value", "value": { "keytype": "ed25519", "scheme": "ed25519", "keyval": "dummy_value" } },
                    { "op": "add", "path": "/roles/snapshot/keyids", "value": ["dummy_value"] },
                    { "op": "add", "path": "/roles/timestamp/keyids", "value": ["dummy_value"] }
                    ])"_json;
                auto updated_root = root.update(upgrade_to_v1(root, signable_patch));
                EXPECT_EQ(updated_root->spec_version(), "1.0.17");
                EXPECT_EQ(updated_root->version(), 2);
            }

            TEST_F(RootRoleV06, equivalent_upgraded_spec_version)
            {
                RootRole root(root1_json);

                json signable_patch = R"([
                    { "op": "add", "path": "/keys/dummy_value", "value": { "keytype": "ed25519", "scheme": "ed25519", "keyval": "dummy_value" } },
                    { "op": "add", "path": "/roles/snapshot/keyids", "value": ["dummy_value"] },
                    { "op": "add", "path": "/roles/timestamp/keyids", "value": ["dummy_value"] }
                    ])"_json;
                v1::RootRole updated_root(upgrade_to_v1(root, signable_patch));

                EXPECT_EQ(updated_root.spec_version(), "1.0.17");
                EXPECT_EQ(updated_root.version(), 1);
            }

            TEST_F(RootRoleV06, wrong_spec_version)
            {
                RootRole root(root1_json);

                json patch = R"([
                    { "op": "replace", "path": "/signed/version", "value": 2 },
                    { "op": "replace", "path": "/signed/metadata_spec_version", "value": "1.0.0" }
                    ])"_json;
                auto tmp_file = create_test_update("2.", ".root.json", patch);

                EXPECT_THROW(root.update(tmp_file->path()), spec_version_error);
            }

            TEST_F(RootRoleV06, wrong_filename_role)
            {
                RootRole root(root1_json);

                json patch = R"([])"_json;
                auto tmp_file = create_test_update("2.", ".rooot.json", patch);

                EXPECT_THROW(root.update(tmp_file->path()), role_file_error);
            }

            TEST_F(RootRoleV06, wrong_filename_version)
            {
                RootRole root(root1_json);

                json patch = R"([])"_json;
                auto tmp_file = create_test_update("3.", ".root.json", patch);

                EXPECT_THROW(root.update(tmp_file->path()), role_file_error);
            }

            TEST_F(RootRoleV06, hillformed_filename_version)
            {
                RootRole root(root1_json);

                json patch = R"([])"_json;
                auto tmp_file = create_test_update("wrong.", ".root.json", patch);

                EXPECT_THROW(root.update(tmp_file->path()), role_file_error);
            }

            TEST_F(RootRoleV06, rollback_attack)
            {
                RootRole root(root1_json);

                json patch = R"([
                    { "op": "replace", "path": "/signed/version", "value": 1 }
                    ])"_json;
                auto tmp_file = create_test_update("2.", ".root.json", patch);

                EXPECT_THROW(root.update(tmp_file->path()), rollback_error);
            }

            TEST_F(RootRoleV06, wrong_type)
            {
                RootRole root(root1_json);

                json patch = R"([
                    { "op": "replace", "path": "/signed/type", "value": "timestamp" },
                    { "op": "replace", "path": "/signed/version", "value": 2 }
                    ])"_json;
                auto tmp_file = create_test_update("2.", ".root.json", patch);

                EXPECT_THROW(root.update(tmp_file->path()), role_metadata_error);
            }

            TEST_F(RootRoleV06, missing_type)
            {
                RootRole root(root1_json);

                json patch = R"([
                    { "op": "remove", "path": "/signed/type" },
                    { "op": "replace", "path": "/signed/version", "value": 2 }
                    ])"_json;
                auto tmp_file = create_test_update("2.", ".root.json", patch);
                EXPECT_THROW(root.update(tmp_file->path()), role_metadata_error);
            }

            TEST_F(RootRoleV06, missing_delegations)
            {
                RootRole root(root1_json);

                json patch = R"([
                    { "op": "remove", "path": "/signed/delegations" },
                    { "op": "replace", "path": "/signed/version", "value": 2 }
                    ])"_json;
                auto tmp_file = create_test_update("2.", ".root.json", patch);
                EXPECT_THROW(root.update(tmp_file->path()), role_metadata_error);
            }

            TEST_F(RootRoleV06, missing_delegation)
            {
                RootRole root(root1_json);

                json patch = R"([
                                { "op": "remove", "path": "/signed/delegations/root" },
                                { "op": "replace", "path": "/signed/version", "value": 2 }
                                ])"_json;
                auto tmp_file = create_test_update("2.", ".root.json", patch);

                EXPECT_THROW(root.update(tmp_file->path()), role_metadata_error);
            }

            TEST_F(RootRoleV06, empty_delegation_pubkeys)
            {
                RootRole root(root1_json);

                json patch = R"([
                                { "op": "replace", "path": "/signed/delegations/root/pubkeys", "value": [] },
                                { "op": "replace", "path": "/signed/version", "value": 2 }
                                ])"_json;
                auto tmp_file = create_test_update("2.", ".root.json", patch);

                EXPECT_THROW(root.update(tmp_file->path()), role_metadata_error);
            }

            TEST_F(RootRoleV06, null_role_threshold)
            {
                RootRole root(root1_json);

                json patch = R"([
                                { "op": "replace", "path": "/signed/delegations/root/threshold", "value": 0 },
                                { "op": "replace", "path": "/signed/version", "value": 2 }
                                ])"_json;
                auto tmp_file = create_test_update("2.", ".root.json", patch);

                EXPECT_THROW(root.update(tmp_file->path()), role_metadata_error);
            }

            TEST_F(RootRoleV06, extra_roles)
            {
                RootRole root(root1_json);

                json patch = R"([
                                { "op": "add", "path": "/signed/delegations/some_wrong_role",
                                    "value": { "keyids": ["c"], "threshold": 1 } },
                                { "op": "replace", "path": "/signed/version", "value": 2 }
                                ])"_json;
                auto tmp_file = create_test_update("2.", ".root.json", patch);

                EXPECT_THROW(root.update(tmp_file->path()), role_metadata_error);
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
                auto tmp_file = create_test_update("", ".root.json", patch);

                RootRole root(tmp_file->path());
                bool mirrors_role_found = (root.roles().find("mirrors") != root.roles().end());
                EXPECT_TRUE(mirrors_role_found);
            }
            */
            TEST_F(RootRoleV06, threshold_not_met)
            {
                RootRole root(root1_json);

                json patch = R"([
                    { "op": "replace", "path": "/signed/version", "value": 2 },
                    { "op": "replace", "path": "/signed/delegations/root/threshold", "value": 2 }
                    ])"_json;

                auto tmp_file = create_test_update("2.", ".root.json", patch);

                EXPECT_THROW(root.update(tmp_file->path()), threshold_error);
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
                    generate_secrets();
                    sign_root();
                }

                std::unique_ptr<TemporaryFile> create_test_initial_root()
                {
                    std::unique_ptr<TemporaryFile> root_file
                        = std::make_unique<mamba::TemporaryFile>("1.", ".root.json");

                    auto p = root_file->path();
                    std::ofstream out_file(p, std::ofstream::out | std::ofstream::trunc);
                    out_file << root1_json;
                    out_file.close();
                    return root_file;
                }

                std::unique_ptr<TemporaryFile> create_test_update(const std::string& prefix,
                                                                  const std::string& suffix,
                                                                  json patch)
                {
                    std::unique_ptr<TemporaryFile> root_update
                        = std::make_unique<mamba::TemporaryFile>(prefix, suffix);

                    auto p = root_update->path();
                    std::ofstream out_file(p, std::ofstream::out | std::ofstream::trunc);

                    auto new_root = root1_json.patch(patch);

                    json sig_patch = R"([
                            { "op": "replace", "path": "/signatures", "value": 2 }
                            ])"_json;
                    sig_patch[0]["value"] = sign_root_meta(new_root.at("signed"));

                    out_file << new_root.patch(sig_patch);
                    out_file.close();

                    return root_update;
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

                    root1_json["signatures"] = sign_root_meta(root1_json["signed"]);
                }

                json sign_root_meta(const json& root_meta)
                {
                    std::vector<RoleSignature> signatures;
                    char sig_hex[MAMBA_ED25519_SIGSIZE_HEX];
                    unsigned char sig_bin[MAMBA_ED25519_SIGSIZE_BYTES];

                    for (auto& secret : secrets.at("root"))
                    {
                        sign(root_meta.dump(), secret.second.data(), sig_bin);

                        bin2hex(sig_hex,
                                MAMBA_ED25519_SIGSIZE_HEX,
                                sig_bin,
                                MAMBA_ED25519_SIGSIZE_BYTES);
                        signatures.push_back(
                            { secret.first,
                              std::string(sig_hex, sig_hex + MAMBA_ED25519_SIGSIZE_HEX) });
                    }

                    return signatures;
                }

            protected:
                fs::path root1 = "validation_data/root.json";
                json root1_json;

                secrets_type secrets;

                std::map<std::string, std::array<unsigned char, MAMBA_ED25519_KEYSIZE_BYTES>>
                generate_role_secrets(int count)
                {
                    std::map<std::string, std::array<unsigned char, MAMBA_ED25519_KEYSIZE_BYTES>>
                        secrets;

                    unsigned char pk[MAMBA_ED25519_KEYSIZE_BYTES];
                    std::array<unsigned char, MAMBA_ED25519_KEYSIZE_BYTES> sk;
                    char pk_hex[MAMBA_ED25519_KEYSIZE_HEX];

                    for (int i = 0; i < count; ++i)
                    {
                        generate_ed25519_keypair(pk, sk.data());
                        bin2hex(pk_hex, MAMBA_ED25519_KEYSIZE_HEX, pk, MAMBA_ED25519_KEYSIZE_BYTES);

                        secrets.insert(
                            { std::string(pk_hex, pk_hex + MAMBA_ED25519_KEYSIZE_HEX), sk });
                    }
                    return secrets;
                }
            };

            TEST_F(RootRoleV1, ctor_from_path)
            {
                auto tmp_file = create_test_initial_root();
                RootRole root(tmp_file->path());

                EXPECT_EQ(root.type(), "root");
                EXPECT_EQ(root.file_ext(), "json");
                EXPECT_EQ(root.spec_version(), "1.0.17");
                EXPECT_EQ(root.version(), 1);
            }

            TEST_F(RootRoleV1, ctor_from_json)
            {
                RootRole root(root1_json);

                EXPECT_EQ(root.type(), "root");
                EXPECT_EQ(root.file_ext(), "json");
                EXPECT_EQ(root.spec_version(), "1.0.17");
                EXPECT_EQ(root.version(), 1);
            }

            TEST_F(RootRoleV1, update_from_path)
            {
                using namespace mamba;

                auto tmp_file = create_test_initial_root();
                RootRole root(tmp_file->path());

                json patch = R"([
                    { "op": "replace", "path": "/signed/version", "value": 2 }
                    ])"_json;
                tmp_file = create_test_update("2.", ".root.json", patch);

                auto updated_root = root.update(tmp_file->path());

                EXPECT_EQ(updated_root->type(), "root");
                EXPECT_EQ(updated_root->file_ext(), "json");
                EXPECT_EQ(updated_root->spec_version(), "1.0.17");
                EXPECT_EQ(updated_root->version(), 2);
            }

            TEST_F(RootRoleV1, wrong_version)
            {
                RootRole root(root1_json);

                json patch = R"([
                    { "op": "replace", "path": "/signed/version", "value": 3 }
                    ])"_json;
                auto tmp_file = create_test_update("2.", ".root.json", patch);

                EXPECT_THROW(root.update(tmp_file->path()), role_metadata_error);
            }

            TEST_F(RootRoleV1, spec_version)
            {
                RootRole root(root1_json);

                json patch = R"([
                    { "op": "replace", "path": "/signed/version", "value": 2 },
                    { "op": "replace", "path": "/signed/spec_version", "value": "1.30.10" }
                    ])"_json;
                auto tmp_file = create_test_update("2.", ".root.json", patch);

                auto updated_root = root.update(tmp_file->path());
                EXPECT_EQ(updated_root->spec_version(), "1.30.10");
                EXPECT_EQ(updated_root->version(), 2);
            }

            TEST_F(RootRoleV1, wrong_spec_version)
            {
                RootRole root(root1_json);

                json patch = R"([
                    { "op": "replace", "path": "/signed/spec_version", "value": "2.0.0" }
                    ])"_json;
                auto tmp_file = create_test_update("2.", ".root.json", patch);

                EXPECT_THROW(root.update(tmp_file->path()), spec_version_error);
            }

            TEST_F(RootRoleV1, wrong_filename_role)
            {
                RootRole root(root1_json);

                json patch = R"([])"_json;
                auto tmp_file = create_test_update("2.", ".rooot.json", patch);

                EXPECT_THROW(root.update(tmp_file->path()), role_file_error);
            }

            TEST_F(RootRoleV1, wrong_filename_version)
            {
                RootRole root(root1_json);

                json patch = R"([])"_json;
                auto tmp_file = create_test_update("3.", ".root.json", patch);

                EXPECT_THROW(root.update(tmp_file->path()), role_file_error);
            }

            TEST_F(RootRoleV1, hillformed_filename_version)
            {
                RootRole root(root1_json);

                json patch = R"([])"_json;
                auto tmp_file = create_test_update("wrong.", ".root.json", patch);

                EXPECT_THROW(root.update(tmp_file->path()), role_file_error);
            }

            TEST_F(RootRoleV1, rollback_attack)
            {
                RootRole root(root1_json);

                json patch = R"([
                    { "op": "replace", "path": "/signed/version", "value": 1 }
                    ])"_json;
                auto tmp_file = create_test_update("2.", ".root.json", patch);

                EXPECT_THROW(root.update(tmp_file->path()), rollback_error);
            }

            TEST_F(RootRoleV1, wrong_type)
            {
                RootRole root(root1_json);

                json patch = R"([
                    { "op": "replace", "path": "/signed/_type", "value": "timestamp" },
                    { "op": "replace", "path": "/signed/version", "value": 2 }
                    ])"_json;
                auto tmp_file = create_test_update("2.", ".root.json", patch);

                EXPECT_THROW(root.update(tmp_file->path()), role_metadata_error);
            }

            TEST_F(RootRoleV1, missing_type)
            {
                RootRole root(root1_json);

                json patch = R"([
                    { "op": "remove", "path": "/signed/_type" },
                    { "op": "replace", "path": "/signed/version", "value": 2 }
                    ])"_json;
                auto tmp_file = create_test_update("2.", ".root.json", patch);
                EXPECT_THROW(root.update(tmp_file->path()), role_metadata_error);
            }

            TEST_F(RootRoleV1, missing_keys)
            {
                RootRole root(root1_json);

                json patch = R"([
                    { "op": "remove", "path": "/signed/keys" },
                    { "op": "replace", "path": "/signed/version", "value": 2 }
                    ])"_json;
                auto tmp_file = create_test_update("2.", ".root.json", patch);
                EXPECT_THROW(root.update(tmp_file->path()), role_metadata_error);
            }

            TEST_F(RootRoleV1, missing_roles)
            {
                RootRole root(root1_json);

                json patch = R"([
                    { "op": "remove", "path": "/signed/roles" },
                    { "op": "replace", "path": "/signed/version", "value": 2 }
                    ])"_json;
                auto tmp_file = create_test_update("2.", ".root.json", patch);
                EXPECT_THROW(root.update(tmp_file->path()), role_metadata_error);
            }

            TEST_F(RootRoleV1, missing_role)
            {
                RootRole root(root1_json);

                json patch = R"([
                    { "op": "remove", "path": "/signed/roles/timestamp" },
                    { "op": "replace", "path": "/signed/version", "value": 2 }
                    ])"_json;
                auto tmp_file = create_test_update("2.", ".root.json", patch);

                EXPECT_THROW(root.update(tmp_file->path()), role_metadata_error);
            }

            TEST_F(RootRoleV1, empty_role_keyids)
            {
                RootRole root(root1_json);

                json patch = R"([
                                { "op": "replace", "path": "/signed/roles/snapshot/keyids", "value": [] },
                                { "op": "replace", "path": "/signed/version", "value": 2 }
                                ])"_json;
                auto tmp_file = create_test_update("2.", ".root.json", patch);

                EXPECT_THROW(root.update(tmp_file->path()), role_metadata_error);
            }

            TEST_F(RootRoleV1, null_role_threshold)
            {
                RootRole root(root1_json);

                json patch = R"([
                                { "op": "replace", "path": "/signed/roles/snapshot/threshold", "value": 0 },
                                { "op": "replace", "path": "/signed/version", "value": 2 }
                                ])"_json;
                auto tmp_file = create_test_update("2.", ".root.json", patch);

                EXPECT_THROW(root.update(tmp_file->path()), role_metadata_error);
            }

            TEST_F(RootRoleV1, extra_roles)
            {
                RootRole root(root1_json);

                json patch = R"([
                    { "op": "add", "path": "/signed/roles/some_wrong_role", "value": { "keyids": ["c"], "threshold": 1 } },
                    { "op": "replace", "path": "/signed/version", "value": 2 }
                    ])"_json;
                auto tmp_file = create_test_update("2.", ".root.json", patch);

                EXPECT_THROW(root.update(tmp_file->path()), role_metadata_error);
            }

            TEST_F(RootRoleV1, not_found_keyid)
            {
                RootRole root(root1_json);

                json patch = R"([
                    { "op": "add", "path": "/signed/roles/snapshot/keyids/-", "value": "c" },
                    { "op": "replace", "path": "/signed/version", "value": 2 }
                    ])"_json;
                auto tmp_file = create_test_update("2.", ".root.json", patch);

                EXPECT_THROW(root.update(tmp_file->path()), role_metadata_error);
            }

            TEST_F(RootRoleV1, mirrors_role)
            {
                json patch = R"([
                    { "op": "add", "path": "/signed/roles/mirrors", "value": { "keyids": ["c"], "threshold": 1 } },
                    { "op": "add", "path": "/signed/keys/c", "value": { "scheme": "ed25519", "keytype": "ed25519", "keyval": "c"} },
                    { "op": "replace", "path": "/signed/version", "value": 2 }
                    ])"_json;
                auto tmp_file = create_test_update("2.", ".root.json", patch);

                RootRole root(tmp_file->path());
                bool mirrors_role_found = (root.roles().find("mirrors") != root.roles().end());
                EXPECT_TRUE(mirrors_role_found);
            }

            TEST_F(RootRoleV1, threshold_not_met)
            {
                RootRole root(root1_json);

                json patch = R"([
                    { "op": "replace", "path": "/signed/version", "value": 2 },
                    { "op": "replace", "path": "/signed/roles/root/threshold", "value": 2 }
                    ])"_json;

                auto tmp_file = create_test_update("2.", ".root.json", patch);

                EXPECT_THROW(root.update(tmp_file->path()), threshold_error);
            }
        }  // namespace testing
    }      // namespace v1
}  // namespace validate
