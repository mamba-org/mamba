// Copyright (c) 2022, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <array>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_all.hpp>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <nlohmann/json.hpp>

#include "mamba/core/channel_context.hpp"
#include "mamba/core/package_database_loader.hpp"
#include "mamba/core/prefix_data.hpp"
#include "mamba/core/subdir_index.hpp"
#include "mamba/core/util.hpp"
#include "mamba/fs/filesystem.hpp"
#include "mamba/solver/libsolv/database.hpp"
#include "mamba/solver/libsolv/repo_info.hpp"
#include "mamba/solver/libsolv/solver.hpp"
#include "mamba/solver/libsolv/unsolvable.hpp"
#include "mamba/solver/problems_graph.hpp"
#include "mamba/specs/package_info.hpp"
#include "mamba/util/string.hpp"

#include "mambatests.hpp"

using namespace mamba;
using namespace mamba::solver;

namespace
{
    TEST_CASE("conflict_map::symmetric", "[mamba::solver]")
    {
        auto c = conflict_map<std::size_t>();
        REQUIRE(c.size() == 0);
        REQUIRE_FALSE(c.has_conflict(0));
        REQUIRE_FALSE(c.in_conflict(0, 1));
        REQUIRE(c.add(0, 1));
        REQUIRE(c.add(1, 2));
        REQUIRE_FALSE(c.add(1, 2));
        REQUIRE(c.has_conflict(0));
        REQUIRE(c.in_conflict(0, 1));
        REQUIRE(c.in_conflict(1, 2));
        REQUIRE(c.has_conflict(2));
        REQUIRE_FALSE(c.in_conflict(0, 2));
        // With same
        REQUIRE(c.add(5, 5));
        REQUIRE(c.has_conflict(5));
        REQUIRE(c.in_conflict(5, 5));
    }

    TEST_CASE("conflict_map::remove", "[mamba::solver]")
    {
        auto c = conflict_map<std::size_t>({ { 1, 1 }, { 1, 2 }, { 1, 3 }, { 2, 4 } });
        REQUIRE(c.size() == 4);

        REQUIRE(c.in_conflict(2, 4));
        REQUIRE(c.in_conflict(4, 2));
        REQUIRE(c.remove(2, 4));
        REQUIRE_FALSE(c.in_conflict(4, 2));
        REQUIRE_FALSE(c.in_conflict(2, 4));
        REQUIRE(c.has_conflict(2));
        REQUIRE_FALSE(c.has_conflict(4));

        REQUIRE_FALSE(c.remove(2, 4));

        REQUIRE(c.remove(1));
        REQUIRE_FALSE(c.has_conflict(1));
        REQUIRE_FALSE(c.in_conflict(1, 1));
        REQUIRE_FALSE(c.in_conflict(1, 2));
        REQUIRE_FALSE(c.in_conflict(3, 1));
    }
}

namespace
{
    using namespace mamba::specs::match_spec_literals;
    using Request = solver::Request;

    /**
     * Simple factory for building a specs::PackageInfo.
     */
    auto mkpkg(std::string name, std::string version, std::vector<std::string> dependencies = {})
        -> specs::PackageInfo
    {
        auto pkg = specs::PackageInfo(std::move(name));
        pkg.version = std::move(version);
        pkg.dependencies = std::move(dependencies);
        pkg.build_string = "bld";
        return pkg;
    }

    /**
     * Create a solver and a database of a conflict.
     *
     * The underlying packages do not exist, we are only interested in the conflict.
     */
    template <typename PkgRange>
    auto create_pkgs_database(ChannelContext& channel_context, const PkgRange& packages)
    {
        solver::libsolv::Database db{ channel_context.params() };
        db.add_repo_from_packages(packages);
        return db;
    }
}

TEST_CASE("Test create_pkgs_database utility", "[mamba::solver]")
{
    auto& ctx = mambatests::context();
    auto channel_context = ChannelContext::make_conda_compatible(ctx);
    auto db = create_pkgs_database(channel_context, std::array{ mkpkg("foo", "0.1.0", {}) });
    auto request = Request{ {}, { Request::Install{ "foo"_ms } } };
    const auto outcome = solver::libsolv::Solver().solve(db, request).value();
    REQUIRE(std::holds_alternative<solver::Solution>(outcome));
}

TEST_CASE("Test empty specs", "[mamba::solver]")
{
    auto& ctx = mambatests::context();
    auto channel_context = ChannelContext::make_conda_compatible(ctx);
    auto db = create_pkgs_database(
        channel_context,
        std::array{ mkpkg("foo", "0.1.0", {}), mkpkg("", "", {}) }
    );
    auto request = Request{ {}, { Request::Install{ "foo"_ms } } };
    const auto outcome = solver::libsolv::Solver().solve(db, request).value();
    REQUIRE(std::holds_alternative<solver::Solution>(outcome));
}

namespace
{
    auto create_basic_conflict(Context&, ChannelContext& channel_context)
    {
        return std::pair(
            create_pkgs_database(
                channel_context,
                std::array{
                    mkpkg("A", "0.1.0"),
                    mkpkg("A", "0.2.0"),
                    mkpkg("A", "0.3.0"),
                }
            ),
            Request{ {}, { Request::Install{ "A=0.4.0"_ms } } }
        );
    }

    /**
     * Create the PubGrub blog post example.
     *
     * The example given by Natalie Weizenbaum
     * (credits https://nex3.medium.com/pubgrub-2fb6470504f).
     */
    auto create_pubgrub(Context&, ChannelContext& channel_context)
    {
        return std::pair(
            create_pkgs_database(
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
                }
            ),
            Request{
                {},
                {
                    Request::Install{ "menu"_ms },
                    Request::Install{ "icons=1.*"_ms },
                    Request::Install{ "intl=5.*"_ms },
                },
            }
        );
    }

    auto create_pubgrub_hard_(Context&, ChannelContext& channel_context, bool missing_package)
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
        return std::pair(
            create_pkgs_database(channel_context, packages),
            Request{
                {},
                {
                    Request::Install{ "menu"_ms },
                    Request::Install{ "pyicons=1.*"_ms },
                    Request::Install{ "intl=5.*"_ms },
                    Request::Install{ "intl-mod"_ms },
                    Request::Install{ "pretty>=1.0"_ms },
                },
            }
        );
    }

    /**
     * A harder version of ``create_pubgrub``.
     */
    auto create_pubgrub_hard(Context& ctx, ChannelContext& channel_context)
    {
        return create_pubgrub_hard_(ctx, channel_context, false);
    }

    /**
     * The hard version of the alternate PubGrub with missing packages.
     */
    auto create_pubgrub_missing(Context& ctx, ChannelContext& channel_context)
    {
        return create_pubgrub_hard_(ctx, channel_context, true);
    }

    /**
     * Create a conflict due to a pin.
     */
    auto create_pin_conflict(Context&, ChannelContext& channel_context)
    {
        return std::pair(
            create_pkgs_database(
                channel_context,
                std::array{
                    mkpkg("foo", "2.0.0", { "bar=2.0" }),
                    mkpkg("bar", "1.0.0"),
                    mkpkg("bar", "2.0.0"),
                }
            ),
            Request{
                {},
                {
                    Request::Install{ "foo"_ms },
                    Request::Pin{ "bar=1.0"_ms },
                },
            }
        );
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

    /*
     * Mock of channel_loader.cpp:create_mirrors
     * TODO: factorize that code
     */
    void create_mirrors(Context& ctx, const specs::Channel& channel)
    {
        if (!ctx.mirrors.has_mirrors(channel.id()))
        {
            for (const specs::CondaURL& url : channel.mirror_urls())
            {
                ctx.mirrors.add_unique_mirror(
                    channel.id(),
                    download::make_mirror(url.str(specs::CondaURL::Credentials::Show))
                );
            }
        }
    }

    /**
     * Mock of channel_loader.hpp:load_channels that takes a list of channels.
     */
    auto load_channels(
        Context& ctx,
        ChannelContext& channel_context,
        solver::libsolv::Database& database,
        MultiPackageCache& cache,
        std::vector<std::string>&& channels
    )
    {
        auto sub_dirs = std::vector<SubdirIndexLoader>();
        for (const auto& location : channels)
        {
            for (const auto& chan : channel_context.make_channel(location))
            {
                create_mirrors(ctx, chan);
                for (const auto& platform : chan.platforms())
                {
                    auto sub_dir = SubdirIndexLoader::create(
                                       ctx.subdir_params(),
                                       channel_context,
                                       chan,
                                       platform,
                                       cache
                    )
                                       .value();
                    sub_dirs.push_back(std::move(sub_dir));
                }
            }
        }

        const auto result = SubdirIndexLoader::download_required_indexes(
            sub_dirs,
            mambatests::context().subdir_params(),
            mambatests::context().authentication_info(),
            mambatests::context().mirrors,
            mambatests::context().download_options(),
            mambatests::context().remote_fetch_params
        );
        REQUIRE(result.has_value());

        for (auto& sub_dir : sub_dirs)
        {
            auto repo = load_subdir_in_database(ctx, database, sub_dir);
        }
    }

    /**
     * Create a solver and a database of a conflict from conda-forge packages.
     */
    auto create_conda_forge_database(
        Context& ctx,
        ChannelContext& channel_context,
        const std::vector<specs::PackageInfo>& virtual_packages = { mkpkg("__glibc", "2.17.0") },
        std::vector<std::string>&& channels = { "conda-forge" },
        const std::vector<std::string>& platforms = { "linux-64", "noarch" }
    )
    {
        // Reusing the cache for all invocations of this function for speedup

        static const auto tmp_dir = TemporaryDirectory();

        auto prefix_data = PrefixData::create(tmp_dir.path() / "prefix", channel_context).value();
        prefix_data.add_packages(virtual_packages);
        auto db = solver::libsolv::Database{ channel_context.params() };

        load_installed_packages_in_database(ctx, db, prefix_data);

        auto cache = MultiPackageCache({ tmp_dir.path() / "cache" }, ctx.validation_params);
        create_cache_dir(cache.first_writable_path());

        bool prev_progress_bars_value = ctx.graphics_params.no_progress_bars;
        ctx.graphics_params.no_progress_bars = true;
        load_channels(
            ctx,
            channel_context,
            db,
            cache,
            make_platform_channels(std::move(channels), platforms)
        );
        ctx.graphics_params.no_progress_bars = prev_progress_bars_value;

        return db;
    }
}

TEST_CASE("Test create_conda_forge utility", "[mamba::solver]")
{
    auto& ctx = mambatests::context();
    auto channel_context = ChannelContext::make_conda_compatible(ctx);
    auto db = create_conda_forge_database(ctx, channel_context);
    auto request = Request{ {}, { Request::Install{ "xtensor>=0.7"_ms } } };
    const auto outcome = solver::libsolv::Solver().solve(db, request).value();
    REQUIRE(std::holds_alternative<solver::Solution>(outcome));
}

namespace
{
    auto create_pytorch_cpu(Context& ctx, ChannelContext& channel_context)
    {
        return std::pair(
            create_conda_forge_database(ctx, channel_context),
            Request{
                {},
                {
                    Request::Install{ "python=2.7"_ms },
                    Request::Install{ "pytorch=1.12"_ms },
                },
            }
        );
    }

    auto create_pytorch_cuda(Context& ctx, ChannelContext& channel_context)
    {
        return std::pair(
            create_conda_forge_database(
                ctx,
                channel_context,
                { mkpkg("__glibc", "2.17.0"), mkpkg("__cuda", "10.2.0") }
            ),
            Request{
                {},
                {
                    Request::Install{ "python=2.7"_ms },
                    Request::Install{ "pytorch=1.12"_ms },
                },
            }
        );
    }

    auto create_cudatoolkit(Context& ctx, ChannelContext& channel_context)
    {
        return std::pair(
            create_conda_forge_database(
                ctx,
                channel_context,
                { mkpkg("__glibc", "2.17.0"), mkpkg("__cuda", "11.1") }
            ),
            Request{
                {},
                {
                    Request::Install{ "python=3.7"_ms },
                    Request::Install{ "cudatoolkit=11.1"_ms },
                    Request::Install{ "cudnn=8.0"_ms },
                    Request::Install{ "pytorch=1.8"_ms },
                    Request::Install{ "torchvision=0.9=*py37_cu111*"_ms },
                },
            }
        );
    }

    auto create_jpeg9b(Context& ctx, ChannelContext& channel_context)
    {
        return std::pair(
            create_conda_forge_database(ctx, channel_context),
            Request{
                {},
                {
                    Request::Install{ "python=3.7"_ms },
                    Request::Install{ "jpeg=9b"_ms },
                },
            }
        );
    }

    auto create_r_base(Context& ctx, ChannelContext& channel_context)
    {
        return std::pair(
            create_conda_forge_database(ctx, channel_context),
            Request{
                {},
                {
                    Request::Install{ "r-base=3.5.* "_ms },
                    Request::Install{ "pandas=0"_ms },
                    Request::Install{ "numpy<1.20.0"_ms },
                    Request::Install{ "matplotlib=2"_ms },
                    Request::Install{ "r-matchit=4.*"_ms },
                },
            }
        );
    }

    auto create_scip(Context& ctx, ChannelContext& channel_context)
    {
        return std::pair(
            create_conda_forge_database(ctx, channel_context),
            Request{
                {},
                {
                    Request::Install{ "scip=8.*"_ms },
                    Request::Install{ "pyscipopt<4.0"_ms },
                },
            }
        );
    }

    auto create_double_python(Context& ctx, ChannelContext& channel_context)
    {
        return std::pair(
            create_conda_forge_database(ctx, channel_context),
            Request{
                {},
                {
                    Request::Install{ "python=3.9.*"_ms },
                    Request::Install{ "python=3.10.*"_ms },
                },
            }
        );
    }

    auto create_numba(Context& ctx, ChannelContext& channel_context)
    {
        return std::pair(
            create_conda_forge_database(ctx, channel_context),
            Request{
                {},
                {
                    Request::Install{ "python=3.11"_ms },
                    Request::Install{ "numba<0.56"_ms },
                },
            }
        );
    }

    auto create_sudoku(Context&, ChannelContext& channel_context)
    {
        auto db = solver::libsolv::Database{ channel_context.params() };
        const auto xpt = db.add_repo_from_repodata_json(
            mambatests::test_data_dir / "repodata/sudoku.json",
            "https://conda.anaconda.org/jjhelmus/label/sudoku/noarch/repodata.json",
            "sudoku"
        );

        auto request = Request{
            {},
            {
                Request::Install{ "sudoku_0_0 == 5"_ms }, Request::Install{ "sudoku_1_0 == 3"_ms },
                Request::Install{ "sudoku_4_0 == 7"_ms }, Request::Install{ "sudoku_0_1 == 6"_ms },
                Request::Install{ "sudoku_3_1 == 1"_ms }, Request::Install{ "sudoku_4_1 == 9"_ms },
                Request::Install{ "sudoku_5_1 == 5"_ms }, Request::Install{ "sudoku_1_2 == 9"_ms },
                Request::Install{ "sudoku_2_2 == 8"_ms }, Request::Install{ "sudoku_7_2 == 6"_ms },
                Request::Install{ "sudoku_0_3 == 8"_ms }, Request::Install{ "sudoku_4_3 == 6"_ms },
                Request::Install{ "sudoku_8_3 == 3"_ms }, Request::Install{ "sudoku_0_4 == 4"_ms },
                Request::Install{ "sudoku_3_4 == 8"_ms }, Request::Install{ "sudoku_5_4 == 3"_ms },
                Request::Install{ "sudoku_8_4 == 1"_ms }, Request::Install{ "sudoku_0_5 == 7"_ms },
                Request::Install{ "sudoku_4_5 == 2"_ms }, Request::Install{ "sudoku_8_5 == 6"_ms },
                Request::Install{ "sudoku_1_6 == 6"_ms }, Request::Install{ "sudoku_6_6 == 2"_ms },
                Request::Install{ "sudoku_7_6 == 8"_ms }, Request::Install{ "sudoku_3_7 == 4"_ms },
                Request::Install{ "sudoku_4_7 == 1"_ms }, Request::Install{ "sudoku_5_7 == 9"_ms },
                Request::Install{ "sudoku_8_7 == 5"_ms }, Request::Install{ "sudoku_4_8 == 8"_ms },
                Request::Install{ "sudoku_7_8 == 7"_ms }, Request::Install{ "sudoku_8_8 == 9"_ms },
            },
        };

        return std::pair(std::move(db), std::move(request));
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
                else if constexpr (std::is_same_v<Node, ProblemsGraph::UnresolvedDependencyNode>
                                   || std::is_same_v<Node, ProblemsGraph::ConstraintNode>)
                {
                    return util::starts_with(std::invoke(&Node::name, n).to_string(), "__");
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

TEST_CASE("NamedList", "[mamba::solver]")
{
    auto l = CompressedProblemsGraph::PackageListNode();
    static constexpr std::size_t n_packages = 9;
    for (std::size_t minor = 1; minor <= n_packages; ++minor)
    {
        l.insert({ mkpkg("pkg", fmt::format("0.{}.0", minor)) });
    }
    REQUIRE(l.size() == n_packages);
    REQUIRE(l.name() == "pkg");
    {
        auto [str, size] = l.versions_trunc(", ", "...", 5);
        REQUIRE(size == 9);
        REQUIRE(str == "0.1.0, 0.2.0, ..., 0.9.0");
    }
    {
        auto [str, size] = l.build_strings_trunc(", ", "...", 5, false);
        REQUIRE(size == 9);
        REQUIRE(str == "bld, bld, ..., bld");
    }
    {
        auto [str, size] = l.build_strings_trunc(", ", "...", 5, true);
        REQUIRE(size == 1);
        REQUIRE(str == "bld");
    }
    {
        auto [str, size] = l.versions_and_build_strings_trunc("|", "---", 5);
        REQUIRE(size == 9);
        REQUIRE(str == "0.1.0 bld|0.2.0 bld|---|0.9.0 bld");
    }
}

TEST_CASE("Create problem graph", "[mamba::solver]")
{
    using PbGr = ProblemsGraph;
    using CpPbGr = CompressedProblemsGraph;

    const auto [name, factory] = GENERATE(
        std::pair{ "Basic conflict", &create_basic_conflict },
        std::pair{ "PubGrub example", &create_pubgrub },
        std::pair{ "Harder PubGrub example", &create_pubgrub_hard },
        std::pair{ "PubGrub example with missing packages", &create_pubgrub_missing },
        std::pair{ "Pin conflict", &create_pin_conflict },
        std::pair{ "PyTorch CPU", &create_pytorch_cpu },
        std::pair{ "PyTorch Cuda", &create_pytorch_cuda },
        std::pair{ "Cuda Toolkit", &create_cudatoolkit },
        std::pair{ "Jpeg", &create_jpeg9b },
        std::pair{ "R base", &create_r_base },
        std::pair{ "SCIP", &create_scip },
        std::pair{ "Two different Python", &create_double_python },
        std::pair{ "Numba", &create_numba },
        std::pair{ "Sudoku", &create_sudoku }
    );

    auto& ctx = mambatests::context();
    auto channel_context = ChannelContext::make_conda_compatible(ctx);

    // Somehow the capture does not work directly on ``name``
    std::string_view name_copy = name;
    CAPTURE(name_copy);
    auto [db, request] = factory(ctx, channel_context);
    auto outcome = solver::libsolv::Solver().solve(db, request).value();
    REQUIRE(std::holds_alternative<solver::libsolv::UnSolvable>(outcome));
    auto& unsolvable = std::get<solver::libsolv::UnSolvable>(outcome);
    const auto pbs_init = unsolvable.problems_graph(db);
    const auto& graph_init = pbs_init.graph();

    REQUIRE(graph_init.number_of_nodes() >= 1);
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
                    REQUIRE(id == pbs_init.root_node());
                    REQUIRE(std::holds_alternative<PbGr::RootNode>(node));
                }
                else if (graph_init.out_degree(id) == 0)
                {
                    REQUIRE_FALSE(std::holds_alternative<PbGr::RootNode>(node));
                }
                else
                {
                    REQUIRE(std::holds_alternative<PbGr::PackageNode>(node));
                }
                // All nodes reachable from the root
                REQUIRE(is_reachable(pbs_init.graph(), pbs_init.root_node(), id));
            }
        }
    );

    const auto& conflicts_init = pbs_init.conflicts();
    for (const auto& [n, _] : conflicts_init)
    {
        bool tmp = std::holds_alternative<PbGr::PackageNode>(graph_init.node(n))
                   || std::holds_alternative<PbGr::ConstraintNode>(graph_init.node(n));
        REQUIRE(tmp);
    }

    SECTION("Simplify conflicts")
    {
        const auto& pbs_simplified = simplify_conflicts(pbs_init);
        const auto& graph_simplified = pbs_simplified.graph();

        REQUIRE(graph_simplified.number_of_nodes() >= 1);
        REQUIRE(graph_simplified.number_of_nodes() <= pbs_init.graph().number_of_nodes());

        for (const auto& [id, _] : pbs_simplified.conflicts())
        {
            const auto& node = graph_simplified.node(id);
            // Currently we do not make assumption about virtual package since
            // we are not sure we are including them the same way than they would be in
            // practice
            if (!is_virtual_package(node))
            {
                REQUIRE(graph_simplified.has_node(id));
                // Unfortunately not all conflicts are on leaves
                // REQUIRE(graph_simplified.out_degree(id) == 0);
                REQUIRE(is_reachable(graph_simplified, pbs_simplified.root_node(), id));
            }
        }

        SECTION("Compress graph")
        {
            const auto pbs_comp = CpPbGr::from_problems_graph(pbs_simplified);
            const auto& graph_comp = pbs_comp.graph();

            REQUIRE(pbs_init.graph().number_of_nodes() >= graph_comp.number_of_nodes());
            REQUIRE(graph_comp.number_of_nodes() >= 1);
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
                            REQUIRE(id == pbs_init.root_node());
                            REQUIRE(std::holds_alternative<CpPbGr::RootNode>(node));
                        }
                        else if (graph_comp.out_degree(id) == 0)
                        {
                            REQUIRE_FALSE(std::holds_alternative<CpPbGr::RootNode>(node));
                        }
                        else
                        {
                            REQUIRE(std::holds_alternative<CpPbGr::PackageListNode>(node));
                        }
                        // All nodes reachable from the root
                        REQUIRE(is_reachable(graph_comp, pbs_comp.root_node(), id));
                    }
                }
            );

            const auto& conflicts_comp = pbs_comp.conflicts();
            for (const auto& [n, _] : conflicts_comp)
            {
                bool tmp = std::holds_alternative<CpPbGr::PackageListNode>(graph_comp.node(n))
                           || std::holds_alternative<CpPbGr::ConstraintListNode>(graph_comp.node(n));
                REQUIRE(tmp);
            }

            SECTION("Compose error message")
            {
                const auto message = problem_tree_msg(pbs_comp);

                auto message_contains = [&message, &name_copy](const auto& node)
                {
                    using Node = std::remove_cv_t<std::remove_reference_t<decltype(node)>>;
                    if constexpr (!std::is_same_v<Node, CpPbGr::RootNode>)
                    {
                        if ((name_copy == "Pin conflict") && util::contains(node.name(), "pin on"))
                        {
                            return;
                        }
                        REQUIRE(util::contains(message, node.name()));
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
