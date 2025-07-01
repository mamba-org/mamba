// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <cwchar>
#include <fstream>
#include <iomanip>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

#include <time.h>

#if defined(__PPC64__) || defined(__ppc64__) || defined(_ARCH_PPC64)
#include <iomanip>
#endif

#if defined(__APPLE__) || defined(__linux__)
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#endif

#ifdef _WIN32
#include <cassert>

#include <io.h>

extern "C"
{
#include <fcntl.h>
#include <io.h>
#include <process.h>
#include <sys/locking.h>
}

#include "mamba/core/shell_init.hpp"
#endif

#include <nlohmann/json.hpp>
#include <tl/expected.hpp>

#include "mamba/core/error_handling.hpp"
#include "mamba/core/execution.hpp"
#include "mamba/core/invoke.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/thread_utils.hpp"
#include "mamba/core/util.hpp"
#include "mamba/core/util_os.hpp"
#include "mamba/fs/filesystem.hpp"
#include "mamba/util/build.hpp"
#include "mamba/util/environment.hpp"
#include "mamba/util/random.hpp"
#include "mamba/util/string.hpp"
#include "mamba/util/url.hpp"

namespace mamba
{
    namespace
    {
        std::atomic<bool> persist_temporary_files{ false };
        std::atomic<bool> persist_temporary_directories{ false };
    }

    const std::regex& token_regex()
    {
        // usernames on anaconda.org can have a underscore, which influences the
        // first two characters
        static const std::regex token_regex{ "/t/([a-zA-Z0-9-_]{0,2}[a-zA-Z0-9-]*)" };
        return token_regex;
    }

    const std::regex& http_basicauth_regex()
    {
        static const std::regex http_basicauth_regex{ "(://|^)([^\\s]+):([^\\s]+)@" };
        return http_basicauth_regex;
    }

    std::string expandvars(std::string s)
    {
        if (s.find("$") == std::string::npos)
        {
            // Bail out early
            return s;
        }
        std::regex env_var_re(R"(\$(\{\w+\}|\w+))");
        for (auto matches = std::sregex_iterator(s.begin(), s.end(), env_var_re);
             matches != std::sregex_iterator();
             ++matches)
        {
            std::smatch match = *matches;
            auto var = match[0].str();
            if (util::starts_with(var, "${"))
            {
                // strip ${ and }
                var = var.substr(2, var.size() - 3);
            }
            else
            {
                // strip $
                var = var.substr(1);
            }
            auto val = util::get_env(var);
            if (val)
            {
                s.replace(match[0].first, match[0].second, val.value());
                // It turns out to be unsafe to modify the string during
                // sregex_iterator iteration. Start a new search by recursing.
                return expandvars(s);
            }
        }
        return s;
    }

    bool must_persist_temporary_files()
    {
        return persist_temporary_files;
    }

    bool set_persist_temporary_files(bool new_value)
    {
        return persist_temporary_files.exchange(new_value);
    }

    bool must_persist_temporary_directories()
    {
        return persist_temporary_directories;
    }

    bool set_persist_temporary_directories(bool new_value)
    {
        return persist_temporary_directories.exchange(new_value);
    }

    // This function returns true even for broken symlinks
    // E.g.
    // ln -s abcdef emptylink
    // fs::exists(emptylink) == false
    // lexists(emptylink) == true
    bool lexists(const fs::u8path& path, std::error_code& ec)
    {
        auto status = fs::symlink_status(path, ec).type();
        if (status != fs::file_type::none)
        {
            ec.clear();
            return status != fs::file_type::not_found || status == fs::file_type::symlink;
        }
        return false;
    }

    bool lexists(const fs::u8path& path)
    {
        auto status = fs::symlink_status(path);
        return status.type() != fs::file_type::not_found || status.type() == fs::file_type::symlink;
    }

    std::vector<fs::u8path> filter_dir(const fs::u8path& dir, const std::string& suffix)
    {
        std::vector<fs::u8path> result;
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
    bool paths_equal(const fs::u8path& lhs, const fs::u8path& rhs)
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
        const std::string template_path = (fs::temp_directory_path() / "mambadXXXXXX").string();
        // include \0 terminator
        auto err [[maybe_unused]] = _mktemp_s(
            const_cast<char*>(template_path.c_str()),
            template_path.size() + 1
        );
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
        if (!must_persist_temporary_directories())
        {
            fs::remove_all(m_path);
        }
    }

    const fs::u8path& TemporaryDirectory::path() const
    {
        return m_path;
    }

    TemporaryDirectory::operator fs::u8path()
    {
        return m_path;
    }

    TemporaryFile::TemporaryFile(
        const std::string& prefix,
        const std::string& suffix,
        const std::optional<fs::u8path>& dir
    )
    {
        static std::mutex file_creation_mutex;

        bool success = false;
        fs::u8path final_path;
        fs::u8path temp_path = dir.value_or(fs::temp_directory_path());

        std::lock_guard<std::mutex> file_creation_lock(file_creation_mutex);

        do
        {
            std::string random_file_name = util::generate_random_alphanumeric_string(10);
            final_path = temp_path / util::concat(prefix, random_file_name, suffix);
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
        if (!must_persist_temporary_files())
        {
            fs::remove(m_path);
        }
    }

    fs::u8path& TemporaryFile::path()
    {
        return m_path;
    }

    TemporaryFile::operator fs::u8path()
    {
        return m_path;
    }

    std::string read_contents(const fs::u8path& file_path, std::ios::openmode mode)
    {
        std::ifstream in(file_path.std_path(), std::ios::in | mode);

        if (in)
        {
            std::string contents;
            in.seekg(0, std::ios::end);
            contents.resize(static_cast<std::size_t>(in.tellg()));
            in.seekg(0, std::ios::beg);
            in.read(&contents[0], static_cast<std::streamsize>(contents.size()));
            in.close();
            return (contents);
        }
        else
        {
            throw std::system_error(
                errno,
                std::system_category(),
                "failed to open " + file_path.string()
            );
        }
    }

    std::vector<std::string> read_lines(const fs::u8path& file_path)
    {
        std::fstream file_stream(file_path.std_path(), std::ios_base::in | std::ios_base::binary);
        if (file_stream.fail())
        {
            throw std::system_error(
                errno,
                std::system_category(),
                "failed to open " + file_path.string()
            );
        }

        std::vector<std::string> output;
        std::string line;
        while (std::getline(file_stream, line))
        {
            // Remove the trailing \r to accommodate Windows line endings.
            if ((!line.empty()) && (line.back() == '\r'))
            {
                line.pop_back();
            }

            // Remove leading and trailing whitespace in place not to create a new string.
            util::inplace_strip(line);

            // Skipping empty lines
            if (line.empty())
            {
                continue;
            }

            // Skipping comment lines starting with #
            if (util::starts_with(line, "#"))
            {
                continue;
            }

            // Skipping comment lines starting with @ BUT headers of explicit environment specs
            if (util::starts_with(line, "@"))
            {
                auto is_explicit_header = util::starts_with(line, "@EXPLICIT");

                if (is_explicit_header)
                {
                    output.push_back(line);
                }
                continue;
            }

            // By default, add the line to the output (MatchSpecs, etc.)
            output.push_back(line);
        }
        file_stream.close();

        return output;
    }

    void split_package_extension(const std::string& file, std::string& name, std::string& extension)
    {
        if (util::ends_with(file, ".conda"))
        {
            name = file.substr(0, file.size() - 6);
            extension = ".conda";
        }
        else if (util::ends_with(file, ".tar.bz2"))
        {
            name = file.substr(0, file.size() - 8);
            extension = ".tar.bz2";
        }
        else if (util::ends_with(file, ".json"))
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

    std::string quote_for_shell(const std::vector<std::string>& arguments, const std::string& shell)
    {
        if ((shell.empty() && util::on_win) || shell == "cmdexe")
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
                    // separate arguments
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
            // Identical to Python's shlex.quote.
            auto quote_arg = [](const std::string& s)
            {
                if (s.size() == 0)
                {
                    return std::string("''");
                }
                std::regex unsafe("[^\\w@%+=:,./-]");
                if (std::regex_search(s, unsafe))
                {
                    std::string s2 = s;
                    util::replace_all(s2, "'", "'\"'\"'");
                    return util::concat("'", s2, "'");
                }
                else
                {
                    return s;
                }
            };

            if (arguments.empty())
            {
                return "";
            }

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

    std::size_t clean_trash_files(const fs::u8path& prefix, bool deep_clean)
    {
        std::size_t deleted_files = 0;
        std::size_t remaining_trash = 0;
        std::error_code ec;
        std::vector<fs::u8path> remaining_files;
        auto trash_txt = prefix / "conda-meta" / "mamba_trash.txt";
        if (!deep_clean && fs::exists(trash_txt))
        {
            auto all_files = read_lines(trash_txt);
            for (auto& f : all_files)
            {
                fs::u8path full_path = prefix / f;
                LOG_INFO << "Trash: removing " << full_path;
                if (!fs::exists(full_path) || fs::remove(full_path, ec))
                {
                    deleted_files += 1;
                }
                else
                {
                    LOG_INFO << "Trash: could not remove " << full_path;
                    remaining_trash += 1;
                    // save relative path
                    remaining_files.push_back(f);
                }
            }
        }

        if (deep_clean)
        {
            // recursive iterate over all files and delete `.mamba_trash` files
            std::vector<fs::u8path> f_to_rm;
            for (auto& p : fs::recursive_directory_iterator(prefix))
            {
                if (p.path().extension() == ".mamba_trash")
                {
                    f_to_rm.push_back(p.path());
                }
            }
            for (auto& p : f_to_rm)
            {
                LOG_INFO << "Trash: removing " << p;
                if (fs::remove(p, ec))
                {
                    deleted_files += 1;
                }
                else
                {
                    remaining_trash += 1;
                    // save relative path
                    remaining_files.push_back(fs::relative(p, prefix));
                }
            }
        }

        if (remaining_files.empty())
        {
            fs::remove(trash_txt, ec);
        }
        else
        {
            auto trash_out_file = open_ofstream(
                trash_txt,
                std::ios::out | std::ios::binary | std::ios::trunc
            );
            for (auto& rf : remaining_files)
            {
                trash_out_file << rf.string() << "\n";
            }
        }

        LOG_INFO << "Cleaned " << deleted_files << " .mamba_trash files. " << remaining_trash
                 << " remaining.";
        return deleted_files;
    }

    std::size_t remove_or_rename(const fs::u8path& target_prefix, const fs::u8path& path)
    {
        std::error_code ec;
        std::size_t result = 0;
        if (!lexists(path, ec))
        {
            return 0;
        }

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

            // we should only attempt writing to the trash index file from one thread at a time
            static std::mutex trash_mutex;
            std::lock_guard<std::mutex> guard(trash_mutex);

            while (ec)
            {
                LOG_INFO << "Caught a filesystem error for '" << path.string()
                         << "':" << ec.message() << " (File in use?)";
                fs::u8path trash_file = path;
                std::size_t fcounter = 0;

                trash_file.replace_extension(
                    util::concat(trash_file.extension().string(), ".mamba_trash")
                );
                while (lexists(trash_file))
                {
                    trash_file = path;
                    trash_file.replace_extension(util::concat(
                        trash_file.extension().string(),
                        std::to_string(fcounter),
                        ".mamba_trash"
                    ));
                    fcounter += 1;
                    if (fcounter > 100)
                    {
                        throw std::runtime_error("Too many existing trash files. Please force clean");
                    }
                }
                fs::rename(path, trash_file, ec);
                if (!ec)
                {
                    // The conda-meta directory is locked by transaction execute
                    auto trash_index = open_ofstream(
                        target_prefix / "conda-meta" / "mamba_trash.txt",
                        std::ios::app | std::ios::binary
                    );

                    // TODO add some unicode tests here?
                    trash_index << fs::relative(trash_file, target_prefix).string() << "\n";
                    return 1;
                }

                // this is some exponential back off
                counter += 1;
                LOG_ERROR << "Trying to remove " << path << ": " << ec.message()
                          << " (file in use?). Sleeping for " << counter * 2 << "s";
                if (counter > 3)
                {
                    throw std::runtime_error(util::concat("Could not delete file ", path.string()));
                }
                std::this_thread::sleep_for(std::chrono::seconds(counter * 2));
            }
        }
        return result;
    }

    std::string unindent(const char* p)
    {
        std::string result;
        if (*p == '\n')
        {
            ++p;
        }
        const char* p_leading = p;
        while (std::isspace(*p) && *p != '\n')
        {
            ++p;
        }
        std::size_t leading_len = static_cast<std::size_t>(p - p_leading);
        while (*p)
        {
            result += *p;
            if (*p++ == '\n')
            {
                for (std::size_t i = 0; i < leading_len; ++i)
                {
                    if (p[i] != p_leading[i])
                    {
                        goto dont_skip_leading;
                    }
                }
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

    class LockFileOwner
    {
    public:

        explicit LockFileOwner(const fs::u8path& file_path, const std::chrono::seconds timeout);
        ~LockFileOwner();

        LockFileOwner(const LockFileOwner&) = delete;
        LockFileOwner& operator=(const LockFileOwner&) = delete;
        LockFileOwner(LockFileOwner&&) = delete;
        LockFileOwner& operator=(LockFileOwner&&) = delete;

        bool set_fd_lock(bool blocking) const;
        bool lock_non_blocking();
        bool lock_blocking();
        bool lock(bool blocking) const;

        void remove_lockfile() noexcept;
        int close_fd();
        bool unlock();

        int fd() const
        {
            return m_fd;
        }

        fs::u8path path() const
        {
            return m_path;
        }

        fs::u8path lockfile_path() const
        {
            return m_lockfile_path;
        }

    private:

        fs::u8path m_path;
        fs::u8path m_lockfile_path;
        std::chrono::seconds m_timeout;
        int m_fd = -1;
        bool m_locked;
        bool m_lockfile_existed;

        template <typename Func = no_op>
        void throw_lock_error(std::string error_message, Func before_throw_task = no_op{}) const
        {
            auto complete_error_message = fmt::format(
                "LockFile acquisition failed, aborting: {}",
                std::move(error_message)
            );
            LOG_ERROR << error_message;
            safe_invoke(before_throw_task)
                .map_error([](const auto& error)
                           { LOG_ERROR << "While handling LockFile failure: " << error.what(); });
            throw mamba_error(complete_error_message, mamba_error_code::lockfile_failure);
        }
    };

    LockFileOwner::LockFileOwner(const fs::u8path& path, const std::chrono::seconds timeout)
        : m_path(path)
        , m_timeout(timeout)
        , m_locked(false)
    {
        std::error_code ec;

        // Check if `path` exists
        if (!fs::exists(path, ec))
        {
            // If `path` doesn't exist, consider creating the directory
            // (and its parents if they don't exist)
            if (!fs::create_directories(path, ec))
            {
                throw_lock_error(
                    fmt::format("Could not create directory '{}': {}", path.string(), ec.message())
                );
            }
        }

        if (fs::is_directory(path))
        {
            LOG_DEBUG << "Locking directory '" << path.string() << "'";
            m_lockfile_path = m_path / (m_path.filename().string() + ".lock");
        }
        else
        {
            LOG_DEBUG << "Locking file '" << path.string() << "'";
            m_lockfile_path = m_path.string() + ".lock";
        }

        m_lockfile_existed = fs::exists(m_lockfile_path, ec);
#ifdef _WIN32
        m_fd = _wopen(m_lockfile_path.wstring().c_str(), O_RDWR | O_CREAT, 0666);
#else
        m_fd = open(m_lockfile_path.string().c_str(), O_RDWR | O_CREAT, 0666);
#endif
        if (m_fd <= 0)
        {
            throw_lock_error(
                fmt::format("Could not open lockfile '{}'", m_lockfile_path.string()),
                [this] { unlock(); }
            );
        }
        else
        {
            if ((m_locked = lock_non_blocking()) == false)
            {
                LOG_WARNING << "Cannot lock '" << m_path.string() << "'"
                            << "\nWaiting for other mamba process to finish";

                m_locked = lock_blocking();
            }

            if (m_locked)
            {
                LOG_TRACE << "Lockfile created at '" << m_lockfile_path.string() << "'";
                LOG_DEBUG << "Successfully locked";
            }
            else
            {
                throw_lock_error(
                    fmt::format(
                        "LockFile can't be set at '{}'\n"
                        "This could be fixed by changing the locks' timeout or "
                        "cleaning your environment from previous runs",
                        m_path.string()
                    ),
                    [this] { unlock(); }
                );
            }
        }
    }

    LockFileOwner::~LockFileOwner()
    {
        LOG_DEBUG << "Unlocking '" << m_path.string() << "'";
        unlock();
    }

    void LockFileOwner::remove_lockfile() noexcept
    {
        close_fd();

        if (!m_lockfile_existed)
        {
            std::error_code ec;
            LOG_TRACE << "Removing file '" << m_lockfile_path.string() << "'";
            fs::remove(m_lockfile_path, ec);

            if (ec)
            {
                LOG_ERROR << "Removing lock file '" << m_lockfile_path.string() << "' failed\n"
                          << "You may need to remove it manually";
            }
        }
    }

    int LockFileOwner::close_fd()
    {
        int ret = 0;
        if (m_fd > -1)
        {
            ret = close(m_fd);
            m_fd = -1;
        }
        return ret;
    }

    bool LockFileOwner::unlock()
    {
        int ret = 0;

        // POSIX systems automatically remove locks when closing any file
        // descriptor related to the file
#ifdef _WIN32
        LOG_TRACE << "Removing lock on '" << m_lockfile_path.string() << "'";
        _lseek(m_fd, MAMBA_LOCK_POS, SEEK_SET);
        ret = _locking(m_fd, LK_UNLCK, 1 /*lock_file_contents_length()*/);
#endif
        remove_lockfile();
        return ret == 0;
    }

#ifndef _WIN32
    int timedout_set_fd_lock(int fd, struct flock& lock, const std::chrono::seconds timeout)
    {
        int ret;
        std::mutex m;
        std::condition_variable cv;

        thread t(
            [&cv, &ret, &fd, &lock]()
            {
                ret = fcntl(fd, F_SETLKW, &lock);
                cv.notify_one();
            }
        );

        auto th = t.native_handle();

        int err = 0;
        set_signal_handler(
            [&th, &cv, &ret, &err](sigset_t sigset) -> int
            {
                int signum = 0;
                sigwait(&sigset, &signum);
                pthread_cancel(th);
                err = EINTR;
                ret = -1;
                cv.notify_one();
                return signum;
            }
        );

        MainExecutor::instance().take_ownership(t.extract());

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

    bool LockFileOwner::set_fd_lock(bool blocking) const
    {
        int ret = 0;
#ifdef _WIN32
        _lseek(m_fd, MAMBA_LOCK_POS, SEEK_SET);

        if (blocking)
        {
            static constexpr auto default_timeout = std::chrono::seconds(30);
            const auto timeout = m_timeout > std::chrono::seconds::zero() ? m_timeout
                                                                          : default_timeout;
            const auto begin_time = std::chrono::system_clock::now();
            while ((std::chrono::system_clock::now() - begin_time) < timeout)
            {
                ret = _locking(m_fd, LK_NBLCK, 1 /*lock_file_contents_length()*/);
                if (ret == 0)
                {
                    break;
                }
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }

            if (ret != 0)
            {
                errno = EINTR;
            }
        }
        else
        {
            ret = _locking(m_fd, LK_NBLCK, 1 /*lock_file_contents_length()*/);
        }
#else
        struct flock lock;
        lock.l_type = F_WRLCK;
        lock.l_whence = SEEK_SET;
        lock.l_start = MAMBA_LOCK_POS;
        lock.l_len = 1;

        if (blocking)
        {
            if (m_timeout.count())
            {
                ret = timedout_set_fd_lock(m_fd, lock, m_timeout);
            }
            else
            {
                ret = fcntl(m_fd, F_SETLKW, &lock);
            }
        }
        else
        {
            ret = fcntl(m_fd, F_SETLK, &lock);
        }
#endif
        return ret == 0;
    }

    bool LockFileOwner::lock(bool blocking) const
    {
        if (!set_fd_lock(blocking))
        {
            LOG_ERROR << "Could not set lock (" << strerror(errno) << ")";
            return false;
        }
        return true;
    }

    bool LockFileOwner::lock_blocking()
    {
        return lock(true);
    }

    namespace
    {

        void log_duplicate_lockfile_in_process(const fs::u8path& path)
        {
            LOG_DEBUG << "Path already locked by the same process: '" << fs::absolute(path).string()
                      << "'";
        }

// This function is only used in `assert()` expressions
// That's why it might get reported as unused in Release builds
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif

        bool is_lockfile_locked(const LockFileOwner& lockfile)
        {
#ifdef _WIN32
            return LockFile::is_locked(lockfile.lockfile_path());
#else
            // Opening a new file descriptor on Unix would clear locks
            return LockFile::is_locked(lockfile.fd());
#endif
        }

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif



        class LockedFilesRegistry
        {
        public:

            LockedFilesRegistry() = default;
            LockedFilesRegistry(LockedFilesRegistry&&) = delete;
            LockedFilesRegistry(const LockedFilesRegistry&) = delete;
            LockedFilesRegistry& operator=(LockedFilesRegistry&&) = delete;
            LockedFilesRegistry& operator=(const LockedFilesRegistry&) = delete;

            bool is_file_locking_allowed() const
            {
                return m_is_file_locking_allowed;
            }

            bool allow_file_locking(bool allow)
            {
                return m_is_file_locking_allowed.exchange(allow);
            }

            std::chrono::seconds default_file_locking_timeout() const
            {
                return m_default_lock_timeout;
            }

            std::chrono::seconds set_file_locking_timeout(const std::chrono::seconds& new_timeout)
            {
                return m_default_lock_timeout.exchange(new_timeout);
            }

            tl::expected<std::shared_ptr<LockFileOwner>, mamba_error>
            acquire_lock(const fs::u8path& file_path, const std::chrono::seconds timeout)
            {
                if (!m_is_file_locking_allowed)
                {
                    // No locking allowed, so do nothing.
                    return std::shared_ptr<LockFileOwner>{};
                }

                const auto absolute_file_path = fs::absolute(file_path);
                auto data = m_data.synchronize();

                const auto it = data->locked_files.find(absolute_file_path);
                if (it != data->locked_files.end())
                {
                    if (auto lockedfile = it->second.lock())
                    {
                        log_duplicate_lockfile_in_process(absolute_file_path);
                        return lockedfile;
                    }
                }

                // At this point, we didn't find a lockfile alive, so we create one.
                return safe_invoke(
                    [&]
                    {
                        auto lockedfile = std::make_shared<LockFileOwner>(absolute_file_path, timeout);
                        auto tracker = std::weak_ptr{ lockedfile };
                        data->locked_files.insert_or_assign(absolute_file_path, std::move(tracker));
                        data->fd_to_locked_path.insert_or_assign(lockedfile->fd(), absolute_file_path);
                        assert(is_lockfile_locked(*lockedfile));
                        return lockedfile;
                    }
                );
            }

            // note: the resulting value will be obsolete before returning.
            bool is_locked(const fs::u8path& file_path) const
            {
                const auto absolute_file_path = fs::absolute(file_path);
                auto data = m_data.synchronize();
                auto it = data->locked_files.find(file_path);
                if (it != data->locked_files.end())
                {
                    return !it->second.expired();
                }
                else
                {
                    return false;
                }
            }

            // note: the resulting value will be obsolete before returning.
            bool is_locked(int fd) const
            {
                auto data = m_data.synchronize();
                const auto it = data->fd_to_locked_path.find(fd);
                if (it != data->fd_to_locked_path.end())
                {
                    return is_locked(it->second);
                }
                else
                {
                    return false;
                }
            }

        private:

            std::atomic_bool m_is_file_locking_allowed{ true };
            std::atomic<std::chrono::seconds> m_default_lock_timeout{ std::chrono::seconds::zero() };

            struct Data
            {
                Data()
                {
                    // this user-defined constructor is a workaround for CWG2335 and related compiler issues
                }

                // TODO: replace by something like boost::multiindex or equivalent to avoid having
                // to handle 2 hashmaps
                std::unordered_map<fs::u8path, std::weak_ptr<LockFileOwner>> locked_files;  // TODO:
                                                                                            // consider
                                                                                            // replacing
                                                                                            // by
                                                                                            // real
                                                                                            // concurrent
                                                                                            // set
                                                                                            // to
                                                                                            // avoid
                                                                                            // having
                                                                                            // to
                                                                                            // lock
                                                                                            // the
                                                                                            // whole
                                                                                            // container

                std::unordered_map<int, fs::u8path> fd_to_locked_path;  // this is a workaround the
                                                                        // usage of file descriptors
                                                                        // on linux instead of paths
            };
            util::synchronized_value<Data, std::recursive_mutex> m_data;
        };

        static LockedFilesRegistry files_locked_by_this_process;
    }

    bool is_file_locking_allowed()
    {
        return files_locked_by_this_process.is_file_locking_allowed();
    }

    bool allow_file_locking(bool allow)
    {
        return files_locked_by_this_process.allow_file_locking(allow);
    }

    std::chrono::seconds default_file_locking_timeout()
    {
        return files_locked_by_this_process.default_file_locking_timeout();
    }

    std::chrono::seconds set_file_locking_timeout(const std::chrono::seconds& new_timeout)
    {
        return files_locked_by_this_process.set_file_locking_timeout(new_timeout);
    }

    bool LockFileOwner::lock_non_blocking()
    {
        if (files_locked_by_this_process.is_locked(m_lockfile_path))
        {
            log_duplicate_lockfile_in_process(m_lockfile_path);
            return true;
        }
        return lock(false);
    }

    LockFile::~LockFile() = default;
    LockFile::LockFile(LockFile&&) = default;
    LockFile& LockFile::operator=(LockFile&&) = default;

    LockFile::LockFile(const fs::u8path& path, const std::chrono::seconds& timeout)
        : impl{ files_locked_by_this_process.acquire_lock(path, timeout) }
    {
    }

    LockFile::LockFile(const fs::u8path& path)
        : LockFile(path, files_locked_by_this_process.default_file_locking_timeout())
    {
    }

    int LockFile::fd() const
    {
        return impl.value()->fd();
    }

    fs::u8path LockFile::path() const
    {
        return impl.value()->path();
    }

    fs::u8path LockFile::lockfile_path() const
    {
        return impl.value()->lockfile_path();
    }

#ifdef _WIN32
    bool LockFile::is_locked(const fs::u8path& path)
    {
        // Windows locks are isolated between file descriptor
        // We can then test if locked by opening a new one
        int fd = _wopen(path.wstring().c_str(), O_RDWR | O_CREAT, 0666);
        if (fd == -1)
        {
            if (errno == EACCES)
            {
                return true;
            }

            // In other cases, something is wrong.
            throw mamba_error{ fmt::format("failed to check if path is locked : '{}'", path.string()),
                               mamba_error_code::lockfile_failure };
        }
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

        // Here we replaced the pid check by tracking internally if we did or not lock
        // the file.
        if (files_locked_by_this_process.is_locked(fd))
        {
            return true;
        }

        const auto this_process_pid = getpid();

        struct flock lock;
        lock.l_type = F_WRLCK;
        lock.l_whence = SEEK_SET;
        lock.l_start = MAMBA_LOCK_POS;
        lock.l_len = 1;
        auto result = fcntl(fd, F_GETLK, &lock);

        if ((lock.l_type == F_UNLCK) && (this_process_pid != lock.l_pid))
        {
            LOG_ERROR << "LockFile file has wrong owner PID " << this_process_pid << ", actual is "
                      << lock.l_pid;
        }

        return lock.l_type != F_UNLCK && result != -1;
    }
#endif


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
        std::tm tt = {};

        if (sscanf(
                timestamp.data(),
                "%04d-%02d-%02dT%02d:%02d:%02dZ",
                &tt.tm_year,
                &tt.tm_mon,
                &tt.tm_mday,
                &tt.tm_hour,
                &tt.tm_min,
                &tt.tm_sec
            )
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

    bool ensure_comspec_set()
    {
        std::string cmd_exe = util::get_env("COMSPEC").value_or("");
        if (!util::ends_with(util::to_lower(cmd_exe), "cmd.exe"))
        {
            cmd_exe = (fs::u8path(util::get_env("SystemRoot").value_or("")) / "System32" / "cmd.exe")
                          .string();
            if (!fs::is_regular_file(cmd_exe))
            {
                cmd_exe = (fs::u8path(util::get_env("windir").value_or("")) / "System32" / "cmd.exe")
                              .string();
            }
            if (!fs::is_regular_file(cmd_exe))
            {
                LOG_WARNING << "cmd.exe could not be found. Looked in SystemRoot and "
                               "windir env vars.";
            }
            else
            {
                util::set_env("COMSPEC", cmd_exe);
            }
        }
        return true;
    }

    std::ofstream open_ofstream(const fs::u8path& path, std::ios::openmode mode)
    {
        std::ofstream outfile(path.std_path(), mode);

        if (!outfile.good())
        {
            LOG_ERROR << "Error opening for writing " << path << ": " << strerror(errno);
        }

        return outfile;
    }

    std::ifstream open_ifstream(const fs::u8path& path, std::ios::openmode mode)
    {
        std::ifstream infile(path.std_path(), mode);
        if (!infile.good())
        {
            LOG_ERROR << "Error opening for reading " << path << ": " << strerror(errno);
        }

        return infile;
    }

    namespace
    {
        constexpr bool debug_wrapper_scripts = false;
    }

    std::unique_ptr<TemporaryFile> wrap_call(
        const fs::u8path& root_prefix,
        const fs::u8path& prefix,
        const std::vector<std::string>& arguments,
        bool is_mamba_exe
    )
    {
        // todo add abspath here
        fs::u8path tmp_prefix = prefix / ".tmp";

#ifdef _WIN32
        ensure_comspec_set();
        std::string conda_bat;

        // TODO
        std::string CONDA_PACKAGE_ROOT = "";

        std::string bat_name = get_self_exe_path().stem().string();

        conda_bat = util::get_env("CONDA_BAT")
                        .value_or((fs::absolute(root_prefix) / "condabin" / bat_name).string());
        if (!fs::exists(conda_bat) && is_mamba_exe)
        {
            // this adds in the needed .bat files for activation
            init_root_prefix_cmdexe(root_prefix);
        }

        auto tf = std::make_unique<TemporaryFile>("mamba_bat_", ".bat");

        std::ofstream out = open_ofstream(tf->path());

        std::string silencer = debug_wrapper_scripts ? "" : "@";

        out << silencer << "ECHO OFF\n";
        out << silencer << "SET PYTHONIOENCODING=utf-8\n";
        out << silencer << "SET PYTHONUTF8=1\n";
        out << silencer
            << "FOR /F \"tokens=2 delims=:.\" %%A in (\'chcp\') do for %%B in (%%A) "
               "do set \"_CONDA_OLD_CHCP=%%B\"\n";
        out << silencer << "chcp 65001 > NUL\n";

        if (debug_wrapper_scripts)
        {
            out << "echo *** environment before *** 1>&2\n";
            out << "SET 1>&2\n";
        }

        out << silencer << "CALL \"" << conda_bat << "\" activate " << prefix << "\n";
        out << silencer << "IF %ERRORLEVEL% NEQ 0 EXIT /b %ERRORLEVEL%\n";

        if (debug_wrapper_scripts)
        {
            out << "echo *** environment after *** 1>&2\n";
            out << "SET 1>&2\n";
        }
#else
        auto tf = std::make_unique<TemporaryFile>();
        std::ofstream out = open_ofstream(tf->path());
        std::stringstream hook_quoted;

        std::string shebang, dev_arg;

        if (!is_mamba_exe)
        {
            if (auto exe = util::get_env("CONDA_EXE"))
            {
                shebang = exe.value();
            }
            else
            {
                shebang = std::string(root_prefix / "bin" / "conda");
            }

            hook_quoted << std::quoted(shebang, '\'') << " 'shell.posix' 'hook' " << dev_arg;
        }
        else
        {
            // Micromamba hook
            out << "export MAMBA_EXE=" << std::quoted(get_self_exe_path().string(), '\'') << "\n";
            hook_quoted << "\"$MAMBA_EXE\" 'shell' 'hook' '-s' 'bash' '-r' "
                        << std::quoted(root_prefix.string(), '\'');
        }
        if (debug_wrapper_scripts)
        {
            out << "set -x\n";
            out << ">&2 echo \"*** environment before ***\"\n"
                << ">&2 env\n"
                << ">&2 echo \"$(" << hook_quoted.str() << ")\"\n";
        }
        out << "eval \"$(" << hook_quoted.str() << ")\"\n";

        if (!is_mamba_exe)
        {
            out << "conda activate " << dev_arg << " " << std::quoted(prefix.string()) << "\n";
        }
        else
        {
            out << get_self_exe_path().stem().string() << " activate "
                << std::quoted(prefix.string()) << "\n";
        }


        if (debug_wrapper_scripts)
        {
            out << ">&2 echo \"*** environment after ***\"\n"
                << ">&2 env\n";
        }
#endif
        // write our command
        out << "\n" << quote_for_shell(arguments);
        return tf;
    }

    PreparedWrappedCall prepare_wrapped_call(
        const PrefixParams& prefix_params,
        const std::vector<std::string>& cmd,
        bool is_mamba_exe
    )
    {
        std::vector<std::string> command_args;
        std::unique_ptr<TemporaryFile> script_file;

        if (util::on_win)
        {
            ensure_comspec_set();
            auto comspec = util::get_env("COMSPEC");
            if (!comspec)
            {
                throw std::runtime_error(
                    util::concat("Failed to run script: COMSPEC not set in env vars.")
                );
            }

            script_file = wrap_call(
                prefix_params.root_prefix,
                prefix_params.target_prefix,
                cmd,
                is_mamba_exe
            );

            command_args = { comspec.value(), "/D", "/C", script_file->path().string() };
        }
        else
        {
            // shell_path = 'sh' if 'bsd' in sys.platform else 'bash'
            fs::u8path shell_path = util::which("bash");
            if (shell_path.empty())
            {
                shell_path = util::which("sh");
            }
            if (shell_path.empty())
            {
                LOG_ERROR << "Failed to find a shell to run the script with.";
                shell_path = "sh";
            }

            script_file = wrap_call(
                prefix_params.root_prefix,
                prefix_params.target_prefix,
                cmd,
                is_mamba_exe
            );
            command_args.push_back(shell_path.string());
            command_args.push_back(script_file->path().string());
        }
        return { command_args, std::move(script_file) };
    }

    bool is_yaml_file_name(std::string_view filename)
    {
        return util::ends_with(filename, ".yml") || util::ends_with(filename, ".yaml");
    }

    std::optional<std::string>
    proxy_match(const std::string& url, const std::map<std::string, std::string>& proxy_servers)
    {
        /* This is a reimplementation of requests.utils.select_proxy(), of the python requests
        library used by conda */
        if (proxy_servers.empty())
        {
            return std::nullopt;
        }

        const auto url_parsed = util::URL::parse(url).value();
        auto scheme = url_parsed.scheme();
        auto host = url_parsed.host();
        std::vector<std::string> options;

        if (host.empty())
        {
            options = { std::string(scheme), "all" };
        }
        else
        {
            options = {
                util::concat(scheme, "://", host),
                std::string(scheme),
                util::concat("all://", host),
                "all",
            };
        }

        for (auto& option : options)
        {
            auto proxy = proxy_servers.find(option);
            if (proxy != proxy_servers.end())
            {
                return proxy->second;
            }
        }

        return std::nullopt;
    }

    std::string hide_secrets(std::string_view str)
    {
        std::string copy(str);

        if (util::contains(str, "/t/"))
        {
            copy = std::regex_replace(copy, token_regex(), "/t/*****");
        }

        copy = std::regex_replace(copy, http_basicauth_regex(), "$1$2:*****@");

        return copy;
    }

}  // namespace mamba
