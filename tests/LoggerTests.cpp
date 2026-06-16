#include "Logger.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <ranges>
#include <string>
#include <thread>
#include <vector>

namespace {

using namespace std::chrono_literals;
namespace fs = std::filesystem;

std::vector<std::string> readLines(const fs::path& path) {
    std::ifstream             file{path};
    std::vector<std::string>  lines{};
    for (std::string line{}; std::getline(file, line);) {
        lines.push_back(std::move(line));
    }
    return lines;
}

bool waitForLineCount(const fs::path& path, std::size_t expectedCount, std::chrono::milliseconds timeout = 2000ms) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (readLines(path).size() == expectedCount) {
            return true;
        }
        std::this_thread::sleep_for(10ms);
    }
    return false;
}

std::size_t countContaining(const std::vector<std::string>& lines, std::string_view needle) {
    return static_cast<std::size_t>(std::ranges::count_if(lines, [needle](const std::string& line) {
        return line.contains(needle);
    }));
}

std::string stripTimestampPrefix(const std::string& line) {
    const auto bracket = line.find("] ");
    if (bracket == std::string::npos || bracket + 2 >= line.size()) {
        return line;
    }
    return line.substr(bracket + 2);
}

std::vector<std::string> normalizeLines(const std::vector<std::string>& lines) {
    std::vector<std::string> normalized{};
    normalized.reserve(lines.size());
    for (const auto& line : lines) {
        normalized.push_back(stripTimestampPrefix(line));
    }
    return normalized;
}

std::size_t countExact(const std::vector<std::string>& lines, std::string_view needle) {
    return static_cast<std::size_t>(std::ranges::count(lines, std::string{needle}));
}

bool waitForStableLineCount(
    const fs::path&                 path,
    std::size_t                     expectedCount,
    std::chrono::milliseconds       timeout = 5000ms,
    std::chrono::milliseconds       stableFor = 100ms
) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    auto       stableAt = std::chrono::steady_clock::time_point{};

    while (std::chrono::steady_clock::now() < deadline) {
        const auto lineCount = readLines(path).size();
        if (lineCount == expectedCount) {
            if (stableAt == std::chrono::steady_clock::time_point{}) {
                stableAt = std::chrono::steady_clock::now();
            }
            if (std::chrono::steady_clock::now() - stableAt >= stableFor) {
                return true;
            }
        } else {
            stableAt = {};
        }
        std::this_thread::sleep_for(10ms);
    }
    return false;
}

bool hasCompletePrefix(const std::string& line) {
    return line.contains("[Info] [ProxyPass]")
        || line.contains("[Warn] [ProxyPass]")
        || line.contains("[Error] [ProxyPass]")
        || line.contains("[Trace] [ProxyPass]")
        || line.contains("[Debug] [ProxyPass]")
        || line.contains("[Fatal] [ProxyPass]");
}

void resetLogger() {
    sculk::Logger::setFile({});
    sculk::Logger::wait();
}

int runDestructorFlushTest(const fs::path& directory) {
    resetLogger();
    const auto path = directory / "destructor-flush.log";
    std::ofstream{path, std::ios::trunc}.close();

    sculk::Logger::setFile(path);
    {
        sculk::Logger("ProxyPass").info("first message").error("second message");
    }

    sculk::Logger::wait();
    if (!waitForLineCount(path, 2)) {
        return 1;
    }

    const auto lines = readLines(path);
    if (lines.size() != 2) {
        return 2;
    }
    if (!lines[0].contains("[Info] [ProxyPass] first message")) {
        return 3;
    }
    if (!lines[1].contains("[Error] [ProxyPass] second message")) {
        return 4;
    }
    if (countContaining(lines, "first message") != 1 || countContaining(lines, "second message") != 1) {
        return 5;
    }
    return 0;
}

int runSetFileSwitchTest(const fs::path& directory) {
    resetLogger();
    const auto firstPath  = directory / "switch-a.log";
    const auto secondPath = directory / "switch-b.log";
    std::ofstream{firstPath, std::ios::trunc}.close();
    std::ofstream{secondPath, std::ios::trunc}.close();

    sculk::Logger::setFile(firstPath);
    sculk::Logger("ProxyPass").info("message in first file");
    sculk::Logger::wait();
    if (!waitForLineCount(firstPath, 1)) {
        return 6;
    }

    sculk::Logger::setFile(secondPath);
    sculk::Logger("ProxyPass").warn("message in second file");
    sculk::Logger::wait();
    if (!waitForLineCount(secondPath, 1)) {
        return 7;
    }

    const auto firstLines  = readLines(firstPath);
    const auto secondLines = readLines(secondPath);
    if (firstLines.size() != 1 || secondLines.size() != 1) {
        return 8;
    }
    if (!firstLines[0].contains("[Info] [ProxyPass] message in first file")) {
        return 9;
    }
    if (!secondLines[0].contains("[Warn] [ProxyPass] message in second file")) {
        return 10;
    }
    if (countContaining(firstLines, "message in second file") != 0 || countContaining(secondLines, "message in first file") != 0) {
        return 11;
    }
    return 0;
}

int runMultithreadTest(const fs::path& directory) {
    resetLogger();
    const auto path = directory / "multithread.log";
    std::ofstream{path, std::ios::trunc}.close();
    sculk::Logger::setFile(path);

    constexpr int                       threadCount  = 6;
    constexpr int                       messageCount = 25;
    constexpr int                       totalCount   = threadCount * messageCount;
    std::array<std::vector<std::string>, threadCount> expectedMessages{};
    std::vector<std::jthread>           workers{};
    workers.reserve(threadCount);

    for (int threadIndex = 0; threadIndex < threadCount; ++threadIndex) {
        expectedMessages[threadIndex].reserve(messageCount);
        for (int messageIndex = 0; messageIndex < messageCount; ++messageIndex) {
            expectedMessages[threadIndex].push_back(std::format("thread {} message {}", threadIndex, messageIndex));
        }
    }

    for (int threadIndex = 0; threadIndex < threadCount; ++threadIndex) {
        workers.emplace_back([threadIndex, &expectedMessages] {
            for (const auto& message : expectedMessages[threadIndex]) {
                sculk::Logger("ProxyPass").info("{}", message);
            }
        });
    }

    workers.clear();
    sculk::Logger::wait();

    if (!waitForStableLineCount(path, totalCount, 5000ms, 200ms)) {
        return 12;
    }

    const auto lines = readLines(path);
    if (lines.size() != totalCount) {
        std::cerr << "unexpected-line-count: expected=" << totalCount << " actual=" << lines.size() << "\n";
        const auto previewCount = std::min<std::size_t>(5, lines.size());
        for (std::size_t index = 0; index < previewCount; ++index) {
            std::cerr << "head[" << index << "]: " << lines[index] << "\n";
        }
        for (std::size_t index = lines.size() > previewCount ? lines.size() - previewCount : 0; index < lines.size(); ++index) {
            std::cerr << "tail[" << index << "]: " << lines[index] << "\n";
        }
        return 13;
    }
    if (!std::ranges::all_of(lines, hasCompletePrefix)) {
        std::cerr << "invalid-prefix-detected\n";
        for (const auto& line : lines) {
            if (!hasCompletePrefix(line)) {
                std::cerr << "bad-line: " << line << "\n";
                break;
            }
        }
        return 14;
    }

    const auto normalizedLines = normalizeLines(lines);
    for (int threadIndex = 0; threadIndex < threadCount; ++threadIndex) {
        for (const auto& message : expectedMessages[threadIndex]) {
            const auto expectedLine = std::format("[Info] [ProxyPass] {}", message);
            if (countExact(normalizedLines, expectedLine) != 1) {
                std::cerr << "duplicate-or-missing-message: " << expectedLine << "\n";
                std::cerr << "line-count: " << lines.size() << "\n";
                const auto previewCount = std::min<std::size_t>(5, lines.size());
                for (std::size_t index = 0; index < previewCount; ++index) {
                    std::cerr << "head[" << index << "]: " << lines[index] << "\n";
                }
                for (std::size_t index = lines.size() > previewCount ? lines.size() - previewCount : 0; index < lines.size(); ++index) {
                    std::cerr << "tail[" << index << "]: " << lines[index] << "\n";
                }
                return 15;
            }
        }
    }
    return 0;
}

} // namespace

int main() {
    const auto directory = fs::temp_directory_path() / "proxypass-logger-tests";
    fs::create_directories(directory);

    if (const auto result = runDestructorFlushTest(directory); result != 0) {
        return result;
    }
    if (const auto result = runSetFileSwitchTest(directory); result != 0) {
        return result;
    }
    if (const auto result = runMultithreadTest(directory); result != 0) {
        return result;
    }

    sculk::Logger::setFile({});
    return 0;
}
