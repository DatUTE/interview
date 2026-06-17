# Mobile Bridge Examples

Shared IDL and skeleton code for **C++ core ↔ Android ↔ iOS**.

| File | Purpose |
|------|---------|
| [bridge.proto](bridge.proto) | IDL — `protoc` for C++, Java/Kotlin, ObjC/Swift |
| [cpp_bridge_core.hpp](cpp_bridge_core.hpp) | C++ core API sketch |

**Generate stubs:**

```bash
# C++
protoc --cpp_out=./gen/cpp bridge.proto

# Java / Kotlin (Android)
protoc --java_out=./gen/android bridge.proto

# Objective-C (iOS — or use SwiftProtobuf plugin)
protoc --objc_out=./gen/ios bridge.proto
```

Full guide: [../../cpp_core_mobile_bridge_idl.md](../../cpp_core_mobile_bridge_idl.md)
