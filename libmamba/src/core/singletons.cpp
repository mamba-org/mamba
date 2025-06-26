// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <atomic>
#include <cassert>
#include <mutex>

#include "mamba/util/build.hpp"
#include "mamba/util/synchronized_value.hpp"

extern "C"
{
#include <curl/urlapi.h>
}

#include "mamba/core/execution.hpp"
#include "mamba/core/output.hpp"

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

    //--- Dependencies singletons
    //----------------------------------------------------------------------


    class CURLSetup final
    {
    public:

        CURLSetup()
        {
#ifdef LIBMAMBA_STATIC_DEPS
            CURLsslset sslset_res;
            const curl_ssl_backend** available_backends;

            if (util::on_linux)
            {
                sslset_res = curl_global_sslset(CURLSSLBACKEND_OPENSSL, nullptr, &available_backends);
            }
            else if (util::on_mac)
            {
                sslset_res = curl_global_sslset(
                    CURLSSLBACKEND_SECURETRANSPORT,
                    nullptr,
                    &available_backends
                );
            }
            else if (util::on_win)
            {
                sslset_res = curl_global_sslset(CURLSSLBACKEND_SCHANNEL, nullptr, &available_backends);
            }

            if (sslset_res == CURLSSLSET_TOO_LATE)
            {
                LOG_ERROR << "cURL SSL init called too late, that is a bug.";
            }
            else if (sslset_res == CURLSSLSET_UNKNOWN_BACKEND || sslset_res == CURLSSLSET_NO_BACKENDS)
            {
                LOG_WARNING << "Could not use preferred SSL backend (Linux: OpenSSL, OS X: SecureTransport, Win: SChannel)"
                            << std::endl;
                LOG_WARNING << "Please check the cURL library configuration that you are using."
                            << std::endl;
            }
#endif

            if (curl_global_init(CURL_GLOBAL_ALL) != 0)
            {
                throw std::runtime_error("failed to initialize curl");
            }
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

    static util::synchronized_value<std::unique_ptr<MainExecutor>> default_executor;

    MainExecutor& MainExecutor::instance()
    {
        if (!main_executor)
        {
            // When no MainExecutor was created before we create a static one.
            auto synched_default_executor = default_executor.synchronize();
            if (!main_executor)  // double check necessary to avoid data race
            {
                *synched_default_executor = std::make_unique<MainExecutor>();
                assert(main_executor == synched_default_executor->get());
            }
        }

        return *main_executor;
    }

    void MainExecutor::stop_default()
    {
        default_executor->reset();
    }

    MainExecutor::MainExecutor()
    {
        MainExecutor* expected = nullptr;
        if (!main_executor.compare_exchange_strong(expected, this))
        {
            throw MainExecutorError(
                "attempted to create multiple main executors",
                mamba_error_code::incorrect_usage
            );
        }
    }

    MainExecutor::~MainExecutor()
    {
        close();
        main_executor = nullptr;
    }

    static std::atomic<Console*> main_console{ nullptr };

    Console& Console::instance()
    {
        if (!main_console)  // NOTE: this is a tentative check, not a perfect one, it is possible
                            // `main_console` becomes null after the if scope and before returning.
                            // A perfect check would involve locking a mutex, we want to avoid that
                            // here.
        {
            throw mamba_error(
                "attempted to access the console but it have not been created yet",
                mamba_error_code::incorrect_usage
            );
        }

        return *main_console;
    }

    bool Console::is_available()
    {
        return main_console != nullptr;
    }

    void Console::set_singleton(Console& console)
    {
        Console* expected = nullptr;
        if (!main_console.compare_exchange_strong(expected, &console))
        {
            throw mamba_error("attempted to create multiple consoles", mamba_error_code::incorrect_usage);
        }
    }

    void Console::clear_singleton()
    {
        main_console = nullptr;
    }
}
