#ifndef MAMBA_LINK
#define MAMBA_LINK

#include <iostream>

#include "nlohmann/json.hpp"
#include "thirdparty/filesystem.hpp"

namespace fs = ghc::filesystem;

namespace mamba
{

class UnlinkPackage
{
public:
    UnlinkPackage(const std::string& specifier, const fs::path& prefix);

    bool unlink_path(const nlohmann::json& path_data);
    bool execute();

private:

    std::string m_specifier;
    fs::path m_prefix;
};

class LinkPackage
{
public:

    LinkPackage(const fs::path& source, const fs::path& prefix);

    std::string link_path(const nlohmann::json& path_data);
    bool execute();

private:
    fs::path m_prefix, m_source;
    nlohmann::json m_files;
};

}

#endif