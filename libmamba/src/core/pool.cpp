// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <list>

#include <solv/evr.h>
#include <solv/pool.h>
#include <solv/selection.h>
#include <solv/solver.h>
extern "C"  // Incomplete header
{
#include <solv/conda.h>
}
#include <spdlog/spdlog.h>

#include "mamba/core/context.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/pool.hpp"

#include "solv-cpp/queue.hpp"

namespace mamba
{
    namespace
    {
        void libsolv_debug_callback(Pool* /*pool*/, void* userptr, int type, const char* str)
        {
            auto* dbg = (std::pair<spdlog::logger*, std::string>*) (userptr);
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

    Id MPool::matchspec2id(const std::string& ms)
    {
        Id id = pool_conda_matchspec(pool(), ms.c_str());
        if (!id)
        {
            throw std::runtime_error("libsolv error: could not create matchspec from string");
        }
        return id;
    }

    std::optional<PackageInfo> MPool::id2pkginfo(Id id)
    {
        if (id == 0 || id >= pool()->nsolvables)
        {
            return std::nullopt;
        }
        return pool_id2solvable(pool(), id);
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
