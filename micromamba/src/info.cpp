// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "common_options.hpp"

#include "mamba/api/info.hpp"


using namespace mamba;  // NOLINT(build/namespaces)

void
init_info_parser(CLI::App* subcom)
{
    init_general_options(subcom);
    init_prefix_options(subcom);
}

void
set_info_command(CLI::App* subcom)
{
    init_info_parser(subcom);
    static bool print_licenses;

    subcom->add_flag("--licenses", print_licenses, "Print licenses");

    subcom->callback(
        [&]()
        {
            // TODO: Print full license texts.
            if (print_licenses)
            {
                const std::vector<std::pair<std::string, std::string>> licenses = {
                    { "micromamba",
                      "BSD license, Copyright 2019 QuantStack and the Mamba contributors." },
                    { "c_ares",
                      "MIT license, Copyright (c) 2007 - 2018, Daniel Stenberg with many contributors, see AUTHORS file." },
                    { "cli11",
                      "BSD license, CLI11 1.8 Copyright (c) 2017-2019 University of Cincinnati, developed by Henry Schreiner under NSF AWARD 1414736. All rights reserved." },
                    { "cpp_filesystem",
                      "MIT license, Copyright (c) 2018, Steffen Schümann <s.schuemann@pobox.com>" },
                    { "curl",
                      "MIT license, Copyright (c) 1996 - 2020, Daniel Stenberg, daniel@haxx.se, and many contributors, see the THANKS file." },
                    { "krb5",
                      "MIT license, Copyright 1985-2020 by the Massachusetts Institute of Technology." },
                    { "libarchive",
                      "New BSD license, The libarchive distribution as a whole is Copyright by Tim Kientzle and is subject to the copyright notice reproduced at the bottom of this file." },
                    { "libev",
                      "BSD license, All files in libev are Copyright (c)2007,2008,2009,2010,2011,2012,2013 Marc Alexander Lehmann." },
                    { "liblz4", "LZ4 Library, Copyright (c) 2011-2016, Yann Collet" },
                    { "libnghttp2",
                      "MIT license, Copyright (c) 2012, 2014, 2015, 2016 Tatsuhiro Tsujikawa; 2012, 2014, 2015, 2016 nghttp2 contributors" },
                    { "libopenssl_3", "Apache license, Version 2.0, January 2004" },
                    { "libopenssl",
                      "Apache license, Copyright (c) 1998-2019 The OpenSSL Project, All rights reserved; 1995-1998 Eric Young (eay@cryptsoft.com)" },
                    { "libsolv", "BSD license, Copyright (c) 2019, SUSE LLC" },
                    { "nlohmann_json", "MIT license, Copyright (c) 2013-2020 Niels Lohmann" },
                    { "reproc", "MIT license, Copyright (c) Daan De Meyer" },
                    { "fmt", "MIT license, Copyright (c) 2012-present, Victor Zverovich." },
                    { "spdlog", "MIT license, Copyright (c) 2016 Gabi Melman." },
                    { "zstd",
                      "BSD license, Copyright (c) 2016-present, Facebook, Inc. All rights reserved." },
                };
                for (const auto& [dep, text] : licenses)
                {
                    std::cout << dep << "\n"
                              << std::string(dep.size(), '-') << "\n"
                              << text << "\n\n";
                }
            }
            else
            {
                info();
            }
        });
}
