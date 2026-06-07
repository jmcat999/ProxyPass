#include "ProxyPass.hpp"
#include <iostream>

#include <print>

int main() {
    std::mutex              waitMutex{};
    std::condition_variable waitCv{};
    bool                    stopped{false};

    sculk::protocol::thread::ThreadPool       sharedPool{4};
    sculk::protocol::io::ClientIoRuntime      sharedIoRuntime{4};
    sculk::protocol::AuthenticationKeyManager authKeyManager{};
    sculk::ProxySettings                      settings{};

    settings.load();

    std::println("[ProxyPass] Waiting for Microsoft Service...");
    if (auto status = authKeyManager.initMojangPublicKeyBlocking(); !status) {
        std::println(stderr, "[ProxyPass] Failed to connect to Microsoft Service: {}", status.error().message());
        return 1;
    }

    auto proxyPass = sculk::ProxyPass(sharedPool, sharedIoRuntime, authKeyManager, settings);
    if (!proxyPass.start()) {
        std::println(stderr, "[ProxyPass] Failed to start proxy server.");
        return 1;
    }
    std::println("[ProxyPass] Proxy server started successfully.");

    std::unique_lock waitLock{waitMutex};
    waitCv.wait(waitLock, [&] { return stopped; });

    std::println("[ProxyPass] Stopping proxy server...");

    return 0;
}