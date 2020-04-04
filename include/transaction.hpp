#pragma once

#include <iomanip>
#include "nlohmann/json.hpp"

#include "thirdparty/minilog.hpp"


namespace mamba
{
    inline void try_add(nlohmann::json& j, const char* key, const char* val)
    {
        if (!val)
            return;
        j[key] = val;
    }
    std::string solvable_to_json(Solvable* s)
    {
        nlohmann::json j;
        auto* pool = s->repo->pool;

        j["name"] = pool_id2str(pool, s->name);
        j["version"] = pool_id2str(pool, s->evr);

        try_add(j, "build", solvable_lookup_str(s, SOLVABLE_BUILDFLAVOR));
        // Note we should?! fix this in libsolv?!
        const char* build_number = solvable_lookup_str(s, SOLVABLE_BUILDVERSION);
        if (build_number)
        {
            j["build_number"] = std::stoi(build_number);
        }

        try_add(j, "license", solvable_lookup_str(s, SOLVABLE_LICENSE));
        j["size"] = solvable_lookup_num(s, SOLVABLE_DOWNLOADSIZE, -1);

        std::size_t timestamp = solvable_lookup_num(s, SOLVABLE_BUILDTIME, 0);
        if (timestamp != 0) timestamp *= 1000;
        j["timestamp"] = timestamp;

        Id check_type;
        try_add(j, "md5", solvable_lookup_checksum(s, SOLVABLE_PKGID, &check_type));
        try_add(j, "sha256", solvable_lookup_checksum(s, SOLVABLE_CHECKSUM, &check_type));

        j["subdir"] = solvable_lookup_str(s, SOLVABLE_MEDIADIR);

        std::vector<std::string> depends, constrains;
        Queue q;
        queue_init(&q);
        solvable_lookup_deparray(s, SOLVABLE_REQUIRES, &q, -1);
        depends.resize(q.count);
        for (int i = 0; i < q.count; ++i)
        {
            depends[i] = pool_dep2str(pool, q.elements[i]);
        }
        queue_empty(&q);
        solvable_lookup_deparray(s, SOLVABLE_CONSTRAINS, &q, -1);
        constrains.resize(q.count);
        for (int i = 0; i < q.count; ++i)
        {
            constrains[i] = pool_dep2str(pool, q.elements[i]);
        }
        queue_free(&q);

        if (depends.size()) j["depends"] = depends;
        if (constrains.size()) j["constrains"] = constrains;

        return j.dump(4);
    }

    class MTransaction
    {
    public:
        MTransaction(MSolver& solver)
        {
            if (!solver.is_solved())
            {
                throw std::runtime_error("Cannot create transaction without calling solver.solve() first.");
            }
            m_transaction = solver_create_transaction(solver);
            init();
        }

        ~MTransaction()
        {
            LOG(INFO) << "Freeing transaction.";
            transaction_free(m_transaction);
        }

        void init()
        {
            Queue classes, pkgs;

            queue_init(&classes);
            queue_init(&pkgs);

            int mode = SOLVER_TRANSACTION_SHOW_OBSOLETES |
                       SOLVER_TRANSACTION_OBSOLETE_IS_UPGRADE;

            transaction_classify(m_transaction, mode, &classes);

            Id cls;
            std::string location;
            for (int i = 0; i < classes.count; i += 4)
            {
                cls = classes.elements[i];
                // cnt = classes.elements[i + 1];

                transaction_classify_pkgs(m_transaction, mode, cls, classes.elements[i + 2],
                                          classes.elements[i + 3], &pkgs);
                for (int j = 0; j < pkgs.count; j++)
                {
                    Id p = pkgs.elements[j];
                    Solvable *s = m_transaction->pool->solvables + p;

                    switch (cls)
                    {
                        case SOLVER_TRANSACTION_DOWNGRADED:
                        case SOLVER_TRANSACTION_UPGRADED:
                        case SOLVER_TRANSACTION_CHANGED:
                            m_to_remove.push_back(s);
                            m_to_install.push_back(m_transaction->pool->solvables + transaction_obs_pkg(m_transaction, p));
                            break;
                        case SOLVER_TRANSACTION_ERASE:
                            m_to_remove.push_back(s);
                            break;
                        case SOLVER_TRANSACTION_INSTALL:
                            m_to_install.push_back(s);
                            break;
                        case SOLVER_TRANSACTION_VENDORCHANGE:
                        case SOLVER_TRANSACTION_ARCHCHANGE:
                        default:
                            LOG(WARNING) << "CASE NOT HANDLED. " << cls;
                            break;
                    }
                }
            }

            queue_free(&classes);
            queue_free(&pkgs);
        }

        auto to_conda()
        {
            std::vector<std::tuple<std::string, std::string, std::string>> to_install_structured; 
            std::vector<std::tuple<std::string, std::string>> to_remove_structured; 

            for (Solvable* s : m_to_remove)
            {
                const char* mediafile = solvable_lookup_str(s, SOLVABLE_MEDIAFILE);
                to_remove_structured.emplace_back(s->repo->name, mediafile);
            }

            for (Solvable* s : m_to_install)
            {
                const char* mediafile = solvable_lookup_str(s, SOLVABLE_MEDIAFILE);
                std::string s_json = solvable_to_json(s);
                to_install_structured.emplace_back(s->repo->name, mediafile, s_json);
            }

            return std::make_tuple(to_install_structured, to_remove_structured);
        }

        void print()
        {
            transaction_print(m_transaction);
        }

    private:
        std::vector<Solvable*> m_to_install, m_to_remove;
        Transaction* m_transaction;
    };  
}