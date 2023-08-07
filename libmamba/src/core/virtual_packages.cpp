// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/core/context.hpp"
#include "mamba/core/environment.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/util.hpp"
#include "mamba/core/util_os.hpp"
#include "mamba/core/virtual_packages.hpp"
#include "mamba/util/string.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <regex>
#include <vector>

#include <reproc++/run.hpp>

namespace mamba
{
    namespace detail
    {
        std::string glibc_version()
        {
            auto override_version = env::get("CONDA_OVERRIDE_GLIBC");
            if (override_version)
            {
                return override_version.value();
            }

            if (!on_linux)
            {
                return "";
            }

            const char* version = "";
#ifdef __linux__
            std::vector<char> ver;
            const size_t n = confstr(_CS_GNU_LIBC_VERSION, NULL, size_t{ 0 });

            if (n > 0)
            {
                ver.assign(n, '\n');
                confstr(_CS_GNU_LIBC_VERSION, ver.data(), n);
                version = ver.data();
            }
#endif
            return std::string(util::strip(version, "glibc "));
        }

        std::string cuda_version()
        {
            LOG_DEBUG << "Loading CUDA virtual package";

            auto override_version = env::get("CONDA_OVERRIDE_CUDA");
            if (override_version)
            {
                return override_version.value();
            }

            std::string out, err;
            std::vector<std::string> args = { "nvidia-smi", "--query", "-u", "-x" };
            auto [status, ec] = reproc::run(
                args,
                reproc::options{},
                reproc::sink::string(out),
                reproc::sink::string(err)
            );

            if (ec)
            {
                out = "";
            }

            if (ec && on_win)
            {
                // Windows fallback
                bool may_exist = false;
                std::string path = env::get("PATH").value_or("");
                std::vector<std::string> paths = util::split(path, env::pathsep());

                for (auto& p : paths)
                {
                    if (fs::exists(fs::u8path(p) / "nvcuda.dll"))
                    {
                        may_exist = true;
                        break;
                    }
                }

                std::string base = "C:\\Windows\\System32\\DriverStore\\FileRepository";
                if (may_exist)
                {
                    for (auto& p : fs::directory_iterator(base))
                    {
                        if (util::starts_with(p.path().filename().string(), "nv")
                            && fs::exists(p.path() / "nvidia-smi.exe"))
                        {
                            std::string f = (p.path() / "nvidia-smi.exe").string();
                            LOG_DEBUG << "Found nvidia-smi in: " << f;
                            std::vector<std::string> command = { f, "--query", "-u", "-x" };
                            auto [_ /*cmd_status*/, cmd_ec] = reproc::run(
                                command,
                                reproc::options{},
                                reproc::sink::string(out),
                                reproc::sink::string(err)
                            );

                            if (!cmd_ec)
                            {
                                break;
                            }
                        }
                    }
                }
            }

            if (out.empty())
            {
                LOG_DEBUG << "Could not find CUDA version by calling 'nvidia-smi' (skipped)\n";
                return "";
            }

            std::regex re("<cuda_version>(.*)<\\/cuda_version>");
            std::smatch m;

            if (std::regex_search(out, m, re))
            {
                if (m.size() == 2)
                {
                    std::ssub_match cuda_version = m[1];
                    LOG_DEBUG << "CUDA driver version found: " << cuda_version;
                    return cuda_version.str();
                }
            }

            LOG_DEBUG << "CUDA not found";
            return "";
        }

        PackageInfo make_virtual_package(
            const std::string& name,
            const std::string& version,
            const std::string& build_string
        )
        {
            PackageInfo res(name);
            res.version = version.size() ? version : "0";
            res.build_string = build_string.size() ? build_string : "0";
            res.build_number = 0;
            res.channel = "@";
            res.subdir = Context::instance().platform;
            res.md5 = "12345678901234567890123456789012";
            res.fn = name;
            return res;
        }

        std::vector<PackageInfo> dist_packages()
        {
            LOG_DEBUG << "Loading distribution virtual packages";

            std::vector<PackageInfo> res;
            auto platform = Context::instance().platform;
            auto split_platform = util::split(platform, "-", 1);

            if (split_platform.size() != 2)
            {
                LOG_ERROR << "Platform is ill-formed, expected <os>-<arch> in: '" + platform + "'";
                return res;
            }
            std::string os = split_platform[0];
            std::string arch = split_platform[1];

            if (os == "win")
            {
                res.push_back(make_virtual_package("__win"));
            }
            if (os == "linux")
            {
                res.push_back(make_virtual_package("__unix"));

                std::string linux_ver = linux_version();
                if (!linux_ver.empty())
                {
                    res.push_back(make_virtual_package("__linux", linux_ver));
                }
                else
                {
                    LOG_WARNING << "linux version not found (virtual package skipped)";
                }

                std::string libc_ver = detail::glibc_version();
                if (!libc_ver.empty())
                {
                    res.push_back(make_virtual_package("__glibc", libc_ver));
                }
                else
                {
                    LOG_WARNING << "glibc version not found (virtual package skipped)";
                }
            }
            if (os == "osx")
            {
                res.push_back(make_virtual_package("__unix"));

                std::string osx_ver = macos_version();
                if (!osx_ver.empty())
                {
                    res.push_back(make_virtual_package("__osx", osx_ver));
                }
                else
                {
                    LOG_WARNING << "osx version not found (virtual package skipped)";
                }
            }

            if (arch == "64")
            {
                arch = "x86_64";
            }
            else if (arch == "32")
            {
                arch = "x86";
            }
            res.push_back(make_virtual_package("__archspec", "1", arch));

            return res;
        }
    }

    std::vector<PackageInfo> get_virtual_packages()
    {
        LOG_DEBUG << "Loading virtual packages";
        auto res = detail::dist_packages();

        auto cuda_ver = detail::cuda_version();
        if (!cuda_ver.empty())
        {
            res.push_back(detail::make_virtual_package("__cuda", cuda_ver));
        }

        return res;
    }
}
