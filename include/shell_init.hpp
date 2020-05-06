#include <string>
#include <regex>

#include "output.hpp"
#include "util.hpp"
#include "thirdparty/filesystem.hpp"

#include "thirdparty/termcolor.hpp"

namespace fs = ghc::filesystem;

#ifndef _WIN32
#include <wordexp.h>
#endif

namespace mamba
{
    #ifndef _WIN32
    fs::path expand_path(const fs::path& path)
    {
        wordexp_t w{};
        std::unique_ptr<wordexp_t, void(*)(wordexp_t*)> hold{&w, ::wordfree};
        ::wordexp(path.c_str(), &w, 0);
        if (w.we_wordc != 1)
            throw std::runtime_error("Cannot expand path: " + path.string());
        fs::path result = w.we_wordv[0];
        return result;
    }
    #endif

    // Heavily inspired by https://github.com/gpakosz/whereami/
    fs::path get_self_exe_path()
    {
        #ifdef _WIN32
        DWORD size;
        std::wstring buffer(MAX_PATH);
        size = GetModuleFileNameW(NULL, (wchar_t*) buffer.c_str(), (DWORD) buffer.size());
        if (size == 0)
        {
            throw std::runtime_error("Could find location of the mamba executable!")
        }
        else if (size == buffer.size())
        {
            DWORD new_size = size;
            do {
                new_size *= 2;
                buffer.reserver(new_size);
                size = GetModuleFileNameW(NULL, (wchar_t*) buffer.c_str(), (DWORD) buffer.size());
            } while (new_size == size);
        }
        return fs::absolute(buffer);
        #elif defined(__APPLE__)
        uint32_t size = PATH_MAX;
        std::string buffer(size);
        if (_NSGetExecutablePath((char*)buffer.c_str(), &size) == -1)
        {
            buffer.reserve(size);
            if (!_NSGetExecutablePath((char*)buffer.c_str(), &size))
            {
                throw std::runtime_error("Couldn't find location the mamba executable!");
            }
        }
        return fs::absolute(buffer);
        #else
        return fs::read_symlink("/proc/self/exe");
        #endif
    }


    fs::path home_directory()
    {
        return expand_path("~");
    }

    std::regex CONDA_INITIALIZE_RE_BLOCK("# >>> mamba initialize >>>(?:\n|\r\n)?"
                                         "([\\s\\S]*?)"
                                         "# <<< mamba initialize <<<(?:\n|\r\n)?");

    std::regex CONDA_INITIALIZE_PS_RE_BLOCK("^#region mamba initialize(?:\n|\r\n)"
                                            "([\\s\\S]*?)"
                                            "#endregion(?:\n|\r\n)?");

    std::string rcfile_content(const fs::path& env_prefix, const std::string& shell, const fs::path& mamba_exe)
    {
        // todo use get bin dir here!
        #ifndef _WIN32
        fs::path env_bin = env_prefix / "bin";
        #else
        #warning "Not implemented yet!";
        #endif

        std::stringstream content;
        content << "# >>> mamba initialize >>>\n";
        content << "# !! Contents within this block are managed by 'mamba init' !!\n";
        content << "__mamba_setup=\"$(" << std::quoted(mamba_exe.string(), '\'') << " shell hook --shell "
                << shell << " --prefix " << std::quoted(env_prefix.string(), '\'') << " 2> /dev/null)\"\n";
        content << "if [ $? -eq 0 ]; then\n";
        content << "    eval \"$__mamba_setup\"\n";
        content << "else\n";
        content << "    if [ -f " << (env_prefix / "etc" / "profile.d" / "mamba.sh") << " ]; then\n";
        content << "        . " << (env_prefix / "etc" / "profile.d" / "mamba.sh") << "\n";
        content << "    else\n";
        content << "        export PATH=\"" << env_bin.c_str() << ":$PATH\"\n";
        content << "    fi\n";
        content << "fi\n";
        content << "unset __mamba_setup\n";
        content << "# <<< mamba initialize <<<\n";

        return content.str();
    }

    bool modify_rc_file(const fs::path& file_path, const fs::path& conda_prefix,
                        const std::string& shell, const fs::path& mamba_exe)
    {
        Console::stream() << "Modifiying RC file " << file_path
                          << "\nGenerating config for root prefix " << conda_prefix
                          << "\nSetting mamba executable to: " << mamba_exe;

        std::string rc_content = get_file_contents(file_path);
        std::string original_content = rc_content;
        std::string conda_init_content = rcfile_content(conda_prefix, shell, mamba_exe);

        Console::stream() << "Adding (or replacing) the following in your " <<file_path << " file\n"
                          << termcolor::colorize << termcolor::green << conda_init_content << termcolor::reset;

        std::string result = std::regex_replace(
            rc_content,
            CONDA_INITIALIZE_RE_BLOCK,
            conda_init_content
        );

        if (result.find("# >>> mamba initialize >>>") == result.npos)
        {
            std::ofstream rc_file(file_path, std::ios::app);
            rc_file << "\n" << conda_init_content;
        }
        else
        {
            std::ofstream rc_file(file_path, std::ios::out);
            rc_file << result;
        }
        return true;
    }

    void init_shell(const std::string& shell, const fs::path& conda_prefix)
    {
        auto mamba_exe = get_self_exe_path();
        fs::path home = home_directory();
        if (shell == "bash")
        {
            fs::path bashrc_path = (on_mac || on_win) ? home / ".bash_profile" : home / ".bashrc";
            modify_rc_file(bashrc_path, conda_prefix, shell, mamba_exe);
        }
        else
        {
            throw std::runtime_error("Support for other shells not yet implemented.");
        }
    }
}