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

#ifdef _WIN32
#include <windows.h>
#endif

int main() {
    struct LoggerWaitGuard {
        ~LoggerWaitGuard() { sculk::Logger::wait(); }
    } loggerWaitGuard{};

#ifdef _WIN32
    SetConsoleTitle(L"ProxyPass");
#endif

    std::ios::sync_with_stdio(false);

    std::mutex              waitMutex{};
    std::condition_variable waitCv{};
    bool                    stopped{false};

    sculk::protocol::AuthenticationKeyManager authKeyManager{};
    sculk::ProxySettings                      settings{};

    settings.load();

    sculk::Logger("ProxyPass").info("Waiting for Microsoft Service...");
    if (auto status = authKeyManager.initMojangPublicKeyBlocking(); !status) {
        sculk::Logger("ProxyPass").error("Failed to connect to Microsoft Service: {}", status.error().message());
        return 1;
    }

    auto proxyPass = sculk::ProxyPass(authKeyManager, settings);
    if (!proxyPass.start()) {
        sculk::Logger("ProxyPass").error("Failed to start proxy server.");
        return 1;
    }
    sculk::Logger("ProxyPass").info("Proxy server started successfully.");

    std::unique_lock waitLock{waitMutex};
    waitCv.wait(waitLock, [&] { return stopped; });

    sculk::Logger("ProxyPass").info("Stopping proxy server...");

    return 0;
}