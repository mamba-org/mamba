// Copyright (c) 2026, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.

#include <fstream>

#include <catch2/catch_all.hpp>
#include <msgpack.h>
#include <msgpack/zone.h>

#include "mamba/core/channel_context.hpp"
#include "mamba/core/shard_types.hpp"
#include "mamba/core/shards.hpp"
#include "mamba/core/util.hpp"
#include "mamba/download/mirror.hpp"
#include "mamba/download/parameters.hpp"
#include "mamba/download/request.hpp"
#include "mamba/fs/filesystem.hpp"
#include "mamba/specs/channel.hpp"
#include "mamba/specs/conda_url.hpp"
#include "mamba/specs/unresolved_channel.hpp"
#include "mamba/specs/version.hpp"
#include "mamba/specs/version_spec.hpp"
#include "mamba/util/string.hpp"

#include "api/utils.hpp"
#include "core/shard_python_minor_prefilter.hpp"

#include "mambatests.hpp"
#include "test_shard_utils.hpp"

using namespace mamba;
using namespace mambatests::shard_test_utils;

namespace
{
    auto make_simple_channel(std::string_view chan) -> specs::Channel
    {
        const auto resolve_params = ChannelContext::ChannelResolveParams{
            { "linux-64", "noarch" },
            specs::CondaURL::parse("https://conda.anaconda.org").value()
        };

        return specs::Channel::resolve(specs::UnresolvedChannel::parse(chan).value(), resolve_params)
            .value()
            .front();
    }

    auto create_shard_with_checksum(
        const std::string& package_name,
        const std::string& version,
        const std::string& build,
        const std::vector<std::string>& depends = {},
        const std::vector<std::string>& track_features = {}
    ) -> std::vector<std::uint8_t>
    {
        auto package_record = create_shard_package_record_msgpack(
            package_name,
            version,
            build,
            0,
            "abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890",
            std::nullopt,
            depends,
            {},
            std::nullopt,
            HashFormat::String,
            HashFormat::String,
            track_features
        );

        msgpack_sbuffer sbuf;
        msgpack_sbuffer_init(&sbuf);
        msgpack_packer pk;
        msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);
        msgpack_pack_map(&pk, 1);
        msgpack_pack_str(&pk, 8);
        msgpack_pack_str_body(&pk, "packages", 8);
        msgpack_pack_map(&pk, 1);
        std::string filename = package_name + "-" + version + "-" + build + ".tar.bz2";
        msgpack_pack_str(&pk, filename.size());
        msgpack_pack_str_body(&pk, filename.c_str(), filename.size());
        msgpack_sbuffer_write(
            &sbuf,
            reinterpret_cast<const char*>(package_record.data()),
            package_record.size()
        );
        std::vector<std::uint8_t> shard_msgpack(
            reinterpret_cast<const std::uint8_t*>(sbuf.data),
            reinterpret_cast<const std::uint8_t*>(sbuf.data + sbuf.size)
        );
        msgpack_sbuffer_destroy(&sbuf);
        return compress_zstd(shard_msgpack);
    }

    auto v(std::string_view s) -> specs::Version
    {
        return specs::Version::parse(std::string(s)).value();
    }
}

TEST_CASE("version_from_single_equality_spec")
{
    using specs::VersionSpec;

    SECTION("explicit double-equals")
    {
        const auto vs = VersionSpec::parse("==3.7.12").value();
        const auto got = version_from_single_equality_spec(vs);
        REQUIRE(got.has_value());
        REQUIRE(got.value() == v("3.7.12"));
    }

    SECTION("bare equality parses as single ==")
    {
        const auto vs = VersionSpec::parse("3.12.5").value();
        const auto got = version_from_single_equality_spec(vs);
        REQUIRE(got.has_value());
        REQUIRE(got.value() == v("3.12.5"));
    }

    SECTION("greater-or-equal is not single equality")
    {
        const auto vs = VersionSpec::parse(">=3.7").value();
        REQUIRE_FALSE(version_from_single_equality_spec(vs).has_value());
    }

    SECTION("conjunction is not a single leaf")
    {
        const auto vs = VersionSpec::parse(">=3.12,<3.13").value();
        REQUIRE_FALSE(version_from_single_equality_spec(vs).has_value());
    }

    SECTION("disjunction is not a single leaf")
    {
        const auto vs = VersionSpec::parse("==3.7.12|==3.8.0").value();
        REQUIRE_FALSE(version_from_single_equality_spec(vs).has_value());
    }

    SECTION("free spec")
    {
        const VersionSpec vs{};
        REQUIRE_FALSE(version_from_single_equality_spec(vs).has_value());
    }
}

TEST_CASE("extract_requested_python_minor")
{
    SECTION("empty specs")
    {
        REQUIRE_FALSE(extract_requested_python_minor({}).has_value());
    }

    SECTION("no python package")
    {
        REQUIRE_FALSE(extract_requested_python_minor({ "numpy >=1.0", "openssl 3" }).has_value());
    }

    SECTION("two-token exact pin relaxes to minor")
    {
        const auto got = extract_requested_python_minor({ "python 3.12.5" });
        REQUIRE(got.has_value());
        REQUIRE(got.value() == v("3.12"));
    }

    SECTION("three-token conda pin")
    {
        const auto got = extract_requested_python_minor({ "python 3.7.12 0_73_pypy" });
        REQUIRE(got.has_value());
        REQUIRE(got.value() == v("3.7"));
    }

    SECTION("explicit equality operator form")
    {
        const auto got = extract_requested_python_minor({ "python ==3.11" });
        REQUIRE(got.has_value());
        REQUIRE(got.value() == v("3.11"));
    }

    SECTION("range spec yields no minor (not single equality after relax)")
    {
        REQUIRE_FALSE(extract_requested_python_minor({ "python >=3.12,<3.13" }).has_value());
    }

    SECTION("uses first matching python spec")
    {
        const auto got = extract_requested_python_minor(
            { "numpy 1.0", "python 3.10.0", "python 3.11" }
        );
        REQUIRE(got.has_value());
        REQUIRE(got.value() == v("3.10"));
    }

    SECTION("bare python name without version constraint")
    {
        REQUIRE_FALSE(extract_requested_python_minor({ "python" }).has_value());
    }
}

TEST_CASE("Shards - python minor prefilter")
{
    ShardsIndexDict index;
    index.info.base_url = "https://example.com/packages";
    index.info.shards_base_url = "shards";
    index.info.subdir = "linux-64";
    index.version = 1;
    index.shards["test-pkg"] = std::vector<std::uint8_t>(32, 0xAB);

    specs::Channel channel = make_simple_channel("https://example.com/conda-forge");
    specs::AuthenticationDataBase auth_info;
    download::RemoteFetchParams remote_fetch_params;

    const auto tmp_dir = TemporaryDirectory();
    const auto shard_file = tmp_dir.path() / "test-pkg.msgpack.zst";

    std::map<std::string, fs::u8path> package_to_cache_path;
    package_to_cache_path["test-pkg"] = shard_file;

    auto run_for_dep = [&](const std::string& dep,
                           std::optional<specs::Version> python_minor) -> expected_t<ShardDict>
    {
        auto shard_data = create_shard_with_checksum("test-pkg", "1.0.0", "0", { dep });
        {
            std::ofstream file(shard_file.string(), std::ios::binary);
            file.write(
                reinterpret_cast<const char*>(shard_data.data()),
                static_cast<std::streamsize>(shard_data.size())
            );
        }

        download::Success success;
        success.content = download::Filename{ shard_file.string() };
        success.transfer.downloaded_size = shard_data.size();

        Shards shards(
            index,
            "https://example.com/conda-forge/linux-64/repodata.json",
            channel,
            auth_info,
            remote_fetch_params,
            0,
            std::nullopt,
            std::move(python_minor)
        );
        return test_process_downloaded_shard(shards, "test-pkg", success, package_to_cache_path);
    };

    SECTION("mismatching python minor is discarded before record creation")
    {
        auto result = run_for_dep(
            "python >=3.11,<3.12",
            specs::Version::parse("3.12").value_or(specs::Version())
        );
        REQUIRE(result.has_value());
        REQUIRE(result->packages.empty());
        REQUIRE(result->conda_packages.empty());
    }

    SECTION("matching python minor is retained")
    {
        auto result = run_for_dep(
            "python >=3.12,<3.13",
            specs::Version::parse("3.12").value_or(specs::Version())
        );
        REQUIRE(result.has_value());
        REQUIRE(result->packages.size() == 1);
        REQUIRE(result->packages.begin()->second.name == "test-pkg");
    }

    SECTION("no python minor context does not apply prefilter")
    {
        auto result = run_for_dep("python >=3.11,<3.12", std::nullopt);
        REQUIRE(result.has_value());
        REQUIRE(result->packages.size() == 1);
        REQUIRE(result->packages.begin()->second.name == "test-pkg");
    }

    SECTION("exact python pin matches requested minor (conda three-token depends)")
    {
        auto result = run_for_dep(
            "python 3.7.12 0_73_pypy",
            specs::Version::parse("3.7").value_or(specs::Version())
        );
        REQUIRE(result.has_value());
        REQUIRE(result->packages.size() == 1);
        REQUIRE(result->packages.begin()->second.name == "test-pkg");
    }
}

TEST_CASE("relax_version_spec_to_minor")
{
    using specs::Version;
    using specs::VersionSpec;

    const auto req = [](std::string_view s) -> Version { return Version::parse(s).value(); };

    SECTION("bare equality pin relaxes so requested minor is contained")
    {
        const auto vs = VersionSpec::parse("3.7.12").value();
        const auto relaxed = relax_version_spec_to_minor(vs);
        REQUIRE(relaxed.contains(req("3.7")));
        REQUIRE_FALSE(relaxed.contains(req("3.8")));
    }

    SECTION("explicit double-equals string form relaxes")
    {
        const auto vs = VersionSpec::parse("==3.7.12").value();
        const auto relaxed = relax_version_spec_to_minor(vs);
        REQUIRE(relaxed.contains(req("3.7")));
        REQUIRE(util::starts_with(relaxed.to_string(), "=="));
    }

    SECTION("four-component pin relaxes to first two components")
    {
        const auto vs = VersionSpec::parse("1.2.3.4").value();
        const auto relaxed = relax_version_spec_to_minor(vs);
        REQUIRE(relaxed.contains(req("1.2")));
        REQUIRE_FALSE(relaxed.contains(req("1.3")));
    }

    SECTION("greater-or-equal is unchanged")
    {
        const auto vs = VersionSpec::parse(">=3.7").value();
        REQUIRE(relax_version_spec_to_minor(vs).to_string() == vs.to_string());
    }

    SECTION("less-than is unchanged")
    {
        const auto vs = VersionSpec::parse("<4").value();
        REQUIRE(relax_version_spec_to_minor(vs).to_string() == vs.to_string());
    }

    SECTION("compatible-release operator is unchanged")
    {
        const auto vs = VersionSpec::parse("~=3.7").value();
        REQUIRE(relax_version_spec_to_minor(vs).to_string() == vs.to_string());
    }

    SECTION("not-equal is unchanged")
    {
        const auto vs = VersionSpec::parse("!=3.7.12").value();
        REQUIRE(relax_version_spec_to_minor(vs).to_string() == vs.to_string());
    }

    SECTION("disjunction is unchanged")
    {
        const auto vs = VersionSpec::parse("==3.7.12|==3.8.0").value();
        REQUIRE(relax_version_spec_to_minor(vs).to_string() == vs.to_string());
    }

    SECTION("conjunction is unchanged")
    {
        const auto vs = VersionSpec::parse(">=3.7,<3.8").value();
        REQUIRE(relax_version_spec_to_minor(vs).to_string() == vs.to_string());
    }

    SECTION("free spec is unchanged")
    {
        const VersionSpec vs{};
        REQUIRE(relax_version_spec_to_minor(vs).is_explicitly_free());
    }
}

TEST_CASE("matches_python_minor")
{
    const auto req = [](std::string_view s) -> specs::Version
    { return specs::Version::parse(s).value(); };

    SECTION("non-python dependency is not filtered")
    {
        REQUIRE(matches_python_minor("numpy >=1.0", req("3.12")));
        REQUIRE(matches_python_minor("libstdcxx-ng >=12", req("3.12")));
    }

    SECTION("name starting with python but not the python package")
    {
        REQUIRE(matches_python_minor("python_abi 3.12 1_cp312", req("3.12")));
    }

    SECTION("python version range matches requested minor")
    {
        REQUIRE(matches_python_minor("python >=3.12,<3.13", req("3.12")));
        REQUIRE(matches_python_minor("python >=3.12", req("3.12")));
    }

    SECTION("python version range does not match requested minor")
    {
        REQUIRE_FALSE(matches_python_minor("python >=3.12,<3.13", req("3.11")));
        REQUIRE_FALSE(matches_python_minor("python >=3.12,<3.13", req("3.13")));
    }

    SECTION("exact three-token conda pin matches requested minor")
    {
        REQUIRE(matches_python_minor("python 3.7.12 0_73_pypy", req("3.7")));
    }

    SECTION("two-token exact pin matches requested minor")
    {
        REQUIRE(matches_python_minor("python 3.7.12", req("3.7")));
    }

    SECTION("exact pin does not match different minor")
    {
        REQUIRE_FALSE(matches_python_minor("python 3.8.0", req("3.7")));
    }

    SECTION("leading whitespace on dependency line")
    {
        REQUIRE(matches_python_minor("  python >=3.12,<3.13", req("3.12")));
    }

    SECTION("unparsable python dependency does not filter (passes)")
    {
        REQUIRE(matches_python_minor("python ,,not-a-valid-spec,,", req("3.12")));
    }

    SECTION("namespaced python pin")
    {
        REQUIRE(matches_python_minor("conda-forge::python 3.7.12 0_73_pypy", req("3.7")));
    }

    SECTION("only python in range with no upper bound")
    {
        REQUIRE(matches_python_minor("python", req("3.12")));
    }
}
