#include <iostream>
#include <iomanip>

#ifdef _WIN32
#include <io.h>
#include <cassert>
#endif

#include "util.hpp"

namespace mamba
{
    bool ends_with(const std::string_view& str, const std::string_view& suffix)
    {
        return str.size() >= suffix.size() && 0 == str.compare(str.size() - suffix.size(), suffix.size(), suffix);
    }

    bool starts_with(const std::string_view& str, const std::string_view& prefix)
    {
        return str.size() >= prefix.size() && 0 == str.compare(0, prefix.size(), prefix);
    }

    // TODO make sure this returns true even for broken symlinks
    // E.g.
    // ln -s abcdef emptylink
    // >>> import os.path
    // >>> os.path.lexists("emptylink")
    // True
    // >>> os.path.exists("emptylink")
    // False
    bool lexists(const fs::path& path)
    {
        return fs::exists(path); // && fs::status_known(fs::symlink_status(path));
    }

    bool is_package_file(const std::string_view& fn)
    {
        return ends_with(fn, ".tar.bz2") || ends_with(fn, ".conda");
    }

    void to_human_readable_filesize(std::ostream& o, double bytes, std::size_t precision)
    {
        const char* sizes[] = { " B", "KB", "MB", "GB", "TB" };
        int order = 0;
        while (bytes >= 1024 && order < (5 - 1))
        {
            order++;
            bytes = bytes / 1024;
        }
        o << std::fixed << std::setprecision(precision) << bytes << sizes[order];
    }

    TemporaryDirectory::TemporaryDirectory()
    {
        bool success = false;
        #ifndef _WIN32
            std::string template_path = fs::temp_directory_path() / "mambadXXXXXX";
            char* pth = mkdtemp((char*)template_path.c_str());
            success = (pth != nullptr);
            template_path = pth;
        #else
            std::string template_path = fs::temp_directory_path() / "mambadXXXXXX";
            // include \0 terminator
            auto err = _mktemp_s((char*)template_path.c_str(), template_path.size() + 1);
            assert(err == 0);
            success = fs::create_directory(template_path);
        #endif
        if (!success)
        {
            throw std::runtime_error("Could not create temporary directory!");
        }
        else
        {
            m_path = template_path;
        }
    }

    TemporaryDirectory::~TemporaryDirectory()
    {
        fs::remove_all(m_path);
    }

    fs::path& TemporaryDirectory::path()
    {
        return m_path;
    }

    TemporaryDirectory::operator fs::path()
    {
        return m_path;
    }

    TemporaryFile::TemporaryFile()
    {
        bool success = false;

        std::string template_path = fs::temp_directory_path() / "mambafXXXXXX";
        #ifndef _WIN32
            int fd = mkstemp((char*) template_path.c_str());
            success = (fd != 0);
        #else
            // include \0 terminator
            auto err = _mktemp_s((char*)template_path.c_str(), template_path.size() + 1);
            assert(err == 0);
            std::ofstream fcreate(template_path);
            success = true;
        #endif
        if (!success)
        {
            throw std::runtime_error("Could not create temporary file!");
        }
        else
        {
            m_path = template_path;
            #ifndef _WIN32
            close(fd);
            #else
            fcreate.close();
            #endif
        }
    }

    TemporaryFile::~TemporaryFile()
    {
        fs::remove(m_path);
    }

    fs::path& TemporaryFile::path()
    {
        return m_path;
    }

    TemporaryFile::operator fs::path()
    {
        return m_path;
    }

}

