#ifndef MAMBA_SUBDIRDATA_HPP
#define MAMBA_SUBDIRDATA_HPP

#include <string>
#include "thirdparty/filesystem.hpp"
#include <regex>

#include "nlohmann/json.hpp"

#include "context.hpp"
#include "util.hpp"
#include "fetch.hpp"
#include "output.hpp"

namespace fs = ghc::filesystem;

namespace decompress
{
    bool raw(const std::string& in, const std::string& out);
}

namespace mamba
{
    class MSubdirData
    {
    public:

        MSubdirData(const std::string& name, const std::string& url, const std::string& repodata_fn);

        // TODO return seconds as double
        double check_cache(const fs::path& cache_file);
        bool loaded();
        bool forbid_cache();
        bool load();
        std::string cache_path() const;
        std::unique_ptr<DownloadTarget>& target();
        const std::string& name() const;
        int finalize_transfer();

    private:

        bool decompress();
        void create_target(nlohmann::json& mod_etag);
        std::size_t get_cache_control_max_age(const std::string& val);
        nlohmann::json read_mod_and_etag();

        std::unique_ptr<DownloadTarget> m_target;

        bool m_json_cache_valid = false;
        bool m_solv_cache_valid = false;

        std::ofstream out_file;

        ProgressProxy m_progress_bar;

        bool m_loaded, m_download_complete;
        std::string m_url;
        std::string m_name;
        std::string m_json_fn;
        std::string m_solv_fn;
        std::unique_ptr<TemporaryFile> m_temp_file;
    };
}

#endif // MAMBA_SUBDIRDATA_HPP
