// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <iostream>
#include <stack>

#include <fmt/color.h>
#include <fmt/format.h>
#include <fmt/ostream.h>

#include "mamba/core/channel.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/execution.hpp"
#include "mamba/core/fetch.hpp"
#include "mamba/core/mamba_fs.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/package_cache.hpp"
#include "mamba/core/package_download.hpp"
#include "mamba/core/package_handling.hpp"
#include "mamba/core/progress_bar.hpp"
#include "mamba/core/thread_utils.hpp"
#include "mamba/util/string.hpp"

#include "progress_bar_impl.hpp"

namespace mamba
{
    /********************************
     * PackageDownloadExtractTarget *
     ********************************/

    counting_semaphore DownloadExtractSemaphore::semaphore(0);

    std::ptrdiff_t DownloadExtractSemaphore::get_max()
    {
        return DownloadExtractSemaphore::semaphore.get_max();
    }

    void DownloadExtractSemaphore::set_max(int value)
    {
        DownloadExtractSemaphore::semaphore.set_max(value);
    }

    PackageDownloadExtractTarget::PackageDownloadExtractTarget(
        const PackageInfo& pkg_info,
        ChannelContext& channel_context
    )
        : m_finished(false)
        , m_package_info(pkg_info)
    {
        m_filename = pkg_info.fn;

        // only do this for micromamba for now
        if (Context::instance().command_params.is_micromamba)
        {
            m_url = channel_context.make_channel(pkg_info.url).urls(true)[0];
        }
        else
        {
            m_url = pkg_info.url;
        }

        m_name = pkg_info.name;

        m_expected_size = pkg_info.size;
        m_sha256 = pkg_info.sha256;
        m_md5 = pkg_info.md5;

        auto& ctx = Context::instance();
        m_has_progress_bars = !(
            ctx.graphics_params.no_progress_bars || ctx.output_params.quiet || ctx.output_params.json
        );
    }

    void PackageDownloadExtractTarget::write_repodata_record(const fs::u8path& base_path)
    {
        fs::u8path repodata_record_path = base_path / "info" / "repodata_record.json";
        fs::u8path index_path = base_path / "info" / "index.json";

        nlohmann::json index, solvable_json;
        std::ifstream index_file = open_ifstream(index_path);
        index_file >> index;

        solvable_json = m_package_info.json_record();
        index.insert(solvable_json.cbegin(), solvable_json.cend());

        if (index.find("size") == index.end() || index["size"] == 0)
        {
            index["size"] = fs::file_size(m_tarball_path);
        }

        std::ofstream repodata_record(repodata_record_path.std_path());
        repodata_record << index.dump(4);
    }

    static std::mutex urls_txt_mutex;
    void PackageDownloadExtractTarget::add_url()
    {
        std::lock_guard<std::mutex> lock(urls_txt_mutex);
        const auto urls_file_path = m_cache_path / "urls.txt";
        std::ofstream urls_txt(urls_file_path.std_path(), std::ios::app);
        urls_txt << m_url << std::endl;
    }

    void PackageDownloadExtractTarget::validate()
    {
        m_validation_result = VALIDATION_RESULT::VALID;
        if (m_expected_size && (m_target->get_downloaded_size() != m_expected_size))
        {
            LOG_ERROR << "File not valid: file size doesn't match expectation " << m_tarball_path
                      << "\nExpected: " << m_expected_size
                      << "\nActual: " << m_target->get_downloaded_size() << "\n";
            if (m_has_progress_bars)
            {
                m_download_bar.set_postfix("validation failed");
                m_download_bar.mark_as_completed();
            }
            Console::instance().print(m_filename + " tarball has incorrect size");
            m_validation_result = SIZE_ERROR;
            return;
        }
        interruption_point();

        if (!m_sha256.empty())
        {
            auto sha256sum = validation::sha256sum(m_tarball_path);
            if (m_sha256 != sha256sum)
            {
                m_validation_result = SHA256_ERROR;
                if (m_has_progress_bars)
                {
                    m_download_bar.set_postfix("validation failed");
                    m_download_bar.mark_as_completed();
                }
                Console::instance().print(m_filename + " tarball has incorrect checksum");
                LOG_ERROR << "File not valid: SHA256 sum doesn't match expectation " << m_tarball_path
                          << "\nExpected: " << m_sha256 << "\nActual: " << sha256sum << "\n";
            }
            return;
        }
        if (!m_md5.empty())
        {
            auto md5sum = validation::md5sum(m_tarball_path);
            if (m_md5 != md5sum)
            {
                m_validation_result = MD5SUM_ERROR;
                if (m_has_progress_bars)
                {
                    m_download_bar.set_postfix("validation failed");
                    m_download_bar.mark_as_completed();
                }
                Console::instance().print(m_filename + " tarball has incorrect checksum");
                LOG_ERROR << "File not valid: MD5 sum doesn't match expectation " << m_tarball_path
                          << "\nExpected: " << m_md5 << "\nActual: " << md5sum << "\n";
            }
        }
    }

    std::function<void(ProgressBarRepr&)> PackageDownloadExtractTarget::extract_repr()
    {
        return [&](ProgressBarRepr& r) -> void
        {
            if (r.progress_bar().started())
            {
                r.postfix.set_value("Extracting");
            }
            else
            {
                r.postfix.set_value("Extracted");
            }
        };
    }

    std::function<void(ProgressProxy&)> PackageDownloadExtractTarget::extract_progress_callback()
    {
        return [&](ProgressProxy& bar) -> void
        {
            if (bar.started())
            {
                bar.set_progress(0, 1);
            }
        };
    }

    bool PackageDownloadExtractTarget::extract()
    {
        // Extracting is __not__ yet thread safe it seems...
        interruption_point();

        if (m_has_progress_bars)
        {
            m_extract_bar.start();
        }

        LOG_DEBUG << "Waiting for decompression " << m_tarball_path;
        if (m_has_progress_bars)
        {
            m_extract_bar.update_progress(0, 1);
        }
        {
            std::lock_guard<counting_semaphore> lock(DownloadExtractSemaphore::semaphore);
            interruption_point();
            LOG_DEBUG << "Decompressing '" << m_tarball_path.string() << "'";
            fs::u8path extract_path;
            try
            {
                std::string fn = m_filename;
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
                    LOG_ERROR << "Unknown package format '" << m_filename << "'";
                    throw std::runtime_error("Unknown package format.");
                }
                // Be sure the first writable cache doesn't contain invalid extracted package
                extract_path = m_cache_path / fn;
                if (fs::exists(extract_path))
                {
                    LOG_DEBUG << "Removing '" << extract_path.string()
                              << "' before extracting it again";
                    fs::remove_all(extract_path);
                }

                // Use non-subproc version if concurrency is disabled to avoid
                // any potential subprocess issues
                if (DownloadExtractSemaphore::get_max() == 1)
                {
                    mamba::extract(m_tarball_path, extract_path);
                }
                else
                {
                    mamba::extract_subproc(m_tarball_path, extract_path);
                }
                // mamba::extract(m_tarball_path, extract_path);
                interruption_point();
                LOG_DEBUG << "Extracted to '" << extract_path.string() << "'";
                write_repodata_record(extract_path);
                add_url();

                if (m_has_progress_bars)
                {
                    m_extract_bar.set_full();
                    m_extract_bar.mark_as_completed();
                }
            }
            catch (std::exception& e)
            {
                Console::instance().print(m_filename + " extraction failed");
                LOG_ERROR << "Error when extracting package: " << e.what();
                m_decompress_exception = e;
                m_validation_result = VALIDATION_RESULT::EXTRACT_ERROR;
                if (m_has_progress_bars)
                {
                    m_extract_bar.set_postfix("extraction failed");
                    m_extract_bar.mark_as_completed();
                }
                return false;
            }
        }
        return true;
    }

    bool PackageDownloadExtractTarget::extract_from_cache()
    {
        this->extract();
        m_finished = true;
        return true;
    }

    bool PackageDownloadExtractTarget::validate_extract()
    {
        using std::chrono::nanoseconds;

        if (m_has_progress_bars)
        {
            m_extract_bar.start();
            m_extract_bar.set_postfix("validating");
        }
        validate();

        // Validation
        if (m_validation_result != VALIDATION_RESULT::VALID)
        {
            if (m_has_progress_bars)
            {
                m_extract_bar.set_postfix("validation failed");
            }
            LOG_WARNING << "'" << m_tarball_path.string() << "' validation failed";
            // abort here, but set finished to true
            m_finished = true;
            return true;
        }

        if (m_has_progress_bars)
        {
            m_extract_bar.set_postfix("validated");
        }
        LOG_DEBUG << "'" << m_tarball_path.string() << "' successfully validated";

        bool result = this->extract();
        m_finished = true;
        return result;
    }

    bool PackageDownloadExtractTarget::finalize_callback(const DownloadTarget&)
    {
        if (m_has_progress_bars)
        {
            m_download_bar.repr().postfix.set_value("Downloaded").deactivate();
            m_download_bar.mark_as_completed();
        }

        if (m_target->get_http_status() >= 400)
        {
            LOG_ERROR << "Failed to download package from " << m_url << " (status "
                      << m_target->get_http_status() << ")";
            m_validation_result = VALIDATION_RESULT::UNDEFINED;
            return false;
        }

        LOG_INFO << "Download finished, validating '" << m_tarball_path.string() << "'";
        MainExecutor::instance().schedule(&PackageDownloadExtractTarget::validate_extract, this);

        return true;
    }

    bool PackageDownloadExtractTarget::finished()
    {
        return m_finished;
    }

    auto PackageDownloadExtractTarget::validation_result() const -> VALIDATION_RESULT
    {
        return m_validation_result;
    }

    void PackageDownloadExtractTarget::clear_cache() const
    {
        fs::remove_all(m_tarball_path);
        fs::u8path dest_dir = strip_package_extension(m_tarball_path.string());
        if (fs::exists(dest_dir))
        {
            fs::remove_all(dest_dir);
        }
    }

    const std::string& PackageDownloadExtractTarget::name() const
    {
        return m_name;
    }

    std::size_t PackageDownloadExtractTarget::expected_size() const
    {
        return m_expected_size;
    }

    // todo remove cache from this interface
    DownloadTarget* PackageDownloadExtractTarget::target(MultiPackageCache& caches)
    {
        // tarball can be removed, it's fine if only the correct dest dir exists
        // 1. If there is extracted cache, use it, otherwise next.
        // 2. If there is valid tarball, extract it, otherwise next.
        // 3. Run the full download pipeline.

        fs::u8path extracted_cache = caches.get_extracted_dir_path(m_package_info);

        if (extracted_cache.empty())
        {
            fs::u8path tarball_cache = caches.get_tarball_path(m_package_info);
            // Compute the first writable cache and clean its status for the current package
            caches.first_writable_cache(true).clear_query_cache(m_package_info);
            m_cache_path = caches.first_writable_path();

            if (m_has_progress_bars)
            {
                m_extract_bar = Console::instance().add_progress_bar(m_name, 1);
                m_extract_bar.activate_spinner();
                m_extract_bar.set_progress_hook(extract_progress_callback());
                m_extract_bar.set_repr_hook(extract_repr());
                Console::instance().progress_bar_manager().add_label("Extract", m_extract_bar);
            }

            if (!tarball_cache.empty())
            {
                LOG_DEBUG << "Found valid tarball cache at '" << tarball_cache.string() << "'";

                m_tarball_path = tarball_cache / m_filename;
                m_validation_result = VALIDATION_RESULT::VALID;
                MainExecutor::instance().schedule(
                    &PackageDownloadExtractTarget::extract_from_cache,
                    this
                );
                LOG_DEBUG << "Using cached tarball '" << m_filename << "'";
                return nullptr;
            }
            else
            {
                caches.clear_query_cache(m_package_info);
                // need to download this file
                LOG_DEBUG << "Adding '" << m_name << "' to download targets from '" << m_url << "'";

                m_tarball_path = m_cache_path / m_filename;
                m_target = std::make_unique<DownloadTarget>(m_name, m_url, m_tarball_path.string());
                m_target->set_finalize_callback(&PackageDownloadExtractTarget::finalize_callback, this);
                m_target->set_expected_size(m_expected_size);
                if (m_has_progress_bars)
                {
                    m_download_bar = Console::instance().add_progress_bar(m_name, m_expected_size);
                    m_target->set_progress_bar(m_download_bar);
                    Console::instance().progress_bar_manager().add_label("Download", m_download_bar);
                }
                return m_target.get();
            }
        }
        LOG_DEBUG << "Using cached '" << m_name << "'";
        m_finished = true;
        return nullptr;
    }
}
