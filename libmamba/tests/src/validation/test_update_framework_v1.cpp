// Copyright (c) 2022, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <map>

#include <catch2/catch_all.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "mamba/core/fsutil.hpp"
#include "mamba/core/util.hpp"
#include "mamba/util/encoding.hpp"
#include "mamba/validation/errors.hpp"
#include "mamba/validation/tools.hpp"
#include "mamba/validation/update_framework_v1.hpp"

#include "mambatests.hpp"

using namespace mamba;
using namespace mamba::validation;
namespace nl = nlohmann;

class RootImplT_v1
{
public:

    using role_secrets_type = std::map<std::string, std::array<std::byte, MAMBA_ED25519_KEYSIZE_BYTES>>;
    using secrets_type = std::map<std::string, role_secrets_type>;

    RootImplT_v1()
    {
        channel_dir = std::make_unique<TemporaryDirectory>();

        generate_secrets();
        sign_root();
    }

    auto trusted_root_file() -> fs::u8path
    {
        fs::u8path p = channel_dir->path() / "root.json";

        std::ofstream out_file(p.std_path(), std::ofstream::out | std::ofstream::trunc);
        out_file << root1_json;
        out_file.close();

        return p;
    }

    auto create_root_update(const fs::u8path& name, const nl::json& patch = nl::json()) -> fs::u8path
    {
        fs::u8path p = channel_dir->path() / name;

        std::ofstream out_file(p.std_path(), std::ofstream::out | std::ofstream::trunc);

        nl::json new_root = root1_json;

        if (!patch.empty())
        {
            new_root = new_root.patch(patch);
        }

        nl::json sig_patch = nl::json::parse(
            R"([
                                        { "op": "replace", "path": "/signatures", "value":)"
            + sign_root_meta(new_root.at("signed")).dump() + R"(}
                                        ])"
        );
        out_file << new_root.patch(sig_patch);
        out_file.close();

        return p;
    }

    void generate_secrets(int root = 1, int targets = 1, int snapshot = 1, int timestamp = 1)
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

    auto sign_root_meta(const nl::json& root_meta) -> nl::json
    {
        std::vector<RoleSignature> signatures;
        auto sig_bin = std::array<std::byte, MAMBA_ED25519_SIGSIZE_BYTES>{};

        for (auto& secret : secrets.at("root"))
        {
            sign(root_meta.dump(), secret.second.data(), sig_bin.data());

            auto sig_hex = util::bytes_to_hex_str(sig_bin.data(), sig_bin.data() + sig_bin.size());
            signatures.push_back({ secret.first, sig_hex });
        }

        return signatures;
    }

protected:

    fs::u8path root1 = mambatests::test_data_dir / "validation/root.json";
    nl::json root1_json;

    std::unique_ptr<TemporaryDirectory> channel_dir;

    secrets_type secrets;

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
    TEST_CASE_FIXTURE(RootImplT_v1, "ctor_from_path")
    {
        v1::RootImpl root(trusted_root_file());

        REQUIRE(root.type() == "root");
        REQUIRE(root.file_ext() == "json");
        REQUIRE(root.spec_version() == v1::SpecImpl("1.0.17");
        REQUIRE(root.version() == 1);
    }

    TEST_CASE_FIXTURE(RootImplT_v1, "ctor_from_json")
    {
        v1::RootImpl root(root1_json);

        REQUIRE(root.type() == "root");
        REQUIRE(root.file_ext() == "json");
        REQUIRE(root.spec_version() == v1::SpecImpl("1.0.17");
        REQUIRE(root.version() == 1);
    }

    TEST_CASE_FIXTURE(RootImplT_v1, "update_from_path")
    {
        using namespace mamba;

        v1::RootImpl root(trusted_root_file());

        nl::json patch = R"([
                        { "op": "replace", "path": "/signed/version", "value": 2 }
                        ])"_json;
        auto updated_root = root.update(create_root_update("2.root.json", patch));

        auto testing_root = static_cast<v1::RootImpl*>(updated_root.get());
        REQUIRE(testing_root->type() == "root");
        REQUIRE(testing_root->file_ext() == "json");
        REQUIRE(testing_root->spec_version() == v1::SpecImpl("1.0.17");
        REQUIRE(testing_root->version() == 2);
    }

    TEST_CASE_FIXTURE(RootImplT_v1, "ctor_wrong_filename_spec_version")
    {
        fs::u8path p = channel_dir->path() / "2.sv0.6.root.json";

        std::ofstream out_file(p.std_path(), std::ofstream::out | std::ofstream::trunc);
        out_file << root1_json;
        out_file.close();

        // "2.sv0.6.root.json" is not compatible spec version (spec version N)
        REQUIRE_THROWS_AS(v1::RootImpl root(p), role_file_error);
    }

    TEST_CASE_FIXTURE(RootImplT_v1, "wrong_version")
    {
        v1::RootImpl root(root1_json);

        nl::json patch = R"([
                        { "op": "replace", "path": "/signed/version", "value": 3 }
                        ])"_json;

        REQUIRE_THROWS_AS(root.update(create_root_update("2.root.json", patch)), role_metadata_error);
    }

    TEST_CASE_FIXTURE(RootImplT_v1, "spec_version")
    {
        v1::RootImpl root(root1_json);

        nl::json patch = R"([
                        { "op": "replace", "path": "/signed/version", "value": 2 },
                        { "op": "replace", "path": "/signed/spec_version", "value": "1.30.10" }
                        ])"_json;

        auto updated_root = root.update(create_root_update("2.root.json", patch));

        auto testing_root = static_cast<v1::RootImpl*>(updated_root.get());
        REQUIRE(testing_root->spec_version() == v1::SpecImpl("1.30.10");
        REQUIRE(testing_root->version() == 2);
    }

    TEST_CASE_FIXTURE(RootImplT_v1, "wrong_spec_version")
    {
        v1::RootImpl root(root1_json);

        nl::json patch = R"([
                        { "op": "replace", "path": "/signed/spec_version", "value": "2.0.0" }
                        ])"_json;

        REQUIRE_THROWS_AS(root.update(create_root_update("2.root.json", patch)), spec_version_error);
    }

    TEST_CASE_FIXTURE(RootImplT_v1, "wrong_filename_role")
    {
        v1::RootImpl root(root1_json);

        nl::json patch = R"([])"_json;

        REQUIRE_THROWS_AS(root.update(create_root_update("2.rooot.json", patch)), role_file_error);
    }

    TEST_CASE_FIXTURE(RootImplT_v1, "wrong_filename_version")
    {
        v1::RootImpl root(root1_json);

        nl::json patch = R"([])"_json;

        REQUIRE_THROWS_AS(root.update(create_root_update("3.root.json", patch)), role_file_error);
    }

    TEST_CASE_FIXTURE(RootImplT_v1, "wrong_filename_spec_version")
    {
        v1::RootImpl root(root1_json);

        // "2.sv2.root.json" is upgradable spec version (spec version N+1)
        // but v2 is NOT implemented yet, so v1::RootImpl is not upgradable
        REQUIRE_THROWS_AS(root.update(create_root_update("2.sv2.root.json")), spec_version_error);
        // "2.sv3.root.json" is NOT upgradable spec version (spec version N+1)
        REQUIRE_THROWS_AS(root.update(create_root_update("2.sv3.root.json")), role_file_error);
        REQUIRE_THROWS_AS(root.update(create_root_update("2.sv0.6.root.json")), role_file_error);
    }

    TEST_CASE_FIXTURE(RootImplT_v1, "illformed_filename_version")
    {
        v1::RootImpl root(root1_json);

        nl::json patch = R"([])"_json;

        REQUIRE_THROWS_AS(root.update(create_root_update("wrong.root.json", patch)), role_file_error);
    }

    TEST_CASE_FIXTURE(RootImplT_v1, "rollback_attack")
    {
        v1::RootImpl root(root1_json);

        nl::json patch = R"([
                        { "op": "replace", "path": "/signed/version", "value": 1 }
                        ])"_json;

        REQUIRE_THROWS_AS(root.update(create_root_update("2.root.json", patch)), rollback_error);
    }

    TEST_CASE_FIXTURE(RootImplT_v1, "wrong_type")
    {
        v1::RootImpl root(root1_json);

        nl::json patch = R"([
                        { "op": "replace", "path": "/signed/_type", "value": "timestamp" },
                        { "op": "replace", "path": "/signed/version", "value": 2 }
                        ])"_json;

        REQUIRE_THROWS_AS(root.update(create_root_update("2.root.json", patch)), role_metadata_error);
    }

    TEST_CASE_FIXTURE(RootImplT_v1, "missing_type")
    {
        v1::RootImpl root(root1_json);

        nl::json patch = R"([
                        { "op": "remove", "path": "/signed/_type" },
                        { "op": "replace", "path": "/signed/version", "value": 2 }
                        ])"_json;

        REQUIRE_THROWS_AS(root.update(create_root_update("2.root.json", patch)), role_metadata_error);
    }

    TEST_CASE_FIXTURE(RootImplT_v1, "missing_keys")
    {
        v1::RootImpl root(root1_json);

        nl::json patch = R"([
                        { "op": "remove", "path": "/signed/keys" },
                        { "op": "replace", "path": "/signed/version", "value": 2 }
                        ])"_json;

        REQUIRE_THROWS_AS(root.update(create_root_update("2.root.json", patch)), role_metadata_error);
    }

    TEST_CASE_FIXTURE(RootImplT_v1, "missing_roles")
    {
        v1::RootImpl root(root1_json);

        nl::json patch = R"([
                        { "op": "remove", "path": "/signed/roles" },
                        { "op": "replace", "path": "/signed/version", "value": 2 }
                        ])"_json;

        REQUIRE_THROWS_AS(root.update(create_root_update("2.root.json", patch)), role_metadata_error);
    }

    TEST_CASE_FIXTURE(RootImplT_v1, "missing_role")
    {
        v1::RootImpl root(root1_json);

        nl::json patch = R"([
                        { "op": "remove", "path": "/signed/roles/timestamp" },
                        { "op": "replace", "path": "/signed/version", "value": 2 }
                        ])"_json;

        REQUIRE_THROWS_AS(root.update(create_root_update("2.root.json", patch)), role_metadata_error);
    }

    TEST_CASE_FIXTURE(RootImplT_v1, "empty_role_keyids")
    {
        v1::RootImpl root(root1_json);

        nl::json patch = R"([
                                    { "op": "replace", "path": "/signed/roles/snapshot/keyids", "value": [] },
                                    { "op": "replace", "path": "/signed/version", "value": 2 }
                                    ])"_json;

        REQUIRE_THROWS_AS(root.update(create_root_update("2.root.json", patch)), role_metadata_error);
    }

    TEST_CASE_FIXTURE(RootImplT_v1, "null_role_threshold")
    {
        v1::RootImpl root(root1_json);

        nl::json patch = R"([
                                    { "op": "replace", "path": "/signed/roles/snapshot/threshold", "value": 0 },
                                    { "op": "replace", "path": "/signed/version", "value": 2 }
                                    ])"_json;

        REQUIRE_THROWS_AS(root.update(create_root_update("2.root.json", patch)), role_metadata_error);
    }

    TEST_CASE_FIXTURE(RootImplT_v1, "extra_roles")
    {
        v1::RootImpl root(root1_json);

        nl::json patch = R"([
                        { "op": "add", "path": "/signed/roles/some_wrong_role", "value": { "keyids": ["c"], "threshold": 1 } },
                        { "op": "replace", "path": "/signed/version", "value": 2 }
                        ])"_json;

        REQUIRE_THROWS_AS(root.update(create_root_update("2.root.json", patch)), role_metadata_error);
    }

    TEST_CASE_FIXTURE(RootImplT_v1, "key_not_found")
    {
        v1::RootImpl root(root1_json);

        nl::json patch = R"([
                        { "op": "add", "path": "/signed/roles/snapshot/keyids/-", "value": "c" },
                        { "op": "replace", "path": "/signed/version", "value": 2 }
                        ])"_json;

        REQUIRE_THROWS_AS(root.update(create_root_update("2.root.json", patch)), role_metadata_error);
    }

    TEST_CASE_FIXTURE(RootImplT_v1, "mirrors_role")
    {
        nl::json patch = R"([
                        { "op": "add", "path": "/signed/roles/mirrors", "value": { "keyids": ["c"], "threshold": 1 } },
                        { "op": "add", "path": "/signed/keys/c", "value": { "scheme": "ed25519", "keytype": "ed25519", "keyval": "c"} },
                        { "op": "replace", "path": "/signed/version", "value": 2 }
                        ])"_json;

        const v1::RootImpl root(create_root_update("2.root.json", patch));
        const bool mirrors_role_found = root.roles().find("mirrors") != root.roles().cend();
        REQUIRE(mirrors_role_found);
    }

    TEST_CASE_FIXTURE(RootImplT_v1, "threshold_not_met")
    {
        v1::RootImpl root(root1_json);

        nl::json patch = R"([
                        { "op": "replace", "path": "/signed/version", "value": 2 },
                        { "op": "replace", "path": "/signed/roles/root/threshold", "value": 2 }
                        ])"_json;

        REQUIRE_THROWS_AS(root.update(create_root_update("2.root.json", patch)), role_error);
    }

    TEST_CASE_FIXTURE(RootImplT_v1, "expires")
    {
        v1::RootImpl root(root1_json);

        // expiration is set to now+3600s in 'sign_root'
        TimeRef time_ref;
        REQUIRE_FALSE(root.expired(time_ref));

        time_ref.set(utc_time_now() + 7200);
        REQUIRE(root.expired(time_ref));

        nl::json patch = nl::json::parse(
            R"([
                        { "op": "replace", "path": "/signed/expires", "value": ")"
            + timestamp(utc_time_now() + 10800) + R"(" },
                        { "op": "replace", "path": "/signed/version", "value": 2 }
                        ])"
        );
        auto updated_root = root.update(create_root_update("2.root.json", patch));

        auto testing_root = static_cast<v1::RootImpl*>(updated_root.get());
        REQUIRE_FALSE(testing_root->expired(time_ref));

        patch = nl::json::parse(R"([
                        { "op": "replace", "path": "/signed/expires", "value": "2051-10-08T07:07:09+0030" },
                        { "op": "replace", "path": "/signed/version", "value": 2 }
                        ])");
        REQUIRE_THROWS_AS(root.update(create_root_update("2.root.json", patch)), role_metadata_error);

        patch = nl::json::parse(R"([
                        { "op": "replace", "path": "/signed/expires", "value": "2051-10-08T07:07:09D" },
                        { "op": "replace", "path": "/signed/version", "value": 2 }
                        ])");
        REQUIRE_THROWS_AS(root.update(create_root_update("2.root.json", patch)), role_metadata_error);

        patch = nl::json::parse(R"([
                        { "op": "replace", "path": "/signed/expires", "value": "2051-10-08T07:07:09.000" },
                        { "op": "replace", "path": "/signed/version", "value": 2 }
                        ])");
        REQUIRE_THROWS_AS(root.update(create_root_update("2.root.json", patch)), role_metadata_error);
    }

    TEST_CASE_FIXTURE(RootImplT_v1, "possible_update_files")
    {
        v1::RootImpl root(root1_json);

        auto update_f = root.possible_update_files();
        REQUIRE(update_f[0].string().c_str() == doctest::Contains("2.sv2.root.json"));
        REQUIRE(update_f[1].string().c_str() == doctest::Contains("2.sv1.root.json"));
        REQUIRE(update_f[2].string().c_str() == doctest::Contains("2.root.json"));

        nl::json patch = nl::json::parse(R"([
                        { "op": "replace", "path": "/signed/version", "value": 2 }
                        ])");
        auto updated_root = root.update(create_root_update("2.root.json", patch));
        update_f = updated_root->possible_update_files();
        REQUIRE(update_f[0].string().c_str() == doctest::Contains("3.sv2.root.json"));
        REQUIRE(update_f[1].string().c_str() == doctest::Contains("3.sv1.root.json"));
        REQUIRE(update_f[2].string().c_str() == doctest::Contains("3.root.json"));
    }
}

class SpecImplT_v1
{
public:

    SpecImplT_v1() = default;

protected:

    v1::SpecImpl spec;
};

namespace
{
    TEST_CASE_FIXTURE(SpecImplT_v1, "ctore")
    {
        v1::SpecImpl new_spec("1.0.0");
        REQUIRE(new_spec.version_str() == "1.0.0");
    }

    TEST_CASE_FIXTURE(SpecImplT_v1, "version_str")
    {
        REQUIRE(spec.version_str() == "1.0.17");
    }

    TEST_CASE_FIXTURE(SpecImplT_v1, "is_compatible")
    {
        REQUIRE(spec.is_compatible(std::string("1.0.0")));
        REQUIRE(spec.is_compatible(std::string("1.0.17")));
        REQUIRE(spec.is_compatible(std::string("1.25.10")));

        REQUIRE_FALSE(spec.is_compatible(std::string("2.0.0")));
        REQUIRE_FALSE(spec.is_compatible(std::string("2.0.17")));
        REQUIRE_FALSE(spec.is_compatible(std::string("0.6.0")));
    }

    TEST_CASE_FIXTURE(SpecImplT_v1, "is_upgrade")
    {
        REQUIRE(spec.is_upgrade(std::string("2.0.0")));
        REQUIRE(spec.is_upgrade(std::string("2.1.10")));

        REQUIRE_FALSE(spec.is_upgrade(std::string("0.6.0")));
        REQUIRE_FALSE(spec.is_upgrade(std::string("3.0.0")));
        // not an upgrade, compatible version
        REQUIRE_FALSE(spec.is_upgrade(std::string("1.0.17")));
        REQUIRE_FALSE(spec.is_upgrade(std::string("1.0.0")));
    }

    TEST_CASE_FIXTURE(SpecImplT_v1, "upgradable")
    {
        REQUIRE_FALSE(spec.upgradable());
    }

    TEST_CASE_FIXTURE(SpecImplT_v1, "compatible_prefix")
    {
        REQUIRE(spec.compatible_prefix() == "1");
    }

    TEST_CASE_FIXTURE(SpecImplT_v1, "upgrade_prefix")
    {
        REQUIRE(spec.upgrade_prefix()[0].c_str() == doctest::Contains("2"));
    }

    TEST_CASE_FIXTURE(SpecImplT_v1, "json_key")
    {
        REQUIRE(spec.json_key() == "spec_version");
    }

    TEST_CASE_FIXTURE(SpecImplT_v1, "expiration_json_key")
    {
        REQUIRE(spec.expiration_json_key() == "expires");
    }

    TEST_CASE_FIXTURE(SpecImplT_v1, "canonicalize")
    {
        REQUIRE(spec.canonicalize(R"({"foo":"bar"})"_json) == "{\"foo\":\"bar\"}");
    }

    TEST_CASE_FIXTURE(SpecImplT_v1, "signatures")
    {
        nl::json j = R"({
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
        REQUIRE(sigs.size() == 1);
        REQUIRE(sigs.begin()->keyid == "foo");
        REQUIRE(sigs.begin()->sig == "baz");
        REQUIRE(sigs.begin()->pgp_trailer == "bar");
    }
}

namespace
{
    // Test serialization/deserialization
    TEST_CASE("to_json")
    {
        RoleSignature s{ "some_key_id", "some_signature", "" };
        nl::json j = R"({"keyid": "some_key_id", "sig": "some_signature"})"_json;
        REQUIRE(j == nl::json(s);

        s = { "some_key_id", "some_signature", "some_pgp_trailer" };
        j = R"({"keyid": "some_key_id", "other_headers": "some_pgp_trailer", "sig": "some_signature"})"_json;
        REQUIRE(j == nl::json(s);
    }
}
