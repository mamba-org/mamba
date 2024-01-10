// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <string_view>

#include <fmt/format.h>
#include <solv/evr.h>
#include <solv/pool.h>
#include <solv/selection.h>
#include <solv/solver.h>

#include "mamba/core/subdirdata.hpp"
#include "mamba/util/string.hpp"
extern "C"  // Incomplete header
{
#include <solv/conda.h>
}
#include <spdlog/spdlog.h>

#include "mamba/core/channel_context.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/pool.hpp"
#include "mamba/core/repo.hpp"
#include "mamba/specs/match_spec.hpp"
#include "solv-cpp/pool.hpp"
#include "solv-cpp/queue.hpp"

namespace mamba
{
    struct MPool::MPoolData
    {
        MPoolData(Context& ctx, ChannelContext& cc)
            : context(ctx)
            , channel_context(cc)
        {
        }

        solv::ObjPool pool = {};
        Context& context;
        ChannelContext& channel_context;
    };

    MPool::MPool(Context& ctx, ChannelContext& channel_context)
        : m_data(std::make_shared<MPoolData>(ctx, channel_context))
    {
        pool().set_disttype(DISTTYPE_CONDA);
        set_debuglevel();
    }

    MPool::~MPool() = default;

    ChannelContext& MPool::channel_context() const
    {
        return m_data->channel_context;
    }

    const Context& MPool::context() const
    {
        return m_data->context;
    }

    solv::ObjPool& MPool::pool()
    {
        return m_data->pool;
    }

    const solv::ObjPool& MPool::pool() const
    {
        return m_data->pool;
    }

    void MPool::set_debuglevel()
    {
        // ensure that debug logging goes to stderr as to not interfere with stdout json output
        pool().raw()->debugmask |= SOLV_DEBUG_TO_STDERR;
        const auto& ctx = context();
        if (ctx.output_params.verbosity > 2)
        {
            pool_setdebuglevel(pool().raw(), ctx.output_params.verbosity - 1);
            pool().set_debug_callback(
                [logger = spdlog::get("libsolv"), &ctx](::Pool*, int type, std::string_view msg) noexcept
                {
                    if (msg.size() == 0 || msg.back() != '\n')
                    {
                        return;
                    }

                    auto log = Console::hide_secrets(msg);
                    if (type & SOLV_FATAL || type & SOLV_ERROR)
                    {
                        logger->error(log);
                    }
                    else if (type & SOLV_WARN)
                    {
                        logger->warn(log);
                    }
                    else if (ctx.output_params.verbosity > 2)
                    {
                        logger->info(log);
                    }
                }
            );
        }
    }

    void MPool::create_whatprovides()
    {
        pool().create_whatprovides();
    }

    MPool::operator Pool*()
    {
        return pool().raw();
    }

    MPool::operator const Pool*() const
    {
        return pool().raw();
    }

    std::vector<Id> MPool::select_solvables(Id matchspec, bool sorted) const
    {
        auto solvables = pool().select_solvables({ SOLVER_SOLVABLE_PROVIDES, matchspec });

        if (sorted)
        {
            std::sort(
                solvables.begin(),
                solvables.end(),
                [pool_ptr = pool().raw()](Id a, Id b)
                {
                    Solvable* sa = pool_id2solvable(pool_ptr, a);
                    Solvable* sb = pool_id2solvable(pool_ptr, b);
                    return (pool_evrcmp(pool_ptr, sa->evr, sb->evr, EVRCMP_COMPARE) > 0);
                }
            );
        }
        return solvables.as<std::vector>();
    }

    namespace
    {
        auto
        channel_match(const std::vector<specs::Channel>& ms_channels, const specs::CondaURL& pkg_url)
            -> specs::Channel::Match
        {
            auto match = specs::Channel::Match::No;
            // More than one element means the channel spec was a custom_multi_channel
            for (const auto& chan : ms_channels)
            {
                switch (chan.contains_package(pkg_url))
                {
                    case specs::Channel::Match::Full:
                        return specs::Channel::Match::Full;
                    case specs::Channel::Match::InOtherPlatform:
                        // Keep looking for full matches
                        match = specs::Channel::Match::InOtherPlatform;
                        break;
                    case specs::Channel::Match::No:
                        // No overriding potential InOtherPlatform match
                        break;
                }
            }
            return match;
        }

        /**
         * Add function to handle matchspec while parsing is done by libsolv.
         */
        auto add_channel_specific_matchspec(
            ChannelContext& channel_context,
            solv::ObjPool& pool,
            const specs::MatchSpec& ms
        ) -> solv::DependencyId
        {
            assert(ms.channel().has_value());
            // Poor man's ms repr to match waht the user provided
            const std::string repr = fmt::format("{}::{}", *ms.channel(), ms.conda_build_form());

            // Already added, return that id
            if (const auto maybe_id = pool.find_string(repr))
            {
                return maybe_id.value();
            }

            // conda_build_form does **NOT** contain the channel info
            const solv::DependencyId match = pool_conda_matchspec(
                pool.raw(),
                ms.conda_build_form().c_str()
            );

            auto ms_channels = channel_context.make_channel(*ms.channel());

            solv::ObjQueue selected_pkgs = {};
            auto other_subdir_match = std::string();
            pool.for_each_whatprovides(
                match,
                [&](solv::ObjSolvableViewConst s)
                {
                    if (s.installed())
                    {
                        // This will have the effect that channel-specific MatchSpec will always be
                        // reinstalled.
                        // This is not the intended behaviour but an historical artifact on which
                        // ``--force-reinstall`` currently rely.
                        return;
                    }

                    assert(ms.channel().has_value());
                    const auto match = channel_match(ms_channels, specs::CondaURL::parse(s.url()));
                    switch (match)
                    {
                        case (specs::Channel::Match::Full):
                        {
                            selected_pkgs.push_back(s.id());
                            break;
                        }
                        case (specs::Channel::Match::InOtherPlatform):
                        {
                            other_subdir_match = s.subdir();
                            break;
                        }
                        case (specs::Channel::Match::No):
                        {
                            break;
                        }
                    }
                }
            );

            if (selected_pkgs.empty())
            {
                if (!other_subdir_match.empty())
                {
                    const auto& filters = ms.channel()->platform_filters();
                    throw std::runtime_error(fmt::format(
                        R"(The package "{}" is not available for the specified platform{} ({}))"
                        R"( but is available on {}.)",
                        ms.str(),
                        filters.size() > 1 ? "s" : "",
                        fmt::join(filters, ", "),
                        other_subdir_match
                    ));
                }
                else
                {
                    throw std::runtime_error(fmt::format(
                        R"(The package "{}" is not found in any loaded channels.)"
                        R"( Try adding more channels or subdirs.)",
                        ms.str()
                    ));
                }
            }

            const solv::StringId repr_id = pool.add_string(repr);
            // FRAGILE This get deleted when calling ``pool_createwhatprovides`` so care
            // must be taken to do it before
            // TODO investigate namespace providers
            pool.add_to_whatprovides(repr_id, pool.add_to_whatprovides_data(selected_pkgs));
            return repr_id;
        }
    }

    ::Id MPool::matchspec2id(const specs::MatchSpec& ms)
    {
        ::Id id = 0;
        if (!ms.channel().has_value())
        {
            id = pool_conda_matchspec(pool().raw(), ms.conda_build_form().c_str());
        }
        else
        {
            // Working around shortcomings of ``pool_conda_matchspec``
            // The channels are not processed.
            // TODO Fragile! Installing this matchspec will always trigger a reinstall
            id = add_channel_specific_matchspec(channel_context(), pool(), ms);
        }
        if (id == 0)
        {
            throw std::runtime_error("libsolv error: could not create matchspec from string");
        }
        return id;
    }

    namespace
    {
        auto make_package_info(const solv::ObjPool& pool, solv::ObjSolvableViewConst s)
            -> specs::PackageInfo
        {
            specs::PackageInfo out = {};

            out.name = s.name();
            out.version = s.version();
            out.build_string = s.build_string();
            out.noarch = specs::noarch_parse(s.noarch()).value_or(specs::NoArchType::No);
            out.build_number = s.build_number();
            out.channel = s.channel();
            out.package_url = s.url();
            out.subdir = s.subdir();
            out.filename = s.file_name();
            out.license = s.license();
            out.size = s.size();
            out.timestamp = s.timestamp();
            out.md5 = s.md5();
            out.sha256 = s.sha256();

            const auto dep_to_str = [&pool](solv::DependencyId id)
            { return pool.dependency_to_string(id); };
            {
                const auto deps = s.dependencies();
                out.depends.reserve(deps.size());
                std::transform(deps.cbegin(), deps.cend(), std::back_inserter(out.depends), dep_to_str);
            }
            {
                const auto cons = s.constraints();
                out.constrains.reserve(cons.size());
                std::transform(cons.cbegin(), cons.cend(), std::back_inserter(out.constrains), dep_to_str);
            }
            {
                const auto id_to_str = [&pool](solv::StringId id)
                { return std::string(pool.get_string(id)); };
                auto feats = s.track_features();
                out.track_features.reserve(feats.size());
                std::transform(
                    feats.begin(),
                    feats.end(),
                    std::back_inserter(out.track_features),
                    id_to_str
                );
            }

            return out;
        }
    }

    std::optional<specs::PackageInfo> MPool::id2pkginfo(Id solv_id) const
    {
        if (const auto solv = pool().get_solvable(solv_id))
        {
            return { make_package_info(pool(), solv.value()) };
        }
        return std::nullopt;
    }

    std::optional<std::string> MPool::dep2str(Id dep_id) const
    {
        if (!dep_id)
        {
            return std::nullopt;
        }
        // Not const because might alloctmp space
        return pool_dep2str(const_cast<::Pool*>(pool().raw()), dep_id);
    }

    void MPool::remove_repo(::Id repo_id, bool reuse_ids)
    {
        pool().remove_repo(repo_id, reuse_ids);
    }

    void MPool::set_installed_repo(const MRepo& repo)
    {
        pool().set_installed_repo(repo.id());
    }

    // TODO machinery functions in separate files
    auto load_subdir_in_pool(const Context& ctx, MPool& pool, const MSubdirData& subdir)
        -> expected_t<MRepo>
    {
        auto meta = solver::libsolv::RepodataOrigin{
            /* .url= */ util::rsplit(subdir.metadata().url(), "/", 1).front(),
            /* .etag= */ subdir.metadata().etag(),
            /* .mod= */ subdir.metadata().last_modified(),
        };

        auto cache = subdir.cache_path();  // Dynamic cache path, either .solv or .json
        if (!cache)
        {
            return forward_error(cache);
        }
        return { MRepo(
            pool,
            subdir.name(),
            *cache,
            meta,
            static_cast<MRepo::PipAsPythonDependency>(ctx.add_pip_as_python_dependency)
        ) };
    }
}
