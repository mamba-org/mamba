// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_DOWNLOAD_DOWNLOADER_HPP
#define MAMBA_DOWNLOAD_DOWNLOADER_HPP

#include <functional>
#include <optional>
#include <string>
#include <variant>

#include <tl/expected.hpp>

#include "mamba/core/context.hpp"
#include "mamba/core/error_handling.hpp"
#include "mamba/download/mirror_map.hpp"
#include "mamba/download/request.hpp"

namespace mamba::download
{
    struct Options
    {
        using termination_function = std::optional<std::function<void()>>;

        bool fail_fast = false;
        bool sort = true;
        termination_function on_unexpected_termination = std::nullopt;
    };

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
        const Context& context,
        Options options = {},
        Monitor* monitor = nullptr
    );

    Result download(
        Request request,
        const mirror_map& mirrors,
        const Context& context,
        Options options = {},
        Monitor* monitor = nullptr
    );

    bool check_resource_exists(const std::string& url, const Context& context);
}

#endif
