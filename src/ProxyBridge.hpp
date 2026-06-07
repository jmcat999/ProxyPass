#pragma once
#include <sculk/protocol/auth/ConnectionRequest.hpp>
#include <sculk/protocol/connection/ClientNetworkSystem.hpp>

namespace sculk {

class ProxyBridge {
public:
    RakNet::RakNetGUID            mRealGuid{};
    RakNet::SystemAddress         mRealAddress{};
    protocol::ClientNetworkSystem mProxyClient{};
    protocol::Session&            mRealClientSession;
    protocol::ConnectionRequest   mConnectionRequest{};

public:
    explicit ProxyBridge(
        const RakNet::RakNetGUID&      guid,
        const RakNet::SystemAddress&   address,
        protocol::thread::ThreadPool&  threadPool,
        protocol::io::ClientIoRuntime& ioRuntime,
        protocol::Session&             realClientSession
    ) noexcept;

    ~ProxyBridge();

    bool sendPacketToClient(const protocol::IPacket& packet, bool immediate = false);

    bool sendPacketToServer(const protocol::IPacket& packet, bool immediate = false);
};

} // namespace sculk