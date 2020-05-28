#ifndef MAMBA_REPO_HPP
#define MAMBA_REPO_HPP

#include <string>
#include <tuple>

#include "prefix_data.hpp"

extern "C"
{
    #include "solv/repo.h"
    #include "solv/repo_solv.h"
    #include "solv/conda.h"
    #include "solv/repo_conda.h"
}

#include "pool.hpp"

namespace mamba
{
    struct RepoMetadata
    {
        std::string url;
        bool pip_added;
        std::string etag;
        std::string mod;
    };

    inline bool operator==(const RepoMetadata& lhs, const RepoMetadata& rhs)
    {
        return lhs.url == rhs.url && lhs.pip_added == rhs.pip_added &&
               lhs.etag == rhs.etag && lhs.mod == rhs.mod;
    }

    class MRepo
    {
    public:

        MRepo(MPool& pool, const PrefixData& prefix_data);
        MRepo(MPool& pool, const std::string& name,
              const std::string& filename, const std::string& url);
        MRepo(MPool& pool, const std::string& name, const fs::path& path, const RepoMetadata& meta);
        ~MRepo();

        void set_installed();
        void set_priority(int priority, int subpriority);

        std::string name() const;
        bool write() const;
        const std::string& url() const;
        Repo* repo();
        std::tuple<int, int> priority() const;
        std::size_t size() const;

        bool clear(bool reuse_ids);

    private:

        bool read_file(const std::string& filename);

        std::string m_json_file, m_solv_file;
        std::string m_url;

        RepoMetadata m_metadata;

        Repo* m_repo;
    };
}

#endif // MAMBA_REPO_HPP
