#include "thirdparty/simdjson/simdjson.h"

extern "C"
{
    #include "solv/pool.h"
    #include "solv/repo.h"
    #include "solv/queue.h"
    #include "solv/solver.h"
    #include "solv/solverdebug.h"
}

static Pool* global_pool;

#include <iostream>
#include <map>

#include "solver.hpp"
#include "parsing.hpp"
#include "json_helper.hpp"


void parse_repo(ParsedJson::iterator &i, Repo* repo, std::map<Id, std::string>& rmap) {

  if (!i.move_to_key("packages"))
  {
      throw std::runtime_error("Could not find packages key!");
  }

  std::string_view version, build_string, features, name;
  int build_number = 0; // change to char* as well

  i.down();

  do {
    Id s_id = repo_add_solvable(repo);
    auto& s = global_pool->solvables[s_id];
    rmap[s_id] = i.get_string();

    i.next(); i.down();
    do {
        if (strcmp(i.get_string(), "name") == 0)
        {
            i.next();
            name = i.get_string();
            Id name_id = pool_str2id(global_pool, i.get_string(), 1);
            s.name = name_id;
        }
        else if (strcmp(i.get_string(), "build_number") == 0)
        {
            i.next();
            build_number = i.get_integer();
        }
        else if (strcmp(i.get_string(), "build") == 0)
        {
            i.next();
            build_string = i.get_string();
        }
        else if (strcmp(i.get_string(), "features") == 0)
        {
            i.next();
            features = i.get_string();
        }
        else if (strcmp(i.get_string(), "version") == 0)
        {
            i.next();
            version = i.get_string();
        }
        else if (strcmp(i.get_string(), "depends") == 0)
        {
            i.next();
            if (i.down())
            {
                do {
                    Id rel = parse_to_relation(i.get_string(), global_pool);
                    solvable_add_deparray(
                        &s,
                        SOLVABLE_REQUIRES,
                        rel, -1);

                } while (i.next());
                i.up();
            }
        }
        else {
            i.next(); // skip value?
        }
    } while (i.next());

    s.evr = pool_str2id(global_pool, normalize_version(version, build_number, build_string).c_str(), 1);

    solvable_add_deparray(&s, SOLVABLE_PROVIDES,
                          pool_rel2id(global_pool, s.name, s.evr, REL_EQ, 1), -1);

    if (features.size())
    {
        std::stringstream os;
        os << name << "[" << features << "]";
        std::string feature_name = os.str();
        auto feat_id = pool_strn2id(global_pool, feature_name.c_str(), feature_name.size(), 1);

        solvable_add_deparray(&s, SOLVABLE_PROVIDES,
                              pool_rel2id(global_pool, feat_id, s.evr, REL_EQ, 1), -1);
    }

    if (build_string.size())
    {
        std::stringstream os;
        os << name << "[" << build_string << "]";
        std::string feature_name = os.str();
        auto feat_id = pool_strn2id(global_pool, feature_name.c_str(), feature_name.size(), 1);

        solvable_add_deparray(&s, SOLVABLE_PROVIDES,
                              pool_rel2id(global_pool, feat_id, s.evr, REL_EQ, 1), -1);
    }

    i.up();
  } while (i.next());
}

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
solve(std::vector<std::pair<std::string, std::string>> repos,
           std::string installed,
           std::vector<std::string> jobs,
           bool update,
           bool strict_priority)
{
    Pool* pool = pool_create();
    global_pool = pool;

    std::map<std::string, std::map<Id, std::string>> repo_to_file_map;

    std::map<std::string, ParsedJson> chan_to_json;

    if (installed.size())
    {
        Repo* repo = repo_create(pool, "installed");
        repo_to_file_map["installed"] = std::map<Id, std::string>();
        pool_set_installed(pool, repo);
        std::string_view p = get_corpus(installed);
        ParsedJson pj = build_parsed_json(p);
        ParsedJson::iterator pjh(pj);
        parse_repo(pjh, repo, repo_to_file_map["installed"]);
    }

    int priority = repos.size();
    for (auto& fn : repos)
    {
        std::string_view p = get_corpus(fn.second);

        repo_to_file_map[fn.first] = std::map<Id, std::string>();
        Repo* repo = repo_create(pool, fn.first.c_str());

        if (strict_priority)
        {
            repo->priority = priority--;
        }
        else
        {
            repo->subpriority = priority--;
        }

        chan_to_json.emplace(fn.first, build_parsed_json(p));
        auto& pj = chan_to_json[fn.first];
        if (!pj.isValid())
        {
            throw std::runtime_error("Invalid JSON detected!");
        }
        else
        {
            std::cout << "Parsing " << fn.second << std::endl;
        }

        ParsedJson::iterator pjh(pj);
        parse_repo(pjh, repo, repo_to_file_map[fn.first]);
        std::cout << repo->nsolvables << " packages in " << fn.first << std::endl;
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
        int rel = parse_to_relation(job, pool);
        std::cout << "Job: " << pool_dep2str(pool, rel) << std::endl;;
        queue_push2(&q, update_or_install | SOLVER_SOLVABLE_NAME, rel);
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
                s2 = pool->solvables + transaction_obs_pkg(transy, p);
                to_remove_structured.emplace_back(s->repo->name, repo_to_file_map[s->repo->name][p]);
                to_install_structured.emplace_back(s2->repo->name, repo_to_file_map[s2->repo->name][transaction_obs_pkg(transy, p)], "");
                break;
              case SOLVER_TRANSACTION_VENDORCHANGE:
              case SOLVER_TRANSACTION_ARCHCHANGE:
                // Not used yet.
                break;
              case SOLVER_TRANSACTION_ERASE:
                to_remove_structured.emplace_back(s->repo->name, repo_to_file_map[s->repo->name][p]);
                break;
              case SOLVER_TRANSACTION_INSTALL:
                to_install_structured.emplace_back(s->repo->name, repo_to_file_map[s->repo->name][p], "");
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