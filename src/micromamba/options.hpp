// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef UMAMBA_OPTIONS_HPP
#define UMAMBA_OPTIONS_HPP

#include <string>
#include <vector>


struct ShellOptions
{
    bool hook;
    std::string shell_type;
    std::string action;
    std::string prefix = "base";
    bool stack;
};

extern ShellOptions shell_options;

struct CleanOptions
{
    bool all = false;
    bool index_cache = false;
    bool packages = false;
    bool tarballs = false;
    // bool force_pkgs_dirs;
};

extern CleanOptions clean_options;

struct CreateOptions
{
    std::vector<std::string> specs;
    std::string prefix;
    std::string name;
    std::vector<std::string> files;
    std::vector<std::string> channels;
    int override_channels = 0;
    int strict_channel_priority = 0;
    std::string safety_checks;
    int extra_safety_checks;
    int no_pin = 0;
    int allow_softlinks = 0;
    int always_copy = 0;
    int always_softlink = 0;
};

extern CreateOptions create_options;

struct UpdateOptions
{
    bool update_all = false;
};

extern UpdateOptions update_options;

struct NetworkOptions
{
    bool ssl_verify = true;
    std::size_t repodata_ttl = 1;
    bool retry_clean_cache = false;
    bool ssl_no_revoke = false;
    std::string cacert_path;
};

extern NetworkOptions network_options;

struct ConfigOptions
{
    bool show_sources = false;
};

extern ConfigOptions config_options;

struct ListOptions
{
    std::string regex = "";
};

extern ListOptions list_options;

struct GeneralOptions
{
    int verbosity = 0;
    bool always_yes = false;
    bool quiet = false;
    bool json = false;
    bool offline = false;
    bool dry_run = false;
    std::string rc_file = "";
    bool no_rc = false;
};

extern GeneralOptions general_options;

struct ConstructorOptions
{
    std::string prefix;
    bool extract_conda_pkgs = false;
    bool extract_tarball = false;
};

extern ConstructorOptions constructor_options;

#endif
