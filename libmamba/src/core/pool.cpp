// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <list>

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
#include "solv-cpp/queue.hpp"

namespace mamba
{
    namespace
    {
        void libsolv_debug_callback(Pool* /*pool*/, void* userptr, int type, const char* str)
        {
            auto* dbg = reinterpret_cast<std::pair<spdlog::logger*, std::string>*>(userptr);
            dbg->second += str;
            if (dbg->second.size() == 0 || dbg->second.back() != '\n')
            {
                return;
            }

            auto log = Console::hide_secrets(dbg->second);
            if (type & SOLV_FATAL || type & SOLV_ERROR)
            {
                dbg->first->error(log);
            }
            else if (type & SOLV_WARN)
            {
                dbg->first->warn(log);
            }
            else if (Context::instance().verbosity > 2)
            {
                dbg->first->info(log);
            }
            dbg->second.clear();
        }

        void libsolv_delete_pool(::Pool* pool)
        {
            LOG_INFO << "Freeing pool.";
            pool_free(pool);
        }

    }

    struct MPool::MPoolData
    {
        MPoolData()
            : pool(pool_create(), &libsolv_delete_pool)
        {
        }

        std::unique_ptr<Pool, decltype(&libsolv_delete_pool)> pool;
        std::pair<spdlog::logger*, std::string> debug_logger = {};
        std::list<MRepo> repo_list = {};
    };

    MPool::MPool()
        : m_data(std::make_shared<MPoolData>())
    {
        pool_setdisttype(pool(), DISTTYPE_CONDA);
        set_debuglevel();
    }

    MPool::~MPool() = default;

    Pool* MPool::pool()
    {
        return m_data->pool.get();
    }

    const Pool* MPool::pool() const
    {
        return m_data->pool.get();
    }

    std::size_t MPool::n_solvables() const
    {
        return util::safe_num_cast<std::size_t>(pool()->nsolvables);
    }

    void MPool::set_debuglevel()
    {
        // ensure that debug logging goes to stderr as to not interfere with stdout json output
        pool()->debugmask |= SOLV_DEBUG_TO_STDERR;
        if (Context::instance().verbosity > 2)
        {
            pool_setdebuglevel(pool(), Context::instance().verbosity - 1);
            auto logger = spdlog::get("libsolv");
            m_data->debug_logger.first = logger.get();
            pool_setdebugcallback(pool(), &libsolv_debug_callback, &(m_data->debug_logger));
        }
    }

    void MPool::create_whatprovides()
    {
        pool_createwhatprovides(pool());
    }

    MPool::operator Pool*()
    {
        return pool();
    }

    MPool::operator const Pool*() const
    {
        return pool();
    }

    std::vector<Id> MPool::select_solvables(Id matchspec, bool sorted) const
    {
        solv::ObjQueue job = { SOLVER_SOLVABLE_PROVIDES, matchspec };
        solv::ObjQueue solvables = {};
        selection_solvables(const_cast<Pool*>(pool()), job.raw(), solvables.raw());

        if (sorted)
        {
            std::sort(
                solvables.begin(),
                solvables.end(),
                [this](Id a, Id b)
                {
                    Solvable* sa = pool_id2solvable(pool(), a);
                    Solvable* sb = pool_id2solvable(pool(), b);
                    return (pool_evrcmp(this->pool(), sa->evr, sb->evr, EVRCMP_COMPARE) > 0);
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
            pool_set_whatprovides(pool, repr_id, offset);
            return repr_id;
        }
    }

    ::Id MPool::matchspec2id(const MatchSpec& ms)
    {
        ::Id id = 0;
        if (ms.channel.empty())
        {
            id = pool_conda_matchspec(pool(), ms.conda_build_form().c_str());
        }
        else
        {
            // Working around shortcomings of ``pool_conda_matchspec``
            // The channels are not processed.
            id = add_channel_specific_matchspec(pool(), ms);
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

            static ::Id real_repo_key = pool_str2id(pool, "solvable:real_repo_url", 1);
            if (const char* str = solvable_lookup_str(&s, real_repo_key); str != nullptr)
            {
                out.url = str;
                out.channel = make_channel(out.url).canonical_name();
            }
            else
            {
                if (!s.repo || strcmp(s.repo->name, "__explicit_specs__") == 0)
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
            solvable_lookup_idarray(&s, SOLVABLE_TRACK_FEATURES, q.raw());
            for (::Id const id : q)
            {
                out.track_features += pool_id2str(pool, id);
                out.track_features += ',';
            }
            if (!out.track_features.empty())
            {
                out.track_features.pop_back();
            }

            const ::Id extra_keys_id = pool_str2id(pool, "solvable:extra_keys", 0);
            const ::Id extra_values_id = pool_str2id(pool, "solvable:extra_values", 0);
            if (extra_keys_id && extra_values_id)
            {
                // Get extra signed keys
                q.clear();
                solvable_lookup_idarray(&s, extra_keys_id, q.raw());
                std::vector<std::string> extra_keys = {};
                extra_keys.reserve(q.size());
                std::transform(q.begin(), q.end(), std::back_inserter(extra_keys), dep2str);

                // Get extra signed values
                q.clear();
                solvable_lookup_idarray(&s, extra_values_id, q.raw());
                std::vector<std::string> extra_values = {};
                extra_values.reserve(q.size());
                std::transform(q.begin(), q.end(), std::back_inserter(extra_values), dep2str);

                // Build a JSON string for extra signed metadata
                if (!extra_keys.empty() && (extra_keys.size() == extra_values.size()))
                {
                    std::vector<std::string> extra = {};
                    extra.reserve(extra_keys.size());
                    for (std::size_t i = 0; i < extra_keys.size(); ++i)
                    {
                        extra.push_back(fmt::format(R"("{}":{})", extra_keys[i], extra_values[i]));
                    }
                    out.extra_metadata = fmt::format("{{{}}}", fmt::join(extra, ","));
                }
            }
            else
            {
                out.extra_metadata = "{}";
            }
            return out;
        }
    }

    std::optional<PackageInfo> MPool::id2pkginfo(Id solv_id) const
    {
        if (solv_id == 0 || util::cmp_greater_equal(solv_id, n_solvables()))
        {
            return std::nullopt;
        }
        return { make_package_info(*pool_id2solvable(pool(), solv_id)) };
    }

    std::optional<std::string> MPool::dep2str(Id dep_id) const
    {
        if (!dep_id)
        {
            return std::nullopt;
        }
        // Not const because might alloctmp space
        return pool_dep2str(const_cast<::Pool*>(pool()), dep_id);
    }

    MRepo& MPool::add_repo(MRepo&& repo)
    {
        m_data->repo_list.push_back(std::move(repo));
        return m_data->repo_list.back();
    }

    void MPool::remove_repo(Id repo_id)
    {
        m_data->repo_list.remove_if([repo_id](const MRepo& repo) { return repo_id == repo.id(); });
    }

}  // namespace mamba
