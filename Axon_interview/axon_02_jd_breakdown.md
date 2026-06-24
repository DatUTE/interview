# JD Breakdown — Embedded Software Engineer I

Source: [Axon Embedded Software Engineer I JD](https://www.axon.com/careers/job/4958517003)

## Role Summary

This is an embedded software role for Axon's camera systems in Ho Chi Minh City. The JD emphasizes:

- Secure, privacy-preserving camera features.
- High stability and fast, consistent performance.
- Cross-team coordination with desktop, mobile, and backend services.
- System design trade-offs for a "bullet-proof" system.
- Troubleshooting, ambiguity resolution, mentoring, and code/architecture review.

Practical interpretation:

> This is not only "write embedded C++." It is connected device engineering: camera firmware, embedded Linux, networking, upload/sync, testing, security, privacy, and production debugging.

---

## Responsibilities Mapped to Skills

| JD Responsibility | What It Really Means | Prep Focus |
|-------------------|----------------------|------------|
| Design/develop/deploy features for camera systems | Own feature lifecycle from requirement to field behavior | Embedded Linux, camera pipeline, storage, OTA, logging |
| Secure, privacy-preserving software | Evidence data is sensitive and legally important | Secure boot, encryption, key management, access control, auditability |
| Coordinate desktop/mobile/backend | Device is one node in a larger ecosystem | API contracts, sync protocol, versioning, telemetry, incident debugging |
| Set high technical bar in reviews | Code quality and architecture judgement | C++ design, testing, review checklist, trade-off communication |
| Troubleshoot technical issues | Debug real hardware/software failures | Logs, core dumps, strace, gdb, perf, network captures |
| Resolve ambiguity/conflicts | Convert product uncertainty into engineering decisions | Requirements analysis, risk-driven design, staged rollout |
| Mentor junior engineers | Explain, review, raise team capability | Clear technical communication and examples |

---

## Requirements Mapped to Interview Topics

| Requirement | Topics to Prepare |
|-------------|-------------------|
| 2+ years embedded software | MCU/RTOS basics, embedded Linux, boot, drivers, device constraints |
| CS fundamentals | Data structures, algorithms, runtime analysis, OOP, code quality |
| C/C++ and Go | C++17/20, RAII, ownership, concurrency, C ABI, basic Go services/tooling |
| Embedded Linux | Yocto basics, device tree, systemd, process model, kernel/user boundary |
| Unit/integration/system tests | GoogleTest, mocks, HIL, test pyramid, CI gates |
| Troubleshooting | gdb, logs, crash dumps, hard faults, network traces, performance profiling |
| Networking | TCP/IP, UDP, Bluetooth, DNS, Wi-Fi, reconnect, buffering, TLS |
| Quick learning | Talk through a time learning an unfamiliar stack |
| English communication | Clear technical explanations and status updates |
| Security knowledge | Secure boot, OTA signing, encryption, privacy, threat modeling |
| CI/CD tools | Jenkins/TeamCity basics, build pipelines, artifacts, release gates |
| Yocto | Recipes, layers, images, packages, SDK, cross-compile |
| Agile SDLC | Jira, sprint planning, acceptance criteria, incremental delivery |

---

## Must-Have Technical Stories

Prepare 5-7 STAR stories:

1. **Debugging story**: intermittent crash, memory bug, race, or field issue.
2. **Performance story**: reduced latency, CPU, memory, boot time, or battery usage.
3. **Testing story**: added unit/integration/system tests or improved CI reliability.
4. **Security/privacy story**: protected data, improved auth, fixed unsafe parser, secure update.
5. **Cross-team story**: coordinated API/protocol change with backend/mobile/cloud.
6. **Ambiguity story**: turned vague product requirement into technical plan.
7. **Mentoring/review story**: raised code quality or helped a junior engineer.

---

## What "Camera Systems" Likely Implies

You should be ready to discuss:

- Camera sensor capture and buffering.
- Audio/video encoding basics.
- Local storage and crash-safe file writes.
- Evidence metadata: timestamp, device ID, user/case assignment.
- Wi-Fi/Bluetooth connectivity.
- Upload/retry/resume logic.
- Battery/thermal constraints.
- Secure boot and OTA update.
- Device provisioning and identity.
- Logs/telemetry that do not leak sensitive content.

You do not need to claim product-specific internals. Frame answers as design reasoning:

> "For a body camera style device, I would design capture as a pipeline with bounded queues, explicit backpressure, crash-safe local persistence, and upload resume. Metadata and integrity hashes must be generated at capture time and preserved through upload."

---

## Red Flags to Avoid

- Treating security as only TLS.
- Treating privacy as only encryption.
- Saying "just use TCP" without discussing offline, retry, and tail latency.
- Ignoring storage corruption/power loss.
- Ignoring battery/thermal constraints.
- Overusing dynamic allocation in hot paths.
- No test strategy beyond manual testing.
- No plan for observability on deployed devices.

---

## One-Minute Pitch

> "My background is aligned with connected embedded systems: C/C++ device software, Linux-level debugging, networking, testing, and secure firmware practices. For Axon's camera systems, I would focus on reliability, evidence integrity, privacy, and field diagnostics. I can work across device, mobile, backend, and product boundaries, and I like turning ambiguous requirements into testable engineering decisions."
