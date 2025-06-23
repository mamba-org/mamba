// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <fmt/format.h>

#include "mamba/download/mirror.hpp"

namespace mamba::download
{
    /***************************
     * MirrorID implementation *
     ***************************/

    MirrorID::MirrorID(std::string v)
        : m_value(std::move(v))
    {
    }

    std::string MirrorID::to_string() const
    {
        return fmt::format("MirrorID <{}>", m_value);
    }

    bool operator<(const MirrorID& lhs, const MirrorID& rhs)
    {
        return lhs.m_value < rhs.m_value;
    }

    bool operator==(const MirrorID& lhs, const MirrorID& rhs)
    {
        return lhs.m_value == rhs.m_value;
    }

    /*****************
     * MirrorRequest *
     *****************/
    MirrorRequest::MirrorRequest(
        std::string_view lname,
        std::string_view lurl,
        header_list lheaders,
        bool lis_repodata_zst
    )
        : RequestBase(lname, std::nullopt, false, false)
        , url(lurl)
        , headers(std::move(lheaders))
        , is_repodata_zst(lis_repodata_zst)
    {
    }

    MirrorRequest::MirrorRequest(
        const RequestBase& base,
        std::string_view lurl,
        header_list lheaders,
        bool lis_repodata_zst
    )
        : RequestBase(base)
        , url(lurl)
        , headers(std::move(lheaders))
        , is_repodata_zst(lis_repodata_zst)
    {
    }

    /*************************
     * Mirror implementation *
     *************************/

    Mirror::Mirror(MirrorID id, std::size_t max_retries)
        : m_id(std::move(id))
        , m_max_retries(max_retries)
    {
    }

    const MirrorID& Mirror::id() const
    {
        return m_id;
    }

    auto
    Mirror::get_request_generators(const std::string& url_path, const std::string& spec_sha256) const
        -> request_generator_list
    {
        return get_request_generators_impl(url_path, spec_sha256);
    }

    std::size_t Mirror::max_retries() const
    {
        return m_max_retries;
    }

    std::size_t Mirror::successful_transfers() const
    {
        return m_stats->successful_transfers;
    }

    std::size_t Mirror::failed_transfers() const
    {
        return m_stats->failed_transfers;
    }

    bool Mirror::can_accept_more_connections() const
    {
        const auto stats = m_stats.synchronize();
        return !stats->allowed_connections.has_value()
               || stats->running_transfers < stats->allowed_connections;
    }

    bool Mirror::can_retry_with_fewer_connections() const
    {
        const auto stats = m_stats.synchronize();
        return stats->running_transfers > 0
               || (stats->successful_transfers > 0
                   && stats->failed_transfers < stats->max_tried_connections);
    }

    namespace
    {
        using lock_guard = std::lock_guard<std::mutex>;
    }

    void Mirror::cap_allowed_connections()
    {
        auto stats = m_stats.synchronize();
        if (stats->running_transfers > 0)
        {
            stats->allowed_connections = stats->running_transfers;
        }
        else
        {
            stats->allowed_connections = std::size_t(1);
        }
    }

    void Mirror::increase_running_transfers()
    {
        auto stats = m_stats.synchronize();
        ++stats->running_transfers;
        if (stats->max_tried_connections < stats->running_transfers)
        {
            stats->max_tried_connections = stats->running_transfers;
        }
    }

    void Mirror::update_transfers_done(bool success, bool record_success)
    {
        auto stats = m_stats.synchronize();
        --stats->running_transfers;
        if (record_success)
        {
            if (success)
            {
                ++stats->successful_transfers;
            }
            else
            {
                ++stats->failed_transfers;
            }
        }
    }
}
