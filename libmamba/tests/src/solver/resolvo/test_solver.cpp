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

struct PackageDatabase : public DependencyProvider {

    Pool<NameId, String> names;
    Pool<StringId, String> strings;

    std::unordered_map<PackageInfo, SolvableId> solvable_ids;
    std::unordered_map<MatchSpec, VersionSetId> version_set_ids;

    // Reverse mapping
    std::unordered_map<SolvableId, PackageInfo> solvable_map;
    std::unordered_map<VersionSetId, MatchSpec> version_set_map;

    /**
     * Allocates a new requirement and return the id of the requirement.
     */
    VersionSetId alloc_version_set_id(
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
        PackageInfo solvable
    ) {
        auto got = solvable_ids.find(solvable);

        if (got != solvable_ids.end()) {
            return got->second;
        } else {
            auto id = SolvableId{static_cast<uint32_t>(solvable_ids.size())};
            solvable_ids[solvable] = id;
            solvable_map[id] = solvable;
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
        return "";
    }

    /**
     * Returns a user-friendly string representation of the name of the
     * specified solvable.
     */
    virtual String display_solvable_name(SolvableId solvable) {
        return display_name(solvable_name(solvable));
    }

    /**
     * Returns a string representation of multiple solvables merged together.
     *
     * When formatting the solvables, both the name of the packages and any
     * other identifying properties should be included.
     */
    virtual String display_merged_solvables(Slice<SolvableId> solvable) {
        return "";
    }

    /**
     * Returns an object that can be used to display the given name in a
     * user-friendly way.
     */
    virtual String display_name(NameId name) {
        return "";
    }

    /**
     * Returns a user-friendly string representation of the specified version
     * set.
     *
     * The name of the package should *not* be included in the display. Where
     * appropriate, this information is added.
     */
    virtual String display_version_set(VersionSetId version_set) {
        return "";
    }

    /**
     * Returns the string representation of the specified string.
     */
    virtual String display_string(StringId string) {
        return "";
    }

    /**
     * Returns the name of the package that the specified version set is
     * associated with.
     */
    virtual NameId version_set_name(VersionSetId version_set_id) {
        return NameId{};
    }

    /**
     * Returns the name of the package for the given solvable.
     */
    virtual NameId solvable_name(SolvableId solvable_id) {

    }

    /**
     * Obtains a list of solvables that should be considered when a package
     * with the given name is requested.
     */
    virtual Candidates get_candidates(NameId package) {
        return {};
    }

    /**
     * Sort the specified solvables based on which solvable to try first. The
     * solver will iteratively try to select the highest version. If a
     * conflict is found with the highest version the next version is
     * tried. This continues until a solution is found.
     */
    virtual void sort_candidates(Slice<SolvableId> solvables) {

    }

    /**
     * Given a set of solvables, return the solvables that match the given
     * version set or if `inverse` is true, the solvables that do *not* match
     * the version set.
     */
    virtual Vector<SolvableId> filter_candidates(Slice<SolvableId> candidates,
                                                 VersionSetId version_set_id, bool inverse) {
        return {};
    }

    /**
     * Returns the dependencies for the specified solvable.
     */
    virtual Dependencies get_dependencies(SolvableId solvable) {
        const PackageInfo& package_info = solvables[solvable.id];
        Dependencies dependencies;
        for (auto& dep : package_info.dependencies) {
            dependencies.requirements.push_back(dep);
        }

        return {Vector(package.dependencies.begin(), package.dependencies.end()), Vector{package.constrains}};
    }


};

TEST_SUITE("solver::resolvo")
{
    using PackageInfo = PackageInfo;

    TEST_CASE("Simple resolution problem") {

        PackageDatabase database;

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