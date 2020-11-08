#ifndef MAMBA_FS_HPP
#define MAMBA_FS_HPP

#ifdef MAMBA_USE_STD_FS
#include <filesystem>
#include <fstream>
namespace fs = std::filesystem;
#else
#include "ghc/filesystem.hpp"
namespace fs = ghc::filesystem;
#endif

#endif
