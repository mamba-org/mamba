// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.


#ifndef MAMBA_CORE_PACKAGE_DOWNLOAD_HPP
#define MAMBA_CORE_PACKAGE_DOWNLOAD_HPP

#include <future>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include "mamba/filesystem/u8path.hpp"

#include "fetch.hpp"
#include "package_cache.hpp"
#include "progress_bar.hpp"
#include "tasksync.hpp"
#include "thread_utils.hpp"

namespace mamba
{
    class ChannelContext;

    class PackageDownloadExtractTarget
    {
    public:

        enum VALIDATION_RESULT
        {
            UNDEFINED = 0,
            VALID = 1,
            SHA256_ERROR,
            MD5SUM_ERROR,
            SIZE_ERROR,
            EXTRACT_ERROR
        };

        // TODO: REVIEW: consider caputring a reference to the context from the ChannelContext, if
        // that makes sense.
        PackageDownloadExtractTarget(const PackageInfo& pkg_info, ChannelContext& channel_context);

        void write_repodata_record(const fs::u8path& base_path);
        void add_url();
        bool finalize_callback(const DownloadTarget& target);
        bool finished();
        void validate();
        bool extract(const Context& context);
        bool extract_from_cache(const Context& context);
        bool validate_extract(const Context& context);
        const std::string& name() const;
        std::size_t expected_size() const;
        VALIDATION_RESULT validation_result() const;
        void clear_cache() const;

        DownloadTarget* target(Context& context, MultiPackageCache& cache);

        std::exception m_decompress_exception;

    private:

        bool m_finished;
        PackageInfo m_package_info;

        std::string m_sha256, m_md5;
        std::size_t m_expected_size;

        bool m_has_progress_bars = false;
        ProgressProxy m_download_bar, m_extract_bar;
        std::unique_ptr<DownloadTarget> m_target;

        std::string m_url, m_name, m_filename;
        fs::u8path m_tarball_path, m_cache_path;

        std::future<bool> m_extract_future;

        VALIDATION_RESULT m_validation_result = VALIDATION_RESULT::UNDEFINED;

        TaskSynchronizer m_tasksync;

        std::function<void(ProgressBarRepr&)> extract_repr();
        std::function<void(ProgressProxy&)> extract_progress_callback();
    };

    class DownloadExtractSemaphore
    {
    public:

        static std::ptrdiff_t get_max();
        static void set_max(int value);

    private:

        static counting_semaphore semaphore;

        friend class PackageDownloadExtractTarget;
    };
}  // namespace mamba

#endif
