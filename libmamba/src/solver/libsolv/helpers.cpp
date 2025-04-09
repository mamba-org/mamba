// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <string>
#include <string_view>
#include <type_traits>
#include <variant>

#include <simdjson.h>
#include <solv/conda.h>
#include <solv/repo.h>
#include <solv/repo_conda.h>
#include <solv/repo_solv.h>
#include <solv/repo_write.h>
#include <solv/solvable.h>
#include <solv/solver.h>

#include "mamba/core/output.hpp"
#include "mamba/core/util.hpp"
#include "mamba/specs/archive.hpp"
#include "mamba/specs/conda_url.hpp"
#include "mamba/util/cfile.hpp"
#include "mamba/util/random.hpp"
#include "mamba/util/string.hpp"
#include "mamba/util/type_traits.hpp"

#include "solver/helpers.hpp"
#include "solver/libsolv/helpers.hpp"
#include "solver/libsolv/matcher.hpp"

#define MAMBA_TOOL_VERSION "2.0"

#define MAMBA_SOLV_VERSION MAMBA_TOOL_VERSION "_" LIBSOLV_VERSION_STRING

namespace mamba::solver::libsolv
{
    // Beyond this value, the timestamp would be in milliseconds and therefore should be converted
    // to seconds.
    inline constexpr auto MAX_CONDA_TIMESTAMP = 253402300799ULL;

    void set_solvable(solv::ObjPool& pool, solv::ObjSolvableView solv, const specs::PackageInfo& pkg)
    {
        solv.set_name(pkg.name);
        solv.set_version(pkg.version);
        solv.set_build_string(pkg.build_string);
        if (pkg.noarch != specs::NoArchType::No)
        {
            auto noarch = std::string(specs::noarch_name(pkg.noarch));  // SSO
            solv.set_noarch(noarch);
        }
        solv.set_build_number(pkg.build_number);
        solv.set_channel(pkg.channel);
        // TODO In the case of a repo with all similar subdir (which is not the case in the
        // install repo) we could also not set this (to save the strings stored in libsolv)
        // and recreate it by concatenating filename and repo URL.
        solv.set_url(pkg.package_url);
        solv.set_platform(pkg.platform);
        solv.set_file_name(pkg.filename);
        solv.set_license(pkg.license);
        solv.set_size(pkg.size);
        // TODO conda timestamp are not Unix timestamp.
        // Libsolv normalize them this way, we need to do the same here otherwise the current
        // package may get arbitrary priority.
        solv.set_timestamp(
            (pkg.timestamp > MAX_CONDA_TIMESTAMP) ? (pkg.timestamp / 1000) : pkg.timestamp
        );
        solv.set_md5(pkg.md5);
        solv.set_sha256(pkg.sha256);

        for (const auto& dep : pkg.dependencies)
        {
            // TODO pool's matchspec2id
            const solv::DependencyId dep_id = pool.add_conda_dependency(dep);
            assert(dep_id);
            solv.add_dependency(dep_id);
        }

        for (const auto& cons : pkg.constrains)
        {
            // TODO pool's matchspec2id
            const solv::DependencyId dep_id = pool.add_conda_dependency(cons);
            assert(dep_id);
            solv.add_constraint(dep_id);
        }

        solv.add_track_features(pkg.track_features);

        solv.add_self_provide();
    }

    auto make_package_info(const solv::ObjPool& pool, solv::ObjSolvableViewConst s)
        -> specs::PackageInfo
    {
        specs::PackageInfo out = {};

        out.name = s.name();
        out.version = s.version();
        out.build_string = s.build_string();
        out.noarch = specs::noarch_parse(s.noarch()).value_or(specs::NoArchType::No);
        out.build_number = s.build_number();
        out.channel = s.channel();
        out.package_url = s.url();
        out.platform = s.platform();
        out.filename = s.file_name();
        out.license = s.license();
        out.size = s.size();
        out.timestamp = s.timestamp();
        out.md5 = s.md5();
        out.sha256 = s.sha256();
        out.signatures = s.signatures();

        const auto dep_to_str = [&pool](solv::DependencyId id)
        { return pool.dependency_to_string(id); };
        {
            const auto deps = s.dependencies();
            out.dependencies.reserve(deps.size());
            std::transform(deps.cbegin(), deps.cend(), std::back_inserter(out.dependencies), dep_to_str);
        }
        {
            const auto cons = s.constraints();
            out.constrains.reserve(cons.size());
            std::transform(cons.cbegin(), cons.cend(), std::back_inserter(out.constrains), dep_to_str);
        }
        {
            const auto id_to_str = [&pool](solv::StringId id)
            { return std::string(pool.get_string(id)); };
            auto feats = s.track_features();
            out.track_features.reserve(feats.size());
            std::transform(feats.begin(), feats.end(), std::back_inserter(out.track_features), id_to_str);
        }

        return out;
    }

    namespace
    {
        auto lsplit_track_features(std::string_view features)
        {
            constexpr auto is_sep = [](char c) -> bool { return (c == ',') || util::is_space(c); };
            auto [_, tail] = util::lstrip_if_parts(features, is_sep);
            return util::lstrip_if_parts(tail, [&](char c) { return !is_sep(c); });
        }

        void set_solv_signatures(
            solv::ObjSolvableView solv,
            const std::string& filename,
            const std::optional<nlohmann::json>& signatures
        )
        {
            // NOTE We need to use an intermediate nlohmann::json object to store signatures
            // as simdjson objects are not conceived to be modified smoothly
            // and we need an equivalent structure to how libsolv is storing the signatures
            nlohmann::json glob_sigs;
            if (signatures)
            {
                if (auto signatures_for_file = signatures->find(filename);
                    signatures_for_file != signatures->end())
                {
                    glob_sigs["signatures"] = *signatures_for_file;

                    solv.set_signatures(glob_sigs.dump());
                    LOG_INFO << "Signatures for '" << filename
                             << "' are set in corresponding solvable.";
                }
            }
        }

        template <class SimdJSONValue>
        std::optional<nlohmann::json> extract_signatures(std::optional<SimdJSONValue>& signatures)
        {
            if (!signatures || signatures->error())
            {
                return {};
            }

            const std::string raw_json(signatures->raw_json().value());
            auto all_signatures = nlohmann::json::parse(raw_json);

            return all_signatures;
        }

        template <class JSONObject>
        [[nodiscard]] auto set_solvable(
            solv::ObjPool& pool,
            // const std::string& repo_url_str,
            const specs::CondaURL& repo_url,
            const std::string& channel_id,
            solv::ObjSolvableView solv,

            const std::string& filename,
            JSONObject&& pkg,
            const std::optional<nlohmann::json>& signatures,
            const std::string& default_subdir
        ) -> bool
        {
            // Not available from RepoDataPackage
            solv.set_url((repo_url / filename).str(specs::CondaURL::Credentials::Show));
            solv.set_channel(channel_id);

            solv.set_file_name(filename);
            if (auto fn = pkg["fn"]; !fn.error())
            {
                solv.set_name(fn.get_string().value_unsafe());
            }
            else
            {
                // Fallback from key entry
                solv.set_file_name(filename);
            }

            if (auto name = pkg["name"]; !name.error())
            {
                solv.set_name(name.get_string().value_unsafe());
            }
            else
            {
                LOG_WARNING << R"(Found invalid name in ")" << filename << R"(")";
                return false;
            }

            if (auto version = pkg["version"]; !version.error())
            {
                solv.set_version(version.get_string().value_unsafe());
            }
            else
            {
                LOG_WARNING << R"(Found invalid version in ")" << filename << R"(")";
                return false;
            }

            if (auto build_string = pkg["build"]; !build_string.error())
            {
                solv.set_build_string(build_string.get_string().value_unsafe());
            }
            else
            {
                LOG_WARNING << R"(Found invalid build in ")" << filename << R"(")";
                return false;
            }

            if (auto build_number = pkg["build_number"]; !build_number.error())
            {
                solv.set_build_number(build_number.get_uint64().value_unsafe());
            }
            else
            {
                LOG_WARNING << R"(Found invalid build_number in ")" << filename << R"(")";
                return false;
            }

            if (auto subdir = pkg["subdir"]; !subdir.error())
            {
                solv.set_platform(std::string(subdir.get_string().value_unsafe()));
            }
            else
            {
                solv.set_platform(default_subdir);
            }

            if (auto size = pkg["size"]; !size.error())
            {
                solv.set_size(size.get_uint64().value_unsafe());
            }

            if (auto md5 = pkg["md5"]; !md5.error())
            {
                solv.set_md5(std::string(md5.get_string().value_unsafe()));
            }

            if (auto sha256 = pkg["sha256"]; !sha256.error())
            {
                solv.set_sha256(std::string(sha256.get_string().value_unsafe()));
            }

            if (auto elem = pkg["noarch"]; !elem.error())
            {
                if (auto noarch = elem.get_bool(); !noarch.error() && noarch.value_unsafe())
                {
                    solv.set_noarch("generic");
                }
                else if (elem.is_string())
                {
                    solv.set_noarch(std::string(elem.get_string().value_unsafe()));
                }
            }

            if (auto license = pkg["license"]; !license.error())
            {
                solv.set_license(std::string(license.get_string().value_unsafe()));
            }

            // TODO conda timestamp are not Unix timestamp.
            // Libsolv normalize them this way, we need to do the same here otherwise the current
            // package may get arbitrary priority.
            if (auto timestamp = pkg["timestamp"]; !timestamp.error())
            {
                const auto time = timestamp.get_uint64().value_unsafe();
                solv.set_timestamp((time > MAX_CONDA_TIMESTAMP) ? (time / 1000) : time);
            }

            if (auto depends = pkg["depends"].get_array(); !depends.error())
            {
                for (auto elem : depends)
                {
                    if (!elem.error() && elem.is_string())
                    {
                        if (const auto dep_id = pool.add_conda_dependency(
                                std::string(elem.get_string().value_unsafe())
                            ))
                        {
                            solv.add_dependency(dep_id);
                        }
                    }
                }
            }

            if (auto constrains = pkg["constrains"]; !constrains.error())
            {
                for (auto elem : constrains.get_array())
                {
                    if (!elem.error() && elem.is_string())
                    {
                        if (const auto dep_id = pool.add_conda_dependency(
                                std::string(elem.get_string().value_unsafe())
                            ))
                        {
                            solv.add_constraint(dep_id);
                        }
                    }
                }
            }

            if (auto obj = pkg["track_features"]; !obj.error())
            {
                if (obj.is_string())
                {
                    auto splits = lsplit_track_features(obj.get_string().value_unsafe());
                    while (!splits[0].empty())
                    {
                        solv.add_track_feature(splits[0]);
                        splits = lsplit_track_features(splits[1]);
                    }
                }
                else
                {
                    // assuming obj is an array
                    for (auto elem : obj.get_array())
                    {
                        if (!elem.error() && elem.is_string())
                        {
                            solv.add_track_feature(elem.get_string().value_unsafe());
                        }
                    }
                }
            }

            // Setting signatures in solvable if they are available and `verify-artifacts` flag is
            // enabled
            set_solv_signatures(solv, filename, signatures);

            solv.add_self_provide();
            return true;
        }

        template <class T>
        class showme;

        template <typename JSONObject, typename Filter, typename OnParsed>
        void set_repo_solvables_impl(
            solv::ObjPool& pool,
            solv::ObjRepoView repo,
            const specs::CondaURL& repo_url,
            const std::string& channel_id,
            const std::string& default_subdir,
            JSONObject& packages,
            const std::optional<nlohmann::json>& signatures,
            Filter&& filter,
            OnParsed&& on_parsed
        )
        {
            auto packages_as_object = packages.get_object();
            for (auto pkg_field : packages_as_object)
            {
                // showme<decltype(packages)> debug;
                const std::string filename(pkg_field.unescaped_key().value());
                if (filter(filename))
                {
                    auto [id, solv] = repo.add_solvable();
                    const bool parsed = set_solvable(
                        pool,
                        repo_url,
                        channel_id,
                        solv,
                        filename,
                        pkg_field.value(),
                        signatures,
                        default_subdir
                    );
                    if (parsed)
                    {
                        on_parsed(filename);
                    }
                    else
                    {
                        repo.remove_solvable(id, /* reuse_id= */ true);
                        LOG_WARNING << "Failed to parse from repodata " << filename;
                    }
                }
            }
        }

        template <typename JSONObject>
        void set_repo_solvables(
            solv::ObjPool& pool,
            solv::ObjRepoView repo,
            const specs::CondaURL& repo_url,
            const std::string& channel_id,
            const std::string& default_subdir,
            JSONObject& packages,
            const std::optional<nlohmann::json>& signatures
        )
        {
            return set_repo_solvables_impl(
                pool,
                repo,
                repo_url,
                channel_id,
                default_subdir,
                packages,
                signatures,
                /* filter= */ [](const auto&) { return true; },
                /* on_parsed= */ [](const auto&) {}
            );
        }

        template <typename JSONObject>
        auto set_repo_solvables_and_return_added_filename_stem(
            solv::ObjPool& pool,
            solv::ObjRepoView repo,
            const specs::CondaURL& repo_url,
            const std::string& channel_id,
            const std::string& default_subdir,
            JSONObject& packages,
            const std::optional<nlohmann::json>& signatures
        ) -> util::flat_set<std::string>
        {
            auto filenames = util::flat_set<std::string>();
            set_repo_solvables_impl(
                pool,
                repo,
                repo_url,
                channel_id,
                default_subdir,
                packages,
                signatures,
                /* filter= */ [](const auto&) { return true; },
                /* on_parsed= */ [&](const auto& fn)
                { filenames.insert(std::string(specs::strip_archive_extension(fn))); }
            );
            // Sort only once
            return filenames;
        }

        template <class JSONObject, class SortedStringRange>
        void set_repo_solvables_if_not_already_set(
            solv::ObjPool& pool,
            solv::ObjRepoView repo,
            const specs::CondaURL& repo_url,
            const std::string& channel_id,
            const std::string& default_subdir,
            JSONObject& packages,
            const std::optional<nlohmann::json>& signatures,
            const SortedStringRange& added
        )
        {
            return set_repo_solvables_impl(
                pool,
                repo,
                repo_url,
                channel_id,
                default_subdir,
                packages,
                signatures,
                /* filter= */ [&](const auto& fn)
                { return !added.contains(specs::strip_archive_extension(fn)); },
                /* on_parsed= */ [&](const auto&) {}
            );
        }
    }

    auto libsolv_read_json(
        solv::ObjRepoView repo,
        const fs::u8path& filename,
        PackageTypes types,
        bool verify_artifacts
    ) -> expected_t<solv::ObjRepoView>
    {
        if ((types != PackageTypes::TarBz2Only) && (types != PackageTypes::CondaOrElseTarBz2))
        {
            return make_unexpected(
                "Invalid PackageTypes option for libsolv repodata.json parser:"
                " supported types are TarBz2Only and CondaOrElseTarBz2.",
                mamba_error_code::repodata_not_loaded
            );
        }

        LOG_INFO << "Reading repodata.json file " << filename << " for repo " << repo.name()
                 << " using libsolv";

        int flags = (types == PackageTypes::TarBz2Only) ? CONDA_ADD_USE_ONLY_TAR_BZ2 : 0;
        if (verify_artifacts)
        {
            // cf.
            // https://github.com/openSUSE/libsolv/commit/cc2da2e789f651b2d0d55fe31c258426bf9e984d
            flags |= CONDA_ADD_WITH_SIGNATUREDATA;
        }

        const auto lock = LockFile(filename);

        return util::CFile::try_open(filename, "rb")
            .transform_error([](std::error_code&& ec) { return ec.message(); })
            .and_then(
                [&](util::CFile&& file_ptr) -> tl::expected<void, std::string>
                {
                    auto out = repo.legacy_read_conda_repodata(file_ptr.raw(), flags);
                    file_ptr.try_close().or_else([&](const auto& err) {  //
                        LOG_WARNING << R"(Fail to close file ")" << filename << R"(": )" << err;
                    });
                    return out;
                }
            )
            .transform([&]() { return repo; })
            .transform_error(
                [](std::string&& str)
                { return mamba_error(std::move(str), mamba_error_code::repodata_not_loaded); }
            );
    }

    auto mamba_read_json(
        solv::ObjPool& pool,
        solv::ObjRepoView repo,
        const fs::u8path& filename,
        const std::string& repo_url,
        const std::string& channel_id,
        PackageTypes package_types,
        bool verify_artifacts
    ) -> expected_t<solv::ObjRepoView>
    {
        LOG_INFO << "Reading repodata.json file " << filename << " for repo " << repo.name()
                 << " using mamba";

        // WARNING:
        // We use below SimdJson's "on demand" parser, which does not tolerate
        // reading the same value more than once. This means we need to make
        // sure that the objects and their fields are read once and kept around
        // until being used if necessary. This is why the code belows tries hard
        // to pre-read the general data in a way that prevents jumping up and down
        // the hierarchy of json objects. When this rule is not followed, the parsing
        // might end earlier than expected or might skip data that are read when they
        // shouldnt be, leading to *runtime issues* that might not be visible at first.
        // Because of these reasons, be careful when modifingy the following parsing
        // code.

        auto parser = simdjson::ondemand::parser();
        const auto lock = LockFile(filename);
        const auto json_content = simdjson::padded_string::load(filename.string());  // must be kept
                                                                                     // alive while
                                                                                     // reading json
        auto repodata_doc = parser.iterate(json_content);

        const auto repodata_version = [&]
        {
            if (auto version = repodata_doc["repodata_version"].get_int64(); !version.error())
            {
                return version.value();
            }
            else
            {
                return std::int64_t{ 1 };
            }
        }();


        auto info = [&]
        {
            if (auto value = repodata_doc["info"]; !value.error())
            {
                if (auto object = value.get_object(); !object.error())
                {
                    return std::make_optional(object);
                }
            }
            return decltype(std::make_optional(repodata_doc["info"].get_object())){};
        }();

        // An override for missing package subdir is found at the top level
        const auto default_subdir = [&]
        {
            if (info)
            {
                if (auto subdir = info.value()["subdir"]; !subdir.error())
                {
                    return std::string(subdir.get_string().value_unsafe());
                }
            }

            return std::string{};
        }();


        // Get `base_url` in case 'repodata_version': 2
        // cf. https://github.com/conda-incubator/ceps/blob/main/cep-15.md
        const auto base_url = [&]
        {
            if (repodata_version == 2 && info)
            {
                if (auto url = info.value()["base_url"]; !url.error())
                {
                    return std::string(url.get_string().value_unsafe());
                }
            }

            return repo_url;
        }();

        const auto parsed_url = specs::CondaURL::parse(base_url)
                                    .or_else([](specs::ParseError&& err) { throw std::move(err); })
                                    .value();

        auto signatures = [&]
        {
            auto maybe_sigs = repodata_doc["signatures"];
            if (!maybe_sigs.error() && verify_artifacts)
            {
                return std::make_optional(maybe_sigs);
            }
            else
            {
                LOG_DEBUG << "No signatures available or requested. Downloading without verifying artifacts.";
                return decltype(std::make_optional(maybe_sigs)){};
            }
        }();


        const auto json_signatures = extract_signatures(signatures);

        if (package_types == PackageTypes::CondaOrElseTarBz2)
        {
            auto added = util::flat_set<std::string>();
            if (auto pkgs = repodata_doc["packages.conda"]; !pkgs.error())
            {
                added = set_repo_solvables_and_return_added_filename_stem(  //
                    pool,
                    repo,
                    parsed_url,
                    channel_id,
                    default_subdir,
                    pkgs,
                    json_signatures
                );
            }
            if (auto pkgs = repodata_doc["packages"]; !pkgs.error())
            {
                set_repo_solvables_if_not_already_set(  //
                    pool,
                    repo,
                    parsed_url,
                    channel_id,
                    default_subdir,
                    pkgs,
                    json_signatures,
                    added
                );
            }
        }
        else
        {
            if (auto pkgs = repodata_doc["packages"];
                !pkgs.error() && (package_types != PackageTypes::CondaOnly))
            {
                set_repo_solvables(  //
                    pool,
                    repo,
                    parsed_url,
                    channel_id,
                    default_subdir,
                    pkgs,
                    json_signatures
                );
            }

            if (auto pkgs = repodata_doc["packages.conda"];
                !pkgs.error() && (package_types != PackageTypes::TarBz2Only))
            {
                set_repo_solvables(  //
                    pool,
                    repo,
                    parsed_url,
                    channel_id,
                    default_subdir,
                    pkgs,
                    json_signatures
                );
            }
        }

        return { repo };
    }

    [[nodiscard]] auto read_solv(
        solv::ObjPool& pool,
        solv::ObjRepoView repo,
        const fs::u8path& filename,
        const RepodataOrigin& expected,
        bool expected_pip_added
    ) -> expected_t<solv::ObjRepoView>
    {
        static constexpr auto expected_binary_version = std::string_view(MAMBA_SOLV_VERSION);

        LOG_INFO << "Attempting to read libsolv solv file " << filename << " for repo "
                 << repo.name();

        if (!fs::exists(filename))
        {
            return make_unexpected(
                fmt::format(R"(File "{}" does not exist)", filename),
                mamba_error_code::repodata_not_loaded
            );
        }

        {
            auto j = nlohmann::json(expected);
            j["tool_version"] = expected_binary_version;
            LOG_INFO << "Expecting solv metadata : " << j.dump();
        }

        auto lock = LockFile(filename);

        return util::CFile::try_open(filename, "rb")
            .transform_error([](std::error_code&& ec) { return ec.message(); })
            .and_then(
                [&](util::CFile&& file_ptr) -> tl::expected<void, std::string>
                {
                    auto out = repo.read(file_ptr.raw());
                    file_ptr.try_close().or_else([&](const auto& err) {  //
                        LOG_WARNING << R"(Fail to close file ")" << filename << R"(": )" << err;
                    });
                    return out;
                }
            )
            .transform_error(
                [](std::string&& str)
                { return mamba_error(std::move(str), mamba_error_code::repodata_not_loaded); }
            )
            .and_then(
                [&]() -> expected_t<solv::ObjRepoView>
                {
                    auto read_binary_version = repo.tool_version();

                    if (read_binary_version != expected_binary_version)
                    {
                        repo.clear(/* reuse_ids= */ true);
                        return make_unexpected(
                            "Metadata from solv are binary incompatible",
                            mamba_error_code::repodata_not_loaded
                        );
                    }

                    const auto read_metadata = RepodataOrigin{
                        /* .url= */ std::string(repo.url()),
                        /* .etag= */ std::string(repo.etag()),
                        /* .mod= */ std::string(repo.mod()),
                    };

                    {
                        auto j = nlohmann::json(read_metadata);
                        j["tool_version"] = read_binary_version;
                        LOG_INFO << "Loaded solv metadata : " << j.dump();
                    }

                    if ((read_metadata == RepodataOrigin{}) || (read_metadata != expected))
                    {
                        repo.clear(/* reuse_ids= */ true);
                        return make_unexpected(
                            "Metadata from solv are outdated",
                            mamba_error_code::repodata_not_loaded
                        );
                    }

                    const bool read_pip_added = repo.pip_added();
                    if (expected_pip_added != read_pip_added)
                    {
                        if (expected_pip_added)
                        {
                            add_pip_as_python_dependency(pool, repo);
                            LOG_INFO << "Added missing pip dependencies";
                        }
                        else
                        {
                            repo.clear(/* reuse_ids= */ true);
                            return make_unexpected(
                                "Metadata from solv contain extra pip dependencies",
                                mamba_error_code::repodata_not_loaded
                            );
                        }
                    }

                    LOG_INFO << "Metadata from solv are valid, loading successful";
                    return { repo };
                }
            );
    }

    auto write_solv(solv::ObjRepoView repo, fs::u8path filename, const RepodataOrigin& metadata)
        -> expected_t<solv::ObjRepoView>
    {
        LOG_INFO << "Writing libsolv solv file " << filename << " for repo " << repo.name();

        repo.set_url(metadata.url);
        repo.set_etag(metadata.etag);
        repo.set_mod(metadata.mod);
        repo.set_tool_version(MAMBA_SOLV_VERSION);
        repo.internalize();

        fs::create_directories(filename.parent_path());
        const auto lock = LockFile(fs::exists(filename) ? filename : filename.parent_path());

        return util::CFile::try_open(filename, "wb")
            .transform_error([](std::error_code&& ec) { return ec.message(); })
            .and_then(
                [&](util::CFile&& file_ptr) -> tl::expected<void, std::string>
                {
                    auto out = repo.write(file_ptr.raw());
                    file_ptr.try_close().or_else([&](const auto& err) {  //
                        LOG_WARNING << R"(Fail to close file ")" << filename << R"(": )" << err;
                    });
                    return out;
                }
            )
            .transform([&]() { return repo; })
            .transform_error(
                [](std::string&& str)
                { return mamba_error(std::move(str), mamba_error_code::repodata_not_loaded); }
            );
    }

    void
    set_solvables_url(solv::ObjRepoView repo, const std::string& repo_url, const std::string& channel_id)
    {
        // WARNING cannot call ``url()`` at this point because it has not been internalized.
        // Setting the channel url on where the solvable so that we can retrace
        // where it came from
        const auto url = specs::CondaURL::parse(repo_url)
                             .or_else([](specs::ParseError&& err) { throw std::move(err); })
                             .value();
        repo.for_each_solvable(
            [&](solv::ObjSolvableView s)
            {
                // The solvable url, this is not set in libsolv parsing so we set it manually
                // while we still rely on libsolv for parsing
                // TODO
                s.set_url((url / s.file_name()).str(specs::CondaURL::Credentials::Show));
                // The name of the channel where it came from, may be different from repo name
                // for instance with the installed repo
                s.set_channel(channel_id);
            }
        );
    }

    void add_pip_as_python_dependency(solv::ObjPool& pool, solv::ObjRepoView repo)
    {
        const solv::DependencyId python_id = pool.add_conda_dependency("python");
        const solv::DependencyId pip_id = pool.add_conda_dependency("pip");
        repo.for_each_solvable(
            [&](solv::ObjSolvableView s)
            {
                if ((s.name() == "python") && !s.version().empty() && (s.version()[0] >= '2'))
                {
                    s.add_dependency(pip_id);
                }
                if (s.name() == "pip")
                {
                    s.add_dependency(python_id, SOLVABLE_PREREQMARKER);
                }
            }
        );
        repo.set_pip_added(true);
    }

    auto
    make_abused_namespace_dep_args(solv::ObjPool& pool, std::string_view dependency, const MatchFlags& flags)
        -> std::pair<solv::StringId, solv::StringId>
    {
        return {
            pool.add_string(dependency),
            pool.add_string(flags.internal_serialize()),
        };
    }

    auto get_abused_namespace_callback_args(  //
        solv::ObjPoolView& pool,
        solv::StringId name,
        solv::StringId ver
    ) -> std::pair<std::string_view, MatchFlags>
    {
        return {
            pool.get_string(name),
            MatchFlags::internal_deserialize(pool.get_string(ver)),
        };
    }

    [[nodiscard]] auto pool_add_matchspec(  //
        solv::ObjPool& pool,
        const specs::MatchSpec& ms
    ) -> expected_t<solv::DependencyId>
    {
        auto check_not_zero = [&](solv::DependencyId id) -> expected_t<solv::DependencyId>
        {
            if (id == 0)
            {
                return make_unexpected(
                    fmt::format(R"(Invalid MatchSpec "{}")", ms.str()),
                    mamba_error_code::invalid_spec
                );
            }
            return id;
        };

        if (ms.is_simple())
        {
            return check_not_zero(pool.add_conda_dependency(ms.conda_build_form()));
        }
        const auto [first, second] = make_abused_namespace_dep_args(pool, ms.str());
        return check_not_zero(pool.add_dependency(first, REL_NAMESPACE, second));
    }

    auto pool_add_pin(  //
        solv::ObjPool& pool,
        const specs::MatchSpec& pin
    ) -> expected_t<solv::ObjSolvableView>
    {
        // In libsolv, locking means that a package keeps the same state: if it is installed,
        // it remains installed, if not it remains uninstalled.
        // Locking on a spec applies the lock to all packages matching the spec.
        // In mamba, we do not want to lock the package because we want to allow other variants
        // (matching the same spec) to unlock more solutions.
        // For instance we may pin ``libfmt=8.*`` but allow it to be swapped with a version built
        // by a more recent compiler.
        //
        // A previous version of this function would use ``SOLVER_LOCK`` to lock all packages not
        // matching the pin.
        // That played poorly with ``all_problems_structured`` because we could not interpret
        // the ids that were returned (since they were not associated with a single reldep).
        //
        // Another wrong idea is to add the pin as an install job.
        // This is not what is expected of pins, as they must not be installed if they were not
        // in the environment.
        // They can be configure in ``.condarc`` for generally specifying what versions are wanted.
        //
        // The idea behind the current version is to add the pin/spec as a constraint that must be
        // fullfield only if the package is installed.
        // This is not supported on solver jobs but it is on ``Solvable`` with
        // ``disttype == DISTYPE_CONDA``.
        // Therefore, we add a dummy solvable marked as already installed, and add the pin/spec
        // as one of its constrains.
        // Then we lock this solvable and force the re-checking of its dependencies.


        if (pool.disttype() != DISTTYPE_CONDA)
        {
            return make_unexpected(
                fmt::format(R"(Cannot add pin "{}" to a pool that is not of Conda distype)", pin.str()),
                mamba_error_code::incorrect_usage
            );
        }
        auto installed = [&]() -> solv::ObjRepoView
        {
            if (auto repo = pool.installed_repo())
            {
                return *repo;
            }
            // If the installed repo does not exists, we can safely create it because this is
            // called right before the solve function.
            // If it gets modified latter on the pin should not interfere with user packages.
            // If it gets overridden this it is not a problem for the solve because pins are added
            // on each solve.
            auto [id, repo] = pool.add_repo("installed");
            pool.set_installed_repo(id);
            return repo;
        }();

        return pool_add_matchspec(pool, pin).transform(
            [&](solv::DependencyId cons)
            {
                // Add dummy solvable with a constraint on the pin (not installed if not
                // present)
                auto [cons_solv_id, cons_solv] = installed.add_solvable();
                const std::string cons_solv_name = fmt::format(
                    "pin-{}",
                    util::generate_random_alphanumeric_string(10)
                );
                cons_solv.set_name(cons_solv_name);
                cons_solv.set_version("1");

                cons_solv.add_constraint(cons);

                // Solvable need to provide itself
                cons_solv.add_self_provide();

                // Even if we lock it, libsolv may still try to remove it with
                // `SOLVER_FLAG_ALLOW_UNINSTALL`, so we flag it as not a real package to filter
                // it out in the transaction
                cons_solv.set_type(solv::SolvableType::Pin);

                // Necessary for attributes to be properly stored
                // TODO move this at the end of all job requests
                installed.internalize();

                return cons_solv;
            }
        );
    }

    namespace
    {

        template <typename Func>
        auto transaction_to_solution_impl(
            const solv::ObjPool& pool,
            const solv::ObjTransaction& trans,
            Func&& filter
        ) -> Solution
        {
            auto get_pkginfo = [&](solv::SolvableId id)
            {
                assert(pool.get_solvable(id).has_value());
                return make_package_info(pool, pool.get_solvable(id).value());
            };

            auto get_newer_pkginfo = [&](solv::SolvableId id)
            {
                auto maybe_newer_id = trans.step_newer(pool, id);
                assert(maybe_newer_id.has_value());
                return get_pkginfo(maybe_newer_id.value());
            };

            auto out = Solution::action_list();
            out.reserve(trans.size());
            trans.for_each_step_id(
                [&](const solv::SolvableId id)
                {
                    auto pkginfo = get_pkginfo(id);

                    // In libsolv, system dependencies are provided as a special dependency,
                    // while in Conda it is implemented as a virtual package.
                    // Maybe there is a way to tell libsolv to never try to install or remove these
                    // solvables (SOLVER_LOCK or SOLVER_USERINSTALLED?).
                    // In the meantime (and probably later for safety) we filter all virtual
                    // packages out.
                    if (util::starts_with(pkginfo.name, "__"))  // i.e. is_virtual_package
                    {
                        return;
                    }

                    // here are packages that were added to implement a feature
                    // (e.g. a pin) but do not represent a Conda package.
                    // They can appear in the transaction depending on libsolv flags.
                    // We use this attribute to filter them out.
                    if (const auto solv = pool.get_solvable(id);
                        solv.has_value() && (solv->type() != solv::SolvableType::Package))
                    {
                        LOG_DEBUG << "Solution: Remove artificial " << pkginfo.str();
                        return;
                    }

                    // We can specifically filter out packages, for things such as deps-only or
                    // no-deps.
                    // We add them as omitted anyhow so that downstream code can print them for
                    // instance.
                    if (!filter(pkginfo))
                    {
                        LOG_DEBUG << "Solution: Omit " << pkginfo.str();
                        out.emplace_back(Solution::Omit{ std::move(pkginfo) });
                        return;
                    }

                    const auto type = trans.step_type(
                        pool,
                        id,
                        SOLVER_TRANSACTION_SHOW_OBSOLETES | SOLVER_TRANSACTION_OBSOLETE_IS_UPGRADE
                    );
                    switch (type)
                    {
                        case SOLVER_TRANSACTION_UPGRADED:
                        {
                            auto newer = get_newer_pkginfo(id);
                            LOG_DEBUG << "Solution: Upgrade " << pkginfo.str() << " -> "
                                      << newer.str();
                            out.emplace_back(Solution::Upgrade{
                                /* .remove= */ std::move(pkginfo),
                                /* .install= */ std::move(newer),
                            });
                            break;
                        }
                        case SOLVER_TRANSACTION_CHANGED:
                        {
                            auto newer = get_newer_pkginfo(id);
                            LOG_DEBUG << "Solution: Change " << pkginfo.str() << " -> "
                                      << newer.str();
                            out.emplace_back(Solution::Change{
                                /* .remove= */ std::move(pkginfo),
                                /* .install= */ std::move(newer),
                            });
                            break;
                        }
                        case SOLVER_TRANSACTION_REINSTALLED:
                        {
                            LOG_DEBUG << "Solution: Reinstall " << pkginfo.str();
                            out.emplace_back(Solution::Reinstall{ std::move(pkginfo) });
                            break;
                        }
                        case SOLVER_TRANSACTION_DOWNGRADED:
                        {
                            auto newer = get_newer_pkginfo(id);
                            LOG_DEBUG << "Solution: Downgrade " << pkginfo.str() << " -> "
                                      << newer.str();
                            out.emplace_back(Solution::Downgrade{
                                /* .remove= */ std::move(pkginfo),
                                /* .install= */ std::move(newer),
                            });
                            break;
                        }
                        case SOLVER_TRANSACTION_ERASE:
                        {
                            LOG_DEBUG << "Solution: Remove " << pkginfo.str();
                            out.emplace_back(Solution::Remove{ std::move(pkginfo) });
                            break;
                        }
                        case SOLVER_TRANSACTION_INSTALL:
                        {
                            LOG_DEBUG << "Solution: Install " << pkginfo.str();
                            out.emplace_back(Solution::Install{ std::move(pkginfo) });
                            break;
                        }
                        case SOLVER_TRANSACTION_IGNORE:
                            break;
                        default:
                            LOG_WARNING << "solv::ObjTransaction case not handled: " << type;
                            break;
                    }
                }
            );
            return { std::move(out) };
        }
    }

    auto transaction_to_solution_all(const solv::ObjPool& pool, const solv::ObjTransaction& trans)
        -> Solution
    {
        return transaction_to_solution_impl(pool, trans, [](const auto&) { return true; });
    }

    namespace
    {
        auto package_is_requested(const Request& request, const specs::PackageInfo& pkg) -> bool
        {
            auto job_matches = [&pkg](const auto& job) -> bool
            {
                using Job = std::decay_t<decltype(job)>;
                if constexpr (util::is_any_of_v<Job, Request::Install, Request::Remove, Request::Update>)
                {
                    return job.spec.name().contains(pkg.name);
                }
                return false;
            };

            auto iter = std::find_if(
                request.jobs.cbegin(),
                request.jobs.cend(),
                [&](const auto& unknown_job) { return std::visit(job_matches, unknown_job); }
            );
            return iter != request.jobs.cend();
        }
    }

    auto transaction_to_solution_only_deps(  //
        const solv::ObjPool& pool,
        const solv::ObjTransaction& trans,
        const Request& request
    ) -> Solution
    {
        return transaction_to_solution_impl(
            pool,
            trans,
            [&](const auto& pkg) -> bool { return !package_is_requested(request, pkg); }
        );
    }

    auto transaction_to_solution_no_deps(  //
        const solv::ObjPool& pool,
        const solv::ObjTransaction& trans,
        const Request& request
    ) -> Solution
    {
        return transaction_to_solution_impl(
            pool,
            trans,
            [&](const auto& pkg) -> bool { return package_is_requested(request, pkg); }
        );
    }

    auto transaction_to_solution(
        const solv::ObjPool& pool,
        const solv::ObjTransaction& trans,
        const Request& request,
        const Request::Flags& flags
    ) -> Solution
    {
        if (!flags.keep_user_specs && flags.keep_dependencies)
        {
            return { solver::libsolv::transaction_to_solution_only_deps(pool, trans, request) };
        }
        else if (flags.keep_user_specs && !flags.keep_dependencies)
        {
            return { solver::libsolv::transaction_to_solution_no_deps(pool, trans, request) };
        }
        else if (flags.keep_user_specs && flags.keep_dependencies)
        {
            return { solver::libsolv::transaction_to_solution_all(pool, trans) };
        }
        return {};
    }

    auto installed_python(const solv::ObjPool& pool) -> std::optional<solv::ObjSolvableViewConst>
    {
        auto py_id = solv::SolvableId{ 0 };
        pool.for_each_installed_solvable(
            [&](solv::ObjSolvableViewConst s)
            {
                if (s.name() == "python")
                {
                    py_id = s.id();
                    return solv::LoopControl::Break;
                }
                return solv::LoopControl::Continue;
            }
        );
        return pool.get_solvable(py_id);
    }

    auto solution_needs_python_relink(const solv::ObjPool& pool, const Solution& solution) -> bool
    {
        if (auto installed = installed_python(pool))
        {
            if (auto newer = find_new_python_in_solution(solution))
            {
                auto installed_ver = specs::Version::parse(installed->version());
                auto newer_ver = specs::Version::parse(newer->get().version);
                return !installed_ver.has_value() || !newer_ver.has_value()
                       || !python_binary_compatible(installed_ver.value(), newer_ver.value());
            }
        }
        return false;
    }

    namespace
    {
        auto action_refers_to(const Solution::Action& unknown_action, std::string_view pkg_name)
            -> bool
        {
            return std::visit(
                [&](const auto& action)
                {
                    using Action = std::decay_t<decltype(action)>;
                    if constexpr (Solution::has_remove_v<Action>)
                    {
                        if (action.remove.name == pkg_name)
                        {
                            return true;
                        }
                    }
                    if constexpr (Solution::has_install_v<Action>)
                    {
                        if (action.install.name == pkg_name)
                        {
                            return true;
                        }
                    }
                    if constexpr (std::is_same_v<Action, Solution::Reinstall>
                                  || std::is_same_v<Action, Solution::Omit>)
                    {
                        if (action.what.name == pkg_name)
                        {
                            return true;
                        }
                    }
                    return false;
                },
                unknown_action
            );
        }
    }

    auto add_noarch_relink_to_solution(  //
        Solution solution,
        const solv::ObjPool& pool,
        std::string_view noarch_type
    ) -> Solution
    {
        pool.for_each_installed_solvable(
            [&](solv::ObjSolvableViewConst s)
            {
                if (s.noarch() == noarch_type)
                {
                    auto s_in_sol = std::find_if(
                        solution.actions.begin(),
                        solution.actions.end(),
                        [&](const auto& action) { return action_refers_to(action, s.name()); }
                    );

                    if (s_in_sol == solution.actions.end())
                    {
                        solution.actions.emplace_back(Solution::Reinstall{
                            make_package_info(pool, s) });
                    }
                    else if (auto* omit = std::get_if<Solution::Omit>(&(*s_in_sol)))
                    {
                        *s_in_sol = { Solution::Reinstall{ std::move(omit->what) } };
                    }
                }
            }
        );
        return solution;
    }

    namespace
    {
        [[nodiscard]] auto match_as_closely(solv::ObjSolvableViewConst s) -> specs::MatchSpec
        {
            auto ms = specs::MatchSpec();
            ms.set_name(specs::MatchSpec::NameSpec(std::string(s.name())));
            // Ignoring version error, the point is to find a close match
            specs::Version::parse(s.version())
                .transform(
                    [&](specs::Version&& ver)
                    {
                        ms.set_version(specs::VersionSpec::from_predicate(
                            specs::VersionPredicate::make_equal_to(std::move(ver))
                        ));
                    }
                );
            ms.set_build_string(
                specs::MatchSpec::BuildStringSpec(specs::GlobSpec(std::string(s.build_string())))
            );
            ms.set_build_number(
                specs::BuildNumberSpec(specs::BuildNumberPredicate::make_equal_to(s.build_number()))
            );
            ms.set_md5(std::string(s.md5()));
            ms.set_sha256(std::string(s.sha256()));

            return ms;
        }

        [[nodiscard]] auto
        add_reinstall_job(solv::ObjQueue& jobs, solv::ObjPool& pool, const specs::MatchSpec& ms)
            -> expected_t<void>
        {
            auto solvable = std::optional<solv::ObjSolvableViewConst>{};

            // the data about the channel is only in the prefix_data unfortunately
            pool.for_each_installed_solvable(
                [&](solv::ObjSolvableViewConst s)
                {
                    if (ms.name().contains(s.name()))
                    {
                        solvable = s;
                        return solv::LoopControl::Break;
                    }
                    return solv::LoopControl::Continue;
                }
            );

            if (solvable.has_value())
            {
                // To Reinstall, we add a install job with our custom namespace matcher,
                // passing a flag to exclude matching installed packages.
                // This has the effect of reinstalling in libsolv.
                const auto [first, second] = make_abused_namespace_dep_args(
                    pool,
                    match_as_closely(solvable.value()).str(),
                    { /* .skip_installed= */ true }
                );
                const auto job_id = pool.add_dependency(first, REL_NAMESPACE, second);
                jobs.push_back(SOLVER_INSTALL, job_id);
                return {};
            }

            // We are not reinstalling but simply installing.
            return pool_add_matchspec(pool, ms).transform([&](auto id)
                                                          { jobs.push_back(SOLVER_INSTALL, id); });
        }

        [[nodiscard]] auto has_installed_package(  //
            const solv::ObjPool& pool,
            const specs::MatchSpec::NameSpec& name_spec
        ) -> bool
        {
            bool found = false;
            pool.for_each_installed_solvable(
                [&](solv::ObjSolvableViewConst s)
                {
                    if (name_spec.contains(s.name()))
                    {
                        found = true;
                        return solv::LoopControl::Break;
                    }
                    return solv::LoopControl::Continue;
                }
            );
            return found;
        }

        template <typename Job>
        [[nodiscard]] auto
        add_job(const Job& job, solv::ObjQueue& raw_jobs, solv::ObjPool& pool, bool force_reinstall)
            -> expected_t<void>
        {
            if constexpr (std::is_same_v<Job, Request::Install>)
            {
                if (force_reinstall)
                {
                    return add_reinstall_job(raw_jobs, pool, job.spec);
                }
                else
                {
                    return pool_add_matchspec(pool, job.spec)
                        .transform([&](auto id) { raw_jobs.push_back(SOLVER_INSTALL, id); });
                }
            }
            if constexpr (std::is_same_v<Job, Request::Remove>)
            {
                return pool_add_matchspec(pool, job.spec)
                    .transform(
                        [&](auto id)
                        {
                            raw_jobs.push_back(
                                SOLVER_ERASE | (job.clean_dependencies ? SOLVER_CLEANDEPS : 0),
                                id
                            );
                        }
                    );
            }
            if constexpr (std::is_same_v<Job, Request::Update>)
            {
                return pool_add_matchspec(pool, job.spec)
                    .transform(
                        [&](auto id)
                        {
                            // In libsolv update specs apply to installed packages, not available
                            // ones, as opposed to mamba.
                            // With ``numpy=0.5`` installed, update ``numpy>=1.0`` means update
                            // numpy if a ``numpy>=1.0`` is installed, which would be false.
                            // In Mamba, it means update any installed numpy to a new
                            // ``numpy>=1.0``, leading to an update.
                            // This is especially tricky with channel-specific MatchSpec.

                            const auto clean_deps = job.clean_dependencies ? SOLVER_CLEANDEPS : 0;

                            // In this case, libsolv and mamba meanings are the same.
                            if (job.spec.is_only_package_name())
                            {
                                raw_jobs.push_back(SOLVER_UPDATE | clean_deps, id);
                            }
                            // Otherwise, we try our ad-hoc solution
                            else if (has_installed_package(pool, job.spec.name()))
                            {
                                // We still need to issue an update command to libsolv, otherwise
                                // the package won't be changed, but we apply it only to the
                                // package name, not the full spec.
                                if (job.spec.name().is_exact())
                                {
                                    auto name_id = pool.add_string(job.spec.name().str());
                                    raw_jobs.push_back(SOLVER_UPDATE | clean_deps, name_id);
                                }
                                // And we add an install statement to be sure the full spec is
                                // respected.
                                // Unfortunately this breaks ``clean_deps``.
                                raw_jobs.push_back(SOLVER_INSTALL, id);
                            }
                            // Finally there is no such package installed so we simply don't do
                            // anything.
                        }
                    );
            }
            if constexpr (std::is_same_v<Job, Request::UpdateAll>)
            {
                raw_jobs.push_back(
                    SOLVER_UPDATE | SOLVER_SOLVABLE_ALL
                        | (job.clean_dependencies ? SOLVER_CLEANDEPS : 0),
                    0
                );
                return {};
            }
            if constexpr (std::is_same_v<Job, Request::Freeze>)
            {
                return pool_add_matchspec(pool, job.spec)
                    .transform([&](auto id) { raw_jobs.push_back(SOLVER_LOCK, id); });
            }
            if constexpr (std::is_same_v<Job, Request::Keep>)
            {
                raw_jobs.push_back(SOLVER_USERINSTALLED, pool_add_matchspec(pool, job.spec).value());
                return {};
            }
            if constexpr (std::is_same_v<Job, Request::Pin>)
            {
                return pool_add_pin(pool, job.spec)
                    .transform(
                        [&](solv::ObjSolvableView pin_solv)
                        {
                            const auto name_id = pool.add_string(pin_solv.name());
                            // WARNING keep separate or libsolv does not understand
                            // Force verify the dummy solvable dependencies, as this is not the
                            // default for installed packages.
                            raw_jobs.push_back(SOLVER_VERIFY, name_id);
                            // Lock the dummy solvable so that it stays install.
                            raw_jobs.push_back(SOLVER_LOCK, name_id);
                        }
                    );
            }
            assert(false);
            return {};
        }
    }

    auto request_to_decision_queue(  //
        const Request& request,
        solv::ObjPool& pool,
        bool force_reinstall
    ) -> expected_t<solv::ObjQueue>
    {
        auto solv_jobs = solv::ObjQueue();

        auto error = expected_t<void>();
        for (const auto& unknown_job : request.jobs)
        {
            auto xpt = std::visit(
                [&](const auto& job) -> expected_t<void>
                {
                    if constexpr (std::is_same_v<std::decay_t<decltype(job)>, Request::Pin>)
                    {
                        return add_job(job, solv_jobs, pool, force_reinstall);
                    }
                    return {};
                },
                unknown_job
            );
            if (!xpt)
            {
                return forward_error(std::move(xpt));
            }
        }
        // Pins add solvables to Pol and hence require a call to create_whatprovides.
        // For some reason we need to add them first.
        pool.create_whatprovides();
        for (const auto& unknown_job : request.jobs)
        {
            auto xpt = std::visit(
                [&](const auto& job) -> expected_t<void>
                {
                    if constexpr (!std::is_same_v<std::decay_t<decltype(job)>, Request::Pin>)
                    {
                        return add_job(job, solv_jobs, pool, force_reinstall);
                    }
                    return {};
                },
                unknown_job
            );
            if (!xpt)
            {
                return forward_error(std::move(xpt));
            }
        }
        return { std::move(solv_jobs) };
    }
}
