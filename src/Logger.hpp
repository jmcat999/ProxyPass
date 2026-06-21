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
#include <filesystem>
#include <format>
#include <memory>
#include <string>
#include <utility>

namespace sculk {

class Logger {
public:
    explicit Logger(std::string moduleName, std::filesystem::path filePath = "latest.log");
    ~Logger();

    Logger(const Logger&)            = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&& other) noexcept;
    Logger& operator=(Logger&& other) noexcept;

    enum class LogLevel {
        Trace,
        Debug,
        Info,
        Warn,
        Error,
        Fatal,
    };

    static void wait();

    template <class... Args>
    Logger& trace(std::format_string<Args...> format, Args&&... args) {
        appendMessage(LogLevel::Trace, std::format(format, std::forward<Args>(args)...));
        return *this;
    }

    template <class... Args>
    Logger& debug(std::format_string<Args...> format, Args&&... args) {
        appendMessage(LogLevel::Debug, std::format(format, std::forward<Args>(args)...));
        return *this;
    }

    template <class... Args>
    Logger& info(std::format_string<Args...> format, Args&&... args) {
        appendMessage(LogLevel::Info, std::format(format, std::forward<Args>(args)...));
        return *this;
    }

    template <class... Args>
    Logger& warn(std::format_string<Args...> format, Args&&... args) {
        appendMessage(LogLevel::Warn, std::format(format, std::forward<Args>(args)...));
        return *this;
    }

    template <class... Args>
    Logger& error(std::format_string<Args...> format, Args&&... args) {
        appendMessage(LogLevel::Error, std::format(format, std::forward<Args>(args)...));
        return *this;
    }

    template <class... Args>
    Logger& fatal(std::format_string<Args...> format, Args&&... args) {
        appendMessage(LogLevel::Fatal, std::format(format, std::forward<Args>(args)...));
        return *this;
    }

    Logger& trace(std::string message);
    Logger& debug(std::string message);
    Logger& info(std::string message);
    Logger& warn(std::string message);
    Logger& error(std::string message);
    Logger& fatal(std::string message);

    void flushFile();
    void closeFile();

    static void flushFile(std::filesystem::path filePath);
    static void closeFile(std::filesystem::path filePath);

private:
    void appendMessage(LogLevel level, std::string message);
    void flush() noexcept;

    struct Impl;
    std::unique_ptr<Impl> mImpl;
};

} // namespace sculk
