// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <solv/repo.h>
#include <solv/repo_solv.h>
#include <solv/repo_write.h>
#include <solv/solvable.h>
extern "C"  // Incomplete header
{
#include <solv/conda.h>
#include <solv/repo_conda.h>
}

#include "mamba/core/channel.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/package_info.hpp"
#include "mamba/core/pool.hpp"
#include "mamba/core/prefix_data.hpp"
#include "mamba/core/repo.hpp"
#include "mamba/core/util_string.hpp"


#define MAMBA_TOOL_VERSION "1.1"

#define MAMBA_SOLV_VERSION MAMBA_TOOL_VERSION "_" LIBSOLV_VERSION_STRING

namespace mamba
{
    const char* mamba_tool_version()
    {
        const size_t bufferSize = 30;
        static char MTV[bufferSize];
        MTV[0] = '\0';
        snprintf(MTV, bufferSize, MAMBA_SOLV_VERSION);
        return MTV;
    }

    MRepo::MRepo(MPool& pool, const std::string& /*name*/, const fs::u8path& index, const RepoMetadata& metadata)
        : m_pool(pool)
        , m_url(rsplit(metadata.url, "/", 1)[0])
        , m_metadata(metadata)
    {
        m_repo = repo_create(pool, m_url.c_str());
        init(pool);
        read_file(index);
        set_solvables_url();
        repo_internalize(m_repo);
    }

    MRepo::MRepo(MPool& pool, const std::string& name, const std::string& index, const std::string& url)
        : m_pool(pool)
        , m_url(url)
        , m_repo(repo_create(pool, name.c_str()))
    {
        init(pool);
        read_file(index);
        set_solvables_url();
        repo_internalize(m_repo);
    }

    MRepo::MRepo(MPool& pool, const std::string& name, const std::vector<PackageInfo>& package_infos)
        : m_pool(pool)
        , m_repo(repo_create(pool, name.c_str()))
    {
        init(pool);
        int flags = 0;
        Repodata* data;
        data = repo_add_repodata(m_repo, flags);
        for (auto& info : package_infos)
        {
            add_package_info(data, info);
        }
        repodata_internalize(data);
    }

    MRepo::MRepo(MPool& pool, const PrefixData& prefix_data)
        : m_pool(pool)
        , m_repo(repo_create(pool, "installed"))
    {
        init(pool);
        int flags = 0;
        Repodata* data;
        data = repo_add_repodata(m_repo, flags);

        for (auto& [name, record] : prefix_data.records())
        {
            add_package_info(data, record);
        }

        if (Context::instance().add_pip_as_python_dependency)
        {
            add_pip_as_python_dependency();
        }

        repodata_internalize(data);
        set_installed();
    }

    void MRepo::init(MPool& pool)
    {
        m_mrepo_key = pool_str2id(pool, "solvable:mrepo_url", 1);
        m_noarch_repo_key = pool_str2id(pool, "solvable:noarch_type", 1);
    }

    void MRepo::set_solvables_url()
    {
        // Setting the channel url on where the solvable so that we can retrace
        // where it came from
        Id id = 0;
        Solvable* s = nullptr;
        FOR_REPO_SOLVABLES(m_repo, id, s)
        {
            solvable_set_str(s, m_mrepo_key, this->url().c_str());
        }
    }

    MRepo::MRepo(MRepo&& rhs)
        : m_json_file(std::move(rhs.m_json_file))
        , m_solv_file(std::move(rhs.m_solv_file))
        , m_url(std::move(rhs.m_url))
        , m_metadata(std::move(rhs.m_metadata))
        , m_repo(rhs.m_repo)
        , m_mrepo_key(rhs.m_mrepo_key)
        , m_noarch_repo_key(rhs.m_noarch_repo_key)
    {
        rhs.m_repo = nullptr;
    }

    MRepo& MRepo::operator=(MRepo&& rhs)
    {
        using std::swap;
        swap(m_json_file, rhs.m_json_file);
        swap(m_solv_file, rhs.m_solv_file);
        swap(m_url, rhs.m_url);
        swap(m_metadata, rhs.m_metadata);
        swap(m_repo, rhs.m_repo);
        swap(m_mrepo_key, rhs.m_mrepo_key);
        swap(m_noarch_repo_key, rhs.m_noarch_repo_key);
        return *this;
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

    void MRepo::add_package_info(Repodata* data, const PackageInfo& info)
    {
        // TODO missing track_feature
        LOG_INFO << "Adding package record to repo " << info.name;
        Pool* pool = m_repo->pool;

        Id handle = repo_add_solvable(m_repo);
        Solvable* s = pool_id2solvable(pool, handle);
        solvable_set_str(s, SOLVABLE_BUILDVERSION, std::to_string(info.build_number).c_str());
        solvable_add_poolstr_array(s, SOLVABLE_BUILDFLAVOR, info.build_string.c_str());
        solvable_set_str(s, SOLVABLE_NAME, info.name.c_str());
        solvable_set_str(s, SOLVABLE_EVR, info.version.c_str());
        solvable_set_num(s, SOLVABLE_DOWNLOADSIZE, info.size);
        // No ``solvable_xxx`` equivalent
        repodata_set_checksum(data, handle, SOLVABLE_PKGID, REPOKEY_TYPE_MD5, info.md5.c_str());

        solvable_set_str(s, SOLVABLE_URL, info.url.c_str());
        solvable_set_str(s, m_mrepo_key, url().c_str());

        if (!info.noarch.empty())
        {
            solvable_set_str(s, m_noarch_repo_key, info.noarch.c_str());
        }

        // No ``solvable_xxx`` equivalent
        repodata_set_location(data, handle, 0, info.subdir.c_str(), info.fn.c_str());
        repodata_set_checksum(data, handle, SOLVABLE_CHECKSUM, REPOKEY_TYPE_SHA256, info.sha256.c_str());

        if (!info.depends.empty())
        {
            for (std::string dep : info.depends)
            {
                Id dep_id = pool_conda_matchspec(pool, dep.c_str());
                if (dep_id)
                {
                    s->requires = repo_addid_dep(m_repo, s->requires, dep_id, 0);
                }
            }
        }

        if (!info.constrains.empty())
        {
            for (std::string cst : info.constrains)
            {
                Id constrains_id = pool_conda_matchspec(pool, cst.c_str());
                if (constrains_id)
                {
                    solvable_add_idarray(s, SOLVABLE_CONSTRAINS, constrains_id);
                }
            }
        }

        s->provides = repo_addid_dep(
            m_repo,
            s->provides,
            pool_rel2id(pool, s->name, s->evr, REL_EQ, /* create= */ 1),
            0
        );

        repo_internalize(m_repo);
    }

    Id MRepo::id() const
    {
        return m_repo->repoid;
    }

    std::string MRepo::name() const
    {
        return m_repo->name ? m_repo->name : "";
    }

    const std::string& MRepo::url() const
    {
        return m_url;
    }

    Repo* MRepo::repo() const
    {
        return m_repo;
    }

    std::tuple<int, int> MRepo::priority() const
    {
        return std::make_tuple(m_repo->priority, m_repo->subpriority);
    }

    std::size_t MRepo::size() const
    {
        return static_cast<std::size_t>(m_repo->nsolvables);
    }

    const fs::u8path& MRepo::index_file()
    {
        return m_json_file;
    }

    void MRepo::add_pip_as_python_dependency()
    {
        Id pkg_id;
        Solvable* pkg_s;
        Id python = pool_str2id(m_repo->pool, "python", 0);
        Id pip_dep = pool_conda_matchspec(m_repo->pool, "pip");
        Id pip = pool_str2id(m_repo->pool, "pip", 0);
        Id python_dep = pool_conda_matchspec(m_repo->pool, "python");

        FOR_REPO_SOLVABLES(m_repo, pkg_id, pkg_s)
        {
            if (pkg_s->name == python)
            {
                const char* version = pool_id2str(m_repo->pool, pkg_s->evr);
                if (version && version[0] >= '2')
                {
                    pkg_s->requires = repo_addid_dep(m_repo, pkg_s->requires, pip_dep, 0);
                }
            }
            if (pkg_s->name == pip)
            {
                pkg_s->requires = repo_addid_dep(
                    m_repo,
                    pkg_s->requires,
                    python_dep,
                    SOLVABLE_PREREQMARKER
                );
            }
        }
    }

    bool MRepo::read_file(const fs::u8path& filename)
    {
        bool is_solv = filename.extension() == ".solv";

        fs::u8path filename_wo_extension;
        if (is_solv)
        {
            m_solv_file = filename;
            m_json_file = filename;
            m_json_file.replace_extension("json");
        }
        else
        {
            m_json_file = filename;
            m_solv_file = filename;
            m_solv_file.replace_extension("solv");
        }

        LOG_INFO << "Reading cache files '" << (filename.parent_path() / filename).string()
                 << ".*' for repo index '" << m_repo->name << "'";

        if (is_solv)
        {
            auto lock = LockFile(m_solv_file);
#ifdef _WIN32
            auto fp = _wfopen(m_solv_file.wstring().c_str(), L"rb");
#else
            auto fp = fopen(m_solv_file.string().c_str(), "rb");
#endif
            if (!fp)
            {
                throw std::runtime_error("Could not open repository file " + filename.string());
            }

            LOG_INFO << "Attempt load from solv " << m_solv_file;

            int ret = repo_add_solv(m_repo, fp, 0);
            if (ret != 0)
            {
                LOG_ERROR << "Could not load SOLV file, falling back to JSON ("
                          << pool_errstr(m_repo->pool) << ")";
            }
            else
            {
                auto* repodata = repo_last_repodata(m_repo);
                if (!repodata)
                {
                    LOG_ERROR << "Could not find valid repodata attached to solv file";
                }
                else
                {
                    Id url_id = pool_str2id(m_repo->pool, "mamba:url", 1);
                    Id etag_id = pool_str2id(m_repo->pool, "mamba:etag", 1);
                    Id mod_id = pool_str2id(m_repo->pool, "mamba:mod", 1);
                    Id pip_added_id = pool_str2id(m_repo->pool, "mamba:pip_added", 1);

                    static constexpr auto failure = std::numeric_limits<unsigned long long>::max();
                    const char* url = repodata_lookup_str(repodata, SOLVID_META, url_id);
                    const auto pip_added = repodata_lookup_num(
                        repodata,
                        SOLVID_META,
                        pip_added_id,
                        failure
                    );
                    const char* etag = repodata_lookup_str(repodata, SOLVID_META, etag_id);
                    const char* mod = repodata_lookup_str(repodata, SOLVID_META, mod_id);
                    const char* tool_version = repodata_lookup_str(
                        repodata,
                        SOLVID_META,
                        REPOSITORY_TOOLVERSION
                    );
                    LOG_INFO << "Metadata solv file: " << url << " " << pip_added << " " << etag
                             << " " << mod << " " << tool_version;
                    bool metadata_valid = !(
                        !url || !etag || !mod || !tool_version || pip_added == failure
                    );

                    if (metadata_valid)
                    {
                        RepoMetadata read_metadata{ url, pip_added == 1, etag, mod };
                        metadata_valid = (read_metadata == m_metadata)
                                         && (std::strcmp(tool_version, mamba_tool_version()) == 0);
                    }

                    LOG_INFO << "Metadata from SOLV are " << (metadata_valid ? "valid" : "NOT valid");

                    if (!metadata_valid)
                    {
                        LOG_INFO << "SOLV file was written with a previous version of "
                                    "libsolv or mamba "
                                 << (tool_version != nullptr ? tool_version : "<NULL>")
                                 << ", updating it now!";
                    }
                    else
                    {
                        LOG_DEBUG << "Loaded from SOLV " << m_solv_file;
                        fclose(fp);
                        return true;
                    }
                }
            }

            // fallback to JSON file
            repo_empty(m_repo, /*reuseids*/ 0);
            fclose(fp);
        }

        auto lock = LockFile(m_json_file);
#ifdef _WIN32
        auto fp = _wfopen(m_json_file.wstring().c_str(), L"r");
#else
        auto fp = fopen(m_json_file.string().c_str(), "r");
#endif
        if (!fp)
        {
            throw std::runtime_error("Could not open repository file " + m_json_file.string());
        }

        LOG_DEBUG << "Loading JSON file '" << m_json_file.string() << "'";
        int flags = Context::instance().use_only_tar_bz2 ? CONDA_ADD_USE_ONLY_TAR_BZ2 : 0;
        int ret = repo_add_conda(m_repo, fp, flags);
        if (ret != 0)
        {
            fclose(fp);
            throw std::runtime_error(
                "Could not read JSON repodata file (" + m_json_file.string() + ") "
                + std::string(pool_errstr(m_repo->pool))
            );
        }

        // TODO move this to a more structured approach for repodata patching?
        if (Context::instance().add_pip_as_python_dependency)
        {
            add_pip_as_python_dependency();
        }

        if (name() != "installed")
        {
            write();
        }

        fclose(fp);
        return true;
    }

    bool MRepo::write() const
    {
        Repodata* info;

        LOG_INFO << "Writing SOLV file '" << m_solv_file.filename().string() << "'";

        info = repo_add_repodata(m_repo, 0);  // add new repodata for our meta info
        repodata_set_str(info, SOLVID_META, REPOSITORY_TOOLVERSION, mamba_tool_version());

        Id url_id = pool_str2id(m_repo->pool, "mamba:url", 1);
        Id pip_added_id = pool_str2id(m_repo->pool, "mamba:pip_added", 1);
        Id etag_id = pool_str2id(m_repo->pool, "mamba:etag", 1);
        Id mod_id = pool_str2id(m_repo->pool, "mamba:mod", 1);

        repodata_set_str(info, SOLVID_META, url_id, m_metadata.url.c_str());
        repodata_set_num(info, SOLVID_META, pip_added_id, m_metadata.pip_added);
        repodata_set_str(info, SOLVID_META, etag_id, m_metadata.etag.c_str());
        repodata_set_str(info, SOLVID_META, mod_id, m_metadata.mod.c_str());

        repodata_internalize(info);

#ifdef _WIN32
        auto solv_f = _wfopen(m_solv_file.wstring().c_str(), L"wb");
#else
        auto solv_f = fopen(m_solv_file.string().c_str(), "wb");
#endif
        if (!solv_f)
        {
            LOG_ERROR << "Failed to open .solv file";
            return false;
        }

        if (repo_write(m_repo, solv_f) != 0)
        {
            LOG_ERROR << "Failed to write .solv:" << pool_errstr(m_repo->pool);
            fclose(solv_f);
            return false;
        }

        if (fflush(solv_f))
        {
            LOG_ERROR << "Failed to flush .solv file.";
            fclose(solv_f);
            return false;
        }

        fclose(solv_f);
        repodata_free(info);  // delete meta info repodata again
        return true;
    }

    bool MRepo::clear(bool reuse_ids = 1)
    {
        repo_free(m_repo, static_cast<int>(reuse_ids));
        m_repo = nullptr;
        return true;
    }
}  // namespace mamba
