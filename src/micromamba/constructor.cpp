// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "constructor.hpp"
#include "options.hpp"

#include "mamba/package_handling.hpp"
#include "mamba/util.hpp"


using namespace mamba;  // NOLINT(build/namespaces)

void
init_constructor_parser(CLI::App* subcom)
{
    subcom->add_option("-p,--prefix", constructor_options.prefix, "Path to the prefix");
    subcom->add_flag("--extract-conda-pkgs",
                     constructor_options.extract_conda_pkgs,
                     "Extract the conda pkgs in <prefix>/pkgs");
    subcom->add_flag("--extract-tarball",
                     constructor_options.extract_tarball,
                     "Extract given tarball into prefix");
}

void
set_constructor_command(CLI::App* subcom)
{
    init_constructor_parser(subcom);

    subcom->callback([&]() {
        if (constructor_options.prefix.empty())
        {
            throw std::runtime_error("Prefix is required.");
        }
        if (constructor_options.extract_conda_pkgs)
        {
            fs::path pkgs_dir = constructor_options.prefix;
            fs::path filename;
            pkgs_dir = pkgs_dir / "pkgs";
            for (const auto& entry : fs::directory_iterator(pkgs_dir))
            {
                filename = entry.path().filename();
                if (ends_with(filename.string(), ".tar.bz2")
                    || ends_with(filename.string(), ".conda"))
                {
                    extract(entry.path());
                }
            }
        }
        if (constructor_options.extract_tarball)
        {
            fs::path extract_tarball_path = fs::path(constructor_options.prefix) / "_tmp.tar.bz2";
            read_binary_from_stdin_and_write_to_file(extract_tarball_path);
            extract_archive(extract_tarball_path, constructor_options.prefix);
            fs::remove(extract_tarball_path);
        }
        return 0;
    });
}

void
read_binary_from_stdin_and_write_to_file(fs::path& filename)
{
    std::ofstream out_stream(filename.string().c_str(), std::ofstream::binary);
    // Need to reopen stdin as binary
    std::freopen(nullptr, "rb", stdin);
    if (std::ferror(stdin))
    {
        throw std::runtime_error("Re-opening stdin as binary failed.");
    }
    std::size_t len;
    std::array<char, 1024> buffer;

    while ((len = std::fread(buffer.data(), sizeof(char), buffer.size(), stdin)) > 0)
    {
        if (std::ferror(stdin) && !std::feof(stdin))
        {
            throw std::runtime_error("Reading from stdin failed.");
        }
        out_stream.write(buffer.data(), len);
    }
    out_stream.close();
}
