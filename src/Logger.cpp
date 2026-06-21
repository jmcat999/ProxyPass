// Copyright © 2026 SculkCatalystMC. All rights reserved.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, version 3 of the License.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "Logger.hpp"
#include <chrono>
#include <concurrentqueue.h>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <memory>
#include <print>
#include <semaphore>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>


#ifdef _WIN32
#include <windows.h>
#endif

namespace sculk {

namespace {

using LogLevel = Logger::LogLevel;

struct MessageRecord {
    LogLevel    level{LogLevel::Info};
    std::string message{};
};

struct Task {
    enum class Kind {
        Message,
        FlushFile,
        CloseFile,
        Wait,
    } kind{Kind::Message};

    LogLevel               level{LogLevel::Info};
    std::string            moduleName{};
    std::string            message{};
    std::filesystem::path  filePath{};
    std::binary_semaphore* completion{nullptr};
};

std::string_view levelName(LogLevel level) {
    switch (level) {
    case LogLevel::Trace:
        return "Trace";
    case LogLevel::Debug:
        return "Debug";
    case LogLevel::Info:
        return "Info";
    case LogLevel::Warn:
        return "Warn";
    case LogLevel::Error:
        return "Error";
    case LogLevel::Fatal:
        return "Fatal";
    }

    return "Unknown";
}

std::string_view colorCode(LogLevel level) {
    switch (level) {
    case LogLevel::Trace:
        return "90";
    case LogLevel::Debug:
        return "36";
    case LogLevel::Info:
        return "0";
    case LogLevel::Warn:
        return "33";
    case LogLevel::Error:
        return "31";
    case LogLevel::Fatal:
        return "1;31";
    }

    return "0";
}

std::string timestamp() {
    const auto now    = std::chrono::system_clock::now();
    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;
    const auto time   = std::chrono::system_clock::to_time_t(now);
    auto       local  = std::tm{};

#ifdef _WIN32
    localtime_s(&local, &time);
#else
    localtime_r(&time, &local);
#endif

    return std::format("{:02}:{:02}:{:02}.{:03}", local.tm_hour, local.tm_min, local.tm_sec, millis);
}

std::string formatLogLine(LogLevel level, std::string_view moduleName, std::string_view message) {
    const auto ts           = timestamp();
    const auto name         = levelName(level);
    const auto scopedModule = moduleName.empty() ? std::string_view{"Logger"} : moduleName;
    return std::format("[{}] [{}] [{}] {}", ts, name, scopedModule, message);
}

std::string formatConsoleLine(LogLevel level, std::string_view moduleName, std::string_view message) {
    const auto ts           = timestamp();
    const auto name         = levelName(level);
    const auto scopedModule = moduleName.empty() ? std::string_view{"Logger"} : moduleName;
    return std::format("[{}] [\033[{}m{}\033[0m] [{}] {}", ts, colorCode(level), name, scopedModule, message);
}

std::filesystem::path absoluteLogPath(const std::filesystem::path& filePath) {
    if (filePath.empty()) {
        return {};
    }

    std::error_code error{};
    auto            absolutePath = std::filesystem::absolute(filePath, error);
    if (error) {
        return filePath;
    }
    return absolutePath;
}

struct SharedBackend {
    struct LogFile {
        std::filesystem::path path{};
        std::ofstream         stream{};
    };

    moodycamel::ConcurrentQueue<Task> mQueue{};
    std::counting_semaphore<>         mWake{0};
    std::jthread                      mWorker{};
    std::vector<LogFile>              mLogFiles{};

    SharedBackend() {
#ifdef _WIN32
        SetConsoleOutputCP(CP_UTF8);
        HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hStdout != INVALID_HANDLE_VALUE) {
            DWORD dwMode = 0;
            if (GetConsoleMode(hStdout, &dwMode)) {
                SetConsoleMode(hStdout, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
            }
        }

        HANDLE hStderr = GetStdHandle(STD_ERROR_HANDLE);
        if (hStderr != INVALID_HANDLE_VALUE) {
            DWORD dwMode = 0;
            if (GetConsoleMode(hStderr, &dwMode)) {
                SetConsoleMode(hStderr, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
            }
        }
#endif

        mWorker = std::jthread{[this](std::stop_token stopToken) { run(stopToken); }};
    }

    ~SharedBackend() {
        mWorker.request_stop();
        mWake.release();
        if (mWorker.joinable()) {
            mWorker.join();
        }
    }

    void submitMessage(LogLevel level, std::string moduleName, std::string message, std::filesystem::path filePath) {
        mQueue.enqueue(
            Task{
                .kind       = Task::Kind::Message,
                .level      = level,
                .moduleName = std::move(moduleName),
                .message    = std::move(message),
                .filePath   = std::move(filePath),
            }
        );
        mWake.release();
    }

    void submitFlushFile(std::filesystem::path filePath) {
        mQueue.enqueue(Task{.kind = Task::Kind::FlushFile, .filePath = std::move(filePath)});
        mWake.release();
    }

    void submitCloseFile(std::filesystem::path filePath) {
        mQueue.enqueue(Task{.kind = Task::Kind::CloseFile, .filePath = std::move(filePath)});
        mWake.release();
    }

    void wait() {
        std::binary_semaphore completion{0};
        mQueue.enqueue(Task{.kind = Task::Kind::Wait, .completion = &completion});
        mWake.release();
        completion.acquire();
    }

    void run(std::stop_token stopToken) {
        while (true) {
            mWake.acquire();

            Task task{};
            if (mQueue.try_dequeue(task)) {
                handleTask(std::move(task));
                continue;
            }

            if (stopToken.stop_requested()) {
                break;
            }
        }

        for (auto& logFile : mLogFiles) {
            if (logFile.stream.is_open()) {
                logFile.stream.flush();
                logFile.stream.close();
            }
        }
    }

    void handleTask(Task task) {
        switch (task.kind) {
        case Task::Kind::Message:
            emit(task.level, task.moduleName, task.message, task.filePath);
            return;
        case Task::Kind::FlushFile:
            flushFile(task.filePath);
            return;
        case Task::Kind::CloseFile:
            closeFile(task.filePath);
            return;
        case Task::Kind::Wait:
            for (auto& logFile : mLogFiles) {
                if (logFile.stream.is_open()) {
                    logFile.stream.flush();
                }
            }
            if (task.completion) {
                task.completion->release();
            }
            return;
        }
    }

    std::ofstream& streamFor(const std::filesystem::path& filePath) {
        for (auto& logFile : mLogFiles) {
            if (logFile.path == filePath) {
                if (!logFile.stream.is_open()) {
                    logFile.stream.clear();
                    logFile.stream.open(logFile.path, std::ios::out | std::ios::app);
                }
                return logFile.stream;
            }
        }

        auto& logFile = mLogFiles.emplace_back(filePath, std::ofstream{});
        logFile.stream.open(logFile.path, std::ios::out | std::ios::app);
        return logFile.stream;
    }

    void flushFile(const std::filesystem::path& filePath) {
        if (filePath.empty()) {
            return;
        }

        for (auto& logFile : mLogFiles) {
            if (logFile.path == filePath && logFile.stream.is_open()) {
                logFile.stream.flush();
                return;
            }
        }
    }

    void closeFile(const std::filesystem::path& filePath) {
        if (filePath.empty()) {
            return;
        }

        for (auto& logFile : mLogFiles) {
            if (logFile.path == filePath && logFile.stream.is_open()) {
                logFile.stream.flush();
                logFile.stream.close();
                return;
            }
        }
    }

    void
    emit(LogLevel level, std::string_view moduleName, std::string_view message, const std::filesystem::path& filePath) {
        auto* const stream  = level == LogLevel::Error || level == LogLevel::Fatal ? stderr : stdout;
        const auto  line    = formatLogLine(level, moduleName, message);
        const auto  colored = formatConsoleLine(level, moduleName, message);

        std::println(stream, "{}", colored);
        std::fflush(stream);

        if (!filePath.empty()) {
            auto& logFile = streamFor(filePath);
            if (logFile.is_open()) {
                logFile << line << '\n';
            }
        }
    }
};

SharedBackend& backend() {
    static auto instance = std::make_unique<SharedBackend>();
    return *instance;
}

} // anonymous namespace

struct Logger::Impl {
    std::string                mModuleName{};
    std::filesystem::path      mFilePath{};
    std::vector<MessageRecord> mMessages{};

    Impl(std::string moduleName, std::filesystem::path filePath)
    : mModuleName(std::move(moduleName)),
      mFilePath(std::move(filePath)) {}
};

Logger::Logger(std::string moduleName, std::filesystem::path filePath)
: mImpl(std::make_unique<Impl>(std::move(moduleName), absoluteLogPath(filePath))) {}

Logger::~Logger() { flush(); }

Logger::Logger(Logger&& other) noexcept = default;

Logger& Logger::operator=(Logger&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    flush();
    mImpl = std::move(other.mImpl);
    return *this;
}

void Logger::flushFile(std::filesystem::path filePath) { backend().submitFlushFile(absoluteLogPath(filePath)); }

void Logger::closeFile(std::filesystem::path filePath) { backend().submitCloseFile(absoluteLogPath(filePath)); }

void Logger::wait() { backend().wait(); }

void Logger::appendMessage(LogLevel level, std::string message) {
    if (!mImpl) {
        return;
    }

    mImpl->mMessages.emplace_back(level, std::move(message));
}

void Logger::flush() noexcept {
    if (!mImpl) {
        return;
    }

    for (auto& message : mImpl->mMessages) {
        backend().submitMessage(message.level, mImpl->mModuleName, std::move(message.message), mImpl->mFilePath);
    }
    mImpl->mMessages.clear();
}

void Logger::flushFile() {
    if (!mImpl) {
        return;
    }

    flush();
    backend().submitFlushFile(mImpl->mFilePath);
}

void Logger::closeFile() {
    if (!mImpl) {
        return;
    }

    flush();
    backend().submitCloseFile(mImpl->mFilePath);
}

Logger& Logger::trace(std::string message) {
    appendMessage(LogLevel::Trace, std::move(message));
    return *this;
}
Logger& Logger::debug(std::string message) {
    appendMessage(LogLevel::Debug, std::move(message));
    return *this;
}
Logger& Logger::info(std::string message) {
    appendMessage(LogLevel::Info, std::move(message));
    return *this;
}
Logger& Logger::warn(std::string message) {
    appendMessage(LogLevel::Warn, std::move(message));
    return *this;
}
Logger& Logger::error(std::string message) {
    appendMessage(LogLevel::Error, std::move(message));
    return *this;
}
Logger& Logger::fatal(std::string message) {
    appendMessage(LogLevel::Fatal, std::move(message));
    return *this;
}

} // namespace sculk
