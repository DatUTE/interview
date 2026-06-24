# Axon Interview Questions

Questions tailored to the Embedded Software Engineer I JD.

---

## 1. C/C++ Questions

**Q: Why is RAII important in embedded C++?**

> RAII ties resource lifetime to object lifetime. In embedded software, resources include file descriptors, mutexes, buffers, device handles, and power locks. RAII prevents leaks and makes early returns/exceptions safer. Example: a lock guard releases a mutex even if the function returns early.

**Q: `volatile` vs `std::atomic`?**

> `volatile` prevents the compiler from optimizing away memory accesses, useful for memory-mapped registers. It does not provide atomicity, ordering, or inter-thread synchronization. `std::atomic` is for thread synchronization and lock-free shared variables.

**Q: How would you parse a binary packet safely?**

> Accept `std::span<const uint8_t>`, check minimum header size before reading fields, validate declared length against input size and destination capacity, reject unknown commands, and return structured errors without partially mutating global state.

**Q: When avoid dynamic allocation?**

> Avoid it in real-time/hot paths and long-running embedded services where fragmentation, allocation latency, and failure behavior matter. Prefer pre-allocated pools, fixed-size buffers, and ownership types.

---

## 2. Embedded Linux Questions

**Q: Explain embedded Linux boot flow.**

> ROM code loads bootloader, bootloader initializes memory/peripherals and loads kernel/device tree/initramfs or rootfs, kernel initializes drivers, then `init`/systemd starts userspace services.

**Q: What is device tree?**

> A hardware description consumed by the Linux kernel. It describes peripherals, memory, interrupts, clocks, GPIOs, buses, and driver binding data so the same kernel can support different boards.

**Q: How debug a camera service that crashes in the field?**

> Collect firmware version, logs, crash dump/core dump if enabled, restart history, resource usage, recent config, and repro steps. Use `journalctl`, `dmesg`, core backtrace with symbols, and add targeted telemetry. Check memory corruption with ASan in host tests and reproduce on dev hardware.

---

## 3. Networking Questions

**Q: TCP vs UDP for camera upload?**

> Reliable evidence upload should usually use TCP/TLS or HTTPS because data completeness matters. UDP may be useful for real-time low-latency streaming, but then the app must handle loss, ordering, security, and congestion. Local recording should remain source of truth.

**Q: How handle Wi-Fi drop during video upload?**

> Upload from committed local storage, split file into chunks, track confirmed chunks, retry with backoff/jitter, use idempotency keys, and resume after reconnect. Do not delete local evidence until cloud confirms complete upload.

**Q: Why can DNS or TLS fail on embedded devices?**

> DNS can fail due to network/captive portal/resolver issues. TLS can fail due to invalid system time, expired certificates, missing CA bundle, hostname mismatch, or disabled certificate verification. Device logs should distinguish these cases.

---

## 4. Testing Questions

**Q: What tests would you write for an uploader?**

> Unit test retry/backoff/idempotency logic with fake clock and fake server. Integration test chunk upload/resume. System test Wi-Fi disconnect/reconnect. Fault injection for disk full, power loss, corrupted chunk, server 500, TLS failure.

**Q: How would you test firmware update?**

> Test valid update, invalid signature, corrupted image, rollback prevention, power loss before/after slot switch, health check failure, and recovery to previous confirmed image.

**Q: What belongs in unit vs system tests?**

> Unit: parser, state machine, retry policy, metadata serialization. System: real hardware capture, storage, network, upload, OTA, battery/power behavior.

---

## 5. Security and Privacy Questions

**Q: Secure boot vs encrypted firmware?**

> Secure boot verifies authenticity and integrity before execution. Encrypted firmware protects confidentiality. Signature verification is mandatory for trust; encryption is optional depending on IP/secrecy needs.

**Q: Why is CRC not enough for firmware update?**

> CRC catches accidental corruption but an attacker can modify firmware and recompute CRC. A digital signature proves the image came from a holder of the private signing key.

**Q: How avoid leaking private evidence through logs?**

> Log event IDs, evidence IDs, hashes, failure reasons, and counters. Do not log raw video/audio, tokens, private keys, full user identifiers, or sensitive metadata unless explicitly approved and protected.

---

## 6. System Design Questions

**Q: Design offline-first camera recording and upload.**

> Capture writes to local durable storage with metadata and hashes. Uploader is decoupled from recording. Upload chunks with resume and idempotency. Confirm cloud receipt before retention/deletion. Encrypt at rest and in transit. Test power loss, disk full, network drop, and corrupted chunks.

**Q: Add live streaming without risking evidence loss.**

> Keep local recording as priority. Live streaming uses separate queue and adaptive bitrate. If CPU/network/thermal pressure occurs, degrade/drop live stream before local recording. Secure stream and enforce policy controls.

**Q: How design observability for deployed cameras?**

> Log recording start/stop, upload state, network transitions, storage space, firmware version, crash/reboot reason, queue depths, battery, temperature. Keep logs privacy-preserving and correlate with backend traces using evidence/device IDs.

---

## 7. Behavioral Questions

**Q: Tell me about resolving ambiguous requirements.**

Answer structure:

1. Clarify user/customer outcome.
2. Identify constraints and risks.
3. Propose options with trade-offs.
4. Agree on measurable acceptance criteria.
5. Deliver incrementally and validate.

**Q: Tell me about raising code quality.**

Use an example involving:

- Review checklist.
- Tests added.
- Static analysis.
- Cleaner ownership/API.
- Reduced bug class.

**Q: Tell me about mentoring.**

Show:

- How you explained a concept.
- How you reviewed code constructively.
- How the junior engineer became more independent.

---

## 8. Questions to Ask Interviewers

- What is the main camera subsystem this role would contribute to?
- What is the device OS stack: embedded Linux, RTOS, or hybrid?
- What are the current biggest field reliability problems?
- How do you test camera firmware before release?
- What does your hardware-in-loop setup look like?
- How are device logs and cloud logs correlated?
- How do you handle secure boot, OTA, rollback, and provisioning?
- What does success look like after 3 months?
