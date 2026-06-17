#pragma once
// Example: C++ core side — parse/emit protobuf bytes (see bridge.proto)
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace myapp::bridge {

// Callbacks registered from JNI / ObjC++ shim
using EventCallback = std::function<void(const std::vector<uint8_t>& serialized_event)>;

class CoreBridge {
public:
    void set_event_callback(EventCallback cb) { on_event_ = std::move(cb); }

    // Called from platform layer with serialized AppCommand bytes
    void handle_command(const uint8_t* data, size_t len);

    // Optional: length-prefix framing helper
    static std::vector<uint8_t> frame_message(const std::vector<uint8_t>& payload);

private:
    void emit_ready(uint64_t request_id);
    EventCallback on_event_;
};

}  // namespace myapp::bridge
