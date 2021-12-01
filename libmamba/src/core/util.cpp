// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.


#if defined(__APPLE__) || defined(__linux__)
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>

#include <csignal>
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

#include <cerrno>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <thread>
#include <condition_variable>

#include <reproc/reproc.h>

#include "mamba/core/context.hpp"
#include "mamba/core/util.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/thread_utils.hpp"


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
            std::ofstream f = open_ofstream(final_path);
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
        if (start == std::string::npos)
        {
            return "";
        }
        size_t stop = input.find_last_not_of(chars) + 1;
        size_t length = stop - start;
        return length == 0 ? "" : input.substr(start, length);
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
#ifdef _WIN32
        std::ifstream in(file_path.wstring(), std::ios::in | mode);
#else
        std::ifstream in(file_path, std::ios::in | mode);
#endif
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

    std::size_t remove_or_rename(const fs::path& path)
    {
        std::error_code ec;
        std::size_t result = 0;
        if (!fs::exists(path, ec))
            return 0;

        if (fs::is_directory(path, ec))
        {
            result = fs::remove_all(path, ec);
        }
        else
        {
            result = fs::remove(path, ec);
        }

        if (ec)
        {
            int counter = 0;
            while (ec)
            {
                LOG_ERROR << "Caught a filesystem error: " << ec.message();
                fs::path new_path = path;
                new_path.replace_extension(new_path.extension().string() + ".mamba_trash");
                fs::rename(path, new_path, ec);
                if (!ec)
                    return 1;
                counter += 1;
                LOG_ERROR << "ERROR " << ec.message() << " sleeping for " << counter * 2;
                if (counter > 5)
                    throw std::runtime_error("Couldnt delete file.");
                std::this_thread::sleep_for(std::chrono::seconds(counter * 2));
            }
        }
        return result;
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

    LockFile::LockFile(const fs::path& path, const std::chrono::seconds& timeout)
        : m_path(path)
        , m_timeout(timeout)
        , m_locked(false)
    {
        std::error_code ec;
        if (!fs::exists(path, ec))
        {
            LOG_ERROR << "Could not lock non-existing path '" << path.string() << "'";
            throw std::runtime_error("LockFile error. Aborting.");
        }

        if (fs::is_directory(path))
        {
            LOG_DEBUG << "Locking directory '" << path.string() << "'";
            m_lock = m_path / (m_path.filename().string() + ".lock");
        }
        else
        {
            LOG_DEBUG << "Locking file '" << path.string() << "'";
            m_lock = m_path.string() + ".lock";
        }

        m_lockfile_existed = fs::exists(m_lock, ec);
        m_fd = open(m_lock.c_str(), O_RDWR | O_CREAT, 0666);

        if (m_fd <= 0)
        {
            LOG_ERROR << "Could not open lockfile '" << m_lock.string() << "'";
            unlock();
            throw std::runtime_error("LockFile error. Aborting.");
        }
        else
        {
            m_pid = getpid();
            if (!(m_locked = lock_non_blocking()))
            {
                LOG_WARNING << "Cannot lock '" << m_path.string() << "'"
                            << "\nWaiting for other mamba process to finish";

                m_locked = lock_blocking();
            }

            if (m_locked)
            {
                LOG_TRACE << "Lockfile created at '" << m_lock.string() << "'";
                LOG_DEBUG << "Successfully locked";
            }
            else
            {
                LOG_ERROR << "LockFile can't be set at '" << m_path.string() << "'\n"
                          << "This could be fixed by changing the locks' timeout or "
                          << "cleaning your environment from previous runs";
                unlock();
                throw std::runtime_error("LockFile error. Aborting.");
            }
        }
    }

    LockFile::LockFile(const fs::path& path)
        : LockFile(path, std::chrono::seconds(Context::instance().lock_timeout))
    {
    }

    LockFile::~LockFile()
    {
        LOG_DEBUG << "Unlocking '" << m_path.string() << "'";
        unlock();
    }

    void LockFile::remove_lockfile() noexcept
    {
        close_fd();

        if (!m_lockfile_existed)
        {
            std::error_code ec;
            LOG_TRACE << "Removing file '" << m_lock.string() << "'";
            fs::remove(m_lock, ec);

            if (ec)
            {
                LOG_ERROR << "Removing lock file '" << m_lock.string() << "' failed\n"
                          << "You may need to remove it manually";
            }
        }
    }

    int LockFile::close_fd()
    {
        int ret = 0;
        if (m_fd > -1)
        {
            ret = close(m_fd);
            m_fd = -1;
        }
        return ret;
    }

    bool LockFile::unlock()
    {
        int ret = 0;

        // POSIX systems automatically remove locks when closing any file
        // descriptor related to the file
#ifdef _WIN32
        LOG_TRACE << "Removing lock on '" << m_lock.string() << "'";
        _lseek(m_fd, MAMBA_LOCK_POS, SEEK_SET);
        ret = _locking(m_fd, LK_UNLCK, 1 /*lock_file_contents_length()*/);
#endif
        remove_lockfile();

        return ret == 0;
    }

    int LockFile::read_pid(int fd)
    {
        char pid_buffer[20] = "";

#ifdef _WIN32
        _lseek(fd, 0, SEEK_SET);
        int read_res = _read(fd, pid_buffer, 20);
#else
        lseek(fd, 0, SEEK_SET);
        int read_res = read(fd, pid_buffer, 20);
#endif
        if (read_res == -1 && errno != EBADF)
        {
            LOG_ERROR << "Could not read lockfile (" << strerror(errno) << ")";
            return -1;
        }

        if (strlen(pid_buffer) == 0)
            return -1;

        int pid;
        try
        {
            pid = std::stoi(pid_buffer);
        }
        catch (...)
        {
            LOG_ERROR << "Could not parse lockfile";
            return -1;
        };

        return pid;
    }

    int LockFile::read_pid() const
    {
        return read_pid(m_fd);
    }

#ifdef _WIN32
    bool LockFile::is_locked(const fs::path& path)
    {
        // Windows locks are isolated between file descriptor
        // We can then test if locked by opening a new one
        int fd = _wopen(path.wstring().c_str(), O_RDWR | O_CREAT, 0666);
        _lseek(fd, MAMBA_LOCK_POS, SEEK_SET);
        char buffer[1];
        bool is_locked = _read(fd, buffer, 1) == -1;
        _close(fd);
        return is_locked;
    }
#endif

#ifndef _WIN32
    bool LockFile::is_locked(int fd)
    {
        // UNIX/POSIX record locks can't be checked from current process: opening
        // then closing a new file descriptor would unset the locks
        // 1. compare owner PID written in lockfile with current PID
        // 2. call fcntl called with F_GETLK
        //  -> log an error if fcntl return a different owner PID vs lockfile content

        // Warning:
        // If called from the same process as the lockfile one and PID written in
        // file is corrupted, the result is a false negative

        // Note: don't use on Windows
        // On Windows, if called from the same process, with the lockfile file descriptor
        // and PID written in lockfile is corrupted, the result would be a false negative

        int pid = read_pid(fd);
        if (pid == getpid())
            return true;

        struct flock lock;
        lock.l_type = F_WRLCK;
        lock.l_whence = SEEK_SET;
        lock.l_start = MAMBA_LOCK_POS;
        lock.l_len = 1;
        fcntl(fd, F_GETLK, &lock);

        if ((lock.l_type == F_UNLCK) && (pid != lock.l_pid))
            LOG_ERROR << "LockFile file has wrong owner PID " << pid << ", actual is "
                      << lock.l_pid;

        return lock.l_type != F_UNLCK;
    }
#endif

    bool LockFile::write_pid(int pid) const
    {
        auto pid_s = std::to_string(pid);
#ifdef _WIN32
        _lseek(m_fd, 0, SEEK_SET);
        _chsize_s(m_fd, 0);
        return _write(m_fd, pid_s.c_str(), pid_s.size()) > -1;
#else
        lseek(m_fd, 0, SEEK_SET);
        ftruncate(m_fd, 0);
        return write(m_fd, pid_s.c_str(), pid_s.size()) > -1;
#endif
    }

#ifndef _WIN32
    int timedout_set_fd_lock(int fd, struct flock& lock, const std::chrono::seconds& timeout)
    {
        int ret;
        std::mutex m;
        std::condition_variable cv;

        thread t([&cv, &ret, &fd, &lock]() {
            ret = fcntl(fd, F_SETLKW, &lock);
            cv.notify_one();
        });

        auto th = t.native_handle();

        int err = 0;
        set_signal_handler([&th, &cv, &ret, &err](sigset_t sigset) -> int {
            int signum = 0;
            sigwait(&sigset, &signum);
            pthread_cancel(th);
            err = EINTR;
            ret = -1;
            cv.notify_one();
            return signum;
        });

        t.detach();

        {
            std::unique_lock<std::mutex> l(m);
            if (cv.wait_for(l, timeout) == std::cv_status::timeout)
            {
                pthread_cancel(th);
                kill_receiver_thread();
                err = EINTR;
                ret = -1;
            }
        }
        set_default_signal_handler();
        errno = err;

        return ret;
    }
#endif

    bool LockFile::set_fd_lock(bool blocking) const
    {
        int ret;
#ifdef _WIN32
        _lseek(m_fd, MAMBA_LOCK_POS, SEEK_SET);

        if (blocking)
        {
            std::chrono::seconds timer(0);
            bool has_timeout = m_timeout.count() > 0;

            while (!has_timeout || (timer < m_timeout))
            {
                ret = _locking(m_fd, LK_NBLCK, 1 /*lock_file_contents_length()*/);
                if (ret == 0)
                    break;
                std::this_thread::sleep_for(std::chrono::seconds(1));
                timer += std::chrono::seconds(1);
            }

            if (has_timeout && (timer >= m_timeout) && (ret == -1))
                errno = EINTR;
        }
        else
            ret = _locking(m_fd, LK_NBLCK, 1 /*lock_file_contents_length()*/);
#else
        struct flock lock;
        lock.l_type = F_WRLCK;
        lock.l_whence = SEEK_SET;
        lock.l_start = MAMBA_LOCK_POS;
        lock.l_len = 1;

        if (blocking)
        {
            if (m_timeout.count())
                ret = timedout_set_fd_lock(m_fd, lock, m_timeout);
            else
                ret = fcntl(m_fd, F_SETLKW, &lock);
        }
        else
            ret = fcntl(m_fd, F_SETLK, &lock);
#endif
        return ret == 0;
    }

    bool LockFile::lock(int pid, bool blocking) const
    {
        if (!set_fd_lock(blocking))
        {
            LOG_ERROR << "Could not set lock (" << strerror(errno) << ")";
            return false;
        }

        if (!write_pid(pid))
        {
            LOG_ERROR << "Could not write PID to lockfile (" << strerror(errno) << ")";
            return false;
        }

        return true;
    }

    int LockFile::fd() const
    {
        return m_fd;
    }

    bool LockFile::lock_blocking()
    {
        return lock(m_pid, true);
    }

    bool LockFile::lock_non_blocking()
    {
        int old_pid = read_pid();
        if (old_pid > 0)
        {
            if (old_pid == m_pid)
            {
                LOG_ERROR << "Path already locked by the same PID: '" << m_path.string() << "'";
                unlock();
                throw std::logic_error("LockFile error.");
            }

            LOG_TRACE << "File currently locked by PID " << old_pid;
#ifdef _WIN32
            return false;
#else
            // sending `0` with kill will check if the process is still alive
            if (kill(old_pid, 0) != -1)
                return false;
            else
            {
                LOG_TRACE << "Removing dangling lock file '" << m_lock << "'";
                m_lockfile_existed = false;
            }
#endif
        }

        return lock(m_pid, false);
    }

    fs::path LockFile::path() const
    {
        return m_path;
    }

    fs::path LockFile::lockfile_path() const
    {
        return m_lock;
    }

    std::unique_ptr<LockFile> LockFile::try_lock(const fs::path& path) noexcept
    {
        try
        {
            auto ptr = std::make_unique<LockFile>(path);
            return ptr;
        }
        catch (...)
        {
            LOG_WARNING << "LockFile creation for path '" << path.string()
                        << "' failed, continuing without it";
        }
        return nullptr;
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

    bool reproc_killed(int status)
    {
        return status == REPROC_SIGKILL;
    }

    bool reproc_terminated(int status)
    {
        return status == REPROC_SIGTERM;
    }

    void assert_reproc_success(const reproc::options& options, int status, std::error_code ec)
    {
        bool killed_not_an_err = (options.stop.first.action == reproc::stop::kill)
                                 || (options.stop.second.action == reproc::stop::kill)
                                 || (options.stop.third.action == reproc::stop::kill);

        bool terminated_not_an_err = (options.stop.first.action == reproc::stop::terminate)
                                     || (options.stop.second.action == reproc::stop::terminate)
                                     || (options.stop.third.action == reproc::stop::terminate);

        if (ec || (!killed_not_an_err && reproc_killed(status))
            || (!terminated_not_an_err && reproc_terminated(status)))
        {
            if (ec)
                LOG_ERROR << "Subprocess call failed: " << ec.message();
            else if (reproc_killed(status))
                LOG_ERROR << "Subprocess call failed (killed)";
            else
                LOG_ERROR << "Subprocess call failed (terminated)";
            throw std::runtime_error("Subprocess call failed. Aborting.");
        }
    }
}  // namespace mamba
