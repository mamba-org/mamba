#ifndef UMAMBA_INSTALL_HPP
#define UMAMBA_INSTALL_HPP

#include "mamba/context.hpp"
#include "mamba/mamba_fs.hpp"
#include "mamba/package_info.hpp"
#include "mamba/pool.hpp"
#include "mamba/repo.hpp"
#include "mamba/solver.hpp"

#ifdef VENDORED_CLI11
#include "mamba/CLI.hpp"
#else
#include <CLI/CLI.hpp>
#endif

#include <string>
#include <vector>


void
init_install_parser(CLI::App* subcom);

void
set_install_command(CLI::App* subcom);

void
install_specs(const std::vector<std::string>& specs,
              bool create_env = false,
              int solver_flag = SOLVER_INSTALL,
              int is_retry = 0);

void
parse_file_options();

mamba::MRepo
create_repo_from_pkgs_dir(mamba::MPool& pool, const fs::path& pkgs_dir);

void
install_explicit_specs(std::vector<std::string>& specs);

bool
download_explicit(const std::vector<mamba::PackageInfo>& pkgs);

#endif
