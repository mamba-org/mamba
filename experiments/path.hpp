#pragma once

#include <string>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <regex>

// #include <pystring14/pystring.hpp>

// MD5 hash
#include <openssl/md5.h>

namespace fs = std::filesystem;

static bool ends_with(const std::string& str, const std::string& suffix)
{
    return str.size() >= suffix.size() && 0 == str.compare(str.size() - suffix.size(), suffix.size(), suffix);
}

static bool starts_with(const std::string& str, const std::string& prefix)
{
    return str.size() >= prefix.size() && 0 == str.compare(0, prefix.size(), prefix);
}

namespace path
{
    constexpr char pathsep() {
        #if _WIN32
            return '\\';
        #else
            return '/';
        #endif
    }

    std::string join(const std::string& a, const std::string& b)
    {
        std::string res = a;
        // TODO do something more clever here! (adding slashes, and checking for double slashes)
        if (res.size() && res[res.size() - 1] != pathsep()) res += pathsep();
        res += b;
        return res;
    }

    std::string cache_fn_url(const std::string& url)
    {
        // if (repodata_fn != REPODATA_FN) url += repodata_fn;

        unsigned char result[MD5_DIGEST_LENGTH];
        MD5((const unsigned char*) url.c_str(), url.size(), result);
        
        std::ostringstream sout;
        sout << std::hex << std::setfill('0');

        for(long long c: result)
        {
            sout << std::setw(2) << (long long) c;
        }

        return sout.str() + ".json";
    }

    bool uncompress_bz2(const std::string& path)
    {
        // remove `.bz2` from end of filename
        std::string final_name(path.begin(), path.end() - 4);
        std::string cmd = std::string("bzip2 --stdout -d ") + path + " > " + final_name; 
        std::cout << "Executing: " << cmd << std::endl;
        std::system(cmd.c_str());
        return true;
        // cmd = std::string("rm ") + path;
        // std::system(cmd.c_str());
    }

    // expand user
    // note this should also expand to absolute path etc.
    fs::path expand(const fs::path& path)
    {
        std::string p = path;
        if (not p.empty() and p[0] == '~')
        {
            const char* home = getenv("HOME");

            if (home || (home = getenv("USERPROFILE")))
            {
                p.replace(0, 1, home);
            }
            else
            {
                const char* hdrive = getenv("HOMEDRIVE");
                const char* hpath = getenv("HOMEPATH");
                if (!hdrive || !hpath) throw std::runtime_error("Could not find $HOME, %USERPROFILE%, %HOMEDRIVE% or %HOMEPATH%.");
                p.replace(0, 1, std::string(hdrive) + hpath);
            }
        }
        return p;
    }

    bool starts_with_home(const fs::path& path)
    {
        std::string ps = path;
        if (ps[0] == '~') return true;
        auto home = expand("~");
        if (starts_with(path, home))
        {
            return true;
        }
        return false;
    }

    bool lexists(const fs::path& path)
    {
        return fs::status_known(fs::symlink_status(path));
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

    template <class T>
    auto now(std::chrono::time_point<T>)
    {
        return T::now();
    }

    bool touch(fs::path path, bool mkdir=false, bool sudo_safe=false)
    {
        // TODO error handling!
        path = expand(path);
        std::cout << "touching " << path << std::endl;
        if (lexists(path))
        {
            fs::last_write_time(path, now(fs::file_time_type{}));
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
        // path = expand(path)
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

    // PATH_MATCH_REGEX = (
    //     r"\./"              # ./
    //     r"|\.\."            # ..
    //     r"|~"               # ~
    //     r"|/"               # /
    //     r"|[a-zA-Z]:[/\\]"  # drive letter, colon, forward or backslash
    //     r"|\\\\"            # windows UNC path
    //     r"|//"              # windows UNC path
    // )

    const std::regex path_regex("\\./|\\.\\.|~|/|[a-zA-Z]:[/\\\\]|\\\\\\\\|//");

    bool is_path(const std::string& value)
    {
        if (value.find("://") != std::string::npos) 
        {
            return false;
        }
        return std::regex_search(value, path_regex);
    }

// def path_to_url(path):
//     if not path:
//         raise ValueError('Not allowed: %r' % path)
//     if path.startswith(file_scheme):
//         try:
//             path.decode('ascii')
//         except UnicodeDecodeError:
//             raise ValueError('Non-ascii not allowed for things claiming to be URLs: %r' % path)
//         return path
//     path = abspath(expanduser(path)).replace('\\', '/')
//     # We do not use urljoin here because we want to take our own
//     # *very* explicit control of how paths get encoded into URLs.
//     #   We should not follow any RFCs on how to encode and decode
//     # them, we just need to make sure we can represent them in a
//     # way that will not cause problems for whatever amount of
//     # urllib processing we *do* need to do on them (which should
//     # be none anyway, but I doubt that is the case). I have gone
//     # for ASCII and % encoding of everything not alphanumeric or
//     # not in `!'()*-._/:`. This should be pretty save.
//     #
//     # To avoid risking breaking the internet, this code only runs
//     # for `file://` URLs.
//     #
//     percent_encode_chars = "!'()*-._/\\:"
//     percent_encode = lambda s: "".join(["%%%02X" % ord(c), c]
//                                        [c < "{" and c.isalnum() or c in percent_encode_chars]
//                                        for c in s)
//     if any(ord(char) >= 128 for char in path):
//         path = percent_encode(path.decode('unicode-escape')
//                               if hasattr(path, 'decode')
//                               else bytes(path, "utf-8").decode('unicode-escape'))

//     # https://blogs.msdn.microsoft.com/ie/2006/12/06/file-uris-in-windows/
//     if len(path) > 1 and path[1] == ':':
//         path = file_scheme + '/' + path
//     else:
//         path = file_scheme + path
//     return path

    bool is_package_file(const std::string& path)
    {
        // TODO use constants here!
        return ends_with(path, ".conda") || ends_with(path, ".tar.bz2");
    }

    std::string win_path_backout(const std::string& path)
    {
        // replace all backslashes except those escaping spaces
        // if we pass a file url, something like file://\\unc\path\on\win, make sure
        //   we clean that up too
        std::regex rex("(\\\\(?! ))");
        auto res = std::regex_replace(path, rex, "/");
        res = std::regex_replace(res, std::regex(":////"), "://");
        return res;
    }
}
