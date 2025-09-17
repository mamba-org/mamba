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


// TODO: rename these macros in a more friendly/conflict-averse manner, with a mamba prefix for
// example.
#undef LOG
#undef LOG_TRACE
#undef LOG_DEBUG
#undef LOG_INFO
#undef LOG_WARNING
#undef LOG_ERROR
#undef LOG_CRITICAL

// clang-format off
#define LOG(severity)   mamba::logging::MessageLogger(severity).stream()
#define LOG_TRACE       LOG(mamba::log_level::trace)
#define LOG_DEBUG       LOG(mamba::log_level::debug)
#define LOG_INFO        LOG(mamba::log_level::info)
#define LOG_WARNING     LOG(mamba::log_level::warn)
#define LOG_ERROR       LOG(mamba::log_level::err)
#define LOG_CRITICAL    LOG(mamba::log_level::critical)
// clang-format on

namespace mamba
{
    /** Level of logging, used to filter out logs which are at a lower level than the current one.
        @see `mamba::LoggingParams`
        @see `mamba::logging::LogRecord`
        @see `mamba::logging::set_log_level`
     */
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

    inline constexpr auto operator<=>(log_level left, log_level right) noexcept
    {
        return static_cast<int>(left) <=> static_cast<int>(right);
    }

    inline constexpr auto operator<=>(log_level left, unsigned long right) noexcept
    {
        return static_cast<unsigned long>(left) <=> right;
    }

    /// @returns The name of the specified log level as an UTF-8 null-terminated string.
    inline constexpr auto name_of(log_level level) noexcept -> const char*
    {
        constexpr std::array names{ "trace", "debug",    "info", "warning",
                                    "error", "critical", "off",  "all" };

        assert(level < sizeof(names));
        return names.at(static_cast<size_t>(level));
    }

    /** Parameters for the logging system.
     */
    struct LoggingParams
    {
        /// Minimum level a log record must have to not be filtered out.
        log_level logging_level{ log_level::warn };

        /** Number of log records to keep in the backtrace history.
            The backtrace feature will be enabled only if the value
            is different from `0`.
        */
        std::size_t log_backtrace{ 0 };

        /// Formatting pattern to use in formatted logs.
        std::string_view log_pattern{ "%^%-9!l%-8n%$ %v" };  // FIXME: IS THIS SPECIFIC TO spdlog???

        auto operator==(const LoggingParams& other) const noexcept -> bool = default;
    };

    /** Specifies the source a `LogRecord` is originating from.
        This is mainly useful for debugging issues coming from
        dependencies that have logging callbacks.
    */
    enum class log_source
    {
        libmamba,  // default
        libcurl,
        libsolv,

        tests,  // only used for testing
    };

    inline constexpr auto operator<=>(log_source left, std::size_t right) noexcept
    {
        return static_cast<std::size_t>(left) <=> right;
    }

    /// @returns The name of the specified log source as an UTF-8 null-terminated string.
    inline constexpr auto name_of(log_source source) noexcept -> const char*
    {
        constexpr std::array names{ "libmamba", "libcurl", "libsolv", "tests" };

        assert(source < sizeof(names));
        return names.at(static_cast<std::size_t>(source));
    }

    /// @returns All `log_source` values as a range.
    // FIXME: should be constexpr but some compilers don't implement vector's constexpr destructor
    // yet
    inline auto all_log_sources() noexcept -> std::vector<log_source>
    {
        return { log_source::libmamba, log_source::libcurl, log_source::libsolv };
    }

    namespace logging
    {
        // TODO: this is a placeholder, replace it by the real thing once available.
        // The intent is to have a type doing SBO when possible, but act as unique_ptr otherwise.
        // Might require allowing to use shared_ptr too, or some kind of value_ptr.
        template <typename Interface, std::size_t local_storage_size = sizeof(std::shared_ptr<Interface>)>
        using SBO_storage = std::unique_ptr<Interface>;

        // Helper comparison of source locations, to help with testing.
        inline constexpr bool
        operator==(const std::source_location& left, const std::source_location& right)
        {
            return std::string_view(left.file_name()) == std::string_view(right.file_name())
                   && std::string_view(left.function_name())
                          == std::string_view(right.function_name())
                   && left.line() == right.line() && left.column() == right.column();
        }

        /** All the information about a log.

            @see `mamba::logging::log`
            @see `mamba::logging::AnyLogHandler::log`
            @see The `LOG_...` macros
         */
        struct LogRecord
        {
            /// Message to be printed/captured in the logging implementation.
            std::string message;  // THINK: could be made lazy if it was a function instead, but
                                  // requires macros to be functions

            /// Level of this log. If lower than the current level, this log will be ignored.
            log_level level = log_level::off;

            /// Origin of this log.
            log_source source = log_source::libmamba;

            /// Source location of this log if available, otherwise empty.
            std::source_location location = {};  // assigned explicitly to please apple-clang

            // comparisons are mainly used for testing
            auto operator==(const LogRecord& other) const noexcept -> bool = default;
        };

        /** Reason why we are stopping the logging system.
            This is mainly used to inform implementations as to why
            `stop_logging` is being called. Depending on the situation an implementation
            can decide to do nothing if it is a program exit.

            @see `mamba::logging::stop_logging` for details.
        */
        enum class stop_reason
        {
            manual_stop,  ///< The stop was requested by user-code, this is not a program exit
                          ///< situation.
            program_exit  ///< We are in the process of exiting the program (either after `main()`
                          ///< or through `exit()` call).
        };

        // clang-format off
        /** Requirements for types which provides log handling implementations.

            While all requirements specified here are interface based,
            there are also some important behavior requirements, as specified for each
            expression below.

            The most important requirement is the implementation must implement
            all the required functions or expressions in a thread-safe manner.
            We cannot guarantee which thread will use these operations.

            The implementation must be at least move-enabled.
         */
        template <typename T>
        concept LogHandler = requires(
                                T& handler,
                                LoggingParams params,
                                std::vector<log_source> sources,
                                LogRecord log_record,
                                std::optional<log_source> source // no value means all sources
                            )
        {
            /// REQUIREMENT: All the following operations must be thread-safe.

            /** Called once by the logging system once the handler is registered as the current active one.

                The intent is to allow the implementation to allocate the necessary
                resources or proceeds to cleanup or prepare whatever it needs to before
                starting receiving log records.

                The implementation must keep track of the logging parameters and take them into account as much as possible.

                No other specified operations will be used by the logging system before this one.
                After this operation ends, all other specified operations might be called
                concurrently.

                This operation must be thread-safe.

                @see `mamba::logging::set_log_handler`
                @see `mamba::logging::AnyLogHandler::start_log_handling`

                @param params  Logging system parameters values currently set in the logging system.
                @param sources List of possible `LogRecords` origins used by libmamba. This is
                               mostly used in implementations that need to setup separate log sinks depending
                               on the source.
            */
            handler.start_log_handling(params, sources);

            /** When this log handler was registered in the logging system and later
                another implementation object is registered to replace it or while stopping the logging system,
                this operation will be called once after unregistering the object from the logging system.

                It could also be called once while the program exits without calling `mamba::logging::stop_logging`, for example when `::exit(0);` is called.
                Naturally this will always be called before the implementation's destructor.

                The intent is to allow the implementation to cleanup or release resources that wont be necessary anymore after this call,
                but not destroy the object at this point.

                No other specified operations will be used by the logging system after this one.

                This operation must be thread-safe.

                @param stop_reason Reason why this function was called, mainly used to inform the implementation about
                                   the ongoing context while stopping the logging system.
                                   @see `mamba::logging::stop_logging` for details.

                @see `mamba::logging::set_log_handler`
                @see `mamba::logging::stop_logging`
                @see `mamba::logging::AnyLogHandler::~AnyLogHandler`
                @see `mamba::logging::AnyLogHandler::stop_log_handling`
             */
            handler.stop_log_handling(stop_reason::manual_stop);


            /** While registered in it, called by the logging system when it's current logging system log level changed.

                The implementation must keep track of the current logging system logging level and
                filter out log records which are of a lower log level.

                This operation must be thread-safe.

                @see `mamba::logging::set_log_level`
                @see `mamba::logging::AnyLogHandler::set_log_level`
            */
            handler.set_log_level(params.logging_level);

            /** While registered in it, called by the logging system when it's configuration changed.

                The implementation must keep track of the current logging system configuration and
                apply it's parameters as expected.

                This operation must be thread-safe.

                @see `mamba::logging::set_logging_params`
                @see `mamba::logging::AnyLogHandler::set_params`
            */
            handler.set_params(params);

            /** While registered in it, called by the logging system to process a log record.

                The implementation must ignore log records with a log level lower then the current
                logging system logging level.
                If the log record is not ignored, it must be either sent in a logging sink, or
                if backtrace is enabled, it must be pushed in the backtrace history.

                The implementation is free to flush or not it's internal sinks when the provided log
                records has a lower logging level than the current flush threshold.
                However if the log record has a level greater or equal than the current flush threshold,
                the implementation must flush the sink related to the log record's source.

                This operation must be thread-safe.

                @see `mamba::logging::log`
                @see `mamba::logging::AnyLogHandler::log`
                @see The `LOG_...` macros.
            */
            handler.log(log_record);

            /** While registered in it, called by the logging system when enabling, configuring or disabling
                the backtrace functionality.

                A specified size of zero means disabling the backtrace feature.
                Any other values means the backtrace functionality is enabled.

                FIXME: do we allow implementation to ignore/no-op this call?

                The backtrace functionality must be provided by the implementation.
                If the feature is enabled, log records which are not filtered out by their log levels must be
                kept in order in an history buffer of the specified size.

                The implementation must keep track of only the last log records pushed in the
                backtrace. If the history size is already equal to the specified size and
                a new log should be pushed in the history, the implementation must remove the oldest
                log record and push the new log record in the history.

                Log records in the backtrace must only be sent to logging sinks once either
                `log_backtrace` or `log_backtrace_no_guards` is called.

                This operation must be thread-safe.

                @see `mamba::logging::enable_backtrace`
                @see `mamba::logging::log_backtrace`
                @see `mamba::logging::AnyLogHandler::enable_backtrace`
                @see `mamba::logging::AnyLogHandler::log_backtrace`

            */
            handler.enable_backtrace(size_t(42));

            /** While registered in it, called by the logging system when the current backtrace history of
                log records needs to be sent to the implementation's logging sinks.
                After this call, the backtrace history must be empty.
                The implementation must ignore this call if the backtrace functionality is not enabled. (FIXME: or implemented?)

                This operation must be thread-safe.

                @see `mamba::logging::log_backtrace`
                @see `mamba::logging::AnyLogHandler::log_backtrace`
            */
            handler.log_backtrace();

            /** While registered in it, called by the logging system when the current backtrace history of
                log records needs to be sent to the implementation's logging sinks but without filtering the logging level of the log records.

                This operation must be thread-safe.

                @see `mamba::logging::log_backtrace_no_guards`
                @see `mamba::logging::AnyLogHandler::log_backtrace_no_guards`
            */
            handler.log_backtrace_no_guards();

            /** While registered in it, called by the logging system when all the logging sinks from the
                implementation needs to be flushed.

                The implementation is also free to flush at any other time but must guarantee
                the flush is done after then end of this operation.

                This operation must be thread-safe.

                @see `mamba::logging::flush_logs`
                @see `mamba::logging::AnyLogHandler::flush`
            */
            handler.flush();

            /** While registered in it, called by the logging system when the logging sink from the
                implementation associated to the specified source needs to be flushed.

                The implementation is also free to flush at any other time but must guarantee
                the flush is done after then end of this operation.

                This operation must be thread-safe.

                @see `mamba::logging::flush_logs`
                @see `mamba::logging::AnyLogHandler::flush`
            */
            handler.flush(log_source::libmamba);

            /** While registered in it, called by the logging system when the flush threshold
                changed.

                The flush threshold is the logging level for which if a log record is pushed
                and has this level or higher, the implementation must flush it's sinks immediately.

                If the specified level is "all", the implementation must flush at every call
                to `log`.

                The implementation is also free to flush at any other time but must guarantee
                the flush is done after then end of this operation.

                This operation must be thread-safe.

                @see `mamba::logging::set_flush_threshold`
                @see `mamba::logging::AnyLogHandler::set_flush_threshold`

            */
            handler.set_flush_threshold(log_level::all);
        };
        // clang-format on

        /** Matches `LogHandler` types which are also move-enabled.
            @see `mamba::logging::LogHandler`
        */
        template <typename T>
        concept LogHandler_Moveable = LogHandler<T> and std::movable<T>;

        template <typename T>
        concept LogHandlerPtr = std::is_pointer_v<T> and LogHandler<std::remove_pointer_t<T>>;

        /** Matches either a type of a log handler implementation satisfying the requirements of
            `mamba::logging::LogHandler`, or a pointer to such type.
        */
        template <typename T>
        concept LogHandlerOrPtr = LogHandler_Moveable<T> or LogHandlerPtr<T>;

        /** Stores or refers to a log handler implementation object which must satisfy the
            requirements from `mamba::logging::LogHandler`.

            Used in the logging system functions to register a log handler implementation.
            (This is an API barrier, separating the logging system implementation from the log
            handler implementation, whatever it is).

            The log handler implementation can be passed to this object through constructor or
            assignment either:
            - by copy or move, in which case it will be owned by this object;
            - using a pointed to the implementation, in which case this object will only refer to
            the existing implementation object and that object MUST have a greater lifetime than
            this object.

            Only operations specified as such are thread-safe.

            @see `mamba::logging::LogHandler`
        */
        class AnyLogHandler
        {
        public:

            /** Constructs this object empty.

                Calling any non-const operations on this object is undefined behavior
                unless it is an assignment operation.

                post-conditions:
                    - `this->has_value() == false`

                @see `mamba::logging::LogHandler`
            */
            constexpr AnyLogHandler() noexcept = default;

            /** Destructor calling `stop_log_handling()` if this is the log handler
                currently registered in the logging system and `has_value() == true`.
            */
            ~AnyLogHandler();

            // move is allowed
            AnyLogHandler(AnyLogHandler&&) noexcept = default;
            AnyLogHandler& operator=(AnyLogHandler&&) noexcept = default;

            // TODO: implement/allow copies (might require value_ptr)

            /** Construction by storing a provided moved log handler implementation.
                This will take ownership of the moved object.

                post-conditions:
                    - `has_value() == true`

                @param handler An moveable instance of a log handler type satisfying
                               `mamba::logging::LogHandler` requirements.

                @see `mamba::logging::LogHandler`
            */
            template <class T>
                requires(not std::is_same_v<std::remove_cvref_t<T>, AnyLogHandler>)
                        and LogHandler_Moveable<T>  //
            AnyLogHandler(T&& handler);

            /** Construction by storing the provided pointer to a log handler implementation.

                This will not take ownership of the pointed object.
                The pointed object MUST be valid and it's lifetime MUST be greater than the lifetime
                of `this`.

                This is mainly useful for situations where  users cannot or do not want to move the
                implementation.

                pre-conditions:
                    - `handler_ptr != nullptr`
                    - `handler_ptr` must point to a valid object.

                post-conditions:
                    - `has_value() == true`

                @param handler_ptr A pointer to a log handler object which type satisfies
                                   `mamba::logging::LogHandler` requirements.

                @see `mamba::logging::LogHandler`
            */
            template <class T>
                requires(not std::is_same_v<std::remove_cvref_t<T>, AnyLogHandler>)
                        and LogHandler<T>  //
            AnyLogHandler(T* handler_ptr);

            /** Stores a provided moved log handler implementation.

                This will take ownership of the moved object.
                If `this` already stores an object before this call, it is destroyed before we store
                the provided one.

                post-conditions:
                    - `has_value() == true`

                @param new_handler An moveable instance of a log handler type satisfying
                                   `mamba::logging::LogHandler` requirements.

                @see `mamba::logging::LogHandler`
            */
            template <class T>
                requires(not std::is_same_v<std::remove_cvref_t<T>, AnyLogHandler>)
                        and LogHandler_Moveable<T>  //
            AnyLogHandler& operator=(T&& new_handler);

            /** Stores the provided pointer to a log handler implementation.

                This will not take ownership of the pointed object.
                The pointed object MUST be valid and it's lifetime MUST be greater than the lifetime
                of `this`.

                If `this` already stores an object before this call, it is destroyed before we store
                the provided pointer.

                This is mainly useful for situations where users cannot or do not want to move the
                implementation.

                pre-conditions:
                    - `new_handler_ptr != nullptr`
                    - `new_handler_ptr` must point to a valid object.

                post-conditions:
                    - `has_value() == true`

                @param new_handler_ptr A pointer to a log handler object which type satisfies
               `mamba::logging::LogHandler` requirements.

                @see `mamba::logging::LogHandler`
            */
            template <class T>
                requires(not std::is_same_v<std::remove_cvref_t<T>, AnyLogHandler>)
                        and LogHandler<T>  //
            AnyLogHandler& operator=(T* new_handler_ptr);

            /** Calls the same function with the same arguments (if any) in the implementation
                object pointed or stored in `this`,
                @see `mamba::logging::LogHandler` specifications for the requirements and behavior
                that the implementation must perform.

                This call shall be thread-safe because the stored or pointed implementation is
                required to be thread-safe.

                pre-condition: `has_value() == true`
            */
            ///@{
            auto start_log_handling(LoggingParams params, std::vector<log_source> sources) -> void;
            auto stop_log_handling(stop_reason reason = stop_reason::manual_stop) -> void;
            auto set_log_level(log_level new_level) -> void;
            auto set_params(LoggingParams new_params) -> void;
            auto log(LogRecord record) -> void;
            auto enable_backtrace(size_t record_buffer_size) -> void;
            auto log_backtrace() -> void;
            auto log_backtrace_no_guards() -> void;
            auto flush(std::optional<log_source> source = {}) -> void;
            auto set_flush_threshold(log_level threshold_level) -> void;
            ///@}

            /** @returns `true` if there is an log handler object or pointer stored in `this`,
                         `false` otherwise.
            */
            auto has_value() const noexcept -> bool;

            /// @returns @see `has_value()`
            explicit operator bool() const noexcept
            {
                return has_value();
            }

            /// @returns An identifier for the stored object's type, or `std::nullopt` if there is
            ///          no stored object (`has_value() == false`).
            auto type_id() const noexcept -> std::optional<std::type_index>;

            /** Unsafe access to log handler implementation stored in this object.

                Mainly used for testing and contracts checking.

                This is not thread-safe.

                @tparam X `LogHandler` type that is assumed stored in this object.

                @returns A pointer to the stored `LogHandler`
                            - if we store an value(`has_value() == true`)
                            - and `X` matches the stored handler's type (`*type_id() == typeid(X)`),
                         otherwise return `nullptr`.
                         If `this` is `const`, the returned pointer is pointing to `const`.
            */
            ///@{
            template <LogHandler X>
            auto unsafe_get() -> X*;

            template <LogHandler X>
            auto unsafe_get() const -> const X*;
            ///@}

            /** Unsafe access to log handler implementation pointer stored in this object.

                Mainly used for testing and contracts checking.

                This is not thread-safe.

                @tparam P Pointer type to a `LogHandler` that is assumed stored in this object.

                @returns The value of the pointer stored
                           - if there is one (`has_value() == true`)
                           - and if the pointed handler's type is `P` (`*type_id() == typeid(X)`),
                         otherwise returns `nullptr`.
                         If `this` is `const`, the returned pointer is pointing to `const`.
            */
            ///@{
            template <LogHandlerPtr P>
            auto unsafe_get() -> std::remove_const_t<P>;

            template <LogHandlerPtr P>
            auto unsafe_get() const -> const std::remove_pointer_t<P>*;
            ///@}

        private:

            struct Interface;

            template <LogHandlerOrPtr T>
            struct Wrapper;

            SBO_storage<Interface> m_storage;
        };

        static_assert(LogHandler<AnyLogHandler>);  // FIXME: NEEDED? AnyLogHandler must not
                                                   // recursively host itself

        ////////////////////////////////////////////////////////////////////////////////
        // Logging System API

        /** Stops the logging system.

            Unregisters the current log handler if any is registered.
            This is equivalent to `set_log_handler({});`,
            @see `mamba::logging::set_log_handler` for  details.

            This call is NOT thread-safe.

            @param reason The reason why this function has been called. This is to inform the
                          implementation about the context of the stop. The implementation could
                          decide to do something different if it could be re-used or when we know it
                          is the end of the program. Implementations which relies on libraries
                          having global objects will probably need to do nothing and expect the
                          library to handle program exit adequately (spdlog is a good example
                          of that case).

            @returns The registered log handler if any.
        */
        auto stop_logging(stop_reason reason = stop_reason::manual_stop) -> AnyLogHandler;

        /** Registers a log handler to use in the logging system, or no log handler.

            The other logging operations will be no-op if no log handler is registered.

            If a log handler is registered before this call, this function call
            `AnyLogHandler::stop_logging` with the same arguments. If a log handler with value is
            provided as argument, this function will call this handler's
            `AnyLogHandler::start_logging` function with the same `LoggingParams` as argument.
            @see `mamba::logging:LogHandler` and @see `mamba::logging::AnyLogHandler` for details.

            This call is NOT thread-safe.

            @param handler The log handler implementation to use when the other operations of the
                           logging system are called, or if no log handler
                           (`handler.has_value() == false`).

            @returns The previously registered log handler if any.
        */
        auto
        set_log_handler(AnyLogHandler handler, std::optional<LoggingParams> maybe_new_params = {})
            -> AnyLogHandler;

        /// @returns The currently registered log handler, if any. This call is NOT thread-safe.
        auto get_log_handler() -> AnyLogHandler&;

        /** Changes the logging system logging level.

            After this call, if a log handler is registered and `mamba::logging::log` is called, the
            provided log record will be ignored if it's log level is lesser than the current logging
            system log level.

            If a log handler is registered, this function call `AnyLogHandler::set_log_level` with
            the same arguments.
            @see `mamba::logging:LogHandler` and @see `mamba::logging::AnyLogHandler` for details.

            This call is thread safe as long as the log handler implementation fulfills the
            thread-safety requirements, @see `mamba::logging::LogHandler`.

            @returns The previous log level of the logging system.
        */
        auto set_log_level(log_level new_level) -> log_level;

        /// @returns The current log level of the logging system. This call is thread-safe but the
        ///          returned value must be considered immediately obsolete.
        auto get_log_level() -> log_level;

        /// @returns The current configuration of the logging system. This call is thread-safe but
        ///          the returned value must be considered immediately obsolete.
        auto get_logging_params() -> LoggingParams;

        /** Changes the logging system configuration.

            If a log handler is registered, this function calls `AnyLogHandler::set_params` with the
            same arguments.
            @see `mamba::logging:LogHandler` and @see `mamba::logging::AnyLogHandler` for details.

            This call is thread safe as long as the log handler implementation fulfills the
            thread-safety requirements, @see `mamba::logging::LogHandler`.

            @returns The previous configuration of the logging system.
        */
        auto set_logging_params(LoggingParams new_params) -> LoggingParams;

        // TODO: potential performance improvement: log(record_generator, log_level) where
        // record_generator is a callable which generates the log record but is only called AFTER we
        // filter using the provided log_level

        /** Process a log record to be sent in a logging sink to be printed if the necessary
            conditions are met.

            If a log handler is registered, this function calls `AnyLogHandler::log` with the same
            arguments, otherwise this function will do nothing.
            @see `mamba::logging:LogHandler` and @see `mamba::logging::AnyLogHandler` for details.

            The implementation must ignore log records with a log level lower then the current
            logging system logging level, @see `mamba::logging::get_log_level`.

            If the log record is not ignored, it must be either sent in a logging sink, or
            if backtrace is enabled, it must be pushed in the backtrace history,
            @see `mamba::logging::enable_backtrace` for details.

            The implementation is free to flush or not it's internal sinks when the provided log
            records has a lower logging level than the current flush threshold.
            However if the log record has a level greater or equal than the current flush threshold,
            the implementation must flush the sink related to the log record's source.

            This call is thread safe as long as the log handler implementation fulfills the
            thread-safety requirements, @see `mamba::logging::LogHandler`.

            @param record Log record to be processed.
        */
        auto log(LogRecord record) -> void;

        /** Enabling, configuring or disabling the backtrace functionality.

            If a log handler is registered, this function calls `AnyLogHandler::enable_backtrace`
            with the same arguments, otherwise this function will do nothing.
            @see `mamba::logging:LogHandler` and @see `mamba::logging::AnyLogHandler` for details.

            A specified size of zero means disabling the backtrace feature.
            Any other values means the backtrace functionality is enabled.

            If the feature is enabled, log records which are not filtered out by their log levels
            must be kept in order in an history buffer of the specified size, @see
           `mamba::logging::log`

            The implementation shall keep track of only the last log records pushed in the
            backtrace. If the history size is already equal to the specified size and
            a new log should be pushed in the history, the implementation must remove the oldest
            log record and push the new log record in the history.

            Log records in the backtrace must only be sent to logging sinks once either
            `log_backtrace` or `log_backtrace_no_guards` is called, @see
            `mamba::logging::log_backtrace`.

            This call is thread safe as long as the log handler implementation fulfills the
            thread-safety requirements, @see `mamba::logging::LogHandler`.

            @param records_buffer_size Size of the history buffer that will contain the log records.
                                       A size of zero means disabling the backtrace feature.
                                       Any other values means the backtrace functionality is
                                       enabled.
        */
        auto enable_backtrace(size_t records_buffer_size) -> void;

        /// Equivalent to `enable_backtrace(0);`. @see `mamba::logging::enable_backtrace`
        auto disable_backtrace() -> void;

        /** Sends the log records in the backtrace history to the implementation's logging sinks.

            If a log handler is registered, this function calls `AnyLogHandler::log_backtrace`,
            otherwise this function will do nothing.
            @see `mamba::logging:LogHandler` and @see `mamba::logging::AnyLogHandler` for details.

            After this call, the backtrace history shall be empty.

            This call is thread safe as long as the log handler implementation fulfills the
            thread-safety requirements, @see `mamba::logging::LogHandler`.
        */
        auto log_backtrace() -> void;

        /** Sends the log records in the backtrace history to the implementation's logging sinks,
            but without filtering the logging level of the log records.

            If a log handler is registered, this function calls
           `AnyLogHandler::log_backtrace_no_guards`, otherwise this function will do nothing.
            @see `mamba::logging:LogHandler` and @see `mamba::logging::AnyLogHandler` for details.

            After this call, the backtrace history shall be empty.

            This call is thread safe as long as the log handler implementation fulfills the
            thread-safety requirements, @see `mamba::logging::LogHandler`.
        */
        auto log_backtrace_no_guards() -> void;


        /** Flushes all the logging sinks.

            If a log handler is registered, this function calls `AnyLogHandler::flush` with the same
            argument, otherwise this function will do nothing.
            @see `mamba::logging:LogHandler` and @see `mamba::logging::AnyLogHandler` for details.

            The implementation is also free to flush at any other time but must guarantee
            the flush is done after then end of this operation.

            This call is thread safe as long as the log handler implementation fulfills the
            thread-safety requirements, @see `mamba::logging::LogHandler`.
        */
        auto flush_logs(std::optional<log_source> source = {}) -> void;


        /** Set a flush threshold, a logging level for which log records pushed in `log` will
            trigger a flush of logging sinks.

            If a log handler is registered, this function calls `AnyLogHandler::set_flush_threshold`
            with the same argument, otherwise this function will do nothing.
            @see `mamba::logging:LogHandler` and @see `mamba::logging::AnyLogHandler` for details.

            The flush threshold is the logging level for which if a log record is pushed
            and has this logging level or higher, the implementation must flush it's sinks
            immediately.

            If the specified level is "all", the implementation must flush at every call to
            `mamba::logging::log`.

            The implementation is also free to flush at any other time but must guarantee
            the flush is done after then end of this operation.

            This call is thread safe as long as the log handler implementation fulfills the
            thread-safety requirements, @see `mamba::logging::LogHandler`.

            @see `mamba::logging::log`
        */
        auto set_flush_threshold(log_level threshold_level) -> void;

        ///////////////////////////////////////////////////////
        // MIGHT DISAPPEAR SOON
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

        namespace details
        {
            // NOTE: this looks complicated because it's a workaround for `std::vector`
            // implementations which are not `constexpr` (required by c++20), we defer the vector
            // creation to the moment it's needed. Constexpr constructor is required for a type
            // which is usable in a `constinit` definition, which is required to avoid the
            // static-initialization-fiasco (at least for initialization).
            // TODO: once homebrew stl impl has `constexpr` vector, replace all this by just `using
            // MessageLoggerBuffer = vector<LogRecord>;`
            struct MessageLoggerBuffer
            {
                using buffer = std::vector<LogRecord>;

                constexpr MessageLoggerBuffer() = default;

                auto ready_records() -> buffer&
                {
                    if (not records)
                    {
                        records = buffer{};
                    }
                    return *records;
                }

                std::optional<buffer> records;
            };
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace mamba::logging
{
    // NOTE: the following definitions are inline for performance reasons.

    template <typename Func, typename... Args>
        requires std::invocable<Func, AnyLogHandler&, Args...>
    auto call_log_handler_if_existing(Func&& func, Args&&... args) -> void
    {
        // TODO: consider enabling for user to specify that no check is needed (one less branch)
        if (auto& log_handler = get_log_handler())
        {
            std::invoke(std::forward<Func>(func), log_handler, std::forward<Args>(args)...);
        }
    }

    // as thread-safe as handler's implementation
    inline auto log(LogRecord record) -> void
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
        call_log_handler_if_existing(&AnyLogHandler::enable_backtrace, 0);
    }

    // as thread-safe as handler's implementation if set
    inline auto log_backtrace() -> void
    {
        call_log_handler_if_existing(&AnyLogHandler::log_backtrace);
    }

    // as thread-safe as handler's implementation if set
    inline auto log_backtrace_no_guards() -> void
    {
        call_log_handler_if_existing(&AnyLogHandler::log_backtrace_no_guards);
    }

    // as thread-safe as handler's implementation
    inline auto flush_logs(std::optional<log_source> source) -> void
    {
        call_log_handler_if_existing(&AnyLogHandler::flush, std::move(source));
    }

    inline auto set_flush_threshold(log_level threshold_level) -> void
    {
        call_log_handler_if_existing(&AnyLogHandler::set_flush_threshold, threshold_level);
    }

    //////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////


    struct AnyLogHandler::Interface
    {
        virtual ~Interface() = default;

        virtual void start_log_handling(LoggingParams params, std::vector<log_source> sources) = 0;
        virtual void stop_log_handling(stop_reason reason) = 0;
        virtual void set_log_level(log_level new_level) = 0;
        virtual void set_params(LoggingParams new_params) = 0;
        virtual void log(LogRecord record) = 0;
        virtual void enable_backtrace(size_t record_buffer_size) = 0;
        virtual void log_backtrace() = 0;
        virtual void log_backtrace_no_guards() = 0;
        virtual void flush(std::optional<log_source> source) = 0;
        virtual void set_flush_threshold(log_level threshold_level) = 0;
        virtual std::type_index type_id() const noexcept = 0;
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

        void stop_log_handling(stop_reason reason) override
        {
            as_ref(object).stop_log_handling(reason);
        }

        void set_log_level(log_level new_level) override
        {
            as_ref(object).set_log_level(new_level);
        }

        void set_params(LoggingParams new_params) override
        {
            as_ref(object).set_params(std::move(new_params));
        }

        void log(LogRecord record) override
        {
            as_ref(object).log(std::move(record));
        }

        void enable_backtrace(size_t records_buffer_size) override
        {
            as_ref(object).enable_backtrace(records_buffer_size);
        }

        void log_backtrace() override
        {
            as_ref(object).log_backtrace();
        }

        void log_backtrace_no_guards() override
        {
            as_ref(object).log_backtrace_no_guards();
        }

        void flush(std::optional<log_source> source) override
        {
            as_ref(object).flush(std::move(source));
        }

        void set_flush_threshold(log_level threshold_level) override
        {
            as_ref(object).set_flush_threshold(threshold_level);
        }

        std::type_index type_id() const noexcept override
        {
            return typeid(object);
        }
    };

    template <class T>
        requires(not std::is_same_v<std::remove_cvref_t<T>, AnyLogHandler>) and LogHandler_Moveable<T>
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
        requires(not std::is_same_v<std::remove_cvref_t<T>, AnyLogHandler>) and LogHandler_Moveable<T>
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

    inline auto AnyLogHandler::stop_log_handling(stop_reason reason) -> void
    {
        assert(m_storage);
        m_storage->stop_log_handling(reason);
    }

    inline auto AnyLogHandler::set_log_level(log_level new_level) -> void
    {
        assert(m_storage);
        m_storage->set_log_level(new_level);
    }

    inline auto AnyLogHandler::set_params(LoggingParams new_params) -> void
    {
        assert(m_storage);
        m_storage->set_params(std::move(new_params));
    }

    inline auto AnyLogHandler::log(LogRecord record) -> void
    {
        assert(m_storage);
        m_storage->log(std::move(record));
    }

    inline auto AnyLogHandler::enable_backtrace(size_t record_buffer_size) -> void
    {
        assert(m_storage);
        m_storage->enable_backtrace(record_buffer_size);
    }

    inline auto AnyLogHandler::log_backtrace() -> void
    {
        assert(m_storage);
        m_storage->log_backtrace();
    }

    inline auto AnyLogHandler::log_backtrace_no_guards() -> void
    {
        assert(m_storage);
        m_storage->log_backtrace_no_guards();
    }

    inline auto AnyLogHandler::flush(std::optional<log_source> source) -> void
    {
        assert(m_storage);
        m_storage->flush(std::move(source));
    }

    inline auto AnyLogHandler::set_flush_threshold(log_level threshold_level) -> void
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
        return has_value() ? std::make_optional(m_storage->type_id()) : std::nullopt;
    }

    template <LogHandler X>
    auto AnyLogHandler::unsafe_get() -> X*
    {
        auto type = type_id();
        if (not type or *type != typeid(X))
        {
            return nullptr;
        }

        auto* wrapper = static_cast<AnyLogHandler::Wrapper<X>*>(m_storage.get());
        return &wrapper->object;
    }

    template <LogHandler X>
    auto AnyLogHandler::unsafe_get() const -> const X*
    {
        return const_cast<AnyLogHandler*>(this)->unsafe_get<X>();
    }

    template <LogHandlerPtr P>
    auto AnyLogHandler::unsafe_get() -> std::remove_const_t<P>
    {
        using stored_ptr_type = std::remove_pointer_t<P>*;
        auto type = type_id();
        if (not type or *type != typeid(stored_ptr_type))
        {
            return nullptr;
        }

        auto* wrapper = static_cast<AnyLogHandler::Wrapper<stored_ptr_type>*>(m_storage.get());
        return wrapper->object;
    }

    template <LogHandlerPtr P>
    auto AnyLogHandler::unsafe_get() const -> const std::remove_pointer_t<P>*
    {
        return const_cast<AnyLogHandler*>(this)->unsafe_get<P>();
    }
}

#endif
