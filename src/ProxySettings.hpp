#pragma once
#include <cstdint>
#include <sculk/protocol/codec/MinecraftPacketIds.hpp>
#include <set>
#include <string>

namespace sculk {

struct ProxySettings {
    std::string   motd          = "A Minecraft Proxy Server";
    std::uint16_t ipv4_port     = 19132;
    std::uint16_t ipv6_port     = 19133;
    bool          online_mode   = true;
    std::string   upstream_host = "127.0.0.1";
    std::uint16_t upstream_port = 19134;
    std::uint32_t max_players   = 100;
    struct {
        bool                                   black_list_mode = true;
        std::set<protocol::MinecraftPacketIds> packet_ids      = {
            protocol::MinecraftPacketIds::Login,
            protocol::MinecraftPacketIds::RequestNetworkSettings,
            protocol::MinecraftPacketIds::NetworkSettings,
            protocol::MinecraftPacketIds::ClientToServerHandshake,
            protocol::MinecraftPacketIds::ServerToClientHandshake,
            protocol::MinecraftPacketIds::NetworkChunkPublisherUpdate,
            protocol::MinecraftPacketIds::SubChunk,
            protocol::MinecraftPacketIds::SubChunkRequest,
            protocol::MinecraftPacketIds::LevelChunk,
            protocol::MinecraftPacketIds::MoveActorDelta,
            protocol::MinecraftPacketIds::PlayerAuthInput,
            protocol::MinecraftPacketIds::SetActorMotion,
            protocol::MinecraftPacketIds::StartGame,
            protocol::MinecraftPacketIds::CraftingData,
        };
    } packets_logger;

    bool load();
    void save() const;
};

} // namespace sculk