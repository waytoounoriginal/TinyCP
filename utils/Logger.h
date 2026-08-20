#ifndef TCP_FROM_SCRATCH_LOGGER_H
#define TCP_FROM_SCRATCH_LOGGER_H

#include <mutex>
#include <ostream>
#include <sstream>
#include <string>
#include <utility>

/** Log severity levels. */
enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARN = 2,
    ERROR = 3,
};

/** Returns the tag used when printing a level, e.g. "INFO". */
const char* LevelName(LogLevel level);

/**
 * Process-wide logger.
 *
 * A singleton, like TunDevice. Messages are streamed with the usual
 * `operator<<` and flushed to std::cout with a timestamp + level tag:
 *
 *     INFO << "Connection established, seq=" << seq;
 *
 * Lines below the configured threshold are dropped before printing.
 */
class Logger {
public:
    /** Returns the one logger. */
    static Logger& instance() {
        static Logger logger;
        return logger;
    }

    /** Sets the minimum level that gets printed. */
    void set_level(LogLevel level) noexcept {
        level_ = level;
    }

    /** Returns the current minimum printed level. */
    LogLevel level() const noexcept {
        return level_;
    }

    /** A single log line; the message is emitted on destruction. */
    class Line {
    public:
        explicit Line(LogLevel level) : level_{level}, active_{true} {}

        Line(Line&& other) noexcept
            : level_{other.level_}, stream_{std::move(other.stream_)}, active_{other.active_} {
            other.active_ = false;
        }

        Line(const Line&) = delete;
        Line& operator=(const Line&) = delete;
        Line& operator=(Line&&) = delete;

        ~Line() {
            if (active_) {
                Logger::instance().Write(level_, stream_.str());
            }
        }

        template <typename T>
        Line& operator<<(const T& value) {
            stream_ << value;
            return *this;
        }

        Line& operator<<(std::ostream& (*manip)(std::ostream&)) {
            stream_ << manip;
            return *this;
        }

    private:
        LogLevel level_;
        std::ostringstream stream_;
        bool active_{true};
    };

    Line debug() const {
        return Line{LogLevel::DEBUG};
    }

    Line info() const {
        return Line{LogLevel::INFO};
    }

    Line warn() const {
        return Line{LogLevel::WARN};
    }

    Line error() const {
        return Line{LogLevel::ERROR};
    }

private:
    Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    /** Prints one finished message, if its level is not filtered out. */
    void Write(LogLevel level, const std::string& message);

    LogLevel level_ = LogLevel::INFO;
    std::mutex mutex_;
};

#ifdef ERROR
#undef ERROR
#endif
#ifdef WARN
#undef WARN
#endif
#ifdef INFO
#undef INFO
#endif
#ifdef DEBUG
#undef DEBUG
#endif

#if defined(TCP_DEBUG_LOGGING) || defined(DEBUG_LOGGING) || (!defined(NDEBUG) && !defined(DISABLE_LOGGING))
#define TCP_LOGGING_ENABLED 1

struct LogTag {
    LogLevel level;

    template <typename T>
    Logger::Line operator<<(const T& value) const {
        Logger::Line line(level);
        line << value;
        return line;
    }

    Logger::Line operator<<(std::ostream& (*manip)(std::ostream&)) const {
        Logger::Line line(level);
        line << manip;
        return line;
    }
};

inline constexpr LogTag DEBUG{LogLevel::DEBUG};
inline constexpr LogTag INFO{LogLevel::INFO};
inline constexpr LogTag WARN{LogLevel::WARN};
inline constexpr LogTag ERROR{LogLevel::ERROR};

#else
#define TCP_LOGGING_ENABLED 0

/** Zero-overhead no-op logging tag in Release/non-debug builds */
struct NullLogTag {
    template <typename T>
    constexpr const NullLogTag& operator<<(const T&) const noexcept {
        return *this;
    }

    constexpr const NullLogTag& operator<<(std::ostream& (*)(std::ostream&)) const noexcept {
        return *this;
    }
};

inline constexpr NullLogTag DEBUG{};
inline constexpr NullLogTag INFO{};
inline constexpr NullLogTag WARN{};
inline constexpr NullLogTag ERROR{};

#endif

#endif // TCP_FROM_SCRATCH_LOGGER_H