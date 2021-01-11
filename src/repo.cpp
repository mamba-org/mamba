// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/repo.hpp"
#include "mamba/output.hpp"
#include "mamba/package_info.hpp"

extern "C"
{
#include "solv/repo_write.h"
}

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

    MRepo::MRepo(MPool& pool,
                 const std::string& name,
                 const fs::path& filename,
                 const RepoMetadata& metadata)
        : m_metadata(metadata)
    {
        m_url = rsplit(metadata.url, "/", 1)[0];
        m_repo = repo_create(pool, m_url.c_str());
        read_file(filename);
    }

    MRepo::MRepo(MPool& pool,
                 const std::string& name,
                 const std::string& filename,
                 const std::string& url)
        : m_url(url)
    {
        m_repo = repo_create(pool, name.c_str());
        read_file(filename);
    }

    MRepo::MRepo(MPool& pool, const PrefixData& prefix_data)
    {
        m_repo = repo_create(pool, "installed");
        int flags = 0;
        Repodata* data;
        data = repo_add_repodata(m_repo, flags);

        for (auto& [name, record] : prefix_data.records())
        {
            LOG_INFO << "Adding package record to repo " << name;
            Id handle = repo_add_solvable(m_repo);
            Solvable* s;
            s = pool_id2solvable(pool, handle);
            repodata_set_str(
                data, handle, SOLVABLE_BUILDVERSION, std::to_string(record.build_number).c_str());
            repodata_add_poolstr_array(
                data, handle, SOLVABLE_BUILDFLAVOR, record.build_string.c_str());
            s->name = pool_str2id(pool, record.name.c_str(), 1);
            s->evr = pool_str2id(pool, record.version.c_str(), 1);

            repodata_set_location(data, handle, 0, record.subdir.c_str(), record.fn.c_str());

            if (!record.depends.empty())
            {
                for (std::string dep : record.depends)
                {
                    Id dep_id = pool_conda_matchspec(pool, dep.c_str());
                    if (dep_id)
                    {
                        s->requires = repo_addid_dep(m_repo, s->requires, dep_id, 0);
                    }
                }
            }

            if (!record.constrains.empty())
            {
                for (std::string cst : record.constrains)
                {
                    Id constrains_id = pool_conda_matchspec(pool, cst.c_str());
                    if (constrains_id)
                    {
                        repodata_add_idarray(data, handle, SOLVABLE_CONSTRAINS, constrains_id);
                    }
                }
            }

            s->provides = repo_addid_dep(
                m_repo, s->provides, pool_rel2id(pool, s->name, s->evr, REL_EQ, 1), 0);
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
        // TODO this is actually freed with the pool, and calling it here will cause
        // segfaults. need to find a more clever way to do this. repo_free(m_repo,
        // /*reuse_ids*/1);
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

    std::string MRepo::name() const
    {
        return m_repo->name ? m_repo->name : "";
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

        if (is_solv)
        {
            auto fp = fopen(m_solv_file.c_str(), "rb");
            if (!fp)
            {
                throw std::runtime_error("Could not open repository file " + filename);
            }

            LOG_INFO << "Attempt load from solv " << m_solv_file;

            int ret = repo_add_solv(m_repo, fp, 0);
            if (ret != 0)
            {
                LOG_ERROR << "Could not load .solv file, falling back to JSON: "
                          << pool_errstr(m_repo->pool);
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

                    const char* url = repodata_lookup_str(repodata, SOLVID_META, url_id);
                    int pip_added = repodata_lookup_num(repodata, SOLVID_META, pip_added_id, -1);
                    const char* etag = repodata_lookup_str(repodata, SOLVID_META, etag_id);
                    const char* mod = repodata_lookup_str(repodata, SOLVID_META, mod_id);
                    const char* tool_version
                        = repodata_lookup_str(repodata, SOLVID_META, REPOSITORY_TOOLVERSION);
                    bool metadata_valid
                        = !(!url || !etag || !mod || !tool_version || pip_added == -1);

                    if (metadata_valid)
                    {
                        RepoMetadata read_metadata{ url, pip_added == 1, etag, mod };
                        metadata_valid = (read_metadata == m_metadata)
                                         && (std::strcmp(tool_version, mamba_tool_version()) == 0);
                    }

                    LOG_INFO << "Metadata from .solv is "
                             << (metadata_valid ? "valid" : "NOT valid");

                    if (!metadata_valid)
                    {
                        LOG_INFO << "solv file was written with a previous version of "
                                    "libsolv or mamba "
                                 << (tool_version != nullptr ? tool_version : "<NULL>")
                                 << ", updating it now!";
                    }
                    else
                    {
                        LOG_INFO << "Loaded from solv " << m_solv_file;
                        repo_internalize(m_repo);
                        fclose(fp);
                        return true;
                    }
                }
            }

            // fallback to JSON file
            repo_empty(m_repo, /*reuseids*/ 0);
            fclose(fp);
        }

        auto fp = fopen(m_json_file.c_str(), "r");
        if (!fp)
        {
            throw std::runtime_error("Could not open repository file " + m_json_file);
        }

        LOG_INFO << "loading from json " << m_json_file;
        int ret = repo_add_conda(m_repo, fp, 0);
        if (ret != 0)
        {
            throw std::runtime_error("Could not read JSON repodata file (" + m_json_file + ") "
                                     + std::string(pool_errstr(m_repo->pool)));
        }

        // TODO move this to a more structured approach for repodata patching?
        if (Context::instance().add_pip_as_python_dependency)
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
                        m_repo, pkg_s->requires, python_dep, SOLVABLE_PREREQMARKER);
                }
            }
        }

        repo_internalize(m_repo);

        if (name() != "installed")
        {
            write();
        }

        return true;
    }

    bool MRepo::write() const
    {
        Repodata* info;

        LOG_INFO << "writing solv file: " << m_solv_file;

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

        auto solv_f = fopen(m_solv_file.c_str(), "wb");
        repodata_internalize(info);

        if (repo_write(m_repo, solv_f) != 0)
        {
            LOG_ERROR << "Failed to write .solv:" << pool_errstr(m_repo->pool);
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
