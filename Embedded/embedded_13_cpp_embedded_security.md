# Embedded Systems 13 — C++ Embedded Security Concepts

This document focuses on **security concepts for C++ embedded firmware**, complementing [embedded_11_safety_security.md](embedded_11_safety_security.md). The emphasis is practical: what can go wrong in real devices, how C++ choices affect attack surface, and what a senior engineer should mention in design/review/interview.

---

## 13.1 Security Goal

Embedded security protects the device, firmware, data, and physical behavior against malicious or unauthorized action.

Typical assets:

- Firmware intellectual property.
- Device identity and private keys.
- User data, credentials, telemetry.
- Safety-critical actuator control.
- Firmware update path.
- Debug/programming interface.
- Cloud/backend trust relationship.

Security mindset:

> Assume the attacker may have physical access, can sniff buses, can send malformed input, can power-cycle/glitch the device, and can reverse engineer firmware binaries.

---

## 13.2 Threat Model for Embedded Devices

| Attacker Capability | Example | Mitigation |
|---------------------|---------|------------|
| Network access | Malformed TCP/MQTT/CoAP packet | Input validation, TLS/DTLS, fuzzing |
| Local bus access | UART shell, CAN injection, I2C/SPI sniffing | Authenticated commands, bus filtering, disable debug shells |
| Physical access | SWD/JTAG attach, flash dump | RDP/debug lock, secure boot, encrypted storage |
| Firmware image access | Reverse engineer `.bin` | Signature verification, optional encryption, no hardcoded secrets |
| Power/fault injection | Bypass signature check via glitching | Redundant checks, secure element, voltage/clock monitors |
| Side-channel observation | Timing/power leaks during crypto | Constant-time crypto, hardware accelerators, noise/countermeasures |
| Supply chain compromise | Malicious firmware in factory/update | Signed builds, CI provenance, key custody, manufacturing audit |

Interview answer:

> Embedded security is different from server security because the attacker may own the hardware. Physical access changes the model: debug ports, flash extraction, glitching, and bus sniffing become realistic.

---

## 13.3 Secure Boot and Chain of Trust

Secure boot ensures that only trusted firmware runs.

Basic chain:

```text
Immutable Root of Trust
    |
ROM bootloader verifies bootloader signature
    |
Bootloader verifies application signature
    |
Application verifies update images / modules
```

Key ideas:

- Store root public key hash in OTP/eFuse/secure storage.
- Verify signatures before executing firmware.
- Include version/rollback metadata in signed image.
- Fail closed: invalid image must not boot.
- Keep recovery path controlled and authenticated.

Why signature, not just CRC:

| Mechanism | Protects Against Corruption | Protects Against Malicious Image |
|-----------|-----------------------------|----------------------------------|
| CRC32 | Yes | No |
| SHA-256 hash alone | Yes, if trusted hash is protected | Usually no, if attacker can replace both image and hash |
| Digital signature | Yes | Yes, if private key is protected |

Common bug:

> Bootloader verifies image integrity with CRC but not authenticity. An attacker can modify the firmware and recompute CRC.

---

## 13.4 Secure Firmware Update / OTA

A secure OTA update should verify:

- Authenticity: image signed by trusted vendor key.
- Integrity: image not corrupted.
- Version: prevent rollback to vulnerable firmware.
- Compatibility: hardware model, board revision, flash layout.
- Atomicity: device should survive power loss during update.
- Recovery: known-good fallback image or recovery mode.

Recommended A/B layout:

```text
Bootloader
Metadata: active_slot, pending_slot, version, rollback_counter
Slot A: firmware image
Slot B: firmware image
NVM: device config / keys / calibration
```

Update flow:

1. Download image to inactive slot.
2. Verify signature and metadata.
3. Mark inactive slot as pending.
4. Reboot into pending image.
5. New image performs health check.
6. Mark pending image as confirmed.
7. If health check fails, bootloader rolls back.

Security details:

- Sign metadata and firmware together.
- Use monotonic counter / anti-rollback fuse if available.
- Never store update signing private key on device.
- Protect update transport with TLS, but still verify image signature locally.

---

## 13.5 C++ Memory Safety Risks

C++ gives deterministic performance and hardware control, but unsafe memory handling can become security bugs.

High-risk patterns:

| Pattern | Risk | Safer Approach |
|---------|------|----------------|
| Raw buffer + length mismatch | Buffer overflow | `std::array`, `std::span`, checked length |
| `strcpy`, `sprintf`, `memcpy` without bounds | Stack/heap corruption | `snprintf`, checked copy wrapper |
| Use-after-free | Control-flow hijack, data corruption | Avoid heap, ownership types, pools |
| Integer overflow in length calculation | Under-allocation, overflow | Checked arithmetic |
| Type punning / invalid casts | UB, alignment faults | `std::bit_cast` C++20, `memcpy`, explicit serialization |
| Unbounded recursion | Stack overflow | Iterative algorithms, stack budget |
| Exceptions crossing C/ISR boundaries | Undefined/uncontrolled failure | No exceptions in ISR/low-level boundary |

Example: unsafe parser

```cpp
void parse_packet(const uint8_t* input, size_t len) {
    uint8_t payload[32];
    uint8_t payload_len = input[0];       // BUG: len may be 0
    std::memcpy(payload, input + 1, payload_len);  // BUG: payload_len may exceed 32
}
```

Safer parser shape:

```cpp
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>

struct Packet {
    std::array<uint8_t, 32> payload{};
    size_t payload_len = 0;
};

std::optional<Packet> parse_packet(std::span<const uint8_t> input) {
    if (input.empty()) {
        return std::nullopt;
    }

    const size_t payload_len = input[0];
    if (payload_len > 32 || input.size() < payload_len + 1) {
        return std::nullopt;
    }

    Packet packet;
    packet.payload_len = payload_len;
    std::memcpy(packet.payload.data(), input.data() + 1, payload_len);
    return packet;
}
```

Embedded rule:

> Every parser should validate length before reading, validate length before copying, and define behavior for malformed input.

---

## 13.6 Input Validation at Trust Boundaries

Trust boundaries in embedded systems:

- UART command shell.
- BLE characteristic writes.
- CAN frames.
- USB control requests.
- TCP/UDP/MQTT/CoAP packets.
- Firmware update package.
- External flash config.
- Sensor data if attacker can influence the sensor/bus.

Validation checklist:

- Check minimum packet size before reading header.
- Check declared length against actual buffer length.
- Check maximum length against destination capacity.
- Validate enum/range values.
- Reject unknown commands by default.
- Authenticate privileged commands.
- Rate-limit expensive operations.
- Avoid parsing directly inside ISR.

Command handler pattern:

```cpp
enum class Command : uint8_t {
    Ping = 1,
    ReadStatus = 2,
    FactoryReset = 3,
};

bool is_privileged(Command cmd) {
    return cmd == Command::FactoryReset;
}

bool handle_command(Command cmd, bool authenticated) {
    if (is_privileged(cmd) && !authenticated) {
        return false;
    }

    switch (cmd) {
        case Command::Ping:
        case Command::ReadStatus:
        case Command::FactoryReset:
            return true;
    }

    return false;
}
```

---

## 13.7 Secrets and Key Management

Bad practices:

- Hardcoded API keys in firmware.
- Same symmetric key across all devices.
- Private signing key stored on device.
- Debug logs printing tokens/keys.
- Keys stored in normal flash without readout protection.

Better practices:

- Use device-unique identity/key.
- Derive session keys; do not reuse long-term keys directly.
- Store secrets in secure element, TrustZone secure storage, OTP, or protected flash.
- Use hardware true random number generator for key generation.
- Rotate/revoke credentials when possible.
- Separate development, staging, and production keys.

Manufacturing flow:

```text
Device boots in provisioning mode
  -> factory authenticates station
  -> device generates key internally or receives wrapped secret
  -> stores key in secure storage / secure element
  -> locks debug/provisioning mode
  -> records public identity/certificate in backend
```

Senior note:

> A firmware update signing private key must never be present on the device. Devices only need the public key or a hash of the trusted public key.

---

## 13.8 Crypto Usage Rules

Use proven crypto libraries and hardware accelerators where available.

Do:

- Use AES-GCM or ChaCha20-Poly1305 for authenticated encryption.
- Use SHA-256/SHA-384 for hashing.
- Use ECDSA/Ed25519/RSA-PSS for signatures, depending on platform support.
- Use TLS/DTLS for transport security.
- Use constant-time compare for MAC/signature checks.
- Use secure RNG for nonces and keys.

Avoid:

- Custom encryption.
- ECB mode.
- Reusing nonce with AES-GCM.
- `rand()` for crypto.
- Early-return secret comparisons.
- Ignoring crypto API return codes.

Constant-time compare:

```cpp
#include <cstddef>
#include <cstdint>
#include <span>

bool constant_time_equal(std::span<const uint8_t> a, std::span<const uint8_t> b) {
    if (a.size() != b.size()) {
        return false;
    }

    uint8_t diff = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        diff |= static_cast<uint8_t>(a[i] ^ b[i]);
    }
    return diff == 0;
}
```

Note:

> Use the constant-time primitive from your crypto library when available. Hand-written versions should be reviewed carefully because compiler optimizations and calling code can accidentally reintroduce timing leaks.

---

## 13.9 RTOS Security Concepts

RTOS systems often run everything in one address space, so a bug in one task can corrupt another task.

Mitigations:

- Use MPU to isolate critical regions.
- Separate privileged and unprivileged tasks if RTOS supports it.
- Keep crypto/key handling in trusted task/secure world.
- Give each task a measured stack size plus watermark monitoring.
- Minimize shared mutable globals.
- Validate messages crossing queues.
- Avoid blocking critical safety/security tasks behind low-priority tasks.

Example separation:

```text
High privilege:
  - secure update task
  - key manager
  - watchdog/safety monitor

Lower privilege:
  - UI task
  - telemetry task
  - network parser
```

Security bug pattern:

> A low-priority network task parses attacker-controlled data and writes past its buffer, corrupting a global command structure used by a high-priority actuator task.

---

## 13.10 Embedded Linux Security Concepts

For embedded Linux devices, security expands from firmware to OS and services.

Checklist:

- Secure boot through SPL/U-Boot/kernel/rootfs.
- Verified root filesystem: dm-verity, signed squashfs, read-only rootfs.
- Minimize services and open ports.
- Run services as non-root users.
- Use Linux capabilities instead of full root.
- Enable ASLR, stack protector, RELRO, PIE, NX.
- Use seccomp/AppArmor/SELinux for process sandboxing.
- Keep update mechanism signed and atomic.
- Disable password login if not needed.
- Protect SSH keys and factory credentials.
- Maintain SBOM and patch CVEs.

Example hardening flags:

```text
-fstack-protector-strong
-D_FORTIFY_SOURCE=2
-fPIE -pie
-Wl,-z,relro,-z,now
```

---

## 13.11 Debug, Test, and Production Lockdown

Development features become vulnerabilities if shipped.

Production checklist:

- Disable or lock SWD/JTAG.
- Remove unauthenticated UART shell.
- Disable test commands, backdoors, factory bypasses.
- Disable verbose logs containing secrets.
- Enable flash readout protection.
- Configure watchdog.
- Enable secure boot.
- Verify rollback protection.
- Verify device-unique provisioning.
- Verify failure behavior for invalid firmware.

Debug policy:

| Environment | Debug Access |
|-------------|--------------|
| Development | Full debug allowed |
| Factory | Controlled provisioning/debug with audit |
| Production | Locked or authenticated debug only |
| RMA / repair | Explicit recovery flow, usually data-destructive unlock |

---

## 13.12 Secure Coding Checklist for C++ Embedded

Design:

- Define threat model before choosing mechanisms.
- Minimize trusted computing base.
- Separate parser, validator, and executor.
- Fail closed for security decisions.
- Prefer simple state machines over ad-hoc flags.

C++ implementation:

- Prefer `std::array`, `std::span`, fixed-size buffers.
- Avoid dynamic allocation in long-running firmware paths.
- Use RAII for locks, peripherals, memory pools, and semaphore permits.
- Avoid exceptions across ISR/C ABI boundaries.
- Avoid RTTI/exceptions if project policy forbids them; document policy.
- Keep ISR code minimal and non-parsing.
- Treat all external input as hostile.
- Use checked arithmetic for sizes/offsets.

Build/tooling:

- Enable warnings as errors for firmware code.
- Use static analysis: clang-tidy, Cppcheck, Coverity, MISRA/AUTOSAR tools.
- Use sanitizers in host-side tests where possible: ASan, UBSan, TSan.
- Fuzz protocol parsers on host.
- Generate map file and verify no debug/test symbols in release.
- Track dependencies and CVEs.

Review questions:

- Can malformed input crash or hang the device?
- Can an attacker bypass authentication by replaying old messages?
- Can firmware be downgraded?
- What happens if power is lost during update?
- Where are keys stored?
- Is debug locked in production?
- Is every privileged command authenticated and authorized?
- Are crypto comparisons constant-time?

---

## 13.13 Interview Q&As

### Mid

**Q: CRC vs cryptographic signature for firmware update?**

> CRC detects accidental corruption but does not prove authenticity. An attacker can modify firmware and recompute CRC. A digital signature proves the image was signed by the vendor private key. Secure OTA should verify signature, version metadata, target hardware, and anti-rollback before booting the image.

**Q: Why are hardcoded credentials dangerous in embedded firmware?**

> Firmware can often be extracted through debug ports, flash dumps, OTA packages, or reverse engineering. If all devices share one hardcoded key, compromise of one device compromises the fleet. Use device-unique keys, secure provisioning, protected storage, and backend revocation.

**Q: Why is `std::span` useful in secure embedded C++ parsers?**

> `std::span` carries pointer and length together, making APIs harder to misuse than raw pointer-only parameters. It does not automatically validate bounds for every operation, but it encourages explicit size checks and clearer parser contracts.

### Senior

**Q: Design a secure OTA flow for a battery-powered MCU device.**

> Use A/B slots with signed firmware metadata. Download into inactive slot in chunks with hash verification. Verify signature using public key hash stored in OTP/eFuse. Check hardware compatibility and monotonic version to prevent rollback. Mark slot pending and reboot. New firmware must pass health check before marking confirmed. If power fails or health check fails, bootloader returns to previous confirmed image. Transport should use TLS, but local signature verification remains mandatory.

**Q: How would you harden a C++ parser for BLE/CAN/UART commands?**

> Treat input as hostile. Validate minimum header size before reading fields. Validate declared length against actual buffer length and destination capacity. Use fixed-size buffers, `std::span`, and checked arithmetic. Reject unknown commands by default. Authenticate privileged commands. Keep parsing out of ISR. Fuzz the parser on host with ASan/UBSan. Ensure parser returns structured errors instead of partially mutating global state.

**Q: What is the difference between secure boot and encrypted firmware?**

> Secure boot verifies authenticity/integrity before execution. Encrypted firmware protects confidentiality of the binary. Encryption alone does not prove the image is trusted unless combined with authentication/signature. Many systems need signature verification more than encryption; encryption is added when IP protection or secret-bearing firmware confidentiality matters.
