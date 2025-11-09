// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>

#include "mamba/core/context.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/package_cache.hpp"
#include "mamba/core/package_handling.hpp"
#include "mamba/specs/archive.hpp"
#include "mamba/specs/conda_url.hpp"
#include "mamba/util/string.hpp"
#include "mamba/validation/tools.hpp"

namespace mamba
{
    PackageCacheData::PackageCacheData(const fs::u8path& path)
        : m_path(path)
    {
    }

    bool PackageCacheData::create_directory()
    {
        try
        {
            LOG_DEBUG << "Attempt to create package cache directory '" << m_path.string() << "'";
            bool sudo_safe = path::starts_with_home(m_path);
            path::touch(
                m_path / PACKAGE_CACHE_MAGIC_FILE,
                /*mkdir*/ true,
                sudo_safe
            );
            return true;
        }
        catch (...)
        {
            LOG_DEBUG << "Package cache directory is not writable '" << m_path.string() << "'";
            return false;
        }
    }

    void PackageCacheData::set_writable(Writable writable)
    {
        m_writable = writable;
    }

    auto PackageCacheData::is_writable() -> Writable
    {
        if (m_writable == Writable::UNKNOWN)
        {
            check_writable();
        }

        return m_writable;
    }

    fs::u8path PackageCacheData::path() const
    {
        return m_path;
    }

    void PackageCacheData::clear_query_cache(const specs::PackageInfo& s)
    {
        m_valid_tarballs.erase(s.str());
        m_valid_extracted_dir.erase(s.str());
    }

    void PackageCacheData::check_writable()
    {
        fs::u8path magic_file = m_path / PACKAGE_CACHE_MAGIC_FILE;
        LOG_DEBUG << "Checking if '" << m_path.string() << "' is writable";

        std::error_code ec;
        if (fs::exists(m_path, ec))
        {
            if (fs::is_regular_file(magic_file))
            {
                LOG_TRACE << "'" << magic_file.string() << "' exists, checking if writable";
                if (path::is_writable(magic_file))
                {
                    m_writable = Writable::WRITABLE;
                    LOG_DEBUG << "'" << m_path.string() << "' writable";
                    return;
                }
                else
                {
                    m_writable = Writable::NOT_WRITABLE;
                    LOG_DEBUG << "'" << m_path.string() << "' not writable";
                    return;
                }
            }
        }
        else
        {
            LOG_TRACE << "Cache path does not exists or is not writable";
        }

        try
        {
            path::touch(magic_file, true);
            m_writable = Writable::WRITABLE;
            LOG_DEBUG << "'" << m_path.string() << "' writable";
        }
        catch (const fs::filesystem_error&)
        {
            m_writable = Writable::NOT_WRITABLE;
            LOG_DEBUG << "'" << m_path.string() << "' not writable";
        }
    }

    bool
    PackageCacheData::has_valid_tarball(const specs::PackageInfo& s, const ValidationParams& params)
    {
        std::string pkg = s.str();
        if (m_valid_tarballs.find(pkg) != m_valid_tarballs.end())
        {
            return m_valid_tarballs[pkg];
        }

        assert(!s.filename.empty());
        const auto pkg_name = specs::strip_archive_extension(s.filename);
        LOG_DEBUG << "Verify cache '" << m_path.string() << "' for package tarball '" << pkg_name
                  << "'";

        bool valid = false;
        if (fs::exists(m_path / s.filename))
        {
            fs::u8path tarball_path = m_path / s.filename;
            // validate that this tarball has the right size and MD5 sum
            // we handle the case where s.size == 0 (explicit packages) or md5 is unknown
            valid = s.size == 0 || validation::file_size(tarball_path, s.size);
            if (!s.md5.empty())
            {
                valid = valid && (validation::md5sum(tarball_path) == s.md5);
            }
            else if (!s.sha256.empty())
            {
                valid = valid && (validation::sha256sum(tarball_path) == s.sha256);
            }
            else
            {
                if (params.safety_checks == VerificationLevel::Warn)
                {
                    LOG_WARNING << "Could not validate package '" + tarball_path.string()
                                       + "': md5 and sha256 sum unknown.\n"
                                         "Set safety_checks to disabled to override this warning.";
                }
                else if (params.safety_checks == VerificationLevel::Enabled)
                {
                    // we cannot validate this archive, but we could also not validate a downloaded
                    // archive since we just don't know the sha256 or md5 sum
                    throw std::runtime_error(
                        "Could not validate package '" + tarball_path.string()
                        + "': md5 and sha256 sum unknown.\n"
                          "Set safety_checks to warn or disabled to override this error."
                    );
                }
            }

            if (valid)
            {
                LOG_TRACE << "Package tarball '" << tarball_path.string() << "' is valid";
            }
            else
            {
                std::stringstream msg;
                msg << "Package tarball '" << tarball_path.string() << "' is invalid.\n";
                if (s.size != 0)
                {
                    msg << "  - Expected size     : " << s.size << "\n"
                        << "  - Effective size    : " << fs::file_size(tarball_path) << "\n";
                }
                if (!s.md5.empty())
                {
                    msg << "  - Expected md5      : " << s.md5 << "\n"
                        << "  - Effective md5     : " << validation::md5sum(tarball_path) << "\n";
                }
                if (!s.sha256.empty())
                {
                    msg << "  - Expected sha256   : " << s.sha256 << "\n"
                        << "  - Effective sha256  : " << validation::sha256sum(tarball_path) << "\n";
                }
                msg << "Deleting '" << tarball_path.string() << "'";
                LOG_TRACE << msg.str();

                std::error_code ec;
                fs::remove(tarball_path, ec);

                if (ec)
                {
                    LOG_ERROR << "Failed to remove invalid tarball '" << tarball_path.string()
                              << "' (error_code: " << ec.message() << ")";
                }
                else
                {
                    LOG_TRACE << "Package tarball '" << tarball_path.string() << "' removed";
                }
            }
            m_valid_tarballs[pkg] = valid;
        }

        LOG_DEBUG << "'" << pkg_name << "' tarball cache is " << (valid ? "valid" : "invalid");
        return valid;
    }

    namespace
    {
        bool compare_cleaned_url(std::string_view url_str1, std::string_view url_str2)
        {
            using Credentials = specs::CondaURL::Credentials;

            auto url1 = specs::CondaURL::parse(url_str1);
            if (!url1)
            {
                return false;
            }
            auto url2 = specs::CondaURL::parse(url_str2);
            if (!url2)
            {
                return false;
            }
            url1->set_scheme("https");
            url2->set_scheme("https");
            return util::rstrip(url1->str(Credentials::Remove), '/')
                   == util::rstrip(url2->str(Credentials::Remove), '/');
        }
    }

    bool
    PackageCacheData::has_valid_extracted_dir(const specs::PackageInfo& s, const ValidationParams& params)
    {
        bool valid = false, can_validate = false;

        std::string pkg = s.str();
        if (m_valid_extracted_dir.find(pkg) != m_valid_extracted_dir.end())
        {
            return m_valid_extracted_dir[pkg];
        }

        auto pkg_name = specs::strip_archive_extension(s.filename);
        fs::u8path extracted_dir = m_path / pkg_name;
        LOG_DEBUG << "Verify cache '" << m_path.string() << "' for package extracted directory '"
                  << pkg_name << "'";

        if (fs::exists(extracted_dir))
        {
            auto repodata_record_path = extracted_dir / "info" / "repodata_record.json";
            if (fs::exists(repodata_record_path))
            {
                try
                {
                    std::ifstream repodata_record_f(repodata_record_path.std_path());
                    nlohmann::json repodata_record;
                    repodata_record_f >> repodata_record;

                    valid = true;

                    // we can only validate if we have at least one data point of these three
                    can_validate = (!s.md5.empty() && repodata_record.contains("md5"))
                                   || (!s.sha256.empty() && repodata_record.contains("sha256"));
                    if (!can_validate)
                    {
                        if (params.safety_checks == VerificationLevel::Warn)
                        {
                            LOG_WARNING
                                << "Could not validate package '" + repodata_record_path.string()
                                       + "': md5 and sha256 sum unknown.\n"
                                         "Set safety_checks to disabled to override this warning.";
                        }
                        else if (params.safety_checks == VerificationLevel::Enabled)
                        {
                            throw std::runtime_error(
                                "Could not validate package '" + repodata_record_path.string()
                                + "': md5 and sha256 sum unknown.\n"
                                  "Set safety_checks to warn or disabled to override this error."
                            );
                        }
                    }

                    // Validate size
                    if (s.size != 0)
                    {
                        valid = repodata_record["size"].get<std::size_t>() == s.size;
                        if (!valid)
                        {
                            LOG_WARNING << "Extracted package cache '" << extracted_dir.string()
                                        << "' has invalid size";
                        }
                    }

                    // Validate checksum
                    if (!s.sha256.empty() && repodata_record.contains("sha256"))
                    {
                        // TODO handle case if repodata_record __does not__ contain any value
                        if (s.sha256 != repodata_record["sha256"].get<std::string>())
                        {
                            valid = false;
                            LOG_WARNING << "Extracted package cache '" << extracted_dir.string()
                                        << "' has invalid SHA-256 checksum";
                        }
                        else if (s.size == 0)
                        {
                            // in case we have no s.size
                            // set valid true here
                            valid = true;
                        }
                    }
                    else if (!s.md5.empty() && repodata_record.contains("md5"))
                    {
                        // TODO handle case if repodata_record __does not__ contain any value
                        if (s.md5 != repodata_record["md5"].get<std::string>())
                        {
                            LOG_WARNING << "Extracted package cache '" << extracted_dir.string()
                                        << "' has invalid MD5 checksum";
                            valid = false;
                        }
                        else if (s.size == 0)
                        {
                            // for explicit env, we have no size, nor sha256 so we need to
                            // set valid true here
                            valid = true;
                        }
                    }
                    else if (s.size != 0)
                    {
                        // cannot validate if we don't know either md5 or sha256
                        LOG_WARNING << "Extracted package cache '" << extracted_dir.string()
                                    << "' has no checksum";
                        valid = false;
                    }

                    // Validate URL
                    if (valid)
                    {
                        if (!repodata_record["url"].get<std::string>().empty())
                        {
                            const auto pkg_url = repodata_record["url"].get<std::string>();
                            if (!compare_cleaned_url(pkg_url, s.package_url))
                            {
                                LOG_WARNING << "Extracted package cache '" << extracted_dir.string()
                                            << "' has invalid url";
                                valid = false;
                            }
                        }
                        else
                        {
                            const auto pkg_channel = repodata_record["channel"].get<std::string>();
                            if (pkg_channel != s.channel)
                            {
                                LOG_WARNING << "Extracted package cache '" << extracted_dir.string()
                                            << "' has invalid channel";
                                valid = false;
                            }
                        }
                    }
                }
                catch (const nlohmann::json::exception& e)
                {
                    LOG_WARNING << "Extracted package cache '" << extracted_dir.string()
                                << "' has invalid 'repodata_record.json' file: " << e.what();
                    valid = false;
                }
                catch (const std::runtime_error& re)
                {
                    LOG_WARNING << "Extracted package cache '" << extracted_dir.string()
                                << " couldn't be validated due to runtime error: " << re.what();
                    valid = false;
                }
                catch (const std::exception& ex)
                {
                    LOG_WARNING << "Extracted package cache '" << extracted_dir.string()
                                << " couldn't be validated due to error: " << ex.what();
                    valid = false;
                }

                if (valid)
                {
                    valid = validate(extracted_dir, params);
                }
            }
        }
        else
        {
            LOG_DEBUG << "Extracted package cache '" << extracted_dir.string() << "' not found";
        }

        m_valid_extracted_dir[pkg] = valid;
        LOG_DEBUG << "'" << pkg_name << "' extracted directory cache is "
                  << (valid ? "valid" : "invalid");

        return valid;
    }

    MultiPackageCache::MultiPackageCache(
        const std::vector<fs::u8path>& cache_paths,
        const ValidationParams& params
    )
        : m_params(params)
    {
        m_caches.reserve(cache_paths.size());
        for (auto& c : cache_paths)
        {
            m_caches.emplace_back(c);
        }
    }

    PackageCacheData& MultiPackageCache::first_writable_cache(bool create)
    {
        for (auto& pc : m_caches)
        {
            auto status = pc.is_writable();
            if (status == Writable::WRITABLE)
            {
                return pc;
            }
            else if (create && (status == Writable::DIR_DOES_NOT_EXIST))
            {
                bool created = pc.create_directory();
                if (created)
                {
                    pc.set_writable(Writable::WRITABLE);
                    return pc;
                }
            }
        }
        // TODO better error class?!
        throw std::runtime_error("Did not find a writable package cache directory!");
    }

    std::vector<PackageCacheData*> MultiPackageCache::writable_caches()
    {
        std::vector<PackageCacheData*> res;
        for (auto& pc : m_caches)
        {
            auto status = pc.is_writable();
            if (status == Writable::WRITABLE)
            {
                res.push_back(&pc);
            }
        }
        return res;
    }

    fs::u8path MultiPackageCache::first_writable_path()
    {
        for (auto& pc : m_caches)
        {
            if (pc.is_writable() == Writable::WRITABLE)
            {
                return pc.path();
            }
        }

        return fs::u8path();
    }

    fs::u8path MultiPackageCache::get_tarball_path(const specs::PackageInfo& s, bool return_empty)
    {
        const std::string pkg(s.str());
        const auto cache_iter(m_cached_tarballs.find(pkg));
        if (cache_iter != m_cached_tarballs.end())
        {
            return cache_iter->second;
        }

        for (PackageCacheData& c : m_caches)
        {
            if (c.has_valid_tarball(s, m_params))
            {
                m_cached_tarballs[pkg] = c.path();
                return c.path();
            }
        }

        if (return_empty)
        {
            return fs::u8path();
        }
        else
        {
            LOG_ERROR << "Cannot find tarball cache for '" << s.filename << "'";
            throw std::runtime_error("Package cache error.");
        }
    }

    fs::u8path
    MultiPackageCache::get_extracted_dir_path(const specs::PackageInfo& s, bool return_empty)
    {
        const std::string pkg(s.str());
        const auto cache_iter(m_cached_extracted_dirs.find(pkg));
        if (cache_iter != m_cached_extracted_dirs.end())
        {
            return cache_iter->second;
        }

        for (auto& c : m_caches)
        {
            if (c.has_valid_extracted_dir(s, m_params))
            {
                m_cached_extracted_dirs[pkg] = c.path();
                return c.path();
            }
        }

        if (return_empty)
        {
            return fs::u8path();
        }
        else
        {
            LOG_ERROR << "Cannot find a valid extracted directory cache for '" << s.filename << "'";
            throw std::runtime_error("Package cache error.");
        }
    }

    std::vector<fs::u8path> MultiPackageCache::paths() const
    {
        std::vector<fs::u8path> paths;
        for (auto& c : m_caches)
        {
            paths.push_back(c.path());
        }

        return paths;
    }

    void MultiPackageCache::clear_query_cache(const specs::PackageInfo& s)
    {
        for (auto& c : m_caches)
        {
            c.clear_query_cache(s);
        }
    }
}  // namespace mamba
