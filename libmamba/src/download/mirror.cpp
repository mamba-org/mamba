#include <fmt/format.h>
#include "mamba/download/mirror.hpp"

namespace mamba
{
    /***************************
     * MirrorID implementation *
     ***************************/

    MirrorID::MirrorID(std::string_view v)
        : m_value(v)
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
    
    /*************************
     * MirrorDownloadRequest *
     *************************/

    MirrorDownloadRequest::MirrorDownloadRequest(
        const DownloadRequestBase& base,
        std::string_view lurl,
        std::optional<header_list> lheaders
    )
        : DownloadRequestBase(base)
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

    auto Mirror::get_request_generators() const -> request_generator_list
    {
        return get_request_generators_impl();
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
        return !m_allowed_connections.has_value() ||
            m_running_transfers < m_allowed_connections;
    }

    bool Mirror::can_retry_with_fewer_connections() const
    {
        return m_running_transfers > 0 ||
            (m_successful_transfers > 0 && m_failed_transfers < m_max_tried_connections);
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

    void Mirror::update_transfers_done(bool success)
    {
        lock_guard lock(m_stats_mutex);
        --m_running_transfers;
        if (success)
        {
            ++m_successful_transfers;
        }
        else
        {
            ++m_failed_transfers;
        }
    }

    /************************************
     * PassThroughMirror implementation *
     ************************************/

    PassThroughMirror::PassThroughMirror()
        : Mirror(PassThroughMirror::id())
    {
    }

    MirrorID PassThroughMirror::id()
    {
        return MirrorID("");
    }

    auto PassThroughMirror::get_request_generators_impl() const -> request_generator_list 
    {
        return
        {
            [](const DownloadRequest& dl_request, const DownloadContent*)
            {
                return MirrorDownloadRequest(dl_request, dl_request.url_path);
            }
        };
    }
    
    /******************************
     * make_mirror implementation *
     ******************************/

    std::unique_ptr<Mirror> make_mirror(std::string_view url)
    {
        if (url.empty())
        {
            return std::make_unique<PassThroughMirror>();
        }
        return nullptr;
    }
}

