// Copyright (c) 2022, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <array>
#include <string>
#include <vector>

#include <doctest/doctest.h>
#include <fmt/format.h>
#include <nlohmann/json.hpp>
#include <solv/solver.h>

#include "mamba/core/channel_context.hpp"
#include "mamba/core/pool.hpp"
#include "mamba/core/prefix_data.hpp"
#include "mamba/core/satisfiability_error.hpp"
#include "mamba/core/solver.hpp"
#include "mamba/core/subdirdata.hpp"
#include "mamba/core/util.hpp"
#include "mamba/fs/filesystem.hpp"
#include "mamba/solver/libsolv/repo_info.hpp"
#include "mamba/specs/package_info.hpp"
#include "mamba/util/random.hpp"
#include "mamba/util/string.hpp"

#include "mambatests.hpp"

using namespace mamba;

TEST_SUITE("conflict_map")
{
    TEST_CASE("symetric")
    {
        auto c = conflict_map<std::size_t>();
        CHECK_EQ(c.size(), 0);
        CHECK_FALSE(c.has_conflict(0));
        CHECK_FALSE(c.in_conflict(0, 1));
        CHECK(c.add(0, 1));
        CHECK(c.add(1, 2));
        CHECK_FALSE(c.add(1, 2));
        CHECK(c.has_conflict(0));
        CHECK(c.in_conflict(0, 1));
        CHECK(c.in_conflict(1, 2));
        CHECK(c.has_conflict(2));
        CHECK_FALSE(c.in_conflict(0, 2));
        // With same
        CHECK(c.add(5, 5));
        CHECK(c.has_conflict(5));
        CHECK(c.in_conflict(5, 5));
    }

    TEST_CASE("remove")
    {
        auto c = conflict_map<std::size_t>({ { 1, 1 }, { 1, 2 }, { 1, 3 }, { 2, 4 } });
        REQUIRE_EQ(c.size(), 4);

        REQUIRE(c.in_conflict(2, 4));
        REQUIRE(c.in_conflict(4, 2));
        CHECK(c.remove(2, 4));
        CHECK_FALSE(c.in_conflict(4, 2));
        CHECK_FALSE(c.in_conflict(2, 4));
        CHECK(c.has_conflict(2));
        CHECK_FALSE(c.has_conflict(4));

        CHECK_FALSE(c.remove(2, 4));

        CHECK(c.remove(1));
        CHECK_FALSE(c.has_conflict(1));
        CHECK_FALSE(c.in_conflict(1, 1));
        CHECK_FALSE(c.in_conflict(1, 2));
        CHECK_FALSE(c.in_conflict(3, 1));
    }
}

TEST_SUITE_BEGIN("satisfiability_error");

namespace
{
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
     * Simple factory for building a specs::PackageInfo.
     */
    auto mkpkg(std::string name, std::string version, std::vector<std::string> dependencies = {})
        -> specs::PackageInfo
    {
        auto pkg = specs::PackageInfo(std::move(name));
        pkg.version = std::move(version);
        pkg.depends = std::move(dependencies);
        pkg.build_string = "bld";
        return pkg;
    }

    /**
     * Create the repodata.json file containing the package information.
     */
    template <typename PkgRange>
    auto create_repodata_json(fs::u8path dir, const PkgRange& packages) -> fs::u8path
    {
        namespace nl = nlohmann;

        auto packages_j = nl::json::object();
        for (const auto& pkg : packages)
        {
            auto fname = fmt::format("{}-{}-{}.tar.bz2", pkg.name, pkg.version, pkg.build_string);
            packages_j[std::move(fname)] = nl::json(pkg);
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
    auto create_problem(
        Context& ctx,
        ChannelContext& channel_context,
        const PkgRange& packages,
        const std::vector<std::string>& specs
    )
    {
        const auto tmp_dir = dir_guard(
            fs::temp_directory_path() / "mamba/tests" / util::generate_random_alphanumeric_string(20)
        );
        const auto repodata_f = create_repodata_json(tmp_dir.path, packages);

        auto pool = MPool{ ctx, channel_context };
        pool.add_repo_from_repodata_json(repodata_f, "some-url");
        auto solver = MSolver(
            std::move(pool),
            std::vector{ std::pair{ SOLVER_FLAG_ALLOW_DOWNGRADE, 1 } }
        );
        solver.add_jobs(specs, SOLVER_INSTALL);

        return solver;
    }
}

TEST_CASE("Test create_problem utility")
{
    auto& ctx = mambatests::context();
    auto channel_context = ChannelContext::make_conda_compatible(ctx);
    auto solver = create_problem(ctx, channel_context, std::array{ mkpkg("foo", "0.1.0", {}) }, { "foo" });
    const auto solved = solver.try_solve();
    REQUIRE(solved);
}

TEST_CASE("Test empty specs")
{
    auto& ctx = mambatests::context();
    auto channel_context = ChannelContext::make_conda_compatible(ctx);
    auto solver = create_problem(
        ctx,
        channel_context,
        std::array{ mkpkg("foo", "0.1.0", {}), mkpkg("", "", {}) },
        { "foo" }
    );
    const auto solved = solver.try_solve();
    REQUIRE(solved);
}

namespace
{
    auto create_basic_conflict(Context& ctx, ChannelContext& channel_context) -> MSolver
    {
        return create_problem(
            ctx,
            channel_context,
            std::array{
                mkpkg("A", "0.1.0"),
                mkpkg("A", "0.2.0"),
                mkpkg("A", "0.3.0"),
            },
            { "A=0.4.0" }
        );
    }

    /**
     * Create the PubGrub blog post example.
     *
     * The example given by Natalie Weizenbaum
     * (credits https://nex3.medium.com/pubgrub-2fb6470504f).
     */
    auto create_pubgrub(Context& ctx, ChannelContext& channel_context) -> MSolver
    {
        return create_problem(
            ctx,
            channel_context,
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
            { "menu", "icons=1.*", "intl=5.*" }
        );
    }

    auto create_pubgrub_hard_(Context& ctx, ChannelContext& channel_context, bool missing_package)
        -> MSolver
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
        return create_problem(
            ctx,
            channel_context,
            packages,
            { "menu", "pyicons=1.*", "intl=5.*", "intl-mod", "pretty>=1.0" }
        );
    }

    /**
     * A harder version of ``create_pubgrub``.
     */
    auto create_pubgrub_hard(Context& ctx, ChannelContext& channel_context) -> MSolver
    {
        return create_pubgrub_hard_(ctx, channel_context, false);
    }

    /**
     * The hard version of the alternate PubGrub with missing packages.
     */
    auto create_pubgrub_missing(Context& ctx, ChannelContext& channel_context) -> MSolver
    {
        return create_pubgrub_hard_(ctx, channel_context, true);
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

    auto
    make_platform_channels(std::vector<std::string>&& channels, const std::vector<std::string>& platforms)
        -> std::vector<std::string>
    {
        auto add_plat = [&platforms](const auto& chan)
        { return fmt::format("{}[{}]", chan, fmt::join(platforms, ",")); };
        std::transform(channels.begin(), channels.end(), channels.begin(), add_plat);
        return std::move(channels);
    }

    /**
     * Mock of channel_loader.hpp:load_channels that takes a list of channels.
     */
    auto
    load_channels(Context& ctx, MPool& pool, MultiPackageCache& cache, std::vector<std::string>&& channels)
    {
        auto sub_dirs = std::vector<SubdirData>();
        for (const auto& location : channels)
        {
            for (const auto& chan : pool.channel_context().make_channel(location))
            {
                for (const auto& platform : chan.platforms())
                {
                    auto sub_dir = expected_value_or_throw(SubdirData::create(
                        ctx,
                        pool.channel_context(),
                        chan,
                        platform,
                        chan.platform_url(platform).str(),
                        cache
                    ));
                    sub_dirs.push_back(std::move(sub_dir));
                }
            }
        }

        SubdirData::download_indexes(sub_dirs, mambatests::context());

        for (auto& sub_dir : sub_dirs)
        {
            auto repo = load_subdir_in_pool(ctx, pool, sub_dir);
        }
    }

    /**
     * Create a solver and a pool of a conflict from conda-forge packages.
     */
    auto create_conda_forge(
        Context& ctx,
        ChannelContext& channel_context,
        std::vector<std::string>&& specs,
        const std::vector<specs::PackageInfo>& virtual_packages = { mkpkg("__glibc", "2.17.0") },
        std::vector<std::string>&& channels = { "conda-forge" },
        const std::vector<std::string>& platforms = { "linux-64", "noarch" }
    ) -> MSolver
    {
        // Reusing the cache for all invocation of this funciton for speedup
        static const auto tmp_dir = dir_guard(
            fs::temp_directory_path() / "mamba/tests" / util::generate_random_alphanumeric_string(20)
        );

        auto prefix_data = expected_value_or_throw(
            PrefixData::create(tmp_dir.path / "prefix", channel_context)
        );
        prefix_data.add_packages(virtual_packages);
        auto pool = MPool{ ctx, channel_context };

        load_installed_packages_in_pool(ctx, pool, prefix_data);

        auto cache = MultiPackageCache({ tmp_dir.path / "cache" }, ctx.validation_params);
        create_cache_dir(cache.first_writable_path());

        bool prev_progress_bars_value = ctx.graphics_params.no_progress_bars;
        ctx.graphics_params.no_progress_bars = true;
        load_channels(ctx, pool, cache, make_platform_channels(std::move(channels), platforms));
        ctx.graphics_params.no_progress_bars = prev_progress_bars_value;

        auto solver = MSolver(
            std::move(pool),
            std::vector{ std::pair{ SOLVER_FLAG_ALLOW_DOWNGRADE, 1 } }
        );
        solver.add_jobs(specs, SOLVER_INSTALL);

        return solver;
    }
}

TEST_CASE("Test create_conda_forge utility")
{
    auto& ctx = mambatests::context();
    auto channel_context = ChannelContext::make_conda_compatible(ctx);
    auto solver = create_conda_forge(ctx, channel_context, { "xtensor>=0.7" });
    const auto solved = solver.try_solve();
    REQUIRE(solved);
}

namespace
{
    auto create_pytorch_cpu(Context& ctx, ChannelContext& channel_context) -> MSolver
    {
        return create_conda_forge(ctx, channel_context, { "python=2.7", "pytorch=1.12" });
    }

    auto create_pytorch_cuda(Context& ctx, ChannelContext& channel_context) -> MSolver
    {
        return create_conda_forge(
            ctx,
            channel_context,
            { "python=2.7", "pytorch=1.12" },
            { mkpkg("__glibc", "2.17.0"), mkpkg("__cuda", "10.2.0") }
        );
    }

    auto create_cudatoolkit(Context& ctx, ChannelContext& channel_context) -> MSolver
    {
        return create_conda_forge(
            ctx,
            channel_context,
            { "python=3.7", "cudatoolkit=11.1", "cudnn=8.0", "pytorch=1.8", "torchvision=0.9=*py37_cu111*" },
            { mkpkg("__glibc", "2.17.0"), mkpkg("__cuda", "11.1") }
        );
    }

    auto create_jpeg9b(Context& ctx, ChannelContext& channel_context) -> MSolver
    {
        return create_conda_forge(ctx, channel_context, { "python=3.7", "jpeg=9b" });
    }

    auto create_r_base(Context& ctx, ChannelContext& channel_context) -> MSolver
    {
        return create_conda_forge(
            ctx,
            channel_context,
            { "r-base=3.5.* ", "pandas=0", "numpy<1.20.0", "matplotlib=2", "r-matchit=4.*" }
        );
    }

    auto create_scip(Context& ctx, ChannelContext& channel_context) -> MSolver
    {
        return create_conda_forge(ctx, channel_context, { "scip=8.*", "pyscipopt<4.0" });
    }

    auto create_double_python(Context& ctx, ChannelContext& channel_context) -> MSolver
    {
        return create_conda_forge(ctx, channel_context, { "python=3.9.*", "python=3.10.*" });
    }

    auto create_numba(Context& ctx, ChannelContext& channel_context) -> MSolver
    {
        return create_conda_forge(ctx, channel_context, { "python=3.11", "numba<0.56" });
    }

    template <typename NodeVariant>
    auto is_virtual_package(const NodeVariant& node) -> bool
    {
        return std::visit(
            [](const auto& n) -> bool
            {
                using Node = std::decay_t<decltype(n)>;
                if constexpr (std::is_same_v<Node, ProblemsGraph::RootNode>)
                {
                    return false;
                }
                else if constexpr (std::is_same_v<Node, ProblemsGraph::UnresolvedDependencyNode> || std::is_same_v<Node, ProblemsGraph::ConstraintNode>)
                {
                    return util::starts_with(std::invoke(&Node::name, n).str(), "__");
                }
                else
                {
                    return util::starts_with(std::invoke(&Node::name, n), "__");
                }
            },
            node
        );
    };
}

TEST_CASE("NamedList")
{
    auto l = CompressedProblemsGraph::PackageListNode();
    static constexpr std::size_t n_packages = 9;
    for (std::size_t minor = 1; minor <= n_packages; ++minor)
    {
        l.insert({ mkpkg("pkg", fmt::format("0.{}.0", minor)) });
    }
    CHECK_EQ(l.size(), n_packages);
    CHECK_EQ(l.name(), "pkg");
    {
        auto [str, size] = l.versions_trunc(", ", "...", 5);
        CHECK_EQ(size, 9);
        CHECK_EQ(str, "0.1.0, 0.2.0, ..., 0.9.0");
    }
    {
        auto [str, size] = l.build_strings_trunc(", ", "...", 5, false);
        CHECK_EQ(size, 9);
        CHECK_EQ(str, "bld, bld, ..., bld");
    }
    {
        auto [str, size] = l.build_strings_trunc(", ", "...", 5, true);
        CHECK_EQ(size, 1);
        CHECK_EQ(str, "bld");
    }
    {
        auto [str, size] = l.versions_and_build_strings_trunc("|", "---", 5);
        CHECK_EQ(size, 9);
        CHECK_EQ(str, "0.1.0 bld|0.2.0 bld|---|0.9.0 bld");
    }
}

TEST_CASE("Create problem graph")
{
    using PbGr = ProblemsGraph;
    using CpPbGr = CompressedProblemsGraph;

    const auto issues = std::array{
        std::pair{ "Basic conflict", &create_basic_conflict },
        std::pair{ "PubGrub example", &create_pubgrub },
        std::pair{ "Harder PubGrub example", &create_pubgrub_hard },
        std::pair{ "PubGrub example with missing packages", &create_pubgrub_missing },
        std::pair{ "PyTorch CPU", &create_pytorch_cpu },
        std::pair{ "PyTorch Cuda", &create_pytorch_cuda },
        std::pair{ "Cuda Toolkit", &create_cudatoolkit },
        std::pair{ "Jpeg", &create_jpeg9b },
        std::pair{ "R base", &create_r_base },
        std::pair{ "SCIP", &create_scip },
        std::pair{ "Two different Python", &create_double_python },
        std::pair{ "Numba", &create_numba },
    };

    for (const auto& [name, factory] : issues)
    {
        auto& ctx = mambatests::context();
        auto channel_context = ChannelContext::make_conda_compatible(ctx);

        // Somehow the capture does not work directly on ``name``
        std::string_view name_copy = name;
        CAPTURE(name_copy);
        auto solver = factory(ctx, channel_context);
        const auto solved = solver.try_solve();
        REQUIRE_FALSE(solved);
        const auto pbs_init = solver.problems_graph();
        const auto& graph_init = pbs_init.graph();

        REQUIRE_GE(graph_init.number_of_nodes(), 1);
        graph_init.for_each_node_id(
            [&](auto id)
            {
                const auto& node = graph_init.node(id);
                // Currently we do not make assumption about virtual package since
                // we are not sure we are including them the same way than they would be in
                // practice
                if (!is_virtual_package(node))
                {
                    if (graph_init.in_degree(id) == 0)
                    {
                        // Only one root node
                        CHECK_EQ(id, pbs_init.root_node());
                        CHECK(std::holds_alternative<PbGr::RootNode>(node));
                    }
                    else if (graph_init.out_degree(id) == 0)
                    {
                        CHECK_FALSE(std::holds_alternative<PbGr::RootNode>(node));
                    }
                    else
                    {
                        CHECK(std::holds_alternative<PbGr::PackageNode>(node));
                    }
                    // All nodes reachable from the root
                    CHECK(is_reachable(pbs_init.graph(), pbs_init.root_node(), id));
                }
            }
        );

        const auto& conflicts_init = pbs_init.conflicts();
        for (const auto& [n, _] : conflicts_init)
        {
            bool tmp = std::holds_alternative<PbGr::PackageNode>(graph_init.node(n))
                       || std::holds_alternative<PbGr::ConstraintNode>(graph_init.node(n));
            CHECK(tmp);
        }

        SUBCASE("Simplify conflicts")
        {
            const auto& pbs_simplified = simplify_conflicts(pbs_init);
            const auto& graph_simplified = pbs_simplified.graph();

            REQUIRE_GE(graph_simplified.number_of_nodes(), 1);
            REQUIRE_LE(graph_simplified.number_of_nodes(), pbs_init.graph().number_of_nodes());

            for (const auto& [id, _] : pbs_simplified.conflicts())
            {
                const auto& node = graph_simplified.node(id);
                // Currently we do not make assumption about virtual package since
                // we are not sure we are including them the same way than they would be in
                // practice
                if (!is_virtual_package(node))
                {
                    CHECK(graph_simplified.has_node(id));
                    // Unfortunately not all conflicts are on leaves
                    // CHECK_EQ(graph_simplified.out_degree(id), 0);
                    CHECK(is_reachable(graph_simplified, pbs_simplified.root_node(), id));
                }
            }

            SUBCASE("Compress graph")
            {
                const auto pbs_comp = CpPbGr::from_problems_graph(pbs_simplified);
                const auto& graph_comp = pbs_comp.graph();

                REQUIRE_GE(pbs_init.graph().number_of_nodes(), graph_comp.number_of_nodes());
                REQUIRE_GE(graph_comp.number_of_nodes(), 1);
                graph_comp.for_each_node_id(
                    [&](auto id)
                    {
                        const auto& node = graph_comp.node(id);
                        // Currently we do not make assumption about virtual package since
                        // we are not sure we are including them the same way than they
                        // would be in
                        if (!is_virtual_package(node))
                        {
                            if (graph_comp.in_degree(id) == 0)
                            {
                                // Only one root node
                                CHECK_EQ(id, pbs_init.root_node());
                                CHECK(std::holds_alternative<CpPbGr::RootNode>(node));
                            }
                            else if (graph_comp.out_degree(id) == 0)
                            {
                                CHECK_FALSE(std::holds_alternative<CpPbGr::RootNode>(node));
                            }
                            else
                            {
                                CHECK(std::holds_alternative<CpPbGr::PackageListNode>(node));
                            }
                            // All nodes reachable from the root
                            CHECK(is_reachable(graph_comp, pbs_comp.root_node(), id));
                        }
                    }
                );

                const auto& conflicts_comp = pbs_comp.conflicts();
                for (const auto& [n, _] : conflicts_comp)
                {
                    bool tmp = std::holds_alternative<CpPbGr::PackageListNode>(graph_comp.node(n))
                               || std::holds_alternative<CpPbGr::ConstraintListNode>(graph_comp.node(n
                               ));
                    CHECK(tmp);
                }

                SUBCASE("Compose error message")
                {
                    const auto message = problem_tree_msg(pbs_comp);

                    auto message_contains = [&message](const auto& node)
                    {
                        using Node = std::remove_cv_t<std::remove_reference_t<decltype(node)>>;
                        if constexpr (!std::is_same_v<Node, CpPbGr::RootNode>)
                        {
                            CHECK(util::contains(message, node.name()));
                        }
                    };

                    pbs_comp.graph().for_each_node_id(
                        [&message_contains, &g = pbs_comp.graph()](auto id)
                        {
                            std::visit(message_contains, g.node(id));  //
                        }
                    );
                }
            }
        }
    }
}

TEST_SUITE_END();
