// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <iostream>

#include <catch2/catch_all.hpp>
#include <resolvo/resolvo.h>
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
        const MatchSpec match_spec = MatchSpec::parse(raw_match_spec).value();
        const std::string name = match_spec.name().str();
        auto got = version_set_ids.find(match_spec);

        if (got != version_set_ids.end()) {
            std::cout << "Found version set id for " << raw_match_spec << std::endl;
            return got->second;
        } else {
            std::cout << "Allocating version set id for " << raw_match_spec << std::endl;
            auto id = VersionSetId{static_cast<uint32_t>(version_set_ids.size())};
            std::cout << "Allocated version set id " << id.id << std::endl;
            version_set_ids[match_spec] = id;
            std::cout << "Added version set id to map" << std::endl;
            version_set_map[id] = match_spec;
            std::cout << "Added version set to map" << std::endl;

            auto got_name = name_ids.find(String{name});

            if (got_name == name_ids.end()) {
                auto name_id = NameId{static_cast<uint32_t>(name_map.size())};
                name_map[name_id] = name;
                name_ids[String{name}] = name_id;
                return id;
            }

            auto got_string = string_map.find(String{name});

            if (got_string == string_map.end()) {
                auto string_id = StringId{static_cast<uint32_t>(string_ids.size())};
                string_ids[string_id] = name;
                string_map[String{name}] = string_id;
                return id;
            }

            return id;
        }
    }

    SolvableId alloc_solvable(
        PackageInfo package_info
    ) {
        auto got = solvable_ids.find(package_info);

        const std::string name = package_info.name;

        if (got != solvable_ids.end()) {
            std::cout << "Found solvable id for " << name << std::endl;
            return got->second;
        } else {
            std::cout << "Allocating solvable id for " << name << std::endl;
            auto id = SolvableId{static_cast<uint32_t>(solvable_ids.size())};
            solvable_ids[package_info] = id;
            solvable_map[id] = package_info;

            for (auto& dep : package_info.dependencies) {
                alloc_version_set(dep);
            }
            for (auto& constr : package_info.constrains) {
                alloc_version_set(constr);
            }

            auto got_name = name_ids.find(String{name});

            if (got_name == name_ids.end()) {
                auto name_id = NameId{static_cast<uint32_t>(name_map.size())};
                name_map[name_id] = name;
                name_ids[String{name}] = name_id;
                return id;
            }

            auto got_string = string_map.find(String{name});

            if (got_string == string_map.end()) {
                auto string_id = StringId{static_cast<uint32_t>(string_ids.size())};
                string_ids[string_id] = name;
                string_map[String{name}] = string_id;
                return id;
            }

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
            result += std::to_string(solvable_id.id) + " " + solvable_map[solvable_id].build_string + "\n";
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
        std::cout << "Getting name id for version_set_id " << match_spec.name().str() << std::endl;
        return name_ids[String{match_spec.name().str()}];
    }

    /**
     * Returns the name of the package for the given solvable.
     */
    virtual NameId solvable_name(SolvableId solvable_id) {
        const PackageInfo& package_info = solvable_map[solvable_id];
        std::cout << "Getting name id for solvable " << package_info.name << std::endl;
        return name_ids[String{package_info.name}];
    }

    /**
     * Obtains a list of solvables that should be considered when a package
     * with the given name is requested.
     */
    virtual Candidates get_candidates(NameId package) {
        std::cout << "Getting candidates for " << name_map[package] << std::endl;
        Candidates candidates;
        // TODO: inefficient for now, O(n) which can be turned into O(1)
        for (auto& [solvable_id, package_info] : solvable_map) {
            std::cout << "  Checking " << package_info.name << " " << package_info.version << std::endl;
            if (package == solvable_name(solvable_id)) {
                std::cout << "  Adding candidate " << package_info.name << std::endl;
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
        std::cout << "Sorting candidates" << std::endl;
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
        std::cout << "Getting dependencies for " << package_info.name << std::endl;
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

        PackageDatabase database;

        // Create a PackageInfo for scikit-learn
        PackageInfo scikit_learn0("scikit-learn", "1.5.0", "py310h981052a_0", 0);
        PackageInfo scikit_learn("scikit-learn", "1.5.0", "py310h981052a_1", 1);

        // Add the above dependencies to the PackageInfo object dependencies
        scikit_learn.dependencies.push_back("joblib==1.2.0");
        // scikit_learn.dependencies.push_back("numpy >=1.19,<3");
        // scikit_learn.dependencies.push_back("scipy");
        // scikit_learn.dependencies.push_back("threadpoolctl >=3.1.0");

        // Create a PackageInfo for numpy
        PackageInfo numpy("numpy", "1.21.0", "py310h4a8c4bd_0", 0);

        // Create a PackageInfo for scipy
        PackageInfo scipy("scipy", "1.7.0", "py310h4a8c4bd_0", 0);
        // scipy.dependencies.push_back("numpy >=1.19,<3");

        // Create a PackageInfo for joblib
        PackageInfo joblib("joblib", "1.2.0", "py310h4a8c4bd_0", 0);

        // Create a PackageInfo for threadpoolctl
        PackageInfo threadpoolctl("threadpoolctl", "3.1.0", "py310h4a8c4bd_0", 0);

        // Allocate all the PackageInfo
        database.alloc_solvable(scikit_learn0);
        database.alloc_solvable(scikit_learn);
        // database.alloc_solvable(numpy);
        // database.alloc_solvable(scipy);
        database.alloc_solvable(joblib);
        // database.alloc_solvable(threadpoolctl);

        // Construct a problem to be solved by the solver
        resolvo::Vector<resolvo::VersionSetId> requirements = {
            database.alloc_version_set("scikit-learn==1.5.0"),
        };
        resolvo::Vector<resolvo::VersionSetId> constraints = {};

        // Solve the problem
        std::cout << "Solving the problem" << std::endl;
        resolvo::Vector<resolvo::SolvableId> result;
        String reason = resolvo::solve(database, requirements, constraints, result);

        std::cout << "Reason: " << reason << std::endl;

        // Display the result
        std::cout << "Result contains " << result.size() << " solvables" << std::endl;
        for (auto& solvable_id : result) {
            std::cout << database.display_solvable(solvable_id) << std::endl;
        }

    }
}