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
    MirrorRequest::MirrorRequest(std::string_view lname, std::string_view lurl, header_list lheaders)
        : RequestBase(lname, std::nullopt, false, false)
        , url(lurl)
        , headers(std::move(lheaders))
    {
    }

    MirrorRequest::MirrorRequest(const RequestBase& base, std::string_view lurl, header_list lheaders)
        : RequestBase(base)
        , url(lurl)
        , headers(std::move(lheaders))
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
        return m_successful_transfers;
    }

    std::size_t Mirror::failed_transfers() const
    {
        return m_failed_transfers;
    }

    bool Mirror::can_accept_more_connections() const
    {
        return !m_allowed_connections.has_value() || m_running_transfers < m_allowed_connections;
    }

    bool Mirror::can_retry_with_fewer_connections() const
    {
        return m_running_transfers > 0
               || (m_successful_transfers > 0 && m_failed_transfers < m_max_tried_connections);
    }

    namespace
    {
        using lock_guard = std::lock_guard<std::mutex>;
    }

    void Mirror::cap_allowed_connections()
    {
        lock_guard lock(m_stats_mutex);
        if (m_running_transfers > 0)
        {
            m_allowed_connections = m_running_transfers;
        }
        else
        {
            m_allowed_connections = std::size_t(1);
        }
    }

    void Mirror::increase_running_transfers()
    {
        lock_guard lock(m_stats_mutex);
        ++m_running_transfers;
        if (m_max_tried_connections < m_running_transfers)
        {
            m_max_tried_connections = m_running_transfers;
        }
    }

    void Mirror::update_transfers_done(bool success, bool record_success)
    {
        lock_guard lock(m_stats_mutex);
        --m_running_transfers;
        if (record_success)
        {
            if (success)
            {
                ++m_successful_transfers;
            }
            else
            {
                ++m_failed_transfers;
            }
        }
    }
}
