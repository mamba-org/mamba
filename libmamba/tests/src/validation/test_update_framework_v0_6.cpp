// Copyright (c) 2022, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

// There are several `CHECK_THROWS_AS` expressions in this file.
// Sometimes they call a method marked as `[[nodiscard]]`.
// This causes compiler warnnings.
// This doctest flag is designed specifically to prevent this warning from happening.
// https://github.com/doctest/doctest/blob/master/doc/markdown/configuration.md#doctest_config_void_cast_expressions
#define DOCTEST_CONFIG_VOID_CAST_EXPRESSIONS

#include <map>

#include <catch2/catch_all.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "mamba/core/fsutil.hpp"
#include "mamba/core/util.hpp"
#include "mamba/util/encoding.hpp"
#include "mamba/util/path_manip.hpp"
#include "mamba/validation/errors.hpp"
#include "mamba/validation/repo_checker.hpp"
#include "mamba/validation/tools.hpp"
#include "mamba/validation/update_framework_v0_6.hpp"
#include "mamba/validation/update_framework_v1.hpp"

#include "mambatests.hpp"

using namespace mamba;
using namespace mamba::validation;
namespace nl = nlohmann;

class RootImplT_v0_6
{
public:

    using role_secrets_type = std::map<std::string, std::array<std::byte, MAMBA_ED25519_KEYSIZE_BYTES>>;
    using secrets_type = std::map<std::string, role_secrets_type>;

    RootImplT_v0_6()
    {
        channel_dir = std::make_unique<mamba::TemporaryDirectory>();

        generate_secrets();
        sign_root();
    }

    auto trusted_root_file(const nl::json& j) -> fs::u8path
    {
        fs::u8path p = channel_dir->path() / "root.json";

        std::ofstream out_file(p.std_path(), std::ofstream::out | std::ofstream::trunc);
        out_file << j;
        out_file.close();

        return p;
    }

    auto trusted_root_file_raw_key() -> fs::u8path
    {
        return trusted_root_file(root1_json);
    }

    auto trusted_root_file_pgp() -> fs::u8path
    {
        return trusted_root_file(root1_pgp_json);
    }

    auto create_root_update_json(const nl::json& patch) -> nl::json
    {
        nl::json new_root = root1_json;

        if (!patch.empty())
        {
            new_root = new_root.patch(patch);
        }

        nl::json sig_patch = nl::json::parse(
            R"([
                                        { "op": "replace", "path": "/signatures", "value":)"
            + sign_root_meta(new_root.at("signed")).dump() + R"( }
                                        ])"
        );
        return new_root.patch(sig_patch);
    }

    auto create_root_update(const fs::u8path& name, const nl::json& patch = nl::json()) -> fs::u8path
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

    auto sign_root_meta(const nl::json& root_meta) -> nl::json
    {
        std::map<std::string, std::map<std::string, std::string>> signatures;

        auto sig_bin = std::array<std::byte, MAMBA_ED25519_SIGSIZE_BYTES>{};

        for (auto& secret : secrets.at("root"))
        {
            sign(root_meta.dump(2), secret.second.data(), sig_bin.data());

            auto sig_hex = util::bytes_to_hex_str(sig_bin.data(), sig_bin.data() + sig_bin.size());
            signatures[secret.first].insert({ "signature", sig_hex });
        }

        return signatures;
    }

    auto upgrade_to_v1(const v0_6::RootImpl& root, const nl::json& patch = nl::json()) -> nl::json
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

        nl::json upgraded_root;
        upgraded_root["signed"] = root_meta;
        upgraded_root["signatures"] = signatures;

        return upgraded_root;
    }

protected:

    fs::u8path root1_pgp = mambatests::test_data_dir / "validation/1.sv0.6.root.json";
    nl::json root1_json, root1_pgp_json;

    secrets_type secrets;

    std::unique_ptr<TemporaryDirectory> channel_dir;

    auto generate_role_secrets(int count)
        -> std::map<std::string, std::array<std::byte, MAMBA_ED25519_KEYSIZE_BYTES>>
    {
        std::map<std::string, std::array<std::byte, MAMBA_ED25519_KEYSIZE_BYTES>> role_secrets;

        auto pk = std::array<std::byte, MAMBA_ED25519_KEYSIZE_BYTES>{};
        std::array<std::byte, MAMBA_ED25519_KEYSIZE_BYTES> sk;

        for (int i = 0; i < count; ++i)
        {
            generate_ed25519_keypair(pk.data(), sk.data());
            auto pk_hex = util::bytes_to_hex_str(pk.data(), pk.data() + pk.size());

            role_secrets.insert({ pk_hex, sk });
        }
        return role_secrets;
    }
};

namespace
{
    TEST_CASE_FIXTURE(RootImplT_v0_6, "ctor_from_path")
    {
        v0_6::RootImpl root(trusted_root_file_raw_key());

        CHECK_EQ(root.type(), "root");
        CHECK_EQ(root.file_ext(), "json");
        REQUIRE(root.spec_version() == v0_6::SpecImpl("0.6.0");
        CHECK_EQ(root.version(), 1);
    }

    TEST_CASE_FIXTURE(RootImplT_v0_6, "ctor_from_path_pgp_signed")
    {
        v0_6::RootImpl root(trusted_root_file_pgp());

        CHECK_EQ(root.type(), "root");
        CHECK_EQ(root.file_ext(), "json");
        REQUIRE(root.spec_version() == v0_6::SpecImpl("0.6.0");
        CHECK_EQ(root.version(), 1);
    }

    TEST_CASE_FIXTURE(RootImplT_v0_6, "ctor_from_json")
    {
        v0_6::RootImpl root(root1_json);

        CHECK_EQ(root.type(), "root");
        CHECK_EQ(root.file_ext(), "json");
        REQUIRE(root.spec_version() == v0_6::SpecImpl("0.6.0");
        CHECK_EQ(root.version(), 1);
    }

    TEST_CASE_FIXTURE(RootImplT_v0_6, "ctor_from_json_str")
    {
        v0_6::RootImpl root(root1_json.dump());

        CHECK_EQ(root.type(), "root");
        CHECK_EQ(root.file_ext(), "json");
        REQUIRE(root.spec_version() == v0_6::SpecImpl("0.6.0");
        CHECK_EQ(root.version(), 1);
    }

    TEST_CASE_FIXTURE(RootImplT_v0_6, "ctor_from_json_pgp_signed")
    {
        v0_6::RootImpl root(root1_pgp_json);

        CHECK_EQ(root.type(), "root");
        CHECK_EQ(root.file_ext(), "json");
        REQUIRE(root.spec_version() == v0_6::SpecImpl("0.6.0");
        CHECK_EQ(root.version(), 1);
    }

    TEST_CASE_FIXTURE(RootImplT_v0_6, "ctor_wrong_filename_spec_version")
    {
        fs::u8path p = channel_dir->path() / "2.sv1.root.json";

        std::ofstream out_file(p.std_path(), std::ofstream::out | std::ofstream::trunc);
        out_file << root1_json;
        out_file.close();

        // "2.sv1.root.json" is not compatible spec version (spec version N)
        CHECK_THROWS_AS(v0_6::RootImpl{ p }, role_file_error);
    }

    TEST_CASE_FIXTURE(RootImplT_v0_6, "update_from_path")
    {
        using namespace mamba;

        auto f = trusted_root_file_raw_key();
        v0_6::RootImpl root(f);

        nl::json patch = R"([
                        { "op": "replace", "path": "/signed/version", "value": 2 }
                        ])"_json;
        auto updated_root = root.update(create_root_update("2.root.json", patch));

        auto testing_root = static_cast<v0_6::RootImpl*>(updated_root.get());
        CHECK_EQ(testing_root->type(), "root");
        CHECK_EQ(testing_root->file_ext(), "json");
        REQUIRE(testing_root->spec_version() == v0_6::SpecImpl("0.6.0");
        CHECK_EQ(testing_root->version(), 2);
    }

    TEST_CASE_FIXTURE(RootImplT_v0_6, "wrong_version")
    {
        v0_6::RootImpl root(root1_json);

        nl::json patch = R"([
                        { "op": "replace", "path": "/signed/version", "value": 3 }
                        ])"_json;

        CHECK_THROWS_AS(root.update(create_root_update("2.root.json", patch)), role_metadata_error);
    }

    TEST_CASE_FIXTURE(RootImplT_v0_6, "spec_version")
    {
        v0_6::RootImpl root(root1_json);

        nl::json patch = R"([
                        { "op": "replace", "path": "/signed/version", "value": 2 },
                        { "op": "replace", "path": "/signed/metadata_spec_version", "value": "0.6.1" }
                        ])"_json;
        auto updated_root = root.update(create_root_update("2.root.json", patch));

        auto testing_root = static_cast<v0_6::RootImpl*>(updated_root.get());
        REQUIRE(testing_root->spec_version() == v0_6::SpecImpl("0.6.1");
        CHECK_EQ(testing_root->version(), 2);
        REQUIRE(testing_root->expires() == root.expires();
    }

    TEST_CASE_FIXTURE(RootImplT_v0_6, "upgraded_spec_version")
    {
        v0_6::RootImpl root(root1_json);

        nl::json patch = R"([
                        { "op": "replace", "path": "/signed/version", "value": 2 },
                        { "op": "replace", "path": "/signed/metadata_spec_version", "value": "1.0.0" }
                        ])"_json;

        CHECK_THROWS_AS(root.update(create_root_update("2.root.json", patch)), spec_version_error);

        nl::json signable_patch = nl::json::parse(
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
        REQUIRE(testing_root->spec_version() == v0_6::SpecImpl("1.0.17");
        CHECK_EQ(testing_root->version(), 2);
        REQUIRE(testing_root->expires() < root.expires();
    }

    TEST_CASE_FIXTURE(RootImplT_v0_6, "equivalent_upgraded_spec_version")
    {
        v0_6::RootImpl root(root1_json);

        nl::json signable_patch = R"([
                        { "op": "add", "path": "/keys/dummy_value", "value": { "keytype": "ed25519", "scheme": "ed25519", "keyval": "dummy_value" } },
                        { "op": "add", "path": "/roles/snapshot/keyids", "value": ["dummy_value"] },
                        { "op": "add", "path": "/roles/timestamp/keyids", "value": ["dummy_value"] }
                        ])"_json;
        v1::RootImpl updated_root(upgrade_to_v1(root, signable_patch));

        REQUIRE(updated_root.spec_version() == v1::SpecImpl("1.0.17");
        CHECK_EQ(updated_root.version(), 1);
    }

    TEST_CASE_FIXTURE(RootImplT_v0_6, "wrong_spec_version")
    {
        v0_6::RootImpl root(root1_json);

        nl::json patch = R"([
                        { "op": "replace", "path": "/signed/version", "value": 2 },
                        { "op": "replace", "path": "/signed/metadata_spec_version", "value": "1.0.0" }
                        ])"_json;

        CHECK_THROWS_AS(root.update(create_root_update("2.root.json", patch)), spec_version_error);

        patch = R"([
                        { "op": "replace", "path": "/signed/version", "value": 2 },
                        { "op": "replace", "path": "/signed/metadata_spec_version", "value": "wrong" }
                        ])"_json;

        CHECK_THROWS_AS(root.update(create_root_update("2.root.json", patch)), spec_version_error);
    }

    TEST_CASE_FIXTURE(RootImplT_v0_6, "wrong_filename_role")
    {
        v0_6::RootImpl root(root1_json);

        CHECK_THROWS_AS(root.update(create_root_update("2.rooot.json")), role_file_error);
    }

    TEST_CASE_FIXTURE(RootImplT_v0_6, "wrong_filename_version")
    {
        v0_6::RootImpl root(root1_json);

        CHECK_THROWS_AS(root.update(create_root_update("3.root.json")), role_file_error);
    }

    TEST_CASE_FIXTURE(RootImplT_v0_6, "wrong_filename_spec_version")
    {
        v0_6::RootImpl root(root1_json);

        // "2.sv1.root.json" is upgradable spec version (spec version N+1)
        nl::json signable_patch = R"([
                        { "op": "replace", "path": "/version", "value": 2 },
                        { "op": "replace", "path": "/spec_version", "value": "1.0.0" },
                        { "op": "add", "path": "/keys/dummy_value", "value": { "keytype": "ed25519", "scheme": "ed25519", "keyval": "dummy_value" } },
                        { "op": "add", "path": "/roles/snapshot/keyids", "value": ["dummy_value"] },
                        { "op": "add", "path": "/roles/timestamp/keyids", "value": ["dummy_value"] }
                        ])"_json;
        auto updated_root = root.update(upgrade_to_v1(root, signable_patch));
        auto testing_root = dynamic_cast<v1::RootImpl*>(updated_root.get());
        REQUIRE_NE(testing_root, nullptr);
        REQUIRE(testing_root->spec_version() == v0_6::SpecImpl("1.0.0");

        // "2.sv2.root.json" is not upgradable spec version (spec version N+1)
        nl::json patch = R"([
                        { "op": "replace", "path": "/signed/version", "value": 2 }
                        ])"_json;
        CHECK_THROWS_AS(root.update(create_root_update("2.sv2.root.json", patch)), role_file_error);
    }

    TEST_CASE_FIXTURE(RootImplT_v0_6, "illformed_filename_version")
    {
        v0_6::RootImpl root(root1_json);

        CHECK_THROWS_AS(root.update(create_root_update("wrong.root.json")), role_file_error);
        CHECK_THROWS_AS(root.update(create_root_update("2..root.json")), role_file_error);
        CHECK_THROWS_AS(root.update(create_root_update("2.sv04.root.json")), role_file_error);
    }

    TEST_CASE_FIXTURE(RootImplT_v0_6, "rollback_attack")
    {
        v0_6::RootImpl root(root1_json);

        nl::json patch = R"([
                        { "op": "replace", "path": "/signed/version", "value": 1 }
                        ])"_json;

        CHECK_THROWS_AS(root.update(create_root_update("2.root.json", patch)), rollback_error);
    }

    TEST_CASE_FIXTURE(RootImplT_v0_6, "wrong_type")
    {
        v0_6::RootImpl root(root1_json);

        nl::json patch = R"([
                        { "op": "replace", "path": "/signed/type", "value": "timestamp" },
                        { "op": "replace", "path": "/signed/version", "value": 2 }
                        ])"_json;

        CHECK_THROWS_AS(root.update(create_root_update("2.root.json", patch)), role_metadata_error);
    }

    TEST_CASE_FIXTURE(RootImplT_v0_6, "missing_type")
    {
        v0_6::RootImpl root(root1_json);

        nl::json patch = R"([
                        { "op": "remove", "path": "/signed/type" },
                        { "op": "replace", "path": "/signed/version", "value": 2 }
                        ])"_json;

        CHECK_THROWS_AS(root.update(create_root_update("2.root.json", patch)), role_metadata_error);
    }

    TEST_CASE_FIXTURE(RootImplT_v0_6, "missing_delegations")
    {
        v0_6::RootImpl root(root1_json);

        nl::json patch = R"([
                        { "op": "remove", "path": "/signed/delegations" },
                        { "op": "replace", "path": "/signed/version", "value": 2 }
                        ])"_json;

        CHECK_THROWS_AS(root.update(create_root_update("2.root.json", patch)), role_metadata_error);
    }

    TEST_CASE_FIXTURE(RootImplT_v0_6, "missing_delegation")
    {
        v0_6::RootImpl root(root1_json);

        nl::json patch = R"([
                                    { "op": "remove", "path": "/signed/delegations/root" },
                                    { "op": "replace", "path": "/signed/version", "value": 2 }
                                    ])"_json;

        CHECK_THROWS_AS(root.update(create_root_update("2.root.json", patch)), role_metadata_error);
    }

    TEST_CASE_FIXTURE(RootImplT_v0_6, "empty_delegation_pubkeys")
    {
        v0_6::RootImpl root(root1_json);

        nl::json patch = R"([
                                    { "op": "replace", "path": "/signed/delegations/root/pubkeys", "value": [] },
                                    { "op": "replace", "path": "/signed/version", "value": 2 }
                                    ])"_json;

        CHECK_THROWS_AS(root.update(create_root_update("2.root.json", patch)), role_metadata_error);
    }

    TEST_CASE_FIXTURE(RootImplT_v0_6, "null_role_threshold")
    {
        v0_6::RootImpl root(root1_json);

        nl::json patch = R"([
                                    { "op": "replace", "path": "/signed/delegations/root/threshold", "value": 0 },
                                    { "op": "replace", "path": "/signed/version", "value": 2 }
                                    ])"_json;

        CHECK_THROWS_AS(root.update(create_root_update("2.root.json", patch)), role_metadata_error);
    }

    TEST_CASE_FIXTURE(RootImplT_v0_6, "extra_roles")
    {
        v0_6::RootImpl root(root1_json);

        nl::json patch = R"([
                                    { "op": "add", "path": "/signed/delegations/some_wrong_role",
                                        "value": { "pubkeys": ["c"], "threshold": 1 } },
                                    { "op": "replace", "path": "/signed/version", "value": 2 }
                                    ])"_json;

        CHECK_THROWS_AS(root.update(create_root_update("2.root.json", patch)), role_metadata_error);
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
        REQUIRE(mirrors_role_found);
    }
    */
    TEST_CASE_FIXTURE(RootImplT_v0_6, "threshold_not_met")
    {
        v0_6::RootImpl root(root1_json);

        nl::json patch = R"([
                        { "op": "replace", "path": "/signed/version", "value": 2 },
                        { "op": "replace", "path": "/signed/delegations/root/threshold", "value": 2 }
                        ])"_json;

        CHECK_THROWS_AS(root.update(create_root_update("2.root.json", patch)), role_error);
    }

    TEST_CASE_FIXTURE(RootImplT_v0_6, "expires")
    {
        v0_6::RootImpl root(root1_json);

        // expiration is set to now+3600s in 'sign_root'
        TimeRef time_ref;
        REQUIRE_FALSE(root.expired(time_ref));

        time_ref.set(utc_time_now() + 7200);
        REQUIRE(root.expired(time_ref));

        nl::json patch = nl::json::parse(
            R"([
                        { "op": "replace", "path": "/signed/expiration", "value": ")"
            + timestamp(utc_time_now() + 10800) + R"(" },
                        { "op": "replace", "path": "/signed/version", "value": 2 }
                        ])"
        );
        auto updated_root = root.update(create_root_update("2.root.json", patch));

        auto testing_root = static_cast<v0_6::RootImpl*>(updated_root.get());
        REQUIRE_FALSE(testing_root->expired(time_ref));
    }

    TEST_CASE_FIXTURE(RootImplT_v0_6, "timestamp")
    {
        v0_6::RootImpl root(root1_json);

        nl::json patch;

        patch = nl::json::parse(R"([
                        { "op": "replace", "path": "/signed/timestamp", "value": "2021-09-20T07:07:09+0030" },
                        { "op": "replace", "path": "/signed/version", "value": 2 }
                        ])");
        CHECK_THROWS_AS(root.update(create_root_update("2.root.json", patch)), role_metadata_error);

        patch = nl::json::parse(R"([
                        { "op": "replace", "path": "/signed/timestamp", "value": "2021-09-20T07:07:09D" },
                        { "op": "replace", "path": "/signed/version", "value": 2 }
                        ])");
        CHECK_THROWS_AS(root.update(create_root_update("2.root.json", patch)), role_metadata_error);

        patch = nl::json::parse(R"([
                        { "op": "replace", "path": "/signed/timestamp", "value": "2021-09-20T07:07:09.000" },
                        { "op": "replace", "path": "/signed/version", "value": 2 }
                        ])");
        CHECK_THROWS_AS(root.update(create_root_update("2.root.json", patch)), role_metadata_error);
    }

    TEST_CASE_FIXTURE(RootImplT_v0_6, "possible_update_files")
    {
        v0_6::RootImpl root(root1_json);

        auto update_f = root.possible_update_files();
        REQUIRE(update_f[0].string().c_str() == doctest::Contains("2.sv1.root.json"));
        REQUIRE(update_f[1].string().c_str() == doctest::Contains("2.sv0.7.root.json"));
        REQUIRE(update_f[2].string().c_str() == doctest::Contains("2.sv0.6.root.json"));
        REQUIRE(update_f[3].string().c_str() == doctest::Contains("2.root.json"));

        nl::json patch = nl::json::parse(R"([
                        { "op": "replace", "path": "/signed/version", "value": 2 }
                        ])");
        auto updated_root = root.update(create_root_update("2.root.json", patch));
        update_f = updated_root->possible_update_files();
        REQUIRE(update_f[0].string().c_str() == doctest::Contains("3.sv1.root.json"));
        REQUIRE(update_f[1].string().c_str() == doctest::Contains("3.sv0.7.root.json"));
        REQUIRE(update_f[2].string().c_str() == doctest::Contains("3.sv0.6.root.json"));
        REQUIRE(update_f[3].string().c_str() == doctest::Contains("3.root.json"));
    }
}

class SpecImplT_v06
{
public:

    SpecImplT_v06() = default;

protected:

    v0_6::SpecImpl spec;
};

namespace
{
    TEST_CASE_FIXTURE(SpecImplT_v06, "ctor")
    {
        v0_6::SpecImpl new_spec("0.6.1");
        CHECK_EQ(new_spec.version_str(), "0.6.1");
    }

    TEST_CASE_FIXTURE(SpecImplT_v06, "version_str")
    {
        CHECK_EQ(spec.version_str(), "0.6.0");
    }

    TEST_CASE_FIXTURE(SpecImplT_v06, "is_compatible")
    {
        REQUIRE(spec.is_compatible(std::string("0.6.0")));
        REQUIRE(spec.is_compatible(std::string("0.6.1")));
        REQUIRE(spec.is_compatible(std::string("0.6.10")));

        // minor version change with major version '0' may be
        // backward incompatible
        REQUIRE_FALSE(spec.is_compatible(std::string("0.7.0")));
        REQUIRE_FALSE(spec.is_compatible(std::string("1.0.0")));
        REQUIRE_FALSE(spec.is_compatible(std::string("2.0.0")));
    }

    TEST_CASE_FIXTURE(SpecImplT_v06, "is_upgrade")
    {
        REQUIRE(spec.is_upgrade(std::string("0.7.0")));
        REQUIRE(spec.is_upgrade(std::string("1.0.0")));
        REQUIRE(spec.is_upgrade(std::string("1.1.0")));
        REQUIRE(spec.is_upgrade(std::string("1.0.17")));

        // 2 possible backward incompatible updates
        REQUIRE_FALSE(spec.is_upgrade(std::string("0.8.0")));
        REQUIRE_FALSE(spec.is_upgrade(std::string("2.0.0")));
        // not an upgrade, compatible version
        REQUIRE_FALSE(spec.is_upgrade(std::string("0.6.1")));
    }

    TEST_CASE_FIXTURE(SpecImplT_v06, "upgradable")
    {
        REQUIRE(spec.upgradable());
    }

    TEST_CASE_FIXTURE(SpecImplT_v06, "compatible_prefix")
    {
        CHECK_EQ(spec.compatible_prefix(), "0.6");
    }

    TEST_CASE_FIXTURE(SpecImplT_v06, "upgrade_prefix")
    {
        REQUIRE(spec.upgrade_prefix()[0].c_str() == doctest::Contains("1"));
        REQUIRE(spec.upgrade_prefix()[1].c_str() == doctest::Contains("0.7"));
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
        nl::json j = R"({
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

class KeyMgrT_v06 : public RootImplT_v0_6
{
public:

    KeyMgrT_v06()
        : RootImplT_v0_6()
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

    auto patched_key_mgr_json(const nl::json& patch = nl::json()) -> nl::json
    {
        nl::json update_key_mgr = key_mgr_json;

        if (!patch.empty())
        {
            update_key_mgr = update_key_mgr.patch(patch);
        }

        nl::json sig_patch = nl::json::parse(
            R"([
                            { "op": "replace", "path": "/signatures", "value": )"
            + sign_key_mgr_meta(update_key_mgr.at("signed")).dump() + R"( }
                            ])"
        );
        return update_key_mgr.patch(sig_patch);
    }

    auto write_key_mgr_file(const nl::json& j, const std::string& filename = "key_mgr.json")
        -> fs::u8path
    {
        fs::u8path p = channel_dir->path() / filename;

        std::ofstream out_file(p.std_path(), std::ofstream::out | std::ofstream::trunc);
        out_file << j;
        out_file.close();

        return p;
    }

protected:

    nl::json key_mgr_json;

    auto sign_key_mgr_meta(const nl::json& meta) -> nl::json
    {
        std::map<std::string, std::map<std::string, std::string>> signatures;

        auto sig_bin = std::array<std::byte, MAMBA_ED25519_SIGSIZE_BYTES>{};

        for (auto& secret : secrets.at("key_mgr"))
        {
            sign(meta.dump(2), secret.second.data(), sig_bin.data());

            auto sig_hex = util::bytes_to_hex_str(sig_bin.data(), sig_bin.data() + sig_bin.size());
            signatures[secret.first].insert({ "signature", sig_hex });
        }

        return signatures;
    }
};

namespace
{
    TEST_CASE_FIXTURE(KeyMgrT_v06, "ctor_from_json")
    {
        v0_6::RootImpl root(root1_json);
        auto key_mgr = root.create_key_mgr(key_mgr_json);

        REQUIRE(key_mgr.spec_version() == v0_6::SpecImpl("0.6.0");
        CHECK_EQ(key_mgr.version(), 1);
    }

    TEST_CASE_FIXTURE(KeyMgrT_v06, "ctor_from_json_str")
    {
        v0_6::RootImpl root(root1_json);
        auto key_mgr = v0_6::KeyMgrRole(
            key_mgr_json.dump(),
            root.all_keys()["key_mgr"],
            std::make_shared<v0_6::SpecImpl>(v0_6::SpecImpl())
        );

        REQUIRE(key_mgr.spec_version() == v0_6::SpecImpl("0.6.0");
        CHECK_EQ(key_mgr.version(), 1);
    }

    TEST_CASE_FIXTURE(KeyMgrT_v06, "version")
    {
        v0_6::RootImpl root(root1_json);

        {
            nl::json key_mgr_patch = R"([
                        { "op": "replace", "path": "/signed/version", "value": 2 }
                        ])"_json;
            auto key_mgr = root.create_key_mgr(patched_key_mgr_json(key_mgr_patch));

            REQUIRE(key_mgr.spec_version() == v0_6::SpecImpl("0.6.0");
            CHECK_EQ(key_mgr.version(), 2);
        }

        {  // Any version is valid, without chaining required
            nl::json key_mgr_patch = R"([
                        { "op": "replace", "path": "/signed/version", "value": 20 }
                        ])"_json;
            auto key_mgr = root.create_key_mgr(patched_key_mgr_json(key_mgr_patch));

            REQUIRE(key_mgr.spec_version() == v0_6::SpecImpl("0.6.0");
            CHECK_EQ(key_mgr.version(), 20);
        }
    }

    TEST_CASE_FIXTURE(KeyMgrT_v06, "spec_version")
    {  // spec version as to match exactly 'root' spec version
        v0_6::RootImpl root(root1_json);

        {
            nl::json key_mgr_patch = R"([
                        { "op": "replace", "path": "/signed/metadata_spec_version", "value": "0.6.0" }
                        ])"_json;
            auto key_mgr = root.create_key_mgr(patched_key_mgr_json(key_mgr_patch));

            REQUIRE(key_mgr.spec_version() == v0_6::SpecImpl("0.6.0");
            CHECK_EQ(key_mgr.version(), 1);
        }

        {  // is compatible but not strictly the same as 'root' one
            nl::json key_mgr_patch = R"([
                        { "op": "replace", "path": "/signed/metadata_spec_version", "value": "0.6.1" }
                        ])"_json;

            CHECK_THROWS_AS(root.create_key_mgr(patched_key_mgr_json(key_mgr_patch)), spec_version_error);
        }

        {  // wrong type
            nl::json key_mgr_patch = R"([
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
        v0_6::RootImpl root(root1_json);

        auto key_mgr = root.create_key_mgr(write_key_mgr_file(key_mgr_json));
        REQUIRE(key_mgr.spec_version() == v0_6::SpecImpl("0.6.0");
        CHECK_EQ(key_mgr.version(), 1);

        // TODO: enforce consistency between spec version in filename and metadata
        key_mgr = root.create_key_mgr(write_key_mgr_file(key_mgr_json, "20.sv0.6.key_mgr.json"));
        REQUIRE(key_mgr.spec_version() == v0_6::SpecImpl("0.6.0");
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
            root.create_key_mgr(write_key_mgr_file(key_mgr_json, "wrong.sv0.6.key_mgr.json")),
            role_file_error
        );
    }

    TEST_CASE_FIXTURE(KeyMgrT_v06, "expires")
    {
        v0_6::RootImpl root(root1_json);
        auto key_mgr = root.create_key_mgr(key_mgr_json);

        // expiration is set to now+3600s in 'sign_key_mgr'
        TimeRef time_ref;
        REQUIRE_FALSE(key_mgr.expired(time_ref));
        REQUIRE_FALSE(root.expired(time_ref));

        time_ref.set(utc_time_now() + 7200);
        REQUIRE(key_mgr.expired(time_ref));
        REQUIRE(root.expired(time_ref));

        nl::json patch = nl::json::parse(
            R"([
                        { "op": "replace", "path": "/signed/expiration", "value": ")"
            + timestamp(utc_time_now() + 10800) + R"(" }
                        ])"
        );

        key_mgr = root.create_key_mgr(patched_key_mgr_json(patch));
        REQUIRE_FALSE(key_mgr.expired(time_ref));
        REQUIRE(root.expired(time_ref));
    }

    TEST_CASE_FIXTURE(KeyMgrT_v06, "timestamp")
    {
        v0_6::RootImpl root(root1_json);

        nl::json patch;

        patch = nl::json::parse(R"([
                        { "op": "replace", "path": "/signed/timestamp", "value": "2021-09-20T07:07:09+0030" },
                        { "op": "replace", "path": "/signed/version", "value": 1 }
                        ])");

        CHECK_THROWS_AS(root.create_key_mgr(patched_key_mgr_json(patch)), role_metadata_error);

        patch = nl::json::parse(R"([
                        { "op": "replace", "path": "/signed/timestamp", "value": "2021-09-20T07:07:09D" },
                        { "op": "replace", "path": "/signed/version", "value": 1 }
                        ])");
        CHECK_THROWS_AS(root.create_key_mgr(patched_key_mgr_json(patch)), role_metadata_error);

        patch = nl::json::parse(R"([
                        { "op": "replace", "path": "/signed/timestamp", "value": "2021-09-20T07:07:09.000" },
                        { "op": "replace", "path": "/signed/version", "value": 1 }
                        ])");
        CHECK_THROWS_AS(root.create_key_mgr(patched_key_mgr_json(patch)), role_metadata_error);
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
        root = std::make_unique<v0_6::RootImpl>(root1_json);
    };

    auto sign_repodata(const nl::json& patch = nl::json()) -> nl::json
    {
        nl::json updated_repodata = repodata_json;

        if (!patch.empty())
        {
            updated_repodata = updated_repodata.patch(patch);
        }

        for (auto& it : updated_repodata.at("packages").get<nl::json::object_t>())
        {
            nl::json sig_patch = nl::json::parse(
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
        pkg_mgr_json["signed"]["delegations"] = nl::json::object();

        pkg_mgr_json["signed"]["version"] = 1;
        pkg_mgr_json["signed"]["metadata_spec_version"] = "0.6.0";
        pkg_mgr_json["signed"]["type"] = "pkg_mgr";

        pkg_mgr_json["signed"]["timestamp"] = timestamp(utc_time_now());
        pkg_mgr_json["signed"]["expiration"] = timestamp(utc_time_now() + 3600);
        pkg_mgr_json["signatures"] = sign_pkg_mgr_meta(pkg_mgr_json["signed"]);
    }

    auto patched_pkg_mgr_json(const nl::json& patch = nl::json()) -> nl::json
    {
        nl::json update_pkg_mgr = pkg_mgr_json;

        if (!patch.empty())
        {
            update_pkg_mgr = update_pkg_mgr.patch(patch);
        }

        nl::json sig_patch = nl::json::parse(
            R"([
                            { "op": "replace", "path": "/signatures", "value": )"
            + sign_pkg_mgr_meta(update_pkg_mgr.at("signed")).dump() + R"( }
                            ])"
        );
        return update_pkg_mgr.patch(sig_patch);
    }

    auto write_pkg_mgr_file(const nl::json& j, const std::string& filename = "pkg_mgr.json")
        -> fs::u8path
    {
        fs::u8path p = channel_dir->path() / filename;

        std::ofstream out_file(p.std_path(), std::ofstream::out | std::ofstream::trunc);
        out_file << j;
        out_file.close();

        return p;
    }

protected:

    nl::json pkg_mgr_json, repodata_json, signed_repodata_json;

    std::unique_ptr<v0_6::RootImpl> root;

    auto sign_pkg_mgr_meta(const nl::json& meta) -> nl::json
    {
        std::map<std::string, std::map<std::string, std::string>> signatures;

        auto sig_bin = std::array<std::byte, MAMBA_ED25519_SIGSIZE_BYTES>{};

        for (auto& secret : secrets.at("pkg_mgr"))
        {
            sign(meta.dump(2), secret.second.data(), sig_bin.data());

            auto sig_hex = util::bytes_to_hex_str(sig_bin.data(), sig_bin.data() + sig_bin.size());
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

    auto sign_repodata_meta(const nl::json& meta) -> nl::json
    {
        std::map<std::string, std::map<std::string, std::string>> signatures;

        auto sig_bin = std::array<std::byte, MAMBA_ED25519_SIGSIZE_BYTES>{};

        for (auto& secret : secrets.at("pkg_mgr"))
        {
            sign(meta.dump(2), secret.second.data(), sig_bin.data());

            auto sig_hex = util::bytes_to_hex_str(sig_bin.data(), sig_bin.data() + sig_bin.size());
            signatures[secret.first].insert({ "signature", sig_hex });
        }

        return signatures;
    }
};

namespace
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

        nl::json wrong_pkg_patch = R"([
                                { "op": "replace", "path": "/packages/test-package1-0.1-0.tar.bz2/version", "value": "0.1.1" }
                                ])"_json;
        CHECK_THROWS_AS(pkg_mgr.verify_index(signed_repodata_json.patch(wrong_pkg_patch)), package_error);
    }

    TEST_CASE_FIXTURE(PkgMgrT_v06, "illformed_repodata")
    {
        auto key_mgr = root->create_key_mgr(key_mgr_json);
        auto pkg_mgr = key_mgr.create_pkg_mgr(pkg_mgr_json);

        nl::json illformed_pkg_patch = R"([
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

        nl::json patch = nl::json::parse(R"([
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

    void write_role(const nl::json& j, const fs::u8path& p)
    {
        fs::u8path expanded_p = util::expand_home(p.string());
        path::touch(expanded_p, true);
        std::ofstream out_file(expanded_p.std_path(), std::ofstream::out | std::ofstream::trunc);
        out_file << j.dump(2);
        out_file.close();
    }
};

namespace
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
        nl::json patch = nl::json::parse(
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
        nl::json patch = nl::json::parse(
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

        nl::json wrong_pkg_patch = R"([
                                { "op": "replace", "path": "/packages/test-package1-0.1-0.tar.bz2/version", "value": "0.1.1" }
                                ])"_json;
        checker.generate_index_checker();
        CHECK_THROWS_AS(checker.verify_index(signed_repodata_json.patch(wrong_pkg_patch)), package_error);
    }

    TEST_CASE_FIXTURE(RepoCheckerT, "illformed_repodata")
    {
        RepoChecker checker(mambatests::context(), m_repo_base_url, m_ref_path);

        nl::json illformed_pkg_patch = R"([
                                { "op": "remove", "path": "/signatures"}
                                ])"_json;
        checker.generate_index_checker();
        CHECK_THROWS_AS(
            checker.verify_index(signed_repodata_json.patch(illformed_pkg_patch)),
            index_error
        );
    }
}
