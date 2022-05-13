// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <map>
#include <set>
#include <string>

#include "mamba/core/package_paths.hpp"
#include "mamba/core/util.hpp"

namespace mamba
{
    std::map<std::string, PrefixFileParse> read_has_prefix(const fs::path& path)
    {
        // reads `has_prefix` file and return dict mapping filepaths to
        // tuples(placeholder, FileMode) A line in `has_prefix` contains one of
        //   * filepath
        //   * placeholder mode filepath
        // mode values are one of
        //   * text
        //   * binary
        //
        std::map<std::string, PrefixFileParse> res;
        fs::path file_path = path / "has_prefix";
        if (!fs::exists(file_path))
        {
            return res;
        }
        for (auto& l : read_lines(file_path))
        {
            // TODO: make sure that strings that are quoted are still split correctly
            //       e.g. when a file path contains a space...
            auto s = split(l, " ");
            if (s.size() == 1)
            {
                res[s[0]] = PrefixFileParse{ concat(PREFIX_PLACEHOLDER_1, PREFIX_PLACEHOLDER_2),
                                             "text",
                                             s[0] };
            }
            else if (s.size() == 3)
            {
                res[s[2]] = PrefixFileParse{ s[0], s[1], s[2] };
            }
            else
            {
                throw std::runtime_error(concat("Could not parse ", path.string()));
            }
        }
        return res;
    }

    std::set<std::string> read_no_link(const fs::path& info_dir)
    {
        std::vector<std::string> no_link_lines, no_softlink_lines;
        if (fs::exists(info_dir / "no_link"))
        {
            no_link_lines = read_lines(info_dir / "no_link");
        }

        if (fs::exists(info_dir / "no_softlink"))
        {
            no_softlink_lines = read_lines(info_dir / "no_softlink");
        }

        std::set<std::string> result;
        result.insert(no_link_lines.cbegin(), no_link_lines.cend());
        result.insert(no_softlink_lines.cbegin(), no_softlink_lines.cend());
        return result;
    }

    std::vector<PathData> read_paths(const fs::path& directory)
    {
        auto info_dir = directory / "info";
        auto paths_json_path = info_dir / "paths.json";

        auto parse_file_mode = [](nlohmann::json& j) -> FileMode
        {
            if (j.find("file_mode") != j.end())
            {
                // check if "text" or "binary"
                if (j["file_mode"].get<std::string>()[0] == 't')
                {
                    return FileMode::TEXT;
                }
                else if (j["file_mode"].get<std::string>()[0] == 'b')
                {
                    return FileMode::BINARY;
                }
            }
            return FileMode::UNDEFINED;
        };

        auto parse_path_type = [](nlohmann::json& j) -> PathType
        {
            if (j.find("path_type") != j.end())
            {
                // TODO find a DIRECTORY path type
                // check if "text" or "binary"
                if (j["path_type"].get<std::string>()[0] == 's')
                {
                    return PathType::SOFTLINK;
                }
                else if (j["path_type"].get<std::string>()[0] == 'h')
                {
                    return PathType::HARDLINK;
                }
            }
            return PathType::UNDEFINED;
        };

        std::vector<PathData> res;
        if (fs::exists(paths_json_path))
        {
            nlohmann::json paths_json;
            std::ifstream paths_file = open_ifstream(paths_json_path);
            paths_file >> paths_json;
            if (paths_json["paths_version"] != 1)
            {
                throw std::runtime_error("Package version (paths.json file) too new for mamba.");
            }
            for (auto& jpath : paths_json["paths"])
            {
                PathData p;
                p.path = jpath["_path"];
                p.size_in_bytes = jpath["size_in_bytes"];

                // tentative data
                p.file_mode = parse_file_mode(jpath);
                p.path_type = parse_path_type(jpath);

                if (p.path_type != PathType::SOFTLINK)
                {
                    assign_or(jpath, "sha256", p.sha256, std::string(""));
                }

                if (jpath.find("no_link") != jpath.end())
                {
                    if (jpath["no_link"] == true)
                    {
                        p.no_link = true;
                    }
                }

                if (jpath.find("prefix_placeholder") != jpath.end())
                {
                    p.prefix_placeholder = jpath["prefix_placeholder"];
                }

                res.push_back(std::move(p));
            }
        }
        else
        {
            auto has_prefix_files = read_has_prefix(info_dir);
            auto no_link = read_no_link(info_dir);
            auto files = read_lines(info_dir / "files");

            for (auto& f : files)
            {
                PathData p;
                p.path = f;
                if (has_prefix_files.find(f) != has_prefix_files.end())
                {
                    p.prefix_placeholder = has_prefix_files[f].placeholder;
                    p.file_mode = has_prefix_files[f].file_mode[0] == 't' ? FileMode::TEXT
                                                                          : FileMode::BINARY;
                }
                if (no_link.find(f) != no_link.end())
                {
                    p.no_link = true;
                }
                if (fs::is_symlink(directory / f))
                {
                    p.path_type = PathType::SOFTLINK;
                }
                else
                {
                    p.path_type = PathType::HARDLINK;
                }
                res.push_back(std::move(p));
            }
        }
        return res;
    }
}  // namespace mamba
