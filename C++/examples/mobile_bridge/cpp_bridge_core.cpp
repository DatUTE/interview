// Example implementation — requires generated bridge.pb.h from bridge.proto
// protoc --cpp_out=./gen/cpp bridge.proto

#include "cpp_bridge_core.hpp"
// #include "bridge.pb.h"

#include <arpa/inet.h>
#include <cstring>

namespace myapp::bridge {

void CoreBridge::handle_command(const uint8_t* /*data*/, size_t /*len*/) {
    // myapp::bridge::AppCommand cmd;
    // if (!cmd.ParseFromArray(data, len)) return;
    // switch (cmd.type()) { ... }
}

void CoreBridge::emit_ready(uint64_t /*request_id*/) {
    // Build CoreEvent, SerializeToString, invoke on_event_
}

std::vector<uint8_t> CoreBridge::frame_message(const std::vector<uint8_t>& payload) {
    std::vector<uint8_t> out(4 + payload.size());
    uint32_t len = htonl(static_cast<uint32_t>(payload.size()));
    std::memcpy(out.data(), &len, 4);
    std::memcpy(out.data() + 4, payload.data(), payload.size());
    return out;
}

}  // namespace myapp::bridge
