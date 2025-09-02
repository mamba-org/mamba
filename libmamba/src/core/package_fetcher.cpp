// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/core/invoke.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/package_fetcher.hpp"
#include "mamba/core/util.hpp"
#include "mamba/specs/archive.hpp"
#include "mamba/util/string.hpp"
#include "mamba/validation/tools.hpp"

namespace mamba
{

    /**********************
     * PackageExtractTask *
     **********************/

    PackageExtractTask::PackageExtractTask(PackageFetcher* fetcher, ExtractOptions options)
        : p_fetcher(fetcher)
        , m_options(std::move(options))
    {
    }

    const std::string& PackageExtractTask::name() const
    {
        return p_fetcher->name();
    }

    bool PackageExtractTask::needs_download() const
    {
        return p_fetcher->needs_download();
    }

    void PackageExtractTask::set_progress_callback(progress_callback_t cb)
    {
        m_progress_callback = std::move(cb);
    }

    auto PackageExtractTask::run() -> Result
    {
        bool is_valid = true;
        bool is_extracted = p_fetcher->extract(m_options);
        return { is_valid, is_extracted };
    }

    auto PackageExtractTask::run(std::size_t downloaded_size) -> Result
    {
        using ValidationResult = PackageFetcher::ValidationResult;
        ValidationResult validation_res = p_fetcher->validate(downloaded_size, get_progress_callback());
        const bool is_valid = validation_res == ValidationResult::VALID;
        bool is_extracted = false;
        if (is_valid)
        {
            is_extracted = p_fetcher->extract(m_options, get_progress_callback());
        }
        return { is_valid, is_extracted };
    }

    auto PackageExtractTask::get_progress_callback() -> progress_callback_t*
    {
        if (m_progress_callback.has_value())
        {
            return &m_progress_callback.value();
        }
        else
        {
            return nullptr;
        }
    }

    /*******************
     * PatckageFetcher *
     *******************/

    struct PackageFetcher::CheckSumParams
    {
        std::string_view expected;
        std::string_view actual;
        std::string_view name;
        ValidationResult error;
    };

    PackageFetcher::PackageFetcher(const specs::PackageInfo& pkg_info, MultiPackageCache& caches)

        : m_package_info(pkg_info)
    {
        const fs::u8path extracted_cache = caches.get_extracted_dir_path(m_package_info);
        if (extracted_cache.empty())
        {
            const fs::u8path tarball_cache = caches.get_tarball_path(m_package_info);
            auto& cache = caches.first_writable_cache(true);
            m_cache_path = cache.path();

            if (!tarball_cache.empty())
            {
                LOG_DEBUG << "Found valid tarball cache at '" << tarball_cache.string() << "'";
                cache.clear_query_cache(m_package_info);
                m_tarball_path = tarball_cache / filename();
                m_needs_extract = true;
                LOG_DEBUG << "Using cached tarball '" << filename() << "'";
            }
            else
            {
                caches.clear_query_cache(m_package_info);
                // need to download this file
                LOG_DEBUG << "Adding '" << name() << "' to download targets from '"
                          << hide_secrets(channel()) << "/" << url_path() << "'";
                m_tarball_path = m_cache_path / filename();
                m_needs_extract = true;
                m_needs_download = true;
            }
        }
        else
        {
            LOG_DEBUG << "Using cached '" << name() << "'";
        }
    }

    const std::string& PackageFetcher::name() const
    {
        return m_package_info.name;
    }

    bool PackageFetcher::needs_download() const
    {
        return m_needs_download;
    }

    bool PackageFetcher::needs_extract() const
    {
        return m_needs_extract;
    }

    download::Request
    PackageFetcher::build_download_request(std::optional<post_download_success_t> callback)
    {
        // download::Request request(name(), download::MirrorName(""), url(),
        // m_tarball_path.string());
        download::Request
            request(name(), download::MirrorName(channel()), url_path(), m_tarball_path.string());
        request.expected_size = expected_size();
        request.sha256 = sha256();

        request.on_success = [this, cb = std::move(callback)](const download::Success& success)
        {
            LOG_INFO << "Download finished, tarball available at '" << m_tarball_path.string() << "'";
            if (cb.has_value())
            {
                cb.value()(success.transfer.downloaded_size);
            }
            m_needs_download = false;
            m_downloaded_url = m_package_info.package_url;
            return expected_t<void>();
        };

        request.on_failure = [](const download::Error& error)
        {
            if (error.transfer.has_value())
            {
                LOG_ERROR << "Failed to download package from "
                          << error.transfer.value().effective_url << " (status "
                          << error.transfer.value().http_status << ")\n"
                          << "If you see this message repeatedly, the state of your installation might be corrupted,\n"
                          << "in which case running `mamba clean --all` might fix it.\n\n"
                          << "If you continue to meet this problem, please search or report an issue\n"
                          << "on  mamba's issue tracker: https://github.com/mamba-org/mamba/issues/";
            }
            else
            {
                LOG_WARNING << error.message;
            }

            if (error.retry_wait_seconds.has_value())
            {
                LOG_WARNING << "Retrying in " << error.retry_wait_seconds.value() << " seconds";
            }
        };

        return request;
    }

    auto PackageFetcher::validate(std::size_t downloaded_size, progress_callback_t* cb) const
        -> ValidationResult
    {
        update_monitor(cb, PackageExtractEvent::validate_update);
        ValidationResult res = validate_size(downloaded_size);
        if (res != ValidationResult::VALID)
        {
            update_monitor(cb, PackageExtractEvent::validate_failure);
            return res;
        }

        interruption_point();

        if (!sha256().empty())
        {
            res = validate_checksum({
                /* .expected= */ sha256(),
                /* .actual= */ validation::sha256sum(m_tarball_path),
                /* .name= */ "SHA256",
                /* .error= */ ValidationResult::SHA256_ERROR,
            });
        }
        else if (!md5().empty())
        {
            res = validate_checksum({
                /* .expected= */ md5(),
                /* .actual= */ validation::md5sum(m_tarball_path),
                /* .name= */ "MD5",
                /* .error= */ ValidationResult::MD5SUM_ERROR,
            });
        }

        auto event = res == ValidationResult::VALID ? PackageExtractEvent::validate_success
                                                    : PackageExtractEvent::validate_failure;
        update_monitor(cb, event);
        return res;
    }

    namespace
    {
        fs::u8path get_extract_path(const std::string& filename, const fs::u8path& cache_path)
        {
            std::string fn = filename;
            if (util::ends_with(fn, ".tar.bz2"))
            {
                fn = fn.substr(0, fn.size() - 8);
            }
            else if (util::ends_with(fn, ".conda"))
            {
                fn = fn.substr(0, fn.size() - 6);
            }
            else
            {
                LOG_ERROR << "Unknown package format '" << filename << "'";
                throw std::runtime_error("Unknown package format.");
            }
            return cache_path / fn;
        }

        void clear_extract_path(const fs::u8path& path)
        {
            if (fs::exists(path))
            {
                LOG_DEBUG << "Removing '" << path.string() << "' before extracting it again";
                fs::remove_all(path);
            }
        }

        void extract_impl(
            const fs::u8path& tarball_path,
            const fs::u8path& extract_path,
            const ExtractOptions& options
        )
        {
            // Use non-subproc version if concurrency is disabled to avoid
            // any potential subprocess issues
            if (PackageFetcherSemaphore::get_max() == 1)
            {
                mamba::extract(tarball_path, extract_path, options);
            }
            else
            {
                mamba::extract_subproc(tarball_path, extract_path, options);
            }
        }
    }

    bool PackageFetcher::extract(const ExtractOptions& options, progress_callback_t* cb)
    {
        // Extracting is __not__ yet thread safe it seems...
        interruption_point();

        LOG_DEBUG << "Waiting for decompression " << m_tarball_path;
        update_monitor(cb, PackageExtractEvent::extract_update);

        {
            std::lock_guard<counting_semaphore> lock(PackageFetcherSemaphore::semaphore);
            interruption_point();
            LOG_DEBUG << "Decompressing '" << m_tarball_path.string() << "'";
            try
            {
                const fs::u8path extract_path = get_extract_path(filename(), m_cache_path);
                // Be sure the first writable cache doesn't contain invalid extracted package
                clear_extract_path(extract_path);
                extract_impl(m_tarball_path, extract_path, options);

                interruption_point();
                LOG_DEBUG << "Extracted to '" << extract_path.string() << "'";
                write_repodata_record(extract_path);
                update_urls_txt();
                update_monitor(cb, PackageExtractEvent::extract_success);
            }
            catch (std::exception& e)
            {
                Console::instance().print(filename() + " extraction failed");
                LOG_ERROR << "Error when extracting package: " << e.what();
                update_monitor(cb, PackageExtractEvent::extract_failure);
                return false;
            }
        }
        m_needs_extract = false;
        return true;
    }

    PackageExtractTask PackageFetcher::build_extract_task(ExtractOptions options)
    {
        return { this, std::move(options) };
    }

    void PackageFetcher::clear_cache() const
    {
        fs::remove_all(m_tarball_path);
        const fs::u8path dest_dir = specs::strip_archive_extension(m_tarball_path.string());
        fs::remove_all(dest_dir);
    }

    /*******************
     * Private methods *
     *******************/

    const std::string& PackageFetcher::filename() const
    {
        return m_package_info.filename;
    }

    bool PackageFetcher::use_oci() const
    {
        constexpr std::string_view oci_scheme = "oci://";
        return util::starts_with(m_package_info.package_url, oci_scheme);
    }

    bool PackageFetcher::use_auth() const
    {
        return std::regex_search(m_package_info.package_url, http_basicauth_regex())
               || std::regex_search(m_package_info.package_url, token_regex());
    }

    // NOTE
    // - In the general case (not fetching from an oci registry),
    // `channel()` and `url_path()` are concatenated when passed to `HTTPMirror`
    // and the channel is resolved if needed (using the channel alias).
    // Therefore, `util::url_concat("", m_package_info.package_url)`
    // and `util::url_concat(m_package_info.channel, m_package_info.platform,
    // m_package_info.filename)` should be equivalent, except when an explicit url is used as a spec
    // with `--override-channels` option.
    // Hence, we are favoring the first option (returning "" and `m_package_info.package_url`
    // to be concatenated), valid for all the mentioned cases used with `HTTPMirror`.
    // - In the case of fetching from oci registries (using `OCIMirror`),the actual url
    // used is built differently, and returning `m_package_info.package_url` is not relevant
    // (i.e oci://ghcr.io/<mirror>/<channel>/<platform>/<filename>).
    // - With authenticated downloading (private packages for e.g), we should use
    // `util::url_concat(m_package_info.channel, m_package_info.platform,
    // m_package_info.filename)` as `m_package_info.package_url` would contain the
    // authentication info which shouldn't be passed in the url but set using libcurl's CURLUPart.
    std::string PackageFetcher::channel() const
    {
        if (use_oci() || use_auth())
        {
            return m_package_info.channel;
        }
        return "";
    }

    std::string PackageFetcher::url_path() const
    {
        if (use_oci() || use_auth())
        {
            return util::concat(m_package_info.platform, '/', m_package_info.filename);
        }
        return m_package_info.package_url;
    }

    const std::string& PackageFetcher::url() const
    {
        return m_downloaded_url;
    }

    const std::string& PackageFetcher::sha256() const
    {
        return m_package_info.sha256;
    }

    const std::string& PackageFetcher::md5() const
    {
        return m_package_info.md5;
    }

    std::size_t PackageFetcher::expected_size() const
    {
        return m_package_info.size;
    }

    auto PackageFetcher::validate_size(std::size_t downloaded_size) const -> ValidationResult
    {
        auto res = ValidationResult::VALID;
        if (expected_size() && expected_size() != downloaded_size)
        {
            res = ValidationResult::SIZE_ERROR;
            LOG_ERROR << "File not valid: file size doesn't match expectation " << m_tarball_path
                      << "\nExpected: " << expected_size() << "\nActual: " << downloaded_size
                      << "\n";
            Console::instance().print(filename() + " tarball has incorrect size");
        }
        return res;
    }

    auto PackageFetcher::validate_checksum(const CheckSumParams& params) const -> ValidationResult
    {
        auto res = ValidationResult::VALID;
        if (params.actual != params.expected)
        {
            res = params.error;
            LOG_ERROR << "File not valid: " << params.name << " doesn't match expectation "
                      << m_tarball_path << "\nExpected: " << params.expected
                      << "\nActual: " << params.actual << "\n";
            Console::instance().print(util::concat(filename(), " tarball has incorrect ", params.name)
            );
            // TODO: terminate monitor
        }
        return res;
    }

    void PackageFetcher::write_repodata_record(const fs::u8path& base_path) const
    {
        const fs::u8path repodata_record_path = base_path / "info" / "repodata_record.json";
        const fs::u8path index_path = base_path / "info" / "index.json";

        nlohmann::json index;
        std::ifstream index_file = open_ifstream(index_path);
        index_file >> index;

        nlohmann::json repodata_record = m_package_info;

        // To take correction of packages metadata (e.g. made using repodata patches) into account,
        // we insert the index into the repodata record to only add new fields from the index
        // while keeping the existing fields from the repodata record.
        repodata_record.insert(index.cbegin(), index.cend());

        if (repodata_record.find("size") == repodata_record.end() || repodata_record["size"] == 0)
        {
            repodata_record["size"] = fs::file_size(m_tarball_path);
        }

        std::ofstream repodata_record_file(repodata_record_path.std_path());
        repodata_record_file << repodata_record.dump(4);
    }

    namespace
    {
        std::mutex urls_txt_mutex;
    }

    void PackageFetcher::update_urls_txt() const
    {
        // TODO: check if this lock is really required
        std::unique_lock lock{ urls_txt_mutex };
        const auto urls_file_path = m_cache_path / "urls.txt";
        std::ofstream urls_txt(urls_file_path.std_path(), std::ios::app);
        urls_txt << url() << std::endl;
    }

    void PackageFetcher::update_monitor(progress_callback_t* cb, PackageExtractEvent event) const
    {
        if (cb)
        {
            // We dont want to propagate errors coming from user's callbacks
            [[maybe_unused]] auto result = safe_invoke(*cb, event);
        }
    }

    /***************************
     * PackageFetcherSemaphore *
     ***************************/

    counting_semaphore PackageFetcherSemaphore::semaphore(0);

    std::ptrdiff_t PackageFetcherSemaphore::get_max()
    {
        return PackageFetcherSemaphore::semaphore.get_max();
    }

    void PackageFetcherSemaphore::set_max(int value)
    {
        PackageFetcherSemaphore::semaphore.set_max(value);
    }
}
