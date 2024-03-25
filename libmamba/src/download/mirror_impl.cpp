// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/util/string.hpp"
#include "mamba/util/url_manip.hpp"

#include "mirror_impl.hpp"

namespace mamba::download
{
    /************************************
     * PassThroughMirror implementation *
     ************************************/

    namespace
    {
        const auto PASSTHROUGH_MIRROR_ID = MirrorID("");
    }

    PassThroughMirror::PassThroughMirror()
        : Mirror(PassThroughMirror::make_id())
    {
    }

    MirrorID PassThroughMirror::make_id()
    {
        return PASSTHROUGH_MIRROR_ID;
    }

    auto PassThroughMirror::get_request_generators_impl(const std::string&) const
        -> request_generator_list
    {
        return { [](const Request& dl_request, const Content*)
                 { return MirrorRequest(dl_request, dl_request.url_path); } };
    }

    /*****************************
     * HTTPMirror implementation *
     *****************************/

    HTTPMirror::HTTPMirror(std::string url)
        : Mirror(HTTPMirror::make_id(url))
        , m_url(std::move(url))
    {
    }

    MirrorID HTTPMirror::make_id(std::string url)
    {
        return MirrorID(std::move(url));
    }

    auto HTTPMirror::get_request_generators_impl(const std::string&) const -> request_generator_list
    {
        return { [url = m_url](const Request& dl_request, const Content*)
                 { return MirrorRequest(dl_request, util::url_concat(url, dl_request.url_path)); } };
    }

    /******************************
     * make_mirror implementation *
     ******************************/

    std::unique_ptr<Mirror> make_mirror(std::string url)
    {
        if (url.empty())
        {
            return std::make_unique<PassThroughMirror>();
        }
        else if (util::starts_with(url, "https://") || util::starts_with(url, "http://") || util::starts_with(url, "file://"))
        {
            return std::make_unique<HTTPMirror>(std::move(url));
        }
        return nullptr;
    }
}
