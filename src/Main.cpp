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
#include "ProxyPass.hpp"
#include <iostream>
#include <print>

#ifdef _WIN32
#include <Windows.h>
#endif

void initConsole() {
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
    std::ios::sync_with_stdio(false);
    std::print("\033]0;{}\007", "ProxyPass");
}

int main() {
    initConsole();

    std::mutex              waitMutex{};
    std::condition_variable waitCv{};
    bool                    stopped{false};

    sculk::protocol::AuthenticationKeyManager authKeyManager{};
    sculk::ProxySettings                      settings{};
    sculk::Logger                             logger{"ProxyPass"};

    settings.load();

    logger.info("Waiting for Microsoft Service...");
    if (auto status = authKeyManager.initMojangPublicKeyBlocking(); !status) {
        logger.error("Failed to connect to Microsoft Service: {}", status.error().message());
        return 1;
    }

    auto proxyPass = sculk::ProxyPass(authKeyManager, settings, logger);
    if (!proxyPass.start()) {
        logger.error("Failed to start proxy server.");
        return 1;
    }
    logger.info("Proxy server started successfully.");

    std::unique_lock waitLock{waitMutex};
    waitCv.wait(waitLock, [&] { return stopped; });

    logger.info("Stopping proxy server...");

    return 0;
}