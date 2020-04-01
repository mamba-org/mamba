#include <vector>
#include <string>

#include "constants.hpp"
#include "context.hpp"
#include "path.hpp"

enum Writable {
    UNKNOWN,
    WRITABLE,
    NOT_WRITABLE,
    DIR_DOES_NOT_EXIST
};

class PackageCacheData
{
public:
    PackageCacheData(const std::string& pkgs_dir)
        : m_pkgs_dir(pkgs_dir)
    {
    }

    bool create_directory()
    {
        // from python: create_package_cache_directory
        try
        {
            std::cout << "Attempt to create package cache directory '" << m_pkgs_dir << "'" << std::endl;
            bool sudo_safe = path::starts_with_home(m_pkgs_dir);
            path::touch(path::join(m_pkgs_dir, PACKAGE_CACHE_MAGIC_FILE), sudo_safe, /*mkdir*/true);
            // TODO why magic string "urls" here? is it necessary?
            path::touch(path::join(m_pkgs_dir, "urls"), sudo_safe);
            return true;
        }
        catch(...)
        {
            // TODO better error handling
            std::cout << "cannot create package cache directory " << m_pkgs_dir << std::endl;
            return false;
        }
    }

    void set_writable(Writable writable)
    {
        m_writable = writable;
    }

    Writable is_writable()
    {
        if (m_writable == UNKNOWN)
        {
            check_writable();
        }
        return m_writable;
    }

    static PackageCacheData first_writable(const std::vector<std::string>* pkgs_dirs = nullptr) {
        const std::vector<std::string>* dirs = pkgs_dirs ? pkgs_dirs : &Context::instance()->pkgs_dirs;
        for (const auto& dir : (*dirs))
        {
            std::cout << "Checking dir " << dir << std::endl;
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

private:

    void check_writable()
    {
        fs::path magic_file = fs::path(m_pkgs_dir) / fs::path(PACKAGE_CACHE_MAGIC_FILE);
        if (fs::is_regular_file(magic_file))
        {
            std::cout << "File exists, checking if writable" << std::endl;;
            if (path::file_path_is_writable(magic_file))
            {
                m_writable = WRITABLE;
                std::cout << "Yes, writable" << std::endl;;
            }
            else
            {
                m_writable = NOT_WRITABLE;
                std::cout << "Not writable" << std::endl;;
            }
        }
        else
        {
            m_writable = DIR_DOES_NOT_EXIST;
        }
    }

    Writable m_writable = UNKNOWN;
    std::string m_pkgs_dir;
};