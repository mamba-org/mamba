#ifndef MAMBA_TRANSACTION_CONTEXT
#define MAMBA_TRANSACTION_CONTEXT

#include <string>

#include "thirdparty/filesystem.hpp"

namespace fs = ghc::filesystem;

namespace mamba
{
	class TransactionContext
	{
	public:
		fs::path target_prefix;
	    fs::path site_packages_path;
	    fs::path python_path;
	    std::string python_version;
	    std::string short_python_version;
	};
}

#endif