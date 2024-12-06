// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <catch2/catch_all.hpp>
#include <solv/solver.h>

#include "solv-cpp/pool.hpp"
#include "solv-cpp/queue.hpp"
#include "solv-cpp/solver.hpp"
#include "solv-cpp/transaction.hpp"

#include "pool_data.hpp"

using namespace solv;
using namespace solv::test;

namespace
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

        SECTION(R"(Installed package "a")")
        {
            const auto ia1 = add_simple_package(pool, repo_installed, SimplePkg{ "a", "1.0" });
            repo_installed.internalize();

            auto solver = ObjSolver(pool);

            SECTION("Already statifies iteslf")
            {
                auto jobs = ObjQueue{
                    SOLVER_INSTALL | SOLVER_SOLVABLE_PROVIDES,
                    pool.add_conda_dependency("a"),
                };
                REQUIRE(solver.solve(pool, jobs));
                auto trans = ObjTransaction::from_solver(pool, solver);
                // Outcome: nothing
                REQUIRE(trans.steps().empty());
            }

            SECTION("Already satisfies dependency")
            {
                auto jobs = ObjQueue{
                    SOLVER_INSTALL | SOLVER_SOLVABLE_PROVIDES,
                    pool.add_conda_dependency("b==1.0"),
                };
                REQUIRE(solver.solve(pool, jobs));
                auto trans = ObjTransaction::from_solver(pool, solver);
                // Outcome: install only b 1.0
                REQUIRE(trans.steps() == ObjQueue{ fb1 });
            }

            SECTION("Is not removed when not needed even with ALLOW_UNINSTALL")
            {
                auto jobs = ObjQueue{
                    SOLVER_INSTALL | SOLVER_SOLVABLE_PROVIDES,
                    pool.add_conda_dependency("b==2.0"),
                };
                solver.set_flag(SOLVER_FLAG_ALLOW_UNINSTALL, true);
                REQUIRE(solver.solve(pool, jobs));
                auto trans = ObjTransaction::from_solver(pool, solver);
                // Outcome: install b 2.0, leave a
                REQUIRE(trans.steps() == ObjQueue{ fb2 });
            }

            SECTION("Gets upgraded as a dependency")
            {
                auto jobs = ObjQueue{
                    SOLVER_INSTALL | SOLVER_SOLVABLE_PROVIDES,
                    pool.add_conda_dependency("c==1.0"),
                };
                REQUIRE(solver.solve(pool, jobs));
                auto trans = ObjTransaction::from_solver(pool, solver);
                REQUIRE(trans.steps().size() == 3);
                REQUIRE(trans.steps().contains(ia1));  // Remove a 1.0
                REQUIRE(trans.steps().contains(fa2));  // Install a 2.0
                REQUIRE(trans.steps().contains(fc1));  // Install c 1.0
            }

            SECTION("Fails to upgrade when lock even with ALLOW_UNINSTALL")
            {
                auto jobs = ObjQueue{
                    SOLVER_LOCK | SOLVER_SOLVABLE_PROVIDES,
                    pool.add_conda_dependency("a"),
                    SOLVER_INSTALL | SOLVER_SOLVABLE_PROVIDES,
                    pool.add_conda_dependency("c==1.0"),
                };
                solver.set_flag(SOLVER_FLAG_ALLOW_UNINSTALL, true);
                REQUIRE_FALSE(solver.solve(pool, jobs));
            }
        }

        SECTION(R"(Installed package "a" get downgraded by dependency)")
        {
            const auto ia2 = add_simple_package(pool, repo_installed, SimplePkg{ "a", "2.0" });
            repo_installed.internalize();

            auto solver = ObjSolver(pool);

            SECTION("Fails by default")
            {
                auto jobs = ObjQueue{
                    SOLVER_INSTALL | SOLVER_SOLVABLE_PROVIDES,
                    pool.add_conda_dependency("c==2.0"),
                };
                REQUIRE_FALSE(solver.solve(pool, jobs));
            }

            SECTION("Succeeds with ALLOW_DOWNGRADE or ALLOW_UNINSTALL")
            {
                for (const auto flag : { SOLVER_FLAG_ALLOW_DOWNGRADE, SOLVER_FLAG_ALLOW_UNINSTALL })
                {
                    solver.set_flag(flag, true);
                    auto jobs = ObjQueue{
                        SOLVER_INSTALL | SOLVER_SOLVABLE_PROVIDES,
                        pool.add_conda_dependency("c==2.0"),
                    };
                    REQUIRE(solver.solve(pool, jobs));
                    auto trans = ObjTransaction::from_solver(pool, solver);
                    REQUIRE(trans.steps().size() == 3);
                    REQUIRE(trans.steps().contains(ia2));  // Remove a 2.0
                    REQUIRE(trans.steps().contains(fa1));  // Install a 1.0
                    REQUIRE(trans.steps().contains(fc2));  // Install c 2.0
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

        SECTION("Direct job namespace dependency")
        {
            SECTION("Which resolves to some packages")
            {
                bool called = false;
                pool.set_namespace_callback(
                    [&, a_solv_id = a_solv_id](ObjPoolView, StringId name, StringId ver) noexcept -> OffsetId
                    {
                        called = true;
                        REQUIRE(name == dep_name_id);
                        REQUIRE(ver == dep_ver_id);
                        return pool.add_to_whatprovides_data({ a_solv_id });
                    }
                );

                auto solver = ObjSolver(pool);
                auto solved = solver.solve(pool, { SOLVER_INSTALL, dep_id });
                REQUIRE(solved);
                REQUIRE(called);
            }

            SECTION("Which is unsatisfyable")
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
                REQUIRE(called);
                REQUIRE_FALSE(solved);
            }

            SECTION("Callback throws")
            {
                pool.set_namespace_callback(
                    [](ObjPoolView, StringId, StringId) -> OffsetId
                    { throw std::runtime_error("Error!"); }
                );

                auto solver = ObjSolver(pool);
                REQUIRE_THROWS_AS(
                    [&] { return solver.solve(pool, { SOLVER_INSTALL, dep_id }); }(),
                    std::runtime_error
                );
            }
        }

        SECTION("transitive job dependency")
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

            SECTION("Which resolves to some packages")
            {
                bool called = false;
                pool.set_namespace_callback(
                    [&, a_solv_id = a_solv_id](ObjPoolView, StringId name, StringId ver) noexcept -> OffsetId
                    {
                        called = true;
                        REQUIRE(name == dep_name_id);
                        REQUIRE(ver == dep_ver_id);
                        return pool.add_to_whatprovides_data({ a_solv_id });
                    }
                );

                auto solver = ObjSolver(pool);
                auto solved = solver.solve(pool, { SOLVER_INSTALL, job_id });
                REQUIRE(called);
                REQUIRE(solved);
            }

            SECTION("Which is unsatisfyable")
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
                REQUIRE(called);
                REQUIRE_FALSE(solved);
            }

            SECTION("Callback throws")
            {
                pool.set_namespace_callback(
                    [](ObjPoolView, StringId, StringId) -> OffsetId
                    { throw std::runtime_error("Error!"); }
                );

                auto solver = ObjSolver(pool);
                REQUIRE_THROWS_AS(
                    [&] { return solver.solve(pool, { SOLVER_INSTALL, job_id }); }(),
                    std::runtime_error
                );
            }
        }
    }
}
