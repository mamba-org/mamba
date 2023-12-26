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

    TransactionContext::TransactionContext() = default;

    TransactionContext::TransactionContext(const Context& context)
        : m_context(&context)
    {
        compile_pyc = this->context().compile_pyc;
    }

    TransactionContext::TransactionContext(
        const Context& context,
        const fs::u8path& ltarget_prefix,
        const std::pair<std::string, std::string>& py_versions,
        const std::vector<specs::MatchSpec>& lrequested_specs
    )
        : has_python(py_versions.first.size() != 0)
        , target_prefix(ltarget_prefix)
        , relocate_prefix(ltarget_prefix)
        , python_version(py_versions.first)
        , old_python_version(py_versions.second)
        , requested_specs(lrequested_specs)
        , m_context(&context)
    {
        const auto& ctx = this->context();
        compile_pyc = ctx.compile_pyc;
        allow_softlinks = ctx.allow_softlinks;
        always_copy = ctx.always_copy;
        always_softlink = ctx.always_softlink;

        std::string old_short_python_version;
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
        if (old_python_version.size())
        {
            old_short_python_version = compute_short_python_version(old_python_version);
            relink_noarch = (short_python_version != old_short_python_version);
        }
        else
        {
            relink_noarch = false;
        }
    }

    TransactionContext::TransactionContext(
        const Context& context,
        const fs::u8path& ltarget_prefix,
        const fs::u8path& lrelocate_prefix,
        const std::pair<std::string, std::string>& py_versions,
        const std::vector<specs::MatchSpec>& lrequested_specs
    )
        : TransactionContext(context, ltarget_prefix, py_versions, lrequested_specs)
    {
        if (lrelocate_prefix.empty())
        {
            relocate_prefix = ltarget_prefix;
        }
        else
        {
            relocate_prefix = lrelocate_prefix;
        }
    }

    TransactionContext& TransactionContext::operator=(const TransactionContext& other)
    {
        if (this != &other)
        {
            has_python = other.has_python;
            target_prefix = other.target_prefix;
            relocate_prefix = other.relocate_prefix;
            python_version = other.python_version;
            old_python_version = other.old_python_version;
            requested_specs = other.requested_specs;

            compile_pyc = other.compile_pyc;
            allow_softlinks = other.allow_softlinks;
            always_copy = other.always_copy;
            always_softlink = other.always_softlink;
            short_python_version = other.short_python_version;
            python_path = other.python_path;
            site_packages_path = other.site_packages_path;
            relink_noarch = other.relink_noarch;

            m_context = other.m_context;
        }
        return *this;
    }

    TransactionContext::~TransactionContext()
    {
        wait_for_pyc_compilation();
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
        const auto complete_python_path = target_prefix / python_path;
        std::vector<std::string> command = {
            complete_python_path.string(), "-Wi", "-m", "compileall", "-q", "-l", "-i", "-"
        };

        auto py_ver_split = util::split(python_version, ".");

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
            LOG_ERROR << "Bad conversion of Python version '" << python_version << "': " << e.what();
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

        const std::string cwd = target_prefix.string();
        options.working_directory = cwd.c_str();

        auto [wrapped_command, script_file] = prepare_wrapped_call(ctx, target_prefix, command);
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

        if (!has_python)
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
