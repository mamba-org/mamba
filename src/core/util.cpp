// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <cerrno>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <thread>

#if defined(__APPLE__) || defined(__linux__)
#include <csignal>
#include <unistd.h>
#include <sys/file.h>
#endif

#ifdef _WIN32
#include <io.h>

#include <cassert>

extern "C"
{
#include <io.h>
#include <process.h>
#include <sys/locking.h>
#include <fcntl.h>
}

#endif

#include "mamba/core/context.hpp"
#include "mamba/core/util.hpp"
#include "mamba/core/output.hpp"

namespace mamba
{
    bool is_package_file(const std::string_view& fn)
    {
        return ends_with(fn, ".tar.bz2") || ends_with(fn, ".conda");
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
        return fs::exists(path);  // && fs::status_known(fs::symlink_status(path));
    }

    void to_human_readable_filesize(std::ostream& o, double bytes, std::size_t precision)
    {
        const char* sizes[] = { "  B", " KB", " MB", " GB", " TB" };
        int order = 0;
        while (bytes >= 1024 && order < (5 - 1))
        {
            order++;
            bytes = bytes / 1024;
        }
        o << std::fixed << std::setprecision(precision) << bytes << sizes[order];
    }

    std::vector<fs::path> filter_dir(const fs::path& dir, const std::string& suffix)
    {
        std::vector<fs::path> result;
        if (fs::exists(dir) && fs::is_directory(dir))
        {
            for (const auto& entry : fs::directory_iterator(dir))
            {
                if (suffix.size())
                {
                    if (!entry.is_directory() && entry.path().extension() == suffix)
                    {
                        result.push_back(entry.path());
                    }
                }
                else
                {
                    if (entry.is_directory() == false)
                    {
                        result.push_back(entry.path());
                    }
                }
            }
        }
        return result;
    }

    // TODO expand variables, ~ and make absolute
    bool paths_equal(const fs::path& lhs, const fs::path& rhs)
    {
        return lhs == rhs;
    }

    TemporaryDirectory::TemporaryDirectory()
    {
        bool success = false;
#ifndef _WIN32
        std::string template_path = fs::temp_directory_path() / "mambadXXXXXX";
        char* pth = mkdtemp(const_cast<char*>(template_path.c_str()));
        success = (pth != nullptr);
        template_path = pth;
#else
        std::string template_path = fs::temp_directory_path() / "mambadXXXXXX";
        // include \0 terminator
        auto err = _mktemp_s(const_cast<char*>(template_path.c_str()), template_path.size() + 1);
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
        if (!Context::instance().keep_temp_directories)
        {
            fs::remove_all(m_path);
        }
    }

    fs::path& TemporaryDirectory::path()
    {
        return m_path;
    }

    TemporaryDirectory::operator fs::path()
    {
        return m_path;
    }

    TemporaryFile::TemporaryFile(const std::string& prefix, const std::string& suffix)
    {
        static std::mutex file_creation_mutex;

        bool success = false;
        fs::path temp_path = fs::temp_directory_path(), final_path;

        std::lock_guard<std::mutex> file_creation_lock(file_creation_mutex);

        do
        {
            std::string random_file_name = generate_random_alphanumeric_string(10);
            final_path = temp_path / concat(prefix, random_file_name, suffix);
        } while (fs::exists(final_path));

        try
        {
            std::ofstream f(final_path);
            f.close();
            success = true;
        }
        catch (...)
        {
            success = false;
        }

        if (!success)
        {
            throw std::runtime_error("Could not create temporary file!");
        }
        else
        {
            m_path = final_path;
        }
    }

    TemporaryFile::~TemporaryFile()
    {
        if (!Context::instance().keep_temp_files)
        {
            fs::remove(m_path);
        }
    }

    fs::path& TemporaryFile::path()
    {
        return m_path;
    }

    TemporaryFile::operator fs::path()
    {
        return m_path;
    }

    /********************
     * utils for string *
     ********************/

    bool ends_with(const std::string_view& str, const std::string_view& suffix)
    {
        return str.size() >= suffix.size()
               && 0 == str.compare(str.size() - suffix.size(), suffix.size(), suffix);
    }

    bool starts_with(const std::string_view& str, const std::string_view& prefix)
    {
        return str.size() >= prefix.size() && 0 == str.compare(0, prefix.size(), prefix);
    }

    bool any_starts_with(const std::vector<std::string_view>& str, const std::string_view& prefix)
    {
        for (auto& s : str)
        {
            if (starts_with(s, prefix))
                return true;
        }
        return false;
    }

    bool starts_with_any(const std::string_view& str, const std::vector<std::string_view>& prefix)
    {
        for (auto& p : prefix)
        {
            if (starts_with(str, p))
                return true;
        }
        return false;
    }

    bool contains(const std::string_view& str, const std::string_view& sub_str)
    {
        return str.find(sub_str) != std::string::npos;
    }

    std::string_view strip(const std::string_view& input)
    {
        return strip(input, WHITESPACES);
    }

    std::string_view lstrip(const std::string_view& input)
    {
        return lstrip(input, WHITESPACES);
    }

    std::string_view rstrip(const std::string_view& input)
    {
        return rstrip(input, WHITESPACES);
    }

    std::string_view strip(const std::string_view& input, const std::string_view& chars)
    {
        size_t start = input.find_first_not_of(chars);
        size_t stop = input.find_last_not_of(chars);
        return start == std::string::npos ? "" : input.substr(start, stop + 1);
    }

    std::string_view lstrip(const std::string_view& input, const std::string_view& chars)
    {
        size_t start = input.find_first_not_of(chars);
        return start == std::string::npos ? "" : input.substr(start);
    }

    std::string_view rstrip(const std::string_view& input, const std::string_view& chars)
    {
        size_t end = input.find_last_not_of(chars);
        return end == std::string::npos ? "" : input.substr(0, end + 1);
    }

    std::vector<std::string> split(const std::string_view& input,
                                   const std::string_view& sep,
                                   std::size_t max_split)
    {
        std::vector<std::string> result;
        std::size_t i = 0, j = 0, len = input.size(), n = sep.size();

        while (i + n <= len)
        {
            if (input[i] == sep[0] && input.substr(i, n) == sep)
            {
                if (max_split-- <= 0)
                    break;
                result.emplace_back(input.substr(j, i - j));
                i = j = i + n;
            }
            else
            {
                i++;
            }
        }
        result.emplace_back(input.substr(j, len - j));
        return result;
    }

    std::vector<std::string> rsplit(const std::string_view& input,
                                    const std::string_view& sep,
                                    std::size_t max_split)
    {
        if (max_split == SIZE_MAX)
            return split(input, sep, max_split);

        std::vector<std::string> result;

        std::ptrdiff_t i, j, len = static_cast<std::ptrdiff_t>(input.size()),
                             n = static_cast<std::ptrdiff_t>(sep.size());
        i = j = len;

        while (i >= n)
        {
            if (input[i - 1] == sep[n - 1] && input.substr(i - n, n) == sep)
            {
                if (max_split-- <= 0)
                {
                    break;
                }
                result.emplace_back(input.substr(i, j - i));
                i = j = i - n;
            }
            else
            {
                i--;
            }
        }
        result.emplace_back(input.substr(0, j));
        std::reverse(result.begin(), result.end());

        return result;
    }

    template <class S>
    void replace_all_impl(S& data, const S& search, const S& replace)
    {
        std::size_t pos = data.find(search);
        while (pos != std::string::npos)
        {
            data.replace(pos, search.size(), replace);
            pos = data.find(search, pos + replace.size());
        }
    }

    void replace_all(std::string& data, const std::string& search, const std::string& replace)
    {
        replace_all_impl<std::string>(data, search, replace);
    }

    void replace_all(std::wstring& data, const std::wstring& search, const std::wstring& replace)
    {
        replace_all_impl<std::wstring>(data, search, replace);
    }

    std::string string_transform(const std::string_view& input, int (*functor)(int))
    {
        std::string res(input);
        std::transform(
            res.begin(), res.end(), res.begin(), [&](unsigned char c) { return functor(c); });
        return res;
    }

    std::string to_upper(const std::string_view& input)
    {
        return string_transform(input, std::toupper);
    }

    std::string to_lower(const std::string_view& input)
    {
        return string_transform(input, std::tolower);
    }

    std::string read_contents(const fs::path& file_path, std::ios::openmode mode)
    {
        std::ifstream in(file_path, std::ios::in | mode);
        if (in)
        {
            std::string contents;
            in.seekg(0, std::ios::end);
            contents.resize(in.tellg());
            in.seekg(0, std::ios::beg);
            in.read(&contents[0], contents.size());
            in.close();
            return (contents);
        }
        else
        {
            throw std::system_error(
                errno, std::system_category(), "failed to open " + file_path.string());
        }
    }

    std::vector<std::string> read_lines(const fs::path& file_path)
    {
        std::fstream file_stream(file_path, std::ios_base::in | std::ios_base::binary);
        if (file_stream.fail())
        {
            throw std::system_error(
                errno, std::system_category(), "failed to open " + file_path.string());
        }

        std::vector<std::string> output;
        std::string line;
        while (std::getline(file_stream, line))
        {
            // Remove the trailing \r to accomodate Windows line endings.
            if ((!line.empty()) && (line.back() == '\r'))
                line.pop_back();

            output.push_back(line);
        }
        file_stream.close();

        return output;
    }

    void split_package_extension(const std::string& file, std::string& name, std::string& extension)
    {
        if (ends_with(file, ".conda"))
        {
            name = file.substr(0, file.size() - 6);
            extension = ".conda";
        }
        else if (ends_with(file, ".tar.bz2"))
        {
            name = file.substr(0, file.size() - 8);
            extension = ".tar.bz2";
        }
        else if (ends_with(file, ".json"))
        {
            name = file.substr(0, file.size() - 5);
            extension = ".json";
        }
        else
        {
            name = file;
            extension = "";
        }
    }

    fs::path strip_package_extension(const std::string& file)
    {
        std::string name, extension;
        split_package_extension(file, name, extension);

        if (extension == "")
        {
            throw std::runtime_error("Don't know how to handle " + file);
        }

        return name;
    }

    std::string quote_for_shell(const std::vector<std::string>& arguments, const std::string& shell)
    {
        if ((shell.empty() && on_win) || shell == "cmdexe")
        {
            // ported from CPython's list2cmdline to C++
            //
            // Translate a sequence of arguments into a command line
            // string, using the same rules as the MS C runtime:
            // 1) Arguments are delimited by white space, which is either a
            //    space or a tab.
            // 2) A string surrounded by double quotation marks is
            //    interpreted as a single argument, regardless of white space
            //    contained within.  A quoted string can be embedded in an
            //    argument.
            // 3) A double quotation mark preceded by a backslash is
            //    interpreted as a literal double quotation mark.
            // 4) Backslashes are interpreted literally, unless they
            //    immediately precede a double quotation mark.
            // 5) If backslashes immediately precede a double quotation mark,
            //    every pair of backslashes is interpreted as a literal
            //    backslash.  If the number of backslashes is odd, the last
            //    backslash escapes the next double quotation mark as
            //    described in rule 3.
            // See
            // http://msdn.microsoft.com/en-us/library/17w5ykft.aspx
            // or search http://msdn.microsoft.com for
            // "Parsing C++ Command-Line Arguments"
            std::string result, bs_buf;
            bool need_quote = false;
            for (const auto& arg : arguments)
            {
                bs_buf.clear();
                if (!result.empty())
                {
                    // seperate arguments
                    result += " ";
                }

                need_quote = arg.find_first_of(" \t") != arg.npos || arg.empty();
                if (need_quote)
                {
                    result += "\"";
                }

                for (char c : arg)
                {
                    if (c == '\\')
                    {
                        bs_buf += c;
                    }
                    else if (c == '"')
                    {
                        result += std::string(bs_buf.size() * 2, '\\');
                        bs_buf.clear();
                        result += "\\\"";
                    }
                    else
                    {
                        if (!bs_buf.empty())
                        {
                            result += bs_buf;
                            bs_buf.clear();
                        }
                        result += c;
                    }
                }
                if (!bs_buf.empty())
                {
                    result += bs_buf;
                }
                if (need_quote)
                {
                    result += bs_buf;
                    result += "\"";
                }
            }
            return result;
        }
        else
        {
            auto quote_arg = [](const std::string& s) {
                char quote_char;
                if (s.find('"') != s.npos)
                {
                    quote_char = '\'';
                }
                else if (s.find('\'') != s.npos)
                {
                    quote_char = '\"';
                }
                else if (s.find_first_of(" \n") == s.npos)
                {
                    return s;
                }
                else
                {
                    quote_char = '"';
                }
                return concat(quote_char, s, quote_char);
            };

            if (arguments.empty())
                return "";

            std::string argstring;
            argstring += quote_arg(arguments[0]);
            for (std::size_t i = 1; i < arguments.size(); ++i)
            {
                argstring += " ";
                argstring += quote_arg(arguments[i]);
            }
            return argstring;
        }
    }

    void remove_or_rename(const fs::path& path)
    {
        if (fs::is_directory(path))
        {
            try
            {
                fs::remove_all(path);
            }
            catch (const fs::filesystem_error& e)
            {
                LOG_ERROR << "Caught a filesystem error: " << e.what();
                throw std::runtime_error("Could not remove directory " + path.string());
            }
        }
        else
        {
            try
            {
                fs::remove(path);
            }
            catch (const fs::filesystem_error& e)
            {
                LOG_ERROR << "Caught a filesystem error: " << e.what();
                throw std::runtime_error("Could not remove file " + path.string());
            }
        }
    }

    std::string unindent(const char* p)
    {
        std::string result;
        if (*p == '\n')
            ++p;
        const char* p_leading = p;
        while (std::isspace(*p) && *p != '\n')
            ++p;
        size_t leading_len = p - p_leading;
        while (*p)
        {
            result += *p;
            if (*p++ == '\n')
            {
                for (size_t i = 0; i < leading_len; ++i)
                    if (p[i] != p_leading[i])
                        goto dont_skip_leading;
                p += leading_len;
            }
        dont_skip_leading:;
        }
        return result;
    }

    std::string prepend(const char* p, const char* start, const char* newline)
    {
        std::string result;

        result += start;
        while (*p)
        {
            result += *p;
            if (*p++ == '\n')
            {
                result += newline;
            }
        }
        return result;
    }

    std::string prepend(const std::string& p, const char* start, const char* newline)
    {
        return prepend(p.c_str(), start, newline);
    }

    int try_lock(int fd, int pid, bool wait)
    {
        // very much inspired by boost file_lock and
        // dnf's https://github.com/rpm-software-management/dnf lock.py#L80
#ifdef _WIN32
        int ret;
        if (!wait)
        {
            ret = _locking(fd, LK_NBLCK, 1 /*lock_file_contents_length()*/);
        }
        else
        {
            ret = _locking(fd, LK_LOCK, 1 /*lock_file_contents_length()*/);
        }
        if (ret == 0)
        {
            return pid;
        }
        else
        {
            return 1;
        }
#else
        struct flock lock;
        lock.l_type = F_WRLCK;
        lock.l_whence = SEEK_SET;
        lock.l_start = 0;
        lock.l_len = 1;

        int ret;

        if (wait)
        {
            ret = fcntl(fd, F_SETLKW, &lock);

            if (ret)
            {
                LOG_ERROR << "Could not lock file: " << strerror(errno);
                return -1;
            }
        }

        char pidc[20] = "";
        int read_res = read(fd, pidc, 20);
        if (read_res == -1 && errno != EBADF)
        {
            LOG_ERROR << "Could not read old PID from lockfile " << strerror(errno);
            return -1;
        }

        if (strlen(pidc) != 0)
        {
            int old_pid;

            try
            {
                old_pid = std::stoi(pidc);
            }
            catch (...)
            {
                LOG_ERROR << "Could not parse PID from lock file.";
                return -1;
            }

            LOG_INFO << "File currently locked by PID: " << old_pid;
            if (old_pid == pid)
            {
                return pid;
            }

            // sending `0` with kill will check if the process is still alive
            if (kill(old_pid, 0) == -1)
            {
                lseek(fd, 0, SEEK_SET);
                ftruncate(fd, 0);
                auto s = std::to_string(pid);
                write(fd, s.c_str(), s.size());
            }
            else
            {
                return old_pid;
            }
        }
        else
        {
            auto s = std::to_string(pid);
            write(fd, s.c_str(), s.size());
        }

        ret = fcntl(fd, F_SETLK, &lock);
        if (ret)
        {
            LOG_ERROR << "Could not lock file: " << strerror(errno);
        }

        return pid;
#endif
    }

    bool unlock(int fd)
    {
#ifdef _WIN32
        int ret = _locking(fd, LK_UNLCK, 1 /*lock_file_contents_length()*/);
#endif
        close(fd);
        return true;
    }

    LockFile::LockFile(const fs::path& path)
        : m_path(path)
    {
        LOG_INFO << "Locking " << path;

        m_fd = open(path.c_str(), O_RDWR | O_CREAT, 0666);

        if (m_fd <= 0)
        {
            LOG_ERROR << "Could not open lockfile " << path;
        }
        else
        {
            m_pid = getpid();
            int ret = try_lock(m_fd, m_pid, false);
            if (ret != m_pid)
            {
                LOG_WARNING << "Cannot lock file " << path
                            << "\nWaiting for other mamba process to finish.\n";
                try_lock(m_fd, m_pid, true);
            }
        }
    }

    LockFile::~LockFile()
    {
        unlock(m_fd);
        fs::remove(m_path);
    }

    std::string timestamp(const std::time_t& utc_time)
    {
        char buf[sizeof("2011-10-08T07:07:09Z")];
        strftime(buf, sizeof(buf), "%FT%TZ", gmtime(&utc_time));
        return buf;
    }

    std::time_t utc_time_now()
    {
        std::time_t now;
        std::time(&now);
        gmtime(&now);
        return now;
    }

    std::string utc_timestamp_now()
    {
        return timestamp(utc_time_now());
    }

    std::time_t parse_utc_timestamp(const std::string& timestamp, int& error_code) noexcept
    {
        error_code = 0;
        struct std::tm tt = { 0 };

        if (sscanf(timestamp.data(),
                   "%04d-%02d-%02dT%02d:%02d:%02dZ",
                   &tt.tm_year,
                   &tt.tm_mon,
                   &tt.tm_mday,
                   &tt.tm_hour,
                   &tt.tm_min,
                   &tt.tm_sec)
            != 6)
        {
            error_code = 1;
            return -1;
        }

        tt.tm_mon -= 1;
        tt.tm_year -= 1900;
        tt.tm_isdst = -1;
        return mktime(&tt);
    }

    std::time_t parse_utc_timestamp(const std::string& timestamp)
    {
        int errc = 0;
        auto res = parse_utc_timestamp(timestamp, errc);
        if (errc != 0)
        {
            LOG_ERROR << "Error , should be '2011-10-08T07:07:09Z' (ISO8601), but is: '"
                      << timestamp << "'";
            throw std::runtime_error("Timestamp format error. Aborting");
        }
        return res;
    }
}  // namespace mamba
