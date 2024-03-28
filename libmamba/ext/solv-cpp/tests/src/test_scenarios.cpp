// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <doctest/doctest.h>
#include <solv/solver.h>

#include "solv-cpp/pool.hpp"
#include "solv-cpp/queue.hpp"
#include "solv-cpp/solver.hpp"
#include "solv-cpp/transaction.hpp"

#include "pool_data.hpp"

using namespace solv;
using namespace solv::test;

TEST_SUITE("solv::scenariso")
{
    TEST_CASE("Solving")
    {
        auto pool = ObjPool();

        auto [forge_id, repo_forge] = pool.add_repo("forge");
        const auto fa1 = add_simple_package(pool, repo_forge, SimplePkg{ "a", "1.0" });
        const auto fa2 = add_simple_package(pool, repo_forge, SimplePkg{ "a", "2.0" });
        const auto fb1 = add_simple_package(pool, repo_forge, SimplePkg{ "b", "1.0", { "a==1.0" } });
        const auto fb2 = add_simple_package(pool, repo_forge, SimplePkg{ "b", "2.0" });
        const auto fc1 = add_simple_package(pool, repo_forge, SimplePkg{ "c", "1.0", { "a==2.0" } });
        const auto fc2 = add_simple_package(pool, repo_forge, SimplePkg{ "c", "2.0", { "a==1.0" } });
        repo_forge.internalize();

        auto [installed_id, repo_installed] = pool.add_repo("installed");
        pool.set_installed_repo(installed_id);

        SUBCASE(R"(Installed package "a")")
        {
            const auto ia1 = add_simple_package(pool, repo_installed, SimplePkg{ "a", "1.0" });
            repo_installed.internalize();

            auto solver = ObjSolver(pool);

            SUBCASE("Already statifies iteslf")
            {
                auto jobs = ObjQueue{
                    SOLVER_INSTALL | SOLVER_SOLVABLE_PROVIDES,
                    pool.add_conda_dependency("a"),
                };
                CHECK(solver.solve(pool, jobs));
                auto trans = ObjTransaction::from_solver(pool, solver);
                // Outcome: nothing
                CHECK(trans.steps().empty());
            }

            SUBCASE("Already satisfies dependency")
            {
                auto jobs = ObjQueue{
                    SOLVER_INSTALL | SOLVER_SOLVABLE_PROVIDES,
                    pool.add_conda_dependency("b==1.0"),
                };
                CHECK(solver.solve(pool, jobs));
                auto trans = ObjTransaction::from_solver(pool, solver);
                // Outcome: install only b 1.0
                CHECK_EQ(trans.steps(), ObjQueue{ fb1 });
            }

            SUBCASE("Is not removed when not needed even with ALLOW_UNINSTALL")
            {
                auto jobs = ObjQueue{
                    SOLVER_INSTALL | SOLVER_SOLVABLE_PROVIDES,
                    pool.add_conda_dependency("b==2.0"),
                };
                solver.set_flag(SOLVER_FLAG_ALLOW_UNINSTALL, true);
                CHECK(solver.solve(pool, jobs));
                auto trans = ObjTransaction::from_solver(pool, solver);
                // Outcome: install b 2.0, leave a
                CHECK_EQ(trans.steps(), ObjQueue{ fb2 });
            }

            SUBCASE("Gets upgraded as a dependency")
            {
                auto jobs = ObjQueue{
                    SOLVER_INSTALL | SOLVER_SOLVABLE_PROVIDES,
                    pool.add_conda_dependency("c==1.0"),
                };
                CHECK(solver.solve(pool, jobs));
                auto trans = ObjTransaction::from_solver(pool, solver);
                CHECK_EQ(trans.steps().size(), 3);
                CHECK(trans.steps().contains(ia1));  // Remove a 1.0
                CHECK(trans.steps().contains(fa2));  // Install a 2.0
                CHECK(trans.steps().contains(fc1));  // Install c 1.0
            }

            SUBCASE("Fails to upgrade when lock even with ALLOW_UNINSTALL")
            {
                auto jobs = ObjQueue{
                    SOLVER_LOCK | SOLVER_SOLVABLE_PROVIDES,
                    pool.add_conda_dependency("a"),
                    SOLVER_INSTALL | SOLVER_SOLVABLE_PROVIDES,
                    pool.add_conda_dependency("c==1.0"),
                };
                solver.set_flag(SOLVER_FLAG_ALLOW_UNINSTALL, true);
                CHECK_FALSE(solver.solve(pool, jobs));
            }
        }

        SUBCASE(R"(Installed package "a" get downgraded by dependency)")
        {
            const auto ia2 = add_simple_package(pool, repo_installed, SimplePkg{ "a", "2.0" });
            repo_installed.internalize();

            auto solver = ObjSolver(pool);

            SUBCASE("Fails by default")
            {
                auto jobs = ObjQueue{
                    SOLVER_INSTALL | SOLVER_SOLVABLE_PROVIDES,
                    pool.add_conda_dependency("c==2.0"),
                };
                CHECK_FALSE(solver.solve(pool, jobs));
            }

            SUBCASE("Succeeds with ALLOW_DOWNGRADE or ALLOW_UNINSTALL")
            {
                for (const auto flag : { SOLVER_FLAG_ALLOW_DOWNGRADE, SOLVER_FLAG_ALLOW_UNINSTALL })
                {
                    solver.set_flag(flag, true);
                    auto jobs = ObjQueue{
                        SOLVER_INSTALL | SOLVER_SOLVABLE_PROVIDES,
                        pool.add_conda_dependency("c==2.0"),
                    };
                    CHECK(solver.solve(pool, jobs));
                    auto trans = ObjTransaction::from_solver(pool, solver);
                    CHECK_EQ(trans.steps().size(), 3);
                    CHECK(trans.steps().contains(ia2));  // Remove a 2.0
                    CHECK(trans.steps().contains(fa1));  // Install a 1.0
                    CHECK(trans.steps().contains(fc2));  // Install c 2.0
                }
            }
        }
    }

    TEST_CASE("Resolve namespace dependencies")
    {
        auto pool = ObjPool();

        const auto dep_name_id = pool.add_string("dep-name");
        const auto dep_ver_id = pool.add_string("dep-ver");
        const auto dep_id = pool.add_dependency(dep_name_id, REL_NAMESPACE, dep_ver_id);

        auto [repo_id, repo] = pool.add_repo("forge");
        const auto a_solv_id = add_simple_package(pool, repo, SimplePkg{ "a", "1.0" });
        repo.internalize();

        SUBCASE("Direct job namespace dependency")
        {
            SUBCASE("Which resolves to some packages")
            {
                bool called = false;
                pool.set_namespace_callback(
                    [&, a_solv_id = a_solv_id](ObjPoolView, StringId name, StringId ver) noexcept -> OffsetId
                    {
                        called = true;
                        CHECK_EQ(name, dep_name_id);
                        CHECK_EQ(ver, dep_ver_id);
                        return pool.add_to_whatprovides_data({ a_solv_id });
                    }
                );

                auto solver = ObjSolver(pool);
                auto solved = solver.solve(pool, { SOLVER_INSTALL, dep_id });
                CHECK(solved);
                CHECK(called);
            }

            SUBCASE("Which is unsatisfyable")
            {
                bool called = false;
                pool.set_namespace_callback(
                    [&](ObjPoolView, StringId, StringId) noexcept -> OffsetId
                    {
                        called = true;
                        return 0;  // 0 means "not-found"
                    }
                );

                auto solver = ObjSolver(pool);
                auto solved = solver.solve(pool, { SOLVER_INSTALL, dep_id });
                CHECK(called);
                CHECK_FALSE(solved);
            }

            SUBCASE("Callback throws")
            {
                pool.set_namespace_callback(
                    [](ObjPoolView, StringId, StringId) -> OffsetId
                    { throw std::runtime_error("Error!"); }
                );

                auto solver = ObjSolver(pool);
                CHECK_THROWS_AS(
                    [&] { return solver.solve(pool, { SOLVER_INSTALL, dep_id }); }(),
                    std::runtime_error
                );
            }
        }

        SUBCASE("transitive job dependency")
        {
            // Add a dependency ``job==3.0``
            const auto job_name_id = pool.add_string("job");
            const auto job_ver_id = pool.add_string("3.0");
            const auto job_id = pool.add_dependency(job_name_id, REL_EQ, job_ver_id);

            // Add a package ``{name=job, version=3.0}`` with dependency in namespace dep.
            auto [job_solv_id, job_solv] = repo.add_solvable();
            job_solv.set_name(job_name_id);
            job_solv.set_version(job_ver_id);
            job_solv.set_dependencies({ dep_id });
            job_solv.add_self_provide();
            repo.internalize();

            SUBCASE("Which resolves to some packages")
            {
                bool called = false;
                pool.set_namespace_callback(
                    [&, a_solv_id = a_solv_id](ObjPoolView, StringId name, StringId ver) noexcept -> OffsetId
                    {
                        called = true;
                        CHECK_EQ(name, dep_name_id);
                        CHECK_EQ(ver, dep_ver_id);
                        return pool.add_to_whatprovides_data({ a_solv_id });
                    }
                );

                auto solver = ObjSolver(pool);
                auto solved = solver.solve(pool, { SOLVER_INSTALL, job_id });
                CHECK(called);
                CHECK(solved);
            }

            SUBCASE("Which is unsatisfyable")
            {
                bool called = false;
                pool.set_namespace_callback(
                    [&](ObjPoolView, StringId, StringId) noexcept -> OffsetId
                    {
                        called = true;
                        return 0;  // 0 means "not-found"
                    }
                );

                auto solver = ObjSolver(pool);
                auto solved = solver.solve(pool, { SOLVER_INSTALL, job_id });
                CHECK(called);
                CHECK_FALSE(solved);
            }

            SUBCASE("Callback throws")
            {
                pool.set_namespace_callback(
                    [](ObjPoolView, StringId, StringId) -> OffsetId
                    { throw std::runtime_error("Error!"); }
                );

                auto solver = ObjSolver(pool);
                CHECK_THROWS_AS(
                    [&] { return solver.solve(pool, { SOLVER_INSTALL, job_id }); }(),
                    std::runtime_error
                );
            }
        }
    }
}
