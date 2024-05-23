// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_DL_MIRROR_IMPL_HPP
#define MAMBA_DL_MIRROR_IMPL_HPP

#include <unordered_map>

#include "mamba/download/mirror.hpp"

namespace mamba::download
{
    // TODO: This class will be renamed FileMirror when
    // other mirrors have been plugged. It is used
    // for everything to ensure a smooth transition
    class PassThroughMirror : public Mirror
    {
    public:

        PassThroughMirror();

        static MirrorID make_id();

    private:

        using request_generator_list = Mirror::request_generator_list;
        request_generator_list
        get_request_generators_impl(const std::string&, const std::string&) const override;
    };

    class HTTPMirror : public Mirror
    {
    public:

        explicit HTTPMirror(std::string url);

        static MirrorID make_id(std::string url);

    private:

        using request_generator_list = Mirror::request_generator_list;
        request_generator_list
        get_request_generators_impl(const std::string&, const std::string&) const override;

        std::string m_url;
    };

    class OCIMirror : public Mirror
    {
    public:

        // NOTE
        // `scope` could be: `pull`(download), `push`(upload) or `pull_push`
        // Only `pull` is supported for now
        // Pulling artifacts can be performed anonymously
        // (by requesting a token)
        // This may also be done by setting credentials (username and password),
        // but this is not supported for now as credentials
        // are more used (and needed) with `push` and `pull_push` scope
        explicit OCIMirror(
            std::string url,
            std::string repo_prefix,
            std::string scope,
            std::string username = {},
            std::string password = {}
        );

        static MirrorID make_id(std::string url);

    private:

        struct AuthenticationData
        {
            std::string sha256sum = {};  // TODO what about other checksums types? i.e md5
            std::string token = {};
            bool is_repodata_zst = false;
        };

        using request_generator_list = Mirror::request_generator_list;
        request_generator_list
        get_request_generators_impl(const std::string& url_path, const std::string& spec_sha256)
            const override;

        MirrorRequest
        build_authentication_request(const Request& initial_request, const std::string& split_path) const;

        MirrorRequest build_manifest_request(
            const Request& initial_request,
            const std::string& split_path,
            const std::string& split_tag
        ) const;

        MirrorRequest
        build_blob_request(const Request& initial_request, const std::string& split_path) const;

        bool need_authentication() const;
        std::string get_repo(const std::string& repo) const;
        std::string get_authentication_url(const std::string& repo) const;
        std::string get_authentication_header(const std::string& token) const;
        std::string get_manifest_url(const std::string& repo, const std::string& reference) const;
        std::string get_blob_url(const std::string& repo, const std::string& sha256sum) const;
        AuthenticationData* get_authentication_data(const std::string& split_path) const;

        std::string m_url;
        std::string m_repo_prefix;
        std::string m_scope;
        std::string m_username;
        std::string m_password;
        mutable std::unordered_map<std::string, std::unique_ptr<AuthenticationData>> m_path_map;
    };
}

#endif
