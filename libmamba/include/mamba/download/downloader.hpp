// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_DOWNLOAD_DOWNLOADER_HPP
#define MAMBA_DOWNLOAD_DOWNLOADER_HPP

#include <string>

#include <tl/expected.hpp>

#include "mamba/download/mirror_map.hpp"
#include "mamba/download/parameters.hpp"
#include "mamba/download/request.hpp"
#include "mamba/specs/authentication_info.hpp"

namespace mamba::download
{
    class Monitor
    {
    public:

        virtual ~Monitor() = default;

        Monitor(const Monitor&) = delete;
        Monitor& operator=(const Monitor&) = delete;
        Monitor(Monitor&&) = delete;
        Monitor& operator=(Monitor&&) = delete;

        void observe(MultiRequest& requests, Options& options);
        void on_done();
        void on_unexpected_termination();

    protected:

        Monitor() = default;

    private:

        virtual void observe_impl(MultiRequest& requests, Options& options) = 0;
        virtual void on_done_impl() = 0;
        virtual void on_unexpected_termination_impl() = 0;
    };

    MultiResult download(
        MultiRequest requests,
        const mirror_map& mirrors,
        const RemoteFetchParams& params,
        const specs::AuthenticationDataBase& auth_info,
        Options options = {},
        Monitor* monitor = nullptr
    );

    Result download(
        Request request,
        const mirror_map& mirrors,
        const RemoteFetchParams& params,
        const specs::AuthenticationDataBase& auth_info,
        Options options = {},
        Monitor* monitor = nullptr
    );

    bool check_resource_exists(const std::string& url, const RemoteFetchParams& params);
}

#endif
