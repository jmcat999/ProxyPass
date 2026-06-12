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
#include <cstdio>
#include <print>

namespace sculk {

Logger::Logger() : Logger(std::filesystem::path{"latest.log"}) {}

Logger::Logger(std::filesystem::path filePath) {
    openFile(filePath);
    mWorker = std::jthread{[this](std::stop_token stopToken) { run(stopToken); }};
}

Logger::~Logger() {
    submitStop();
    if (mWorker.joinable()) {
        mWorker.join();
    }
}

void Logger::setFile(std::filesystem::path filePath) { submitSetFile(std::move(filePath)); }

void Logger::submitMessage(LogLevel level, std::string message) {
    if (mStopping.load(std::memory_order_acquire)) {
        return;
    }

    mQueue.enqueue(Task{.kind = TaskKind::Message, .level = level, .message = std::move(message)});
    mWake.release();
}

void Logger::submitSetFile(std::filesystem::path filePath) {
    if (mStopping.load(std::memory_order_acquire)) {
        return;
    }

    mQueue.enqueue(Task{.kind = TaskKind::SetFile, .filePath = std::move(filePath)});
    mWake.release();
}

void Logger::submitStop() {
    const auto alreadyStopping = mStopping.exchange(true, std::memory_order_acq_rel);
    if (alreadyStopping) {
        return;
    }

    mQueue.enqueue(Task{.kind = TaskKind::Stop});
    mWake.release();
    mWorker.request_stop();
}

void Logger::run(std::stop_token stopToken) {
    while (true) {
        mWake.acquire();

        Task task{};
        if (!mQueue.try_dequeue(task)) {
            if (mStopping.load(std::memory_order_acquire) || stopToken.stop_requested()) {
                break;
            }

            continue;
        }

        handleTask(std::move(task));
    }

    if (mLogFile.is_open()) {
        mLogFile.flush();
        mLogFile.close();
    }
}

void Logger::handleTask(Task task) {
    switch (task.kind) {
    case TaskKind::Message:
        writeMessage(task.level, task.message);
        return;
    case TaskKind::SetFile:
        openFile(task.filePath);
        return;
    case TaskKind::Stop:
        mStopping.store(true, std::memory_order_release);
        return;
    }
}

void Logger::openFile(const std::filesystem::path& filePath) {
    if (mLogFile.is_open()) {
        mLogFile.flush();
        mLogFile.close();
    }

    mCurrentFilePath = filePath;
    if (mCurrentFilePath.empty()) {
        return;
    }

    mLogFile.clear();
    mLogFile.open(mCurrentFilePath, std::ios::out | std::ios::trunc);
}

void Logger::writeMessage(LogLevel level, const std::string& message) {
    auto* const stream = level == LogLevel::Error || level == LogLevel::Fatal ? stderr : stdout;
    const auto  line   = std::format("[{}] [{}] {}", timestamp(), levelName(level), message);

    std::println(stream, "{}", line);
    std::fflush(stream);

    if (mLogFile.is_open()) {
        mLogFile << line << '\n';
        mLogFile.flush();
    }
}

std::string_view Logger::levelName(LogLevel level) {
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

std::string Logger::timestamp() {
    const auto now      = std::chrono::floor<std::chrono::milliseconds>(std::chrono::system_clock::now());
    const auto zonedNow = std::chrono::zoned_time{std::chrono::current_zone(), now};
    return std::format("{:%T}", zonedNow);
}

} // namespace sculk