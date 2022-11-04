// Copyright (c) 2022, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <array>
#include <vector>
#include <string>
#include <random>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <fmt/format.h>

#include "mamba/core/pool.hpp"
#include "mamba/core/solver.hpp"
#include "mamba/core/repo.hpp"
#include "mamba/core/prefix_data.hpp"
#include "mamba/core/channel.hpp"
#include "mamba/core/subdirdata.hpp"
#include "mamba/core/mamba_fs.hpp"
#include "mamba/core/satisfiability_error.hpp"
#include "mamba/core/package_info.hpp"
#include "mamba/core/util_string.hpp"
#include "mamba/core/util_random.hpp"
#include "mamba/core/util.hpp"

namespace mamba
{

    TEST(dependency_info, unconstrained)
    {
        auto const d = DependencyInfo("foo7 ");
        EXPECT_EQ(d.name(), "foo7");
        EXPECT_EQ(d.version(), "*");
        EXPECT_EQ(d.build_string(), "*");
        EXPECT_EQ(d.str(), "foo7 * *");
    }

    TEST(dependency_info, version_range)
    {
        auto const d = DependencyInfo(" foo_bar  >=4.3.0,<5.0 ");
        EXPECT_EQ(d.name(), "foo_bar");
        EXPECT_EQ(d.version(), ">=4.3.0,<5.0");
        EXPECT_EQ(d.build_string(), "*");
        EXPECT_EQ(d.str(), "foo_bar >=4.3.0,<5.0 *");
    }

    TEST(dependency_info, version_equality)
    {
        auto const d = DependencyInfo("foo-bar==4.3.0");
        EXPECT_EQ(d.name(), "foo-bar");
        EXPECT_EQ(d.version(), "==4.3.0");
        EXPECT_EQ(d.build_string(), "*");
        EXPECT_EQ(d.str(), "foo-bar ==4.3.0 *");
    }

    TEST(dependency_info, build_range)
    {
        auto const d = DependencyInfo(" python_abi  3.10.*  *_cp310 ");
        EXPECT_EQ(d.name(), "python_abi");
        EXPECT_EQ(d.version(), "3.10.*");
        EXPECT_EQ(d.build_string(), "*_cp310");
        EXPECT_EQ(d.str(), "python_abi 3.10.* *_cp310");
    }

    TEST(dependency_info, fail)
    {
        EXPECT_ANY_THROW(DependencyInfo("<foo"));
    }

    TEST(conflict_map, symetric)
    {
        auto c = conflict_map<std::size_t>();
        EXPECT_EQ(c.size(), 0);
        EXPECT_FALSE(c.has_conflict(0));
        EXPECT_FALSE(c.in_conflict(0, 1));
        c.add(0, 1);
        c.add(1, 2);
        EXPECT_TRUE(c.has_conflict(0));
        EXPECT_TRUE(c.in_conflict(0, 1));
        EXPECT_TRUE(c.in_conflict(1, 2));
        EXPECT_TRUE(c.has_conflict(2));
        EXPECT_FALSE(c.in_conflict(0, 2));
    }

    /**
     * A RAII object to ensure a path exists only for the lifetime of the guard.
     */
    struct dir_guard
    {
        fs::u8path path;

        dir_guard(fs::u8path&& path_)
            : path(std::move(path_))
        {
            fs::create_directories(path);
        }
        ~dir_guard()
        {
            fs::remove_all(path);
        }
    };

    /**
     * Simple factory for building a PackageInfo.
     */
    auto mkpkg(std::string name, std::string version, std::vector<std::string> dependencies = {})
        -> PackageInfo
    {
        auto pkg = PackageInfo(std::move(name));
        pkg.version = std::move(version);
        pkg.depends = std::move(dependencies);
        pkg.build_string = "bld";
        return pkg;
    }


    /**
     * Create the repodata.json file containing the package information.
     */
    template <typename PkgRange>
    auto create_repodata_json(fs::u8path dir, PkgRange const& packages) -> fs::u8path
    {
        namespace nl = nlohmann;

        auto packages_j = nl::json::object();
        for (auto const& pkg : packages)
        {
            auto fname = fmt::format("{}-{}-{}.tar.bz2", pkg.name, pkg.version, pkg.build_string);
            packages_j[std::move(fname)] = pkg.json_record();
        }
        auto repodata_j = nl::json::object();
        repodata_j["packages"] = std::move(packages_j);

        fs::create_directories(dir / "noarch");
        auto repodata_f = dir / "noarch/repodata.json";
        open_ofstream(repodata_f, std::ofstream::app) << repodata_j;

        return repodata_f;
    }

    /**
     * Create a solver and a pool of a conflict.
     *
     * The underlying packages do not exist, we are onl interested in the conflict.
     */
    template <typename PkgRange>
    auto create_problem(PkgRange const& packages, std::vector<std::string> const& specs)
    {
        auto const tmp_dir = dir_guard(fs::temp_directory_path() / "mamba/tests"
                                       / generate_random_alphanumeric_string(20));
        auto const repodata_f = create_repodata_json(tmp_dir.path, packages);

        auto pool = std::make_unique<MPool>();
        MRepo::create(*pool, "some-name", repodata_f, "some-url");
        auto solver = std::make_unique<MSolver>(
            *pool, std::vector{ std::pair{ SOLVER_FLAG_ALLOW_DOWNGRADE, 1 } });
        solver->add_jobs(specs, SOLVER_INSTALL);

        return std::pair{ std::move(solver), std::move(pool) };
    }

    /**
     * Test the test utility function.
     */
    TEST(satifiability_error, create_problem)
    {
        auto [solver, pool] = create_problem(std::array{ mkpkg("foo", "0.1.0", {}) }, { "foo" });
        auto const solved = solver->solve();
        ASSERT_TRUE(solved);
    }

    auto create_basic_conflict() -> std::pair<MSolver&, MPool&>
    {
        static auto solver_pool = create_problem(
            std::array{
                mkpkg("A", "0.1.0"),
                mkpkg("A", "0.2.0"),
                mkpkg("A", "0.3.0"),
            },
            { "A=0.4.0" });
        return { *solver_pool.first, *solver_pool.second };
    }

    /**
     * Create the PubGrub blog post example.
     *
     * The example given by Natalie Weizenbaum
     * (credits https://nex3.medium.com/pubgrub-2fb6470504f).
     */
    auto create_pubgrub() -> std::pair<MSolver&, MPool&>
    {
        static auto solver_pool = create_problem(
            std::array{
                mkpkg("menu", "1.5.0", { "dropdown=2.*" }),
                mkpkg("menu", "1.4.0", { "dropdown=2.*" }),
                mkpkg("menu", "1.3.0", { "dropdown=2.*" }),
                mkpkg("menu", "1.2.0", { "dropdown=2.*" }),
                mkpkg("menu", "1.1.0", { "dropdown=2.*" }),
                mkpkg("menu", "1.0.0", { "dropdown=1.*" }),
                mkpkg("dropdown", "2.3.0", { "icons=2.*" }),
                mkpkg("dropdown", "2.2.0", { "icons=2.*" }),
                mkpkg("dropdown", "2.1.0", { "icons=2.*" }),
                mkpkg("dropdown", "2.0.0", { "icons=2.*" }),
                mkpkg("dropdown", "1.8.0", { "icons=1.*", "intl=3.*" }),
                mkpkg("icons", "2.0.0"),
                mkpkg("icons", "1.0.0"),
                mkpkg("intl", "5.0.0"),
                mkpkg("intl", "4.0.0"),
                mkpkg("intl", "3.0.0"),
            },
            { "menu", "icons=1.*", "intl=5.*" });
        return { *solver_pool.first, *solver_pool.second };
    }

    auto create_pubgrub_hard_(bool missing_package)
    {
        auto packages = std::vector{
            mkpkg("menu", "2.1.0", { "dropdown>=2.1", "emoji" }),
            mkpkg("menu", "2.0.1", { "dropdown>=2", "emoji" }),
            mkpkg("menu", "2.0.0", { "dropdown>=2", "emoji" }),
            mkpkg("menu", "1.5.0", { "dropdown=2.*", "emoji" }),
            mkpkg("menu", "1.4.0", { "dropdown=2.*", "emoji" }),
            mkpkg("menu", "1.3.0", { "dropdown=2.*" }),
            mkpkg("menu", "1.2.0", { "dropdown=2.*" }),
            mkpkg("menu", "1.1.0", { "dropdown=1.*" }),
            mkpkg("menu", "1.0.0", { "dropdown=1.*" }),
            mkpkg("emoji", "1.1.0", { "libicons=2.*" }),
            mkpkg("emoji", "1.0.0", { "libicons=2.*" }),
            mkpkg("dropdown", "2.3.0", { "libicons=2.*" }),
            mkpkg("dropdown", "2.2.0", { "libicons=2.*" }),
            mkpkg("dropdown", "2.1.0", { "libicons=2.*" }),
            mkpkg("dropdown", "2.0.0", { "libicons=2.*" }),
            mkpkg("dropdown", "1.8.0", { "libicons=1.*", "intl=3.*" }),
            mkpkg("dropdown", "1.7.0", { "libicons=1.*", "intl=3.*" }),
            mkpkg("dropdown", "1.6.0", { "libicons=1.*", "intl=3.*" }),
            mkpkg("pyicons", "2.0.0", { "libicons=2.*" }),
            mkpkg("pyicons", "1.1.0", { "libicons=1.2.*" }),
            mkpkg("pyicons", "1.0.0", { "libicons=1.*" }),
            mkpkg("pretty", "1.1.0", { "pyicons=1.1.*" }),
            mkpkg("pretty", "1.0.1", { "pyicons=1.*" }),
            mkpkg("pretty", "1.0.0", { "pyicons=1.*" }),
            mkpkg("intl", "5.0.0"),
            mkpkg("intl", "4.0.0"),
            mkpkg("intl", "3.2.0"),
            mkpkg("intl", "3.1.0"),
            mkpkg("intl", "3.0.0"),
            mkpkg("intl-mod", "1.0.0", { "intl=5.0.*" }),
            mkpkg("intl-mod", "1.0.1", { "intl=5.0.*" }),
            mkpkg("libicons", "2.1.0"),
            mkpkg("libicons", "2.0.1"),
            mkpkg("libicons", "2.0.0"),
            mkpkg("libicons", "1.2.1"),
            mkpkg("libicons", "1.2.0"),
            mkpkg("libicons", "1.0.0"),
        };

        if (missing_package)
        {
            packages.push_back(mkpkg("dropdown", "2.9.3", { "libnothere>1.0" }));
            packages.push_back(mkpkg("dropdown", "2.9.2", { "libicons>10.0", "libnothere>1.0" }));
            packages.push_back(mkpkg("dropdown", "2.9.1", { "libicons>10.0", "libnothere>1.0" }));
            packages.push_back(mkpkg("dropdown", "2.9.0", { "libicons>10.0" }));
        }
        return create_problem(packages,
                              { "menu", "pyicons=1.*", "intl=5.*", "intl-mod", "pretty>=1.0" });
    }

    /**
     * A harder version of ``create_pubgrub``.
     */
    auto create_pubgrub_hard() -> std::pair<MSolver&, MPool&>
    {
        static auto solver_pool = create_pubgrub_hard_(false);
        return { *solver_pool.first, *solver_pool.second };
    }

    /**
     * The hard version of the alternate PubGrub with missing packages.
     */
    auto create_pubgrub_missing() -> std::pair<MSolver&, MPool&>
    {
        static auto solver_pool = create_pubgrub_hard_(true);
        return { *solver_pool.first, *solver_pool.second };
    }

    template <typename T, typename E>
    auto expected_value_or_throw(tl::expected<T, E>&& ex)
    {
        if (!ex.has_value())
        {
            throw ex.error();
        }
        return std::move(ex).value();
    }

    auto make_platform_channels(std::vector<std::string>&& channels,
                                std::vector<std::string> const& platforms)
        -> std::vector<std::string>
    {
        auto add_plat = [&platforms](auto const& chan)
        { return fmt::format("{}[{}]", chan, fmt::join(platforms, ",")); };
        std::transform(channels.begin(), channels.end(), channels.begin(), add_plat);
        return std::move(channels);
    }

    /**
     * Mock of channel_loader.hpp:load_channels that takes a list of channels.
     */
    auto load_channels(MPool& pool, MultiPackageCache& cache, std::vector<std::string>&& channels)
    {
        auto dlist = MultiDownloadTarget();
        auto sub_dirs = std::vector<MSubdirData>();
        for (auto const* chan : get_channels(channels))
        {
            for (auto& [platform, url] : chan->platform_urls(true))
            {
                auto sub_dir
                    = expected_value_or_throw(MSubdirData::create(*chan, platform, url, cache));
                dlist.add(sub_dir.target());
                sub_dirs.push_back(std::move(sub_dir));
            }
        }
        dlist.download(MAMBA_DOWNLOAD_FAILFAST);
        for (auto& sub_dir : sub_dirs)
        {
            sub_dir.create_repo(pool);
        }
    }

    /**
     * Create a solver and a pool of a conflict from conda-forge packages.
     */
    auto create_conda_forge(std::vector<std::string>&& specs,
                            std::vector<PackageInfo> const& virtual_packages
                            = { mkpkg("__glibc", "2.17.0") },
                            std::vector<std::string>&& channels = { "conda-forge" },
                            std::vector<std::string> const& platforms = { "linux-64", "noarch" })
    {
        // Reusing the cache for all invocation of this funciton for speedup
        static auto const tmp_dir = dir_guard(fs::temp_directory_path() / "mamba/tests"
                                              / generate_random_alphanumeric_string(20));

        auto prefix_data = expected_value_or_throw(PrefixData::create(tmp_dir.path / "prefix"));
        prefix_data.add_packages(virtual_packages);
        auto pool = std::make_unique<MPool>();
        auto& repo = MRepo::create(*pool, prefix_data);
        repo.set_installed();

        auto cache = MultiPackageCache({ tmp_dir.path / "cache" });
        create_cache_dir(cache.first_writable_path());
        load_channels(*pool, cache, make_platform_channels(std::move(channels), platforms));

        auto solver = std::make_unique<MSolver>(
            *pool, std::vector{ std::pair{ SOLVER_FLAG_ALLOW_DOWNGRADE, 1 } });
        solver->add_jobs(specs, SOLVER_INSTALL);

        return std::pair{ std::move(solver), std::move(pool) };
    }

    /**
     * Test the test utility function.
     */
    TEST(satifiability_error, create_conda_forge)
    {
        auto [solver, pool] = create_conda_forge({ "xtensor>=0.7" });
        auto const solved = solver->solve();
        ASSERT_TRUE(solved);
    }

    auto create_pytorch_cpu() -> std::pair<MSolver&, MPool&>
    {
        static auto solver_pool = create_conda_forge({ "python=2.7", "pytorch=1.12" });
        return { *solver_pool.first, *solver_pool.second };
    }

    auto create_pytorch_cuda() -> std::pair<MSolver&, MPool&>
    {
        static auto solver_pool
            = create_conda_forge({ "python=2.7", "pytorch=1.12" },
                                 { mkpkg("__glibc", "2.17.0"), mkpkg("__cuda", "10.2.0") });
        return { *solver_pool.first, *solver_pool.second };
    }

    auto create_cudatoolkit() -> std::pair<MSolver&, MPool&>
    {
        static auto solver_pool
            = create_conda_forge({ "python=3.7",
                                   "cudatoolkit=11.1",
                                   "cudnn=8.0",
                                   "pytorch=1.8",
                                   "torchvision=0.9=*py37_cu111*" },
                                 { mkpkg("__glibc", "2.17.0"), mkpkg("__cuda", "11.1") });
        return { *solver_pool.first, *solver_pool.second };
    }

    auto create_jpeg9b() -> std::pair<MSolver&, MPool&>
    {
        static auto solver_pool = create_conda_forge({ "python=3.7", "jpeg=9b" });
        return { *solver_pool.first, *solver_pool.second };
    }

    auto create_r_base() -> std::pair<MSolver&, MPool&>
    {
        static auto solver_pool = create_conda_forge(
            { "r-base=3.5.* ", "pandas=0", "numpy<1.20.0", "matplotlib=2", "r-matchit=4.*" });
        return { *solver_pool.first, *solver_pool.second };
    }

    auto create_scip() -> std::pair<MSolver&, MPool&>
    {
        static auto solver_pool = create_conda_forge({ "scip=8.*", "pyscipopt<4.0" });
        return { *solver_pool.first, *solver_pool.second };
    }

    auto create_jupyterlab() -> std::pair<MSolver&, MPool&>
    {
        static auto solver_pool = create_conda_forge({ "jupyterlab=3.4", "openssl=3.0.0" });
        return { *solver_pool.first, *solver_pool.second };
    }

    class Problem : public testing::TestWithParam<decltype(&create_basic_conflict)>
    {
    };

    template <typename NodeVariant>
    auto is_virtual_package(NodeVariant const& node) -> bool
    {
        return std::visit(
            [](auto const& n) -> bool
            {
                using Node = std::remove_const_t<std::remove_reference_t<decltype(n)>>;
                if constexpr (!std::is_same_v<Node, ProblemsGraph::RootNode>)
                {
                    return starts_with(std::invoke(&Node::name, n), "__");
                }
                return false;
            },
            node);
    };

    auto has_problem_type(ProblemsGraph::node_t const& node) -> bool
    {
        return std::visit(
            [](auto const& n) -> bool
            {
                using Node = std::remove_const_t<std::remove_reference_t<decltype(n)>>;
                if constexpr (std::is_same_v<Node, ProblemsGraph::RootNode>)
                {
                    return false;
                }
                if constexpr (std::is_same_v<Node, ProblemsGraph::PackageNode>)
                {
                    return n.problem_type.has_value();
                }
                return true;
            },
            node);
    };


    TEST_P(Problem, constructor)
    {
        auto [solver, pool] = std::invoke(GetParam());
        auto const solved = solver.solve();
        ASSERT_FALSE(solved);
        auto const pbs = ProblemsGraph::from_solver(solver, pool);
        auto const& g = pbs.graph();

        EXPECT_GE(g.number_of_nodes(), 1);
        for (std::size_t id = 0; id < g.number_of_nodes(); ++id)
        {
            auto const& node = g.node(id);
            if (is_virtual_package(node))
            {
                // Currently we do not make assumption about virtual package since
                // we are not sure we are including them the same way than they would be in practice
                break;
            }
            else if (g.in_degree(id) == 0)
            {
                // Only one root node
                EXPECT_EQ(id, pbs.root_node());
                EXPECT_TRUE(std::holds_alternative<ProblemsGraph::RootNode>(node));
            }
            else if (g.out_degree(id) == 0)
            {
                EXPECT_FALSE(std::holds_alternative<ProblemsGraph::RootNode>(node));
                EXPECT_TRUE(has_problem_type(node));
            }
            else
            {
                EXPECT_TRUE(std::holds_alternative<ProblemsGraph::PackageNode>(node));
                EXPECT_FALSE(has_problem_type(node));
            }
            // All nodes reachable from the root
            EXPECT_TRUE(is_reachable(pbs.graph(), pbs.root_node(), id));
        }

        auto const& conflicts = pbs.conflicts();
        for (auto const& [n, _] : conflicts)
        {
            EXPECT_TRUE(std::holds_alternative<ProblemsGraph::PackageNode>(g.node(n))
                        || std::holds_alternative<ProblemsGraph::ConstraintNode>(g.node(n)));
        }
    }

    TEST(satifiability_error, NamedList)
    {
        auto l = CompressedProblemsGraph::PackageListNode();
        static constexpr std::size_t n_packages = 9;
        for (std::size_t minor = 1; minor <= n_packages; ++minor)
        {
            l.insert({ mkpkg("pkg", fmt::format("0.{}.0", minor)) });
        }
        EXPECT_EQ(l.size(), n_packages);
        EXPECT_EQ(l.name(), "pkg");
        EXPECT_EQ(l.versions_trunc(", ", "..."), "0.1.0, 0.2.0, ..., 0.9.0");
        EXPECT_EQ(l.build_strings_trunc(", ", "..."), "bld, bld, ..., bld");
    }

    TEST_P(Problem, compression)
    {
        using CpPbGr = CompressedProblemsGraph;

        auto [solver, pool] = std::invoke(GetParam());
        auto const solved = solver.solve();
        ASSERT_FALSE(solved);
        auto const pbs = ProblemsGraph::from_solver(solver, pool);
        auto const cp_pbs = CpPbGr::from_problems_graph(pbs);
        auto const& cp_g = cp_pbs.graph();

        EXPECT_GE(pbs.graph().number_of_nodes(), cp_g.number_of_nodes());
        EXPECT_GE(cp_g.number_of_nodes(), 1);
        for (std::size_t id = 0; id < cp_g.number_of_nodes(); ++id)
        {
            auto const& node = cp_g.node(id);
            if (is_virtual_package(node))
            {
                // Currently we do not make assumption about virtual package since
                // we are not sure we are including them the same way than they would be in
                break;
            }
            else if (cp_g.in_degree(id) == 0)
            {
                // Only one root node
                EXPECT_EQ(id, pbs.root_node());
                EXPECT_TRUE(std::holds_alternative<CpPbGr::RootNode>(node));
            }
            else if (cp_g.out_degree(id) == 0)
            {
                EXPECT_FALSE(std::holds_alternative<CpPbGr::RootNode>(node));
            }
            else
            {
                EXPECT_TRUE(std::holds_alternative<CpPbGr::PackageListNode>(node));
            }
            // All nodes reachable from the root
            EXPECT_TRUE(is_reachable(pbs.graph(), pbs.root_node(), id));
        }

        auto const& conflicts = cp_pbs.conflicts();
        for (auto const& [n, _] : conflicts)
        {
            EXPECT_TRUE(std::holds_alternative<CpPbGr::PackageListNode>(cp_g.node(n))
                        || std::holds_alternative<CpPbGr::ConstraintListNode>(cp_g.node(n)));
        }
    }

    TEST_P(Problem, problem_tree_str)
    {
        auto [solver, pool] = std::invoke(GetParam());
        auto const solved = solver.solve();
        ASSERT_FALSE(solved);
        auto const pbs = ProblemsGraph::from_solver(solver, pool);
        auto const cp_pbs = CompressedProblemsGraph::from_problems_graph(pbs);
        auto const message = problem_tree_str(cp_pbs);

        auto message_contains = [&](auto const& node)
        {
            using Node = std::remove_cv_t<std::remove_reference_t<decltype(node)>>;
            if constexpr (!std::is_same_v<Node, CompressedProblemsGraph::RootNode>)
            {
                EXPECT_TRUE(contains(message, node.name()));
            }
        };

        for (auto const& node : cp_pbs.graph().nodes())
        {
            std::visit(message_contains, node);
        }
    }

    INSTANTIATE_TEST_SUITE_P(satifiability_error,
                             Problem,
                             testing::Values(create_basic_conflict,
                                             create_pubgrub,
                                             create_pubgrub_hard,
                                             create_pubgrub_missing,
                                             create_pytorch_cpu,
                                             create_pytorch_cuda,
                                             create_cudatoolkit,
                                             create_jpeg9b,
                                             create_r_base,
                                             create_scip,
                                             create_jupyterlab));
}
