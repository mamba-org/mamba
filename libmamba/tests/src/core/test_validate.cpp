// Copyright (c) 2022, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <algorithm>
#include <map>

#include <doctest/doctest.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "mamba/core/context.hpp"
#include "mamba/core/environment.hpp"
#include "mamba/core/fsutil.hpp"
#include "mamba/core/validate.hpp"
#include "mamba/util/string.hpp"

#include "mambatests.hpp"
#include "test_data.hpp"

namespace mamba::validation
{
    namespace testing
    {
        using nlohmann::json;

        TEST_SUITE("Validate")
        {
            TEST_CASE("sha256sum")
            {
                auto f = mamba::open_ofstream("sometestfile.txt");
                f << "test";
                f.close();
                auto sha256 = sha256sum("sometestfile.txt");
                CHECK_EQ(sha256, "9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08");

                auto md5 = md5sum("sometestfile.txt");
                CHECK_EQ(md5, "098f6bcd4621d373cade4e832627b4f6");
            }

            TEST_CASE("ed25519_key_hex_to_bytes")
            {
                std::array<unsigned char, MAMBA_ED25519_KEYSIZE_BYTES> pk, sk;
                generate_ed25519_keypair(pk.data(), sk.data());

                auto pk_hex = ::mamba::util::hex_string(pk);
                auto pk_bytes = ed25519_key_hex_to_bytes(pk_hex);
                CHECK_EQ(pk_hex, ::mamba::util::hex_string(pk_bytes));

                spdlog::set_level(spdlog::level::debug);

                std::array<unsigned char, 5> not_even_key;
                pk_hex = ::mamba::util::hex_string(not_even_key);
                pk_bytes = ed25519_key_hex_to_bytes(pk_hex);
                CHECK_FALSE(pk_hex == ::mamba::util::hex_string(pk_bytes));

                std::array<unsigned char, 6> wrong_size_key;
                pk_hex = ::mamba::util::hex_string(wrong_size_key);
                pk_bytes = ed25519_key_hex_to_bytes(pk_hex);
                CHECK_FALSE(pk_hex == ::mamba::util::hex_string(pk_bytes));

                spdlog::set_level(spdlog::level::info);
            }

            TEST_CASE("ed25519_sig_hex_to_bytes")
            {
                std::array<unsigned char, MAMBA_ED25519_KEYSIZE_BYTES> pk, sk;
                generate_ed25519_keypair(pk.data(), sk.data());

                std::array<unsigned char, MAMBA_ED25519_SIGSIZE_BYTES> sig;
                sign("Some text.", sk.data(), sig.data());

                auto sig_hex = ::mamba::util::hex_string(sig);
                auto sig_bytes = ed25519_sig_hex_to_bytes(sig_hex);
                CHECK_EQ(sig_hex, ::mamba::util::hex_string(sig_bytes));

                spdlog::set_level(spdlog::level::debug);

                std::array<unsigned char, 5> not_even_sig;
                sig_hex = ::mamba::util::hex_string(not_even_sig);
                sig_bytes = ed25519_sig_hex_to_bytes(sig_hex);
                CHECK_FALSE(sig_hex == ::mamba::util::hex_string(sig_bytes));

                std::array<unsigned char, 6> wrong_size_sig;
                sig_hex = ::mamba::util::hex_string(wrong_size_sig);
                sig_bytes = ed25519_sig_hex_to_bytes(sig_hex);
                CHECK_FALSE(sig_hex == ::mamba::util::hex_string(sig_bytes));

                spdlog::set_level(spdlog::level::info);
            }
        }

        class VerifyMsg
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

        TEST_SUITE("VerifyMsg")
        {
            TEST_CASE_FIXTURE(VerifyMsg, "from_bytes")
            {
                CHECK_EQ(verify("Some text.", pk, signature), 1);
            }

            TEST_CASE_FIXTURE(VerifyMsg, "from_hex")
            {
                auto signature_hex = ::mamba::util::hex_string(signature, MAMBA_ED25519_SIGSIZE_BYTES);
                auto pk_hex = ::mamba::util::hex_string(pk, MAMBA_ED25519_KEYSIZE_BYTES);

                CHECK_EQ(verify("Some text.", pk_hex, signature_hex), 1);
            }

            TEST_CASE_FIXTURE(VerifyMsg, "wrong_signature")
            {
                spdlog::set_level(spdlog::level::debug);
                auto pk_hex = ::mamba::util::hex_string(pk, MAMBA_ED25519_KEYSIZE_BYTES);

                CHECK_EQ(verify("Some text.", pk_hex, "signature_hex"), 0);
                spdlog::set_level(spdlog::level::info);
            }

            TEST_CASE_FIXTURE(VerifyMsg, "wrong_public_key")
            {
                spdlog::set_level(spdlog::level::debug);
                auto signature_hex = ::mamba::util::hex_string(signature, MAMBA_ED25519_SIGSIZE_BYTES);

                CHECK_EQ(verify("Some text.", "pk_hex", signature_hex), 0);
                spdlog::set_level(spdlog::level::info);
            }
        }

        class VerifyGPGMsg
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
            std::string signature = "d891de3fc102a2ff7b96559ff2f4d81a8e25b5d51a44e10a9fbc5bdc3febf22120582f30e26f6dfe9450ca8100566af7cbc286bf7f52c700d074acd3d4a01603";
            std::string trailer = "04001608001d1621040673d781a8b80bcb7b002040ac7bc8bcf821360d050260a52453";
            std::string hash = "5ad6a0995a537a5fc728ead2dda546972607c5ac235945f7c6c66f90eae1b326";
            std::string data;
        };

        TEST_SUITE("VerifyGPGMsg")
        {
            TEST_CASE_FIXTURE(VerifyGPGMsg, "verify_gpg_hashed_msg_from_bin")
            {
                auto bin_signature = ed25519_sig_hex_to_bytes(signature);
                auto bin_pk = ed25519_key_hex_to_bytes(pk);

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
    }  // namespace testing

    namespace v06
    {
        namespace testing
        {
            using namespace mamba;

            class RootImplT_v06
            {
            public:

                using role_secrets_type = std::
                    map<std::string, std::array<unsigned char, MAMBA_ED25519_KEYSIZE_BYTES>>;
                using secrets_type = std::map<std::string, role_secrets_type>;

                RootImplT_v06()
                {
                    channel_dir = std::make_unique<TemporaryDirectory>();

                    generate_secrets();
                    sign_root();
                }

                fs::u8path trusted_root_file(const json& j)
                {
                    fs::u8path p = channel_dir->path() / "root.json";

                    std::ofstream out_file(p.std_path(), std::ofstream::out | std::ofstream::trunc);
                    out_file << j;
                    out_file.close();

                    return p;
                }

                fs::u8path trusted_root_file_raw_key()
                {
                    return trusted_root_file(root1_json);
                }

                fs::u8path trusted_root_file_pgp()
                {
                    return trusted_root_file(root1_pgp_json);
                }

                json create_root_update_json(const json& patch)
                {
                    json new_root = root1_json;

                    if (!patch.empty())
                    {
                        new_root = new_root.patch(patch);
                    }

                    json sig_patch = json::parse(
                        R"([
                                        { "op": "replace", "path": "/signatures", "value":)"
                        + sign_root_meta(new_root.at("signed")).dump() + R"( }
                                        ])"
                    );
                    return new_root.patch(sig_patch);
                }

                fs::u8path create_root_update(const fs::u8path& name, const json& patch = json())
                {
                    fs::u8path p = channel_dir->path() / name;
                    std::ofstream out_file(p.std_path(), std::ofstream::out | std::ofstream::trunc);
                    out_file << create_root_update_json(patch);
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
                        root1_json["signed"]["delegations"][r] = RolePubKeys({ role_public_keys, 1 });
                    }

                    root1_json["signed"]["version"] = 1;
                    root1_json["signed"]["metadata_spec_version"] = "0.6.0";
                    root1_json["signed"]["type"] = "root";
                    root1_json["signed"]["timestamp"] = timestamp(utc_time_now());
                    root1_json["signed"]["expiration"] = timestamp(utc_time_now() + 3600);
                    root1_json["signatures"] = sign_root_meta(root1_json["signed"]);

                    std::ifstream i(root1_pgp.std_path());
                    i >> root1_pgp_json;
                }

                json sign_root_meta(const json& root_meta)
                {
                    std::map<std::string, std::map<std::string, std::string>> signatures;

                    unsigned char sig_bin[MAMBA_ED25519_SIGSIZE_BYTES];

                    for (auto& secret : secrets.at("root"))
                    {
                        sign(root_meta.dump(2), secret.second.data(), sig_bin);

                        auto sig_hex = ::mamba::util::hex_string(sig_bin, MAMBA_ED25519_SIGSIZE_BYTES);
                        signatures[secret.first].insert({ "signature", sig_hex });
                    }

                    return signatures;
                }

                json upgrade_to_v1(const RootImpl& root, const json& patch = json())
                {
                    auto root_meta = root.upgraded_signable();
                    if (!patch.empty())
                    {
                        root_meta = root_meta.patch(patch);
                    }

                    std::vector<RoleSignature> signatures;
                    for (auto& secret : secrets.at("root"))
                    {
                        signatures.push_back(
                            root.upgraded_signature(root_meta, secret.first, secret.second.data())
                        );
                    }

                    json upgraded_root;
                    upgraded_root["signed"] = root_meta;
                    upgraded_root["signatures"] = signatures;

                    return upgraded_root;
                }

            protected:

                fs::u8path root1_pgp = test_data_dir / "validation/1.sv0.6.root.json";
                json root1_json, root1_pgp_json;

                secrets_type secrets;

                std::unique_ptr<TemporaryDirectory> channel_dir;

                std::map<std::string, std::array<unsigned char, MAMBA_ED25519_KEYSIZE_BYTES>>
                generate_role_secrets(int count)
                {
                    std::map<std::string, std::array<unsigned char, MAMBA_ED25519_KEYSIZE_BYTES>> role_secrets;

                    unsigned char pk[MAMBA_ED25519_KEYSIZE_BYTES];
                    std::array<unsigned char, MAMBA_ED25519_KEYSIZE_BYTES> sk;

                    for (int i = 0; i < count; ++i)
                    {
                        generate_ed25519_keypair(pk, sk.data());
                        auto pk_hex = ::mamba::util::hex_string(pk, MAMBA_ED25519_KEYSIZE_BYTES);

                        role_secrets.insert({ pk_hex, sk });
                    }
                    return role_secrets;
                }
            };

            TEST_SUITE("RootImplT_v06")
            {
                TEST_CASE_FIXTURE(RootImplT_v06, "ctor_from_path")
                {
                    RootImpl root(trusted_root_file_raw_key());

                    CHECK_EQ(root.type(), "root");
                    CHECK_EQ(root.file_ext(), "json");
                    CHECK_EQ(root.spec_version(), SpecImpl("0.6.0"));
                    CHECK_EQ(root.version(), 1);
                }

                TEST_CASE_FIXTURE(RootImplT_v06, "ctor_from_path_pgp_signed")
                {
                    RootImpl root(trusted_root_file_pgp());

                    CHECK_EQ(root.type(), "root");
                    CHECK_EQ(root.file_ext(), "json");
                    CHECK_EQ(root.spec_version(), SpecImpl("0.6.0"));
                    CHECK_EQ(root.version(), 1);
                }

                TEST_CASE_FIXTURE(RootImplT_v06, "ctor_from_json")
                {
                    RootImpl root(root1_json);

                    CHECK_EQ(root.type(), "root");
                    CHECK_EQ(root.file_ext(), "json");
                    CHECK_EQ(root.spec_version(), SpecImpl("0.6.0"));
                    CHECK_EQ(root.version(), 1);
                }

                TEST_CASE_FIXTURE(RootImplT_v06, "ctor_from_json_str")
                {
                    RootImpl root(root1_json.dump());

                    CHECK_EQ(root.type(), "root");
                    CHECK_EQ(root.file_ext(), "json");
                    CHECK_EQ(root.spec_version(), SpecImpl("0.6.0"));
                    CHECK_EQ(root.version(), 1);
                }

                TEST_CASE_FIXTURE(RootImplT_v06, "ctor_from_json_pgp_signed")
                {
                    RootImpl root(root1_pgp_json);

                    CHECK_EQ(root.type(), "root");
                    CHECK_EQ(root.file_ext(), "json");
                    CHECK_EQ(root.spec_version(), SpecImpl("0.6.0"));
                    CHECK_EQ(root.version(), 1);
                }

                TEST_CASE_FIXTURE(RootImplT_v06, "ctor_wrong_filename_spec_version")
                {
                    fs::u8path p = channel_dir->path() / "2.sv1.root.json";

                    std::ofstream out_file(p.std_path(), std::ofstream::out | std::ofstream::trunc);
                    out_file << root1_json;
                    out_file.close();

                    // "2.sv1.root.json" is not compatible spec version (spec version N)
                    CHECK_THROWS_AS(RootImpl root(p), role_file_error);
                }

                TEST_CASE_FIXTURE(RootImplT_v06, "update_from_path")
                {
                    using namespace mamba;

                    auto f = trusted_root_file_raw_key();
                    RootImpl root(f);

                    json patch = R"([
                        { "op": "replace", "path": "/signed/version", "value": 2 }
                        ])"_json;
                    auto updated_root = root.update(create_root_update("2.root.json", patch));

                    auto testing_root = static_cast<RootImpl*>(updated_root.get());
                    CHECK_EQ(testing_root->type(), "root");
                    CHECK_EQ(testing_root->file_ext(), "json");
                    CHECK_EQ(testing_root->spec_version(), SpecImpl("0.6.0"));
                    CHECK_EQ(testing_root->version(), 2);
                }

                TEST_CASE_FIXTURE(RootImplT_v06, "wrong_version")
                {
                    RootImpl root(root1_json);

                    json patch = R"([
                        { "op": "replace", "path": "/signed/version", "value": 3 }
                        ])"_json;

                    CHECK_THROWS_AS(
                        root.update(create_root_update("2.root.json", patch)),
                        role_metadata_error
                    );
                }

                TEST_CASE_FIXTURE(RootImplT_v06, "spec_version")
                {
                    RootImpl root(root1_json);

                    json patch = R"([
                        { "op": "replace", "path": "/signed/version", "value": 2 },
                        { "op": "replace", "path": "/signed/metadata_spec_version", "value": "0.6.1" }
                        ])"_json;
                    auto updated_root = root.update(create_root_update("2.root.json", patch));

                    auto testing_root = static_cast<RootImpl*>(updated_root.get());
                    CHECK_EQ(testing_root->spec_version(), SpecImpl("0.6.1"));
                    CHECK_EQ(testing_root->version(), 2);
                    CHECK_EQ(testing_root->expires(), root.expires());
                }

                TEST_CASE_FIXTURE(RootImplT_v06, "upgraded_spec_version")
                {
                    v06::RootImpl root(root1_json);

                    json patch = R"([
                        { "op": "replace", "path": "/signed/version", "value": 2 },
                        { "op": "replace", "path": "/signed/metadata_spec_version", "value": "1.0.0" }
                        ])"_json;

                    CHECK_THROWS_AS(
                        root.update(create_root_update("2.root.json", patch)),
                        spec_version_error
                    );

                    json signable_patch = json::parse(
                        R"([
                        { "op": "replace", "path": "/version", "value": 2 },
                        { "op": "replace", "path": "/expires", "value": ")"
                        + timestamp(utc_time_now() + 1) /* force +1s */ + R"(" },
                        { "op": "add", "path": "/keys/dummy_value", "value": { "keytype": "ed25519", "scheme": "ed25519", "keyval": "dummy_value" } },
                        { "op": "add", "path": "/roles/snapshot/keyids", "value": ["dummy_value"] },
                        { "op": "add", "path": "/roles/timestamp/keyids", "value": ["dummy_value"] }
                        ])"
                    );
                    auto updated_root = root.update(upgrade_to_v1(root, signable_patch));

                    auto testing_root = dynamic_cast<v1::RootImpl*>(updated_root.get());
                    REQUIRE_NE(testing_root, nullptr);
                    CHECK_EQ(testing_root->spec_version(), SpecImpl("1.0.17"));
                    CHECK_EQ(testing_root->version(), 2);
                    CHECK_LT(testing_root->expires(), root.expires());
                }

                TEST_CASE_FIXTURE(RootImplT_v06, "equivalent_upgraded_spec_version")
                {
                    RootImpl root(root1_json);

                    json signable_patch = R"([
                        { "op": "add", "path": "/keys/dummy_value", "value": { "keytype": "ed25519", "scheme": "ed25519", "keyval": "dummy_value" } },
                        { "op": "add", "path": "/roles/snapshot/keyids", "value": ["dummy_value"] },
                        { "op": "add", "path": "/roles/timestamp/keyids", "value": ["dummy_value"] }
                        ])"_json;
                    v1::RootImpl updated_root(upgrade_to_v1(root, signable_patch));

                    CHECK_EQ(updated_root.spec_version(), v1::SpecImpl("1.0.17"));
                    CHECK_EQ(updated_root.version(), 1);
                }

                TEST_CASE_FIXTURE(RootImplT_v06, "wrong_spec_version")
                {
                    RootImpl root(root1_json);

                    json patch = R"([
                        { "op": "replace", "path": "/signed/version", "value": 2 },
                        { "op": "replace", "path": "/signed/metadata_spec_version", "value": "1.0.0" }
                        ])"_json;

                    CHECK_THROWS_AS(
                        root.update(create_root_update("2.root.json", patch)),
                        spec_version_error
                    );

                    patch = R"([
                        { "op": "replace", "path": "/signed/version", "value": 2 },
                        { "op": "replace", "path": "/signed/metadata_spec_version", "value": "wrong" }
                        ])"_json;

                    CHECK_THROWS_AS(
                        root.update(create_root_update("2.root.json", patch)),
                        spec_version_error
                    );
                }

                TEST_CASE_FIXTURE(RootImplT_v06, "wrong_filename_role")
                {
                    RootImpl root(root1_json);

                    CHECK_THROWS_AS(root.update(create_root_update("2.rooot.json")), role_file_error);
                }

                TEST_CASE_FIXTURE(RootImplT_v06, "wrong_filename_version")
                {
                    RootImpl root(root1_json);

                    CHECK_THROWS_AS(root.update(create_root_update("3.root.json")), role_file_error);
                }

                TEST_CASE_FIXTURE(RootImplT_v06, "wrong_filename_spec_version")
                {
                    v06::RootImpl root(root1_json);

                    // "2.sv1.root.json" is upgradable spec version (spec version N+1)
                    json signable_patch = R"([
                        { "op": "replace", "path": "/version", "value": 2 },
                        { "op": "replace", "path": "/spec_version", "value": "1.0.0" },
                        { "op": "add", "path": "/keys/dummy_value", "value": { "keytype": "ed25519", "scheme": "ed25519", "keyval": "dummy_value" } },
                        { "op": "add", "path": "/roles/snapshot/keyids", "value": ["dummy_value"] },
                        { "op": "add", "path": "/roles/timestamp/keyids", "value": ["dummy_value"] }
                        ])"_json;
                    auto updated_root = root.update(upgrade_to_v1(root, signable_patch));
                    auto testing_root = dynamic_cast<v1::RootImpl*>(updated_root.get());
                    REQUIRE_NE(testing_root, nullptr);
                    CHECK_EQ(testing_root->spec_version(), SpecImpl("1.0.0"));

                    // "2.sv2.root.json" is not upgradable spec version (spec version N+1)
                    json patch = R"([
                        { "op": "replace", "path": "/signed/version", "value": 2 }
                        ])"_json;
                    CHECK_THROWS_AS(
                        root.update(create_root_update("2.sv2.root.json", patch)),
                        role_file_error
                    );
                }

                TEST_CASE_FIXTURE(RootImplT_v06, "illformed_filename_version")
                {
                    RootImpl root(root1_json);

                    CHECK_THROWS_AS(root.update(create_root_update("wrong.root.json")), role_file_error);
                    CHECK_THROWS_AS(root.update(create_root_update("2..root.json")), role_file_error);
                    CHECK_THROWS_AS(root.update(create_root_update("2.sv04.root.json")), role_file_error);
                }

                TEST_CASE_FIXTURE(RootImplT_v06, "rollback_attack")
                {
                    RootImpl root(root1_json);

                    json patch = R"([
                        { "op": "replace", "path": "/signed/version", "value": 1 }
                        ])"_json;

                    CHECK_THROWS_AS(
                        root.update(create_root_update("2.root.json", patch)),
                        rollback_error
                    );
                }

                TEST_CASE_FIXTURE(RootImplT_v06, "wrong_type")
                {
                    RootImpl root(root1_json);

                    json patch = R"([
                        { "op": "replace", "path": "/signed/type", "value": "timestamp" },
                        { "op": "replace", "path": "/signed/version", "value": 2 }
                        ])"_json;

                    CHECK_THROWS_AS(
                        root.update(create_root_update("2.root.json", patch)),
                        role_metadata_error
                    );
                }

                TEST_CASE_FIXTURE(RootImplT_v06, "missing_type")
                {
                    RootImpl root(root1_json);

                    json patch = R"([
                        { "op": "remove", "path": "/signed/type" },
                        { "op": "replace", "path": "/signed/version", "value": 2 }
                        ])"_json;

                    CHECK_THROWS_AS(
                        root.update(create_root_update("2.root.json", patch)),
                        role_metadata_error
                    );
                }

                TEST_CASE_FIXTURE(RootImplT_v06, "missing_delegations")
                {
                    RootImpl root(root1_json);

                    json patch = R"([
                        { "op": "remove", "path": "/signed/delegations" },
                        { "op": "replace", "path": "/signed/version", "value": 2 }
                        ])"_json;

                    CHECK_THROWS_AS(
                        root.update(create_root_update("2.root.json", patch)),
                        role_metadata_error
                    );
                }

                TEST_CASE_FIXTURE(RootImplT_v06, "missing_delegation")
                {
                    RootImpl root(root1_json);

                    json patch = R"([
                                    { "op": "remove", "path": "/signed/delegations/root" },
                                    { "op": "replace", "path": "/signed/version", "value": 2 }
                                    ])"_json;

                    CHECK_THROWS_AS(
                        root.update(create_root_update("2.root.json", patch)),
                        role_metadata_error
                    );
                }

                TEST_CASE_FIXTURE(RootImplT_v06, "empty_delegation_pubkeys")
                {
                    RootImpl root(root1_json);

                    json patch = R"([
                                    { "op": "replace", "path": "/signed/delegations/root/pubkeys", "value": [] },
                                    { "op": "replace", "path": "/signed/version", "value": 2 }
                                    ])"_json;

                    CHECK_THROWS_AS(
                        root.update(create_root_update("2.root.json", patch)),
                        role_metadata_error
                    );
                }

                TEST_CASE_FIXTURE(RootImplT_v06, "null_role_threshold")
                {
                    RootImpl root(root1_json);

                    json patch = R"([
                                    { "op": "replace", "path": "/signed/delegations/root/threshold", "value": 0 },
                                    { "op": "replace", "path": "/signed/version", "value": 2 }
                                    ])"_json;

                    CHECK_THROWS_AS(
                        root.update(create_root_update("2.root.json", patch)),
                        role_metadata_error
                    );
                }

                TEST_CASE_FIXTURE(RootImplT_v06, "extra_roles")
                {
                    RootImpl root(root1_json);

                    json patch = R"([
                                    { "op": "add", "path": "/signed/delegations/some_wrong_role",
                                        "value": { "pubkeys": ["c"], "threshold": 1 } },
                                    { "op": "replace", "path": "/signed/version", "value": 2 }
                                    ])"_json;

                    CHECK_THROWS_AS(
                        root.update(create_root_update("2.root.json", patch)),
                        role_metadata_error
                    );
                }
                /*
                TEST_CASE_FIXTURE(RootImplT_v06, mirrors_role)
                {
                    json patch = R"([
                                                            { "op": "add", "path":
                   "/signed/roles/mirrors", "value": { "keyids":
                                           ["c"], "threshold": 1 } }, { "op": "replace", "path":
                   "/signed/version", "value": 2 }
                                                            ])"_json;

                    RootImpl root(create_root_update("2.root.json", patch));
                    bool mirrors_role_found = (root.roles().find("mirrors") != root.roles().end());
                    CHECK(mirrors_role_found);
                }
                */
                TEST_CASE_FIXTURE(RootImplT_v06, "threshold_not_met")
                {
                    RootImpl root(root1_json);

                    json patch = R"([
                        { "op": "replace", "path": "/signed/version", "value": 2 },
                        { "op": "replace", "path": "/signed/delegations/root/threshold", "value": 2 }
                        ])"_json;

                    CHECK_THROWS_AS(root.update(create_root_update("2.root.json", patch)), role_error);
                }

                TEST_CASE_FIXTURE(RootImplT_v06, "expires")
                {
                    RootImpl root(root1_json);

                    // expiration is set to now+3600s in 'sign_root'
                    TimeRef time_ref;
                    CHECK_FALSE(root.expired(time_ref));

                    time_ref.set(utc_time_now() + 7200);
                    CHECK(root.expired(time_ref));

                    json patch = json::parse(
                        R"([
                        { "op": "replace", "path": "/signed/expiration", "value": ")"
                        + timestamp(utc_time_now() + 10800) + R"(" },
                        { "op": "replace", "path": "/signed/version", "value": 2 }
                        ])"
                    );
                    auto updated_root = root.update(create_root_update("2.root.json", patch));

                    auto testing_root = static_cast<RootImpl*>(updated_root.get());
                    CHECK_FALSE(testing_root->expired(time_ref));
                }

                TEST_CASE_FIXTURE(RootImplT_v06, "timestamp")
                {
                    RootImpl root(root1_json);

                    json patch;

                    patch = json::parse(R"([
                        { "op": "replace", "path": "/signed/timestamp", "value": "2021-09-20T07:07:09+0030" },
                        { "op": "replace", "path": "/signed/version", "value": 2 }
                        ])");
                    CHECK_THROWS_AS(
                        root.update(create_root_update("2.root.json", patch)),
                        role_metadata_error
                    );

                    patch = json::parse(R"([
                        { "op": "replace", "path": "/signed/timestamp", "value": "2021-09-20T07:07:09D" },
                        { "op": "replace", "path": "/signed/version", "value": 2 }
                        ])");
                    CHECK_THROWS_AS(
                        root.update(create_root_update("2.root.json", patch)),
                        role_metadata_error
                    );

                    patch = json::parse(R"([
                        { "op": "replace", "path": "/signed/timestamp", "value": "2021-09-20T07:07:09.000" },
                        { "op": "replace", "path": "/signed/version", "value": 2 }
                        ])");
                    CHECK_THROWS_AS(
                        root.update(create_root_update("2.root.json", patch)),
                        role_metadata_error
                    );
                }

                TEST_CASE_FIXTURE(RootImplT_v06, "possible_update_files")
                {
                    RootImpl root(root1_json);

                    auto update_f = root.possible_update_files();
                    CHECK(update_f[0].string().c_str() == doctest::Contains("2.sv1.root.json"));
                    CHECK(update_f[1].string().c_str() == doctest::Contains("2.sv0.7.root.json"));
                    CHECK(update_f[2].string().c_str() == doctest::Contains("2.sv0.6.root.json"));
                    CHECK(update_f[3].string().c_str() == doctest::Contains("2.root.json"));

                    json patch = json::parse(R"([
                        { "op": "replace", "path": "/signed/version", "value": 2 }
                        ])");
                    auto updated_root = root.update(create_root_update("2.root.json", patch));
                    update_f = updated_root->possible_update_files();
                    CHECK(update_f[0].string().c_str() == doctest::Contains("3.sv1.root.json"));
                    CHECK(update_f[1].string().c_str() == doctest::Contains("3.sv0.7.root.json"));
                    CHECK(update_f[2].string().c_str() == doctest::Contains("3.sv0.6.root.json"));
                    CHECK(update_f[3].string().c_str() == doctest::Contains("3.root.json"));
                }
            }

            class SpecImplT_v06
            {
            public:

                SpecImplT_v06() = default;

            protected:

                SpecImpl spec;
            };

            TEST_SUITE("SpecImplT_v06")
            {
                TEST_CASE_FIXTURE(SpecImplT_v06, "ctor")
                {
                    SpecImpl new_spec("0.6.1");
                    CHECK_EQ(new_spec.version_str(), "0.6.1");
                }

                TEST_CASE_FIXTURE(SpecImplT_v06, "version_str")
                {
                    CHECK_EQ(spec.version_str(), "0.6.0");
                }

                TEST_CASE_FIXTURE(SpecImplT_v06, "is_compatible")
                {
                    CHECK(spec.is_compatible(std::string("0.6.0")));
                    CHECK(spec.is_compatible(std::string("0.6.1")));
                    CHECK(spec.is_compatible(std::string("0.6.10")));

                    // minor version change with major version '0' may be
                    // backward incompatible
                    CHECK_FALSE(spec.is_compatible(std::string("0.7.0")));
                    CHECK_FALSE(spec.is_compatible(std::string("1.0.0")));
                    CHECK_FALSE(spec.is_compatible(std::string("2.0.0")));
                }

                TEST_CASE_FIXTURE(SpecImplT_v06, "is_upgrade")
                {
                    CHECK(spec.is_upgrade(std::string("0.7.0")));
                    CHECK(spec.is_upgrade(std::string("1.0.0")));
                    CHECK(spec.is_upgrade(std::string("1.1.0")));
                    CHECK(spec.is_upgrade(std::string("1.0.17")));

                    // 2 possible backward incompatible updates
                    CHECK_FALSE(spec.is_upgrade(std::string("0.8.0")));
                    CHECK_FALSE(spec.is_upgrade(std::string("2.0.0")));
                    // not an upgrade, compatible version
                    CHECK_FALSE(spec.is_upgrade(std::string("0.6.1")));
                }

                TEST_CASE_FIXTURE(SpecImplT_v06, "upgradable")
                {
                    CHECK(spec.upgradable());
                }

                TEST_CASE_FIXTURE(SpecImplT_v06, "compatible_prefix")
                {
                    CHECK_EQ(spec.compatible_prefix(), "0.6");
                }

                TEST_CASE_FIXTURE(SpecImplT_v06, "upgrade_prefix")
                {
                    CHECK(spec.upgrade_prefix()[0].c_str() == doctest::Contains("1"));
                    CHECK(spec.upgrade_prefix()[1].c_str() == doctest::Contains("0.7"));
                }

                TEST_CASE_FIXTURE(SpecImplT_v06, "json_key")
                {
                    CHECK_EQ(spec.json_key(), "metadata_spec_version");
                }

                TEST_CASE_FIXTURE(SpecImplT_v06, "expiration_json_key")
                {
                    CHECK_EQ(spec.expiration_json_key(), "expiration");
                }

                TEST_CASE_FIXTURE(SpecImplT_v06, "canonicalize")
                {
                    CHECK_EQ(spec.canonicalize(R"({"foo":"bar"})"_json), "{\n  \"foo\": \"bar\"\n}");
                }

                TEST_CASE_FIXTURE(SpecImplT_v06, "signatures")
                {
                    json j = R"({
                                    "signatures":
                                    {
                                        "foo":
                                        {
                                            "other_headers": "bar",
                                            "signature": "baz"
                                        }
                                    }
                                })"_json;
                    auto sigs = spec.signatures(j);
                    CHECK_EQ(sigs.size(), 1);
                    CHECK_EQ(sigs.begin()->keyid, "foo");
                    CHECK_EQ(sigs.begin()->sig, "baz");
                    CHECK_EQ(sigs.begin()->pgp_trailer, "bar");
                }
            }


            class KeyMgrT_v06 : public RootImplT_v06
            {
            public:

                KeyMgrT_v06()
                    : RootImplT_v06()
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
                    key_mgr_json["signed"]["delegations"]["pkg_mgr"] = RolePubKeys({ pkg_mgr_pks, 1 });

                    key_mgr_json["signed"]["version"] = 1;
                    key_mgr_json["signed"]["metadata_spec_version"] = "0.6.0";
                    key_mgr_json["signed"]["type"] = "key_mgr";

                    key_mgr_json["signed"]["timestamp"] = timestamp(utc_time_now());
                    key_mgr_json["signed"]["expiration"] = timestamp(utc_time_now() + 3600);
                    key_mgr_json["signatures"] = sign_key_mgr_meta(key_mgr_json["signed"]);
                }

                json patched_key_mgr_json(const json& patch = json())
                {
                    json update_key_mgr = key_mgr_json;

                    if (!patch.empty())
                    {
                        update_key_mgr = update_key_mgr.patch(patch);
                    }

                    json sig_patch = json::parse(
                        R"([
                            { "op": "replace", "path": "/signatures", "value": )"
                        + sign_key_mgr_meta(update_key_mgr.at("signed")).dump() + R"( }
                            ])"
                    );
                    return update_key_mgr.patch(sig_patch);
                }

                fs::u8path
                write_key_mgr_file(const json& j, const std::string& filename = "key_mgr.json")
                {
                    fs::u8path p = channel_dir->path() / filename;

                    std::ofstream out_file(p.std_path(), std::ofstream::out | std::ofstream::trunc);
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

                        auto sig_hex = ::mamba::util::hex_string(sig_bin, MAMBA_ED25519_SIGSIZE_BYTES);
                        signatures[secret.first].insert({ "signature", sig_hex });
                    }

                    return signatures;
                }
            };

            TEST_SUITE("KeyMgrT_v06")
            {
                TEST_CASE_FIXTURE(KeyMgrT_v06, "ctor_from_json")
                {
                    RootImpl root(root1_json);
                    auto key_mgr = root.create_key_mgr(key_mgr_json);

                    CHECK_EQ(key_mgr.spec_version(), SpecImpl("0.6.0"));
                    CHECK_EQ(key_mgr.version(), 1);
                }

                TEST_CASE_FIXTURE(KeyMgrT_v06, "ctor_from_json_str")
                {
                    RootImpl root(root1_json);
                    auto key_mgr = KeyMgrRole(
                        key_mgr_json.dump(),
                        root.all_keys()["key_mgr"],
                        std::make_shared<SpecImpl>(SpecImpl())
                    );

                    CHECK_EQ(key_mgr.spec_version(), SpecImpl("0.6.0"));
                    CHECK_EQ(key_mgr.version(), 1);
                }

                TEST_CASE_FIXTURE(KeyMgrT_v06, "version")
                {
                    RootImpl root(root1_json);

                    {
                        json key_mgr_patch = R"([
                        { "op": "replace", "path": "/signed/version", "value": 2 }
                        ])"_json;
                        auto key_mgr = root.create_key_mgr(patched_key_mgr_json(key_mgr_patch));

                        CHECK_EQ(key_mgr.spec_version(), SpecImpl("0.6.0"));
                        CHECK_EQ(key_mgr.version(), 2);
                    }

                    {  // Any version is valid, without chaining required
                        json key_mgr_patch = R"([
                        { "op": "replace", "path": "/signed/version", "value": 20 }
                        ])"_json;
                        auto key_mgr = root.create_key_mgr(patched_key_mgr_json(key_mgr_patch));

                        CHECK_EQ(key_mgr.spec_version(), SpecImpl("0.6.0"));
                        CHECK_EQ(key_mgr.version(), 20);
                    }
                }

                TEST_CASE_FIXTURE(KeyMgrT_v06, "spec_version")
                {  // spec version as to match exactly 'root' spec version
                    RootImpl root(root1_json);

                    {
                        json key_mgr_patch = R"([
                        { "op": "replace", "path": "/signed/metadata_spec_version", "value": "0.6.0" }
                        ])"_json;
                        auto key_mgr = root.create_key_mgr(patched_key_mgr_json(key_mgr_patch));

                        CHECK_EQ(key_mgr.spec_version(), SpecImpl("0.6.0"));
                        CHECK_EQ(key_mgr.version(), 1);
                    }

                    {  // is compatible but not strictly the same as 'root' one
                        json key_mgr_patch = R"([
                        { "op": "replace", "path": "/signed/metadata_spec_version", "value": "0.6.1" }
                        ])"_json;

                        CHECK_THROWS_AS(
                            root.create_key_mgr(patched_key_mgr_json(key_mgr_patch)),
                            spec_version_error
                        );
                    }

                    {  // wrong type
                        json key_mgr_patch = R"([
                        { "op": "replace", "path": "/signed/metadata_spec_version", "value": 0.6 }
                        ])"_json;

                        CHECK_THROWS_AS(
                            root.create_key_mgr(patched_key_mgr_json(key_mgr_patch)),
                            role_metadata_error
                        );
                    }
                }

                TEST_CASE_FIXTURE(KeyMgrT_v06, "ctor_from_path")
                {
                    RootImpl root(root1_json);

                    auto key_mgr = root.create_key_mgr(write_key_mgr_file(key_mgr_json));
                    CHECK_EQ(key_mgr.spec_version(), SpecImpl("0.6.0"));
                    CHECK_EQ(key_mgr.version(), 1);

                    // TODO: enforce consistency between spec version in filename and metadata
                    key_mgr = root.create_key_mgr(
                        write_key_mgr_file(key_mgr_json, "20.sv0.6.key_mgr.json")
                    );
                    CHECK_EQ(key_mgr.spec_version(), SpecImpl("0.6.0"));
                    CHECK_EQ(key_mgr.version(), 1);


                    CHECK_THROWS_AS(root.create_key_mgr(fs::u8path("not_existing")), role_file_error);

                    CHECK_THROWS_AS(
                        root.create_key_mgr(write_key_mgr_file(key_mgr_json, "wrong.json")),
                        role_file_error
                    );

                    CHECK_THROWS_AS(
                        root.create_key_mgr(write_key_mgr_file(key_mgr_json, "sv1.key_mgr.json")),
                        role_file_error
                    );

                    CHECK_THROWS_AS(
                        root.create_key_mgr(write_key_mgr_file(key_mgr_json, "wrong.sv0.6.key_mgr.json")
                        ),
                        role_file_error
                    );
                }

                TEST_CASE_FIXTURE(KeyMgrT_v06, "expires")
                {
                    RootImpl root(root1_json);
                    auto key_mgr = root.create_key_mgr(key_mgr_json);

                    // expiration is set to now+3600s in 'sign_key_mgr'
                    TimeRef time_ref;
                    CHECK_FALSE(key_mgr.expired(time_ref));
                    CHECK_FALSE(root.expired(time_ref));

                    time_ref.set(utc_time_now() + 7200);
                    CHECK(key_mgr.expired(time_ref));
                    CHECK(root.expired(time_ref));

                    json patch = json::parse(
                        R"([
                        { "op": "replace", "path": "/signed/expiration", "value": ")"
                        + timestamp(utc_time_now() + 10800) + R"(" }
                        ])"
                    );

                    key_mgr = root.create_key_mgr(patched_key_mgr_json(patch));
                    CHECK_FALSE(key_mgr.expired(time_ref));
                    CHECK(root.expired(time_ref));
                }

                TEST_CASE_FIXTURE(KeyMgrT_v06, "timestamp")
                {
                    RootImpl root(root1_json);

                    json patch;

                    patch = json::parse(R"([
                        { "op": "replace", "path": "/signed/timestamp", "value": "2021-09-20T07:07:09+0030" },
                        { "op": "replace", "path": "/signed/version", "value": 1 }
                        ])");

                    CHECK_THROWS_AS(
                        root.create_key_mgr(patched_key_mgr_json(patch)),
                        role_metadata_error
                    );

                    patch = json::parse(R"([
                        { "op": "replace", "path": "/signed/timestamp", "value": "2021-09-20T07:07:09D" },
                        { "op": "replace", "path": "/signed/version", "value": 1 }
                        ])");
                    CHECK_THROWS_AS(
                        root.create_key_mgr(patched_key_mgr_json(patch)),
                        role_metadata_error
                    );

                    patch = json::parse(R"([
                        { "op": "replace", "path": "/signed/timestamp", "value": "2021-09-20T07:07:09.000" },
                        { "op": "replace", "path": "/signed/version", "value": 1 }
                        ])");
                    CHECK_THROWS_AS(
                        root.create_key_mgr(patched_key_mgr_json(patch)),
                        role_metadata_error
                    );
                }
            }

            class PkgMgrT_v06 : public KeyMgrT_v06
            {
            public:

                PkgMgrT_v06()
                    : KeyMgrT_v06()
                {
                    sign_pkg_mgr();
                    generate_index_checkerdata();
                    root = std::make_unique<RootImpl>(root1_json);
                };

                json sign_repodata(const json& patch = json())
                {
                    json updated_repodata = repodata_json;

                    if (!patch.empty())
                    {
                        updated_repodata = updated_repodata.patch(patch);
                    }

                    for (auto& it : updated_repodata.at("packages").get<json::object_t>())
                    {
                        json sig_patch = json::parse(
                            R"({
                                "signatures": { ")"
                            + it.first + "\":" + sign_repodata_meta(it.second).dump() + R"(
                                    }
                                })"
                        );
                        updated_repodata.merge_patch(sig_patch);
                    }
                    return updated_repodata;
                }

                void sign_pkg_mgr()
                {
                    std::vector<std::string> pkg_mgr_pks;
                    for (auto& secret : secrets.at("pkg_mgr"))
                    {
                        pkg_mgr_pks.push_back(secret.first);
                    }
                    pkg_mgr_json["signed"]["delegations"] = json::object();

                    pkg_mgr_json["signed"]["version"] = 1;
                    pkg_mgr_json["signed"]["metadata_spec_version"] = "0.6.0";
                    pkg_mgr_json["signed"]["type"] = "pkg_mgr";

                    pkg_mgr_json["signed"]["timestamp"] = timestamp(utc_time_now());
                    pkg_mgr_json["signed"]["expiration"] = timestamp(utc_time_now() + 3600);
                    pkg_mgr_json["signatures"] = sign_pkg_mgr_meta(pkg_mgr_json["signed"]);
                }

                json patched_pkg_mgr_json(const json& patch = json())
                {
                    json update_pkg_mgr = pkg_mgr_json;

                    if (!patch.empty())
                    {
                        update_pkg_mgr = update_pkg_mgr.patch(patch);
                    }

                    json sig_patch = json::parse(
                        R"([
                            { "op": "replace", "path": "/signatures", "value": )"
                        + sign_pkg_mgr_meta(update_pkg_mgr.at("signed")).dump() + R"( }
                            ])"
                    );
                    return update_pkg_mgr.patch(sig_patch);
                }

                fs::u8path
                write_pkg_mgr_file(const json& j, const std::string& filename = "pkg_mgr.json")
                {
                    fs::u8path p = channel_dir->path() / filename;

                    std::ofstream out_file(p.std_path(), std::ofstream::out | std::ofstream::trunc);
                    out_file << j;
                    out_file.close();

                    return p;
                }

            protected:

                json pkg_mgr_json, repodata_json, signed_repodata_json;

                std::unique_ptr<RootImpl> root;

                json sign_pkg_mgr_meta(const json& meta)
                {
                    std::map<std::string, std::map<std::string, std::string>> signatures;

                    unsigned char sig_bin[MAMBA_ED25519_SIGSIZE_BYTES];

                    for (auto& secret : secrets.at("pkg_mgr"))
                    {
                        sign(meta.dump(2), secret.second.data(), sig_bin);

                        auto sig_hex = ::mamba::util::hex_string(sig_bin, MAMBA_ED25519_SIGSIZE_BYTES);
                        signatures[secret.first].insert({ "signature", sig_hex });
                    }

                    return signatures;
                }

                void generate_index_checkerdata()
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

                        auto sig_hex = ::mamba::util::hex_string(sig_bin, MAMBA_ED25519_SIGSIZE_BYTES);
                        signatures[secret.first].insert({ "signature", sig_hex });
                    }

                    return signatures;
                }
            };

            TEST_SUITE("PkgMgrT_v06")
            {
                TEST_CASE_FIXTURE(PkgMgrT_v06, "verify_index")
                {
                    auto key_mgr = root->create_key_mgr(key_mgr_json);
                    auto pkg_mgr = key_mgr.create_pkg_mgr(pkg_mgr_json);

                    pkg_mgr.verify_index(signed_repodata_json);
                }

                TEST_CASE_FIXTURE(PkgMgrT_v06, "corrupted_repodata")
                {
                    auto key_mgr = root->create_key_mgr(key_mgr_json);
                    auto pkg_mgr = key_mgr.create_pkg_mgr(pkg_mgr_json);

                    json wrong_pkg_patch = R"([
                                { "op": "replace", "path": "/packages/test-package1-0.1-0.tar.bz2/version", "value": "0.1.1" }
                                ])"_json;
                    CHECK_THROWS_AS(
                        pkg_mgr.verify_index(signed_repodata_json.patch(wrong_pkg_patch)),
                        package_error
                    );
                }

                TEST_CASE_FIXTURE(PkgMgrT_v06, "illformed_repodata")
                {
                    auto key_mgr = root->create_key_mgr(key_mgr_json);
                    auto pkg_mgr = key_mgr.create_pkg_mgr(pkg_mgr_json);

                    json illformed_pkg_patch = R"([
                                { "op": "remove", "path": "/signatures"}
                                ])"_json;
                    CHECK_THROWS_AS(
                        pkg_mgr.verify_index(signed_repodata_json.patch(illformed_pkg_patch)),
                        index_error
                    );
                }
            }


            class RepoCheckerT : public PkgMgrT_v06
            {
            public:

                RepoCheckerT()
                    : PkgMgrT_v06()
                {
                    m_repo_base_url = "file://" + channel_dir->path().string();
                    m_ref_path = channel_dir->path().string();

                    write_role(root1_json, channel_dir->path() / "root.json");

                    json patch = json::parse(R"([
                            { "op": "replace", "path": "/signed/version", "value": 2 }
                    ])");
                    write_role(create_root_update_json(patch), channel_dir->path() / "2.root.json");

                    write_role(key_mgr_json, channel_dir->path() / "key_mgr.json");
                    write_role(pkg_mgr_json, channel_dir->path() / "pkg_mgr.json");

                    spdlog::set_level(spdlog::level::debug);
                }

                ~RepoCheckerT()
                {
                    spdlog::set_level(spdlog::level::warn);
                }

            protected:

                std::string m_ref_path, m_repo_base_url;

                void write_role(const json& j, const fs::u8path& p)
                {
                    fs::u8path expanded_p = env::expand_user(p);
                    path::touch(expanded_p, true);
                    std::ofstream out_file(
                        expanded_p.std_path(),
                        std::ofstream::out | std::ofstream::trunc
                    );
                    out_file << j.dump(2);
                    out_file.close();
                }
            };

            TEST_SUITE("RepoCheckerT")
            {
                TEST_CASE_FIXTURE(RepoCheckerT, "ctor")
                {
                    RepoChecker checker(mambatests::context(), m_repo_base_url, m_ref_path);
                    checker.generate_index_checker();
                    CHECK_EQ(checker.root_version(), 2);
                }

                TEST_CASE_FIXTURE(RepoCheckerT, "verify_index")
                {
                    RepoChecker checker(mambatests::context(), m_repo_base_url, m_ref_path);
                    checker.generate_index_checker();
                    checker.verify_index(signed_repodata_json);
                }

                TEST_CASE_FIXTURE(RepoCheckerT, "root_freeze_attack")
                {
                    json patch = json::parse(
                        R"([
                                        { "op": "replace", "path": "/signed/version", "value": 2 },
                                        { "op": "replace", "path": "/signed/expiration", "value": ")"
                        + timestamp(utc_time_now() - 10) + R"(" }
                                    ])"
                    );
                    write_role(create_root_update_json(patch), channel_dir->path() / "2.root.json");
                    RepoChecker checker(mambatests::context(), m_repo_base_url, m_ref_path);
                    CHECK_THROWS_AS(checker.generate_index_checker(), freeze_error);
                }

                TEST_CASE_FIXTURE(RepoCheckerT, "key_mgr_freeze_attack")
                {
                    json patch = json::parse(
                        R"([
                                        { "op": "replace", "path": "/signed/expiration", "value": ")"
                        + timestamp(utc_time_now() - 10) + R"(" }
                                    ])"
                    );
                    write_role(patched_key_mgr_json(patch), channel_dir->path() / "key_mgr.json");
                    RepoChecker checker(mambatests::context(), m_repo_base_url, m_ref_path);
                    CHECK_THROWS_AS(checker.generate_index_checker(), freeze_error);
                }

                TEST_CASE_FIXTURE(RepoCheckerT, "missing_key_mgr_file")
                {
                    fs::remove(channel_dir->path() / "key_mgr.json");
                    RepoChecker checker(mambatests::context(), m_repo_base_url, m_ref_path);
                    CHECK_THROWS_AS(checker.generate_index_checker(), fetching_error);
                }

                TEST_CASE_FIXTURE(RepoCheckerT, "corrupted_repodata")
                {
                    RepoChecker checker(mambatests::context(), m_repo_base_url, m_ref_path);

                    json wrong_pkg_patch = R"([
                                { "op": "replace", "path": "/packages/test-package1-0.1-0.tar.bz2/version", "value": "0.1.1" }
                                ])"_json;
                    checker.generate_index_checker();
                    CHECK_THROWS_AS(
                        checker.verify_index(signed_repodata_json.patch(wrong_pkg_patch)),
                        package_error
                    );
                }

                TEST_CASE_FIXTURE(RepoCheckerT, "illformed_repodata")
                {
                    RepoChecker checker(mambatests::context(), m_repo_base_url, m_ref_path);

                    json illformed_pkg_patch = R"([
                                { "op": "remove", "path": "/signatures"}
                                ])"_json;
                    checker.generate_index_checker();
                    CHECK_THROWS_AS(
                        checker.verify_index(signed_repodata_json.patch(illformed_pkg_patch)),
                        index_error
                    );
                }
            }
        }  // namespace testing
    }      // namespace v06

    namespace v1
    {
        namespace testing
        {
            using namespace mamba;

            class RootImplT_v1
            {
            public:

                using role_secrets_type = std::
                    map<std::string, std::array<unsigned char, MAMBA_ED25519_KEYSIZE_BYTES>>;
                using secrets_type = std::map<std::string, role_secrets_type>;

                RootImplT_v1()
                {
                    channel_dir = std::make_unique<TemporaryDirectory>();

                    generate_secrets();
                    sign_root();
                }

                fs::u8path trusted_root_file()
                {
                    fs::u8path p = channel_dir->path() / "root.json";

                    std::ofstream out_file(p.std_path(), std::ofstream::out | std::ofstream::trunc);
                    out_file << root1_json;
                    out_file.close();

                    return p;
                }

                fs::u8path create_root_update(const fs::u8path& name, const json& patch = json())
                {
                    fs::u8path p = channel_dir->path() / name;

                    std::ofstream out_file(p.std_path(), std::ofstream::out | std::ofstream::trunc);

                    json new_root = root1_json;

                    if (!patch.empty())
                    {
                        new_root = new_root.patch(patch);
                    }

                    json sig_patch = json::parse(
                        R"([
                                        { "op": "replace", "path": "/signatures", "value":)"
                        + sign_root_meta(new_root.at("signed")).dump() + R"(}
                                        ])"
                    );
                    out_file << new_root.patch(sig_patch);
                    out_file.close();

                    return p;
                }

                void
                generate_secrets(int root = 1, int targets = 1, int snapshot = 1, int timestamp = 1)
                {
                    secrets.insert({ "root", generate_role_secrets(root) });
                    secrets.insert({ "targets", generate_role_secrets(targets) });
                    secrets.insert({ "snapshot", generate_role_secrets(snapshot) });
                    secrets.insert({ "timestamp", generate_role_secrets(timestamp) });
                }

                void sign_root()
                {
                    std::ifstream i(root1.std_path());
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

                        auto sig_hex = ::mamba::util::hex_string(sig_bin, MAMBA_ED25519_SIGSIZE_BYTES);
                        signatures.push_back({ secret.first, sig_hex });
                    }

                    return signatures;
                }

            protected:

                fs::u8path root1 = test_data_dir / "validation/root.json";
                json root1_json;

                std::unique_ptr<TemporaryDirectory> channel_dir;

                secrets_type secrets;

                std::map<std::string, std::array<unsigned char, MAMBA_ED25519_KEYSIZE_BYTES>>
                generate_role_secrets(int count)
                {
                    std::map<std::string, std::array<unsigned char, MAMBA_ED25519_KEYSIZE_BYTES>> role_secrets;

                    unsigned char pk[MAMBA_ED25519_KEYSIZE_BYTES];
                    std::array<unsigned char, MAMBA_ED25519_KEYSIZE_BYTES> sk;

                    for (int i = 0; i < count; ++i)
                    {
                        generate_ed25519_keypair(pk, sk.data());

                        auto pk_hex = ::mamba::util::hex_string(pk, MAMBA_ED25519_KEYSIZE_BYTES);
                        role_secrets.insert({ pk_hex, sk });
                    }
                    return role_secrets;
                }
            };

            TEST_SUITE("RootImplT_v1")
            {
                TEST_CASE_FIXTURE(RootImplT_v1, "ctor_from_path")
                {
                    RootImpl root(trusted_root_file());

                    CHECK_EQ(root.type(), "root");
                    CHECK_EQ(root.file_ext(), "json");
                    CHECK_EQ(root.spec_version(), SpecImpl("1.0.17"));
                    CHECK_EQ(root.version(), 1);
                }

                TEST_CASE_FIXTURE(RootImplT_v1, "ctor_from_json")
                {
                    RootImpl root(root1_json);

                    CHECK_EQ(root.type(), "root");
                    CHECK_EQ(root.file_ext(), "json");
                    CHECK_EQ(root.spec_version(), SpecImpl("1.0.17"));
                    CHECK_EQ(root.version(), 1);
                }

                TEST_CASE_FIXTURE(RootImplT_v1, "update_from_path")
                {
                    using namespace mamba;

                    RootImpl root(trusted_root_file());

                    json patch = R"([
                        { "op": "replace", "path": "/signed/version", "value": 2 }
                        ])"_json;
                    auto updated_root = root.update(create_root_update("2.root.json", patch));

                    auto testing_root = static_cast<RootImpl*>(updated_root.get());
                    CHECK_EQ(testing_root->type(), "root");
                    CHECK_EQ(testing_root->file_ext(), "json");
                    CHECK_EQ(testing_root->spec_version(), SpecImpl("1.0.17"));
                    CHECK_EQ(testing_root->version(), 2);
                }

                TEST_CASE_FIXTURE(RootImplT_v1, "ctor_wrong_filename_spec_version")
                {
                    fs::u8path p = channel_dir->path() / "2.sv0.6.root.json";

                    std::ofstream out_file(p.std_path(), std::ofstream::out | std::ofstream::trunc);
                    out_file << root1_json;
                    out_file.close();

                    // "2.sv0.6.root.json" is not compatible spec version (spec version N)
                    CHECK_THROWS_AS(RootImpl root(p), role_file_error);
                }

                TEST_CASE_FIXTURE(RootImplT_v1, "wrong_version")
                {
                    RootImpl root(root1_json);

                    json patch = R"([
                        { "op": "replace", "path": "/signed/version", "value": 3 }
                        ])"_json;

                    CHECK_THROWS_AS(
                        root.update(create_root_update("2.root.json", patch)),
                        role_metadata_error
                    );
                }

                TEST_CASE_FIXTURE(RootImplT_v1, "spec_version")
                {
                    RootImpl root(root1_json);

                    json patch = R"([
                        { "op": "replace", "path": "/signed/version", "value": 2 },
                        { "op": "replace", "path": "/signed/spec_version", "value": "1.30.10" }
                        ])"_json;

                    auto updated_root = root.update(create_root_update("2.root.json", patch));

                    auto testing_root = static_cast<RootImpl*>(updated_root.get());
                    CHECK_EQ(testing_root->spec_version(), SpecImpl("1.30.10"));
                    CHECK_EQ(testing_root->version(), 2);
                }

                TEST_CASE_FIXTURE(RootImplT_v1, "wrong_spec_version")
                {
                    RootImpl root(root1_json);

                    json patch = R"([
                        { "op": "replace", "path": "/signed/spec_version", "value": "2.0.0" }
                        ])"_json;

                    CHECK_THROWS_AS(
                        root.update(create_root_update("2.root.json", patch)),
                        spec_version_error
                    );
                }

                TEST_CASE_FIXTURE(RootImplT_v1, "wrong_filename_role")
                {
                    RootImpl root(root1_json);

                    json patch = R"([])"_json;

                    CHECK_THROWS_AS(
                        root.update(create_root_update("2.rooot.json", patch)),
                        role_file_error
                    );
                }

                TEST_CASE_FIXTURE(RootImplT_v1, "wrong_filename_version")
                {
                    RootImpl root(root1_json);

                    json patch = R"([])"_json;

                    CHECK_THROWS_AS(
                        root.update(create_root_update("3.root.json", patch)),
                        role_file_error
                    );
                }

                TEST_CASE_FIXTURE(RootImplT_v1, "wrong_filename_spec_version")
                {
                    RootImpl root(root1_json);

                    // "2.sv2.root.json" is upgradable spec version (spec version N+1)
                    // but v2 is NOT implemented yet, so v1::RootImpl is not upgradable
                    CHECK_THROWS_AS(
                        root.update(create_root_update("2.sv2.root.json")),
                        spec_version_error
                    );
                    // "2.sv3.root.json" is NOT upgradable spec version (spec version N+1)
                    CHECK_THROWS_AS(root.update(create_root_update("2.sv3.root.json")), role_file_error);
                    CHECK_THROWS_AS(
                        root.update(create_root_update("2.sv0.6.root.json")),
                        role_file_error
                    );
                }

                TEST_CASE_FIXTURE(RootImplT_v1, "illformed_filename_version")
                {
                    RootImpl root(root1_json);

                    json patch = R"([])"_json;

                    CHECK_THROWS_AS(
                        root.update(create_root_update("wrong.root.json", patch)),
                        role_file_error
                    );
                }

                TEST_CASE_FIXTURE(RootImplT_v1, "rollback_attack")
                {
                    RootImpl root(root1_json);

                    json patch = R"([
                        { "op": "replace", "path": "/signed/version", "value": 1 }
                        ])"_json;

                    CHECK_THROWS_AS(
                        root.update(create_root_update("2.root.json", patch)),
                        rollback_error
                    );
                }

                TEST_CASE_FIXTURE(RootImplT_v1, "wrong_type")
                {
                    RootImpl root(root1_json);

                    json patch = R"([
                        { "op": "replace", "path": "/signed/_type", "value": "timestamp" },
                        { "op": "replace", "path": "/signed/version", "value": 2 }
                        ])"_json;

                    CHECK_THROWS_AS(
                        root.update(create_root_update("2.root.json", patch)),
                        role_metadata_error
                    );
                }

                TEST_CASE_FIXTURE(RootImplT_v1, "missing_type")
                {
                    RootImpl root(root1_json);

                    json patch = R"([
                        { "op": "remove", "path": "/signed/_type" },
                        { "op": "replace", "path": "/signed/version", "value": 2 }
                        ])"_json;

                    CHECK_THROWS_AS(
                        root.update(create_root_update("2.root.json", patch)),
                        role_metadata_error
                    );
                }

                TEST_CASE_FIXTURE(RootImplT_v1, "missing_keys")
                {
                    RootImpl root(root1_json);

                    json patch = R"([
                        { "op": "remove", "path": "/signed/keys" },
                        { "op": "replace", "path": "/signed/version", "value": 2 }
                        ])"_json;

                    CHECK_THROWS_AS(
                        root.update(create_root_update("2.root.json", patch)),
                        role_metadata_error
                    );
                }

                TEST_CASE_FIXTURE(RootImplT_v1, "missing_roles")
                {
                    RootImpl root(root1_json);

                    json patch = R"([
                        { "op": "remove", "path": "/signed/roles" },
                        { "op": "replace", "path": "/signed/version", "value": 2 }
                        ])"_json;

                    CHECK_THROWS_AS(
                        root.update(create_root_update("2.root.json", patch)),
                        role_metadata_error
                    );
                }

                TEST_CASE_FIXTURE(RootImplT_v1, "missing_role")
                {
                    RootImpl root(root1_json);

                    json patch = R"([
                        { "op": "remove", "path": "/signed/roles/timestamp" },
                        { "op": "replace", "path": "/signed/version", "value": 2 }
                        ])"_json;

                    CHECK_THROWS_AS(
                        root.update(create_root_update("2.root.json", patch)),
                        role_metadata_error
                    );
                }

                TEST_CASE_FIXTURE(RootImplT_v1, "empty_role_keyids")
                {
                    RootImpl root(root1_json);

                    json patch = R"([
                                    { "op": "replace", "path": "/signed/roles/snapshot/keyids", "value": [] },
                                    { "op": "replace", "path": "/signed/version", "value": 2 }
                                    ])"_json;

                    CHECK_THROWS_AS(
                        root.update(create_root_update("2.root.json", patch)),
                        role_metadata_error
                    );
                }

                TEST_CASE_FIXTURE(RootImplT_v1, "null_role_threshold")
                {
                    RootImpl root(root1_json);

                    json patch = R"([
                                    { "op": "replace", "path": "/signed/roles/snapshot/threshold", "value": 0 },
                                    { "op": "replace", "path": "/signed/version", "value": 2 }
                                    ])"_json;

                    CHECK_THROWS_AS(
                        root.update(create_root_update("2.root.json", patch)),
                        role_metadata_error
                    );
                }

                TEST_CASE_FIXTURE(RootImplT_v1, "extra_roles")
                {
                    RootImpl root(root1_json);

                    json patch = R"([
                        { "op": "add", "path": "/signed/roles/some_wrong_role", "value": { "keyids": ["c"], "threshold": 1 } },
                        { "op": "replace", "path": "/signed/version", "value": 2 }
                        ])"_json;

                    CHECK_THROWS_AS(
                        root.update(create_root_update("2.root.json", patch)),
                        role_metadata_error
                    );
                }

                TEST_CASE_FIXTURE(RootImplT_v1, "key_not_found")
                {
                    RootImpl root(root1_json);

                    json patch = R"([
                        { "op": "add", "path": "/signed/roles/snapshot/keyids/-", "value": "c" },
                        { "op": "replace", "path": "/signed/version", "value": 2 }
                        ])"_json;

                    CHECK_THROWS_AS(
                        root.update(create_root_update("2.root.json", patch)),
                        role_metadata_error
                    );
                }

                TEST_CASE_FIXTURE(RootImplT_v1, "mirrors_role")
                {
                    json patch = R"([
                        { "op": "add", "path": "/signed/roles/mirrors", "value": { "keyids": ["c"], "threshold": 1 } },
                        { "op": "add", "path": "/signed/keys/c", "value": { "scheme": "ed25519", "keytype": "ed25519", "keyval": "c"} },
                        { "op": "replace", "path": "/signed/version", "value": 2 }
                        ])"_json;

                    const RootImpl root(create_root_update("2.root.json", patch));
                    const bool mirrors_role_found = root.roles().find("mirrors")
                                                    != root.roles().cend();
                    CHECK(mirrors_role_found);
                }

                TEST_CASE_FIXTURE(RootImplT_v1, "threshold_not_met")
                {
                    RootImpl root(root1_json);

                    json patch = R"([
                        { "op": "replace", "path": "/signed/version", "value": 2 },
                        { "op": "replace", "path": "/signed/roles/root/threshold", "value": 2 }
                        ])"_json;

                    CHECK_THROWS_AS(root.update(create_root_update("2.root.json", patch)), role_error);
                }

                TEST_CASE_FIXTURE(RootImplT_v1, "expires")
                {
                    RootImpl root(root1_json);

                    // expiration is set to now+3600s in 'sign_root'
                    TimeRef time_ref;
                    CHECK_FALSE(root.expired(time_ref));

                    time_ref.set(utc_time_now() + 7200);
                    CHECK(root.expired(time_ref));

                    json patch = json::parse(
                        R"([
                        { "op": "replace", "path": "/signed/expires", "value": ")"
                        + timestamp(utc_time_now() + 10800) + R"(" },
                        { "op": "replace", "path": "/signed/version", "value": 2 }
                        ])"
                    );
                    auto updated_root = root.update(create_root_update("2.root.json", patch));

                    auto testing_root = static_cast<RootImpl*>(updated_root.get());
                    CHECK_FALSE(testing_root->expired(time_ref));

                    patch = json::parse(R"([
                        { "op": "replace", "path": "/signed/expires", "value": "2051-10-08T07:07:09+0030" },
                        { "op": "replace", "path": "/signed/version", "value": 2 }
                        ])");
                    CHECK_THROWS_AS(
                        root.update(create_root_update("2.root.json", patch)),
                        role_metadata_error
                    );

                    patch = json::parse(R"([
                        { "op": "replace", "path": "/signed/expires", "value": "2051-10-08T07:07:09D" },
                        { "op": "replace", "path": "/signed/version", "value": 2 }
                        ])");
                    CHECK_THROWS_AS(
                        root.update(create_root_update("2.root.json", patch)),
                        role_metadata_error
                    );

                    patch = json::parse(R"([
                        { "op": "replace", "path": "/signed/expires", "value": "2051-10-08T07:07:09.000" },
                        { "op": "replace", "path": "/signed/version", "value": 2 }
                        ])");
                    CHECK_THROWS_AS(
                        root.update(create_root_update("2.root.json", patch)),
                        role_metadata_error
                    );
                }

                TEST_CASE_FIXTURE(RootImplT_v1, "possible_update_files")
                {
                    RootImpl root(root1_json);

                    auto update_f = root.possible_update_files();
                    CHECK(update_f[0].string().c_str() == doctest::Contains("2.sv2.root.json"));
                    CHECK(update_f[1].string().c_str() == doctest::Contains("2.sv1.root.json"));
                    CHECK(update_f[2].string().c_str() == doctest::Contains("2.root.json"));

                    json patch = json::parse(R"([
                        { "op": "replace", "path": "/signed/version", "value": 2 }
                        ])");
                    auto updated_root = root.update(create_root_update("2.root.json", patch));
                    update_f = updated_root->possible_update_files();
                    CHECK(update_f[0].string().c_str() == doctest::Contains("3.sv2.root.json"));
                    CHECK(update_f[1].string().c_str() == doctest::Contains("3.sv1.root.json"));
                    CHECK(update_f[2].string().c_str() == doctest::Contains("3.root.json"));
                }
            }


            class SpecImplT_v1
            {
            public:

                SpecImplT_v1() = default;

            protected:

                SpecImpl spec;
            };

            TEST_SUITE("SpecImplT_v1")
            {
                TEST_CASE_FIXTURE(SpecImplT_v1, "ctore")
                {
                    SpecImpl new_spec("1.0.0");
                    CHECK_EQ(new_spec.version_str(), "1.0.0");
                }

                TEST_CASE_FIXTURE(SpecImplT_v1, "version_str")
                {
                    CHECK_EQ(spec.version_str(), "1.0.17");
                }

                TEST_CASE_FIXTURE(SpecImplT_v1, "is_compatible")
                {
                    CHECK(spec.is_compatible(std::string("1.0.0")));
                    CHECK(spec.is_compatible(std::string("1.0.17")));
                    CHECK(spec.is_compatible(std::string("1.25.10")));

                    CHECK_FALSE(spec.is_compatible(std::string("2.0.0")));
                    CHECK_FALSE(spec.is_compatible(std::string("2.0.17")));
                    CHECK_FALSE(spec.is_compatible(std::string("0.6.0")));
                }

                TEST_CASE_FIXTURE(SpecImplT_v1, "is_upgrade")
                {
                    CHECK(spec.is_upgrade(std::string("2.0.0")));
                    CHECK(spec.is_upgrade(std::string("2.1.10")));

                    CHECK_FALSE(spec.is_upgrade(std::string("0.6.0")));
                    CHECK_FALSE(spec.is_upgrade(std::string("3.0.0")));
                    // not an upgrade, compatible version
                    CHECK_FALSE(spec.is_upgrade(std::string("1.0.17")));
                    CHECK_FALSE(spec.is_upgrade(std::string("1.0.0")));
                }

                TEST_CASE_FIXTURE(SpecImplT_v1, "upgradable")
                {
                    CHECK_FALSE(spec.upgradable());
                }

                TEST_CASE_FIXTURE(SpecImplT_v1, "compatible_prefix")
                {
                    CHECK_EQ(spec.compatible_prefix(), "1");
                }

                TEST_CASE_FIXTURE(SpecImplT_v1, "upgrade_prefix")
                {
                    CHECK(spec.upgrade_prefix()[0].c_str() == doctest::Contains("2"));
                }

                TEST_CASE_FIXTURE(SpecImplT_v1, "json_key")
                {
                    CHECK_EQ(spec.json_key(), "spec_version");
                }

                TEST_CASE_FIXTURE(SpecImplT_v1, "expiration_json_key")
                {
                    CHECK_EQ(spec.expiration_json_key(), "expires");
                }

                TEST_CASE_FIXTURE(SpecImplT_v1, "canonicalize")
                {
                    CHECK_EQ(spec.canonicalize(R"({"foo":"bar"})"_json), "{\"foo\":\"bar\"}");
                }

                TEST_CASE_FIXTURE(SpecImplT_v1, "signatures")
                {
                    json j = R"({
                                    "signatures":
                                    [
                                        {
                                            "keyid": "foo",
                                            "sig": "baz",
                                            "other_headers": "bar"
                                        }
                                    ]
                                })"_json;
                    auto sigs = spec.signatures(j);
                    CHECK_EQ(sigs.size(), 1);
                    CHECK_EQ(sigs.begin()->keyid, "foo");
                    CHECK_EQ(sigs.begin()->sig, "baz");
                    CHECK_EQ(sigs.begin()->pgp_trailer, "bar");
                }
            }

            TEST_SUITE("RoleSignature")
            {
                // Test serialization/deserialization
                TEST_CASE("to_json")
                {
                    RoleSignature s{ "some_key_id", "some_signature", "" };
                    json j = R"({"keyid": "some_key_id", "sig": "some_signature"})"_json;
                    CHECK_EQ(j, json(s));

                    s = { "some_key_id", "some_signature", "some_pgp_trailer" };
                    j = R"({"keyid": "some_key_id", "other_headers": "some_pgp_trailer", "sig": "some_signature"})"_json;
                    CHECK_EQ(j, json(s));
                }
            }
        }  // namespace testing
    }      // namespace v1
}  // namespace validate
