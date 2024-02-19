// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <string>

#include "mamba/core/output.hpp"
#include "mamba/core/timeref.hpp"
#include "mamba/core/util.hpp"
#include "mamba/download/downloader.hpp"
#include "mamba/util/string.hpp"
#include "mamba/validation/errors.hpp"
#include "mamba/validation/repo_checker.hpp"
#include "mamba/validation/update_framework.hpp"
#include "mamba/validation/update_framework_v0_6.hpp"
#include "mamba/validation/update_framework_v1.hpp"

namespace mamba::validation
{
    RepoChecker::RepoChecker(
        const Context& context,
        std::string base_url,
        fs::u8path ref_path,
        fs::u8path cache_path
    )
        : m_context(context)
        , m_base_url(std::move(base_url))
        , m_ref_path(std::move(ref_path))
        , m_cache_path(std::move(cache_path))
    {
        m_root_version = 0;
    }

    RepoChecker::RepoChecker(RepoChecker&&) noexcept = default;

    RepoChecker::~RepoChecker() = default;

    auto RepoChecker::operator=(RepoChecker&&) noexcept -> RepoChecker& = default;

    auto RepoChecker::cache_path() -> const fs::u8path&
    {
        return m_cache_path;
    }

    void RepoChecker::generate_index_checker()
    {
        if (!p_index_checker)
        {
            // TUF spec 5.1 - Record fixed update start time
            // Expiration computations will be done against
            // this reference
            // https://theupdateframework.github.io/specification/latest/#fix-time
            const TimeRef time_reference;

            auto root = get_root_role(time_reference);
            p_index_checker = root->build_index_checker(
                m_context,
                time_reference,
                m_base_url,
                cache_path()
            );

            LOG_INFO << "Index checker successfully generated for '" << m_base_url << "'";
        }
    }

    void RepoChecker::verify_index(const nlohmann::json& j) const
    {
        if (p_index_checker)
        {
            p_index_checker->verify_index(j);
        }
        else
        {
            LOG_ERROR << "Index checker not valid.";
        }
    }

    void RepoChecker::verify_index(const fs::u8path& p) const
    {
        if (p_index_checker)
        {
            p_index_checker->verify_index(p);
        }
        else
        {
            LOG_ERROR << "Index checker not valid.";
        }
    }

    void
    RepoChecker::verify_package(const nlohmann::json& signed_data, const nlohmann::json& signatures) const
    {
        if (p_index_checker)
        {
            p_index_checker->verify_package(signed_data, signatures);
        }
        else
        {
            LOG_ERROR << "Index checker not valid.";
        }
    }

    void
    RepoChecker::verify_package(const nlohmann::json& signed_data, std::string_view signatures) const
    {
        if (signatures.empty())
        {
            LOG_ERROR << "The given package signatures are empty";
            throw signatures_error();
        }
        else
        {
            LOG_INFO << "Verifying package...";
            verify_package(signed_data, nlohmann::json::parse(signatures));
        }
    }

    auto RepoChecker::root_version() -> std::size_t
    {
        return m_root_version;
    }

    auto RepoChecker::ref_root() -> fs::u8path
    {
        return m_ref_path / "root.json";
    }

    auto RepoChecker::cached_root() -> fs::u8path
    {
        if (cache_path().empty())
        {
            return "";
        }
        else
        {
            return cache_path() / "root.json";
        }
    }

    void RepoChecker::persist_file(const fs::u8path& file_path)
    {
        if (fs::exists(cached_root()))
        {
            fs::remove(cached_root());
        }
        if (!cached_root().empty())
        {
            fs::copy(file_path, cached_root());
        }
    }

    auto RepoChecker::initial_trusted_root() -> fs::u8path
    {
        if (fs::exists(cached_root()))
        {
            LOG_DEBUG << "Using cache for 'root' initial trusted file";
            return cached_root();
        }

        if (!fs::exists(m_ref_path))
        {
            LOG_ERROR << "'root' initial trusted file not found at '" << m_ref_path.string()
                      << "' for repo '" << m_base_url << "'";
            throw role_file_error();
        }
        else
        {
            return ref_root();
        }
    }

    auto RepoChecker::get_root_role(const TimeRef& time_reference) -> std::unique_ptr<RootRole>
    {
        // TUF spec 5.3 - Update the root role
        // https://theupdateframework.github.io/specification/latest/#update-root

        std::unique_ptr<RootRole> updated_root;

        LOG_DEBUG << "Loading 'root' metadata for repo '" << m_base_url << "'";
        auto trusted_root = initial_trusted_root();

        if (v0_6::SpecImpl().is_compatible(trusted_root))
        {
            LOG_INFO << "Getting 'root' role, using v0.6";
            updated_root = std::make_unique<v0_6::RootImpl>(trusted_root);
        }
        else if (v1::SpecImpl().is_compatible(trusted_root))
        {
            LOG_INFO << "Getting 'root' role, using v1";
            updated_root = std::make_unique<v1::RootImpl>(trusted_root);
        }
        else
        {
            LOG_ERROR << "Invalid 'root' initial trusted file '" << trusted_root.string()
                      << "' for repo '" << m_base_url << "'";
            throw role_file_error();
        }

        if (trusted_root != cached_root())
        {
            persist_file(trusted_root);
        }

        auto update_files = updated_root->possible_update_files();
        auto tmp_dir = TemporaryDirectory();
        auto tmp_dir_path = tmp_dir.path();

        // do chained updates
        LOG_DEBUG << "Starting updates of 'root' metadata";
        do
        {
            fs::u8path tmp_file_path;

            // Update from the most recent spec supported by this client
            for (auto& f : update_files)
            {
                auto url = util::concat(m_base_url, "/", f.string());
                tmp_file_path = tmp_dir_path / f;

                if (download::check_resource_exists(url, m_context))
                {
                    download::Request
                        request(f.string(), download::MirrorName(""), url, tmp_file_path.string());
                    download::Result res = download::download(
                        std::move(request),
                        m_context.get().mirrors,
                        m_context
                    );

                    if (res)
                    {
                        break;
                    }
                }
                tmp_file_path = "";
            }

            if (tmp_file_path.empty())
            {
                break;
            }

            updated_root = updated_root->update(tmp_file_path);
            // TUF spec 5.3.8 - Persist root metadata
            // Updated 'root' metadata are persisted in a cache directory
            persist_file(tmp_file_path);

            // Set the next possible files
            update_files = updated_root->possible_update_files();
        }
        // TUF spec 5.3.9 - Repeat steps 5.3.2 to 5.3.9
        while (true);

        m_root_version = updated_root->version();
        LOG_DEBUG << "Latest 'root' metadata has version " << m_root_version;

        // TUF spec 5.3.10 - Check for a freeze attack
        // Updated 'root' role should not be expired
        // https://theupdateframework.github.io/specification/latest/#update-root
        if (updated_root->expired(time_reference))
        {
            LOG_ERROR << "Possible freeze attack of 'root' metadata.\nExpired: "
                      << updated_root->expires();
            throw freeze_error();
        }

        return updated_root;
    };
}
