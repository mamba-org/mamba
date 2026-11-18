// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <array>
#include <string>
#include <string_view>

#include "mamba/util/path_manip.hpp"

#include "./transaction_context.hpp"

#ifdef _WIN32
#include <shlobj.h>
#include <windows.h>

#include "mamba/util/os_win.hpp"
#endif

#include "mamba/core/context.hpp"
#include "mamba/core/environments_manager.hpp"
#include "mamba/core/output.hpp"
#include "mamba/util/string.hpp"

namespace mamba
{
    namespace detail
    {
        std::string get_formatted_env_name(
            const std::vector<fs::u8path>& envs_dirs,
            const fs::u8path& root_prefix,
            const fs::u8path& target_prefix
        )
        {
            std::string name = env_name(envs_dirs, root_prefix, target_prefix);
            if (name.find_first_of("\\/") != std::string::npos)
            {
                return "";
            }
            return name;
        }
    }

#ifdef _WIN32
    namespace win
    {
        /*
         * Create a shortcut on a Windows Machine (using COM)
         * Place the shortcut in `programs` directory to make it appear in the
         * startmenu.
         *
         * Args:
         *   path: Path to the executable
         *   description: string that is displayed
         *   filename: Link destination filename
         *   arguments: args to the executable (optional)
         *   work_dir (in v1) or working_dir (in v2): working directory for the executable
         *   icon_path: path to an .ico file
         *   icon_index: index for icon
         */
        void create_shortcut(
            const fs::u8path& path,
            const std::string& description,
            const fs::u8path& filename,
            const std::string& arguments,
            const fs::u8path& work_dir,
            const fs::u8path& icon_path,
            int icon_index
        )
        {
            IShellLink* pShellLink = nullptr;
            IPersistFile* pPersistFile = nullptr;

            HRESULT hres;
            LOG_DEBUG << "Creating shortcut with " << "\n  Path: " << path
                      << "\n  Description: " << description << "\n  Filename: " << filename
                      << "\n  Arguments: " << arguments << "\n  Working Directory: " << work_dir
                      << "\n  Icon Path: " << icon_path << "\n  Icon Index: " << icon_index;

            hres = CoInitialize(nullptr);
            if (FAILED(hres))
            {
                LOG_ERROR << "Could not initialize COM";
                return;
            }
            try
            {
                hres = CoCreateInstance(
                    CLSID_ShellLink,
                    nullptr,
                    CLSCTX_INPROC_SERVER,
                    IID_IShellLink,
                    (void**) &pShellLink
                );
                if (FAILED(hres))
                {
                    throw std::runtime_error("CoCreateInstance failed.");
                }

                hres = pShellLink->QueryInterface(IID_IPersistFile, (void**) &pPersistFile);
                if (FAILED(hres))
                {
                    throw std::runtime_error(
                        "QueryInterface(IPersistFile) error 0x" + std::to_string(hres)
                    );
                }

                hres = pShellLink->SetPath(path.string().c_str());
                if (FAILED(hres))
                {
                    throw std::runtime_error("SetPath() failed, error 0x" + std::to_string(hres));
                }

                hres = pShellLink->SetDescription(description.c_str());
                if (FAILED(hres))
                {
                    throw std::runtime_error(
                        "SetDescription() failed, error 0x" + std::to_string(hres)
                    );
                }

                if (!arguments.empty())
                {
                    hres = pShellLink->SetArguments(arguments.c_str());
                    if (FAILED(hres))
                    {
                        throw std::runtime_error("SetArguments() error 0x" + std::to_string(hres));
                    }
                }

                if (!icon_path.empty())
                {
                    hres = pShellLink->SetIconLocation(icon_path.string().c_str(), icon_index);
                    if (FAILED(hres))
                    {
                        throw std::runtime_error("SetIconLocation() error 0x" + std::to_string(hres));
                    }
                }

                if (!work_dir.empty())
                {
                    hres = pShellLink->SetWorkingDirectory(work_dir.string().c_str());
                    if (FAILED(hres))
                    {
                        throw std::runtime_error(
                            "SetWorkingDirectory() error 0x" + std::to_string(hres)
                        );
                    }
                }

                hres = pPersistFile->Save(filename.wstring().c_str(), true);
                if (FAILED(hres))
                {
                    throw std::runtime_error(
                        util::concat("Failed to create shortcut: ", filename.string(), std::to_string(hres))
                    );
                }
            }
            catch (const std::runtime_error& e)
            {
                LOG_ERROR << e.what();
            }

            if (pPersistFile)
            {
                pPersistFile->Release();
            }
            if (pShellLink)
            {
                pShellLink->Release();
            }

            CoUninitialize();
        }

        void remove_shortcut(const fs::u8path& filename)
        {
            try
            {
                if (fs::exists(filename))
                {
                    fs::remove(filename);
                }
            }
            catch (...)
            {
                LOG_ERROR << "Could not remove shortcut";
            }
            if (fs::is_empty(filename.parent_path()))
            {
                fs::remove(filename.parent_path());
            }
        }
    }  // namespace win
#endif


    void replace_variables(std::string& text, const TransactionContext& context)
    {
        fs::u8path root_prefix = context.prefix_params().root_prefix;
        fs::u8path target_prefix = context.prefix_params().target_prefix;
        std::string py_ver = context.python_params().python_version;

        std::string distribution_name = root_prefix.filename().string();
        if (distribution_name.size() > 1)
        {
            distribution_name[0] = util::to_upper(distribution_name[0]);
        }

        auto to_forward_slash = [](const fs::u8path& p) { return util::path_to_posix(p.string()); };

        auto platform_split = util::split(context.transaction_params().platform, "-");
        std::string platform_bitness;
        if (platform_split.size() >= 2)
        {
            platform_bitness = util::concat("(", platform_split.back(), "-bit)");
        }

        if (py_ver.size())
        {
            py_ver = util::split(py_ver, ".")[0];
        }

        std::map<std::string, std::string> vars = {
            { "${PREFIX}", to_forward_slash(target_prefix) },
            { "${ROOT_PREFIX}", to_forward_slash(root_prefix) },
            { "${PY_VER}", py_ver },
            { "${MENU_DIR}", to_forward_slash(target_prefix / "Menu") },
            { "${DISTRIBUTION_NAME}", distribution_name },
            { "${ENV_NAME}",
              detail::get_formatted_env_name(
                  context.transaction_params().envs_dirs,
                  root_prefix,
                  target_prefix
              ) },
            { "${PLATFORM}", platform_bitness },
        };

#ifdef _WIN32
        vars["${PERSONALDIR}"] = util::path_to_posix(
            util::get_windows_known_user_folder(util::WindowsKnowUserFolder::Documents)
        );
        vars["${USERPROFILE}"] = util::path_to_posix(
            util::get_windows_known_user_folder(util::WindowsKnowUserFolder::Profile)
        );
#endif

        for (auto& [key, val] : vars)
        {
            util::replace_all(text, key, val);
        }
    }

    namespace
    {
        enum class MenuInstVersion : std::uint8_t
        {
            Version1 = 1,
            Version2 = 2,
        };

        void create_remove_shortcut_impl(
            const fs::u8path& json_file,
            const TransactionContext& context,
            [[maybe_unused]] bool remove
        )
        {
            std::string json_content = mamba::read_contents(json_file);
            replace_variables(json_content, context);
            auto j = nlohmann::json::parse(json_content);

            std::string menu_name = j.value("menu_name", "Mamba Shortcuts");

            std::string name_suffix;
            const PrefixParams& pp = context.prefix_params();
            std::string e_name = detail::get_formatted_env_name(
                context.transaction_params().envs_dirs,
                pp.root_prefix,
                pp.target_prefix
            );

            if (e_name.size())
            {
                name_suffix = util::concat(" (", e_name, ")");
            }

#ifdef _WIN32

            // {
            //     "menu_name": "Miniforge${PY_VER}",
            //     "menu_items":
            //     [
            //         {
            //             "name": "Miniforge Prompt",
            //             "system": "%windir%\\system32\\cmd.exe",
            //             "scriptarguments": ["/K", "${ROOT_PREFIX}\\Scripts\\activate.bat",
            //             "${PREFIX}"], "icon": "${MENU_DIR}/console_shortcut.ico"
            //         }
            //     ]
            // }

            const std::array<std::string, 9> override_keys = {
                "name",      "description", "command",  "icon",    "precommand",
                "precreate", "working_dir", "activate", "terminal"
            };

            const fs::u8path root_prefix = pp.root_prefix;
            const fs::u8path target_prefix = pp.target_prefix;

            const bool target_is_base = (target_prefix == root_prefix);

            // using legacy stuff here
            const fs::u8path root_py = root_prefix / "python.exe";
            const fs::u8path root_pyw = root_prefix / "pythonw.exe";
            const fs::u8path env_py = target_prefix / "python.exe";
            const fs::u8path env_pyw = target_prefix / "pythonw.exe";
            const auto cwp_path = root_prefix / "cwp.py";
            std::vector<std::string> cwp_py_args(
                { cwp_path.string(), target_prefix.string(), env_py.string() }
            );
            std::vector<std::string> cwp_pyw_args(
                { cwp_path.string(), target_prefix.string(), env_pyw.string() }
            );

            auto target_dir = fs::u8path(
                                  util::get_windows_known_user_folder(util::WindowsKnowUserFolder::Programs)
                              )
                              / menu_name;

            if (!fs::exists(target_dir))
            {
                fs::create_directories(target_dir);
            }

            auto extend_script_args = [](const auto& item, auto& arguments)
            {
                // this is pretty bad ...
                if (item.contains("scriptargument") && item["scriptargument"].is_string())
                {
                    arguments.push_back(item["scriptargument"].get<std::string>());
                }

                if (item.contains("scriptarguments") && item["scriptarguments"].is_array())
                {
                    for (const auto& arg : item["scriptarguments"])
                    {
                        if (arg.is_string())
                        {
                            arguments.push_back(arg.get<std::string>());
                        }
                    }
                }
            };

            // Check menuinst schema version (through the presence of "$schema" key)
            // cf. https://github.com/conda/ceps/blob/3da0fb0ece/cep-11.md#backwards-compatibility
            auto menuinst_version = MenuInstVersion::Version1;  // v1-legacy
            if (j.contains("$schema"))
            {
                menuinst_version = MenuInstVersion::Version2;  // v2
            }

            if (!j.contains("menu_items") || !j["menu_items"].is_array())
            {
                LOG_WARNING << "No valid 'menu_items' found in " << json_file.string();
                return;
            }

            for (auto& item : j["menu_items"])
            {
                std::string name;
                std::vector<std::string> arguments;
                fs::u8path script;
                fs::u8path workdir;

                // cf. https://github.com/conda/menuinst/pull/180
                if (menuinst_version == MenuInstVersion::Version1)
                {
                    if (!item.contains("name") || !item["name"].is_string())
                    {
                        throw std::runtime_error(
                            "Menu item missing valid 'name' in " + json_file.string()
                        );
                    }
                    name = item["name"].get<std::string>();
                    workdir = item.value("work_dir", "");

                    if (item.contains("pywscript") && item["pywscript"].is_string())
                    {
                        script = root_pyw;
                        arguments = cwp_pyw_args;
                        auto tmp = util::split(item["pywscript"].get<std::string>(), " ");
                        std::copy(tmp.begin(), tmp.end(), back_inserter(arguments));
                    }
                    else if (item.contains("pyscript") && item["pyscript"].is_string())
                    {
                        script = root_py;
                        arguments = cwp_py_args;
                        auto tmp = util::split(item["pyscript"].get<std::string>(), " ");
                        std::copy(tmp.begin(), tmp.end(), back_inserter(arguments));
                    }
                    else if (item.contains("webbrowser") && item["webbrowser"].is_string())
                    {
                        script = root_pyw;
                        arguments = { "-m", "webbrowser", "-t", item["webbrowser"].get<std::string>() };
                    }
                    else if (item.contains("script") && item["script"].is_string())
                    {
                        script = root_py;
                        arguments = { cwp_path.string(), target_prefix.string() };
                        auto tmp = util::split(item["script"].get<std::string>(), " ");
                        std::copy(tmp.begin(), tmp.end(), back_inserter(arguments));
                        extend_script_args(item, arguments);
                    }
                    else if (item.contains("system") && item["system"].is_string())
                    {
                        auto tmp = util::split(item["system"].get<std::string>(), " ");
                        script = tmp[0];
                        if (tmp.size() > 1)
                        {
                            std::copy(tmp.begin() + 1, tmp.end(), back_inserter(arguments));
                        }
                        extend_script_args(item, arguments);
                    }
                    else
                    {
                        LOG_ERROR << "Unknown shortcut type found in " << json_file;
                        throw std::runtime_error("Unknown shortcut type.");
                    }
                }
                else  // MenuInstVersion::Version2
                {
                    if (!item.contains("platforms") || !item["platforms"].is_object())
                    {
                        LOG_DEBUG << "Skipping v2 menu item without valid 'platforms' in "
                                  << json_file.string();
                        continue;
                    }
                    const auto& platforms = item["platforms"];
                    if (!platforms.contains("win") || !platforms["win"].is_object())
                    {
                        LOG_DEBUG << "Skipping menu item not enabled for Windows in "
                                  << json_file.string();
                        continue;
                    }

                    const auto& win = platforms["win"];
                    for (const auto& key : override_keys)
                    {
                        // Override top-level elements with platforms content if not null
                        if (win.contains(key) && !win[key].is_null())
                        {
                            item[key] = win[key];
                        }
                    }

                    if (!item.contains("name"))
                    {
                        throw std::runtime_error("Menu item missing 'name' in " + json_file.string());
                    }

                    const auto& name_node = item["name"];
                    if (name_node.is_string())
                    {
                        name = name_node.get<std::string>();
                    }
                    else if (name_node.is_object())
                    {
                        constexpr std::string_view base_name_key = "target_environment_is_base";
                        constexpr std::string_view non_base_name_key = "target_environment_is_not_base";

                        const std::string selected_key{ target_is_base ? base_name_key
                                                                       : non_base_name_key };

                        const auto it = name_node.find(selected_key);
                        if (it == name_node.end() || !it->is_string())
                        {
                            throw std::runtime_error(
                                "Menu item 'name' object missing valid '" + selected_key + "' in "
                                + json_file.string()
                            );
                        }
                        name = it->get<std::string>();
                    }
                    else
                    {
                        throw std::runtime_error(
                            "Menu item 'name' must be a string or object in " + json_file.string()
                        );
                    }

                    if (name.empty())
                    {
                        throw std::runtime_error(
                            "Menu item 'name' must not be empty in " + json_file.string()
                        );
                    }

                    workdir = item.value("working_dir", "");

                    if (!item.contains("command") || !item["command"].is_array()
                        || item["command"].empty())
                    {
                        throw std::runtime_error(
                            "Menu item missing valid 'command' array in " + json_file.string()
                        );
                    }
                    // cf.
                    // https://conda.github.io/menuinst/defining-shortcuts/#migrating-pywscript-and-pyscript-to-menuinst-v2
                    for (const auto& el : item["command"])
                    {
                        if (!el.is_string())
                        {
                            throw std::runtime_error(
                                "Menu item 'command' elements must be strings in " + json_file.string()
                            );
                        }
                        arguments.push_back(el.get<std::string>());
                    }
                    script = arguments[0];
                }

                std::string full_name = util::concat(name, name_suffix);
                fs::u8path dst = target_dir / (full_name + ".lnk");
                fs::u8path iconpath = item.value("icon", "");
                if (remove == false)
                {
                    std::string argstring;
                    std::string lscript = util::to_lower(script.string());

                    for (auto& arg : arguments)
                    {
                        if (arg.size() >= 1 && arg[0] != '/')
                        {
                            util::replace_all(arg, "/", "\\");
                        }
                    }

                    if (lscript.find("cmd.exe") != std::string::npos
                        || lscript.find("%comspec%") != std::string::npos)
                    {
                        if (arguments.size() > 1)
                        {
                            if (util::to_upper(arguments[0]) == "/K"
                                || util::to_upper(arguments[0]) == "/C")
                            {
                            }
                        }
                        argstring = quote_for_shell(arguments, "cmdexe");
                    }
                    else
                    {
                        argstring = quote_for_shell(arguments, "");
                    }

                    if (workdir.string().size())
                    {
                        if (!(fs::exists(workdir) && fs::is_directory(workdir)))
                        {
                            fs::create_directories(workdir);
                        }
                    }
                    else
                    {
                        workdir = "%HOMEPATH%";
                    }

                    mamba::win::create_shortcut(script, full_name, dst, argstring, workdir, iconpath, 0);
                }
                else
                {
                    mamba::win::remove_shortcut(dst);
                }
            }
#endif
        }
    }

    void
    remove_menu_from_json(const fs::u8path& json_file, const TransactionContext& transaction_context)
    {
        try
        {
            create_remove_shortcut_impl(json_file, transaction_context, true);
        }
        catch (const std::exception& e)
        {
            LOG_ERROR << "Removal of shortcut was not successful " << e.what();
        }
    }

    void
    create_menu_from_json(const fs::u8path& json_file, const TransactionContext& transaction_context)
    {
        try
        {
            create_remove_shortcut_impl(json_file, transaction_context, false);
        }
        catch (const std::exception& e)
        {
            LOG_ERROR << "Creation of shortcut was not successful " << e.what();
        }
    }
}
