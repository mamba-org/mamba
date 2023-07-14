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
extern "C"  // Incomplete header
{
#include <solv/conda.h>
}
#include <spdlog/spdlog.h>

#include "mamba/core/channel.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/match_spec.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/pool.hpp"
#include "mamba/core/util_string.hpp"
#include "mamba/util/cast.hpp"
#include "mamba/util/compare.hpp"
#include "solv-cpp/pool.hpp"
#include "solv-cpp/queue.hpp"

namespace mamba
{
    struct MPool::MPoolData
    {
        MPoolData(ChannelContext& cc)
            : channel_context(cc)
        {
        }

        solv::ObjPool pool = {};
        ChannelContext& channel_context;
    };

    MPool::MPool(ChannelContext& channel_context)
        : m_data(std::make_shared<MPoolData>(channel_context))
    {
        pool().set_disttype(DISTTYPE_CONDA);
        set_debuglevel();
    }

    MPool::~MPool() = default;

    ChannelContext& MPool::channel_context() const
    {
        return m_data->channel_context;
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
        if (Context::instance().output_params.verbosity > 2)
        {
            pool_setdebuglevel(pool().raw(), Context::instance().output_params.verbosity - 1);
            pool().set_debug_callback(
                [logger = spdlog::get("libsolv")](::Pool*, int type, std::string_view msg) noexcept
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
                    else if (Context::instance().output_params.verbosity > 2)
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
        bool
        channel_match(ChannelContext& channel_context, const Channel& chan, const Channel& needle)
        {
            if ((chan) == needle)
            {
                return true;
            }

            auto& custom_multichannels = Context::instance().custom_multichannels;
            auto x = custom_multichannels.find(needle.name());
            if (x != custom_multichannels.end())
            {
                for (auto el : (x->second))
                {
                    const Channel& inner = channel_context.make_channel(el);
                    if ((chan) == inner)
                    {
                        return true;
                    }
                }
            }

            return false;
        }

        bool subdir_match(std::string candidate_repo_url, std::string needle_spec)
        {
            // Example candidate_repo_url: https://.../conda-forge/linux-64
            // example needle_spec: conda-forge/osx-64::xtensor

            std::string needle_channel = split(needle_spec, ":", 1)[0];
            if (!contains(needle_channel, "/"))
            {
                // Subdir not specified, so any subdir is fine
                return true;
            }
            std::string needle_subdir = rsplit(needle_channel, "/", 1)[1];

            std::string candidate_subdir = rsplit(candidate_url, "/", 1)[1];

            if (candidate_subdir == needle_subdir)
            {
                return true;
            }
            throw std::runtime_error(fmt::format(
                "The package \"{}\" is not available for the specified platform",
                needle_spec
            ));
        }

        /**
         * Add function to handle matchspec while parsing is done by libsolv.
         */
        auto add_channel_specific_matchspec(
            ChannelContext& channel_context,
            solv::ObjPool& pool,
            const MatchSpec& ms
        ) -> solv::DependencyId
        {
            // Poor man's ms repr to match waht the user provided
            std::string const repr = fmt::format("{}::{}", ms.channel, ms.conda_build_form());

            // Already added, return that id
            if (const auto maybe_id = pool.find_string(repr))
            {
                return maybe_id.value();
            }

            // conda_build_form does **NOT** contain the channel info
            solv::DependencyId const match = pool_conda_matchspec(
                pool.raw(),
                ms.conda_build_form().c_str()
            );

            const Channel& c = channel_context.make_channel(ms.channel);
            solv::ObjQueue selected_pkgs = {};
            pool.for_each_whatprovides(
                match,
                [&](solv::ObjSolvableViewConst s)
                {
                    // TODO this does not work with s.url(), we need to proper channel class
                    // to properly manage this.
                    auto repo = solv::ObjRepoView(*s.raw()->repo);
                    // TODO make_channel should disapear avoiding conflict here
                    auto const url = std::string(repo.url());
                    if (channel_match(channel_context, channel_context.make_channel(url), c))
                    {
                        if (subdir_match(url, ms.spec))
                        {
                            selected_pkgs.push_back(s.id());
                        }
                    }
                }
            );

            solv::StringId const repr_id = pool.add_string(repr);
            ::Id const offset = pool_queuetowhatprovides(pool.raw(), selected_pkgs.raw());
            // FRAGILE This get deleted when calling ``pool_createwhatprovides`` so care
            // must be taken to do it before
            // TODO investigate namespace providers
            pool_set_whatprovides(pool.raw(), repr_id, offset);
            return repr_id;
        }
    }

    ::Id MPool::matchspec2id(const MatchSpec& ms)
    {
        ::Id id = 0;
        if (ms.channel.empty())
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
        auto make_package_info(const solv::ObjPool& pool, solv::ObjSolvableViewConst s) -> PackageInfo
        {
            PackageInfo out = {};

            out.name = s.name();
            out.version = s.version();
            out.build_string = s.build_string();
            out.noarch = s.noarch();
            out.build_number = s.build_number();
            out.channel = s.channel();
            out.url = s.url();
            out.subdir = s.subdir();
            out.fn = s.file_name();
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

    std::optional<PackageInfo> MPool::id2pkginfo(Id solv_id) const
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
}  // namespace mamba
