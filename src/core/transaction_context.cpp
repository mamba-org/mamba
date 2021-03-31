// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/core/transaction_context.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/util.hpp"

namespace mamba
{
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

    TransactionContext::TransactionContext(const fs::path& prefix, const std::string& py_version)
        : has_python(py_version.size() != 0)
        , target_prefix(prefix)
        , python_version(py_version)
    {
        auto& ctx = Context::instance();
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
}
