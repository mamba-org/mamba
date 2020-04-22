#include "repo.hpp"
#include "output.hpp"

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
        auto fp = fopen(filename.c_str(), "r");
        if (!fp)
        {
            throw std::runtime_error("Could not open repository file " + filename);
        }
        std::string ending = ".solv";

        if (std::equal(filename.end() - ending.size(), filename.end(), ending.begin()))
        {
            repo_add_solv(m_repo, fp, 0);
            LOG_INFO << "loading from solv " << filename;
            m_solv_file = filename;
            m_json_file = filename.substr(0, filename.size() - ending.size());
            m_json_file += std::string(".json");
            repo_internalize(m_repo);
        }
        else
        {
            LOG_INFO << "loading from json " << filename;
            repo_add_conda(m_repo, fp, 0);
            repo_internalize(m_repo);

            m_json_file = filename;
            // disabling solv caching for now on Windows
            #ifndef WIN32
            m_solv_file = filename.substr(0, filename.size() - ending.size());
            m_solv_file += ending;
            LOG_INFO << "creating solv: " << m_solv_file;

            auto sfile = fopen(m_solv_file.c_str(), "w");
            if (sfile)
            {
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

