// Copyright (c) 2026, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <iostream>
#include <unordered_set>

#include <catch2/catch_all.hpp>
#include <resolvo/resolvo.h>
#include <resolvo/resolvo_dependency_provider.h>
#include <resolvo/resolvo_pool.h>
#include <simdjson.h>

#include "mamba/api/install.hpp"  // for parsing YAML specs
#include "mamba/core/util.hpp"    // for LockFile
#include "mamba/specs/channel.hpp"
#include "mamba/specs/package_info.hpp"

// TODO: move PackageTypes and MAX_CONDA_TIMESTAMP to a common place
#include "mamba/core/virtual_packages.hpp"
#include "mamba/solver/libsolv/database.hpp"
#include "mamba/solver/libsolv/parameters.hpp"  // for PackageTypes
#include "mamba/solver/libsolv/solver.hpp"
#include "mamba/solver/resolvo/database.hpp"
#include "mamba/solver/resolvo/solver.hpp"

#include "mambatests.hpp"


using namespace mamba;
using namespace mamba::specs;
using namespace mamba::solver;

using namespace resolvo;

// from `src/test_solver.cpp`
auto
find_actions_with_name(const Solution& solution, std::string_view name)
    -> std::vector<Solution::Action>;
auto
find_actions(const Solution& solution) -> std::vector<Solution::Action>;
auto
extract_package_to_install(const Solution& solution) -> std::vector<specs::PackageInfo>;

// wget https://conda.anaconda.org/conda-forge/linux-64/repodata.json
// wget https://conda.anaconda.org/conda-forge/noarch/repodata.json

mamba::solver::libsolv::Database
create_libsolv_db()
{
    auto libsolv_db = mamba::solver::libsolv::Database(
        {
            /* .platforms= */ { "linux-64", "noarch" },
            /* .channel_alias= */ specs::CondaURL::parse("https://conda.anaconda.org/").value(),
        }
    );


    const auto repo_linux = libsolv_db.add_repo_from_repodata_json(
        "/tmp/linux-64/repodata.json",
        "https://conda.anaconda.org/conda-forge/linux-64",
        "conda-forge",
        libsolv::PipAsPythonDependency::No
    );

    const auto repo_noarch = libsolv_db.add_repo_from_repodata_json(
        "/tmp/noarch/repodata.json",
        "https://conda.anaconda.org/conda-forge/noarch",
        "conda-forge",
        libsolv::PipAsPythonDependency::Yes
    );

    // Not adding Pip dependency since it might needlessly make the installed/active environment
    // broken if pip is not already installed (debatable).

    auto repo = libsolv_db.add_repo_from_packages(
        get_virtual_packages("linux-64"),
        "virtual",
        solver::libsolv::PipAsPythonDependency::No
    );
    libsolv_db.set_installed_repo(repo);

    return libsolv_db;
};

mamba::solver::libsolv::Database libsolv_db = create_libsolv_db();

std::vector<PackageInfo>
libsolv_resolve(const std::vector<std::string>& specs)
{
    // libsolv's specification and resolution

    Request::job_list jobs;

    std::transform(
        specs.begin(),
        specs.end(),
        std::back_inserter(jobs),
        [](const std::string& spec) { return Request::Install{ MatchSpec::parse(spec).value() }; }
    );

    const auto request = Request{
        /* .flags= */ {},
        /* .jobs= */ jobs,
    };

    std::cout << "Start with libsolv" << std::endl;
    auto tick_libsolv = std::chrono::steady_clock::now();
    const auto outcome = libsolv::Solver().solve(libsolv_db, request);
    auto tack_libsolv = std::chrono::steady_clock::now();
    std::cout << "End with libsolv" << std::endl;
    std::cout
        << "Elapsed time: "
        << std::chrono::duration_cast<std::chrono::milliseconds>(tack_libsolv - tick_libsolv).count()
        << "ms" << std::endl;

    REQUIRE(outcome.has_value());
    if (std::holds_alternative<Solution>(outcome.value()))
    {
        const auto& solution = std::get<Solution>(outcome.value());

        std::vector<PackageInfo> libsolv_resolution = extract_package_to_install(solution);
        std::sort(
            libsolv_resolution.begin(),
            libsolv_resolution.end(),
            [&](const PackageInfo& a, const PackageInfo& b) { return a.name < b.name; }
        );
        return libsolv_resolution;
    }
    return {};
}

mamba::solver::resolvo::Database
create_resolvo_libmamba_db()
{
    auto resolvo_db = mamba::solver::resolvo::Database(
        {
            /* .platforms= */ { "linux-64", "noarch" },
            /* .channel_alias= */ specs::CondaURL::parse("https://conda.anaconda.org/").value(),
        }
    );

    resolvo_db.add_repo_from_repodata_json(
        "/tmp/linux-64/repodata.json",
        "https://conda.anaconda.org/conda-forge/linux-64",
        "conda-forge",
        false
    );
    resolvo_db.add_repo_from_repodata_json(
        "/tmp/noarch/repodata.json",
        "https://conda.anaconda.org/conda-forge/noarch",
        "conda-forge",
        false
    );
    resolvo_db.add_repo_from_packages(get_virtual_packages("linux-64"), "virtual", false);
    return resolvo_db;
}

mamba::solver::resolvo::Database resolvo_db = create_resolvo_libmamba_db();

std::vector<PackageInfo>
resolvo_libmamba_resolve(const std::vector<std::string>& specs)
{
    auto resolvo_db = create_resolvo_libmamba_db();

    Request::job_list jobs;
    std::transform(
        specs.begin(),
        specs.end(),
        std::back_inserter(jobs),
        [](const std::string& spec) { return Request::Install{ MatchSpec::parse(spec).value() }; }
    );

    const auto request = Request{
        /* .flags= */ {},
        /* .jobs= */ jobs,
    };

    const auto outcome = mamba::solver::resolvo::Solver().solve(resolvo_db, request);
    REQUIRE(outcome.has_value());
    const auto& solution = std::get<Solution>(outcome.value());
    auto resolution = extract_package_to_install(solution);
    std::sort(
        resolution.begin(),
        resolution.end(),
        [&](const PackageInfo& a, const PackageInfo& b) { return a.name < b.name; }
    );
    return resolution;
}

TEST_CASE("solver::resolvo")
{
    using namespace specs::match_spec_literals;

    using PackageInfo = PackageInfo;

    SECTION("Addition of PackageInfo to PackageDatabase")
    {
        mamba::solver::resolvo::Database database(
            {
                /* .platforms= */ { "linux-64", "noarch" },
                /* .channel_alias= */ specs::CondaURL::parse("https://conda.anaconda.org/").value(),
            }
        );

        PackageInfo scikit_learn("scikit-learn", "1.5.0", "py310h981052a_0", 0);
        scikit_learn.dependencies.emplace_back("numpy >=1.20.0,<2.0a0");
        scikit_learn.dependencies.emplace_back("scipy >=1.6.0,<2.0a0");
        scikit_learn.dependencies.emplace_back("joblib >=1.0.1,<2.0a0");
        scikit_learn.dependencies.emplace_back("threadpoolctl >=2.1.0,<3.0a0");

        auto solvable = database.alloc_solvable(scikit_learn);

        REQUIRE(solvable.id == 0);
        REQUIRE(database.solvable_pool[solvable].name == "scikit-learn");
        REQUIRE(database.solvable_pool[solvable].version == "1.5.0");
        REQUIRE(database.solvable_pool[solvable].build_string == "py310h981052a_0");
        REQUIRE(database.solvable_pool[solvable].build_number == 0);

        auto deps = database.get_dependencies(solvable);
        REQUIRE(deps.requirements.size() == 4);
        REQUIRE(deps.constrains.size() == 0);

        REQUIRE(
            database.version_set_pool[deps.requirements[0]].to_string()
            == "numpy[version=\">=1.20.0,<2.0a0\"]"
        );
        REQUIRE(
            database.version_set_pool[deps.requirements[1]].to_string()
            == "scipy[version=\">=1.6.0,<2.0a0\"]"
        );
        REQUIRE(
            database.version_set_pool[deps.requirements[2]].to_string()
            == "joblib[version=\">=1.0.1,<2.0a0\"]"
        );
        REQUIRE(
            database.version_set_pool[deps.requirements[3]].to_string()
            == "threadpoolctl[version=\">=2.1.0,<3.0a0\"]"
        );

        REQUIRE(database.name_pool.find(String{ "scikit-learn" }) != database.name_pool.end_ids());
        REQUIRE(database.name_pool.find(String{ "numpy" }) != database.name_pool.end_ids());
        REQUIRE(database.name_pool.find(String{ "scipy" }) != database.name_pool.end_ids());
        REQUIRE(database.name_pool.find(String{ "joblib" }) != database.name_pool.end_ids());
        REQUIRE(database.name_pool.find(String{ "threadpoolctl" }) != database.name_pool.end_ids());

        REQUIRE(database.string_pool.find(String{ "scikit-learn" }) != database.string_pool.end_ids());
        REQUIRE(database.string_pool.find(String{ "numpy" }) != database.string_pool.end_ids());
        REQUIRE(database.string_pool.find(String{ "scipy" }) != database.string_pool.end_ids());
        REQUIRE(database.string_pool.find(String{ "joblib" }) != database.string_pool.end_ids());
        REQUIRE(database.string_pool.find(String{ "threadpoolctl" }) != database.string_pool.end_ids());
    }

    SECTION("Filter solvables")
    {
        mamba::solver::resolvo::Database database(
            {
                /* .platforms= */ { "linux-64", "noarch" },
                /* .channel_alias= */ specs::CondaURL::parse("https://conda.anaconda.org/").value(),
            }
        );

        PackageInfo skl0("scikit-learn", "1.4.0", "py310h981052a_0", 0);
        auto sol0 = database.alloc_solvable(skl0);

        PackageInfo skl1("scikit-learn", "1.5.0", "py310h981052a_1", 1);
        auto sol1 = database.alloc_solvable(skl1);

        PackageInfo skl2("scikit-learn", "1.5.1", "py310h981052a_0", 0);
        auto sol2 = database.alloc_solvable(skl2);

        PackageInfo skl3("scikit-learn", "1.5.1", "py310h981052a_2", 2);
        auto sol3 = database.alloc_solvable(skl3);

        auto solvables = Vector<SolvableId>{ sol0, sol1, sol2, sol3 };

        // Filter on scikit-learn
        auto all = database.filter_candidates(
            solvables,
            database.alloc_version_set("scikit-learn"),
            false
        );
        REQUIRE(all.size() == 4);
        REQUIRE(all[0] == sol0);
        REQUIRE(all[1] == sol1);
        REQUIRE(all[2] == sol2);
        REQUIRE(all[3] == sol3);

        // Inverse filter on scikit-learn
        auto none = database.filter_candidates(
            solvables,
            database.alloc_version_set("scikit-learn"),
            true
        );
        REQUIRE(none.size() == 0);

        // Filter on scikit-learn==1.5.1
        auto one = database.filter_candidates(
            solvables,
            database.alloc_version_set("scikit-learn==1.5.1"),
            false
        );
        REQUIRE(one.size() == 2);
        REQUIRE(one[0] == sol2);
        REQUIRE(one[1] == sol3);

        // Inverse filter on scikit-learn==1.5.1
        auto three = database.filter_candidates(
            solvables,
            database.alloc_version_set("scikit-learn==1.5.1"),
            true
        );
        REQUIRE(three.size() == 2);
        REQUIRE(three[0] == sol0);
        REQUIRE(three[1] == sol1);

        // Filter on scikit-learn<1.5.1
        auto two = database.filter_candidates(
            solvables,
            database.alloc_version_set("scikit-learn<1.5.1"),
            false
        );
        REQUIRE(two.size() == 2);
        REQUIRE(two[0] == sol0);
        REQUIRE(two[1] == sol1);

        // Filter on build number 0
        auto build = database.filter_candidates(
            solvables,
            database.alloc_version_set("scikit-learn[build_number==0]"),
            false
        );
        REQUIRE(build.size() == 2);
        REQUIRE(build[0] == sol0);
        REQUIRE(build[1] == sol2);

        // Filter on build number 2
        auto build_bis = database.filter_candidates(
            solvables,
            database.alloc_version_set("scikit-learn[build_number==2]"),
            false
        );
        REQUIRE(build_bis.size() == 1);
        REQUIRE(build_bis[0] == sol3);

        // Filter on build number 3
        auto build_ter = database.filter_candidates(
            solvables,
            database.alloc_version_set("scikit-learn[build_number==3]"),
            false
        );
        REQUIRE(build_ter.size() == 0);
    }

    SECTION("Sort solvables increasing order")
    {
        mamba::solver::resolvo::Database database(
            {
                /* .platforms= */ { "linux-64", "noarch" },
                /* .channel_alias= */ specs::CondaURL::parse("https://conda.anaconda.org/").value(),
            }
        );

        PackageInfo skl0("scikit-learn", "1.5.2", "py310h981052a_0", 0);
        auto sol0 = database.alloc_solvable(skl0);

        PackageInfo skl1("scikit-learn", "1.5.0", "py310h981052a_1", 1);
        auto sol1 = database.alloc_solvable(skl1);

        PackageInfo skl2("scikit-learn", "1.5.1", "py310h981052a_2", 2);
        auto sol2 = database.alloc_solvable(skl2);

        PackageInfo skl3("scikit-learn", "1.5.0", "py310h981052a_2", 2);
        auto sol3 = database.alloc_solvable(skl3);

        PackageInfo skl4("scikit-learn", "1.5.1", "py310h981052a_1", 1);
        auto sol4 = database.alloc_solvable(skl4);

        Vector<SolvableId> solvables = { sol0, sol1, sol2, sol3, sol4 };

        database.sort_candidates(solvables);

        REQUIRE(solvables[0] == sol0);
        REQUIRE(solvables[1] == sol2);
        REQUIRE(solvables[2] == sol4);
        REQUIRE(solvables[3] == sol3);
        REQUIRE(solvables[4] == sol1);
    }

    SECTION("Sort solvables (build number only)")
    {
        mamba::solver::resolvo::Database database(
            {
                /* .platforms= */ { "linux-64", "noarch" },
                /* .channel_alias= */ specs::CondaURL::parse("https://conda.anaconda.org/").value(),
            }
        );

        PackageInfo skl0("scikit-learn", "1.5.0", "py310h981052a_0", 0);
        auto sol0 = database.alloc_solvable(skl0);

        PackageInfo skl1("scikit-learn", "1.5.0", "py310h981052a_3", 3);
        auto sol1 = database.alloc_solvable(skl1);

        PackageInfo skl2("scikit-learn", "1.5.0", "py310h981052a_2", 2);
        auto sol2 = database.alloc_solvable(skl2);

        PackageInfo skl3("scikit-learn", "1.5.0", "py310h981052a_1", 1);
        auto sol3 = database.alloc_solvable(skl3);

        PackageInfo skl4("scikit-learn", "1.5.0", "py310h981052a_4", 4);
        auto sol4 = database.alloc_solvable(skl4);

        PackageInfo skl5("scikit-learn", "1.5.0", "py310h981052a_5", 5);
        skl5.timestamp = 1337;
        auto sol5 = database.alloc_solvable(skl5);

        PackageInfo skl6("scikit-learn", "1.5.0", "py310h981052a_5", 5);
        skl6.timestamp = 42;
        auto sol6 = database.alloc_solvable(skl6);

        PackageInfo skl7("scikit-learn", "1.5.0", "py310h981052a_5", 5);
        skl7.timestamp = 2000;
        auto sol7 = database.alloc_solvable(skl7);

        Vector<SolvableId> solvables = { sol0, sol1, sol2, sol3, sol4, sol5, sol6, sol7 };

        database.sort_candidates(solvables);

        REQUIRE(solvables[0] == sol7);
        REQUIRE(solvables[1] == sol5);
        REQUIRE(solvables[2] == sol6);
        REQUIRE(solvables[3] == sol4);
        REQUIRE(solvables[4] == sol1);
        REQUIRE(solvables[5] == sol2);
        REQUIRE(solvables[6] == sol3);
        REQUIRE(solvables[7] == sol0);
    }

    SECTION("Trivial problem")
    {
        mamba::solver::resolvo::Database database(
            {
                /* .platforms= */ { "linux-64", "noarch" },
                /* .channel_alias= */ specs::CondaURL::parse("https://conda.anaconda.org/").value(),
            }
        );
        // NOTE: the problem can only be solved when two `Solvable` are added to the
        // `PackageDatabase`
        PackageInfo scikit_learn("scikit-learn", "1.5.0", "py310h981052a_0", 0);
        database.alloc_solvable(scikit_learn);

        resolvo::Vector<resolvo::VersionSetId> requirements = {
            database.alloc_version_set("scikit-learn==1.5.0"),
        };
        resolvo::Vector<resolvo::VersionSetId> constraints = {};

        resolvo::Vector<resolvo::SolvableId> result;
        String reason = resolvo::solve(database, requirements, constraints, result);

        REQUIRE(reason == "");
        REQUIRE(result.size() == 1);
        REQUIRE(database.solvable_pool[result[0]] == scikit_learn);
    }

    SECTION("Parse linux-64/repodata.json")
    {
        mamba::solver::resolvo::Database database(
            {
                /* .platforms= */ { "linux-64", "noarch" },
                /* .channel_alias= */ specs::CondaURL::parse("https://conda.anaconda.org/").value(),
            }
        );

        database.add_repo_from_repodata_json(
            "/tmp/linux-64/repodata.json",
            "https://conda.anaconda.org/conda-forge/linux-64",
            "conda-forge",
            false
        );

        std::cout << "Number of solvables: " << database.solvable_pool.size() << std::endl;
    }

    SECTION("Parse noarch/repodata.json")
    {
        mamba::solver::resolvo::Database database(
            {
                /* .platforms= */ { "linux-64", "noarch" },
                /* .channel_alias= */ specs::CondaURL::parse("https://conda.anaconda.org/").value(),
            }
        );

        database.add_repo_from_repodata_json(
            "/tmp/noarch/repodata.json",
            "https://conda.anaconda.org/conda-forge/noarch",
            "conda-forge",
            false
        );

        std::cout << "Number of solvables: " << database.solvable_pool.size() << std::endl;
    }
}

TEST_CASE("Test consistency with libsolv (environment creation)")
{
    using namespace specs::match_spec_literals;

    using PackageInfo = PackageInfo;

    SECTION("numpy")
    {
        const std::vector<std::string> specs_to_install = { "numpy" };
        const auto libsolv_resolution = libsolv_resolve(specs_to_install);
        const auto resolvo_resolution = resolvo_libmamba_resolve(specs_to_install);

        REQUIRE(resolvo_resolution.size() == libsolv_resolution.size());
        for (std::size_t i = 0; i < libsolv_resolution.size(); ++i)
        {
            REQUIRE(resolvo_resolution[i].name == libsolv_resolution[i].name);
            REQUIRE(resolvo_resolution[i].version == libsolv_resolution[i].version);
            REQUIRE(resolvo_resolution[i].build_string == libsolv_resolution[i].build_string);
        }
    }

    SECTION("scikit-learn")
    {
        const std::vector<std::string> specs_to_install = { "scikit-learn" };
        const auto libsolv_resolution = libsolv_resolve(specs_to_install);
        const auto resolvo_resolution = resolvo_libmamba_resolve(specs_to_install);

        REQUIRE(resolvo_resolution.size() == libsolv_resolution.size());
        for (std::size_t i = 0; i < libsolv_resolution.size(); ++i)
        {
            REQUIRE(resolvo_resolution[i].name == libsolv_resolution[i].name);
            REQUIRE(resolvo_resolution[i].version == libsolv_resolution[i].version);
            REQUIRE(resolvo_resolution[i].build_string == libsolv_resolution[i].build_string);
        }
    }

    SECTION("scikit-learn explicit")
    {
        // Note: currently, pip is added to the environment when python is added
        // we add it here to make resolvo's results consistent with libsolv's.
        std::vector<std::string> specs_to_install = { "scikit-learn==1.6.1=py313h8ef605b_0", "pip" };

        std::vector<PackageInfo> known_resolution = {
            PackageInfo("_libgcc_mutex", "0.1", "conda_forge", 0),
            PackageInfo("_openmp_mutex", "4.5", "2_gnu", 0),
            PackageInfo("bzip2", "1.0.8", "h4bc722e_7", 0),
            PackageInfo("ca-certificates", "2025.1.31", "hbcca054_0", 0),
            PackageInfo("joblib", "1.4.2", "pyhd8ed1ab_1", 0),
            PackageInfo("ld_impl_linux-64", "2.43", "h712a8e2_2", 0),
            PackageInfo("libblas", "3.9.0", "28_h59b9bed_openblas", 0),
            PackageInfo("libcblas", "3.9.0", "28_he106b2a_openblas", 0),
            PackageInfo("libexpat", "2.6.4", "h5888daf_0", 0),
            PackageInfo("libffi", "3.4.6", "h2dba641_0", 0),
            PackageInfo("libgcc", "14.2.0", "h77fa898_1", 0),
            PackageInfo("libgcc-ng", "14.2.0", "h69a702a_1", 0),
            PackageInfo("libgfortran", "14.2.0", "h69a702a_1", 0),
            PackageInfo("libgfortran5", "14.2.0", "hd5240d6_1", 0),
            PackageInfo("libgomp", "14.2.0", "h77fa898_1", 0),
            PackageInfo("liblapack", "3.9.0", "28_h7ac8fdf_openblas", 0),
            PackageInfo("liblzma", "5.6.4", "hb9d3cd8_0", 0),
            PackageInfo("libmpdec", "4.0.0", "h4bc722e_0", 0),
            PackageInfo("libopenblas", "0.3.28", "pthreads_h94d23a6_1", 0),
            PackageInfo("libsqlite", "3.48.0", "hee588c1_1", 0),
            PackageInfo("libstdcxx", "14.2.0", "hc0a3c3a_1", 0),
            PackageInfo("libuuid", "2.38.1", "h0b41bf4_0", 0),
            PackageInfo("libzlib", "1.3.1", "hb9d3cd8_2", 0),
            PackageInfo("ncurses", "6.5", "h2d0b736_3", 0),
            PackageInfo("numpy", "2.2.3", "py313h17eae1a_0", 0),
            PackageInfo("openssl", "3.4.1", "h7b32b05_0", 0),
            // Omitted as added by the environment creation
            PackageInfo("pip", "25.0.1", "pyh145f28c_0", 0),
            PackageInfo("python", "3.13.2", "hf636f53_100_cp313", 0),
            PackageInfo("python_abi", "3.13", "5_cp313", 0),
            PackageInfo("readline", "8.2", "h8228510_1", 0),
            PackageInfo("scikit-learn", "1.6.1", "py313h8ef605b_0", 0),
            PackageInfo("scipy", "1.15.1", "py313h750cbce_0", 0),
            PackageInfo("setuptools", "75.8.0", "pyhff2d567_0", 0),
            PackageInfo("threadpoolctl", "3.5.0", "pyhc1e730c_0", 0),
            PackageInfo("tk", "8.6.13", "noxft_h4845f30_101", 0),
            PackageInfo("tzdata", "2025a", "h78e105d_0", 0),
        };

        std::sort(
            known_resolution.begin(),
            known_resolution.end(),
            [&](const PackageInfo& a, const PackageInfo& b) { return a.name < b.name; }
        );

        // libsolv's specification and resolution

        Request::job_list jobs;

        std::transform(
            specs_to_install.begin(),
            specs_to_install.end(),
            std::back_inserter(jobs),
            [](const std::string& spec)
            { return Request::Install{ MatchSpec::parse(spec).value() }; }
        );

        const auto request = Request{
            /* .flags= */ {},
            /* .jobs= */ jobs,
        };

        const auto outcome = libsolv::Solver().solve(libsolv_db, request);

        REQUIRE(outcome.has_value());
        REQUIRE(std::holds_alternative<Solution>(outcome.value()));
        const auto& solution = std::get<Solution>(outcome.value());

        REQUIRE(solution.actions.size() == known_resolution.size());

        std::vector<PackageInfo> libsolv_resolution = extract_package_to_install(solution);
        std::sort(
            libsolv_resolution.begin(),
            libsolv_resolution.end(),
            [&](const PackageInfo& a, const PackageInfo& b) { return a.name < b.name; }
        );

        std::vector<PackageInfo> resolvo_resolution = resolvo_libmamba_resolve(specs_to_install);
        REQUIRE(resolvo_resolution.size() == known_resolution.size());

        // Check libsolv's PackageInfo against the know resolution
        for (size_t i = 0; i < libsolv_resolution.size(); i++)
        {
            const PackageInfo& package_info = libsolv_resolution[i];
            const PackageInfo& known_package_info = known_resolution[i];
            REQUIRE(package_info.name == known_package_info.name);
            REQUIRE(package_info.version == known_package_info.version);
            REQUIRE(package_info.build_string == known_package_info.build_string);
        }

        // Check resolvo's PackageInfo against the know resolution
        for (size_t i = 0; i < resolvo_resolution.size(); i++)
        {
            const PackageInfo& package_info = resolvo_resolution[i];
            const PackageInfo& known_package_info = known_resolution[i];
            REQUIRE(package_info.name == known_package_info.name);
            REQUIRE(package_info.version == known_package_info.version);
            REQUIRE(package_info.build_string == known_package_info.build_string);
        }
    }

    SECTION("Known hard specifications")
    {
        for (const std::vector<std::string>& specs_to_install :
             std::initializer_list<std::vector<std::string>>{
                 // See: https://github.com/mamba-org/rattler/issues/684
                 //            {"arrow-cpp", "libabseil"},
                 //            {"mlflow=2.12.2"},
                 //            {"orange3=3.36.2"},
                 //            {"ray-dashboard=2.6.3"},
                 //            {"ray-default=2.6.3"},
                 //            {"spark-nlp=5.1.2"},
                 //            {"spyder=5.5.1"},
                 //            {"streamlit-faker=0.0.2"},
                 //            // See:
                 //            https://github.com/conda-forge/rubinenv-feedstock/blob/main/recipe/meta.yaml#L45-L191
                 //            {"rubin-env-nosysroot"},
                 //            {"rubin-env"},
                 //            {"rubin-env-rsp"},
                 //            {"rubin-env-developer"}
             })
        {
            // See: https://github.com/mamba-org/rattler/issues/684
            std::vector<PackageInfo> libsolv_resolution = libsolv_resolve(specs_to_install);

            // Print all the packages from libsolv
            std::cout << "libsolv resolution:" << std::endl;
            for (const auto& package_info : libsolv_resolution)
            {
                std::cout << " - " << package_info.long_str() << std::endl;
            }

            std::cout << std::endl;
            std::vector<PackageInfo> resolvo_resolution = resolvo_libmamba_resolve(specs_to_install);

            // Print all the packages from resolvo
            std::cout << "resolvo resolution:" << std::endl;
            for (const auto& package_info : resolvo_resolution)
            {
                std::cout << " - " << package_info.long_str() << std::endl;
            }

            REQUIRE(resolvo_resolution.size() > 0);
            REQUIRE(libsolv_resolution.size() > 0);

            // Check libsolv's PackageInfo against libsolv's
            REQUIRE(resolvo_resolution.size() == libsolv_resolution.size());
            for (size_t i = 0; i < std::min(resolvo_resolution.size(), libsolv_resolution.size()); i++)
            {
                const PackageInfo& resolvo_package_info = resolvo_resolution[i];
                const PackageInfo& libsolv_package_info = libsolv_resolution[i];
                // Currently something in the parsing of the repodata.json must be different.
                // TODO: find the difference and use `PackageInfo::operator==` instead
                REQUIRE(resolvo_package_info.name == libsolv_package_info.name);
                REQUIRE(resolvo_package_info.version == libsolv_package_info.version);
                REQUIRE(resolvo_package_info.build_string == libsolv_package_info.build_string);
            }
        }
    }

    SECTION("Find the highest version of hypothesis")
    {
        // Some builds of hypothesis depends on attrs and vice-versa
        // We test that this complete correctly.
        auto vid = resolvo_db.alloc_version_set("hypothesis");
        auto [version, n_track_features] = resolvo_db.find_highest_version(vid);
        REQUIRE(n_track_features == 0);
        std::cout << "Version: " << version.to_string() << std::endl;
        REQUIRE(version > Version::parse("6.105.1").value());
    }

    SECTION("Consistency with libsolv: Celery & Dash")
    {
        std::vector<std::string> specs_to_install = { "celery",
                                                      "dash",
                                                      "dash-core-components",
                                                      "dash-html-components",
                                                      "dash-table" };

        // Print all the dependencies
        std::cout << "Specification to install:" << std::endl;
        for (const auto& dep : specs_to_install)
        {
            std::cout << " - " << dep << std::endl;
        }

        std::vector<PackageInfo> libsolv_resolution = libsolv_resolve(specs_to_install);

        // Print all the packages from libsolv
        std::cout << "libsolv resolution:" << std::endl;
        for (const auto& package_info : libsolv_resolution)
        {
            std::cout << " - " << package_info.long_str() << std::endl;
        }

        std::cout << std::endl;

        std::vector<PackageInfo> resolvo_resolution = resolvo_libmamba_resolve(specs_to_install);

        // Print all the packages from resolvo
        std::cout << "resolvo resolution:" << std::endl;
        for (const auto& package_info : resolvo_resolution)
        {
            std::cout << " - " << package_info.long_str() << std::endl;
        }

        std::cout << std::endl;

        // Check libsolv's PackageInfo against libsolv's
        REQUIRE(resolvo_resolution.size() == libsolv_resolution.size());
        for (size_t i = 0; i < libsolv_resolution.size(); i++)
        {
            const PackageInfo& resolvo_package_info = resolvo_resolution[i];
            const PackageInfo& libsolv_package_info = libsolv_resolution[i];
            // Currently something in the parsing of the repodata.json must be different.
            // TODO: find the difference and use `PackageInfo::operator==` instead
            REQUIRE(resolvo_package_info.name == libsolv_package_info.name);
            REQUIRE(resolvo_package_info.version == libsolv_package_info.version);
            REQUIRE(resolvo_package_info.build_string == libsolv_package_info.build_string);
        }
    }
}
