#include "mamba/download/request.hpp"

namespace mamba
{
    DownloadRequestBase::DownloadRequestBase(
        std::string_view lname,
        std::optional<std::string> lfilename,
        bool lhead_only,
        bool lignore_failure
    )
        : name(lname)
        , filename(lfilename)
        , head_only(lhead_only)
        , ignore_failure(lignore_failure)
    {
    }

    DownloadRequest::DownloadRequest(
        std::string_view lname,
        std::string_view lmirror_name,
        std::string_view lurl_path,
        std::optional<std::string> lfilename,
        bool lhead_only,
        bool lignore_failure
    )
        : DownloadRequestBase(lname, std::move(lfilename), lhead_only, lignore_failure)
        , mirror_name(lmirror_name)
        , url_path(lurl_path)
    {
    }
}
