// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_DOWNLOAD_REQUEST_HPP
#define MAMBA_DOWNLOAD_REQUEST_HPP

#include <functional>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "mamba/core/error_handling.hpp"

namespace mamba::download
{
    /*******************************
     * Download results structures *
     *******************************/

    struct TransferData
    {
        int http_status = 0;
        std::string effective_url = "";
        std::size_t downloaded_size = 0;
        std::size_t average_speed_Bps = 0;
    };

    struct Filename
    {
        std::string value = "";
    };

    struct Buffer
    {
        std::string value = "";
    };

    using Content = std::variant<Filename, Buffer>;

    struct Success
    {
        Content content = {};
        TransferData transfer = {};
        std::string cache_control = "";
        std::string etag = "";
        std::string last_modified = "";
        std::size_t attempt_number = std::size_t(1);
    };

    struct Error
    {
        std::string message = "";
        std::optional<std::size_t> retry_wait_seconds = std::nullopt;
        std::optional<TransferData> transfer = std::nullopt;
        std::size_t attempt_number = std::size_t(1);
    };

    using Result = tl::expected<Success, Error>;
    using MultiResult = std::vector<Result>;

    /*****************************
     * Download event structures *
     *****************************/

    struct Progress
    {
        std::size_t downloaded_size = 0;
        std::size_t total_to_download = 0;
        std::size_t speed_Bps = 0;
    };

    using Event = std::variant<Progress, Error, Success>;

    /*******************************
     * Download request structures *
     *******************************/

    struct RequestBase
    {
        using progress_callback_t = std::function<void(const Event&)>;

        // TODO: remove these functions when we plug a library with continuation
        using on_success_callback_t = std::function<expected_t<void>(const Success&)>;
        using on_failure_callback_t = std::function<void(const Error&)>;

        std::string name;
        // If filename is not initialized, the data will be downloaded
        // to an internal in-memory buffer.
        std::optional<std::string> filename;
        bool check_only;
        bool ignore_failure;
        std::optional<std::size_t> expected_size = std::nullopt;
        std::optional<std::string> etag = std::nullopt;
        std::optional<std::string> last_modified = std::nullopt;

        std::optional<progress_callback_t> progress = std::nullopt;
        std::optional<on_success_callback_t> on_success = std::nullopt;
        std::optional<on_failure_callback_t> on_failure = std::nullopt;

    protected:

        RequestBase(
            std::string_view lname,
            std::optional<std::string> lfilename,
            bool lcheck_only,
            bool lignore_failure
        );

        ~RequestBase() = default;
        RequestBase(const RequestBase&) = default;
        RequestBase& operator=(const RequestBase&) = default;
        RequestBase(RequestBase&&) = default;
        RequestBase& operator=(RequestBase&&) = default;
    };

    // This class is used to create strong alias on
    // string_view. This helps to avoid error-prone
    // calls to functions that accept many arguments
    // of the same type
    template <int I>
    class string_view_alias
    {
    public:

        explicit string_view_alias(std::string_view s);
        operator std::string_view() const;

    private:

        std::string_view m_wrapped;
    };

    using MirrorName = string_view_alias<0>;

    struct Request : RequestBase
    {
        std::string mirror_name;
        std::string url_path;
        // TODO maybe we would want to use a struct instead
        // containing the `checksum` and its `type`
        // (to handle other checksums types like md5...)
        // cf. `Checksum` struct in powerloader
        std::string sha256;

        Request(
            std::string_view lname,
            MirrorName lmirror_name,
            std::string_view lurl_path,
            std::optional<std::string> lfilename = std::nullopt,
            bool lhead_only = false,
            bool lignore_failure = false
        );

        ~Request() = default;
        Request(const Request&) = default;
        Request& operator=(const Request&) = default;
        Request(Request&&) = default;
        Request& operator=(Request&&) = default;
    };

    using MultiRequest = std::vector<Request>;

    /************************************
     * string_view_alias implementation *
     ************************************/

    template <int I>
    string_view_alias<I>::string_view_alias(std::string_view s)
        : m_wrapped(s)
    {
    }

    template <int I>
    string_view_alias<I>::operator std::string_view() const
    {
        return m_wrapped;
    }
}

#endif
