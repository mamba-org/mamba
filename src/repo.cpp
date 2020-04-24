#include "repo.hpp"
#include "output.hpp"

extern "C"
{
    #include "common_write.c"
}

namespace mamba
{
    MRepo::MRepo(MPool& pool, const std::string& name,
                 const std::string& filename, const std::string& url)
        : m_url(url)
    {
        m_repo = repo_create(pool, name.c_str());
        // pool.add_repo(this);
        read_file(filename);
    }

    MRepo::MRepo(MPool& pool, const PrefixData& prefix_data)
    {
        m_repo = repo_create(pool, "installed");
        int flags = 0;
        Repodata *data;
        data = repo_add_repodata(m_repo, flags);

        for (auto& [name, record] : prefix_data.records())
        {
            LOG_INFO << "Adding package record to repo " << name;
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
        LOG_INFO << "Internalizing";
        repodata_internalize(data);
        set_installed();
    }

    MRepo::~MRepo()
    {
        // not sure if reuse_ids is useful here
        // repo will be freed with pool as well though
        // maybe explicitly free pool for faster repo deletion as well
        // TODO this is actually freed with the pool, and calling it here will cause segfaults.
        // need to find a more clever way to do this.
        // repo_free(m_repo, /*reuse_ids*/1);
    }

    void MRepo::set_installed()
    {
        pool_set_installed(m_repo->pool, m_repo);
    }

    void MRepo::set_priority(int priority, int subpriority)
    {
        m_repo->priority = priority;
        m_repo->subpriority = subpriority;
    }

    const char* MRepo::name() const
    {
        return m_repo->name;
    }

    const std::string& MRepo::url() const
    {
        return m_url;
    }

    Repo* MRepo::repo()
    {
        return m_repo;
    }

    std::tuple<int, int> MRepo::priority() const
    {
        return std::make_tuple(m_repo->priority, m_repo->subpriority);
    }

    std::size_t MRepo::size() const
    {
        return m_repo->nsolvables;
    }

    bool MRepo::read_file(const std::string& filename)
    {
        LOG_INFO << m_repo->name << ": reading repo file " << filename;

        bool is_solv = ends_with(filename, ".solv");

        if (is_solv)
        {
            m_solv_file = filename;
            m_json_file = filename.substr(0, filename.size() - strlen(".solv")) + ".json";
        }
        else
        {
            m_json_file = filename;
            m_solv_file = filename.substr(0, filename.size() - strlen(".json")) + ".solv";
        }

        auto fp = fopen(filename.c_str(), "r");
        if (!fp)
        {
            throw std::runtime_error("Could not open repository file " + filename);
        }

        bool loaded = false;

        if (is_solv)
        {
            LOG_INFO << "Attempt load from solv " << m_solv_file;

            int ret = repo_add_solv(m_repo, fp, 0);
            auto* repodata = repo_last_repodata(m_repo);
            if (!repodata)
            {
                LOG_ERROR << "Could not find valid repodata attached to solv file";
            }
            else
            {
                const char* repo_tool_version = repodata_lookup_str(repodata, SOLVID_META, REPOSITORY_TOOLVERSION);
                if (ret != 0)
                {
                    LOG_ERROR << "Could not load .solv file, falling back to JSON" << pool_errstr(m_repo->pool);
                }
                else if (repo_tool_version == nullptr || std::strcmp(MAMBA_SOLV_VERSION, repo_tool_version) != 0)
                {
                    LOG_ERROR << "solv file was written with a previous version of libsolv or mamba " <<
                                 (repo_tool_version != nullptr ? repo_tool_version : "<NULL>") << ", updating it now!";
                }
                else
                {
                    repo_internalize(m_repo);
                    loaded = true;
                    LOG_INFO << "Loaded from solv " << m_solv_file;
                }
            }
        }

        // fallback to JSON file
        if (is_solv && !loaded)
        {
            repo_empty(m_repo, /*reuseids*/ 0);
            fclose(fp);
            fp = fopen(m_json_file.c_str(), "r");
            if (!fp)
            {
                throw std::runtime_error("Could not open repository file " + m_json_file);
            }
        }

        if (!is_solv || !loaded)
        {
            LOG_INFO << "loading from json " << m_json_file;
            int ret = repo_add_conda(m_repo, fp, 0);
            if (ret != 0)
            {
                throw std::runtime_error("Could not read JSON repodata file (" + m_json_file + ") " + std::string(pool_errstr(m_repo->pool)));
            }
            repo_internalize(m_repo);

            // disabling solv caching for now on Windows
            #ifndef WIN32
            LOG_INFO << "creating solv: " << m_solv_file;

            auto sfile = fopen(m_solv_file.c_str(), "w");
            if (sfile)
            {
                // TODO add error handling to tool_write
                tool_write(m_repo, sfile);
                fclose(sfile);
            }
            else 
            {
                LOG_INFO << "could not create solv";
            }
            #endif
        }

        fclose(fp);
        return true;
    }
}

