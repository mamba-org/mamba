#include <map>
#include <fstream>
#include <iostream>
#include <sstream>

#include "solver.hpp"

#ifndef _MSC_VER
    #include <string_view>
    using std::string_view;
#else
    #include "thirdparty/string_view.hpp"
    using nonstd::string_view;
#endif

extern "C"
{
    #include "solv/pool.h"
    #include "solv/repo.h"
    #include "solv/queue.h"
    #include "solv/solver.h"
    #include "solv/solverdebug.h"

    #include "solv/repo_solv.h"
    #include "solv/conda.h"
    #include "solv/repo_conda.h"

    #include "common_write.c"
}

#define PRINTS(stuff)            \
if (!quiet)                      \
    std::cout << stuff << "\n";  \

static Pool* global_pool;

auto find_on_level(const string_view& substr, const std::string& search_string,
                   const char dstart = '{', const char dend = '}')
{
    std::size_t lvl = 1;
    auto begin = substr.begin();

    while (lvl != 0 && begin != substr.end())
    {
        if (*begin == '{')
        {
            lvl += 1;
        }
        if (*begin == '}')
        {
            lvl -= 1;
        }
        if (lvl == 1)
        {
            if (std::equal(begin, begin + search_string.size(), search_string.begin()))
            {
                break;
            }
        }
        begin++;
    }

    if (lvl == 0)
    {
        throw std::runtime_error("Did not find key as expected!");
    }

    // find begin
    while (*begin != '{')
    {
        begin++;
    }

    auto end = begin + 1;

    lvl = 1;
    while (lvl != 0 && end != substr.end())
    {
        if (*end == '{') ++lvl;
        if (*end == '}') --lvl;
        ++end;
    }
    if (end == substr.end() && *end != '}')
    {
        throw std::runtime_error("Package metadata appears incomplete");
    }
    return std::string(begin, end);
}

std::string get_package_info(const std::string& json, const std::string& pkg_key)
{
    auto pos = json.find("\"packages\"");
    if (pos == std::string::npos) { throw std::runtime_error("Could not find packages key."); }
    auto it = json.begin() + pos;
    while (*it != '{') ++it;
    ++it;

    std::string pkg_key_quoted = "\"" + pkg_key + "\"";
    std::string result = find_on_level(string_view(&(*it), std::size_t(json.size() - std::distance(json.begin(), it))), pkg_key_quoted);
    return result;
}

std::tuple<std::vector<std::tuple<std::string, std::string, std::string>>,
           std::vector<std::tuple<std::string, std::string>>>
solve(std::vector<std::tuple<std::string, std::string, int, int>> repos,
           std::string installed,
           std::vector<std::string> jobs,
           std::vector<std::pair<int, int>> solver_options,
           int solvable_flags,
           bool strict_priority,
           bool quiet,
           int debug_level,
           const std::string& mamba_version)
{
    Pool* pool = pool_create();
    pool_setdisttype(pool, DISTTYPE_CONDA);

    pool_setdebuglevel(pool, debug_level);

    global_pool = pool;

    std::map<std::string, std::map<Id, std::string>> repo_to_file_map;
    std::map<std::string, std::string> chan_to_json;

    FILE *fp;
    if (installed.size())
    {
        Repo* repo = repo_create(pool, "installed");
        repo_to_file_map["installed"] = std::map<Id, std::string>();
        pool_set_installed(pool, repo);
        fp = fopen(installed.c_str(), "r");
        if (fp)
        {
            repo_add_conda(repo, fp, 0);
        }
        else
        {
            throw std::runtime_error("Installed packages file could not be read.");
        }
        repo_internalize(repo);
        fclose(fp);
    }

    string_view last_repo;
    for (auto& fn : repos)
    {
        const std::string& repo_name = std::get<0>(fn);
        std::string& repo_json_file = std::get<1>(fn);

        repo_to_file_map[repo_name] = std::map<Id, std::string>();

        Repo* repo = repo_create(pool, repo_name.c_str());
        repo->priority = std::get<2>(fn);
        repo->subpriority = std::get<3>(fn);

        fp = fopen(repo_json_file.c_str(), "r");
        std::string ending = "_" + mamba_version + ".solv";
        if (std::equal(repo_json_file.end() - ending.size(), repo_json_file.end(), ending.begin()))
        {
            repo_add_solv(repo, fp, 0);
            // PRINT("loading from solv " << repo_json_file);
            auto json_name = repo_json_file.substr(0, repo_json_file.size() - ending.size());
            json_name += std::string(".json");
            repo_json_file = json_name;
            repo_internalize(repo);
        }
        else if (fp)
        {
            repo_add_conda(repo, fp, 0);
            repo_internalize(repo);

            // disabling solv caching for now on Windows
            #if !defined(WIN32)
            std::string solv_name = repo_json_file.substr(0, repo_json_file.size() - 5);
            solv_name += ending;
            auto sfile = fopen(solv_name.c_str(), "w");
            if (sfile)
            {
                tool_write(repo, sfile);
                fclose(sfile);
            }
            #endif
        }
        else
        {
            throw std::runtime_error("File could not be read.");
        }
        fclose(fp);

        std::ifstream fistream(repo_json_file);
        std::stringstream buffer;
        buffer << fistream.rdbuf();
        chan_to_json.emplace(repo_name, buffer.str());

        PRINTS(repo->nsolvables << " packages in " << repo_name);
    }

    pool_createwhatprovides(global_pool);

    Solver* solvy = solver_create(global_pool);
    for (auto& option : solver_options)
    {
        solver_set_flag(solvy, option.first, option.second);
    }

    Queue q;
    queue_init(&q);

    for (const auto& job : jobs)
    {
        Id inst_id = pool_conda_matchspec(pool, job.c_str());
        queue_push2(&q, solvable_flags | SOLVER_SOLVABLE_PROVIDES, inst_id);
    }

    solver_solve(solvy, &q);

    Transaction* transy = solver_create_transaction(solvy);
    int cnt = solver_problem_count(solvy);
    Queue problem_queue;
    queue_init(&problem_queue);

    if (cnt > 0)
    {
        std::stringstream problems;
        for (int i = 1; i <= cnt; i++)
        {
            queue_push(&problem_queue, i);
            problems << "Problem: " << solver_problem2str(solvy, i) << "\n";
        }
        throw mamba_error("Encountered problems while solving.\n" + problems.str());
    }

    queue_free(&problem_queue);

    // DEBUG printout
    // transaction_print(transy);

    std::vector<std::tuple<std::string, std::string, std::string>> to_install_structured; 
    std::vector<std::tuple<std::string, std::string>> to_remove_structured; 

    Queue classes, pkgs;

    queue_init(&classes);
    queue_init(&pkgs);
    int mode = SOLVER_TRANSACTION_SHOW_OBSOLETES |
               SOLVER_TRANSACTION_OBSOLETE_IS_UPGRADE;
    transaction_classify(transy, mode, &classes);

    Id cls;
    std::string location;
    const char* mediafile;
    for (int i = 0; i < classes.count; i += 4)
    {
        cls = classes.elements[i];
        cnt = classes.elements[i + 1];

        transaction_classify_pkgs(transy, mode, cls, classes.elements[i + 2],
                                  classes.elements[i + 3], &pkgs);
        for (int j = 0; j < pkgs.count; j++)
        {
            Id p = pkgs.elements[j];
            Solvable *s = pool->solvables + p;
            Solvable *s2;

            switch (cls)
            {
                case SOLVER_TRANSACTION_DOWNGRADED:
                case SOLVER_TRANSACTION_UPGRADED:
                case SOLVER_TRANSACTION_CHANGED:
                    mediafile = solvable_lookup_str(s, SOLVABLE_MEDIAFILE);
                    to_remove_structured.emplace_back(s->repo->name, mediafile);

                    s2 = pool->solvables + transaction_obs_pkg(transy, p);
                    mediafile = solvable_lookup_str(s2, SOLVABLE_MEDIAFILE);
                    to_install_structured.emplace_back(s2->repo->name, mediafile, "");
                    break;
                case SOLVER_TRANSACTION_VENDORCHANGE:
                case SOLVER_TRANSACTION_ARCHCHANGE:
                    std::cout << "CASE NOT HANDLED. " << cls << std::endl;
                    break;
                case SOLVER_TRANSACTION_ERASE:
                    mediafile = solvable_lookup_str(s, SOLVABLE_MEDIAFILE);
                    to_remove_structured.emplace_back(s->repo->name, mediafile);
                    break;
                case SOLVER_TRANSACTION_INSTALL:
                    mediafile = solvable_lookup_str(s, SOLVABLE_MEDIAFILE);

                    to_install_structured.emplace_back(s->repo->name, mediafile, "");
                    break;
                default:
                    std::cout << "CASE NOT HANDLED. " << cls << std::endl;
                    break;
            }
        }
    }
    queue_free(&classes);
    queue_free(&pkgs);

    for (auto& el : to_install_structured)
    {
        auto& json = chan_to_json[std::get<0>(el)];
        std::get<2>(el) = get_package_info(json, std::get<1>(el));
    }

    transaction_free(transy);
    // pool free also frees all repos etc.
    pool_free(pool);

    return std::make_tuple(to_install_structured, to_remove_structured);
}