#pragma once
#include <cstdint>
#include <sculk/protocol/codec/MinecraftPacketIds.hpp>
#include <set>
#include <string>

namespace sculk {

struct ProxySettings {
    std::string                            motd               = "A Minecraft Proxy Server";
    std::uint16_t                          ipv4_port          = 19132;
    std::uint16_t                          ipv6_port          = 19133;
    bool                                   online_mode        = true;
    std::string                            upstream_host      = "127.0.0.1";
    std::uint16_t                          upstream_port      = 19134;
    std::uint32_t                          max_players        = 100;
    std::set<protocol::MinecraftPacketIds> ignored_packet_ids = {
        protocol::MinecraftPacketIds::NetworkChunkPublisherUpdate,
        protocol::MinecraftPacketIds::SubChunk,
        protocol::MinecraftPacketIds::SubChunkRequest,
        protocol::MinecraftPacketIds::LevelChunk,
    };

    bool load();
    void save() const;
};

} // namespace sculk