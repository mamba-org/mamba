// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/api/configuration.hpp"
#include "mamba/core/package_handling.hpp"
#include "mamba/util/string.hpp"

#include "common_options.hpp"

using namespace mamba;  // NOLINT(build/namespaces)

void
set_package_command(CLI::App* subcom, Configuration& config)
{
    static std::string infile, dest;
    static int compression_level = -1;
    static int compression_threads = 1;

    init_general_options(subcom, config);

    auto extract_subcom = subcom->add_subcommand("extract");
    init_general_options(extract_subcom, config);
    extract_subcom->add_option("archive", infile, "Archive to extract")->option_text("ARCHIVE");
    extract_subcom->add_option("dest", dest, "Destination folder")->option_text("FOLDER");
    extract_subcom->callback(
        [&]
        {
            // load verbose and other options to context
            config.load();

            Console::stream() << "Extracting " << fs::absolute(infile) << " to "
                              << fs::absolute(dest) << std::endl;
            extract(
                fs::absolute(infile),
                fs::absolute(dest),
                ExtractOptions::from_context(config.context())
            );
        }
    );

    auto compress_subcom = subcom->add_subcommand("compress");
    init_general_options(compress_subcom, config);
    compress_subcom->add_option("folder", infile, "Folder to compress")->option_text("FOLDER");
    compress_subcom->add_option("dest", dest, "Destination (e.g. myfile-3.1-0.tar.bz2 or .conda)")
        ->option_text("DEST");
    compress_subcom
        ->add_option(
            "-c,--compression-level",
            compression_level,
            "Compression level from 0-9 (tar.bz2, default is 9), and 1-22 (conda, default is 15)"
        )
        ->option_text("COMP_LEVEL");
    compress_subcom
        ->add_option(
            "--compression-threads",
            compression_threads,
            "Compression threads (only relevant for .conda packages, default is 1)"
        )
        ->option_text("COMP_THREADS");
    compress_subcom->callback(
        [&]()
        {
            // load verbose and other options to context
            config.load();

            Console::stream() << "Compressing " << fs::absolute(infile) << " to " << dest
                              << std::endl;

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

    auto transmute_subcom = subcom->add_subcommand("transmute");
    init_general_options(transmute_subcom, config);
    transmute_subcom->add_option("infile", infile, "Folder to compress")->option_text("FOLDER");
    transmute_subcom
        ->add_option(
            "-c,--compression-level",
            compression_level,
            "Compression level from 0-9 (tar.bz2, default is 9), and 1-22 (conda, default is 15)"
        )
        ->option_text("COMP_LEVEL");
    transmute_subcom
        ->add_option(
            "--compression-threads",
            compression_threads,
            "Compression threads (only relevant for .conda packages, default is 1)"
        )
        ->option_text("COMP_THREADS");
    transmute_subcom->callback(
        [&]()
        {
            // load verbose and other options to context
            config.load();

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
                dest = infile.substr(0, infile.size() - 6) + ".tar.bz2";
            }
            Console::stream() << "Transmuting " << fs::absolute(infile) << " to " << dest
                              << std::endl;
            transmute(
                fs::absolute(infile),
                fs::absolute(dest),
                compression_level,
                compression_threads,
                ExtractOptions::from_context(config.context())
            );
        }
    );
}
