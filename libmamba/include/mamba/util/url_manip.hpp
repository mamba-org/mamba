// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_UTIL_URL_MANIP_HPP
#define MAMBA_UTIL_URL_MANIP_HPP

#include <string>
#include <string_view>

namespace mamba::util
{
    /**
     * If @p url starts with a scheme, return it, otherwise return empty string.
     *
     * Does not include "://"
     */
    [[nodiscard]] auto url_get_scheme(std::string_view url) -> std::string_view;

    /*
     * Return true if @p url is a file URI, i.e. if it starts with "file://".
     */
    [[nodiscard]] auto is_file_uri(std::string_view url) -> bool;

    /**
     * Return true if @p url starts with a URL scheme.
     */
    [[nodiscard]] auto url_has_scheme(std::string_view url) -> bool;

    /**
     * Transform an absolute path to a %-encoded "file://" URL.
     */
    [[nodiscard]] auto abs_path_to_url(std::string_view path) -> std::string;

    /**
     * Transform an absolute path to a %-encoded "file://" URL.
     *
     * Does nothing if the input is already has a URL scheme.
     */
    [[nodiscard]] auto abs_path_or_url_to_url(std::string_view path) -> std::string;

    /**
     * Transform an absolute or relative path to a %-encoded "file://" URL.
     */
    [[nodiscard]] auto path_to_url(std::string_view path) -> std::string;

    /**
     * Transform an absolute or relative path to a %-encoded "file://" URL.
     *
     * Does nothing if the input is already has a URL scheme.
     */
    [[nodiscard]] auto path_or_url_to_url(std::string_view path) -> std::string;

    /**
     * Join folder elements of a URL.
     *
     * Concatenate arguments making sure they are separated by a unique slash separator.
     *
     * @see path_concat
     */
    template <typename... Args>
    [[nodiscard]] auto url_concat(const Args&... args) -> std::string;

    [[nodiscard]] auto make_curl_compatible(std::string url) -> std::string;

    /**
     * Convert UNC2 file URI to UNC4.
     *
     * Windows paths can be expressed in a form, called UNC, where it is possible to express a
     * server location, as in "\\hostname\folder\data.xml".
     * This can be successfully encoded in a file URI like "file://hostname/folder/data.xml"
     * since file URI contain a part for the hostname (empty hostname file URI must start with
     * "file:///").
     * Since CURL does not support hostname in file URI, we can encode UNC hostname as part
     * of the path (called 4-slash), where it becomes "file:////hostname/folder/data.xml".
     *
     * This function leaves all non-matching URI (including a number of invalid URI for unknown
     * legacy reasons taken from ``url_to_path`` in conda.common.path) unchanged.
     *
     * @see https://learn.microsoft.com/en-us/dotnet/standard/io/file-path-formats#unc-paths
     * @see https://en.wikipedia.org/wiki/File_URI_scheme
     */
    [[nodiscard]] auto file_uri_unc2_to_unc4(std::string_view url) -> std::string;

    /********************
     *  Implementation  *
     ********************/

    namespace detail
    {
        inline auto as_string_view(std::string_view str) -> std::string_view
        {
            return str;
        }

        inline auto as_string_view(const char& c) -> std::string_view
        {
            return { &c, 1 };
        }

        template <typename... Args>
        auto url_concat_impl(const Args&... args) -> std::string
        {
            auto join_two = [](std::string& out, std::string_view to_add)
            {
                if (!out.empty() && !to_add.empty())
                {
                    const bool out_has_slash = out.back() == '/';
                    const bool to_add_has_slash = to_add.front() == '/';
                    if (out_has_slash && to_add_has_slash)
                    {
                        to_add = to_add.substr(1);
                    }
                    if (!out_has_slash && !to_add_has_slash)
                    {
                        out += '/';
                    }
                }
                out += to_add;
            };

            std::string result;
            result.reserve(((args.size() + 1) + ...));
            (join_two(result, args), ...);
            return result;
        }
    }

    template <typename... Args>
    auto url_concat(const Args&... args) -> std::string
    {
        return detail::url_concat_impl(detail::as_string_view(args)...);
    }
}
#endif
