// Copyright (c) 2025, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_LOGGING_HPP
#define MAMBA_CORE_LOGGING_HPP

#include <array>
#include <cassert>
#include <concepts>
#include <functional>
#include <memory>
#include <optional>
#include <source_location>
#include <sstream>
#include <string>
#include <typeindex>
#include <utility>
#include <vector>

namespace mamba
{

    enum class log_level
    {
        trace,
        debug,
        info,
        warn,
        err,
        critical,

        // Special values:
        off,
        all
    };

    struct LoggingParams
    {
        int verbosity{ 0 };
        log_level logging_level{ log_level::warn };
        std::string_view log_pattern{ "%^%-9!l%-8n%$ %v" };  // IS THIS SPECIFIC TO spdlog???
        std::size_t log_backtrace{ 0 };
    };

    enum class log_source  // "source" isnt the best way to put it, maybe channel? component? sink?
    {
        libmamba,  // default
        libcurl,
        libsolv
    };

    /// @returns The name of the specified log source as an UTF-8 null-terminated string.
    inline constexpr auto name_of(log_source source) noexcept -> const char*
    {
        switch (source)
        {
            case log_source::libmamba:
                return "libmamba";
            case log_source::libcurl:
                return "libcurl";
            case log_source::libsolv:
                return "libsolv";
        }

        // TODO(c++23): std::unreachable();
        assert(false);
        return "unknown";
    }

    /// @returns All `log_source` values as a range.
    inline constexpr auto all_log_sources() noexcept -> std::initializer_list<log_source>
    {
        return { log_source::libmamba, log_source::libcurl, log_source::libsolv };
    }

    namespace logging
    {
        // FIXME: this is a placeholder, replace it by the real thing once available
        template <typename Interface, std::size_t local_storage_size = sizeof(std::shared_ptr<Interface>)>
        using SBO_storage = std::unique_ptr<Interface>;

        struct LogRecord
        {
            std::string message;  // THINK: could be made lazy if it was a function instead, but
                                  // requires macros to be functions
            log_level level = log_level::off;
            log_source source = log_source::libmamba;
            std::source_location location = {};  // assigned explicitly to please apple-clang
        };

        // NOTE: it might make more sense to talk about sinks than sources when it comes to the
        // implementation

        template <typename T>
        concept LogHandler = std::movable<T>
                             and requires(
                                 T& handler,
                                 const T& const_handler,
                                 LoggingParams params,
                                 std::vector<log_source> sources,
                                 LogRecord log_record,
                                 std::optional<log_source> source
                             )  // no value means all sources
        {
            // REQUIREMENT: all the following operations must be thread-safe

            // TODO: how to make sure calls are noexcepts?

            //
            handler.start_log_handling(params, sources);

            //
            handler.stop_log_handling();

            //
            handler.set_log_level(params.logging_level);

            //
            handler.set_params(std::as_const(params));

            //
            handler.log(log_record);

            // enable buffering a provided number of log records, dont log until `log_backtrace()`
            // is called
            handler.enable_backtrace(size_t(42));

            // disable log buffering
            handler.disable_backtrace();

            // log buffered records
            handler.log_backtrace();

            // log buffered records without filtering
            handler.log_backtrace_no_guards();

            // flush all sources
            handler.flush();

            // flush only a specific source
            handler.flush(std::optional<log_source>{});

            // when a log's record is equal or higher than the specified level, flush
            handler.set_flush_threshold(log_level::all);
        };

        template <typename T>
        concept LogHandlerOrPtr = LogHandler<T>
                                  or (std::is_pointer_v<T> and LogHandler<std::remove_pointer_t<T>>);

        class AnyLogHandler
        {
        public:

            constexpr AnyLogHandler() = default;
            ~AnyLogHandler() = default;  // cannot be constexpr, unfortunately

            AnyLogHandler(AnyLogHandler&&) noexcept = default;
            AnyLogHandler& operator=(AnyLogHandler&&) noexcept = default;

            template <class T>
                requires(not std::is_same_v<std::remove_cvref_t<T>, AnyLogHandler>)
                        and LogHandler<T>  //
            AnyLogHandler(T&& handler);

            template <class T>
                requires(not std::is_same_v<std::remove_cvref_t<T>, AnyLogHandler>)
                        and LogHandler<T>  //
            AnyLogHandler(T* handler_ptr);

            template <class T>
                requires(not std::is_same_v<std::remove_cvref_t<T>, AnyLogHandler>)
                        and LogHandler<T>  //
            AnyLogHandler& operator=(T&& new_handler);

            template <class T>
                requires(not std::is_same_v<std::remove_cvref_t<T>, AnyLogHandler>)
                        and LogHandler<T>  //
            AnyLogHandler& operator=(T* new_handler_ptr);

            ///
            /// Pre-condition: `has_value() == true`
            auto start_log_handling(LoggingParams params, std::vector<log_source> sources) -> void;

            ///
            /// Pre-condition: `has_value() == true`
            auto stop_log_handling() -> void;

            ///
            /// Pre-condition: `has_value() == true`
            auto set_log_level(log_level new_level) -> void;

            ///
            /// Pre-condition: `has_value() == true`
            auto set_params(LoggingParams new_params) -> void;

            ///
            /// Pre-condition: `has_value() == true`
            auto log(LogRecord record) noexcept -> void;

            ///
            /// Pre-condition: `has_value() == true`
            auto enable_backtrace(size_t record_buffer_size) -> void;

            ///
            /// Pre-condition: `has_value() == true`
            auto disable_backtrace() -> void;

            ///
            /// Pre-condition: `has_value() == true`
            auto log_backtrace() noexcept -> void;

            ///
            /// Pre-condition: `has_value() == true`
            auto log_backtrace_no_guards() noexcept -> void;

            ///
            /// Pre-condition: `has_value() == true`
            auto flush(std::optional<log_source> source = {}) noexcept -> void;

            ///
            /// Pre-condition: `has_value() == true`
            auto set_flush_threshold(log_level threshold_level) noexcept -> void;

            /// @returns `true` if there is an handler object stored in `this`, `false` otherwise.
            auto has_value() const noexcept -> bool;

            /// @returns @see `has_value()`
            explicit operator bool() const noexcept
            {
                return has_value();
            }

            /// @returns An identifier for the stored object's type, or `std::nullopt` if there is
            ///          no stored object (`has_value() == false`).
            auto type_id() const noexcept -> std::optional<std::type_index>;

        private:

            struct Interface;

            template <LogHandlerOrPtr T>
            struct Wrapper;

            SBO_storage<Interface> m_storage;
        };

        static_assert(LogHandler<AnyLogHandler>);  // NEEDED? AnyLogHandler must not recursively
                                                   // host itself

        ////////////////////////////////////////////////////////////////////////////////
        // Logging System API

        // not thread-safe
        auto stop_logging() -> AnyLogHandler;

        // not thread-safe
        auto
        set_log_handler(AnyLogHandler handler, std::optional<LoggingParams> maybe_new_params = {})
            -> AnyLogHandler;

        // not thread-safe
        auto get_log_handler() -> AnyLogHandler&;

        // thread-safe
        auto set_log_level(log_level new_level) -> log_level;

        // thread-safe, value immediately obsolete
        auto get_log_level() noexcept -> log_level;

        // thread-safe, value immediately obsolete
        auto get_logging_params() noexcept -> LoggingParams;

        // thread-safe
        auto set_logging_params(LoggingParams new_params);

        // as thread-safe as handler's implementation if set
        auto log(LogRecord record) noexcept -> void;

        // as thread-safe as handler's implementation if set
        auto enable_backtrace(size_t records_buffer_size) -> void;

        // as thread-safe as handler's implementation if set
        auto disable_backtrace() -> void;

        // as thread-safe as handler's implementation if set
        auto log_backtrace() noexcept -> void;

        // as thread-safe as handler's implementation if set
        auto log_backtrace_no_guards() noexcept -> void;


        // as thread-safe as handler's implementation if set
        auto flush_logs(std::optional<log_source> source = {}) noexcept -> void;


        // as thread-safe as handler's implementation if set
        auto set_flush_threshold(log_level threshold_level) noexcept -> void;

        ///////////////////////////////////////////////////////
        // MIGT DISAPPEAR SOON
        class MessageLogger
        {
        public:

            MessageLogger(
                log_level level,
                std::source_location location = std::source_location::current()
            );
            ~MessageLogger();

            std::stringstream& stream()
            {
                // Keep this implementation inline for performance reasons.
                return m_stream;
            }

            static void activate_buffer();
            static void deactivate_buffer();
            static void print_buffer(std::ostream& ostream);

        private:

            log_level m_level;
            std::stringstream m_stream;
            std::source_location m_location;

            static void emit(LogRecord log_record);
        };
    }
}

#undef LOG
#undef LOG_TRACE
#undef LOG_DEBUG
#undef LOG_INFO
#undef LOG_WARNING
#undef LOG_ERROR
#undef LOG_CRITICAL

#define LOG(severity) mamba::logging::MessageLogger(severity).stream()
#define LOG_TRACE LOG(mamba::log_level::trace)
#define LOG_DEBUG LOG(mamba::log_level::debug)
#define LOG_INFO LOG(mamba::log_level::info)
#define LOG_WARNING LOG(mamba::log_level::warn)
#define LOG_ERROR LOG(mamba::log_level::err)
#define LOG_CRITICAL LOG(mamba::log_level::critical)

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace mamba::logging
{
    // NOTE: the following definitions are inline for performance reasons.

    // not thread-safe
    inline auto stop_logging() -> AnyLogHandler
    {
        return set_log_handler({});
    }

    // TODO: find a better name?
    template <typename Func, typename... Args>
        requires std::invocable<Func, AnyLogHandler&, Args...>
    auto call_log_handler_if_existing(Func&& func, Args&&... args) noexcept(noexcept(
        std::invoke(std::forward<Func>(func), get_log_handler(), std::forward<Args>(args)...)
    )) -> void
    {
        // TODO: consider enabling for user to specify that no check is needed (one less branch)
        if (auto& log_handler = get_log_handler())
        {
            std::invoke(std::forward<Func>(func), log_handler, std::forward<Args>(args)...);
        }
    }

    // as thread-safe as handler's implementation
    inline auto log(LogRecord record) noexcept -> void
    {
        call_log_handler_if_existing(&AnyLogHandler::log, std::move(record));
    }

    inline auto enable_backtrace(size_t records_buffer_size) -> void
    {
        call_log_handler_if_existing(&AnyLogHandler::enable_backtrace, records_buffer_size);
    }

    // as thread-safe as handler's implementation if set
    inline auto disable_backtrace() -> void
    {
        call_log_handler_if_existing(&AnyLogHandler::disable_backtrace);
    }

    // as thread-safe as handler's implementation if set
    inline auto log_backtrace() noexcept -> void
    {
        call_log_handler_if_existing(&AnyLogHandler::log_backtrace);
    }

    // as thread-safe as handler's implementation if set
    inline auto log_backtrace_no_guards() noexcept -> void
    {
        call_log_handler_if_existing(&AnyLogHandler::log_backtrace_no_guards);
    }

    // as thread-safe as handler's implementation
    inline auto flush_logs(std::optional<log_source> source) noexcept -> void
    {
        call_log_handler_if_existing(&AnyLogHandler::flush, std::move(source));
    }

    inline auto set_flush_threshold(log_level threshold_level) noexcept -> void
    {
        call_log_handler_if_existing(&AnyLogHandler::set_flush_threshold, threshold_level);
    }

    //////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////


    struct AnyLogHandler::Interface
    {
        virtual ~Interface() = default;

        virtual void start_log_handling(LoggingParams params, std::vector<log_source> sources) = 0;
        virtual void stop_log_handling() = 0;
        virtual void set_log_level(log_level new_level) = 0;
        virtual void set_params(LoggingParams new_params) = 0;
        virtual void log(LogRecord record) noexcept = 0;
        virtual void enable_backtrace(size_t record_buffer_size) = 0;
        virtual void disable_backtrace() = 0;
        virtual void log_backtrace() noexcept = 0;
        virtual void log_backtrace_no_guards() noexcept = 0;
        virtual void flush(std::optional<log_source> source) noexcept = 0;
        virtual void set_flush_threshold(log_level threshold_level) noexcept = 0;
        virtual std::type_index type_id() const = 0;
    };

    template <LogHandler T>
    T& as_ref(T& object)
    {
        return object;
    }

    template <LogHandler T>
    T& as_ref(T* object)
    {
        assert(object);
        return *object;
    }

    template <LogHandlerOrPtr T>
    struct AnyLogHandler::Wrapper : Interface
    {
        T object;

        Wrapper(T new_object)
            : object(std::move(new_object))
        {
        }

        void start_log_handling(LoggingParams params, std::vector<log_source> sources) override
        {
            as_ref(object).start_log_handling(std::move(params), std::move(sources));
        }

        void stop_log_handling() override
        {
            as_ref(object).stop_log_handling();
        }

        void set_log_level(log_level new_level) override
        {
            as_ref(object).set_log_level(new_level);
        }

        void set_params(LoggingParams new_params) override
        {
            as_ref(object).set_params(std::move(new_params));
        }

        void log(LogRecord record) noexcept override
        {
            as_ref(object).log(std::move(record));
        }

        void enable_backtrace(size_t records_buffer_size) override
        {
            as_ref(object).enable_backtrace(records_buffer_size);
        }

        void disable_backtrace() override
        {
            as_ref(object).disable_backtrace();
        }

        void log_backtrace() noexcept override
        {
            as_ref(object).log_backtrace();
        }

        void log_backtrace_no_guards() noexcept override
        {
            as_ref(object).log_backtrace_no_guards();
        }

        void flush(std::optional<log_source> source) noexcept override
        {
            as_ref(object).flush(std::move(source));
        }

        void set_flush_threshold(log_level threshold_level) noexcept override
        {
            as_ref(object).set_flush_threshold(threshold_level);
        }

        std::type_index type_id() const override
        {
            return typeid(object);
        }
    };

    template <class T>
        requires(not std::is_same_v<std::remove_cvref_t<T>, AnyLogHandler>) and LogHandler<T>
    AnyLogHandler::AnyLogHandler(T&& handler)
        : m_storage(std::make_unique<Wrapper<T>>(std::forward<T>(handler)))
    {
    }

    template <class T>
        requires(not std::is_same_v<std::remove_cvref_t<T>, AnyLogHandler>) and LogHandler<T>
    AnyLogHandler::AnyLogHandler(T* handler_ptr)
        : m_storage(std::make_unique<Wrapper<T*>>(handler_ptr))
    {
        assert(handler_ptr);
    }

    template <class T>
        requires(not std::is_same_v<std::remove_cvref_t<T>, AnyLogHandler>) and LogHandler<T>
    AnyLogHandler& AnyLogHandler::operator=(T&& new_handler)
    {
        if (m_storage and typeid(T) == m_storage->type_id())
        {
            static_cast<Wrapper<T>*>(m_storage.get())->object = std::forward<T>(new_handler);
        }
        else
        {
            m_storage = std::make_unique<Wrapper<T>>(std::forward<T>(new_handler));
        }
        return *this;
    }

    template <class T>
        requires(not std::is_same_v<std::remove_cvref_t<T>, AnyLogHandler>) and LogHandler<T>
    AnyLogHandler& AnyLogHandler::operator=(T* new_handler_ptr)
    {
        assert(new_handler_ptr);
        if (m_storage and typeid(T*) == m_storage->type_id())
        {
            static_cast<Wrapper<T>*>(m_storage.get())->object = new_handler_ptr;
        }
        else
        {
            m_storage = std::make_unique<Wrapper<T*>>(new_handler_ptr);
        }
        return *this;
    }

    inline auto
    AnyLogHandler::start_log_handling(LoggingParams params, std::vector<log_source> sources) -> void
    {
        assert(m_storage);
        m_storage->start_log_handling(std::move(params), std::move(sources));
    }

    inline auto AnyLogHandler::stop_log_handling() -> void
    {
        assert(m_storage);
        m_storage->stop_log_handling();
    }

    inline auto AnyLogHandler::set_log_level(log_level new_level) -> void
    {
        assert(m_storage);
        m_storage->set_log_level(new_level);
    }

    inline auto AnyLogHandler::set_params(LoggingParams new_params) -> void
    {
        assert(m_storage);
        m_storage->set_params(new_params);
    }

    inline auto AnyLogHandler::log(LogRecord record) noexcept -> void
    {
        assert(m_storage);
        m_storage->log(std::move(record));
    }

    inline auto AnyLogHandler::enable_backtrace(size_t record_buffer_size) -> void
    {
        assert(m_storage);
        m_storage->enable_backtrace(record_buffer_size);
    }

    inline auto AnyLogHandler::disable_backtrace() -> void
    {
        assert(m_storage);
        m_storage->disable_backtrace();
    }

    inline auto AnyLogHandler::log_backtrace() noexcept -> void
    {
        assert(m_storage);
        m_storage->log_backtrace();
    }

    inline auto AnyLogHandler::log_backtrace_no_guards() noexcept -> void
    {
        assert(m_storage);
        m_storage->log_backtrace_no_guards();
    }

    inline auto AnyLogHandler::flush(std::optional<log_source> source) noexcept -> void
    {
        assert(m_storage);
        m_storage->flush(std::move(source));
    }

    inline auto AnyLogHandler::set_flush_threshold(log_level threshold_level) noexcept -> void
    {
        assert(m_storage);
        m_storage->set_flush_threshold(threshold_level);
    }

    inline auto AnyLogHandler::has_value() const noexcept -> bool
    {
        return m_storage ? true : false;
    }

    inline auto AnyLogHandler::type_id() const noexcept -> std::optional<std::type_index>
    {
        return m_storage ? std::make_optional(m_storage->type_id()) : std::nullopt;
    }

}

#endif
