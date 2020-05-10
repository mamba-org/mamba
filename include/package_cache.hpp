#ifndef MAMBA_PACKAGE_CACHE
#define MAMBA_PACKAGE_CACHE

#include <vector>
#include <string>
#include <iostream>

#include "output.hpp"
#include "context.hpp"
#include "util.hpp"
#include "environment.hpp"

#define PACKAGE_CACHE_MAGIC_FILE "urls.txt"

enum Writable {
    UNKNOWN,
    WRITABLE,
    NOT_WRITABLE,
    DIR_DOES_NOT_EXIST
};

namespace mamba
{
    namespace path
    {
        bool starts_with_home(const fs::path& p)
        {
            std::string path = p;
            return path[0] == '~' || starts_with(env::expand_user(path).string(), env::expand_user("~").string());
        }

        void mkdir_p(const fs::path& path)
        {
            if (fs::is_directory(path)) return;
            std::cout << "making directory " << path << std::endl;
            fs::create_directories(path);
        }

        // TODO more error handling
        void mkdir_p_sudo_safe(const fs::path& path)
        {
            if (fs::is_directory(path)) return;

            fs::path base_dir = path.parent_path();
            if (!fs::is_directory(base_dir))
            {
                mkdir_p_sudo_safe(base_dir);
            }
            std::cout << "making directory " << path << std::endl;
            fs::create_directory(path);

            #ifndef _WIN32
            // set permissions to 0o2775
            fs::permissions(path, fs::perms::set_gid |
                                  fs::perms::owner_all | 
                                  fs::perms::group_all |
                                  fs::perms::others_read | fs::perms::others_exec);
            #endif
        }

        bool touch(fs::path path, bool mkdir=false, bool sudo_safe=false)
        {
            // TODO error handling!
            path = env::expand_user(path);
            std::cout << "touching " << path << std::endl;
            if (lexists(path))
            {
                fs::last_write_time(path, fs::file_time_type::clock::now());
                return true;
            }
            else
            {
                auto dirpath = path.parent_path();
                if (!fs::is_directory(dirpath) && mkdir)
                {
                    if (sudo_safe)
                    {
                        mkdir_p_sudo_safe(dirpath);
                    }
                    else
                    {
                        mkdir_p(dirpath);
                    }
                }
                // directory exists, now create empty file
                std::ofstream(path);
                return false;
            }
        }

        bool file_path_is_writable(const fs::path& path)
        {
            // path = env::expand_user(path)
            std::cout << "Checking that path is writable: " << path << std::endl;
            if (fs::is_directory(path.parent_path()))
            {
                bool path_existed = lexists(path);
                std::ofstream test;
                test.open(path);
                bool is_writable = test.is_open();
                if (!path_existed)
                {
                    test.close();
                    fs::remove(path);
                }
                return is_writable;
            }
            else
            {
                throw std::runtime_error("Cannot check file path at `/` for accessibility.");
            }
        }
    }


    class PackageCacheData
    {
    public:
        PackageCacheData(const fs::path& pkgs_dir)
            : m_pkgs_dir(pkgs_dir)
        {
        }

        bool create_directory()
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

        static PackageCacheData first_writable(const std::vector<fs::path>* pkgs_dirs = nullptr) {
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

    private:

        void check_writable()
        {
            fs::path magic_file = m_pkgs_dir / PACKAGE_CACHE_MAGIC_FILE;
            if (fs::is_regular_file(magic_file))
            {
                LOG_INFO << magic_file << " exists, checking if writable";
                if (path::file_path_is_writable(magic_file))
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

        Writable m_writable = UNKNOWN;
        fs::path m_pkgs_dir;
    };
}

#endif