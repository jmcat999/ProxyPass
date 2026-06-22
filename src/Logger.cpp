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
#include <atomic>
#include <chrono>
#include <concurrentqueue.h>
#include <filesystem>
#include <format>
#include <fstream>
#include <limits>
#include <memory>
#include <parallel_hashmap/phmap.h>
#include <print>
#include <semaphore>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

namespace sculk {

namespace {

struct LogTask {
    std::chrono::system_clock::time_point mTimestamp{};
    std::string                           mModuleName{};
    Logger::LogLevel                      mLevel{};
    std::string                           mMessage{};
    std::shared_ptr<std::ofstream>        mLogFile{};
};

#define WRITE_LOG(FMT)     std::println(FMT "\033[0m", timestamp, moduleName, message)
#define RGB_COLOR(R, G, B) "\033[38;2;" #R ";" #G ";" #B "m"
#define RGB_RESET          "\033[0m"

void writeConsole(
    std::string_view   timestamp,
    const std::string& moduleName,
    Logger::LogLevel   level,
    const std::string& message
) {
    switch (level) {
    case Logger::LogLevel::Trace:
        return WRITE_LOG(RGB_COLOR(0, 102, 51) "[{}] [TRACE] [{}] {}");
    case Logger::LogLevel::Debug:
        return WRITE_LOG(RGB_COLOR(128, 128, 128) "[{}] [DEBUG] [{}] {}");
    case Logger::LogLevel::Warn:
        return WRITE_LOG(RGB_COLOR(255, 255, 0) "[{}] [WARN] [{}] {}");
    case Logger::LogLevel::Error:
        return WRITE_LOG(RGB_COLOR(224, 16, 0) "[{}] [ERROR] [{}] {}");
    case Logger::LogLevel::Fatal:
        return WRITE_LOG(RGB_COLOR(255, 0, 0) "[{}] [FATAL] [{}] {}");
    default:
        return WRITE_LOG(RGB_COLOR(153, 255, 255) "[{}] " RGB_COLOR(0, 255, 255) "[INFO]" RGB_RESET " [{}] {}");
    }
}

constexpr std::string_view logLevelName(Logger::LogLevel level) {
    switch (level) {
    case Logger::LogLevel::Trace:
        return "TRACE";
    case Logger::LogLevel::Debug:
        return "DEBUG";
    case Logger::LogLevel::Warn:
        return "WARN";
    case Logger::LogLevel::Error:
        return "ERROR";
    case Logger::LogLevel::Fatal:
        return "FATAL";
    default:
        return "INFO";
    }
}

std::filesystem::path normalizeLogPath(const std::filesystem::path& filePath) {
    std::error_code ec{};
    auto            absolutePath = std::filesystem::absolute(filePath, ec);
    if (ec) {
        absolutePath = filePath;
    }
    return absolutePath.lexically_normal();
}

void ensureParentDirectory(const std::filesystem::path& filePath) {
    const auto parentPath = filePath.parent_path();
    if (parentPath.empty() || std::filesystem::exists(parentPath)) {
        return;
    }

    std::error_code ec{};
    if (!std::filesystem::create_directories(parentPath, ec)) {
        std::println(stderr, "Failed to create log directory: {}", ec.message());
    }
}

void writeLogTask(const LogTask& task) {
    static auto zone = std::chrono::current_zone();
    const auto  now  = std::chrono::zoned_time{zone, std::chrono::floor<std::chrono::seconds>(task.mTimestamp)};
    writeConsole(std::format("{:%Y-%m-%d %H:%M:%S}", now), task.mModuleName, task.mLevel, task.mMessage);
    if (task.mLogFile) {
        *task.mLogFile << std::format(
            "[{:%Y-%m-%d %H:%M:%S}] [{}] [{}] {}",
            now,
            task.mModuleName,
            logLevelName(task.mLevel),
            task.mMessage
        ) << '\n';
    }
}

constexpr bool operator>(Logger::LogLevel lhs, Logger::LogLevel rhs) {
    return static_cast<std::uint8_t>(lhs) > static_cast<std::uint8_t>(rhs);
}

constexpr auto kPeriodicFlushInterval = std::chrono::seconds{1};

struct SharedBackend {
    using LogFileMap = phmap::parallel_flat_hash_map_m<std::filesystem::path, std::weak_ptr<std::ofstream>>;

    std::shared_ptr<std::ofstream>                           mDefaultLogFile{};
    LogFileMap                                               mLogFiles{};
    moodycamel::ConcurrentQueue<LogTask>                     mLogTasks{};
    std::counting_semaphore<std::numeric_limits<int>::max()> mTaskSignal{0};
    std::atomic<bool>                                        mAcceptingTasks{true};
    std::atomic<std::size_t>                                 mActiveProducers{0};
    std::jthread                                             mIoThread{};

    SharedBackend(std::filesystem::path defaultLogFilePath = "./logs/latest.log") {
        defaultLogFilePath = normalizeLogPath(defaultLogFilePath);
        ensureParentDirectory(defaultLogFilePath);

        mDefaultLogFile = std::make_shared<std::ofstream>(defaultLogFilePath, std::ios::app);
        if (!mDefaultLogFile->is_open()) {
            std::println(stderr, "Failed to open default log file: {}", defaultLogFilePath.string());
            mDefaultLogFile.reset();
        }

        mIoThread = std::jthread([this](std::stop_token stopToken) { ioThreadMain(stopToken); });
    }

    ~SharedBackend() { shutdown(); }

    std::shared_ptr<std::ofstream> getDefaultLogFile() const { return mDefaultLogFile; }

    std::shared_ptr<std::ofstream> getLogFile(const std::filesystem::path& filePath) {
        const auto normalizedPath = normalizeLogPath(filePath);

        std::shared_ptr<std::ofstream> existingLogFile{};
        mLogFiles.if_contains(normalizedPath, [&](const auto& item) { existingLogFile = item.second.lock(); });
        if (existingLogFile) {
            return existingLogFile;
        }

        ensureParentDirectory(normalizedPath);

        auto openedLogFile = std::make_shared<std::ofstream>(normalizedPath, std::ios::app);
        if (!openedLogFile->is_open()) {
            std::println(stderr, "Failed to open log file: {}", normalizedPath.string());
            return nullptr;
        }

        std::shared_ptr<std::ofstream> selectedLogFile = openedLogFile;
        mLogFiles.lazy_emplace_l(
            normalizedPath,
            [&](auto& item) {
                if (auto activeLogFile = item.second.lock()) {
                    selectedLogFile = std::move(activeLogFile);
                } else {
                    item.second = openedLogFile;
                }
            },
            [&](const auto& ctor) { ctor(normalizedPath, std::weak_ptr<std::ofstream>{openedLogFile}); }
        );
        return selectedLogFile;
    }

    struct ProducerGuard final {
        std::atomic<std::size_t>& counter;

        explicit ProducerGuard(std::atomic<std::size_t>& counter) noexcept : counter(counter) {}
        ProducerGuard(const ProducerGuard&)            = delete;
        ProducerGuard& operator=(const ProducerGuard&) = delete;
        ProducerGuard(ProducerGuard&&)                 = delete;
        ProducerGuard& operator=(ProducerGuard&&)      = delete;
        ~ProducerGuard() noexcept { counter.fetch_sub(1, std::memory_order_acq_rel); }
    };

    void processLogMessage(
        std::chrono::system_clock::time_point&& timePoint,
        std::string_view                        moduleName,
        Logger::LogLevel                        level,
        std::string&&                           message,
        std::shared_ptr<std::ofstream>          logFile
    ) {
        if (!mAcceptingTasks.load(std::memory_order_acquire)) {
            return;
        }

        mActiveProducers.fetch_add(1, std::memory_order_acq_rel);
        ProducerGuard guard{mActiveProducers};

        if (!mAcceptingTasks.load(std::memory_order_acquire)) {
            return;
        }

        if (!mLogTasks.enqueue(
                LogTask{std::move(timePoint), std::string(moduleName), level, std::move(message), std::move(logFile)}
            )) {
            std::println(stderr, "Failed to enqueue log task");
            return;
        }

        mTaskSignal.release();
    }

    void shutdown() {
        const bool wasAccepting = mAcceptingTasks.exchange(false, std::memory_order_acq_rel);
        if (wasAccepting) {
            while (mActiveProducers.load(std::memory_order_acquire) != 0) {
                std::this_thread::yield();
            }
        }

        if (mIoThread.joinable()) {
            mIoThread.request_stop();
            mTaskSignal.release();
            mIoThread.join();
        }

        mLogFiles.clear();
        mDefaultLogFile.reset();
    }

    void flushActiveLogFiles() {
        std::vector<std::shared_ptr<std::ofstream>> files{};
        if (mDefaultLogFile) {
            files.emplace_back(mDefaultLogFile);
        }

        mLogFiles.for_each([&](const auto& item) {
            if (auto logFile = item.second.lock()) {
                files.emplace_back(std::move(logFile));
            }
        });

        for (auto& file : files) {
            file->flush();
        }
    }

    void ioThreadMain(std::stop_token stopToken) {
        LogTask    task{};
        bool       hasPendingFileWrites = false;
        const auto isUrgentLevel        = [](Logger::LogLevel level) { return level > Logger::LogLevel::Error; };
        auto       lastPeriodicFlush    = std::chrono::steady_clock::now();

        while (true) {
            mTaskSignal.acquire();

            while (mLogTasks.try_dequeue(task)) {
                writeLogTask(task);

                if (task.mLogFile) {
                    hasPendingFileWrites = true;
                    if (isUrgentLevel(task.mLevel)) {
                        task.mLogFile->flush();
                    }
                }

                const auto now = std::chrono::steady_clock::now();
                if (hasPendingFileWrites && now - lastPeriodicFlush >= kPeriodicFlushInterval) {
                    flushActiveLogFiles();
                    hasPendingFileWrites = false;
                    lastPeriodicFlush    = now;
                }
            }

            if (stopToken.stop_requested() && mActiveProducers.load(std::memory_order_acquire) == 0) {
                while (mLogTasks.try_dequeue(task)) {
                    writeLogTask(task);
                    if (task.mLogFile) {
                        hasPendingFileWrites = true;
                        if (isUrgentLevel(task.mLevel)) {
                            task.mLogFile->flush();
                        }
                    }
                }

                if (hasPendingFileWrites) {
                    flushActiveLogFiles();
                }
                break;
            }
        }
    }
};

SharedBackend& backend() {
    static SharedBackend instance{};
    return instance;
}

} // anonymous namespace

struct Logger::Impl {
    std::string                    mModuleName{};
    std::shared_ptr<std::ofstream> mLogFile{};
    std::atomic<LogLevel>          mLogLevel{};

    Impl(std::string_view moduleName)
    : mModuleName(moduleName),
      mLogFile(backend().getDefaultLogFile()),
      mLogLevel(LogLevel::Info) {}
};

Logger::Logger(std::string_view moduleName) : mImpl(std::make_unique<Impl>(moduleName)) {}

Logger::~Logger() = default;

Logger::Logger(Logger&& other) noexcept            = default;
Logger& Logger::operator=(Logger&& other) noexcept = default;

void Logger::setFile(std::filesystem::path filePath) { mImpl->mLogFile = backend().getLogFile(filePath); }

void Logger::log(LogLevel level, std::string&& message) {
    if (level < mImpl->mLogLevel.load(std::memory_order_relaxed)) {
        return;
    }
    backend().processLogMessage(
        std::chrono::system_clock::now(),
        mImpl->mModuleName,
        level,
        std::move(message),
        mImpl->mLogFile
    );
}

void Logger::setLevel(LogLevel level) noexcept { mImpl->mLogLevel.store(level, std::memory_order_relaxed); }

} // namespace sculk
