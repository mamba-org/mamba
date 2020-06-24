#ifndef MAMBA_LINK
#define MAMBA_LINK

#include <iostream>

#include "nlohmann/json.hpp"
#include "thirdparty/filesystem.hpp"

#include "transaction.hpp"
#include "match_spec.hpp"
#include "transaction_context.hpp"
#include "package_paths.hpp"

namespace fs = ghc::filesystem;

namespace mamba
{

struct python_entry_point_parsed
{
    std::string command, module, func;
};

class UnlinkPackage
{
public:
    UnlinkPackage(const PackageInfo& pkg_info, TransactionContext* context);

    bool execute();

private:

    bool unlink_path(const nlohmann::json& path_data);

    PackageInfo m_pkg_info;
    std::string m_specifier;
    TransactionContext* m_context;
};

class LinkPackage
{
public:

    LinkPackage(const PackageInfo& pkg_info, const fs::path& cache_path, TransactionContext* context);

    bool execute();

private:

    std::tuple<std::string, std::string> link_path(const PathData& path_data, bool noarch_python);
    std::vector<fs::path> compile_pyc_files(const std::vector<fs::path>& py_files);
    auto create_python_entry_point(const fs::path& path, const python_entry_point_parsed& entry_point);

    PackageInfo m_pkg_info;
    fs::path m_source;
    TransactionContext* m_context;
    nlohmann::json m_files;
};

}

#endif