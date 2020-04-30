#ifndef MAMBA_LINK
#define MAMBA_LINK

#include <iostream>

#include "nlohmann/json.hpp"
#include "thirdparty/filesystem.hpp"

#include "transaction.hpp"
#include "match_spec.hpp"
#include "transaction_context.hpp"

namespace fs = ghc::filesystem;

namespace mamba
{

class UnlinkPackage
{
public:
    UnlinkPackage(const PackageInfo& pkg_info, TransactionContext* context);

    bool unlink_path(const nlohmann::json& path_data);
    bool execute();

private:

    PackageInfo m_pkg_info;
    std::string m_specifier;
    TransactionContext* m_context;
};

class LinkPackage
{
public:

    LinkPackage(const PackageInfo& pkg_info, const fs::path& cache_path, TransactionContext* context);

    std::string link_path(const nlohmann::json& path_data, bool noarch_python);
    bool execute();

private:

    std::vector<fs::path> compile_pyc_files(const std::vector<fs::path>& py_files);

    PackageInfo m_pkg_info;
    fs::path m_source;
    TransactionContext* m_context;
    nlohmann::json m_files;
};

}

#endif