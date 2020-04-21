#pragma once

#include <iostream>

#include "nlohmann/json.hpp"
#include "thirdparty/filesystem.hpp"

namespace fs = ghc::filesystem;

namespace mamba
{

class UnlinkPackage
{

};

class LinkPackage
{
public:
    LinkPackage(const fs::path& source, const fs::path& prefix)
        : m_source(source), m_prefix(prefix)
    {

    }

    bool link(const nlohmann::json& path_data)
    {
        std::string subtarget = path_data["_path"].get<std::string>();
        fs::path dst = m_prefix / subtarget;
        fs::path src = m_source / subtarget;
        if (!fs::exists(dst.parent_path()))
        {
            fs::create_directories(dst.parent_path());
        }

        if (fs::exists(dst))
        {
            // TODO this is certainly not correct! :)
            // This needs to raise a clobberwarning
            fs::remove(dst);
            // return true;
        }

        std::string path_type = path_data["path_type"].get<std::string>();

        if (path_data.find("prefix_placeholder") != path_data.end())
        {
            // we have to replace the PREFIX stuff in the data
            // and copy the file
            std::string prefix_placeholder = path_data["prefix_placeholder"].get<std::string>();
            std::string new_prefix = m_prefix;
            bool text_mode = path_data["file_mode"].get<std::string>() == "text";
            if (!text_mode) 
            {
                assert(path_data["file_mode"].get<std::string>() == "binary");
                new_prefix.resize(prefix_placeholder.size(), '\0');
            }

            if (true || path_data["file_mode"].get<std::string>() == "text")
            {
                std::ifstream fi;
                text_mode ? fi.open(src) : fi.open(src, std::ios::binary);
                fi.seekg(0, std::ios::end);
                size_t size = fi.tellg();
                std::string buffer;
                buffer.resize(size);
                fi.seekg(0);
                fi.read(&buffer[0], size);

                std::size_t pos = buffer.find(prefix_placeholder);
                while (pos != std::string::npos)
                {
                    buffer.replace(pos, prefix_placeholder.size(), new_prefix);
                    pos = buffer.find(prefix_placeholder, pos + new_prefix.size());
                }

                std::ofstream fo;
                text_mode ? fo.open(dst) : fo.open(dst, std::ios::binary);
                fo << buffer;
                std::cout << "written " << dst << std::endl;
                fo.close();

                fs::permissions(dst, fs::status(src).permissions());
            }
            return true;
        }

        if (path_type == "hardlink")
        {
            std::cout << "linked " << dst << std::endl;
            fs::create_hard_link(src, dst);
            return true;
        }
        else if (path_type == "softlink")
        {
            // if (fs::islink())
            // fs::path link_target = fs::read_symlink(src);
            std::cout << "soft linked " << dst << std::endl;
            fs::copy_symlink(src, dst);
            return true;
        }
        else
        {
            throw std::runtime_error("Path type not implemented: " + path_type);
        }
    }

    bool link_paths()
    {
        nlohmann::json paths_json;
        std::ifstream paths_file(m_source / "info" / "paths.json");

        paths_file >> paths_json;

        for (auto& path : paths_json["paths"])
        {
            link(path);
        }
    }

private:
    fs::path m_prefix, m_source;
    nlohmann::json m_files;
};

}