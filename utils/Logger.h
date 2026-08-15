#ifndef TCP_FROM_SCRATCH_LOGGER_H
#define TCP_FROM_SCRATCH_LOGGER_H

#include <mutex>
#include <ostream>
#include <sstream>
#include <string>

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
 *     Logger::instance().info() << "Connection established, seq=" << seq;
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
        Line(LogLevel level) : level_{level} {}

        Line(const Line&) = delete;
        Line& operator=(const Line&) = delete;

        ~Line() {
            Logger::instance().Write(level_, stream_.str());
        }

        template <typename T>
        Line& operator<<(const T& value) {
            stream_ << value;
            return *this;
        }

    private:
        LogLevel level_;
        std::ostringstream stream_;
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

#endif // TCP_FROM_SCRATCH_LOGGER_H