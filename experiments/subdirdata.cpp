#include <string>
#include <memory>
#include <iostream>
#include <filesystem>

#include <skyr/url.hpp>

#include "path.hpp"
#include "package_cache_data.hpp"

extern "C" {
    #include <glib.h>
    #include <stdlib.h>
    #include <stdio.h>
    #include <librepo/librepo.h>
}

const std::string REPODATA_FN = "repodata.json";

static void
log_handler_cb(const gchar *log_domain G_GNUC_UNUSED,
               GLogLevelFlags log_level G_GNUC_UNUSED,
               const gchar *message,
               gpointer user_data G_GNUC_UNUSED)
{
    g_print ("%s\n", message);
}

struct ProgressData {
    std::string name;
    bool compressed;
    std::string filename;
};

static int
progress_callback(void *data,
                  double total_to_download,
                  double now_downloaded)
{
    auto& name = ((ProgressData*) data)->name;
    std::cout << "\r" << name << ": " << total_to_download << " / " << now_downloaded;
    fflush(stdout);
    return 0;
}

static int
end_callback(void* data, LrTransferStatus status, const char* msg)
{
    std::cout << "\n";
    if (msg)
    {
        std::cout << msg << std::endl;
    }

    auto* pdata = (ProgressData*) data;
    if (pdata->compressed)
    {
        // Not in a thread, also ugly code since calling out to the shell...
        path::uncompress_bz2(pdata->filename);
    }
    return 0;
}

class Result 
{
public:
    Result(LrResult* result)
        : m_result(result)
    {
    }

    ~Result()
    {
        lr_result_free(m_result);
    }

    LrResult* m_result;
};

class Handle
{
public:
    Handle() {
        m_handle = lr_handle_init();
    }

    ~Handle() {
        lr_handle_free(m_handle);
    }

    LrHandle* get() {
        return m_handle;
    }

    Result perform() {
        LrResult* result = lr_result_init();
        GError* tmp_err = NULL;
        std::cout << "Downloading! " << std::endl;
        lr_handle_perform(m_handle, result, &tmp_err);
        // g_error_free(tmp_err);
        char *destdir;
        lr_handle_getinfo(m_handle, NULL, LRI_DESTDIR, &destdir);

        if (result) {
            printf("Download successful (Destination dir: %s)\n", destdir);
        } else {
            fprintf(stderr, "Error encountered: %s\n", tmp_err->message);
            g_error_free(tmp_err);
            // rc = EXIT_FAILURE;
        }

        return result;
    }

    LrHandle* m_handle;
};

class DownloadTarget {
public:

    DownloadTarget(
        Handle& handle,
        const std::string& name,
        const std::string& url,
        const std::string& filename) :
            m_progress_data{name, true, filename}
    {
        std::string name1 = std::tmpnam(nullptr);
        std::cout << "Target for " << name << std::endl;
        m_target = lr_downloadtarget_new(
            handle.get(),     // ptr to handle
            url.c_str(),      // url (absolute, or relative)
            nullptr,          // base url
            -1,               // file descriptor (opened)
            filename.c_str(), // filename
            nullptr,          // possible checksums
            0,                // expected size
            true,             // resume
            progress_callback,// progress cb
            &m_progress_data, // cb data
            end_callback,     // end cb
            nullptr,          // mirror failure cb
            nullptr,          // user data
            0,                // byte range start (download only range)
            0,                // byte range end
            nullptr,          // range string (overrides start / end, usage unclear)
            false,            // no_cache (tell proxy server that we want fresh data)
            false             // is zchunk
        );
    }

    bool download()
    {
        GError* tmp_err = NULL;
        bool result = lr_download_target(m_target, &tmp_err);

        if (result) {
            printf("Download successful\n");
        } else {
            fprintf(stderr, "Error encountered: %s\n", tmp_err->message);
            g_error_free(tmp_err);
            // rc = EXIT_FAILURE;
        }
        return result;
    }

    LrDownloadTarget* get()
    {
        return m_target;
    }

    ~DownloadTarget() {
        lr_downloadtarget_free(m_target);
    }

    ProgressData m_progress_data;
    LrDownloadTarget* m_target;
};

class DownloadTargetList
{
public:

    DownloadTargetList()
    {
    }

    void append(DownloadTarget& target)
    {
        m_download_targets = g_slist_append(m_download_targets, target.get());
    }

    bool download(bool failfast=true)
    {
        GError* tmp_err = NULL;
        bool result = lr_download(m_download_targets, failfast, &tmp_err);

        if (result) {
            printf("Download successful\n");
        } else {
            fprintf(stderr, "Error encountered: %s\n", tmp_err->message);
            g_error_free(tmp_err);
            // rc = EXIT_FAILURE;
        }
        return result;
    }

    GSList* m_download_targets = nullptr;
};


class Channel {
public:

    Channel(const std::string& name, const std::string& url, int priority) :
        m_name(name),
        m_url(url),
        m_priority(priority)
    {
    }

    const std::string& name() const
    {
        return m_name;
    }

    std::string url(bool with_credentials=false) {
        if (!with_credentials)
        {
            return m_url;
        }
        else
        {
            throw std::runtime_error("Not implemented yet!");
        }
    }

    static Channel from_name(const std::string& name)
    {
        throw std::runtime_error("Not implemented yet!");
        // return Channel()
    }

    // def _get_channel_for_name_helper(name):
    //     if name in context.custom_channels:
    //         return context.custom_channels[name]
    //     else:
    //         test_name = name.rsplit('/', 1)[0]  # progressively strip off path segments
    //         if test_name == name:
    //             return None
    //         return _get_channel_for_name_helper(test_name)

    // _stripped, platform = split_platform(channel_name, context.known_subdirs)
    // channel = _get_channel_for_name_helper(_stripped)

    // if channel is not None:
    //     # stripping off path threw information away from channel_name (i.e. any potential subname)
    //     # channel.name *should still be* channel_name
    //     channel = copy(channel)
    //     channel.name = _stripped
    //     if platform:
    //         channel.platform = platform
    //     return channel
    // else:
    //     ca = context.channel_alias
    //     return Channel(scheme=ca.scheme, auth=ca.auth, location=ca.location, token=ca.token,
    //                    name=_stripped, platform=platform)

    static Channel from_url(const std::string& uri)
    {
        if (uri == "<unknown>" || uri == "None:///<unknown>" || uri == "None")
            return Channel("UNKNOWN_CHANNEL", "", -1);
        auto parsed_uri = skyr::url(uri);
        std::cout << parsed_uri.protocol() << std::endl;
        if (parsed_uri.protocol() == "file:")
        {
            auto fixed_uri = path::win_path_backout(uri);
            return Channel::from_url(fixed_uri);
        }
        else if (path::is_path(uri))
        {
            throw std::runtime_error("Not implemented yet!");
            // return Channel::from_url(path::path_to_url(uri));
        }
        else if (path::is_package_file(uri))
        {
            // return Channel::from_url(value);
        }
        // elif is_package_file(value):
        //     if value.startswith('file:'):
        //         value = win_path_backout(value)
        //     return Channel.from_url(value)
        // else:
        //     # at this point assume we don't have a bare (non-scheme) url
        //     #   e.g. this would be bad:  repo.anaconda.com/pkgs/free
        //     _stripped, platform = split_platform(value, context.known_subdirs)
        //     if _stripped in context.custom_multichannels:
        //         return MultiChannel(_stripped, context.custom_multichannels[_stripped], platform)
        //     else:
        //         return Channel.from_channel_name(value)
        return Channel("asdasd", "", -1);
    }

private:

    std::string m_name;
    std::string m_url;
    int m_priority;
};

class Subdir {
public:

    Subdir(const std::shared_ptr<Channel>& channel, const std::string& platform,
           int sub_priority, const std::string& repodata_fn) : 
        m_channel(channel),
        m_platform(platform),
        m_repodata_fn(repodata_fn),
        m_sub_priority(sub_priority)
    {
        if (m_use_compression)
        {
            m_repodata_fn += ".bz2";
        }
    }

    std::string name() const
    {
        return m_channel->name() + "/" + m_platform;
    }

    std::string url()
    {
        return path::join(path::join(m_channel->url(), m_platform), m_repodata_fn);
    }

    std::string cache_path()
    {
        return path::cache_fn_url(url());
    }

    DownloadTarget target(Handle& handle)
    {
        std::string cp = cache_path();
        if (m_use_compression)
        {
            cp += ".bz2";
        }
        DownloadTarget target(handle, name(), url(), cp);
        return target;
    }

private:
    std::shared_ptr<Channel> m_channel;
    std::string m_platform, m_repodata_fn;
    bool m_loaded = false;
    bool m_cached = false;
    bool m_use_compression = true;
    int m_sub_priority;
};

int main() {

    // std::vector<std::string> pkgs_dirs = {"/etc/miniconda/", "/home/wolfv/miniconda3/pkgs/"};
    // auto pcd = PackageCacheData::first_writable(&pkgs_dirs);

    auto chan = Channel::from_url("https://conda.anaconda.org/conda-forge/");

    // Handle handle;

    // auto chan = std::make_shared<Channel>("conda-forge", "https://conda.anaconda.org/conda-forge/", 0);
    // auto s1 = std::make_shared<Subdir>(chan, "linux-64", 0, "repodata.json");
    // auto s2 = std::make_shared<Subdir>(chan, "noarch", 0, "repodata.json");

    // std::cout << s1->url() << std::endl;
    // auto t1 = s1->target(handle);
    // std::cout << s2->url() << std::endl;
    // auto t2 = s2->target(handle);
    
    // DownloadTargetList dls;
    // dls.append(t1);
    // dls.append(t2);
    // dls.download();

    return 0;
}