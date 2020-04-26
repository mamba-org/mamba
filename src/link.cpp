#include "link.hpp"
#include "output.hpp"
#include "validate.hpp"

namespace mamba
{
    UnlinkPackage::UnlinkPackage(const std::string& specifier, const fs::path& prefix)
        : m_specifier(specifier), m_prefix(prefix)
    {
    }

    bool UnlinkPackage::unlink_path(const nlohmann::json& path_data)
    {
        std::string subtarget = path_data["_path"].get<std::string>();
        fs::path dst = m_prefix / subtarget;
        fs::remove(dst);

        // TODO what do we do with empty directories?
        // remove empty parent path
        auto parent_path = dst.parent_path();
        while (fs::is_empty(parent_path))
        {
            fs::remove(parent_path);
            parent_path = parent_path.parent_path();
        }
    }

    bool UnlinkPackage::execute()
    {
        // find the recorded JSON file
        fs::path json = m_prefix / "conda-meta" / (m_specifier + ".json");
        std::cout << "opening " << json << std::endl;
        std::ifstream json_file(json);
        nlohmann::json json_record;
        json_file >> json_record;

        for (auto& path : json_record["paths_data"]["paths"])
        {
            unlink_path(path);
        }

        json_file.close();

        fs::remove(json);
    }

    LinkPackage::LinkPackage(const fs::path& source, const fs::path& prefix)
        : m_source(source), m_prefix(prefix)
    {

    }

    std::string LinkPackage::link_path(const nlohmann::json& path_data)
    {
        std::string subtarget = path_data["_path"].get<std::string>();
        LOG_INFO << "linking path! " << subtarget;
        fs::path dst = m_prefix / subtarget;
        fs::path src = m_source / subtarget;
        if (!fs::exists(dst.parent_path()))
        {
            fs::create_directories(dst.parent_path());
        }

        if (fs::exists(dst))
        {
            // This needs to raise a clobberwarning
            throw std::runtime_error("clobberwarning");
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
            return validate::sha256sum(dst);
        }

        if (path_type == "hardlink")
        {
            std::cout << "linked " << dst << std::endl;
            fs::create_hard_link(src, dst);
        }
        else if (path_type == "softlink")
        {
            // if (fs::islink())
            // fs::path link_target = fs::read_symlink(src);
            std::cout << "soft linked " << dst << std::endl;
            fs::copy_symlink(src, dst);
        }
        else
        {
            throw std::runtime_error("Path type not implemented: " + path_type);
        }
        // TODO we could also use the SHA256 sum of the paths json
        return validate::sha256sum(dst);
    }

    bool LinkPackage::execute()
    {
        nlohmann::json paths_json, index_json, out_json;
        std::ifstream paths_file(m_source / "info" / "paths.json");

        LOG_INFO << "Executing link: " << (m_source / "info" / "paths.json");

        paths_file >> paths_json;
        LOG_INFO << "Read paths.json";

        // TODO should we take this from `files.txt`?
        std::vector<std::string> files_record;

        // TODO record SHA256 in prefix!
        for (auto& path : paths_json["paths"])
        {
            auto sha256_in_prefix = link_path(path);
            files_record.push_back(path["_path"].get<std::string>());
            path["sha256_in_prefix"] = sha256_in_prefix;
        }

        LOG_INFO << "Reading repodata_record.json";
        std::ifstream repodata_f(m_source / "info" / "repodata_record.json");
        repodata_f >> index_json;
        LOG_INFO << "Read repodata_record.json";
 
        std::string f_name = index_json["name"].get<std::string>() + "-" + 
                             index_json["version"].get<std::string>() + "-" + 
                             index_json["build"].get<std::string>();

        out_json = index_json;
        out_json["paths_data"] = paths_json;
        out_json["files"] = files_record;
        out_json["requested_spec"] = "TODO";
        out_json["package_tarball_full_path"] = std::string(m_source) + ".tar.bz2";
        out_json["extracted_package_dir"] = m_source;

        // TODO find out what `1` means
        out_json["link"] = {
            {"source", std::string(m_source)},
            {"type", 1}
        };

        fs::path prefix_meta = m_prefix / "conda-meta";
        if (!fs::exists(prefix_meta))
        {
            fs::create_directory(prefix_meta);
        }
        LOG_INFO << "Writing out.sjon";

        std::ofstream out_file(prefix_meta / (f_name + ".json"));
        out_file << out_json.dump(4);
    }
}