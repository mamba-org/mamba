// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <spdlog/spdlog.h>

#include "mamba/core/output.hpp"
#include "mamba/util/string.hpp"
#include "mamba/util/url.hpp"
#include "mamba/util/url_manip.hpp"

#include "nlohmann/json.hpp"

#include "mirror_impl.hpp"

namespace nl = nlohmann;

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

    auto PassThroughMirror::get_request_generators_impl(const std::string&, const std::string&) const
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

    auto HTTPMirror::get_request_generators_impl(const std::string&, const std::string&) const
        -> request_generator_list
    {
        return { [url = m_url](const Request& dl_request, const Content*)
                 { return MirrorRequest(dl_request, util::url_concat(url, dl_request.url_path)); } };
    }

    /****************************
     * OCIMirror implementation *
     ****************************/

    namespace
    {
        std::pair<std::string, std::string> split_path_tag(const std::string& path)
        {
            // for OCI, if we have a filename like "xtensor-0.23.10-h2acdbc0_0.tar.bz2"
            // we want to split it to `xtensor:0.23.10-h2acdbc0-0`

            // If the file corresponds to repodata: i.e `repodata.json` or `repodata.json.zst`,
            // the tag is `latest`, and there is no need for splitting parts
            if (util::ends_with(path, ".json") || util::ends_with(path, ".json.zst"))
            {
                return { path, "latest" };
            }

            std::pair<std::string, std::string> result;
            auto parts = util::rsplit(path, "-", 2);

            if (parts.size() < 2)
            {
                LOG_ERROR << "Could not split " << path << " into enough parts";
                throw std::runtime_error("Could not split filename into enough parts");
            }

            result.first = parts[0];

            std::string tag;
            if (parts.size() > 2)
            {
                std::string last_part = parts[2].substr(0, parts[2].find_first_of("."));
                tag = fmt::format("{}-{}", parts[1], last_part);
            }
            else
            {
                tag = parts[1];
            }

            util::replace_all(tag, "_", "-");
            result.second = tag;

            LOG_INFO << "Splitting " << path << " to name: " << result.first
                     << " tag: " << result.second;
            return result;
        }

        nl::json parse_json_nothrow(const std::string& value)
        {
            try
            {
                auto j = nl::json::parse(value);
                return j;
            }
            catch (const nlohmann::detail::parse_error& e)
            {
                spdlog::error("Could not parse JSON\n{}", value);
                spdlog::error("Error message: {}", e.what());
                return nl::json::object();
            }
        }
    }

    OCIMirror::OCIMirror(
        std::string url,
        std::string repo_prefix,
        std::string scope,
        std::string username,
        std::string password
    )
        : Mirror(OCIMirror::make_id(url))
        , m_url(std::move(url))
        , m_repo_prefix(std::move(repo_prefix))
        , m_scope(std::move(scope))
        , m_username(std::move(username))
        , m_password(std::move(password))
        , m_path_map()
    {
    }

    MirrorID OCIMirror::make_id(std::string url)
    {
        return MirrorID(std::move(url));
    }

    auto OCIMirror::get_request_generators_impl(
        const std::string& url_path,
        const std::string& spec_sha256
    ) const -> request_generator_list
    {
        // NB: This method can be executed by many threads in parallel. Therefore,
        // data should not be captured in lambda used for building the request, as
        // inserting a new AuthenticationData object may relocate preexisting ones.
        auto [split_path, split_tag] = split_path_tag(url_path);

        // TODO we are getting here a new token for every artifact/path
        // => we should handle this differently to use the same token
        // => we could assume all requests are necessarily finished in < ~30 min? (max of token
        // validity) and store auth data by subdir instead?
        // but data also contains sha256 which is specific to the artifact
        // (however, the token that we get seems to be the same even if asked multiple times...
        // so maybe that's okay)
        auto* data = get_authentication_data(split_path);
        if (!data)
        {
            m_path_map[split_path].reset(new AuthenticationData);
            data = m_path_map[split_path].get();
        }

        request_generator_list req_gen;

        if (data->token.empty())
        {
            req_gen.push_back([this, split_path](const Request& dl_request, const Content*)
                              { return build_authentication_request(dl_request, split_path); });
        }

        if (data->sha256sum.empty())
        {
            // If we know the spec sha256 (retrieved from repodata.json), we don't ask for the
            // manifest to get the spec
            if (!spec_sha256.empty())
            {
                // Update data with the corresponding spec sha256
                data->sha256sum = spec_sha256;
            }
            else
            {
                // This is the case of requesting repodata.json, we need to get the manifest first
                req_gen.push_back(
                    [this, split_path, split_tag](const Request& dl_request, const Content*)
                    { return build_manifest_request(dl_request, split_path, split_tag); }
                );
            }
        }

        // Request to get the actual artifact
        req_gen.push_back([this, split_path](const Request& dl_request, const Content*)
                          { return build_blob_request(dl_request, split_path); });

        return req_gen;
    }

    MirrorRequest OCIMirror::build_authentication_request(
        const Request& initial_request,
        const std::string& split_path
    ) const
    {
        AuthenticationData* data = get_authentication_data(split_path);
        std::string auth_url = get_authentication_url(split_path);
        MirrorRequest req(initial_request.name, auth_url);

        req.username = m_username;
        req.password = m_password;

        req.on_success = [data](const Success& success) -> expected_t<void>
        {
            const Buffer& buf = std::get<Buffer>(success.content);
            auto j = parse_json_nothrow(buf.value);
            if (j.contains("token"))
            {
                data->token = j["token"].get<std::string>();
                return expected_t<void>();
            }
            else
            {
                return make_unexpected(
                    "Could not retrieve authentication token",
                    mamba_error_code::download_content
                );
            }
        };
        return req;
    }

    MirrorRequest OCIMirror::build_manifest_request(
        const Request& initial_request,
        const std::string& split_path,
        const std::string& split_tag
    ) const
    {
        AuthenticationData* data = get_authentication_data(split_path);
        std::string manifest_url = get_manifest_url(split_path, split_tag);
        std::vector<std::string> headers = { get_authentication_header(data->token),
                                             "Accept: application/vnd.oci.image.manifest.v1+json" };

        MirrorRequest req(initial_request.name, manifest_url, std::move(headers));

        req.on_success = [data](const Success& success) -> expected_t<void>
        {
            const Buffer& buf = std::get<Buffer>(success.content);
            auto j = parse_json_nothrow(buf.value);
            if (j.contains("layers"))
            {
                std::string digest = j["layers"][0]["digest"];
                assert(util::starts_with(digest, "sha256:"));
                data->sha256sum = digest.substr(sizeof("sha256:") - 1);
                return expected_t<void>();
            }
            else
            {
                return make_unexpected("Could not retrieve sha256", mamba_error_code::download_content);
            }
        };
        return req;
    }

    MirrorRequest
    OCIMirror::build_blob_request(const Request& initial_request, const std::string& split_path) const
    {
        const AuthenticationData* data = get_authentication_data(split_path);
        std::string url = get_blob_url(split_path, data->sha256sum);
        std::vector<std::string> headers = { get_authentication_header(data->token) };

        return MirrorRequest(initial_request, url, std::move(headers));
    }

    // This is not used but could be if we use creds
    // cf. OCIMirror constructor comment in header
    bool OCIMirror::need_authentication() const
    {
        return !m_username.empty() && !m_password.empty();
    }

    std::string OCIMirror::get_repo(const std::string& repo) const
    {
        if (!m_repo_prefix.empty())
        {
            return fmt::format("{}/{}", m_repo_prefix, repo);
        }
        else
        {
            return repo;
        }
    }

    std::string OCIMirror::get_authentication_url(const std::string& repo) const
    {
        return fmt::format("{}/token?scope=repository:{}:{}", m_url, get_repo(repo), m_scope);
    }

    std::string OCIMirror::get_authentication_header(const std::string& token) const
    {
        if (token.empty())
        {
            LOG_ERROR << "Trying to pull artifacts with an empty token";
            throw std::invalid_argument("Trying to pull artifacts with an empty token");
        }
        else
        {
            return fmt::format("Authorization: Bearer {}", token);
        }
    }

    std::string OCIMirror::get_manifest_url(const std::string& repo, const std::string& reference) const
    {
        return fmt::format("{}/v2/{}/manifests/{}", m_url, get_repo(repo), reference);
    }

    std::string OCIMirror::get_blob_url(const std::string& repo, const std::string& sha256sum) const
    {
        // Should be this format:
        // https://ghcr.io/v2/wolfv/artifact/blobs/sha256:c5be3ea75353851e1fcf3a298af3b6cfd2af3d7ff018ce52657b6dbd8f986aa4
        return fmt::format("{}/v2/{}/blobs/sha256:{}", m_url, get_repo(repo), sha256sum);
    }

    auto
    OCIMirror::get_authentication_data(const std::string& split_path) const -> AuthenticationData*
    {
        auto it = m_path_map.find(split_path);
        if (it != m_path_map.end())
        {
            return it->second.get();
        }
        return nullptr;
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
        else if ((util::starts_with(url, "https://") || util::starts_with(url, "http://")
                  || util::starts_with(url, "file://"))
                 && !util::contains(url, "ghcr"))
        {
            return std::make_unique<HTTPMirror>(std::move(url));
        }
        else if (util::contains(url, "ghcr"))  // TODO should start with oci? (to be handled)
        {
            const auto parsed_url = util::URL::parse(url).value();
            return std::make_unique<OCIMirror>(
                util::concat(parsed_url.scheme(), "://", parsed_url.host()),
                std::string(util::lstrip(parsed_url.path(), "/")),
                "pull"
            );
        }
        return nullptr;
    }
}
