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
    namespace
    {
        bool parse_packageinfo_json(
            const std::string_view& filename,
            simdjson::ondemand::object& pkg,
            const specs::CondaURL& repo_url,
            const std::string& channel_id,
            Database& database
        )
        {
            specs::PackageInfo package_info;

            package_info.channel = channel_id;
            package_info.filename = filename;
            package_info.package_url = (repo_url / filename).str(specs::CondaURL::Credentials::Show);

            if (auto fn = pkg["fn"].get_string(); !fn.error())
            {
                package_info.name = fn.value();
            }
            else
            {
                // Fallback from key entry
                package_info.name = filename;
            }

            if (auto name = pkg["name"].get_string(); !name.error())
            {
                package_info.name = name.value();
            }
            else
            {
                LOG_WARNING << R"(Found invalid name in ")" << filename << R"(")";
                return false;
            }

            if (auto version = pkg["version"].get_string(); !version.error())
            {
                package_info.version = version.value();
            }
            else
            {
                LOG_WARNING << R"(Found invalid version in ")" << filename << R"(")";
                return false;
            }

            if (auto build_string = pkg["build"].get_string(); !build_string.error())
            {
                package_info.build_string = build_string.value();
            }
            else
            {
                LOG_WARNING << R"(Found invalid build in ")" << filename << R"(")";
                return false;
            }

            if (auto build_number = pkg["build_number"].get_uint64(); !build_number.error())
            {
                package_info.build_number = build_number.value();
            }
            else
            {
                LOG_WARNING << R"(Found invalid build_number in ")" << filename << R"(")";
                return false;
            }

            if (auto subdir = pkg["subdir"].get_c_str(); !subdir.error())
            {
                package_info.platform = subdir.value();
            }
            else
            {
                LOG_WARNING << R"(Found invalid subdir in ")" << filename << R"(")";
            }

            if (auto size = pkg["size"].get_uint64(); !size.error())
            {
                package_info.size = size.value();
            }

            if (auto md5 = pkg["md5"].get_c_str(); !md5.error())
            {
                package_info.md5 = md5.value();
            }

            if (auto sha256 = pkg["sha256"].get_c_str(); !sha256.error())
            {
                package_info.sha256 = sha256.value();
            }

            if (auto elem = pkg["noarch"]; !elem.error())
            {
                // TODO: is the following right?
                if (auto val = elem.get_bool(); !val.error() && val.value())
                {
                    package_info.noarch = specs::NoArchType::No;
                }
                else if (auto noarch = elem.get_c_str(); !noarch.error())
                {
                    package_info.noarch = specs::NoArchType::No;
                }
            }

            if (auto license = pkg["license"].get_c_str(); !license.error())
            {
                package_info.license = license.value();
            }

            // TODO conda timestamp are not Unix timestamp.
            // Libsolv normalize them this way, we need to do the same here otherwise the current
            // package may get arbitrary priority.
            if (auto timestamp = pkg["timestamp"].get_uint64(); !timestamp.error())
            {
                const auto time = timestamp.value();
                // TODO: reuse it from `mamba/solver/libsolv/helpers.cpp`
                constexpr auto MAX_CONDA_TIMESTAMP = 253402300799ULL;
                package_info.timestamp = (time > MAX_CONDA_TIMESTAMP) ? (time / 1000) : time;
            }

            if (auto depends = pkg["depends"].get_array(); !depends.error())
            {
                for (auto elem : depends)
                {
                    if (auto dep = elem.get_c_str(); !dep.error())
                    {
                        package_info.dependencies.emplace_back(dep.value());
                    }
                }
            }

            if (auto constrains = pkg["constrains"].get_array(); !constrains.error())
            {
                for (auto elem : constrains)
                {
                    if (auto cons = elem.get_c_str(); !cons.error())
                    {
                        package_info.constrains.emplace_back(cons.value());
                    }
                }
            }

            if (auto obj = pkg["track_features"]; !obj.error())
            {
                if (auto track_features_arr = obj.get_array(); !track_features_arr.error())
                {
                    for (auto elem : track_features_arr)
                    {
                        if (auto feat = elem.get_string(); !feat.error())
                        {
                            package_info.track_features.emplace_back(feat.value());
                        }
                    }
                }
                else if (auto track_features_str = obj.get_string(); !track_features_str.error())
                {
                    const auto lsplit_track_features = [](std::string_view features)
                    {
                        constexpr auto is_sep = [](char c) -> bool
                        { return (c == ',') || util::is_space(c); };
                        auto [_, tail] = util::lstrip_if_parts(features, is_sep);
                        return util::lstrip_if_parts(tail, [&](char c) { return !is_sep(c); });
                    };

                    auto splits = lsplit_track_features(track_features_str.value());
                    while (!splits[0].empty())
                    {
                        package_info.track_features.emplace_back(splits[0]);
                        splits = lsplit_track_features(splits[1]);
                    }
                }
            }

            database.alloc_solvable(package_info);
            return true;
        }
    }

    Database::Database(specs::ChannelResolveParams channel_params)
        : name_pool(Mapping<::resolvo::NameId, ::resolvo::String>())
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
        auto parser = simdjson::dom::parser();
        const auto lock = LockFile(filename);
        const auto repodata = parser.load(filename);

        // An override for missing package subdir is found at the top level
        auto default_subdir = std::string();
        if (auto subdir = repodata.at_pointer("/info/subdir").get_string(); !subdir.error())
        {
            default_subdir = std::string(subdir.value_unsafe());
        }

        // Get `base_url` in case 'repodata_version': 2
        // cf. https://github.com/conda-incubator/ceps/blob/main/cep-15.md
        auto base_url = repo_url;
        if (auto repodata_version = repodata["repodata_version"].get_int64();
            !repodata_version.error())
        {
            if (repodata_version.value_unsafe() == 2)
            {
                if (auto url = repodata.at_pointer("/info/base_url").get_string(); !url.error())
                {
                    base_url = std::string(url.value_unsafe());
                }
            }
        }

        const auto parsed_url = specs::CondaURL::parse(base_url)
                                    .or_else([](specs::ParseError&& err) { throw std::move(err); })
                                    .value();

        auto signatures = std::optional<simdjson::dom::object>(std::nullopt);
        if (auto maybe_sigs = repodata["signatures"].get_object();
            !maybe_sigs.error() && verify_artifacts)
        {
            signatures = std::move(maybe_sigs).value();
        }

        auto added = util::flat_set<std::string_view>();
        if (auto pkgs = repodata["packages.conda"].get_object(); !pkgs.error())
        {
            for (auto [key, value] : pkgs.value())
            {
                parse_packageinfo_json(key, value, parsed_url, channel_id, *this);
            }
        }
        if (auto pkgs = repodata["packages"].get_object(); !pkgs.error())
        {
            for (auto [key, value] : pkgs.value())
            {
                parse_packageinfo_json(key, value, parsed_url, channel_id, *this);
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

    ::resolvo::String Database::display_solvable(::resolvo::SolvableId solvable)
    {
        const specs::PackageInfo& package_info = solvable_pool[solvable];
        return ::resolvo::String{ package_info.long_str() };
    }

    ::resolvo::String Database::display_solvable_name(::resolvo::SolvableId solvable)
    {
        const specs::PackageInfo& package_info = solvable_pool[solvable];
        return ::resolvo::String{ package_info.name };
    }

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

    ::resolvo::String Database::display_name(::resolvo::NameId name)
    {
        return name_pool[name];
    }

    ::resolvo::String Database::display_version_set(::resolvo::VersionSetId version_set)
    {
        const specs::MatchSpec match_spec = version_set_pool[version_set];
        return ::resolvo::String{ match_spec.to_string() };
    }

    ::resolvo::String Database::display_string(::resolvo::StringId string)
    {
        return string_pool[string];
    }

    ::resolvo::NameId Database::version_set_name(::resolvo::VersionSetId version_set_id)
    {
        const specs::MatchSpec match_spec = version_set_pool[version_set_id];
        return name_pool[::resolvo::String{ match_spec.name().to_string() }];
    }

    ::resolvo::NameId Database::solvable_name(::resolvo::SolvableId solvable_id)
    {
        const specs::PackageInfo& package_info = solvable_pool[solvable_id];
        return name_pool[::resolvo::String{ package_info.name }];
    }

    ::resolvo::Candidates Database::get_candidates(::resolvo::NameId package)
    {
        ::resolvo::Candidates candidates{};
        candidates.favored = nullptr;
        candidates.locked = nullptr;
        candidates.candidates = name_to_solvable[package];
        return candidates;
    }

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

    ::resolvo::VersionSetId Database::alloc_version_set(std::string_view raw_match_spec)
    {
        std::string raw_match_spec_str = std::string(raw_match_spec);
        // Replace all " v" with simply " " to work around the `v` prefix in some version strings
        // e.g. `mingw-w64-ucrt-x86_64-crt-git v12.0.0.r2.ggc561118da h707e725_0` in
        // `inform2w64-sysroot_win-64-v12.0.0.r2.ggc561118da-h707e725_0.conda`        while
        // (raw_match_spec_str.find(" v") != std::string::npos)
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
            std::vector<::resolvo::VersionSetId> version_sets;
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
        for (const std::string& op : { ">=", "<=", "==", ">", "<", "!=", "=", "==" })
        {
            const std::string& bad_op = op + " ";
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
        const ::resolvo::NameId name_id = name_pool.alloc(::resolvo::String{ package_info.name });
        name_to_solvable[name_id].push_back(id);

        return id;
    }

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

}  // namespace mamba::solver::resolvo
