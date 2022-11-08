// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <solv/pool.h>
#include <solv/solver.h>
#include <solv/selection.h>
#include <solv/evr.h>
#include <spdlog/spdlog.h>

#include "mamba/core/context.hpp"
#include "mamba/core/pool.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/queue.hpp"

namespace mamba
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

    MPool::MPool()
        : m_pool(pool_create(), &MPool::delete_libsolv_pool)
    {
        pool_setdisttype(m_pool.get(), DISTTYPE_CONDA);
        set_debuglevel();
    }

    void MPool::set_debuglevel()
    {
        // ensure that debug logging goes to stderr as to not interfere with stdout json output
        m_pool->debugmask |= SOLV_DEBUG_TO_STDERR;
        if (Context::instance().verbosity > 2)
        {
            pool_setdebuglevel(m_pool.get(), Context::instance().verbosity - 1);
            auto logger = spdlog::get("libsolv");
            m_debug_logger.first = logger.get();
            pool_setdebugcallback(m_pool.get(), &libsolv_debug_callback, &m_debug_logger);
        }
    }

    void MPool::create_whatprovides()
    {
        pool_createwhatprovides(m_pool.get());
    }

    MPool::operator Pool*()
    {
        return m_pool.get();
    }

    MPool::operator Pool const*() const
    {
        return m_pool.get();
    }

    std::vector<Id> MPool::select_solvables(Id matchspec, bool sorted) const
    {
        MQueue job, solvables;
        job.push(SOLVER_SOLVABLE_PROVIDES, matchspec);
        selection_solvables(m_pool.get(), job, solvables);

        if (sorted)
        {
            std::sort(solvables.begin(),
                      solvables.end(),
                      [this](Id a, Id b)
                      {
                          Solvable* sa = pool_id2solvable(m_pool.get(), a);
                          Solvable* sb = pool_id2solvable(m_pool.get(), b);
                          return (pool_evrcmp(this->m_pool.get(), sa->evr, sb->evr, EVRCMP_COMPARE)
                                  > 0);
                      });
        }
        return solvables.as<std::vector>();
    }

    Id MPool::matchspec2id(const std::string& ms)
    {
        Id id = pool_conda_matchspec(m_pool.get(), ms.c_str());
        if (!id)
            throw std::runtime_error("libsolv error: could not create matchspec from string");
        return id;
    }

    std::optional<PackageInfo> MPool::id2pkginfo(Id id)
    {
        if (id == 0 || id >= m_pool->nsolvables)
        {
            return std::nullopt;
        }
        return pool_id2solvable(m_pool.get(), id);
    }

    MRepo& MPool::add_repo(MRepo&& repo)
    {
        m_repo_list.push_back(std::move(repo));
        return m_repo_list.back();
    }

    void MPool::remove_repo(Id repo_id)
    {
        m_repo_list.remove_if([repo_id](const MRepo& repo) { return repo_id == repo.id(); });
    }

    void MPool::delete_libsolv_pool(::Pool* pool)
    {
        LOG_INFO << "Freeing pool.";
        pool_free(pool);
    }

}  // namespace mamba
