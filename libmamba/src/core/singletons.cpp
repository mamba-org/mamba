
#include <atomic>
#include <mutex>
#include <cassert>
#include <regex>

extern "C"
{
#include <curl/urlapi.h>
}

#include "spdlog/spdlog.h"

#include "mamba/api/configuration.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/validate.hpp"
#include "mamba/core/channel_builder.hpp"
#include "mamba/core/execution.hpp"


namespace mamba
{
    // WARNING: The order in which the following static objects are defined is important
    // to maintain inter-singleton dependencies conherent.
    // Do not move them around lightly.
    //
    // The intent here is to make sure at main() exit that we have all singletons
    // cleanup their resources (including joining threads) in a predictable order.
    // To achieve this we define them in the same translation unit, which at least guarantees
    // construction and destruction order will follow this file's order.
    // Other static objects from other translation units can be destroyed in parallel to the ones
    // here as C++ does not guarantee any order of destruction after `main()`.

    //--- Dependencie's singletons
    //----------------------------------------------------------------------


    class CURLSetup final
    {
    public:
        CURLSetup()
        {
#ifdef LIBMAMBA_STATIC_DEPS
            CURLsslset sslset_res;
            const curl_ssl_backend** available_backends;

            if (on_linux)
            {
                sslset_res
                    = curl_global_sslset(CURLSSLBACKEND_OPENSSL, nullptr, &available_backends);
            }
            else if (on_mac)
            {
                sslset_res = curl_global_sslset(
                    CURLSSLBACKEND_SECURETRANSPORT, nullptr, &available_backends);
            }
            else if (on_win)
            {
                sslset_res
                    = curl_global_sslset(CURLSSLBACKEND_SCHANNEL, nullptr, &available_backends);
            }

            if (sslset_res == CURLSSLSET_TOO_LATE)
            {
                LOG_ERROR << "cURL SSL init called too late, that is a bug.";
            }
            else if (sslset_res == CURLSSLSET_UNKNOWN_BACKEND
                     || sslset_res == CURLSSLSET_NO_BACKENDS)
            {
                LOG_WARNING
                    << "Could not use preferred SSL backend (Linux: OpenSSL, OS X: SecureTransport, Win: SChannel)"
                    << std::endl;
                LOG_WARNING << "Please check the cURL library configuration that you are using."
                            << std::endl;
            }
#endif

            if (curl_global_init(CURL_GLOBAL_ALL) != 0)
                throw std::runtime_error("failed to initialize curl");
        }

        ~CURLSetup()
        {
            curl_global_cleanup();
        }
    };

    static CURLSetup curl_setup;

    struct MessageLoggerData
    {
        static std::mutex m_mutex;
        static bool use_buffer;
        static std::vector<std::pair<std::string, log_level>> m_buffer;
    };

    std::mutex MessageLoggerData::m_mutex;
    bool MessageLoggerData::use_buffer(false);
    std::vector<std::pair<std::string, log_level>> MessageLoggerData::m_buffer({});

    //--- Concurrency resources / thread-handling
    //------------------------------------------------------------------

    static std::atomic<MainExecutor*> main_executor{ nullptr };

    static std::unique_ptr<MainExecutor> default_executor;
    static std::mutex default_executor_mutex;  // TODO: replace by sychronized_value once available

    MainExecutor& MainExecutor::instance()
    {
        if (!main_executor)
        {
            // When no MainExecutor was created before we create a static one.
            std::scoped_lock lock{ default_executor_mutex };
            if (!main_executor)  // double check necessary to avoid data race
            {
                default_executor = std::make_unique<MainExecutor>();
                assert(main_executor == default_executor.get());
            }
        }

        return *main_executor;
    }

    void MainExecutor::stop_default()
    {
        std::scoped_lock lock{ default_executor_mutex };
        default_executor.reset();
    }

    MainExecutor::MainExecutor()
    {
        MainExecutor* expected = nullptr;
        if (!main_executor.compare_exchange_strong(expected, this))
            throw MainExecutorError("attempted to create multiple main executors");
    }

    MainExecutor::~MainExecutor()
    {
        close();
        main_executor = nullptr;
    }

    //---- Singletons from this library ------------------------------------------------------------

    namespace singletons
    {
        template <typename T>
        struct Singleton : public T
        {
            using T::T;
        };

        template <typename T, typename D>
        T& init_once(std::unique_ptr<T, D>& ptr)
        {
            static std::once_flag init_flag;
            std::call_once(init_flag, [&] { ptr = std::make_unique<T>(); });
            // In case the object was already created and destroyed, we make sure it is clearly
            // visible (no undefined behavior).
            if (!ptr)
            {
                throw mamba::mamba_error(
                    fmt::format("attempt to use {} singleton instance after destruction",
                                typeid(T).name()),
                    mamba_error_code::internal_failure);
            }

            return *ptr;
        }

        static std::unique_ptr<Singleton<Context>> context;
        static std::unique_ptr<Singleton<Console>> console;
        static std::unique_ptr<Singleton<Configuration>> config;
        static std::unique_ptr<Singleton<ChannelCache>> channel_cache;
        static std::unique_ptr<Singleton<ChannelContext>> channel_context;
        static std::unique_ptr<Singleton<validate::TimeRef>> time_ref;
    }


    Configuration& Configuration::instance()
    {
        return singletons::init_once(singletons::config);
    }

    ChannelContext& ChannelContext::instance()
    {
        return singletons::init_once(singletons::channel_context);
    }

    Context& Context::instance()
    {
        return singletons::init_once(singletons::context);
    }

    Console& Console::instance()
    {
        return singletons::init_once(singletons::console);
    }

    auto ChannelBuilder::get_cache() -> ChannelCache&
    {
        return singletons::init_once(singletons::channel_cache);
    }

}

namespace validate
{
    TimeRef& TimeRef::instance()
    {
        return mamba::singletons::init_once(mamba::singletons::time_ref);
    }
}
