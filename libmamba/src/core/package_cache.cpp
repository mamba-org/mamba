// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <iostream>
#include "nlohmann/json.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/package_cache.hpp"
#include "mamba/core/package_handling.hpp"
#include "mamba/core/validate.hpp"
#include "mamba/core/url.hpp"

namespace mamba
{
    PackageCacheData::PackageCacheData(const fs::path& path)
        : m_path(path)
    {
    }

    bool PackageCacheData::create_directory()
    {
        try
        {
            LOG_DEBUG << "Attempt to create package cache directory '" << m_path.string() << "'";
            bool sudo_safe = path::starts_with_home(m_path);
            path::touch(m_path / PACKAGE_CACHE_MAGIC_FILE,
                        /*mkdir*/ true,
                        sudo_safe);
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
            check_writable();

        return m_writable;
    }

    fs::path PackageCacheData::path() const
    {
        return m_path;
    }

    void PackageCacheData::clear_query_cache(const PackageInfo& s)
    {
        m_valid_tarballs.erase(s.str());
        m_valid_extracted_dir.erase(s.str());
    }

    void PackageCacheData::check_writable()
    {
        fs::path magic_file = m_path / PACKAGE_CACHE_MAGIC_FILE;
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
            LOG_TRACE << "Cache path does not exists or is not writable";

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

    bool PackageCacheData::has_valid_tarball(const PackageInfo& s)
    {
        std::string pkg = s.str();
        if (m_valid_tarballs.find(pkg) != m_valid_tarballs.end())
        {
            return m_valid_tarballs[pkg];
        }

        assert(!s.fn.empty());
        auto pkg_name = strip_package_extension(s.fn);
        LOG_DEBUG << "Verify cache '" << m_path.string() << "' for package tarball '"
                  << pkg_name.string() << "'";

        bool valid = false;
        if (fs::exists(m_path / s.fn))
        {
            fs::path tarball_path = m_path / s.fn;
            // validate that this tarball has the right size and MD5 sum
            // we handle the case where s.size == 0 (explicit packages) or md5 is unknown
            valid = s.size == 0 || validate::file_size(tarball_path, s.size);
            if (!s.md5.empty())
            {
                valid = valid && validate::md5(tarball_path, s.md5);
            }
            else if (!s.sha256.empty())
            {
                valid = valid && validate::sha256(tarball_path, s.md5);
            }
            else
            {
                if (Context::instance().safety_checks == VerificationLevel::kWarn)
                {
                    LOG_WARNING << "Could not validate package '" + tarball_path.string()
                                       + "': md5 and sha256 sum unknown.\n"
                                         "Set safety_checks to disabled to override this warning.";
                }
                else if (Context::instance().safety_checks == VerificationLevel::kEnabled)
                {
                    // we cannot validate this archive, but we could also not validate a downloaded
                    // archive since we just don't know the sha256 or md5 sum
                    throw std::runtime_error(
                        "Could not validate package '" + tarball_path.string()
                        + "': md5 and sha256 sum unknown.\n"
                          "Set safety_checks to warn or disabled to override this error.");
                }
            }

            if (valid)
                LOG_TRACE << "Package tarball '" << tarball_path.string() << "' is valid";
            else
                LOG_WARNING << "Package tarball '" << tarball_path.string() << "' is invalid";
            m_valid_tarballs[pkg] = valid;
        }

        LOG_DEBUG << "'" << pkg_name.string() << "' tarball cache is "
                  << (valid ? "valid" : "invalid");
        return valid;
    }

    bool PackageCacheData::has_valid_extracted_dir(const PackageInfo& s)
    {
        bool valid = false, can_validate = false;

        std::string pkg = s.str();
        if (m_valid_extracted_dir.find(pkg) != m_valid_extracted_dir.end())
        {
            return m_valid_extracted_dir[pkg];
        }

        auto pkg_name = strip_package_extension(s.fn);
        fs::path extracted_dir = m_path / pkg_name;
        LOG_DEBUG << "Verify cache '" << m_path.string() << "' for package extracted directory '"
                  << pkg_name.string() << "'";

        if (fs::exists(extracted_dir))
        {
            auto repodata_record_path = extracted_dir / "info" / "repodata_record.json";
            if (fs::exists(repodata_record_path))
            {
                try
                {
                    std::ifstream repodata_record_f(repodata_record_path);
                    nlohmann::json repodata_record;
                    repodata_record_f >> repodata_record;

                    valid = true;

                    // we can only validate if we have at least one data point of these three
                    can_validate = (!s.md5.empty() && repodata_record.contains("md5"))
                                   || (!s.sha256.empty() && repodata_record.contains("sha256"));
                    if (!can_validate)
                    {
                        if (Context::instance().safety_checks == VerificationLevel::kWarn)
                        {
                            LOG_WARNING
                                << "Could not validate package '" + repodata_record_path.string()
                                       + "': md5 and sha256 sum unknown.\n"
                                         "Set safety_checks to disabled to override this warning.";
                        }
                        else if (Context::instance().safety_checks == VerificationLevel::kEnabled)
                        {
                            throw std::runtime_error(
                                "Could not validate package '" + repodata_record_path.string()
                                + "': md5 and sha256 sum unknown.\n"
                                  "Set safety_checks to warn or disabled to override this error.");
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
                            if (!compare_cleaned_url(repodata_record["url"].get<std::string>(),
                                                     s.url))
                            {
                                LOG_WARNING << "Extracted package cache '" << extracted_dir.string()
                                            << "' has invalid url";
                                valid = false;
                            }
                        }
                        else
                        {
                            if (repodata_record["channel"].get<std::string>() != s.channel)
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
                    valid = validate(extracted_dir);
                }
            }
        }
        else
        {
            LOG_DEBUG << "Extracted package cache '" << extracted_dir.string() << "' not found";
        }

        m_valid_extracted_dir[pkg] = valid;
        LOG_DEBUG << "'" << pkg_name.string() << "' extracted directory cache is "
                  << (valid ? "valid" : "invalid");

        return valid;
    }

    MultiPackageCache::MultiPackageCache(const std::vector<fs::path>& cache_paths)
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

    fs::path MultiPackageCache::first_writable_path()
    {
        for (auto& pc : m_caches)
            if (pc.is_writable() == Writable::WRITABLE)
                return pc.path();

        return fs::path();
    }

    fs::path MultiPackageCache::get_tarball_path(const PackageInfo& s, bool return_empty)
    {
        const std::string pkg(s.str());
        const auto cache_iter(m_cached_tarballs.find(pkg));
        if (cache_iter != m_cached_tarballs.end())
        {
            return cache_iter->second;
        }

        for (PackageCacheData& c : m_caches)
            if (c.has_valid_tarball(s))
            {
                m_cached_tarballs[pkg] = c.path();
                return c.path();
            }

        if (return_empty)
            return fs::path();
        else
        {
            LOG_ERROR << "Cannot find tarball cache for '" << s.fn << "'";
            throw std::runtime_error("Package cache error.");
        }
    }


    fs::path MultiPackageCache::get_extracted_dir_path(const PackageInfo& s, bool return_empty)
    {
        const std::string pkg(s.str());
        const auto cache_iter(m_cached_extracted_dirs.find(pkg));
        if (cache_iter != m_cached_extracted_dirs.end())
        {
            return cache_iter->second;
        }

        for (auto& c : m_caches)
        {
            if (c.has_valid_extracted_dir(s))
            {
                m_cached_extracted_dirs[pkg] = c.path();
                return c.path();
            }
        }

        if (return_empty)
            return fs::path();
        else
        {
            LOG_ERROR << "Cannot find a valid extracted directory cache for '" << s.fn << "'";
            throw std::runtime_error("Package cache error.");
        }
    }

    std::vector<fs::path> MultiPackageCache::paths() const
    {
        std::vector<fs::path> paths;
        for (auto& c : m_caches)
            paths.push_back(c.path());

        return paths;
    }

    void MultiPackageCache::clear_query_cache(const PackageInfo& s)
    {
        for (auto& c : m_caches)
            c.clear_query_cache(s);
    }
}  // namespace mamba
