#include "mamba/download/request.hpp"

namespace mamba::download
{
    RequestBase::RequestBase(
        std::string_view lname,
        std::optional<std::string> lfilename,
        bool lcheck_only,
        bool lignore_failure
    )
        : name(lname)
        , filename(lfilename)
        , check_only(lcheck_only)
        , ignore_failure(lignore_failure)
    {
    }

    Request::Request(
        std::string_view lname,
        MirrorName lmirror_name,
        std::string_view lurl_path,
        std::optional<std::string> lfilename,
        bool lcheck_only,
        bool lignore_failure
    )
        : RequestBase(lname, std::move(lfilename), lcheck_only, lignore_failure)
        , mirror_name(lmirror_name)
        , url_path(lurl_path)
    {
    }
}
