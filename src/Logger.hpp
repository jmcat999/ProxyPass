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

#pragma once
#include <atomic>
#include <chrono>
#include <concurrentqueue.h>
#include <filesystem>
#include <format>
#include <fstream>
#include <limits>
#include <semaphore>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

namespace sculk {

class Logger {
public:
    Logger();
    explicit Logger(std::filesystem::path filePath);
    ~Logger();

    Logger(const Logger&)            = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&)                 = delete;
    Logger& operator=(Logger&&)      = delete;

    template <class... Args>
    void trace(std::format_string<Args...> format, Args&&... args) {
        submitMessage(LogLevel::Trace, std::format(format, std::forward<Args>(args)...));
    }

    template <class... Args>
    void debug(std::format_string<Args...> format, Args&&... args) {
        submitMessage(LogLevel::Debug, std::format(format, std::forward<Args>(args)...));
    }

    template <class... Args>
    void info(std::format_string<Args...> format, Args&&... args) {
        submitMessage(LogLevel::Info, std::format(format, std::forward<Args>(args)...));
    }

    template <class... Args>
    void warn(std::format_string<Args...> format, Args&&... args) {
        submitMessage(LogLevel::Warn, std::format(format, std::forward<Args>(args)...));
    }

    template <class... Args>
    void error(std::format_string<Args...> format, Args&&... args) {
        submitMessage(LogLevel::Error, std::format(format, std::forward<Args>(args)...));
    }

    template <class... Args>
    void fatal(std::format_string<Args...> format, Args&&... args) {
        submitMessage(LogLevel::Fatal, std::format(format, std::forward<Args>(args)...));
    }

    void trace(std::string message) { submitMessage(LogLevel::Trace, std::move(message)); }
    void debug(std::string message) { submitMessage(LogLevel::Debug, std::move(message)); }
    void info(std::string message) { submitMessage(LogLevel::Info, std::move(message)); }
    void warn(std::string message) { submitMessage(LogLevel::Warn, std::move(message)); }
    void error(std::string message) { submitMessage(LogLevel::Error, std::move(message)); }
    void fatal(std::string message) { submitMessage(LogLevel::Fatal, std::move(message)); }

    void setFile(std::filesystem::path filePath);

private:
    enum class LogLevel {
        Trace,
        Debug,
        Info,
        Warn,
        Error,
        Fatal,
    };

    enum class TaskKind {
        Message,
        SetFile,
        Stop,
    };

    struct Task {
        TaskKind              kind{TaskKind::Message};
        LogLevel              level{LogLevel::Info};
        std::string           message{};
        std::filesystem::path filePath{};
    };

    void                    submitMessage(LogLevel level, std::string message);
    void                    submitSetFile(std::filesystem::path filePath);
    void                    submitStop();
    void                    run(std::stop_token stopToken);
    void                    handleTask(Task task);
    void                    openFile(const std::filesystem::path& filePath);
    void                    writeMessage(LogLevel level, const std::string& message);
    static std::string_view levelName(LogLevel level);
    static std::string      timestamp();

    static constexpr auto kWakeCapacity = std::numeric_limits<std::ptrdiff_t>::max();

    std::atomic_bool                       mStopping{false};
    moodycamel::ConcurrentQueue<Task>      mQueue{};
    std::counting_semaphore<kWakeCapacity> mWake{0};
    std::jthread                           mWorker{};
    std::filesystem::path                  mCurrentFilePath{};
    std::ofstream                          mLogFile{};
};

} // namespace sculk