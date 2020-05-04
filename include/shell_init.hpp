#include <string>
#include <regex>

#include "util.hpp"
#include "thirdparty/filesystem.hpp"
namespace fs = ghc::filesystem;

#ifndef _WIN32
#include <wordexp.h>
#endif

namespace mamba
{
    #ifndef _WIN32
    std::string expand_path(std::string path)
    {
        wordexp_t w{};
        std::unique_ptr<wordexp_t, void(*)(wordexp_t*)> hold{&w, ::wordfree};
        ::wordexp(path.c_str(), &w, 0);
        if (w.we_wordc != 1)
            throw std::runtime_error("Cannot expand path: " + path);
        path = w.we_wordv[0];
        return path;
    }
    #endif

    fs::path home_directory()
    {
        return expand_path("~");
    }

    std::regex CONDA_INITIALIZE_RE_BLOCK("# >>> conda initialize >>>(?:\n|\r\n)?"
                                         "([\\s\\S]*?)"
                                         "# <<< conda initialize <<<(?:\n|\r\n)?");

    std::regex CONDA_INITIALIZE_PS_RE_BLOCK("^#region conda initialize(?:\n|\r\n)"
                                            "([\\s\\S]*?)"
                                            "#endregion(?:\n|\r\n)?");

    std::string rcfile_content(const fs::path& conda_prefix, const std::string& shell)
    {
        // todo use get bin dir here!
        #ifndef _WIN32
        fs::path conda_exe = conda_prefix / "bin" / "conda";
        fs::path conda_bin = conda_prefix / "bin";
        #else
        #warning "Not implemented yet!";
        #endif

        std::stringstream content;
        content << "\n# >>> conda initialize >>>\n";
        content << "# !! Contents within this block are managed by 'conda init' !!\n";
        content << "__conda_setup=\"$('" << conda_exe.c_str() << "' 'shell." << shell << "' 'hook' 2> /dev/null)\"\n";
        content << "if [ $? -eq 0 ]; then\n";
        content << "    eval \"$__conda_setup\"\n";
        content << "else\n";
        content << "    if [ -f " << (conda_prefix / "etc" / "profile.d" / "conda.sh") << " ]; then\n";
        content << "        . " << (conda_prefix / "etc" / "profile.d" / "conda.sh") << "\n";
        content << "    else\n";
        content << "        export PATH=\"" << conda_bin.c_str() << ":$PATH\"\n";
        content << "    fi\n";
        content << "fi\n";
        content << "unset __conda_setup\n";
        content << "# <<< conda initialize <<<\n";

        return content.str();
    }

    bool modify_rc_file(const fs::path& file_path, const fs::path& conda_prefix, const std::string& shell)
    {
        std::string rc_content = get_file_contents(file_path);
        std::string original_content = rc_content;
        std::string conda_init_content = rcfile_content(conda_prefix, shell);

        rc_content = std::regex_replace(
            rc_content,
            CONDA_INITIALIZE_RE_BLOCK,
            conda_init_content
        );

        if (rc_content.find("# >>> conda initialize >>>") == rc_content.npos)
        {
            std::ofstream rc_file(file_path, std::ios::app);
            rc_file << conda_init_content;
        }
        return true;
    }

//     conda_init_comment = "# commented out by conda initialize"

//     if reverse:
//         # uncomment any lines that were commented by prior conda init run
//         rc_content = re.sub(
//             r"#\s(.*?)\s*{}".format(conda_init_comment),
//             r"\1",
//             rc_content,
//             flags=re.MULTILINE,
//         )

//         # remove any conda init sections added
//         rc_content = re.sub(
//             r"^\s*" + CONDA_INITIALIZE_RE_BLOCK,
//             "",
//             rc_content,
//             flags=re.DOTALL | re.MULTILINE
//         )
//     else:
//         if not on_win:
//             rc_content = re.sub(
//                 r"^[ \t]*?(export PATH=[\'\"].*?%s\/bin:\$PATH[\'\"])"
//                 r"" % basename(conda_prefix),
//                 r"# \1  {}".format(conda_init_comment),
//                 rc_content,
//                 flags=re.MULTILINE,
//             )

//         rc_content = re.sub(
//             r"^[ \t]*[^#\n]?[ \t]*((?:source|\.) .*etc\/profile\.d\/conda\.sh.*?)\n"
//             r"(conda activate.*?)$",
//             r"# \1  {0}\n# \2  {0}".format(conda_init_comment),
//             rc_content,
//             flags=re.MULTILINE,
//         )
//         rc_content = re.sub(
//             r"^[ \t]*[^#\n]?[ \t]*((?:source|\.) .*etc\/profile\.d\/conda\.sh.*?)$",
//             r"# \1  {}".format(conda_init_comment),
//             rc_content,
//             flags=re.MULTILINE,
//         )

//         if on_win:
//             rc_content = re.sub(
//                 r"^[ \t]*^[ \t]*[^#\n]?[ \t]*((?:source|\.) .*Scripts[/\\]activate.*?)$",
//                 r"# \1  # commented out by conda initialize",
//                 rc_content,
//                 flags=re.MULTILINE,
//             )
//         else:
//             rc_content = re.sub(
//                 r"^[ \t]*^[ \t]*[^#\n]?[ \t]*((?:source|\.) .*bin/activate.*?)$",
//                 r"# \1  # commented out by conda initialize",
//                 rc_content,
//                 flags=re.MULTILINE,
//             )

//         replace_str = "__CONDA_REPLACE_ME_123__"
//         rc_content = re.sub(
//             CONDA_INITIALIZE_RE_BLOCK,
//             replace_str,
//             rc_content,
//             flags=re.MULTILINE,
//         )
//         # TODO: maybe remove all but last of replace_str, if there's more than one occurrence
//         rc_content = rc_content.replace(replace_str, conda_initialize_content)

//         if "# >>> conda initialize >>>" not in rc_content:
//             rc_content += '\n%s\n' % conda_initialize_content

//     if rc_content != rc_original_content:
//         if context.verbosity:
//             print('\n')
//             print(target_path)
//             print(make_diff(rc_original_content, rc_content))
//         if not context.dry_run:
//             with open(user_rc_path, 'w') as fh:
//                 fh.write(rc_content)
//         return Result.MODIFIED
//     else:
//         return Result.NO_CHANGE


    void init_shell(const std::string& shell, const fs::path& conda_prefix)
    {
        fs::path home = home_directory();
        if (shell == "bash")
        {
            fs::path bashrc_path = (on_mac || on_win) ? home / ".bash_profile" : home / ".bashrc";
            // plan.append({
            //     'function': init_sh_user.__name__,
            //     'kwargs': {
            //         'target_path': bashrc_path,
            //         'conda_prefix': conda_prefix,
            //         'shell': 'bash',
            //         'reverse': reverse,
            //     },
            // })
        }
    }
}