#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <fcntl.h>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

#include <sys/select.h>
#include <sys/time.h>
// Capture all stdout output (both fprintf and std::cout) via dup2
class LogCaptureFixture : public ::testing::Test {
protected:
    void SetUp() override {
        fflush(stdout);
        std::cout.flush();

        pipe2(pipe_fd_, O_NONBLOCK);
        saved_stdout_ = dup(STDOUT_FILENO);
        dup2(pipe_fd_[1], STDOUT_FILENO);
    }

    void TearDown() override {
        fflush(stdout);
        std::cout.flush();

        dup2(saved_stdout_, STDOUT_FILENO);
        close(saved_stdout_);
        close(pipe_fd_[1]);
        close(pipe_fd_[0]);
    }

    std::string getCaptured() {
        fflush(stdout);
        std::cout.flush();

        // Set pipe read end to blocking briefly to drain
        int flags = fcntl(pipe_fd_[0], F_GETFL);
        fcntl(pipe_fd_[0], F_SETFL, flags & ~O_NONBLOCK);

        // Use a short timeout via select to avoid blocking forever
        std::string result;
        char buf[4096];
        while (true) {
            struct timeval tv = {0, 10000}; // 10ms
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(pipe_fd_[0], &fds);
            int ret = select(pipe_fd_[0] + 1, &fds, nullptr, nullptr, &tv);
            if (ret <= 0) break;
            ssize_t n = read(pipe_fd_[0], buf, sizeof(buf));
            if (n <= 0) break;
            result.append(buf, n);
        }

        // Restore non-blocking
        fcntl(pipe_fd_[0], F_SETFL, flags);
        return result;
    }

private:
    int pipe_fd_[2] = {-1, -1};
    int saved_stdout_ = -1;
};

// Override build level to enable all log levels for testing
#undef OCLEA_LOG_BUILD_LEVEL
#define OCLEA_LOG_BUILD_LEVEL OCLEA_LOG_LEVEL_TRACE

#include "logger.h"

// --- Basic output tests ---

TEST_F(LogCaptureFixture, StreamMacroProducesOutput) {
    OLOG_NOTICE_STREAM("hello world");
    std::string output = getCaptured();
    EXPECT_TRUE(output.find("[NOTICE]") != std::string::npos);
    EXPECT_TRUE(output.find("hello world") != std::string::npos);
}

TEST_F(LogCaptureFixture, StreamMacroIncludesTid) {
    OLOG_INFO_STREAM("tid test");
    std::string output = getCaptured();
    EXPECT_TRUE(output.find("(tid=") != std::string::npos);
}

TEST_F(LogCaptureFixture, StreamMacroIncludesPriority) {
    OLOG_ERROR_STREAM("error test");
    std::string output = getCaptured();
    EXPECT_TRUE(output.find("<3>") != std::string::npos);
}

TEST_F(LogCaptureFixture, StreamMacroWithOperators) {
    int value = 42;
    OLOG_NOTICE_STREAM("value=" << value << " done");
    std::string output = getCaptured();
    EXPECT_TRUE(output.find("value=42 done") != std::string::npos);
}

TEST_F(LogCaptureFixture, StreamWithTraceMacroIncludesFileInfo) {
    OLOG_TRACE_STREAM("trace test");
    std::string output = getCaptured();
    EXPECT_TRUE(output.find("test_logger.cpp") != std::string::npos);
    EXPECT_TRUE(output.find("trace test") != std::string::npos);
}

TEST_F(LogCaptureFixture, AllLevelsProduceCorrectTag) {
    OLOG_CRITICAL_STREAM("c");
    OLOG_ERROR_STREAM("e");
    OLOG_WARN_STREAM("w");
    OLOG_NOTICE_STREAM("n");
    OLOG_INFO_STREAM("i");
    OLOG_DEBUG_STREAM("d");

    std::string output = getCaptured();
    EXPECT_TRUE(output.find("[CRITICAL]") != std::string::npos);
    EXPECT_TRUE(output.find("[ERROR]") != std::string::npos);
    EXPECT_TRUE(output.find("[WARN]") != std::string::npos);
    EXPECT_TRUE(output.find("[NOTICE]") != std::string::npos);
    EXPECT_TRUE(output.find("[INFO]") != std::string::npos);
    EXPECT_TRUE(output.find("[DEBUG]") != std::string::npos);
}

TEST_F(LogCaptureFixture, EachLogLineEndsWithNewline) {
    OLOG_NOTICE_STREAM("line1");
    OLOG_NOTICE_STREAM("line2");
    std::string output = getCaptured();

    int newlines = 0;
    for (char c : output) {
        if (c == '\n') newlines++;
    }
    EXPECT_GE(newlines, 2);
}

// --- Threading tests ---

TEST_F(LogCaptureFixture, ConcurrentStreamLogsDoNotInterleave) {
    const int num_threads = 8;
    const int logs_per_thread = 100;
    std::atomic<bool> start{false};
    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([&start, t, logs_per_thread]() {
            while (!start.load()) {} // spin until all threads ready
            for (int i = 0; i < logs_per_thread; i++) {
                OLOG_NOTICE_STREAM("THREAD_" << t << "_MSG_" << i << "_END");
            }
        });
    }

    start.store(true);
    for (auto& th : threads) {
        th.join();
    }

    std::string output = getCaptured();

    // Split into lines and verify each line is complete (not interleaved)
    std::istringstream iss(output);
    std::string line;
    int valid_lines = 0;

    while (std::getline(iss, line)) {
        if (line.empty()) continue;

        // Each line should contain a complete message with _END marker
        EXPECT_TRUE(line.find("[NOTICE]") != std::string::npos)
            << "Malformed line (missing [NOTICE]): " << line;
        EXPECT_TRUE(line.find("_END") != std::string::npos)
            << "Interleaved line (missing _END): " << line;

        // Verify the THREAD_X_MSG_Y pattern is intact
        std::regex pattern("THREAD_\\d+_MSG_\\d+_END");
        EXPECT_TRUE(std::regex_search(line, pattern))
            << "Interleaved content detected: " << line;

        valid_lines++;
    }

    EXPECT_EQ(valid_lines, num_threads * logs_per_thread);
}

TEST_F(LogCaptureFixture, ConcurrentStreamWithTraceDoNotInterleave) {
    const int num_threads = 4;
    const int logs_per_thread = 50;
    std::atomic<bool> start{false};
    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([&start, t, logs_per_thread]() {
            while (!start.load()) {}
            for (int i = 0; i < logs_per_thread; i++) {
                OLOG_TRACE_STREAM("T" << t << "_I" << i << "_DONE");
            }
        });
    }

    start.store(true);
    for (auto& th : threads) {
        th.join();
    }

    std::string output = getCaptured();
    std::istringstream iss(output);
    std::string line;
    int valid_lines = 0;

    while (std::getline(iss, line)) {
        if (line.empty()) continue;
        EXPECT_TRUE(line.find("[TRACE]") != std::string::npos)
            << "Malformed line: " << line;
        EXPECT_TRUE(line.find("_DONE") != std::string::npos)
            << "Interleaved line: " << line;
        valid_lines++;
    }

    EXPECT_EQ(valid_lines, num_threads * logs_per_thread);
}

// --- Throttle tests ---

TEST_F(LogCaptureFixture, ThrottleMacroDropsMessages) {
    using namespace std::chrono_literals;

    // Log many times quickly - most should be dropped
    for (int i = 0; i < 100; i++) {
        OLOG_NOTICE_STREAM_THROTTLE(200ms, "throttled_msg_" << i);
    }

    std::string output = getCaptured();

    std::istringstream iss(output);
    std::string line;
    int line_count = 0;
    while (std::getline(iss, line)) {
        if (!line.empty()) line_count++;
    }

    // With 200ms interval and near-instant execution, expect very few lines
    EXPECT_GE(line_count, 1);
    EXPECT_LE(line_count, 3);
}

TEST_F(LogCaptureFixture, ThrottleMacroReportsDroppedCount) {
    using namespace std::chrono_literals;

    // Use a helper that calls the throttle macro from a single call site
    auto do_throttled_log = [](int id) {
        OLOG_INFO_STREAM_THROTTLE(50ms, "msg_" << id);
    };

    // First burst - will log immediately with dropped=0, then suppress the rest
    for (int i = 0; i < 50; i++) {
        do_throttled_log(i);
    }

    // Wait for the interval to pass
    std::this_thread::sleep_for(60ms);

    // This call is from the same call site, should show dropped=49
    do_throttled_log(999);

    std::string output = getCaptured();

    // The second emitted line should show a non-zero dropped count
    auto pos = output.find("msg_999");
    ASSERT_NE(pos, std::string::npos) << "msg_999 not found in: " << output;

    auto line_start = output.rfind('\n', pos);
    line_start = (line_start == std::string::npos) ? 0 : line_start + 1;
    auto line_end = output.find('\n', pos);
    std::string line = output.substr(line_start,
        (line_end == std::string::npos) ? std::string::npos : line_end - line_start);

    // Extract dropped count
    std::regex dropped_re("dropped=(\\d+)");
    std::smatch match;
    ASSERT_TRUE(std::regex_search(line, match, dropped_re))
        << "No dropped=N in line: " << line;
    int dropped = std::stoi(match[1].str());
    EXPECT_GE(dropped, 40) << "Expected ~49 dropped, got: " << dropped;
    EXPECT_LE(dropped, 49) << "Expected <=49 dropped, got: " << dropped;
}

TEST_F(LogCaptureFixture, ThrottleConcurrentAccessIsSafe) {
    using namespace std::chrono_literals;

    const int num_threads = 8;
    const int logs_per_thread = 200;
    std::atomic<bool> start{false};
    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([&start, logs_per_thread]() {
            while (!start.load()) {}
            for (int i = 0; i < logs_per_thread; i++) {
                OLOG_WARN_STREAM_THROTTLE(10ms, "concurrent_throttle");
            }
        });
    }

    start.store(true);
    for (auto& th : threads) {
        th.join();
    }

    std::string output = getCaptured();

    // Main assertion: no crash, no garbled output
    std::istringstream iss(output);
    std::string line;
    while (std::getline(iss, line)) {
        if (line.empty()) continue;
        EXPECT_TRUE(line.find("[WARN]") != std::string::npos)
            << "Malformed throttled line: " << line;
        EXPECT_TRUE(line.find("concurrent_throttle") != std::string::npos)
            << "Interleaved throttled line: " << line;
    }
}

// --- Level filtering tests ---

TEST(LogLevelTest, CheckLevelMacro) {
    // OCLEA_LOG_BUILD_LEVEL is TRACE (6) so all levels should pass
    EXPECT_TRUE(OCLEA_LOG_CHECK_LEVEL(OCLEA_LOG_LEVEL_CRITICAL));
    EXPECT_TRUE(OCLEA_LOG_CHECK_LEVEL(OCLEA_LOG_LEVEL_ERROR));
    EXPECT_TRUE(OCLEA_LOG_CHECK_LEVEL(OCLEA_LOG_LEVEL_WARNING));
    EXPECT_TRUE(OCLEA_LOG_CHECK_LEVEL(OCLEA_LOG_LEVEL_NOTICE));
    EXPECT_TRUE(OCLEA_LOG_CHECK_LEVEL(OCLEA_LOG_LEVEL_INFO));
    EXPECT_TRUE(OCLEA_LOG_CHECK_LEVEL(OCLEA_LOG_LEVEL_DEBUG));
    EXPECT_TRUE(OCLEA_LOG_CHECK_LEVEL(OCLEA_LOG_LEVEL_TRACE));
}

TEST(LogLevelTest, LevelConstants) {
    EXPECT_LT(OCLEA_LOG_LEVEL_CRITICAL, OCLEA_LOG_LEVEL_ERROR);
    EXPECT_LT(OCLEA_LOG_LEVEL_ERROR, OCLEA_LOG_LEVEL_WARNING);
    EXPECT_LT(OCLEA_LOG_LEVEL_WARNING, OCLEA_LOG_LEVEL_NOTICE);
    EXPECT_LT(OCLEA_LOG_LEVEL_NOTICE, OCLEA_LOG_LEVEL_INFO);
    EXPECT_LT(OCLEA_LOG_LEVEL_INFO, OCLEA_LOG_LEVEL_DEBUG);
    EXPECT_LT(OCLEA_LOG_LEVEL_DEBUG, OCLEA_LOG_LEVEL_TRACE);
}

// --- Off macros produce no output ---

TEST_F(LogCaptureFixture, OffMacrosProduceNoOutput) {
    OLOG_OFF("should not appear %d", 123);
    OLOG_OFF_STREAM("should not appear either");
    std::string output = getCaptured();
    EXPECT_TRUE(output.empty());
}

// --- fprintf-based macro test ---

TEST_F(LogCaptureFixture, PrintfMacroProducesOutput) {
    OLOG_NOTICE("hello %s %d", "world", 42);
    std::string output = getCaptured();
    EXPECT_TRUE(output.find("[NOTICE]") != std::string::npos);
    EXPECT_TRUE(output.find("hello world 42") != std::string::npos);
}
