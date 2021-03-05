// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/package_cache.hpp"
#include "nlohmann/json.hpp"
#include "mamba/package_handling.hpp"
#include "mamba/validate.hpp"
#include "mamba/url.hpp"

namespace mamba
{
    PackageCacheData::PackageCacheData(const fs::path& pkgs_dir)
        : m_pkgs_dir(pkgs_dir)
    {
    }

    bool PackageCacheData::create_directory()
    {
        try
        {
            LOG_INFO << "Attempt to create package cache directory " << m_pkgs_dir;
            bool sudo_safe = path::starts_with_home(m_pkgs_dir);
            path::touch(m_pkgs_dir / PACKAGE_CACHE_MAGIC_FILE,
                        sudo_safe,
                        /*mkdir*/ true);
            // TODO why magic string "urls" here? is it necessary?
            path::touch(m_pkgs_dir / "urls", sudo_safe);
            return true;
        }
        catch (...)
        {
            // TODO better error handling
            LOG_ERROR << "cannot create package cache directory " << m_pkgs_dir;
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

    fs::path PackageCacheData::get_pkgs_dir() const
    {
        return m_pkgs_dir;
    }

    PackageCacheData PackageCacheData::first_writable(const std::vector<fs::path>* pkgs_dirs)
    {
        const std::vector<fs::path>* dirs = pkgs_dirs ? pkgs_dirs : &Context::instance().pkgs_dirs;
        for (const auto& dir : (*dirs))
        {
            LOG_INFO << "Checking dir " << dir;
            PackageCacheData pkgs_cache(dir);
            auto is_wri = pkgs_cache.is_writable();

            if (is_wri == Writable::WRITABLE)
            {
                return pkgs_cache;
            }
            else if (is_wri == Writable::DIR_DOES_NOT_EXIST)
            {
                bool created = pkgs_cache.create_directory();
                if (created)
                {
                    pkgs_cache.set_writable(Writable::WRITABLE);
                    return pkgs_cache;
                }
            }
        }
        // TODO better error class?!
        throw std::runtime_error("Did not find a writable package cache directory!");
    }

    void PackageCacheData::check_writable()
    {
        fs::path magic_file = m_pkgs_dir / PACKAGE_CACHE_MAGIC_FILE;
        if (fs::is_regular_file(magic_file))
        {
            LOG_INFO << magic_file << " exists, checking if writable";
            if (path::is_writable(magic_file))
            {
                LOG_INFO << magic_file << " writable";
                m_writable = Writable::WRITABLE;
            }
            else
            {
                m_writable = Writable::NOT_WRITABLE;
                LOG_INFO << magic_file << " not writable";
            }
        }
        else
        {
            LOG_INFO << magic_file << " does not exists";
            m_writable = Writable::DIR_DOES_NOT_EXIST;
        }
    }

    bool PackageCacheData::query(const PackageInfo& s)
    {
        std::string pkg = s.str();
        if (m_valid_cache.find(pkg) != m_valid_cache.end())
        {
            return m_valid_cache[pkg];
        }

        assert(!s.fn.empty());

        bool valid = false, extract_dir_valid = false;
        if (fs::exists(m_pkgs_dir / s.fn))
        {
            fs::path tarball_path = m_pkgs_dir / s.fn;
            // validate that this tarball has the right size and MD5 sum
            valid = validate::file_size(tarball_path, s.size);
            valid = (valid || s.size == 0) && validate::md5(tarball_path, s.md5);
            LOG_INFO << tarball_path << " is " << valid;
            m_valid_cache[pkg] = valid;
        }

        fs::path extract_dir = m_pkgs_dir / strip_package_extension(s.fn);
        if (fs::exists(extract_dir))
        {
            auto repodata_record_path = extract_dir / "info" / "repodata_record.json";
            if (fs::exists(repodata_record_path))
            {
                try
                {
                    std::ifstream repodata_record_f(repodata_record_path);
                    nlohmann::json repodata_record;
                    repodata_record_f >> repodata_record;
                    extract_dir_valid = false;
                    if (s.size != 0)
                    {
                        extract_dir_valid = repodata_record["size"].get<std::size_t>() == s.size;
                        if (!extract_dir_valid)
                        {
                            LOG_INFO << "Found cache, size differs.";
                        }
                    }
                    if (!s.sha256.empty())
                    {
                        if (s.sha256 != repodata_record["sha256"].get<std::string>())
                        {
                            extract_dir_valid = false;
                            LOG_INFO << "Found cache, sha256 differs.";
                        }
                        else if (s.size == 0)
                        {
                            // in case we have no s.size
                            // set extract_dir_valid true here
                            extract_dir_valid = true;
                        }
                    }
                    else if (!s.md5.empty())
                    {
                        if (s.md5 != repodata_record["md5"].get<std::string>())
                        {
                            LOG_INFO << "Found cache, md5 differs. ("
                                     << repodata_record["md5"].get<std::string>() << " vs " << s.md5
                                     << ")";
                            extract_dir_valid = false;
                        }
                        else if (s.size == 0)
                        {
                            // for explicit env, we have no size, nor sha256 so we need to
                            // set extract_dir_valid true here
                            extract_dir_valid = true;
                        }
                    }
                    else
                    {
                        // cannot validate if we don't know either md5 or sha256
                        LOG_INFO << "Found cache, no checksum.";
                        extract_dir_valid = false;
                    }
                    if (!repodata_record["url"].get<std::string>().empty())
                    {
                        if (!compare_cleaned_url(repodata_record["url"].get<std::string>(), s.url))
                        {
                            LOG_INFO << "Found cache, url differs.";
                            extract_dir_valid = false;
                        }
                    }
                    else
                    {
                        if (repodata_record["channel"].get<std::string>() != s.channel)
                        {
                            extract_dir_valid = false;
                            LOG_INFO << "Found cache, channel differs.";
                        }
                    }

                    if (!extract_dir_valid)
                    {
                        LOG_INFO << "Found directory with same name, but different size, "
                                    "channel, url or checksum "
                                 << repodata_record_path;
                    }
                }
                catch (...)
                {
                    LOG_WARNING << "Found corrupted repodata_record file " << repodata_record_path;
                    extract_dir_valid = false;
                }

                if (extract_dir_valid)
                {
                    extract_dir_valid = validate(extract_dir);
                }
            }
            if (!extract_dir_valid)
            {
                try
                {
                    remove_or_rename(extract_dir);
                }
                catch (const std::runtime_error& e)
                {
#ifdef UMAMBA_ONLY
                    throw e;
#endif
                }
            }
            else
            {
                valid = true;
            }
        }
        m_valid_cache[pkg] = valid;
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

    PackageCacheData& MultiPackageCache::first_writable()
    {
        for (auto& pc : m_caches)
        {
            auto status = pc.is_writable();
            if (status == Writable::WRITABLE)
            {
                return pc;
            }
            else if (status == Writable::DIR_DOES_NOT_EXIST)
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

    bool MultiPackageCache::query(const PackageInfo& s)
    {
        for (auto& c : m_caches)
        {
            if (c.query(s))
                return true;
        }
        return false;
    }
}  // namespace mamba
