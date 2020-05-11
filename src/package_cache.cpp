#include "package_cache.hpp"

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
            path::touch(m_pkgs_dir / PACKAGE_CACHE_MAGIC_FILE, sudo_safe, /*mkdir*/true);
            // TODO why magic string "urls" here? is it necessary?
            path::touch(m_pkgs_dir / "urls", sudo_safe);
            return true;
        }
        catch(...)
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
        if (m_writable == UNKNOWN)
        {
            check_writable();
        }
        return m_writable;
    }

    PackageCacheData
    PackageCacheData::first_writable(const std::vector<fs::path>* pkgs_dirs)
    {
        const std::vector<fs::path>* dirs = pkgs_dirs ? pkgs_dirs : &Context::instance().pkgs_dirs;
        for (const auto& dir : (*dirs))
        {
            LOG_INFO << "Checking dir " << dir;
            PackageCacheData pkgs_cache(dir);
            auto is_wri = pkgs_cache.is_writable();

            if (is_wri == WRITABLE)
            {
                return pkgs_cache;
            }
            else if (is_wri == DIR_DOES_NOT_EXIST)
            {
                bool created = pkgs_cache.create_directory();
                if (created)
                {
                    pkgs_cache.set_writable(WRITABLE);
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
                m_writable = WRITABLE;
            }
            else
            {
                m_writable = NOT_WRITABLE;
                LOG_INFO << magic_file << " not writable";
            }
        }
        else
        {
            LOG_INFO << magic_file << " does not exists";
            m_writable = DIR_DOES_NOT_EXIST;
        }
    }
}
