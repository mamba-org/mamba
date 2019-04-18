#include "thirdparty/simdjson/simdjson.h"

extern "C"
{
    #include "solv/pool.h"
    #include "solv/repo.h"
    #include "solv/queue.h"
    #include "solv/solver.h"
    #include "solv/solverdebug.h"

    #include "solv/conda.h"
    #include "solv/repo_conda.h"
}

static Pool* global_pool;

#include <iostream>
#include <map>

#include "solver.hpp"
// #include "parsing.hpp"
#include "json_helper.hpp"

auto get_package_info(ParsedJson::iterator &i, const std::string& key)
{
    if (!i.move_to_key("packages"))
    {
        throw std::runtime_error("Could not find packages key!");
    }
    if (!i.move_to_key(key.c_str()))
    {
        throw std::runtime_error("Could not find package " + key);
    }

    std::stringstream json;
    compute_dump(i, json);
    return json.str();
}

std::tuple<std::vector<std::tuple<std::string, std::string, std::string>>,
           std::vector<std::tuple<std::string, std::string>>>
solve(std::vector<std::tuple<std::string, std::string, int>> repos,
           std::string installed,
           std::vector<std::string> jobs,
           bool update,
           bool strict_priority)
{
    Pool* pool = pool_create();
    pool_setdisttype(pool, DISTTYPE_CONDA);
    // pool_setdebuglevel(pool, 2);

    global_pool = pool;

    std::map<std::string, std::map<Id, std::string>> repo_to_file_map;
    std::map<std::string, ParsedJson> chan_to_json;

    FILE *fp;
    if (installed.size())
    {
        Repo* repo = repo_create(pool, "installed");
        repo_to_file_map["installed"] = std::map<Id, std::string>();
        pool_set_installed(pool, repo);
        fp = fopen(installed.c_str(), "r");
        if (fp) {
            repo_add_conda(repo, fp, 0);
        } else {
            throw std::runtime_error("File could no tbe read.");
        }
        fclose(fp);
    }

    int priority = repos.size();
    std::string_view last_repo;
    for (auto& fn : repos)
    {
        const std::string& repo_name = std::get<0>(fn);
        const std::string& repo_json_file = std::get<1>(fn);

        std::string_view p = get_corpus(repo_json_file);

        repo_to_file_map[repo_name] = std::map<Id, std::string>();

        Repo* repo = repo_create(pool, repo_name.c_str());
        repo->priority = std::get<2>(fn);

        chan_to_json.emplace(repo_name, build_parsed_json(p));
        auto& pj = chan_to_json[repo_name];
        if (!pj.isValid())
        {
            throw std::runtime_error("Invalid JSON detected!");
        }
        else
        {
            std::cout << "Parsing " << repo_json_file << std::endl;
        }

        // ParsedJson::iterator pjh(pj);
        // parse_repo(pjh, repo, repo_to_file_map[repo_name]);
        // note here we're parsing the same json twice ... that's not good.
        fp = fopen(repo_json_file.c_str(), "r");
        if (fp) {
            repo_add_conda(repo, fp, 0);
        } else {
            throw std::runtime_error("File could no tbe read.");
        }
        fclose(fp);

        std::cout << repo->nsolvables << " packages in " << repo_name << std::endl;
        std::cout << repo_name << " prio " << repo->priority << std::endl;
        repo_internalize(repo);
    }

    pool_createwhatprovides(global_pool);

    Solver* solvy = solver_create(global_pool);
    solver_set_flag(solvy, SOLVER_FLAG_ALLOW_DOWNGRADE, 1);

    std::cout << "Allowing downgrade: " << solver_get_flag(solvy, SOLVER_FLAG_ALLOW_DOWNGRADE) << std::endl;
    std::cout << "Creating the solver..." << std::endl;

    Queue q;
    queue_init(&q);
    int update_or_install = SOLVER_INSTALL;
    if (update) {
        update_or_install = SOLVER_UPDATE;
    }
    for (const auto& job : jobs)
    {
        Id inst_id = pool_conda_matchspec(pool, job.c_str());
        // int rel = parse_to_relation(job, pool);
        std::cout << "Job: " << pool_dep2str(pool, inst_id) << std::endl;;
        queue_push2(&q, update_or_install | SOLVER_SOLVABLE_PROVIDES, inst_id);
    }

    std::cout << "\n";

    solver_solve(solvy, &q);

    Transaction* transy = solver_create_transaction(solvy);
    int cnt = solver_problem_count(solvy);
    Queue problem_queue;
    queue_init(&problem_queue);

    std::cout << "Encountered " << cnt << " problems.\n\n";
    for (int i = 1; i <= cnt; i++)
    {
        queue_push(&problem_queue, i);
        std::cout << "Problem: " << solver_problem2str(solvy, i) << std::endl;
    }
    if (cnt > 0)
    {
        throw std::runtime_error("Encountered problems while solving.");
    }

    queue_free(&problem_queue);

    transaction_print(transy);

    std::cout << "Solution: \n" << std::endl;

    std::vector<std::tuple<std::string, std::string, std::string>> to_install_structured; 
    std::vector<std::tuple<std::string, std::string>> to_remove_structured; 

    {
        Queue classes, pkgs;
        int i, j, mode;

        queue_init(&classes);
        queue_init(&pkgs);
        mode = SOLVER_TRANSACTION_SHOW_OBSOLETES |
               SOLVER_TRANSACTION_OBSOLETE_IS_UPGRADE;
        transaction_classify(transy, mode, &classes);

        Id cls;
        std::string location;
        unsigned int* somptr;
        const char* mediafile;
        for (i = 0; i < classes.count; i += 4) {
            cls = classes.elements[i];
            cnt = classes.elements[i + 1];

            transaction_classify_pkgs(transy, mode, cls, classes.elements[i + 2],
                                      classes.elements[i + 3], &pkgs);
            for (j = 0; j < pkgs.count; j++) {
              Id p = pkgs.elements[j];
              Solvable *s = pool->solvables + p;
              Solvable *s2;

              switch (cls) {
              case SOLVER_TRANSACTION_DOWNGRADED:
              case SOLVER_TRANSACTION_UPGRADED:
                mediafile = solvable_lookup_str(s, SOLVABLE_MEDIAFILE);
                to_remove_structured.emplace_back(s->repo->name, mediafile);

                s2 = pool->solvables + transaction_obs_pkg(transy, p);
                mediafile = solvable_lookup_str(s2, SOLVABLE_MEDIAFILE);
                to_install_structured.emplace_back(s2->repo->name, mediafile, "");
                break;
              case SOLVER_TRANSACTION_VENDORCHANGE:
              case SOLVER_TRANSACTION_ARCHCHANGE:
                // Not used yet.
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
                std::cout << "CASE NOT HANDLED." << std::endl;
                break;
              }
        }
      }
      queue_free(&classes);
      queue_free(&pkgs);
    }

    for (auto& el : to_install_structured)
    {
        auto& json = chan_to_json[std::get<0>(el)];
        ParsedJson::iterator pjh(json);
        std::get<2>(el) = get_package_info(pjh, std::get<1>(el));
    }

    transaction_free(transy);
    // pool free also frees all repos etc.
    pool_free(pool);

    return std::make_tuple(to_install_structured, to_remove_structured);
}