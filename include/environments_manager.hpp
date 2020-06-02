#ifndef MAMBA_ENVIRONMENT_MANAGER
#define MAMBA_ENVIRONMENT_MANAGER

#include <set>

#include "util.hpp"
#include "environment.hpp"
#include "output.hpp"

namespace mamba
{
    const std::string PREFIX_MAGIC_FILE = "conda-meta/history";

    inline bool is_conda_environment(const fs::path& prefix)
    {
        return fs::exists(prefix / PREFIX_MAGIC_FILE);
    }

    class EnvironmentsManager
    {
    public:

        void register_env(const fs::path& location)
        {
            fs::path env_txt_file = get_environments_txt_file(env::home_directory());
            fs::path final_location = fs::absolute(location);
            fs::path folder = final_location.parent_path();

            if (!fs::exists(env_txt_file.parent_path()))
            {
                try
                {
                    fs::create_directories(env_txt_file.parent_path());
                } catch (...) {}
            }

            std::string final_location_string = remove_trailing_slash(final_location);
            if (final_location_string.find("placehold_pl") != std::string::npos ||
                final_location_string.find("skeleton_") != std::string::npos)
            {
                return;
            }

            auto lines = read_lines(env_txt_file);

            for (auto& l : lines)
            {
                if (l == final_location_string) return;
            }

            std::ofstream out(env_txt_file, std::ios::app);
            out << final_location_string << std::endl;
            if (out.bad())
            {
                if (errno == EACCES || errno == EROFS || errno == ENOENT)
                {
                    LOG_ERROR << "Could not register environment. " << env_txt_file << " not writeable or missing?";
                }
                else
                {
                    throw std::system_error(errno, std::system_category(), "failed to open " + env_txt_file.string());
                }
            }
        }

        void unregister_env(const fs::path& location)
        {
            if (fs::exists(location) && fs::is_directory(location))
            {
                fs::path meta_dir = location / "conda-meta";
                if (fs::exists(meta_dir) && fs::is_directory(meta_dir))
                {
                    std::size_t count = 0;
                    for (auto& _ : fs::directory_iterator(meta_dir))
                    {
                        (void)_;
                        ++count;
                    }
                    if (count > 1)
                    {
                        // if files left other than `conda-meta/history` do not unregister
                        return;
                    }
                }
            }

            clean_environments_txt(get_environments_txt_file(env::home_directory()), location);
        }

        std::set<fs::path> list_all_known_prefixes()
        {
            std::vector<fs::path> search_dirs;

            // TODO
            // if (is_admin())
            // {
            //     if (on_win)
            //     {
            //         fs::path home_dir_dir = env::home_directory().parent_path();
            //         search_dirs = tuple(join(home_dir_dir, d) for d in listdir(home_dir_dir))
            //     }
            //     else
            //     {
            //         from pwd import getpwall
            //         search_dirs = tuple(pwentry.pw_dir for pwentry in getpwall()) or (expand('~'),)
            //     }
            // }
            // else
            {
                search_dirs = std::vector<fs::path> { env::home_directory() };
            }

            std::set<fs::path> all_env_paths;

            for (auto& d : search_dirs)
            {
                auto f = get_environments_txt_file(d);
                if (fs::exists(f))
                {
                    for (auto& env_path : clean_environments_txt(f, fs::path()))
                    {
                        all_env_paths.insert(env_path);
                    }
                }
            }
            for (auto& d : Context::instance().envs_dirs)
            {
                if (fs::exists(d) && fs::is_directory(d))
                {
                    for (auto& potential_env : fs::directory_iterator(d))
                    {
                        if (is_conda_environment(potential_env))
                        {
                            all_env_paths.insert(potential_env);
                        }
                    }
                }
            }
            all_env_paths.insert(Context::instance().root_prefix);
            return all_env_paths;
        }

    private:

        std::set<std::string>
        clean_environments_txt(const fs::path& env_txt_file, const fs::path& location)
        {
            if (!fs::exists(env_txt_file))
                return {};

            fs::path abs_loc = fs::absolute(location);

            std::vector<std::string> lines = read_lines(env_txt_file);
            std::set<std::string> final_lines;
            for (auto& l : lines)
            {
                if (l != abs_loc && is_conda_environment(l))
                {
                    final_lines.insert(l);
                }
            }
            if (final_lines.size() != lines.size())
            {
                std::ofstream out(env_txt_file);
                for (auto& l : final_lines)
                {
                    out << remove_trailing_slash(l) << std::endl;
                }
                if (out.bad())
                {
                    LOG_ERROR << "failed to clean " + env_txt_file.string();
                }
            }
            return final_lines;
        }

        std::string remove_trailing_slash(std::string p)
        {
            if (p[p.size() - 1] == '/' || p[p.size() - 1] == '\\')
            {
                p.pop_back();
            }
            return p;
        }

        fs::path get_environments_txt_file(const fs::path& home) const
        {
            return home / ".conda" / "environments.txt";
        }
    };
}


#endif
