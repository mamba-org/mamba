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
        solv::ObjPool pool = {};
    };

    MPool::MPool()
        : m_data(std::make_shared<MPoolData>())
    {
        pool().set_disttype(DISTTYPE_CONDA);
        set_debuglevel();
    }

    MPool::~MPool() = default;

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
        bool channel_match(const Channel& chan, const Channel& needle)
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
                    const Channel& inner = make_channel(el);
                    if ((chan) == inner)
                    {
                        return true;
                    }
                }
            }

            return false;
        }

        ::Id add_channel_specific_matchspec(::Pool* pool, const MatchSpec& ms)
        {
            // Poor man's ms repr to match waht the user provided
            std::string const repr = fmt::format("{}::{}", ms.channel, ms.conda_build_form());

            // Already added, return that id
            if (::Id repr_id = pool_str2id(pool, repr.c_str(), /* .create= */ false); repr_id != 0)
            {
                return repr_id;
            }

            solv::ObjQueue selected_pkgs;

            // conda_build_form does **NOT** contain the channel info
            ::Id match = pool_conda_matchspec(pool, ms.conda_build_form().c_str());

            const Channel& c = make_channel(ms.channel);
            ::Id const m_mrepo_key = pool_str2id(pool, "solvable:mrepo_url", 1);
            for (Id* wp = pool_whatprovides_ptr(pool, match); *wp; wp++)
            {
                auto* const s = pool_id2solvable(pool, *wp);

                const char* s_url = solvable_lookup_str(s, m_mrepo_key);
                if ((s_url != nullptr) && channel_match(make_channel(s_url), c))
                {
                    selected_pkgs.push_back(*wp);
                }
            }
            ::Id const repr_id = pool_str2id(pool, repr.c_str(), /* .create= */ true);
            ::Id const offset = pool_queuetowhatprovides(pool, selected_pkgs.raw());
            // FRAGILE This get deleted when calling ``pool_createwhatprovides`` so care
            // must be taken to do it before
            // TODO investigate namespace providers
            pool_set_whatprovides(pool, repr_id, offset);
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
            id = add_channel_specific_matchspec(pool().raw(), ms);
        }
        if (id == 0)
        {
            throw std::runtime_error("libsolv error: could not create matchspec from string");
        }
        return id;
    }

    namespace
    {
        auto make_package_info(::Solvable& s) -> PackageInfo
        {
            // Note: this function (especially the checksum part) is NOT YET threadsafe!
            Pool* pool = s.repo->pool;
            Id check_type;

            PackageInfo out = {};

            out.name = pool_id2str(pool, s.name);
            out.version = pool_id2str(pool, s.evr);
            out.build_string = raw_str_or_empty(solvable_lookup_str(&s, SOLVABLE_BUILDFLAVOR));
            if (const char* str = solvable_lookup_str(&s, SOLVABLE_BUILDVERSION); str != nullptr)
            {
                out.build_number = std::stoull(str);
            }

            assert(s.repo != nullptr);
            if (const char* str = solvable_lookup_str(&s, SOLVABLE_URL))
            {
                out.url = str;
                // note this can and should be <unknown> when e.g. installing from a tarball
                out.channel = s.repo->name;
            }
            else if (std::string_view(s.repo->name) == "__explicit_specs__")
            {
                out.url = solvable_lookup_location(&s, 0);
                out.channel = make_channel(out.url).canonical_name();
            }
            else
            {
                out.channel = s.repo->name;  // note this can and should be <unknown> when e.g.
                                             // installing from a tarball
                out.url = fmt::format(
                    "{}/{}",
                    out.channel,
                    raw_str_or_empty(solvable_lookup_str(&s, SOLVABLE_MEDIAFILE))
                );
            }

            out.subdir = raw_str_or_empty(solvable_lookup_str(&s, SOLVABLE_MEDIADIR));
            out.fn = raw_str_or_empty(solvable_lookup_str(&s, SOLVABLE_MEDIAFILE));
            out.license = raw_str_or_empty(solvable_lookup_str(&s, SOLVABLE_LICENSE));
            out.size = solvable_lookup_num(&s, SOLVABLE_DOWNLOADSIZE, 0);
            out.timestamp = solvable_lookup_num(&s, SOLVABLE_BUILDTIME, 0);
            out.md5 = raw_str_or_empty(solvable_lookup_checksum(&s, SOLVABLE_PKGID, &check_type));
            out.sha256 = raw_str_or_empty(solvable_lookup_checksum(&s, SOLVABLE_CHECKSUM, &check_type)
            );
            out.signatures = raw_str_or_empty(solvable_lookup_str(&s, SIGNATURE_DATA));
            if (out.signatures.empty())
            {
                out.signatures = "{}";
            }

            solv::ObjQueue q = {};
            auto dep2str = [&pool](Id id) { return pool_dep2str(pool, id); };
            if (!solvable_lookup_deparray(&s, SOLVABLE_REQUIRES, q.raw(), -1))
            {
                out.defaulted_keys.insert("depends");
            }
            out.depends.reserve(q.size());
            std::transform(q.begin(), q.end(), std::back_inserter(out.depends), dep2str);

            q.clear();
            if (!solvable_lookup_deparray(&s, SOLVABLE_CONSTRAINS, q.raw(), -1))
            {
                out.defaulted_keys.insert("constrains");
            }
            out.constrains.reserve(q.size());
            std::transform(q.begin(), q.end(), std::back_inserter(out.constrains), dep2str);

            q.clear();
            auto id2str = [&pool](Id id) { return pool_id2str(pool, id); };
            solvable_lookup_idarray(&s, SOLVABLE_TRACK_FEATURES, q.raw());
            std::transform(q.begin(), q.end(), std::back_inserter(out.track_features), id2str);

            return out;
        }
    }

    std::optional<PackageInfo> MPool::id2pkginfo(Id solv_id) const
    {
        if (solv_id == 0 || util::cmp_greater_equal(solv_id, pool().raw()->nsolvables))
        {
            return std::nullopt;
        }
        return { make_package_info(*pool_id2solvable(pool().raw(), solv_id)) };
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
