#ifndef MAMBA_LINK
#define MAMBA_LINK

#include <iostream>

#include "nlohmann/json.hpp"
#include "thirdparty/filesystem.hpp"

#include "transaction.hpp"
#include "transaction_context.hpp"

namespace fs = ghc::filesystem;

namespace mamba
{

class UnlinkPackage
{
public:
    UnlinkPackage(const std::string& specifier, TransactionContext* context);

    bool unlink_path(const nlohmann::json& path_data);
    bool execute();

private:

    std::string m_specifier;
    TransactionContext* m_context;
};

class LinkPackage
{
public:

    LinkPackage(const fs::path& source, TransactionContext* context);

    std::string link_path(const nlohmann::json& path_data, bool noarch_python);
    bool execute();

private:

    std::vector<fs::path> compile_pyc_files(const std::vector<fs::path>& py_files);

    fs::path m_source;
    TransactionContext* m_context;
    nlohmann::json m_files;
};

}

#endif