// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_MIRROR_HPP
#define MAMBA_CORE_MIRROR_HPP

#include <chrono>
#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace mamba
{
    class CURLHandle;

    enum class Protocol
    {
        FILE,
        FTP,
        HTTP,
        OCI,
    };

    // TODO make `allowed_parallel_connections` (i.e `max_downloads_per_mirror`) configurable later?
    // Statistics of mirror
    struct MirrorStats
    {
        // Maximum number of allowed parallel connections to this mirror. -1 means no
        // limit. Dynamically adjusted (decreased) if no fatal (temporary) error will
        // occur.
        long allowed_parallel_connections = -1;

        // The maximum number of tried parallel connections to this mirror
        // (including unsuccessful).
        std::size_t max_tried_parallel_connections = 0;

        // How many transfers from this mirror are currently in progress.
        std::size_t running_transfers = 0;

        // How many transfers was finished successfully from the mirror.
        std::size_t successful_transfers = 0;

        // How many transfers failed.
        std::size_t failed_transfers = 0;

        // Maximum ranges supported in a single request. This will be automatically
        // adjusted when mirrors respond with 200 to a range request
        // zchunk case
        std::size_t max_ranges = 256;
    };

    class Mirror
    {
    public:

        Mirror(const std::string& url);

        virtual ~Mirror();

        Mirror(const Mirror&) = delete;
        Mirror& operator=(const Mirror&) = delete;
        Mirror(Mirror&&) = delete;
        Mirror& operator=(Mirror&&) = delete;

        Protocol protocol() const;
        const std::string& url() const;
        std::chrono::system_clock::time_point next_retry() const;
        const MirrorStats& stats() const;

        void set_max_ranges(std::size_t max_ranges);
        void set_allowed_parallel_connections(long allowed_parallel_connections);

        bool need_wait_for_retry() const;
        bool has_running_transfers() const;

        void increase_running_transfers();
        bool is_parallel_connections_limited_and_reached() const;
        void update_statistics(bool transfer_success);

        virtual std::string format_url(const std::string& path) const;
        virtual std::string get_auth_header(const std::string& path) const;
        virtual bool needs_preparation(const std::string& path) const;
        virtual void prepare(const std::string& path, CURLHandle& handle);

    protected:

        Protocol m_protocol;

    private:

        const std::string m_url;

        // TODO put these in a struct?
        // Retry & backoff params
        std::chrono::system_clock::time_point m_next_retry;
        std::chrono::system_clock::duration m_retry_wait_seconds;
        std::size_t m_retry_backoff_factor;
        // count number of retries (this is not the same as failed transfers, as mutiple
        // transfers can be started at the same time, but should all be retried only once)
        std::size_t m_retry_counter;

        MirrorStats m_stats;
    };

    class HTTPMirror : public Mirror
    {
    public:

        HTTPMirror(const std::string& url);
        ~HTTPMirror();

        bool authenticate(CURLHandle& handle, const std::string& user, const std::string& password);
    };

    // Utility function
    // TODO leave it here or put it in a namespace, or move it to utils?
    std::pair<std::string, std::string> split_path_tag(const std::string& path);

    class OCIMirror : public Mirror
    {
    public:

        using proxy_map_type = std::map<std::string, std::string>;

        OCIMirror(const std::string& host, const std::string& repo_prefix, const proxy_map_type& proxy_map);

        OCIMirror(
            const std::string& host,
            const std::string& repo_prefix,
            const std::string& scope,
            const std::string& username,
            const std::string& password,
            const proxy_map_type& proxy_map
        );
        ~OCIMirror();


        std::string get_repo(const std::string& repo) const;
        std::string get_auth_url(const std::string& repo, const std::string& scope) const;
        std::string get_manifest_url(const std::string& repo, const std::string& reference) const;
        std::string get_preupload_url(const std::string& repo) const;

        bool need_auth() const;

        std::string format_url(const std::string& path) const override;
        std::string get_auth_header(const std::string& path) const override;
        bool needs_preparation(const std::string& path) const override;
        void prepare(const std::string& path, CURLHandle& handle) override;

    private:

        struct AuthData
        {
            std::string sha256sum;  // TODO what about other checksums types? i.e md5
            std::string token;
        };

        std::map<std::string, std::unique_ptr<AuthData>> m_path_map;
        std::string m_repo_prefix;
        std::string m_scope;
        std::string m_username;
        std::string m_password;

        // TODO do we really need this here? (only needs in `prepare` so far, add it as an arg
        // there?)
        proxy_map_type m_proxy_map;

        AuthData* get_data(const std::string& path) const;
    };

}  // namespace mamba

#endif  // MAMBA_CORE_MIRROR_HPP
