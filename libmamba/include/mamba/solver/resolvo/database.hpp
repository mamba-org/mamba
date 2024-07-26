#pragma once

// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <iostream>
#include <unordered_set>

#include <resolvo/resolvo.h>
#include <resolvo/resolvo_dependency_provider.h>
#include <resolvo/resolvo_pool.h>

#include "mamba/core/error_handling.hpp"
#include "mamba/fs/filesystem.hpp"
#include "mamba/solver/parameters.hpp"
#include "mamba/solver/repo_info.hpp"
#include "mamba/specs/channel.hpp"
#include "mamba/specs/match_spec.hpp"
#include "mamba/specs/package_info.hpp"
#include "mamba/util/string.hpp"


using namespace mamba;
using namespace mamba::specs;

using namespace resolvo;

template <>
struct std::hash<VersionSetId>
{
    std::size_t operator()(const VersionSetId& id) const
    {
        return std::hash<uint32_t>{}(id.id);
    }
};

template <>
struct std::hash<SolvableId>
{
    std::size_t operator()(const SolvableId& id) const
    {
        return std::hash<uint32_t>{}(id.id);
    }
};

template <>
struct std::hash<NameId>
{
    std::size_t operator()(const NameId& id) const
    {
        return std::hash<uint32_t>{}(id.id);
    }
};

template <>
struct std::hash<StringId>
{
    std::size_t operator()(const StringId& id) const
    {
        return std::hash<uint32_t>{}(id.id);
    }
};

// Create a template Pool class that maps a key to a set of values
template <typename ID, typename T>
struct Mapping
{
    Mapping() = default;
    ~Mapping() = default;

    /**
     * Adds the value to the Mapping and returns its associated id. If the
     * value is already in the Mapping, returns the id associated with it.
     */
    ID alloc(T value)
    {
        if (auto element = value_to_id.find(value); element != value_to_id.end())
        {
            return element->second;
        }
        auto id = ID{ static_cast<uint32_t>(id_to_value.size()) };
        id_to_value[id] = value;
        value_to_id[value] = id;
        return id;
    }

    /**
     * Returns the value associated with the given id.
     */
    T operator[](ID id)
    {
        return id_to_value[id];
    }

    /**
     * Returns the id associated with the given value.
     */
    ID operator[](T value)
    {
        return value_to_id[value];
    }

    // Iterator for the Mapping
    auto begin()
    {
        return id_to_value.begin();
    }

    auto end()
    {
        return id_to_value.end();
    }

    auto begin() const
    {
        return id_to_value.begin();
    }

    auto end() const
    {
        return id_to_value.end();
    }

    auto cbegin()
    {
        return id_to_value.cbegin();
    }

    auto cend()
    {
        return id_to_value.cend();
    }

    auto cbegin() const
    {
        return id_to_value.cbegin();
    }

    auto cend() const
    {
        return id_to_value.cend();
    }

    auto find(T value)
    {
        return value_to_id.find(value);
    }

    auto size() const
    {
        return id_to_value.size();
    }


private:

    std::unordered_map<T, ID> value_to_id;
    std::unordered_map<ID, T> id_to_value;
};

namespace mamba::solver::resolvo
{
    class PackageDatabase final : public DependencyProvider
    {
    public:

        using logger_type = std::function<void(LogLevel, std::string_view)>;

        explicit PackageDatabase(specs::ChannelResolveParams channel_params)
            : params(channel_params)
        {
        }

        PackageDatabase(const PackageDatabase&) = delete;
        PackageDatabase(PackageDatabase&&) = delete;

        ~PackageDatabase() = default;

        auto operator=(const PackageDatabase&) -> PackageDatabase& = delete;
        auto operator=(PackageDatabase&&) -> PackageDatabase& = delete;

        [[nodiscard]] auto channel_params() const -> const specs::ChannelResolveParams&
        {
            return params;
        }

        void set_logger(logger_type callback);

        auto add_repo_from_repodata_json(
            const fs::u8path& path,
            std::string_view url,
            const std::string& channel_id,
            PipAsPythonDependency add = PipAsPythonDependency::No,
            PackageTypes package_types = PackageTypes::CondaOrElseTarBz2,
            VerifyPackages verify_packages = VerifyPackages::No,
            RepodataParser parser = RepodataParser::Mamba
        ) -> expected_t<RepoInfo>;

        auto add_repo_from_native_serialization(
            const fs::u8path& path,
            const RepodataOrigin& expected,
            const std::string& channel_id,
            PipAsPythonDependency add = PipAsPythonDependency::No
        ) -> expected_t<RepoInfo>;

        template <typename Iter>
        auto add_repo_from_packages(
            Iter first_package,
            Iter last_package,
            std::string_view name = "",
            PipAsPythonDependency add = PipAsPythonDependency::No
        ) -> RepoInfo;

        template <typename Range>
        auto add_repo_from_packages(
            const Range& packages,
            std::string_view name = "",
            PipAsPythonDependency add = PipAsPythonDependency::No
        ) -> RepoInfo;

        auto
        native_serialize_repo(const RepoInfo& repo, const fs::u8path& path, const RepodataOrigin& metadata)
            -> expected_t<RepoInfo>;

        [[nodiscard]] auto installed_repo() const -> std::optional<RepoInfo>;

        void set_installed_repo(RepoInfo repo);

        void set_repo_priority(RepoInfo repo, Priorities priorities);

        void remove_repo(RepoInfo repo);

        [[nodiscard]] auto repo_count() const -> std::size_t;

        [[nodiscard]] auto package_count() const -> std::size_t;

        template <typename Func>
        void for_each_package_in_repo(RepoInfo repo, Func&&) const;

        template <typename Func>
        void for_each_package_matching(const specs::MatchSpec& ms, Func&&);

        template <typename Func>
        void for_each_package_depending_on(const specs::MatchSpec& ms, Func&&);

        /**
         * Allocates a new requirement and return the id of the requirement.
         */
        VersionSetId alloc_version_set(std::string_view raw_match_spec)
        {
            std::string raw_match_spec_str = std::string(raw_match_spec);
            // Replace all " v" with simply " " to work around the `v` prefix in some version
            // strings e.g. `mingw-w64-ucrt-x86_64-crt-git v12.0.0.r2.ggc561118da h707e725_0` in
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
                    raw_match_spec_str = raw_match_spec_str.substr(
                        0,
                        raw_match_spec_str.find(specifier)
                    );
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
                return VersionSetId{ 0 };
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
                std::vector<VersionSetId> version_sets;
                for (const std::string& ms : match_specs)
                {
                    alloc_version_set(ms);
                }
                // Placeholder return value
                return VersionSetId{ 0 };
            }

            const MatchSpec match_spec = MatchSpec::parse(raw_match_spec_str).value();
            // Add the version set to the version set pool
            auto id = version_set_pool.alloc(match_spec);

            // Add name to the Name and String pools
            const std::string name = match_spec.name().str();
            name_pool.alloc(String{ name });
            string_pool.alloc(String{ name });

            // Add the MatchSpec's string representation to the Name and String pools
            const std::string match_spec_str = match_spec.str();
            name_pool.alloc(String{ match_spec_str });
            string_pool.alloc(String{ match_spec_str });
            return id;
        }

        SolvableId alloc_solvable(PackageInfo package_info)
        {
            // Add the solvable to the solvable pool
            auto id = solvable_pool.alloc(package_info);

            // Add name to the Name and String pools
            const std::string name = package_info.name;
            name_pool.alloc(String{ name });
            string_pool.alloc(String{ name });

            // Add the long string representation of the package to the Name and String pools
            const std::string long_str = package_info.long_str();
            name_pool.alloc(String{ long_str });
            string_pool.alloc(String{ long_str });

            for (auto& dep : package_info.dependencies)
            {
                alloc_version_set(dep);
            }
            for (auto& constr : package_info.constrains)
            {
                alloc_version_set(constr);
            }

            // Add the solvable to the name_to_solvable map
            const NameId name_id = name_pool.alloc(String{ package_info.name });
            name_to_solvable[name_id].push_back(id);

            return id;
        }

        /**
         * Returns a user-friendly string representation of the specified solvable.
         *
         * When formatting the solvable, it should it include both the name of
         * the package and any other identifying properties.
         */
        String display_solvable(SolvableId solvable) override
        {
            const PackageInfo& package_info = solvable_pool[solvable];
            return String{ package_info.long_str() };
        }

        /**
         * Returns a user-friendly string representation of the name of the
         * specified solvable.
         */
        String display_solvable_name(SolvableId solvable) override
        {
            const PackageInfo& package_info = solvable_pool[solvable];
            return String{ package_info.name };
        }

        /**
         * Returns a string representation of multiple solvables merged together.
         *
         * When formatting the solvables, both the name of the packages and any
         * other identifying properties should be included.
         */
        String display_merged_solvables(Slice<SolvableId> solvable) override
        {
            std::string result;
            for (auto& solvable_id : solvable)
            {
                result += solvable_pool[solvable_id].long_str();
            }
            return String{ result };
        }

        /**
         * Returns an object that can be used to display the given name in a
         * user-friendly way.
         */
        String display_name(NameId name) override
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
        String display_version_set(VersionSetId version_set) override
        {
            const MatchSpec match_spec = version_set_pool[version_set];
            return String{ match_spec.str() };
        }

        /**
         * Returns the string representation of the specified string.
         */
        String display_string(StringId string) override
        {
            return string_pool[string];
        }

        /**
         * Returns the name of the package that the specified version set is
         * associated with.
         */
        NameId version_set_name(VersionSetId version_set_id) override
        {
            const MatchSpec match_spec = version_set_pool[version_set_id];
            return name_pool[String{ match_spec.name().str() }];
        }

        /**
         * Returns the name of the package for the given solvable.
         */
        NameId solvable_name(SolvableId solvable_id) override
        {
            const PackageInfo& package_info = solvable_pool[solvable_id];
            return name_pool[String{ package_info.name }];
        }

        /**
         * Obtains a list of solvables that should be considered when a package
         * with the given name is requested.
         */
        Candidates get_candidates(NameId package) override
        {
            Candidates candidates{};
            candidates.favored = nullptr;
            candidates.locked = nullptr;
            candidates.candidates = name_to_solvable[package];
            return candidates;
        }

        std::pair<Version, size_t> find_highest_version(VersionSetId version_set_id)
        {
            // If the version set has already been computed, return it.
            if (version_set_to_max_version_and_track_features_numbers.find(version_set_id)
                != version_set_to_max_version_and_track_features_numbers.end())
            {
                return version_set_to_max_version_and_track_features_numbers[version_set_id];
            }

            const MatchSpec match_spec = version_set_pool[version_set_id];

            const std::string& name = match_spec.name().str();

            auto name_id = name_pool.alloc(String{ name });

            auto solvables = name_to_solvable[name_id];

            auto filtered = filter_candidates(solvables, version_set_id, false);

            Version max_version = Version();
            size_t max_version_n_track_features = 0;

            for (auto& solvable_id : filtered)
            {
                const PackageInfo& package_info = solvable_pool[solvable_id];
                const auto version = Version::parse(package_info.version).value();
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
        void sort_candidates(Slice<SolvableId> solvables) override
        {
            std::sort(
                solvables.begin(),
                solvables.end(),
                [&](const SolvableId& a, const SolvableId& b)
                {
                    const PackageInfo& package_info_a = solvable_pool[a];
                    const PackageInfo& package_info_b = solvable_pool[b];

                    // If track features are present, prefer the solvable having the least of them.
                    if (package_info_a.track_features.size() != package_info_b.track_features.size())
                    {
                        return package_info_a.track_features.size()
                               < package_info_b.track_features.size();
                    }

                    const auto a_version = Version::parse(package_info_a.version).value();
                    const auto b_version = Version::parse(package_info_b.version).value();

                    if (a_version != b_version)
                    {
                        return a_version > b_version;
                    }

                    if (package_info_a.build_number != package_info_b.build_number)
                    {
                        return package_info_a.build_number > package_info_b.build_number;
                    }

                    // Compare the dependencies of the variants.
                    std::unordered_map<NameId, VersionSetId> a_deps;
                    std::unordered_map<NameId, VersionSetId> b_deps;
                    for (auto dep_a : package_info_a.dependencies)
                    {
                        // TODO: have a VersionID to NameID mapping instead
                        MatchSpec ms = MatchSpec::parse(dep_a).value();
                        const std::string& name = ms.name().str();
                        auto name_id = name_pool.alloc(String{ name });

                        a_deps[name_id] = version_set_pool[ms];
                    }
                    for (auto dep_b : package_info_b.dependencies)
                    {
                        // TODO: have a VersionID to NameID mapping instead
                        MatchSpec ms = MatchSpec::parse(dep_b).value();
                        const std::string& name = ms.name().str();
                        auto name_id = name_pool.alloc(String{ name });

                        b_deps[name_id] = version_set_pool[ms];
                    }

                    auto ordering_score = 0;
                    for (auto [name_id, version_set_id] : a_deps)
                    {
                        if (b_deps.find(name_id) != b_deps.end())
                        {
                            auto [a_dep_version, a_n_track_features] = find_highest_version(
                                version_set_id
                            );
                            auto [b_dep_version, b_n_track_features] = find_highest_version(
                                b_deps[name_id]
                            );

                            // Favor the solvable with higher versions of their dependencies
                            if (a_dep_version != b_dep_version)
                            {
                                ordering_score += a_dep_version > b_dep_version ? 1 : -1;
                            }

                            // Highly penalize the solvable if a dependencies has more track
                            // features
                            if (a_n_track_features != b_n_track_features)
                            {
                                ordering_score += a_n_track_features > b_n_track_features ? -100
                                                                                          : 100;
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
        Vector<SolvableId>
        filter_candidates(Slice<SolvableId> candidates, VersionSetId version_set_id, bool inverse) override
        {
            MatchSpec match_spec = version_set_pool[version_set_id];
            Vector<SolvableId> filtered;

            if (inverse)
            {
                for (auto& solvable_id : candidates)
                {
                    const PackageInfo& package_info = solvable_pool[solvable_id];

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
                    const PackageInfo& package_info = solvable_pool[solvable_id];

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
        Dependencies get_dependencies(SolvableId solvable_id) override
        {
            const PackageInfo& package_info = solvable_pool[solvable_id];

            Dependencies dependencies;

            // TODO: do this in O(1)
            for (auto& dep : package_info.dependencies)
            {
                // std::cout << "Parsing dep " << dep << std::endl;
                const MatchSpec match_spec = MatchSpec::parse(dep).value();
                dependencies.requirements.push_back(version_set_pool[match_spec]);
            }
            for (auto& constr : package_info.constrains)
            {
                // std::cout << "Parsing constr " << constr << std::endl;
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
                const MatchSpec match_spec = MatchSpec::parse(constr2).value();
                dependencies.constrains.push_back(version_set_pool[match_spec]);
            }

            return dependencies;
        }

        const PackageInfo& get_solvable(SolvableId solvable_id)
        {
            return solvable_pool[solvable_id];
        }

    private:

        const ChannelResolveParams& params;

        ::Mapping<NameId, String> name_pool;
        ::Mapping<StringId, String> string_pool;

        // MatchSpec are VersionSet in resolvo's semantics
        ::Mapping<VersionSetId, MatchSpec> version_set_pool;

        // PackageInfo are Solvable in resolvo's semantics
        ::Mapping<SolvableId, PackageInfo> solvable_pool;

        // PackageName to Vector<SolvableId>
        std::unordered_map<NameId, Vector<SolvableId>> name_to_solvable;

        // VersionSetId to max version
        // TODO use `SolvableId` instead of `std::pair<Version, size_t>`?
        std::unordered_map<VersionSetId, std::pair<Version, size_t>>
            version_set_to_max_version_and_track_features_numbers;
    };

}
