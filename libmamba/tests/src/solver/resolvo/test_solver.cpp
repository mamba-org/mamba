// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <iostream>

#include <catch2/catch_all.hpp>
#include <resolvo/resolvo_dependency_provider.h>
#include <resolvo/resolvo_pool.h>

#include "mamba/specs/channel.hpp"
#include "mamba/specs/package_info.hpp"

#include "mambatests.hpp"


using namespace mamba;
using namespace mamba::specs;
using namespace mamba::solver;

using namespace resolvo;

template <>
struct std::hash<VersionSetId> {
    std::size_t operator()(const VersionSetId& id) const {
        return std::hash<uint32_t>{}(id.id);
    }
};

template <>
struct std::hash<SolvableId> {
    std::size_t operator()(const SolvableId& id) const {
        return std::hash<uint32_t>{}(id.id);
    }
};

template <>
struct std::hash<NameId> {
    std::size_t operator()(const NameId& id) const {
        return std::hash<uint32_t>{}(id.id);
    }
};

template <>
struct std::hash<StringId> {
    std::size_t operator()(const StringId& id) const {
        return std::hash<uint32_t>{}(id.id);
    }
};

struct PackageDatabase : public DependencyProvider {

    std::unordered_map<NameId, String> name_map;
    std::unordered_map<String, NameId> name_ids;

    std::unordered_map<StringId, String> string_ids;
    std::unordered_map<String, StringId> string_map;

    // PackageInfo are Solvable in resolvo's semantics
    std::unordered_map<PackageInfo, SolvableId> solvable_ids;
    std::unordered_map<SolvableId, PackageInfo> solvable_map;

    // MatchSpec are VersionSet in resolvo's semantics
    std::unordered_map<MatchSpec, VersionSetId> version_set_ids;
    std::unordered_map<VersionSetId, MatchSpec> version_set_map;

    /**
     * Allocates a new requirement and return the id of the requirement.
     */
    VersionSetId alloc_version_set(
        std::string_view raw_match_spec
    ) {
        const MatchSpec& match_spec = MatchSpec::parse(raw_match_spec).value();
        auto got = version_set_ids.find(match_spec);

        if (got != version_set_ids.end()) {
            return got->second;
        } else {
            auto id = VersionSetId{static_cast<uint32_t>(version_set_ids.size())};
            version_set_ids[match_spec] = id;
            version_set_map[id] = match_spec;
            return id;
        }
    }

    SolvableId alloc_solvable(
        PackageInfo package_info
    ) {
        auto got = solvable_ids.find(package_info);

        if (got != solvable_ids.end()) {
            return got->second;
        } else {
            auto id = SolvableId{static_cast<uint32_t>(solvable_ids.size())};
            solvable_ids[package_info] = id;
            solvable_map[id] = package_info;
            return id;
        }
    }

    /**
     * Returns a user-friendly string representation of the specified solvable.
     *
     * When formatting the solvable, it should it include both the name of
     * the package and any other identifying properties.
     */
    virtual String display_solvable(SolvableId solvable) {
        const PackageInfo& package_info = solvable_map[solvable];
        return String{package_info.long_str()};
    }

    /**
     * Returns a user-friendly string representation of the name of the
     * specified solvable.
     */
    virtual String display_solvable_name(SolvableId solvable) {
        const PackageInfo& package_info = solvable_map[solvable];
        return String{package_info.name};
    }

    /**
     * Returns a string representation of multiple solvables merged together.
     *
     * When formatting the solvables, both the name of the packages and any
     * other identifying properties should be included.
     */
    virtual String display_merged_solvables(Slice<SolvableId> solvable) {
        std::string result;
        for (auto& solvable_id : solvable) {
            // Append "solvable_id" and its name to the result
            result += std::to_string(solvable_id.id) + " " + solvable_map[solvable_id].name + "\n";
        }
        return String{result};
    }

    /**
     * Returns an object that can be used to display the given name in a
     * user-friendly way.
     */
    virtual String display_name(NameId name) {
        return name_map[name];
    }

    /**
     * Returns a user-friendly string representation of the specified version
     * set.
     *
     * The name of the package should *not* be included in the display. Where
     * appropriate, this information is added.
     */
    virtual String display_version_set(VersionSetId version_set) {
        const MatchSpec match_spec = version_set_map[version_set];
        return String{match_spec.str()};
    }

    /**
     * Returns the string representation of the specified string.
     */
    virtual String display_string(StringId string) {
        return string_ids[string];
    }

    /**
     * Returns the name of the package that the specified version set is
     * associated with.
     */
    virtual NameId version_set_name(VersionSetId version_set_id) {
        const MatchSpec match_spec = version_set_map[version_set_id];
        return name_ids[String{match_spec.name().str()}];
    }

    /**
     * Returns the name of the package for the given solvable.
     */
    virtual NameId solvable_name(SolvableId solvable_id) {
        const PackageInfo& package_info = solvable_map[solvable_id];
        return name_ids[String{package_info.name}];
    }

    /**
     * Obtains a list of solvables that should be considered when a package
     * with the given name is requested.
     */
    virtual Candidates get_candidates(NameId package) {
        Candidates candidates;
        // TODO: inefficient for now, O(n) which can be turned into O(1)
        for (auto& [solvable_id, package_info] : solvable_map) {
            if (package == solvable_name(solvable_id)) {
                candidates.candidates.push_back(solvable_id);
            }
        }
        return candidates;
    }

    /**
     * Sort the specified solvables based on which solvable to try first. The
     * solver will iteratively try to select the highest version. If a
     * conflict is found with the highest version the next version is
     * tried. This continues until a solution is found.
     */
    virtual void sort_candidates(Slice<SolvableId> solvables) {
        std::sort(solvables.begin(), solvables.end(), [&](const SolvableId& a, const SolvableId& b) {
            const PackageInfo& package_info_a = solvable_map[a];
            const PackageInfo& package_info_b = solvable_map[b];
            // TODO: Add some caching on the version parsing
            return Version::parse(package_info_a.version).value() < Version::parse(package_info_b.version).value();
        });
    }

    /**
     * Given a set of solvables, return the solvables that match the given
     * version set or if `inverse` is true, the solvables that do *not* match
     * the version set.
     */
    virtual Vector<SolvableId> filter_candidates(Slice<SolvableId> candidates,
                                                 VersionSetId version_set_id, bool inverse) {
        Vector<SolvableId> filtered;

        if(inverse) {
            for (auto& solvable_id : candidates)
            {
                const PackageInfo& package_info = solvable_map[solvable_id];
                const MatchSpec match_spec = version_set_map[version_set_id];

                // Is it an appropriate check? Or must another one be crafted?
                if (!match_spec.contains_except_channel(package_info))
                {
                    filtered.push_back(solvable_id);
                }
            }
        } else {
            for (auto& solvable_id : candidates)
            {
                const PackageInfo& package_info = solvable_map[solvable_id];
                const MatchSpec match_spec = version_set_map[version_set_id];

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
    virtual Dependencies get_dependencies(SolvableId solvable_id) {
        const PackageInfo& package_info = solvable_map[solvable_id];
        Dependencies dependencies;

        for (auto& dep : package_info.dependencies) {
            const MatchSpec match_spec = MatchSpec::parse(dep).value();
            dependencies.requirements.push_back(version_set_ids[match_spec]);
        }
        for (auto& constr : package_info.constrains) {
            const MatchSpec match_spec = MatchSpec::parse(constr).value();
            dependencies.constrains.push_back(version_set_ids[match_spec]);
        }

        return dependencies;
    }


};

TEST_CASE("solver::resolvo")
{
    using PackageInfo = PackageInfo;

    SECTION("Simple resolution problem") {

        // PackageDatabase database;

        // Create a PackageInfo for scikit-learn
        PackageInfo scikit_learn("scikit-learn", "1.5.0", "py310h981052a_1", 1);

        // Add the above dependencies to the PackageInfo object dependencies
        scikit_learn.dependencies.push_back("joblib >=1.2.0");
        scikit_learn.dependencies.push_back("numpy >=1.19,<3");
        scikit_learn.dependencies.push_back("scipy");
        scikit_learn.dependencies.push_back("threadpoolctl >=3.1.0");

        // Create a PackageInfo for numpy
        PackageInfo numpy("numpy", "1.21.0", "py310h4a8c4bd_0", 0);

        // Create a PackageInfo for scipy
        PackageInfo scipy("scipy", "1.7.0", "py310h4a8c4bd_0", 0);
        scipy.dependencies.push_back("numpy >=1.19,<3");

        // Create a PackageInfo for joblib
        PackageInfo joblib("joblib", "1.2.0", "py310h4a8c4bd_0", 0);

        // Create a PackageInfo for threadpoolctl
        PackageInfo threadpoolctl("threadpoolctl", "3.1.0", "py310h4a8c4bd_0", 0);



    }
}