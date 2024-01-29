// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/core/context.hpp"
#include "mamba/core/package_handling.hpp"
#include "mamba/util/string.hpp"

#include "package.hpp"


using namespace mamba;  // NOLINT(build/namespaces)

void
set_package_command(CLI::App* com, mamba::Context& context)
{
    static std::string infile, dest;
    static int compression_level = -1;
    static int compression_threads = 1;

    auto extract_subcom = com->add_subcommand("extract");
    extract_subcom->add_option("archive", infile, "Archive to extract");
    extract_subcom->add_option("dest", dest, "Destination folder");
    extract_subcom->callback(
        [&]()
        {
            std::cout << "Extracting " << fs::absolute(infile) << " to " << fs::absolute(dest)
                      << std::endl;
            extract(fs::absolute(infile), fs::absolute(dest), ExtractOptions::from_context(context));
        }
    );

    auto compress_subcom = com->add_subcommand("compress");
    compress_subcom->add_option("folder", infile, "Folder to compress");
    compress_subcom->add_option("dest", dest, "Destination (e.g. myfile-3.1-0.tar.bz2 or .conda)");
    compress_subcom->add_option(
        "-c,--compression-level",
        compression_level,
        "Compression level from 0-9 (tar.bz2, default is 9), and 1-22 (conda, default is 15)"
    );
    compress_subcom->add_option(
        "--compression-threads",
        compression_threads,
        "Compression threads (only relevant for .conda packages, default is 1)"
    );
    compress_subcom->callback(
        [&]()
        {
            std::cout << "Compressing " << fs::absolute(infile) << " to " << dest << std::endl;

            if (util::ends_with(dest, ".tar.bz2") && compression_level == -1)
            {
                compression_level = 9;
            }
            if (util::ends_with(dest, ".conda") && compression_level == -1)
            {
                compression_level = 15;
            }

            create_package(
                fs::absolute(infile),
                fs::absolute(dest),
                compression_level,
                compression_threads
            );
        }
    );

    auto transmute_subcom = com->add_subcommand("transmute");
    transmute_subcom->add_option("infile", infile, "Folder to compress");
    transmute_subcom->add_option(
        "-c,--compression-level",
        compression_level,
        "Compression level from 0-9 (tar.bz2, default is 9), and 1-22 (conda, default is 15)"
    );
    transmute_subcom->add_option(
        "--compression-threads",
        compression_threads,
        "Compression threads (only relevant for .conda packages, default is 1)"
    );
    transmute_subcom->callback(
        [&]()
        {
            if (util::ends_with(infile, ".tar.bz2"))
            {
                if (compression_level == -1)
                {
                    compression_level = 15;
                }
                dest = infile.substr(0, infile.size() - 8) + ".conda";
            }
            else
            {
                if (compression_level == -1)
                {
                    compression_level = 9;
                }
                dest = infile.substr(0, infile.size() - 8) + ".tar.bz2";
            }
            std::cout << "Transmuting " << fs::absolute(infile) << " to " << dest << std::endl;
            transmute(
                fs::absolute(infile),
                fs::absolute(dest),
                compression_level,
                compression_threads,
                ExtractOptions::from_context(context)
            );
        }
    );
}
