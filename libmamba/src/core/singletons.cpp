
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
    // Other static objects from other translation units can be destroyed in parallel to the ones here
    // as C++ does not guarantee any order of destruction after `main()`.

    //--- Dependencie's singletons ----------------------------------------------------------------------

    class CURLSetup final
    {
    public:
        CURLSetup()
        {
            if (curl_global_init(CURL_GLOBAL_ALL) != 0)
                throw std::runtime_error("failed to initialize curl");
        }

        ~CURLSetup()
        {
            curl_global_cleanup();
        }
    };

    static CURLSetup curl_setup;

    //--- Concurrency resources / thread-handling ------------------------------------------------------------------

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

    //---- Singletons from this library -----------------------------------------------------------------------

    namespace singletons
    {
        template<typename T>
        void init_once(std::unique_ptr<T>& ptr)
        {
            static std::once_flag init_flag;
            std::call_once(init_flag, [&]{
                ptr = std::make_unique<T>();
            });
        }

        static std::unique_ptr<Context> context;
        static std::unique_ptr<Console> console;
        static std::unique_ptr<Configuration> config;
        static std::unique_ptr<ChannelCache> channel_cache;
        static std::unique_ptr<ChannelContext> channel_context;
        static std::unique_ptr<validate::TimeRef> time_ref;
    }


    Configuration& Configuration::instance()
    {
        singletons::init_once(singletons::config);
        return *singletons::config;
    }

    ChannelContext& ChannelContext::instance()
    {
        singletons::init_once(singletons::channel_context);
        return *singletons::channel_context;
    }

    Context& Context::instance()
    {
        singletons::init_once(singletons::context);
        return *singletons::context;
    }

    Console& Console::instance()
    {
        singletons::init_once(singletons::console);
        return *singletons::console;
    }

    auto ChannelBuilder::get_cache() -> ChannelCache&
    {
        singletons::init_once(singletons::channel_cache);
        return *singletons::channel_cache;
    }

}

namespace validate
{
    TimeRef& TimeRef::instance()
    {
        mamba::singletons::init_once(mamba::singletons::time_ref);
        return *mamba::singletons::time_ref;
    }
}
