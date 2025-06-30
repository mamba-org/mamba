// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <simdjson.h>

#include "mamba/core/output.hpp"
#include "mamba/core/util.hpp"
#include "mamba/solver/libsolv/parameters.hpp"
#include "mamba/solver/resolvo/database.hpp"
#include "mamba/specs/channel.hpp"
#include "mamba/specs/package_info.hpp"
#include "mamba/util/string.hpp"

namespace mamba::solver::resolvo
{

    Database::Database(specs::ChannelResolveParams channel_params)
        : name_pool(bijective_map<::resolvo::NameId, ::resolvo::String>())
        , m_channel_params(std::move(channel_params))
    {
    }

    auto Database::channel_params() const -> const specs::ChannelResolveParams&
    {
        return m_channel_params;
    }

    void Database::add_repo_from_repodata_json(
        const fs::u8path& filename,
        const std::string& repo_url,
        const std::string& channel_id,
        bool verify_artifacts
    )
    {
        // BEWARE:
        // We use below `simdjson`'s "on-demand" parser, which does not tolerate reading the same
        // value more than once. This means we need to make sure that the objects and their fields
        // are read and/or concretized only once and if we need to use them more than once we need
        // to persist them in local memory. This is why the code below tries hard to pre-read the
        // data needed in several parts of the computing in a way that prevents jumping up and down
        // the hierarchy of json objects. When this rule is not followed, the parsing might end
        // earlier than expected or might skip data that are read when they shouldn't be, leading to
        // *runtime issues* that might not be visible at first. Because of these reasons, be careful
        // when modifying the following parsing code.

        auto parser = simdjson::ondemand::parser();
        const auto lock = LockFile(filename);

        // The json storage must be kept alive as long as we are reading the json data.
        const auto json_content = simdjson::padded_string::load(filename.string());

        // Note that with the "on-demand" parser, documents/values/objects act as iterators
        // to go through the document.
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

        auto repodata_info = [&]
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

        // An override for missing package subdir could be found at the top level
        const auto default_subdir = [&]
        {
            if (repodata_info)
            {
                if (auto subdir = repodata_info.value()["subdir"]; !subdir.error())
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
            if (repodata_version == 2 && repodata_info)
            {
                if (auto url = repodata_info.value()["base_url"]; !url.error())
                {
                    return std::string(url.get_string().value_unsafe());
                }
            }

            return repo_url;
        }();

        const auto parsed_url = specs::CondaURL::parse(base_url)
                                    .or_else([](specs::ParseError&& err) { throw std::move(err); })
                                    .value();

        // TODO: it does not seems resolvo can handle setting signatures on solvables for now
        // auto signatures = [&]
        // {
        //     auto maybe_sigs = repodata_doc["signatures"];
        //     if (!maybe_sigs.error() && verify_artifacts)
        //     {
        //         return std::make_optional(maybe_sigs);
        //     }
        //     else
        //     {
        //         LOG_DEBUG << "No signatures available or requested. Downloading without verifying
        //         artifacts."; return decltype(std::make_optional(maybe_sigs)){};
        //     }
        // }();

        // Process packages.conda first
        if (auto pkgs = repodata_doc["packages.conda"]; !pkgs.error())
        {
            if (auto packages_as_object = pkgs.get_object(); !packages_as_object.error())
            {
                for (auto field : packages_as_object)
                {
                    if (!field.error())
                    {
                        const std::string key(field.unescaped_key().value());
                        if (auto value = field.value(); !value.error())
                        {
                            if (auto pkg_obj = value.get_object(); !pkg_obj.error())
                            {
                                auto package_info = specs::PackageInfo::from_json(
                                    filename.string(),
                                    pkg_obj.value(),
                                    parsed_url,
                                    channel_id
                                );
                                if (!package_info)
                                {
                                    LOG_WARNING << package_info.error().what();
                                }
                                alloc_solvable(package_info.value());
                            }
                        }
                    }
                }
            }
        }

        // Then process packages
        if (auto pkgs = repodata_doc["packages"]; !pkgs.error())
        {
            if (auto packages_as_object = pkgs.get_object(); !packages_as_object.error())
            {
                for (auto field : packages_as_object)
                {
                    if (!field.error())
                    {
                        const std::string key(field.unescaped_key().value());
                        if (auto value = field.value(); !value.error())
                        {
                            if (auto pkg_obj = value.get_object(); !pkg_obj.error())
                            {
                                auto package_info = specs::PackageInfo::from_json(
                                    filename.string(),
                                    pkg_obj.value(),
                                    parsed_url,
                                    channel_id
                                );
                                if (!package_info)
                                {
                                    LOG_WARNING << package_info.error().what();
                                }
                                alloc_solvable(package_info.value());
                            }
                        }
                    }
                }
            }
        }
    }

    void Database::add_repo_from_packages(
        const std::vector<specs::PackageInfo>& packages,
        [[maybe_unused]] const std::string& repo_name,
        [[maybe_unused]] bool pip_as_python_dependency
    )
    {
        for (const auto& package : packages)
        {
            alloc_solvable(package);
        }
    }

    void Database::set_installed_repo([[maybe_unused]] const std::string& repo_name)
    {
        // TODO: Implement this
    }

    /**
     * Allocates a new requirement and return the id of the requirement.
     */
    ::resolvo::VersionSetId Database::alloc_version_set(std::string_view raw_match_spec)
    {
        std::string raw_match_spec_str = std::string(raw_match_spec);
        // Replace all " v" with simply " " to work around the `v` prefix in some version strings
        // e.g. `mingw-w64-ucrt-x86_64-crt-git v12.0.0.r2.ggc561118da h707e725_0` in
        // `inform2w64-sysroot_win-64-v12.0.0.r2.ggc561118da-h707e725_0.conda`
        while (raw_match_spec_str.find(" v") != std::string::npos)
        {
            raw_match_spec_str = raw_match_spec_str.replace(raw_match_spec_str.find(" v"), 2, " ");
        }

        // Remove any presence of selector on python version in the match spec
        // e.g. `pillow-heif >=0.10.0,<1.0.0<py312` -> `pillow-heif >=0.10.0,<1.0.0` in
        // `infowillow-1.6.3-pyhd8ed1ab_0.conda`
        for (const auto specifier : { "=py", "<py", ">py", ">=py", "<=py", "!=py" })
        {
            while (raw_match_spec_str.find(specifier) != std::string::npos)
            {
                raw_match_spec_str = raw_match_spec_str.substr(0, raw_match_spec_str.find(specifier));
            }
        }
        // Remove any white space between version
        // e.g. `kytea >=0.1.4, 0.2.0` -> `kytea >=0.1.4,0.2.0` in
        // `infokonoha-4.6.3-pyhd8ed1ab_0.tar.bz2`
        while (raw_match_spec_str.find(", ") != std::string::npos)
        {
            raw_match_spec_str = raw_match_spec_str.replace(raw_match_spec_str.find(", "), 2, ",");
        }

        // TODO: skip allocation for now if "*.*" is in the match spec
        if (raw_match_spec_str.find("*.*") != std::string::npos)
        {
            return ::resolvo::VersionSetId{ 0 };
        }

        // NOTE: works around `openblas 0.2.18|0.2.18.*.` from
        // `dlib==19.0=np110py27_blas_openblas_200` If contains "|", split on it and recurse
        if (raw_match_spec_str.find("|") != std::string::npos)
        {
            std::vector<std::string> match_specs;
            std::string match_spec;
            for (char c : raw_match_spec_str)
            {
                if (c == '|')
                {
                    match_specs.push_back(match_spec);
                    match_spec.clear();
                }
                else
                {
                    match_spec += c;
                }
            }
            match_specs.push_back(match_spec);
            for (const std::string& ms : match_specs)
            {
                alloc_version_set(ms);
            }
            // Placeholder return value
            return ::resolvo::VersionSetId{ 0 };
        }

        // NOTE: This works around some improperly encoded `constrains` in the test data, e.g.:
        //      `openmpi-4.1.4-ha1ae619_102`'s improperly encoded `constrains`: "cudatoolkit
        //      >= 10.2" `pytorch-1.13.0-cpu_py310h02c325b_0.conda`'s improperly encoded
        //      `constrains`: "pytorch-cpu = 1.13.0", "pytorch-gpu = 99999999"
        //      `fipy-3.4.2.1-py310hff52083_3.tar.bz2`'s improperly encoded `constrains` or `dep`:
        //      ">=4.5.2"
        // Remove any with space after the binary operators
        for (const char* op : { ">=", "<=", "==", ">", "<", "!=", "=", "==" })
        {
            const std::string bad_op = std::string(op) + " ";
            while (raw_match_spec_str.find(bad_op) != std::string::npos)
            {
                raw_match_spec_str = raw_match_spec_str.substr(0, raw_match_spec_str.find(bad_op)) + op
                                     + raw_match_spec_str.substr(
                                         raw_match_spec_str.find(bad_op) + bad_op.size()
                                     );
            }
            // If start with binary operator, prepend NONE
            if (raw_match_spec_str.find(op) == 0)
            {
                raw_match_spec_str = "NONE " + raw_match_spec_str;
            }
        }

        const specs::MatchSpec match_spec = specs::MatchSpec::parse(raw_match_spec_str).value();
        // Add the version set to the version set pool
        auto id = version_set_pool.alloc(match_spec);

        // Add name to the Name and String pools
        const std::string name = match_spec.name().to_string();
        name_pool.alloc(::resolvo::String{ name });
        string_pool.alloc(::resolvo::String{ name });

        // Add the MatchSpec's string representation to the Name and String pools
        const std::string match_spec_str = match_spec.to_string();
        name_pool.alloc(::resolvo::String{ match_spec_str });
        string_pool.alloc(::resolvo::String{ match_spec_str });
        return id;
    }

    /**
     * Allocates a new solvable and returns its id.
     *
     * - Adds the solvable to the solvable pool.
     * - Adds the name to the Name and String pools.
     * - Adds the long string representation of the package to the Name and String pools.
     * - Allocates version sets for dependencies and constrains.
     * - Adds the solvable to the name_to_solvable map.
     */
    ::resolvo::SolvableId Database::alloc_solvable(specs::PackageInfo package_info)
    {
        // Add the solvable to the solvable pool
        auto id = solvable_pool.alloc(package_info);

        // Add name to the Name and String pools
        const std::string name = package_info.name;
        name_pool.alloc(::resolvo::String{ name });
        string_pool.alloc(::resolvo::String{ name });

        // Add the long string representation of the package to the Name and String pools
        const std::string long_str = package_info.long_str();
        name_pool.alloc(::resolvo::String{ long_str });
        string_pool.alloc(::resolvo::String{ long_str });

        for (auto& dep : package_info.dependencies)
        {
            alloc_version_set(dep);
        }
        for (auto& constr : package_info.constrains)
        {
            alloc_version_set(constr);
        }

        // Add the solvable to the name_to_solvable map
        const auto name_id = name_pool.alloc(::resolvo::String{ package_info.name });
        name_to_solvable[name_id].push_back(id);

        return id;
    }

    /**
     * Returns a user-friendly string representation of the specified solvable.
     *
     * When formatting the solvable, it should it include both the name of
     * the package and any other identifying properties.
     */
    ::resolvo::String Database::display_solvable(::resolvo::SolvableId solvable)
    {
        const specs::PackageInfo& package_info = solvable_pool[solvable];
        return ::resolvo::String{ package_info.long_str() };
    }

    /**
     * Returns a user-friendly string representation of the name of the
     * specified solvable.
     */
    ::resolvo::String Database::display_solvable_name(::resolvo::SolvableId solvable)
    {
        const specs::PackageInfo& package_info = solvable_pool[solvable];
        return ::resolvo::String{ package_info.name };
    }

    /**
     * Returns a string representation of multiple solvables merged together.
     *
     * When formatting the solvables, both the name of the packages and any
     * other identifying properties should be included.
     */
    ::resolvo::String
    Database::display_merged_solvables(::resolvo::Slice<::resolvo::SolvableId> solvable)
    {
        std::string result;
        for (auto& solvable_id : solvable)
        {
            result += solvable_pool[solvable_id].long_str();
        }
        return ::resolvo::String{ result };
    }

    /**
     * Returns an object that can be used to display the given name in a
     * user-friendly way.
     */
    ::resolvo::String Database::display_name(::resolvo::NameId name)
    {
        return name_pool[name];
    }

    /**
     * Returns a user-friendly string representation of the specified version
     * set.
     *
     * The name of the package should *not* be included in the display. Where
     * appropriate, this information is added.
     */
    ::resolvo::String Database::display_version_set(::resolvo::VersionSetId version_set)
    {
        const specs::MatchSpec match_spec = version_set_pool[version_set];
        return ::resolvo::String{ match_spec.to_string() };
    }

    /**
     * Returns the string representation of the specified string.
     */
    ::resolvo::String Database::display_string(::resolvo::StringId string)
    {
        return string_pool[string];
    }

    /**
     * Returns the name of the package that the specified version set is
     * associated with.
     */
    ::resolvo::NameId Database::version_set_name(::resolvo::VersionSetId version_set_id)
    {
        const specs::MatchSpec match_spec = version_set_pool[version_set_id];
        return name_pool[::resolvo::String{ match_spec.name().to_string() }];
    }

    /**
     * Returns the name of the package for the given solvable.
     */
    ::resolvo::NameId Database::solvable_name(::resolvo::SolvableId solvable_id)
    {
        const specs::PackageInfo& package_info = solvable_pool[solvable_id];
        return name_pool[::resolvo::String{ package_info.name }];
    }

    /**
     * Obtains a list of solvables that should be considered when a package
     * with the given name is requested.
     */
    ::resolvo::Candidates Database::get_candidates(::resolvo::NameId package)
    {
        ::resolvo::Candidates candidates{};
        candidates.favored = nullptr;
        candidates.locked = nullptr;
        candidates.candidates = name_to_solvable[package];
        return candidates;
    }

    /**
     * Finds the highest version and the minimum number of track features for a given version set.
     *
     * - If the version set has already been computed, returns the cached value.
     * - Filters candidates for the version set.
     * - Iterates over filtered candidates to find the maximum version and the minimum number of
     * track features.
     * - Caches and returns the result.
     */
    std::pair<specs::Version, size_t>
    Database::find_highest_version(::resolvo::VersionSetId version_set_id)
    {
        // If the version set has already been computed, return it.
        if (version_set_to_max_version_and_track_features_numbers.find(version_set_id)
            != version_set_to_max_version_and_track_features_numbers.end())
        {
            return version_set_to_max_version_and_track_features_numbers[version_set_id];
        }

        const specs::MatchSpec match_spec = version_set_pool[version_set_id];
        const std::string& name = match_spec.name().to_string();
        auto name_id = name_pool.alloc(::resolvo::String{ name });
        auto solvables = name_to_solvable[name_id];
        auto filtered = filter_candidates(solvables, version_set_id, false);

        specs::Version max_version = specs::Version();
        size_t max_version_n_track_features = 0;

        for (auto& solvable_id : filtered)
        {
            const specs::PackageInfo& package_info = solvable_pool[solvable_id];
            const auto version = specs::Version::parse(package_info.version).value();
            if (version == max_version)
            {
                max_version_n_track_features = std::min(
                    max_version_n_track_features,
                    package_info.track_features.size()
                );
            }
            if (version > max_version)
            {
                max_version = version;
                max_version_n_track_features = package_info.track_features.size();
            }
        }

        auto val = std::make_pair(max_version, max_version_n_track_features);
        version_set_to_max_version_and_track_features_numbers[version_set_id] = val;
        return val;
    }

    /**
     * Sort the specified solvables based on which solvable to try first. The
     * solver will iteratively try to select the highest version. If a
     * conflict is found with the highest version the next version is
     * tried. This continues until a solution is found.
     */
    void Database::sort_candidates(::resolvo::Slice<::resolvo::SolvableId> solvables)
    {
        std::sort(
            solvables.begin(),
            solvables.end(),
            [&](const ::resolvo::SolvableId& a, const ::resolvo::SolvableId& b)
            {
                const specs::PackageInfo& package_info_a = solvable_pool[a];
                const specs::PackageInfo& package_info_b = solvable_pool[b];

                // If track features are present, prefer the solvable having the least of them.
                if (package_info_a.track_features.size() != package_info_b.track_features.size())
                {
                    return package_info_a.track_features.size()
                           < package_info_b.track_features.size();
                }

                const auto a_version = specs::Version::parse(package_info_a.version).value();
                const auto b_version = specs::Version::parse(package_info_b.version).value();

                if (a_version != b_version)
                {
                    return a_version > b_version;
                }

                if (package_info_a.build_number != package_info_b.build_number)
                {
                    return package_info_a.build_number > package_info_b.build_number;
                }

                // Compare the dependencies of the variants.
                std::unordered_map<::resolvo::NameId, ::resolvo::VersionSetId> a_deps;
                std::unordered_map<::resolvo::NameId, ::resolvo::VersionSetId> b_deps;
                for (auto dep_a : package_info_a.dependencies)
                {
                    // TODO: have a VersionID to NameID mapping instead
                    specs::MatchSpec ms = specs::MatchSpec::parse(dep_a).value();
                    const std::string& name = ms.name().to_string();
                    auto name_id = name_pool.alloc(::resolvo::String{ name });

                    a_deps[name_id] = version_set_pool[ms];
                }
                for (auto dep_b : package_info_b.dependencies)
                {
                    // TODO: have a VersionID to NameID mapping instead
                    specs::MatchSpec ms = specs::MatchSpec::parse(dep_b).value();
                    const std::string& name = ms.name().to_string();
                    auto name_id = name_pool.alloc(::resolvo::String{ name });

                    b_deps[name_id] = version_set_pool[ms];
                }

                auto ordering_score = 0;
                for (auto [name_id, version_set_id] : a_deps)
                {
                    if (b_deps.find(name_id) != b_deps.end())
                    {
                        auto [a_tf_version, a_n_track_features] = find_highest_version(version_set_id);
                        auto [b_tf_version, b_n_track_features] = find_highest_version(b_deps[name_id]
                        );

                        // Favor the solvable with higher versions of their dependencies
                        if (a_tf_version != b_tf_version)
                        {
                            ordering_score += a_tf_version > b_tf_version ? 1 : -1;
                        }

                        // Highly penalize the solvable if a dependencies has more track features
                        if (a_n_track_features != b_n_track_features)
                        {
                            ordering_score += a_n_track_features > b_n_track_features ? -100 : 100;
                        }
                    }
                }

                if (ordering_score != 0)
                {
                    return ordering_score > 0;
                }

                return package_info_a.timestamp > package_info_b.timestamp;
            }
        );
    }

    /**
     * Given a set of solvables, return the solvables that match the given
     * version set or if `inverse` is true, the solvables that do *not* match
     * the version set.
     */
    ::resolvo::Vector<::resolvo::SolvableId> Database::filter_candidates(
        ::resolvo::Slice<::resolvo::SolvableId> candidates,
        ::resolvo::VersionSetId version_set_id,
        bool inverse
    )
    {
        specs::MatchSpec match_spec = version_set_pool[version_set_id];
        ::resolvo::Vector<::resolvo::SolvableId> filtered;

        if (inverse)
        {
            for (auto& solvable_id : candidates)
            {
                const specs::PackageInfo& package_info = solvable_pool[solvable_id];

                // Is it an appropriate check? Or must another one be crafted?
                if (!match_spec.contains_except_channel(package_info))
                {
                    filtered.push_back(solvable_id);
                }
            }
        }
        else
        {
            for (auto& solvable_id : candidates)
            {
                const specs::PackageInfo& package_info = solvable_pool[solvable_id];

                // Is it an appropriate check? Or must another one be crafted?
                if (match_spec.contains_except_channel(package_info))
                {
                    filtered.push_back(solvable_id);
                }
            }
        }

        return filtered;
    }

    /**
     * Returns the dependencies for the specified solvable.
     */
    ::resolvo::Dependencies Database::get_dependencies(::resolvo::SolvableId solvable_id)
    {
        const specs::PackageInfo& package_info = solvable_pool[solvable_id];

        ::resolvo::Dependencies dependencies;

        // TODO: do this in O(1)
        for (auto& dep : package_info.dependencies)
        {
            const specs::MatchSpec match_spec = specs::MatchSpec::parse(dep).value();
            dependencies.requirements.push_back(version_set_pool[match_spec]);
        }
        for (auto& constr : package_info.constrains)
        {
            // if constr contain " == " replace it with "=="
            std::string constr2 = constr;
            while (constr2.find(" == ") != std::string::npos)
            {
                constr2 = constr2.replace(constr2.find(" == "), 4, "==");
            }
            while (constr2.find(" >= ") != std::string::npos)
            {
                constr2 = constr2.replace(constr2.find(" >= "), 4, ">=");
            }
            const specs::MatchSpec match_spec = specs::MatchSpec::parse(constr2).value();
            dependencies.constrains.push_back(version_set_pool[match_spec]);
        }

        return dependencies;
    }
}
