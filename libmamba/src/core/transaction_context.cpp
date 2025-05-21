// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.
#ifndef _WIN32
#include <csignal>
#endif

#include <reproc++/drain.hpp>

#include "mamba/core/error_handling.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/transaction_context.hpp"
#include "mamba/util/environment.hpp"
#include "mamba/util/string.hpp"

extern const char data_compile_pyc_py[];

namespace mamba
{
    void compile_python_sources(std::ostream& out)
    {
        out << data_compile_pyc_py;
    }

    std::string compute_short_python_version(const std::string& long_version)
    {
        auto sv = util::split(long_version, ".");
        if (sv.size() < 2)
        {
            LOG_ERROR << "Could not compute short python version from " << long_version;
            return long_version;
        }
        return util::concat(sv[0], '.', sv[1]);
    }

    // supply short python version, e.g. 2.7, 3.5...
    fs::u8path get_python_short_path(const std::string& python_version [[maybe_unused]])
    {
#ifdef _WIN32
        return "python.exe";
#else
        return fs::u8path("bin") / util::concat("python", python_version);
#endif
    }

    fs::u8path get_python_site_packages_short_path(const std::string& python_version)
    {
        if (python_version.size() == 0)
        {
            return fs::u8path();
        }

#ifdef _WIN32
        return fs::u8path("Lib") / "site-packages";
#else
        return fs::u8path("lib") / util::concat("python", python_version) / "site-packages";
#endif
    }

    fs::u8path get_bin_directory_short_path()
    {
#ifdef _WIN32
        return "Scripts";
#else
        return "bin";
#endif
    }

    fs::u8path get_python_noarch_target_path(
        const std::string& source_short_path,
        const fs::u8path& target_site_packages_short_path
    )
    {
        if (util::starts_with(source_short_path, "site-packages/"))
        {
            // replace `site_packages/` with prefix/site_packages
            return target_site_packages_short_path
                   / source_short_path.substr(14, source_short_path.size() - 14);
        }
        else if (util::starts_with(source_short_path, "python-scripts/"))
        {
            return get_bin_directory_short_path()
                   / source_short_path.substr(15, source_short_path.size() - 15);
        }
        else
        {
            return source_short_path;
        }
    }

    TransactionContext::PythonParams build_python_params(std::pair<std::string, std::string> py_versions)
    {
        TransactionContext::PythonParams res;
        if (py_versions.first.size() != 0)
        {
            res.has_python = true;
            res.python_version = std::move(py_versions.first);
            res.old_python_version = std::move(py_versions.second);
            res.short_python_version = compute_short_python_version(res.python_version);
            res.python_path = get_python_short_path(res.short_python_version);
            res.site_packages_path = get_python_site_packages_short_path(res.short_python_version);
        }
        return res;
    }

    TransactionContext::TransactionContext() = default;

    TransactionContext::TransactionContext(const Context& context)
        : m_context(&context)
    {
    }

    TransactionContext::TransactionContext(
        const Context& context,
        std::pair<std::string, std::string> py_versions,
        std::vector<specs::MatchSpec> lrequested_specs
    )
        : requested_specs(std::move(lrequested_specs))
        , m_prefix_params(context.prefix_params)
        , m_link_params(context.link_params)
        , m_python_params(build_python_params(std::move(py_versions)))
        , m_context(&context)
    {
        if (m_python_params.python_version.size() == 0)
        {
            LOG_INFO << "No python version given to TransactionContext, leaving it empty";
        }
        if (m_prefix_params.relocate_prefix.empty())
        {
            m_prefix_params.relocate_prefix = m_prefix_params.target_prefix;
        }
    }

    TransactionContext::~TransactionContext()
    {
        wait_for_pyc_compilation();
    }

    auto TransactionContext::prefix_params() const -> const PrefixParams&
    {
        return m_prefix_params;
    }

    auto TransactionContext::link_params() const -> const LinkParams&
    {
        return m_link_params;
    }

    auto TransactionContext::python_params() const -> const PythonParams&
    {
        return m_python_params;
    }

    void TransactionContext::throw_if_not_ready() const
    {
        if (m_context == nullptr)
        {
            throw mamba_error(
                "attempted to use TransactionContext while no Context was specified",
                mamba_error_code::internal_failure
            );
        }
    }

    bool TransactionContext::start_pyc_compilation_process()
    {
        throw_if_not_ready();

        if (m_pyc_process)
        {
            return true;
        }

#ifndef _WIN32
        std::signal(SIGPIPE, SIG_IGN);
#endif
        const auto complete_python_path = prefix_params().target_prefix / python_params().python_path;
        std::vector<std::string> command = {
            complete_python_path.string(), "-Wi", "-m", "compileall", "-q", "-l", "-i", "-"
        };

        auto py_ver_split = util::split(python_params().python_version, ".");

        try
        {
            if (std::stoull(py_ver_split[0]) >= 3 && std::stoull(py_ver_split[1]) > 5)
            {
                m_pyc_compileall = std::make_unique<TemporaryFile>();
                std::ofstream compileall_f = open_ofstream(m_pyc_compileall->path());
                compile_python_sources(compileall_f);
                compileall_f.close();

                command = { complete_python_path.string(),
                            "-Wi",
                            "-u",
                            m_pyc_compileall->path().string() };
            }
        }
        catch (const std::exception& e)
        {
            LOG_ERROR << "Bad conversion of Python version '" << python_params().python_version << "': " << e.what();
            return false;
        }

        m_pyc_process = std::make_unique<reproc::process>();

        reproc::options options;
#ifndef _WIN32
        options.env.behavior = reproc::env::empty;
#endif
        std::map<std::string, std::string> envmap;
        auto& ctx = context();
        envmap["MAMBA_EXTRACT_THREADS"] = std::to_string(ctx.threads_params.extract_threads);
        auto qemu_ld_prefix = util::get_env("QEMU_LD_PREFIX");
        if (qemu_ld_prefix)
        {
            envmap["QEMU_LD_PREFIX"] = qemu_ld_prefix.value();
        }
        options.env.extra = envmap;

        options.stop = {
            { reproc::stop::wait, reproc::milliseconds(10000) },
            { reproc::stop::terminate, reproc::milliseconds(5000) },
            { reproc::stop::kill, reproc::milliseconds(2000) },
        };

        options.redirect.out.type = reproc::redirect::pipe;
        options.redirect.err.type = reproc::redirect::pipe;

        const std::string cwd = prefix_params().target_prefix.string();
        options.working_directory = cwd.c_str();

        auto [wrapped_command, script_file] = prepare_wrapped_call(
            context().prefix_params,
            command,
            WrappedCallOptions::from_context(ctx)
        );
        m_pyc_script_file = std::move(script_file);

        LOG_INFO << "Running wrapped python compilation command " << util::join(" ", command);
        std::error_code ec = m_pyc_process->start(wrapped_command, options);

        if (ec == std::errc::no_such_file_or_directory)
        {
            LOG_ERROR << "Program not found. Make sure it's available from the PATH. "
                      << ec.message();
            m_pyc_process = nullptr;
            return false;
        }

        return true;
    }

    bool TransactionContext::try_pyc_compilation(const std::vector<fs::u8path>& py_files)
    {
        throw_if_not_ready();

        static std::mutex pyc_compilation_mutex;
        std::lock_guard<std::mutex> lock(pyc_compilation_mutex);

        if (!python_params().has_python)
        {
            LOG_WARNING << "Can't compile pyc: Python not found";
            return false;
        }

        if (start_pyc_compilation_process() && !m_pyc_process)
        {
            return false;
        }

        LOG_INFO << "Compiling " << py_files.size() << " files to pyc";
        for (auto& f : py_files)
        {
            auto fs = f.string() + "\n";

            auto [nbytes, ec] = m_pyc_process->write(
                reinterpret_cast<const uint8_t*>(&fs[0]),
                fs.size()
            );
            if (ec)
            {
                LOG_INFO << "writing to stdin failed " << ec.message();
                return false;
            }
        }

        return true;
    }

    void TransactionContext::wait_for_pyc_compilation()
    {
        throw_if_not_ready();

        if (m_pyc_process)
        {
            std::error_code ec;
            ec = m_pyc_process->close(reproc::stream::in);
            if (ec)
            {
                LOG_WARNING << "closing stdin failed " << ec.message();
            }

            std::string output;
            std::string err;
            reproc::sink::string output_sink(output);
            reproc::sink::string err_sink(err);
            ec = reproc::drain(*m_pyc_process, output_sink, err_sink);
            if (ec)
            {
                LOG_WARNING << "draining failed " << ec.message();
            }

            int status = 0;
            std::tie(status, ec) = m_pyc_process->stop({
                { reproc::stop::wait, reproc::milliseconds(100000) },
                { reproc::stop::terminate, reproc::milliseconds(5000) },
                { reproc::stop::kill, reproc::milliseconds(2000) },
            });
            if (ec || status != 0)
            {
                LOG_INFO << "noarch pyc compilation failed (cross-compiling?).";
                if (ec)
                {
                    LOG_INFO << ec.message();
                }
                LOG_INFO << "stdout:" << output;
                LOG_INFO << "stdout:" << err;
            }
            m_pyc_process = nullptr;
        }
    }
}
