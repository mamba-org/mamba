// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.
#ifndef _WIN32
#include <csignal>
#endif

#include <reproc++/drain.hpp>

#include "mamba/core/transaction_context.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/util.hpp"

namespace mamba
{
    void compile_python_sources(std::ostream& out)
    {
        constexpr const char script[] =
#include "../data/compile_pyc.py"
            ;
        out << script;
    }

    std::string compute_short_python_version(const std::string& long_version)
    {
        auto sv = split(long_version, ".");
        return concat(sv[0], '.', sv[1]);
    }

    // supply short python version, e.g. 2.7, 3.5...
    fs::path get_python_short_path(const std::string& python_version)
    {
#ifdef _WIN32
        return "python.exe";
#else
        return fs::path("bin") / concat("python", python_version);
#endif
    }

    fs::path get_python_site_packages_short_path(const std::string& python_version)
    {
        if (python_version.size() == 0)
        {
            return fs::path();
        }

#ifdef _WIN32
        return fs::path("Lib") / "site-packages";
#else
        return fs::path("lib") / concat("python", python_version) / "site-packages";
#endif
    }

    fs::path get_bin_directory_short_path()
    {
#ifdef _WIN32
        return "Scripts";
#else
        return "bin";
#endif
    }

    fs::path get_python_noarch_target_path(const std::string& source_short_path,
                                           const fs::path& target_site_packages_short_path)
    {
        if (starts_with(source_short_path, "site-packages/"))
        {
            // replace `site_packages/` with prefix/site_packages
            return target_site_packages_short_path
                   / source_short_path.substr(14, source_short_path.size() - 14);
        }
        else if (starts_with(source_short_path, "python-scripts/"))
        {
            return get_bin_directory_short_path()
                   / source_short_path.substr(15, source_short_path.size() - 15);
        }
        else
        {
            return source_short_path;
        }
    }

    TransactionContext::TransactionContext()
    {
        compile_pyc = Context::instance().compile_pyc;
    }

    TransactionContext::TransactionContext(const fs::path& prefix,
                                           const std::string& py_version,
                                           const std::vector<MatchSpec>& requested_specs)
        : has_python(py_version.size() != 0)
        , target_prefix(prefix)
        , python_version(py_version)
        , requested_specs(requested_specs)
    {
        auto& ctx = Context::instance();
        compile_pyc = ctx.compile_pyc;
        allow_softlinks = ctx.allow_softlinks;
        always_copy = ctx.always_copy;
        always_softlink = ctx.always_softlink;

        if (python_version.size() == 0)
        {
            LOG_INFO << "No python version given to TransactionContext, leaving it empty";
        }
        else
        {
            short_python_version = compute_short_python_version(python_version);
            python_path = get_python_short_path(short_python_version);
            site_packages_path = get_python_site_packages_short_path(short_python_version);
        }
    }

    TransactionContext& TransactionContext::operator=(const TransactionContext& other)
    {
        if (this != &other) {
            has_python = other.has_python;
            target_prefix = other.target_prefix;
            python_version = other.python_version;
            requested_specs = other.requested_specs;

            compile_pyc = other.compile_pyc;
            allow_softlinks = other.allow_softlinks;
            always_copy = other.always_copy;
            always_softlink = other.always_softlink;
            short_python_version = other.short_python_version;
            python_path = other.python_path;
            site_packages_path = other.site_packages_path;
        }
        return *this;
    }

    TransactionContext::~TransactionContext() {
        wait_for_pyc_compation();
    }

    bool TransactionContext::start_pyc_compilation_process()
    {
        if (m_pyc_process) {
            return true;
        }

#ifndef _WIN32
        std::signal(SIGPIPE, SIG_IGN);
#endif

        std::vector<std::string> command = { target_prefix / python_path,
                                             "-Wi",
                                             "-m",
                                             "compileall",
                                             "-q",
                                             "-l",
                                             "-i",
                                             "-"};

        auto py_ver_split = split(python_version, ".");

        try
        {
            if (std::stoull(std::string(py_ver_split[0])) >= 3
                && std::stoull(std::string(py_ver_split[1])) > 5)
            {
                m_pyc_compileall = std::make_unique<TemporaryFile>();
                std::ofstream compileall_f = open_ofstream(m_pyc_compileall->path());
                compile_python_sources(compileall_f);
                compileall_f.close();

                command = {target_prefix / python_path, "-Wi", "-u", m_pyc_compileall->path().c_str()};
            }
        }
        catch (const std::exception& e)
        {
            LOG_ERROR << "Bad conversion of Python version '" << python_version
                      << "': " << e.what();
            throw std::runtime_error("Bad conversion. Aborting.");
        }

        m_pyc_process = std::make_unique<reproc::process>();

        reproc::options options;
        options.env.behavior = reproc::env::empty;
        options.stop = {
            { reproc::stop::wait, reproc::milliseconds(10000) },
            { reproc::stop::terminate, reproc::milliseconds(5000) },
            { reproc::stop::kill, reproc::milliseconds(2000) },
        };

        options.redirect.out.type = reproc::redirect::pipe;
        options.redirect.err.type = reproc::redirect::pipe;

        std::string cwd = target_prefix;
        options.working_directory = cwd.c_str();

        auto [wrapped_command, script_file]
            = prepare_wrapped_call(target_prefix, command);
        m_pyc_script_file = std::move(script_file);

        LOG_INFO << "Running wrapped python compilation command " << join(" ", command);
        std::error_code ec = m_pyc_process->start(wrapped_command, options);

        if (ec == std::errc::no_such_file_or_directory) {
            LOG_ERROR << "Program not found. Make sure it's available from the PATH. " << ec.message();
            throw std::runtime_error("pyc compilation failed with program not found. Aborting.");
        }

        return true;
    }

    bool TransactionContext::try_pyc_compation(const std::vector<fs::path>& py_files)
    {
        static std::mutex pyc_compation_mutex;
        std::lock_guard<std::mutex> lock(pyc_compation_mutex);

        if (!has_python)
        {
            LOG_WARNING << "Can't compile pyc: Python not found";
            return false;
        }

        start_pyc_compilation_process();
        if (!m_pyc_process)
        {
            return false;
        }

        for (auto& f : py_files)
        {
            LOG_INFO << "Compiling " << f;
            auto fs = f.string() + "\n";
            size_t nbytes;
            std::error_code ec;

            std::tie(nbytes, ec) = m_pyc_process->write(reinterpret_cast<const uint8_t*>(&fs[0]), fs.size());
            if (ec) {
                LOG_INFO << "writing to stdin failed " << ec.message();
                return false;
            }
        }

        return true;
    }

    void TransactionContext::wait_for_pyc_compation() {
        if (m_pyc_process) {
            std::error_code ec = m_pyc_process->close(reproc::stream::in);
            if (ec) {
                LOG_INFO << "closing stdin failed " << ec.message();
            }

            std::string output;
            std::string err;
            reproc::sink::string output_sink(output);
            reproc::sink::string err_sink(err);
            ec = reproc::drain(*m_pyc_process, output_sink, err_sink);
            if (ec) {
                LOG_INFO << "draining failed " << ec.message();
            }

            int status = 0;
            std::tie(status, ec) = m_pyc_process->stop(
                {
                    { reproc::stop::wait, reproc::milliseconds(100000) },
                    { reproc::stop::terminate, reproc::milliseconds(5000) },
                    { reproc::stop::kill, reproc::milliseconds(2000) },
                }
            );
            if (ec || status != 0) {
                LOG_INFO << "noarch pyc compilation failed (cross-compiling?).";
                if (ec) {
                    LOG_INFO << ec.message();
                }
                LOG_INFO << "stdout:" << output;
                LOG_INFO << "stdout:" << err;
            }
            m_pyc_process = nullptr;
        }
    }
}
