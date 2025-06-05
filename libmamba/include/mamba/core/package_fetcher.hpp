// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_PACKAGE_FETCHER_HPP
#define MAMBA_CORE_PACKAGE_FETCHER_HPP

#include <functional>

#include "mamba/core/package_cache.hpp"
#include "mamba/core/package_handling.hpp"
#include "mamba/core/thread_utils.hpp"
#include "mamba/download/downloader.hpp"
#include "mamba/specs/package_info.hpp"

namespace mamba
{

    class PackageFetcher;

    enum class PackageExtractEvent
    {
        validate_update,
        validate_success,
        validate_failure,
        extract_update,
        extract_success,
        extract_failure
    };

    class PackageExtractTask
    {
    public:

        struct Result
        {
            bool valid;
            bool extracted;
        };

        using progress_callback_t = std::function<void(PackageExtractEvent)>;

        PackageExtractTask(PackageFetcher* fetcher, ExtractOptions options);

        const std::string& name() const;
        bool needs_download() const;

        void set_progress_callback(progress_callback_t cb);

        Result run();
        Result run(std::size_t downloaded_size);

    private:

        progress_callback_t* get_progress_callback();

        PackageFetcher* p_fetcher;
        ExtractOptions m_options;
        std::optional<progress_callback_t> m_progress_callback = std::nullopt;
    };

    class PackageFetcher
    {
    public:

        enum class ValidationResult
        {
            UNDEFINED = 0,
            VALID = 1,
            SHA256_ERROR,
            MD5SUM_ERROR,
            SIZE_ERROR,
            EXTRACT_ERROR
        };

        using post_download_success_t = std::function<void(std::size_t)>;
        using progress_callback_t = std::function<void(PackageExtractEvent)>;

        PackageFetcher(const specs::PackageInfo& pkg_info, MultiPackageCache& caches);

        const std::string& name() const;

        bool needs_download() const;
        bool needs_extract() const;

        download::Request
        build_download_request(std::optional<post_download_success_t> callback = std::nullopt);
        ValidationResult
        validate(std::size_t downloaded_size, progress_callback_t* cb = nullptr) const;
        bool extract(const ExtractOptions& options, progress_callback_t* cb = nullptr);

        // The PackageFetcher object should be stable in memory (i.e. not moved) after this
        // method has been called, until the PackageExtractTask has been completed.
        PackageExtractTask build_extract_task(ExtractOptions options);

        void clear_cache() const;

    private:

        struct CheckSumParams;

        bool use_oci() const;
        bool use_auth() const;
        const std::string& filename() const;
        std::string channel() const;
        std::string url_path() const;
        const std::string& url() const;
        const std::string& sha256() const;
        const std::string& md5() const;
        std::size_t expected_size() const;

        ValidationResult validate_size(std::size_t downloaded_size) const;
        ValidationResult validate_checksum(const CheckSumParams& params) const;

        void write_repodata_record(const fs::u8path& base_path) const;
        void update_urls_txt() const;

        void update_monitor(progress_callback_t* cb, PackageExtractEvent event) const;

        specs::PackageInfo m_package_info;

        fs::u8path m_tarball_path;
        fs::u8path m_cache_path;

        bool m_needs_download = false;
        std::string m_downloaded_url = {};
        bool m_needs_extract = false;
    };

    class PackageFetcherSemaphore
    {
    public:

        static std::ptrdiff_t get_max();
        static void set_max(int value);

    private:

        static counting_semaphore semaphore;

        friend class PackageFetcher;
    };
}

#endif
