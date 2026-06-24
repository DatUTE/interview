# Technical Knowledge Map

This file maps Axon's JD to concrete technical knowledge to prepare.

---

## 1. C / C++ for Embedded Devices

Must know:

- RAII for resource ownership: file descriptors, locks, buffers, device handles.
- Rule of 0/3/5, move semantics, object lifetime.
- `unique_ptr` vs `shared_ptr`; avoid shared ownership unless needed.
- `std::vector`, `std::array`, `std::span`, `std::string_view`.
- `const`, `constexpr`, `noexcept`, `explicit`, `override`.
- Error handling without uncontrolled exceptions in low-level boundaries.
- Avoid undefined behavior: data races, invalid casts, unaligned access, use-after-free.
- Fixed-width types: `uint8_t`, `uint32_t`, endian conversion.
- C ABI boundary: callbacks, `extern "C"`, ownership contracts.

Interview examples:

- Implement a bounded ring buffer.
- Parse a binary packet safely.
- Explain why `volatile` is not thread synchronization.
- Explain RAII for a file descriptor or mutex.
- Explain how to avoid heap fragmentation.

Repo references:

- [../C++/core_cpp_memory_and_ownership.md](../C++/core_cpp_memory_and_ownership.md)
- [../C++/modern_cpp_11_14_17_20.md](../C++/modern_cpp_11_14_17_20.md)
- [../Embedded/embedded_04_cpp_specifics.md](../Embedded/embedded_04_cpp_specifics.md)

---

## 2. Embedded Linux

Must know:

- Boot flow: ROM -> bootloader -> kernel -> init/systemd.
- Kernel vs userspace responsibilities.
- Device tree basics: hardware description for kernel drivers.
- Character devices, `/dev`, sysfs, procfs.
- Logging: `dmesg`, journald, application logs.
- Process supervision: systemd service restart policy, watchdog.
- Cross-compilation and deployment.
- Filesystems: read-only rootfs, overlay, persistent data partition.
- Time sync and clock drift.
- Resource limits: CPU, memory, storage, thermal.

Commands to be comfortable with:

```bash
dmesg -w
journalctl -u camera-service -f
top
htop
ps aux
strace -f -p <pid>
lsof -p <pid>
ip addr
ip route
iw dev
bluetoothctl
```

Repo references:

- [../Embedded/embedded_10_embedded_linux.md](../Embedded/embedded_10_embedded_linux.md)
- [../C++/linux_concepts.md](../C++/linux_concepts.md)

---

## 3. Networking for Camera Devices

JD mentions TCP/IP, UDP, Bluetooth, DNS, Wi-Fi.

Prepare:

- TCP vs UDP trade-offs.
- DNS resolution and caching.
- Wi-Fi reconnect and captive/poor network behavior.
- TLS handshake and certificate validation.
- Upload resume for large video files.
- Offline-first design: store locally, upload later.
- Backpressure: what happens if network is slower than recording.
- Bluetooth pairing/provisioning risks.
- NAT, keepalive, heartbeat.
- Packet capture with `tcpdump`/Wireshark.

Camera-specific network problems:

- Device records faster than it uploads.
- Wi-Fi drops during upload.
- DNS failure or TLS certificate issue.
- Backend returns 5xx; retries must not cause duplicate evidence records.
- Bluetooth provisioning command must be authenticated.

Repo references:

- [../C++/networking.md](../C++/networking.md)
- [../Embedded/embedded_03_communication_protocols.md](../Embedded/embedded_03_communication_protocols.md)

---

## 4. Testing Strategy

Axon asks for unit, integration, and system-level tests.

Test pyramid:

| Level | Example |
|-------|---------|
| Unit | Parser, state machine, retry policy, metadata builder |
| Component | Storage manager with fake filesystem, uploader with fake server |
| Integration | Camera service + network service + local DB |
| System | Real device records, disconnects Wi-Fi, reconnects, uploads |
| HIL | Automated hardware-in-loop test with power/network fault injection |

Important test patterns:

- Dependency injection for time, network, filesystem, crypto provider.
- Fake clock for retry/backoff tests.
- Fuzz binary parsers.
- Golden files for metadata serialization.
- Fault injection: power loss, disk full, network timeout, corrupted file.
- Sanitizers in host tests: ASan/UBSan/TSan.

Repo references:

- [../C++/gtest_unit_testing.md](../C++/gtest_unit_testing.md)
- [../Embedded/embedded_09_debugging_toolchain.md](../Embedded/embedded_09_debugging_toolchain.md)

---

## 5. Security and Privacy

The JD explicitly says secure, privacy-preserving camera systems.

Prepare:

- Threat modeling for physical device access.
- Secure boot and signed firmware.
- OTA update verification and rollback prevention.
- Device-unique identity.
- Key storage: secure element, TrustZone, TPM, protected flash.
- TLS certificate validation.
- Encryption at rest for evidence on device.
- Chain of custody: hash, timestamp, audit log.
- Access control for debug shell, UART, Bluetooth provisioning.
- Privacy-preserving logs: avoid raw video/audio/user secrets in logs.
- Data retention and deletion semantics.

Axon context:

- Axon Evidence emphasizes digital fingerprints, audit trails, encryption, permissions, and compliance ([Axon Evidence](https://www.axon.com/products/axon-evidence)).
- Axon's body-worn camera resource discusses timestamps, camera serial number, activity logging, encrypted footage, auto-upload, and audit trails ([Axon resource](https://www.axon.com/resources/how-axon-evidence-and-body-worn-camera-systems-work)).

Repo reference:

- [../Embedded/embedded_13_cpp_embedded_security.md](../Embedded/embedded_13_cpp_embedded_security.md)

---

## 6. CS Fundamentals

JD explicitly mentions:

- Data structures.
- Algorithms.
- Runtime analysis.
- Object-oriented design.
- Code quality verification.

Prepare:

- Big-O for common operations.
- Hash table vs tree.
- Queue/ring buffer.
- Heap/priority queue.
- Graph basics.
- Binary search.
- Sorting trade-offs.
- OOP design: interface, dependency injection, SOLID.

Embedded-friendly angle:

> Complexity is not only Big-O. Memory allocation, cache locality, bounded latency, and predictable failure behavior matter heavily on devices.

Repo reference:

- [../CS%20Fundamental/README.md](../CS%20Fundamental/README.md)

---

## 7. Yocto Basics

Yocto is preferred in the JD.

Know the vocabulary:

- Recipe: build instructions for a package.
- Layer: collection of recipes/config.
- Image: final root filesystem composition.
- Machine config: board/platform-specific settings.
- SDK: cross-compilation environment for app developers.
- BitBake: build engine.

Interview-level answer:

> Yocto helps build a reproducible embedded Linux distribution: kernel, bootloader, rootfs, packages, and configuration. For a camera product, it matters because you need controlled builds, signed artifacts, repeatable dependency versions, and OTA-compatible images.

---

## 8. CI/CD

Preferred tools include Jenkins/TeamCity, Git, Jira.

Prepare:

- Build pipeline stages: lint, unit tests, static analysis, cross-compile, package, sign artifact, integration tests.
- Artifact versioning and traceability.
- Reproducible builds.
- Release gates.
- Hardware test farm / HIL.
- Rollback and staged rollout.

Example pipeline:

```text
PR
  -> format/lint/static analysis
  -> host unit tests with sanitizers
  -> cross-compile firmware/service
  -> package image/update bundle
  -> sign artifact
  -> integration tests
  -> hardware-in-loop smoke tests
  -> staged release
```
