// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <fmt/format.h>

#include "mamba/core/mirror.hpp"
#include "mamba/core/output.hpp"
#include "mamba/util/string.hpp"
#include "mamba/util/url_manip.hpp"

#include "curl.hpp"

namespace mamba
{

    /*************************
     * Mirror implementation *
     *************************/

    Mirror::Mirror(const std::string& url)
        : m_protocol(Protocol::HTTP)
        , m_url(std::string(util::rstrip(url, '/')))
        , m_next_retry(std::chrono::system_clock::now())
        , m_retry_wait_seconds(std::chrono::milliseconds(200))
        , m_retry_backoff_factor(2)
        , m_retry_counter(0)
    {
    }

    Mirror::~Mirror() = default;

    Protocol Mirror::protocol() const
    {
        return m_protocol;
    }

    const std::string& Mirror::url() const
    {
        return m_url;
    }

    std::chrono::system_clock::time_point Mirror::next_retry() const
    {
        return m_next_retry;
    }

    const MirrorStats& Mirror::stats() const
    {
        return m_stats;
    }

    void Mirror::set_max_ranges(std::size_t max_ranges)
    {
        m_stats.max_ranges = max_ranges;
    }

    void Mirror::set_allowed_parallel_connections(long allowed_parallel_connections)
    {
        m_stats.allowed_parallel_connections = allowed_parallel_connections;
    }

    bool Mirror::need_wait_for_retry() const
    {
        return m_retry_counter != 0 && m_next_retry > std::chrono::system_clock::now();
    }

    bool Mirror::has_running_transfers() const
    {
        return m_stats.running_transfers > 0;
    }

    void Mirror::increase_running_transfers()
    {
        m_stats.running_transfers++;
        if (m_stats.max_tried_parallel_connections < m_stats.running_transfers)
        {
            m_stats.max_tried_parallel_connections = m_stats.running_transfers;
        }
    }

    bool Mirror::is_parallel_connections_limited_and_reached() const
    {
        return m_stats.allowed_parallel_connections != -1
               && m_stats.running_transfers >= std::size_t(m_stats.allowed_parallel_connections);
    }

    void Mirror::update_statistics(bool transfer_success)
    {
        m_stats.running_transfers--;
        if (transfer_success)
        {
            m_stats.successful_transfers++;
        }
        else
        {
            m_stats.failed_transfers++;
            if (m_stats.failed_transfers == 1 || m_next_retry < std::chrono::system_clock::now())
            {
                m_retry_counter++;
                m_retry_wait_seconds = m_retry_wait_seconds * m_retry_backoff_factor;
                m_next_retry = std::chrono::system_clock::now() + m_retry_wait_seconds;
            }
        }
    }

    std::string Mirror::format_url(const std::string& path) const
    {
        return util::join_url(m_url, path);
    }

    std::vector<std::string> Mirror::get_auth_headers(const std::string&) const
    {
        return {};
    }

    bool Mirror::needs_preparation(const std::string&) const
    {
        return false;
    }

    void Mirror::prepare(const std::string&, CURLHandle&)
    {
    }

    /*****************************
     * HTTPMirror implementation *
     *****************************/

    HTTPMirror::HTTPMirror(const std::string& url)
        : Mirror(url)
    {
    }

    HTTPMirror::~HTTPMirror() = default;

    // TODO Maybe move this curl stuff somewhere else?
    // Or remove it if authentication won't be used
    bool
    HTTPMirror::authenticate(CURLHandle& handle, const std::string& user, const std::string& password)
    {
        if (!password.empty() && !user.empty())
        {
            handle.set_opt(CURLOPT_USERNAME, user);
            handle.set_opt(CURLOPT_PASSWORD, password);
            return true;
        }
        else
        {
            LOG_WARNING << "Cannot authenticate: user or password empty";
            return false;
        }
    }

    // Utility function
    std::pair<std::string, std::string> split_path_tag(const std::string& path)
    {
        // for OCI, if we have a filename like "xtensor-0.23.10-h2acdbc0_0.tar.bz2"
        // we want to split it to `xtensor:0.23.10-h2acdbc0-0`
        if (util::ends_with(path, ".json"))
        {
            return { path, "latest" };
        }

        std::pair<std::string, std::string> result;
        auto parts = util::rsplit(path, "-", 2);

        if (parts.size() < 2)
        {
            LOG_ERROR << "Could not split filename into enough parts";
            throw std::runtime_error("Could not split filename into enough parts");
        }

        result.first = parts[0];

        std::string tag;
        if (parts.size() > 2)
        {
            std::string last_part = parts[2].substr(0, parts[2].find_first_of("."));
            tag = fmt::format("{}-{}", parts[1], last_part);
        }
        else
        {
            tag = parts[1];
        }

        util::replace_all(tag, "_", "-");
        result.second = tag;

        LOG_INFO << "Splitting " << path << " to name: " << result.first << " tag: " << result.second;
        return result;
    }

    /*****************************
     * OCIMirror implementation *
     *****************************/

    // OCI Mirror:
    // When knowing the SHA256 we can directly get to the blob
    // When we do not know the SHA256 sum, we need to find the `latest` or some
    // other blob

    // OCI upload process
    // 4 steps:
    //  - first get auth token with push rights
    //  - then

    // This is what an OCI manifest (index) looks like:
    // {
    //     "schemaVersion": 2,
    //     "config": {
    //         "mediaType": "application/vnd.unknown.config.v1+json",
    //         "digest":
    //         "sha256:e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
    //         "size": 0
    //     },
    //     "layers": [
    //         {
    //             "mediaType": "application/vnd.unknown.layer.v1+txt",
    //             "digest":
    //             "sha256:c5be3ea75353851e1fcf3a298af3b6cfd2af3d7ff018ce52657b6dbd8f986aa4",
    //             "size": 13,
    //             "annotations": {
    //                 "org.opencontainers.image.title": "artifact.txt"
    //             }
    //         }
    //     ]
    // }

    // Used for pull/read (doesn't need auth)
    OCIMirror::OCIMirror(
        const std::string& host,
        const std::string& repo_prefix,
        const proxy_map_type& proxy_map
    )
        : Mirror(host)
        , m_repo_prefix(repo_prefix)
        , m_scope("pull")
        , m_proxy_map(proxy_map)
    {
        m_protocol = Protocol::OCI;
    }

    OCIMirror::OCIMirror(
        const std::string& host,
        const std::string& repo_prefix,
        const std::string& scope,
        const std::string& username,
        const std::string& password,
        const proxy_map_type& proxy_map
    )
        : Mirror(host)
        , m_repo_prefix(repo_prefix)
        , m_scope(scope)
        , m_username(username)
        , m_password(password)
        , m_proxy_map(proxy_map)
    {
        m_protocol = Protocol::OCI;
    }

    OCIMirror::~OCIMirror() = default;

    std::string OCIMirror::get_repo(const std::string& repo) const
    {
        if (!m_repo_prefix.empty())
        {
            return fmt::format("{}/{}", m_repo_prefix, repo);
        }
        else
        {
            return repo;
        }
    }

    std::string OCIMirror::get_auth_url(const std::string& repo, const std::string& scope) const
    {
        return fmt::format("{}/token?scope=repository:{}:{}", this->url(), get_repo(repo), scope);
    }

    std::string OCIMirror::get_manifest_url(const std::string& repo, const std::string& reference) const
    {
        return fmt::format("{}/v2/{}/manifests/{}", this->url(), get_repo(repo), reference);
    }

    std::string OCIMirror::get_preupload_url(const std::string& repo) const
    {
        return fmt::format("{}/v2/{}/blobs/uploads/", this->url(), get_repo(repo));
    }

    bool OCIMirror::need_auth() const
    {
        return !m_username.empty() && !m_password.empty();
    }

    std::string OCIMirror::format_url(const std::string& path) const
    {
        // TODO Maybe we would need a std::string checksum to pass as arg (to be empty in Mirror)
        // (in case the map data is not set yet)
        // to be checked in use/call of this method

        auto* data = get_data(path);
        if (data)
        {
            auto [split_path, split_tag] = split_path_tag(path);
            // Should be this format:
            // https://ghcr.io/v2/wolfv/artifact/blobs/sha256:c5be3ea75353851e1fcf3a298af3b6cfd2af3d7ff018ce52657b6dbd8f986aa4
            return fmt::format(
                "{}/v2/{}/blobs/sha256:{}",
                this->url(),
                get_repo(split_path),
                data->sha256sum
            );
        }
        else
        {
            LOG_ERROR << "Checksum corresponding to " << path << " is not available";
            return {};
        }
    }

    std::vector<std::string> OCIMirror::get_auth_headers(const std::string& path) const
    {
        if (m_username.empty() && m_password.empty())
        {
            return {};
        }

        auto* data = get_data(path);
        if (data)
        {
            return { fmt::format("Authorization: Bearer {}", data->token) };
        }
        else
        {
            return {};
        }
    }

    bool OCIMirror::needs_preparation(const std::string& path) const
    {
        auto* data = get_data(path);
        if ((!data || (data && data->token.empty())) && need_auth())
        {
            return true;
        }

        if (!data || (data && data->sha256sum.empty()))
        {
            return true;
        }

        return false;
    }

    void OCIMirror::prepare(const std::string& path, CURLHandle& handle)
    {
        auto [split_path, split_tag] = split_path_tag(path);

        auto* data = get_data(path);
        if (!data)
        {
            m_path_map[split_path].reset(new AuthData);
            data = m_path_map[split_path].get();
        }

        if (data && data->token.empty() && need_auth())
        {
            std::string auth_url = get_auth_url(split_path, m_scope);

            // TODO to be checked
            // Not sure this is necessary
            // Do we need to call curl config beforehand using the right args in
            // `configure_curl_handle`
            handle.set_url(auth_url, m_proxy_map);
            // IMPORTANT: Make sure the callbacks are set before calling this
            // i.e CURLOPT_HEADERFUNCTION, CURLOPT_HEADERDATA, CURLOPT_WRITEFUNCTION,
            // CURLOPT_WRITEDATA

            if (!m_username.empty())
            {
                handle.set_opt(CURLOPT_USERNAME, m_username);
            }

            if (!m_password.empty())
            {
                handle.set_opt(CURLOPT_PASSWORD, m_password);
            }

            auto transfer_end_callback =
                [&data](bool success, const std::string& token, const std::string&)  // Args got as
                                                                                     // a response
                                                                                     // to transfer
            {
                if (!success)
                {
                    return false;  // ERROR result to be modified to enum/struct?
                }

                if (!token.empty())
                {
                    data->token = token;
                    return true;  // OK result to be modified to enum/struct?
                }
                return false;  // ERROR result to be modified to enum/struct?
            };

            // TODO set here a callback to `transfer_end_callback` in CURLHandle or other relevant
            // class which performs the transfer finalization and sets data token
            // handle.set_end_callback(transfer_end_callback);
        }
        else
        {
            std::string manifest_url = get_manifest_url(split_path, split_tag);

            handle.set_url(manifest_url, m_proxy_map)
                .add_headers(get_auth_headers(path))
                .add_header("Accept: application/vnd.oci.image.manifest.v1+json");

            auto finalize_manifest_callback =
                [&data](bool success, const std::string&, const std::string& digest)
            {
                if (!success)
                {
                    return false;  // ERROR result to be modified to enum/struct?
                }

                // digest is got from a json response like
                // j["layers"][0]["digest"]
                if (!digest.empty())
                {
                    assert(util::starts_with(digest, "sha256:"));

                    data->sha256sum = digest.substr(sizeof("sha256:") - 1);
                    return true;  // OK result to be modified to enum/struct?
                }

                return false;  // ERROR result to be modified to enum/struct?
            };

            // TODO set here a callback to `finalize_manifest_callback` in CURLHandle or other
            // relevant class which performs the transfer finalization and sets data sha256sum
            // handle.set_end_callback(finalize_manifest_callback);
        }
    }

    OCIMirror::AuthData* OCIMirror::get_data(const std::string& path) const
    {
        auto [split_path, _] = split_path_tag(path);
        auto it = m_path_map.find(split_path);
        if (it != m_path_map.end())
        {
            return it->second.get();
        }
        return nullptr;
    }

}  // namespace mamba
