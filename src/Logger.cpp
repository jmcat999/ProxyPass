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
#include <filesystem>
#include <fstream>
#include <limits>
#include <print>
#include <semaphore>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#endif

namespace sculk {

namespace {

using LogLevel = Logger::LogLevel;

enum class TaskKind {
    Message,
    SetFile,
};

struct Task {
    TaskKind              kind{TaskKind::Message};
    LogLevel              level{LogLevel::Info};
    std::string           message{};
    std::filesystem::path filePath{};
};

constexpr auto kWakeCapacity = std::numeric_limits<std::ptrdiff_t>::max();

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
    const auto now      = std::chrono::floor<std::chrono::milliseconds>(std::chrono::system_clock::now());
    const auto zonedNow = std::chrono::zoned_time{std::chrono::current_zone(), now};
    return std::format("{:%T}", zonedNow);
}

} // anonymous namespace

struct Logger::Impl {
    moodycamel::ConcurrentQueue<Task>      mQueue{};
    std::counting_semaphore<kWakeCapacity> mWake{0};
    std::jthread                           mWorker{};
    std::filesystem::path                  mCurrentFilePath{};
    std::ofstream                          mLogFile{};

    Impl() = default;

    explicit Impl(std::filesystem::path filePath) {
        openFile(std::move(filePath));

#ifdef _WIN32
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

    ~Impl() {
        mWorker.request_stop();
        mWake.release();
        if (mWorker.joinable()) {
            mWorker.join();
        }
    }

    void submitMessage(LogLevel level, std::string message) {
        mQueue.enqueue(Task{.kind = TaskKind::Message, .level = level, .message = std::move(message)});
        mWake.release();
    }

    void submitSetFile(std::filesystem::path filePath) {
        mQueue.enqueue(Task{.kind = TaskKind::SetFile, .filePath = std::move(filePath)});
        mWake.release();
    }

    void run(std::stop_token stopToken) {
        while (true) {
            mWake.acquire();

            Task task{};
            while (mQueue.try_dequeue(task)) {
                handleTask(std::move(task));
            }

            if (stopToken.stop_requested()) {
                break;
            }
        }

        if (mLogFile.is_open()) {
            mLogFile.flush();
            mLogFile.close();
        }
    }

    void handleTask(Task task) {
        switch (task.kind) {
        case TaskKind::Message:
            writeMessage(task.level, task.message);
            return;
        case TaskKind::SetFile:
            openFile(std::move(task.filePath));
            return;
        }
    }

    void openFile(std::filesystem::path filePath) {
        if (mLogFile.is_open()) {
            mLogFile.flush();
            mLogFile.close();
        }

        mCurrentFilePath = std::move(filePath);
        if (mCurrentFilePath.empty()) {
            return;
        }

        mLogFile.clear();
        mLogFile.open(mCurrentFilePath, std::ios::out | std::ios::trunc);
    }

    void writeMessage(LogLevel level, const std::string& message) {
        auto* const stream = level == LogLevel::Error || level == LogLevel::Fatal ? stderr : stdout;
        const auto  ts     = timestamp();
        const auto  name   = levelName(level);

        std::println(stream, "[{}] [\033[{}m{}\033[0m] {}", ts, colorCode(level), name, message);
        std::fflush(stream);

        if (mLogFile.is_open()) {
            const auto line = std::format("[{}] [{}] {}", ts, name, message);
            mLogFile << line << '\n';
            mLogFile.flush();
        }
    }
};

Logger::Logger() : Logger(std::filesystem::path{"latest.log"}) {}

Logger::Logger(std::filesystem::path filePath) : mImpl(std::make_unique<Impl>(std::move(filePath))) {}

Logger::~Logger() = default;

void Logger::submitMessage(LogLevel level, std::string message) { mImpl->submitMessage(level, std::move(message)); }

void Logger::setFile(std::filesystem::path filePath) { mImpl->submitSetFile(std::move(filePath)); }

void Logger::trace(std::string message) { submitMessage(LogLevel::Trace, std::move(message)); }
void Logger::debug(std::string message) { submitMessage(LogLevel::Debug, std::move(message)); }
void Logger::info(std::string message) { submitMessage(LogLevel::Info, std::move(message)); }
void Logger::warn(std::string message) { submitMessage(LogLevel::Warn, std::move(message)); }
void Logger::error(std::string message) { submitMessage(LogLevel::Error, std::move(message)); }
void Logger::fatal(std::string message) { submitMessage(LogLevel::Fatal, std::move(message)); }

} // namespace sculk
