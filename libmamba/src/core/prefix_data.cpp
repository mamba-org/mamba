// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/core/prefix_data.hpp"

#include <solv/transaction.h>

#include "mamba/core/output.hpp"
#include "mamba/core/pool.hpp"
#include "mamba/core/repo.hpp"
#include "mamba/core/util_string.hpp"

#include "solv-cpp/queue.hpp"

namespace mamba
{
    auto PrefixData::create(const fs::u8path& prefix_path) -> expected_t<PrefixData>
    {
        try
        {
            return PrefixData(prefix_path);
        }
        catch (std::exception& e)
        {
            return tl::make_unexpected(mamba_error(e.what(), mamba_error_code::prefix_data_not_loaded)
            );
        }
        catch (...)
        {
            return tl::make_unexpected(mamba_error(
                "Unknown error when trying to load prefix data " + prefix_path.string(),
                mamba_error_code::unknown
            ));
        }
    }

    PrefixData::PrefixData(const fs::u8path& prefix_path)
        : m_history(prefix_path)
        , m_prefix_path(prefix_path)
    {
        load();
    }

    void PrefixData::load()
    {
        auto conda_meta_dir = m_prefix_path / "conda-meta";
        if (lexists(conda_meta_dir))
        {
            for (auto& p : fs::directory_iterator(conda_meta_dir))
            {
                if (ends_with(p.path().string(), ".json"))
                {
                    load_single_record(p.path());
                }
            }
        }
    }

    void PrefixData::add_packages(const std::vector<PackageInfo>& packages)
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

        solv::ObjQueue q;
        {
            // TODO check prereq marker to `pip` if it's part of the installed packages
            // so that it gets installed after Python.
            auto& repo = MRepo::create(pool, *this);

            Solvable* s;
            Id pkg_id;
            pool_createwhatprovides(pool);

            FOR_REPO_SOLVABLES(repo.repo(), pkg_id, s)
            {
                q.push_back(pkg_id);
            }
        }

        Pool* pp = pool;
        pp->installed = nullptr;

        Transaction* t = transaction_create_decisionq(pool, q.raw(), nullptr);
        transaction_order(t, 0);

        for (int i = 0; i < t->steps.count; i++)
        {
            Id p = t->steps.elements[i];
            Id ttype = transaction_type(t, p, SOLVER_TRANSACTION_SHOW_ALL);
            Solvable* s = pool_id2solvable(t->pool, p);

            switch (ttype)
            {
                case SOLVER_TRANSACTION_INSTALL:
                {
                    const auto it = m_package_records.find(pool_id2str(pool, s->name));
                    if (it != m_package_records.end())
                    {
                        result.push_back(it->second);
                        break;
                    }
                    [[fallthrough]];
                }
                default:
                    throw std::runtime_error(
                        "Package not found in prefix records or other unexpected condition"
                    );
            }
        }
        return result;
    }

    History& PrefixData::history()
    {
        return m_history;
    }

    const fs::u8path& PrefixData::path() const
    {
        return m_prefix_path;
    }

    void PrefixData::load_single_record(const fs::u8path& path)
    {
        LOG_INFO << "Loading single package record: " << path;
        auto infile = open_ifstream(path);
        nlohmann::json j;
        infile >> j;
        auto prec = PackageInfo(std::move(j));
        m_package_records.insert({ prec.name, std::move(prec) });
    }
}  // namespace mamba
