// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/core/prefix_data.hpp"
#include "mamba/core/output.hpp"

#include "mamba/core/pool.hpp"
#include "mamba/core/repo.hpp"
#include "mamba/core/transaction.hpp"


namespace mamba
{
    PrefixData::PrefixData(const std::string& prefix_path)
        : m_history(prefix_path)
        , m_prefix_path(fs::path(prefix_path))
    {
    }

    void PrefixData::load()
    {
        auto conda_meta_dir = m_prefix_path / "conda-meta";
        if (lexists(conda_meta_dir))
        {
            for (auto& p : fs::directory_iterator(conda_meta_dir))
            {
                if (ends_with(p.path().c_str(), ".json"))
                {
                    load_single_record(p.path());
                }
            }
        }
    }

    void PrefixData::add_virtual_packages(const std::vector<PackageInfo>& packages)
    {
        for (const auto& pkg : packages)
        {
            LOG_DEBUG << "Adding virtual package: " << pkg.name << "=" << pkg.version << "="
                      << pkg.build_string;
            m_package_records.insert({ pkg.name, std::move(pkg) });
        }
    }

    const PrefixData::package_map& PrefixData::records() const
    {
        return m_package_records;
    }

    std::vector<PackageInfo> PrefixData::sorted_records() const
    {
        std::vector<PackageInfo> result;
        MPool pool;

        // TODO check prereq marker to `pip` if it's part of the installed packages
        // so that it gets installed after Python.
        auto repo = MRepo(pool, *this);

        Queue q;
        queue_init(&q);

        Solvable* s;
        Id pkg_id;
        pool_createwhatprovides(pool);

        FOR_REPO_SOLVABLES(repo.repo(), pkg_id, s)
        {
            queue_push(&q, pkg_id);
        }

        Pool* pp = pool;
        pp->installed = nullptr;

        Transaction* t = transaction_create_decisionq(pool, &q, nullptr);
        transaction_order(t, 0);

        for (int i = 0; i < t->steps.count; i++)
        {
            Id p = t->steps.elements[i];
            Id ttype = transaction_type(t, p, SOLVER_TRANSACTION_SHOW_ALL);
            Solvable* s = pool_id2solvable(t->pool, p);

            package_map::const_iterator it, end = m_package_records.end();
            switch (ttype)
            {
                case SOLVER_TRANSACTION_INSTALL:
                    it = m_package_records.find(pool_id2str(pool, s->name));
                    if (it != end)
                    {
                        result.push_back(it->second);
                        break;
                    }
                default:
                    throw std::runtime_error(
                        "Package not found in prefix records or other unexpected condition");
            }
        }
        queue_free(&q);
        return result;
    }

    History& PrefixData::history()
    {
        return m_history;
    }

    const fs::path& PrefixData::path() const
    {
        return m_prefix_path;
    }

    void PrefixData::load_single_record(const fs::path& path)
    {
        LOG_INFO << "Loading single package record: " << path;
        std::ifstream infile(path);
        nlohmann::json j;
        infile >> j;
        auto prec = PackageInfo(std::move(j));
        m_package_records.insert({ prec.name, std::move(prec) });
    }
}  // namespace mamba
