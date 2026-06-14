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
    Logger();
    explicit Logger(std::filesystem::path filePath);
    ~Logger();

    Logger(const Logger&)            = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&)                 = delete;
    Logger& operator=(Logger&&)      = delete;

    enum class LogLevel {
        Trace,
        Debug,
        Info,
        Warn,
        Error,
        Fatal,
    };

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

    void trace(std::string message);
    void debug(std::string message);
    void info(std::string message);
    void warn(std::string message);
    void error(std::string message);
    void fatal(std::string message);

    void setFile(std::filesystem::path filePath);

private:
    void submitMessage(LogLevel level, std::string message);

    struct Impl;
    std::unique_ptr<Impl> mImpl;
};

} // namespace sculk
