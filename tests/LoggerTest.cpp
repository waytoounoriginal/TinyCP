#include <gtest/gtest.h>

#include <iostream>
#include <sstream>
#include <string>

#define TCP_DEBUG_LOGGING 1
#include "utils/Logger.h"

namespace {

/** Runs a lambda with std::cout redirected; returns what was printed. */
std::string CaptureOutput(const std::function<void()>& action) {
    std::ostringstream captured;
    std::streambuf* original = std::cout.rdbuf(captured.rdbuf());
    action();
    std::cout.rdbuf(original);
    return captured.str();
}

} // namespace

TEST(LoggerTest, DefaultLevelIsInfo) {
    EXPECT_EQ(Logger::instance().level(), LogLevel::INFO);
}

TEST(LoggerTest, SetLevelIsVisible) {
    Logger::instance().set_level(LogLevel::DEBUG);
    EXPECT_EQ(Logger::instance().level(), LogLevel::DEBUG);
    Logger::instance().set_level(LogLevel::INFO);
}

TEST(LoggerTest, DropsMessagesBelowThreshold) {
    Logger::instance().set_level(LogLevel::WARN);

    const std::string output = CaptureOutput([] {
        DEBUG << "hidden debug";
        INFO << "hidden info";
        WARN << "keep warn";
        ERROR << "keep error";
    });

    EXPECT_NE(output.find("[WARN] keep warn"), std::string::npos);
    EXPECT_NE(output.find("[ERROR] keep error"), std::string::npos);
    EXPECT_EQ(output.find("hidden debug"), std::string::npos);
    EXPECT_EQ(output.find("hidden info"), std::string::npos);

    Logger::instance().set_level(LogLevel::INFO);
}

TEST(LoggerTest, StreamsValues) {
    const std::string output = CaptureOutput([] {
        INFO << "seq=" << 42 << ", ack=" << 0x1234;
    });

    EXPECT_NE(output.find("[INFO] seq=42, ack=4660"), std::string::npos);
}

TEST(LoggerTest, PrintsTimestampedTaggedLine) {
    const std::string output = CaptureOutput([] {
        WARN << "hello";
    });

    EXPECT_EQ(output.find("["), 0u);
    EXPECT_NE(output.find("][WARN] hello"), std::string::npos);
}