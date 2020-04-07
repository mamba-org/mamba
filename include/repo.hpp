#ifndef MAMBA_REPO_HPP
#define MAMBA_REPO_HPP

#include <string>
#include <tuple>

#include "prefix_data.hpp"

extern "C"
{
    #include "common_write.c"

    #include "solv/repo.h"
    #include "solv/repo_solv.h"
    #include "solv/conda.h"
    #include "solv/repo_conda.h"
}

#include "pool.hpp"

namespace mamba
{
    class MRepo
    {
    public:

        MRepo(MPool& pool, const PrefixData& prefix_data)
        {
            m_repo = repo_create(pool, "installed");
            int flags = 0;
            Repodata *data;
            data = repo_add_repodata(m_repo, flags);

            for (auto& [name, record] : prefix_data.records())
            {
                LOG(INFO) << "Adding package record to repo " << name;
                Id handle = repo_add_solvable(m_repo);
                Solvable *s;
                s = pool_id2solvable(pool, handle);
                repodata_set_str(data, handle, SOLVABLE_BUILDVERSION, std::to_string(record.build_number).c_str());
                repodata_add_poolstr_array(data, handle, SOLVABLE_BUILDFLAVOR, record.build.c_str());
                s->name = pool_str2id(pool, record.name.c_str(), 1);
                s->evr = pool_str2id(pool, record.version.c_str(), 1);

                repodata_set_location(data, handle, 0, record.subdir.c_str(), record.fn.c_str());
                if (record.json.find("depends") != record.json.end())
                {
                    assert(record.json["depends"].is_array());
                    for (std::string dep : record.json["depends"])
                    {
                        Id dep_id = pool_conda_matchspec(pool, dep.c_str());
                        if (dep_id)
                        {
                            s->requires = repo_addid_dep(m_repo, s->requires, dep_id, 0);
                        }
                    }
                }

                if (record.json.find("constrains") != record.json.end())
                {
                    assert(record.json["constrains"].is_array());
                    for (std::string cst : record.json["constrains"])
                    {
                        Id constrains_id = pool_conda_matchspec(pool, cst.c_str());
                        if (constrains_id)
                        {
                            repodata_add_idarray(data, handle, SOLVABLE_CONSTRAINS, constrains_id);
                        }
                    }
                }

                s->provides = repo_addid_dep(m_repo, s->provides, pool_rel2id(pool, s->name, s->evr, REL_EQ, 1), 0);
            }
            LOG(INFO) << "Internalizing";
            repodata_internalize(data);
            set_installed();
        }

        ~MRepo()
        {
            // not sure if reuse_ids is useful here
            // repo will be freed with pool as well though
            // maybe explicitly free pool for faster repo deletion as well
            // TODO this is actually freed with the pool, and calling it here will cause segfaults.
            // need to find a more clever way to do this.
            // repo_free(m_repo, /*reuse_ids*/1);
        }

        void set_installed()
        {
            pool_set_installed(m_repo->pool, m_repo);
        }

        MRepo(MPool& pool, const std::string& name,
              const std::string& filename, const std::string& url);
        ~MRepo();

        void set_installed();
        void set_priority(int priority, int subpriority);

        const char* name() const;
        const std::string& url() const;
        Repo* repo();
        std::tuple<int, int> priority() const;
        std::size_t size() const;

    private:

        bool read_file(const std::string& filename);

        std::string m_json_file, m_solv_file;
        std::string m_url;

        Repo* m_repo;
    };
}

#endif // MAMBA_REPO_HPP
