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
    enum class LogLevel : std::uint8_t {
        Trace = 0,
        Debug = 1,
        Info  = 2,
        Warn  = 3,
        Error = 4,
        Fatal = 5,
    };

public:
    explicit Logger(std::string_view moduleName);
    ~Logger();

    Logger(const Logger&)            = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&& other) noexcept;
    Logger& operator=(Logger&& other) noexcept;

    void setFile(std::filesystem::path filePath);
    void setLevel(LogLevel level) noexcept;

    template <class... Args>
    void trace(std::format_string<Args...> format, Args&&... args) {
        log(LogLevel::Trace, std::format(format, std::forward<Args>(args)...));
    }

    template <class... Args>
    void debug(std::format_string<Args...> format, Args&&... args) {
        log(LogLevel::Debug, std::format(format, std::forward<Args>(args)...));
    }

    template <class... Args>
    void info(std::format_string<Args...> format, Args&&... args) {
        log(LogLevel::Info, std::format(format, std::forward<Args>(args)...));
    }

    template <class... Args>
    void warn(std::format_string<Args...> format, Args&&... args) {
        log(LogLevel::Warn, std::format(format, std::forward<Args>(args)...));
    }

    template <class... Args>
    void error(std::format_string<Args...> format, Args&&... args) {
        log(LogLevel::Error, std::format(format, std::forward<Args>(args)...));
    }

    template <class... Args>
    void fatal(std::format_string<Args...> format, Args&&... args) {
        log(LogLevel::Fatal, std::format(format, std::forward<Args>(args)...));
    }

    void trace(std::string_view message) { log(LogLevel::Trace, std::string(message)); }
    void debug(std::string_view message) { log(LogLevel::Debug, std::string(message)); }
    void info(std::string_view message) { log(LogLevel::Info, std::string(message)); }
    void warn(std::string_view message) { log(LogLevel::Warn, std::string(message)); }
    void error(std::string_view message) { log(LogLevel::Error, std::string(message)); }
    void fatal(std::string_view message) { log(LogLevel::Fatal, std::string(message)); }

private:
    void log(LogLevel level, std::string&& message);

    struct Impl;
    std::unique_ptr<Impl> mImpl;
};

} // namespace sculk
