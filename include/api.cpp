/*
<%
cfg['compiler_args'] = ['-std=c++17', '-march=native']
cfg['libraries'] = ['solv']
setup_pybind11(cfg)
%>
*/

#include "thirdparty/simdjson/simdjson.h"
#include "thirdparty/simdjson/simdjson.cpp"

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

#include "parsing.hpp"

struct package {
    std::string name;
    std::string version;
    std::string build_string;
    int build_number;
};

std::ostream& operator<<(std::ostream& os, package& pkg)
{
    return os << pkg.name << " -> " << pkg.version << ", " << pkg.build_string;
}

struct repo_package {
    std::string name;
    std::string version;
    std::string build_string;
    int build_number;
    std::vector<std::string> dependencies;
};

std::ostream& operator<<(std::ostream& os, repo_package& pkg)
{
    return os << pkg.name << " -> " << pkg.version << ", " << pkg.build_string;
}

void parse_repo(ParsedJson::iterator &i, Repo* repo) {
  package pkg;

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
    i.next(); i.down();
    do {
        if (strcmp(i.get_string(), "name") == 0)
        {
            i.next();
            pkg.name = i.get_string();
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


void installed_packages(Repo* repo, ParsedJson::iterator &i) {
  package pkg;
  switch (i.get_type()) {
  case '{':
  {
    package pkg;
    i.down();
    do {
        if (strcmp(i.get_string(), "name") == 0)
        {
            i.next();
            pkg.name = i.get_string();
        }
        else if (strcmp(i.get_string(), "build_number") == 0)
        {
            i.next();
            pkg.build_number = i.get_integer();
        }
        else if (strcmp(i.get_string(), "build_string") == 0)
        {
            i.next();
            pkg.build_string = i.get_string();
        }
        else if (strcmp(i.get_string(), "version") == 0)
        {
            i.next();
            pkg.version = i.get_string();
        }
        else {
            i.next(); // skip value?
        }
    } while (i.next());
    i.up();

    Id s_id = repo_add_solvable(repo);
    auto& s = global_pool->solvables[s_id];

    s.name = pool_str2id(global_pool, pkg.name.c_str(), 1);
    s.evr = pool_str2id(global_pool, normalize_version(pkg.version, pkg.build_number, pkg.build_string).c_str(), 1);
    solvable_add_deparray(&s, SOLVABLE_PROVIDES,
                          pool_rel2id(global_pool, s.name, s.evr, REL_EQ, 1), -1);
    break;
  }
  case '[':
    if (i.down()) {
      do {
        if (i.is_object_or_array()) {
          installed_packages(repo, i);
        }
      } while (i.next());
      i.up();
    }
    break;
  case 'l':
  case 'd':
  case 'n':
  case 't':
  case 'f':
  default:
    break;
  }
}

std::string solve(std::vector<std::string> repos,
                  std::string installed,
                  std::vector<std::string> jobs)
{
    Pool* pool = pool_create();
    global_pool = pool;

    if (installed.size())
    {
        Repo* repo = repo_create(pool, "installed");
        pool_set_installed(pool, repo);
        std::string_view p = get_corpus(installed);
        ParsedJson pj = build_parsed_json(p);
        ParsedJson::iterator pjh(pj);
        installed_packages(repo, pjh);
    }
    for (auto& fn : repos)
    {
        std::string_view p = get_corpus(fn);

        Repo* repo = repo_create(pool, fn.c_str());

        ParsedJson pj = build_parsed_json(p);
        if (!pj.isValid())
        {
            std::cout << "Found invalid json!";
        }
        else
        {
            std::cout << "Parsing " << fn << std::endl;
        }
        ParsedJson::iterator pjh(pj);
        parse_repo(pjh, repo);
        std::cout << repo->nsolvables << " packages in " << fn << std::endl;
        repo_internalize(repo);
    }

    pool_createwhatprovides(global_pool);
    Solver* solvy = solver_create(global_pool);
    solver_set_flag(solvy, SOLVER_FLAG_ALLOW_DOWNGRADE, 1);

    std::cout << "Allowing downgrade: " << solver_get_flag(solvy, SOLVER_FLAG_ALLOW_DOWNGRADE) << std::endl;

    std::cout << "Creating the solver...\n" << std::endl;

    Queue q;
    queue_init(&q);
    for (const auto& job : jobs)
    {
        int rel = parse_to_relation(job, pool);
        std::cout << "Job: " << pool_dep2str(pool, rel) << std::endl;;
        queue_push2(&q, SOLVER_INSTALL | SOLVER_SOLVABLE_NAME, rel);
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
        return "";
    }

    transaction_print(transy);

    Queue q2;
    int cut;
    queue_init(&q2);
    cut = transaction_installedresult(transy, &q2);
    queue_truncate(&q2, cut);

    std::cout << "Solution: \n" << std::endl;

    std::vector<std::string> to_install;
    for (int i = 0; i < q2.count; ++i)
    {
        to_install.emplace_back(pool_id2str(global_pool, global_pool->solvables[q2.elements[i]].name));
        to_install.back() += " ==";
        std::string version = pool_id2str(global_pool, global_pool->solvables[q2.elements[i]].evr);

        auto vsplit = pystring::split(version, ".");
        vsplit.pop_back();
        version = pystring::join(".", vsplit);
        to_install.back() += version;
    }

    std::sort(to_install.begin(), to_install.end());

    std::string result;
    for (auto& line : to_install)
    {
        result += "- " + line + "\n";
    }

    return result;
}