// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_DOWNLOAD_MIRROR_HPP
#define MAMBA_DOWNLOAD_MIRROR_HPP

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <fmt/core.h>

#include "mamba/download/request.hpp"
#include "mamba/util/synchronized_value.hpp"

namespace mamba::download
{
    class MirrorID
    {
    public:

        explicit MirrorID(std::string v);

        std::string to_string() const;

        friend bool operator<(const MirrorID& lhs, const MirrorID& rhs);
        friend bool operator==(const MirrorID& lhs, const MirrorID& rhs);

    private:

        std::string m_value;
    };

    struct MirrorRequest : RequestBase
    {
        using header_list = std::vector<std::string>;

        std::string url;
        header_list headers;
        bool is_repodata_zst;
        std::string username = {};
        std::string password = {};

        MirrorRequest(
            std::string_view name,
            std::string_view url,
            header_list headers = {},
            bool is_repodata_zst = false
        );
        MirrorRequest(
            const RequestBase& base,
            std::string_view url,
            header_list headers = {},
            bool is_repodata_zst = false
        );

        ~MirrorRequest() = default;
        MirrorRequest(const MirrorRequest&) = default;
        MirrorRequest& operator=(const MirrorRequest&) = default;
        MirrorRequest(MirrorRequest&&) = default;
        MirrorRequest& operator=(MirrorRequest&&) = default;
    };

    struct MirrorStats  // Moved out of Mirror internals because of compilers not agreeing:
                        // https://godbolt.org/z/GcjWhrb9W
    {
        std::optional<std::size_t> allowed_connections = std::nullopt;
        std::size_t max_tried_connections = 0;
        std::size_t running_transfers = 0;
        std::size_t successful_transfers = 0;
        std::size_t failed_transfers = 0;
    };

    // A Mirror represents a location from where an asset can be downloaded.
    // It handles the generation of required requests to get the asset, and
    // provides some statistics about its usage.
    class Mirror
    {
    public:

        using request_generator = std::function<MirrorRequest(const Request&, const Content*)>;
        using request_generator_list = std::vector<request_generator>;

        virtual ~Mirror() = default;

        Mirror(const Mirror&) = delete;
        Mirror& operator=(const Mirror&) = delete;
        Mirror(Mirror&&) = delete;
        Mirror& operator=(Mirror&&) = delete;

        const MirrorID& id() const;
        request_generator_list
        get_request_generators(const std::string& url_path, const std::string& spec_sha256) const;

        std::size_t max_retries() const;
        std::size_t successful_transfers() const;
        std::size_t failed_transfers() const;

        bool can_accept_more_connections() const;
        bool can_retry_with_fewer_connections() const;

        void cap_allowed_connections();
        void increase_running_transfers();
        void update_transfers_done(bool success, bool record_success);

    protected:

        explicit Mirror(MirrorID id, std::size_t max_retries = 3);

    private:

        virtual request_generator_list get_request_generators_impl(const std::string&, const std::string&) const = 0;

        MirrorID m_id;
        size_t m_max_retries;

        static_assert(std::default_initializable<MirrorStats>);

        util::synchronized_value<MirrorStats> m_stats;
    };

    std::unique_ptr<Mirror> make_mirror(std::string url);

}

#endif
