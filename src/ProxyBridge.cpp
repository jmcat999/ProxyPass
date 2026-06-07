#include "ProxyBridge.hpp"

namespace sculk {

ProxyBridge::ProxyBridge(
    const RakNet::RakNetGUID&      guid,
    const RakNet::SystemAddress&   address,
    protocol::thread::ThreadPool&  threadPool,
    protocol::io::ClientIoRuntime& ioRuntime,
    protocol::Session&             realClientSession
) noexcept
: mRealGuid(guid),
  mRealAddress(address),
  mProxyClient(threadPool, ioRuntime),
  mRealClientSession(realClientSession),
  mClientReady(false) {}

ProxyBridge::~ProxyBridge() {
    if (mProxyClient.isConnected()) {
        mProxyClient.disconnect();
    }
}

bool ProxyBridge::sendPacketToClient(const protocol::IPacket& packet, bool immediate) {
    protocol::Session::Buffer buffer{};
    protocol::BinaryStream    stream{buffer};
    packet.writeWithHeader(stream);
    if (immediate) {
        return mRealClientSession.sendPacketImmediately(std::move(buffer));
    }
    return mRealClientSession.sendPacket(std::move(buffer));
}

bool ProxyBridge::sendPacketToServer(const protocol::IPacket& packet, bool immediate) {
    if (!mProxyClient.isConnected()) {
        return false;
    }
    protocol::Session::Buffer buffer{};
    protocol::BinaryStream    stream{buffer};
    packet.writeWithHeader(stream);
    if (immediate) {
        return mProxyClient.getSession().sendPacketImmediately(std::move(buffer));
    }
    return mProxyClient.getSession().sendPacket(std::move(buffer));
}

} // namespace sculk
