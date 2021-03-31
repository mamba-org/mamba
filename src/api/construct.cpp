#include "mamba/api/configuration.hpp"
#include "mamba/api/construct.hpp"

#include "mamba/core/package_handling.hpp"
#include "mamba/core/util.hpp"


namespace mamba
{
    void construct(const fs::path& prefix, bool extract_conda_pkgs, bool extract_tarball)
    {
        auto& config = Configuration::instance();

        config.at("show_banner").get_wrapped<bool>().set_value(false);
        config.load(MAMBA_ALLOW_ROOT_PREFIX | MAMBA_ALLOW_FALLBACK_PREFIX
                    | MAMBA_ALLOW_EXISTING_PREFIX);

        if (extract_conda_pkgs)
        {
            fs::path pkgs_dir = prefix / "pkgs";
            fs::path filename;
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
        if (extract_tarball)
        {
            fs::path extract_tarball_path = prefix / "_tmp.tar.bz2";
            detail::read_binary_from_stdin_and_write_to_file(extract_tarball_path);
            extract_archive(extract_tarball_path, prefix);
            fs::remove(extract_tarball_path);
        }
    }

    namespace detail
    {
        void read_binary_from_stdin_and_write_to_file(fs::path& filename)
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
    }  // detail
}  // mamba
