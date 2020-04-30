#ifndef MAMBA_TRANSACTION_CONTEXT
#define MAMBA_TRANSACTION_CONTEXT

#include <string>

#include "thirdparty/filesystem.hpp"

namespace fs = ghc::filesystem;

namespace mamba
{
    std::string compute_short_python_version(const std::string& long_version);
    // supply short python version, e.g. 2.7, 3.5...
    fs::path get_python_short_path(const std::string& python_version);
    fs::path get_python_site_packages_short_path(const std::string& python_version);
    fs::path get_bin_directory_short_path();
    fs::path get_python_noarch_target_path(const std::string& source_short_path, const fs::path& target_site_packages_short_path);

	class TransactionContext
	{
	public:

		TransactionContext() = default;
		TransactionContext(const fs::path& prefix, const std::string& py_version);

		fs::path target_prefix;
	    fs::path site_packages_path;
	    fs::path python_path;
	    std::string python_version;
	    std::string short_python_version;
	};
}

#endif