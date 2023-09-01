// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <string>

#ifdef _WIN32
#include <shlobj.h>
#include <windows.h>
#endif

#include "mamba/core/context.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/transaction_context.hpp"
#include "mamba/util/string.hpp"


namespace mamba
{
    namespace detail
    {
        std::string get_formatted_env_name(const fs::u8path& target_prefix)
        {
            std::string name = env_name(target_prefix);
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
         *   work_dir: workdir for the executable
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
            LOG_DEBUG << "Creating shortcut with "
                      << "\n  Path: " << path << "\n  Description: " << description
                      << "\n  Filename: " << filename << "\n  Arguments: " << arguments
                      << "\n  Workdir: " << work_dir << "\n  Icon Path: " << icon_path
                      << "\n  Icon Index: " << icon_index;
            try
            {
                hres = CoInitialize(nullptr);
                if (FAILED(hres))
                {
                    throw std::runtime_error("Could not initialize COM");
                }
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
                    throw std::runtime_error(util::concat(
                        "Failed to create shortcut: ",
                        filename.string(),
                        std::to_string(hres)
                    ));
                }
            }
            catch (const std::runtime_error& e)
            {
                if (pPersistFile)
                {
                    pPersistFile->Release();
                }
                if (pShellLink)
                {
                    pShellLink->Release();
                }
                CoUninitialize();

                LOG_ERROR << e.what();
            }

            pPersistFile->Release();
            pShellLink->Release();

            CoUninitialize();
        }

        const std::map<std::string, KNOWNFOLDERID> knownfolders = {
            { "programs", FOLDERID_Programs },       { "profile", FOLDERID_Profile },
            { "documents", FOLDERID_Documents },     { "roamingappdata", FOLDERID_RoamingAppData },
            { "programdata", FOLDERID_ProgramData }, { "localappdata", FOLDERID_LocalAppData },
        };

        fs::u8path get_folder(const std::string& id)
        {
            wchar_t* localAppData;
            HRESULT hres;

            hres = SHGetKnownFolderPath(knownfolders.at(id), KF_FLAG_DONT_VERIFY, nullptr, &localAppData);

            if (FAILED(hres))
            {
                throw std::runtime_error("Could not retrieve known folder");
            }

            std::wstring tmp(localAppData);
            fs::u8path res(tmp);
            CoTaskMemFree(localAppData);
            return res;
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


    void replace_variables(std::string& text, TransactionContext* transaction_context)
    {
        auto& ctx = mamba::Context::instance();
        fs::u8path root_prefix = ctx.prefix_params.root_prefix;

        fs::u8path target_prefix;
        std::string py_ver;
        if (transaction_context)
        {
            target_prefix = transaction_context->target_prefix;
            py_ver = transaction_context->python_version;
        }

        std::string distribution_name = root_prefix.filename().string();
        if (distribution_name.size() > 1)
        {
            distribution_name[0] = util::to_upper(distribution_name[0]);
        }

        auto to_forward_slash = [](const fs::u8path& p)
        {
            std::string ps = p.string();
            util::replace_all(ps, "\\", "/");
            return ps;
        };

        auto platform_split = util::split(ctx.platform, "-");
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
            { "${ENV_NAME}", detail::get_formatted_env_name(target_prefix) },
            { "${PLATFORM}", platform_bitness },
        };

#ifdef _WIN32
        vars["${PERSONALDIR}"] = to_forward_slash(win::get_folder("documents"));
        vars["${USERPROFILE}"] = to_forward_slash(win::get_folder("profile"));
#endif

        for (auto& [key, val] : vars)
        {
            util::replace_all(text, key, val);
        }
    }

    namespace
    {
        void create_remove_shortcut_impl(
            const fs::u8path& json_file,
            TransactionContext* transaction_context,
            [[maybe_unused]] bool remove
        )
        {
            std::string json_content = mamba::read_contents(json_file);
            replace_variables(json_content, transaction_context);
            auto j = nlohmann::json::parse(json_content);

            std::string menu_name = j.value("menu_name", "Mamba Shortcuts");

            std::string name_suffix;
            std::string e_name = detail::get_formatted_env_name(transaction_context->target_prefix);

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

            auto& ctx = mamba::Context::instance();
            const fs::u8path root_prefix = ctx.prefix_params.root_prefix;
            const fs::u8path target_prefix = ctx.prefix_params.target_prefix;

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

            fs::u8path target_dir = win::get_folder("programs") / menu_name;
            if (!fs::exists(target_dir))
            {
                fs::create_directories(target_dir);
            }

            auto extend_script_args = [](const auto& item, auto& arguments)
            {
                // this is pretty bad ...
                if (item.contains("scriptargument"))
                {
                    arguments.push_back(item["scriptargument"]);
                }

                if (item.contains("scriptarguments"))
                {
                    std::vector<std::string> tmp_args = item["scriptarguments"];
                    for (auto& arg : tmp_args)
                    {
                        arguments.push_back(arg);
                    }
                }
            };

            for (auto& item : j["menu_items"])
            {
                std::string name = item["name"];
                std::string full_name = util::concat(name, name_suffix);

                std::vector<std::string> arguments;
                fs::u8path script;
                if (item.contains("pywscript"))
                {
                    script = root_pyw;
                    arguments = cwp_pyw_args;
                    auto tmp = util::split(item["pywscript"], " ");
                    std::copy(tmp.begin(), tmp.end(), back_inserter(arguments));
                }
                else if (item.contains("pyscript"))
                {
                    script = root_py;
                    arguments = cwp_py_args;
                    auto tmp = util::split(item["pyscript"], " ");
                    std::copy(tmp.begin(), tmp.end(), back_inserter(arguments));
                }
                else if (item.contains("webbrowser"))
                {
                    script = root_pyw;
                    arguments = { "-m", "webbrowser", "-t", item["webbrowser"] };
                }
                else if (item.contains("script"))
                {
                    script = root_py;
                    arguments = { cwp_path.string(), target_prefix.string() };
                    auto tmp = util::split(item["script"], " ");
                    std::copy(tmp.begin(), tmp.end(), back_inserter(arguments));
                    extend_script_args(item, arguments);
                }
                else if (item.contains("system"))
                {
                    auto tmp = util::split(item["system"], " ");
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

                fs::u8path dst = target_dir / (full_name + ".lnk");
                fs::u8path workdir = item.value("workdir", "");
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

    void remove_menu_from_json(const fs::u8path& json_file, TransactionContext* context)
    {
        try
        {
            create_remove_shortcut_impl(json_file, context, true);
        }
        catch (const std::exception& e)
        {
            LOG_ERROR << "Removal of shortcut was not successful " << e.what();
        }
    }

    void create_menu_from_json(const fs::u8path& json_file, TransactionContext* context)
    {
        try
        {
            create_remove_shortcut_impl(json_file, context, false);
        }
        catch (const std::exception& e)
        {
            LOG_ERROR << "Creation of shortcut was not successful " << e.what();
        }
    }
}
