# Axon 4-Week Study Plan

Use this plan if you have about one month. If time is short, focus on Week 1 and Week 2 first.

---

## Week 1 — C++ and Embedded Fundamentals

Goals:

- Speak confidently about modern C++ in embedded contexts.
- Review memory, ownership, RAII, concurrency basics.
- Practice safe parsers and bounded buffers.

Study:

- [../C++/core_cpp_memory_and_ownership.md](../C++/core_cpp_memory_and_ownership.md)
- [../C++/modern_cpp_11_14_17_20.md](../C++/modern_cpp_11_14_17_20.md)
- [../C++/multithreading_and_concurrency.md](../C++/multithreading_and_concurrency.md)
- [../Embedded/embedded_04_cpp_specifics.md](../Embedded/embedded_04_cpp_specifics.md)
- [../Embedded/embedded_05_memory_management.md](../Embedded/embedded_05_memory_management.md)

Practice:

- Implement a safe binary packet parser.
- Implement SPSC ring buffer.
- Explain `volatile` vs `atomic`.
- Explain mutex vs semaphore.
- Explain heap fragmentation and memory pools.

---

## Week 2 — Embedded Linux, Networking, Testing

Goals:

- Be ready for embedded Linux troubleshooting.
- Be ready for networking questions around TCP/IP, UDP, Bluetooth, DNS, Wi-Fi.
- Be ready to discuss unit/integration/system tests.

Study:

- [../Embedded/embedded_10_embedded_linux.md](../Embedded/embedded_10_embedded_linux.md)
- [../C++/linux_concepts.md](../C++/linux_concepts.md)
- [../C++/networking.md](../C++/networking.md)
- [../C++/gtest_unit_testing.md](../C++/gtest_unit_testing.md)
- [../Embedded/embedded_09_debugging_toolchain.md](../Embedded/embedded_09_debugging_toolchain.md)

Practice:

- Design upload retry/resume.
- Explain DNS/TLS failure modes.
- Write test cases for uploader state machine.
- Explain `journalctl`, `dmesg`, `strace`, `tcpdump`.
- Explain hardware-in-loop testing.

---

## Week 3 — Security, Privacy, Camera System Design

Goals:

- Connect Axon's mission/product to device security.
- Discuss evidence integrity, chain of custody, privacy-preserving logs.
- Design camera capture/storage/upload architecture.

Study:

- [axon_04_camera_system_design.md](axon_04_camera_system_design.md)
- [axon_05_security_privacy_evidence.md](axon_05_security_privacy_evidence.md)
- [../Embedded/embedded_11_safety_security.md](../Embedded/embedded_11_safety_security.md)
- [../Embedded/embedded_13_cpp_embedded_security.md](../Embedded/embedded_13_cpp_embedded_security.md)

Practice:

- Design body camera offline recording/upload.
- Design secure OTA with rollback.
- Explain CRC vs signature.
- Explain privacy-preserving logs.
- Explain chain of custody from device perspective.

---

## Week 4 — Mock Interviews and Stories

Goals:

- Convert technical knowledge into concise answers.
- Prepare behavioral stories.
- Practice system design trade-offs.

Study:

- [axon_01_company_context.md](axon_01_company_context.md)
- [axon_02_jd_breakdown.md](axon_02_jd_breakdown.md)
- [axon_06_interview_questions.md](axon_06_interview_questions.md)
- [../C++/senior_engineering_behaviors.md](../C++/senior_engineering_behaviors.md)

Prepare STAR stories:

- Debugging a hard issue.
- Improving performance/reliability.
- Adding tests or CI.
- Security/privacy improvement.
- Cross-team coordination.
- Mentoring/code review.
- Resolving ambiguity.

---

## Last-Day Checklist

Company:

- Mission: Protect Life.
- Ecosystem: body cameras, in-car cameras, TASER, Evidence, real-time ops, AI/productivity.
- Role: camera systems, secure/privacy-preserving embedded software.

Technical:

- C++ RAII/ownership/concurrency.
- Embedded Linux boot/debugging.
- TCP/UDP/DNS/Wi-Fi/Bluetooth basics.
- Secure boot/OTA/encryption/key handling.
- Unit/integration/system tests.
- Camera capture/storage/upload design.

Questions to ask:

- Product/subsystem scope.
- OS stack and testing infrastructure.
- Reliability challenges.
- Security/OTA/provisioning flow.
- First 90-day expectations.

---

## 30-Second Self Introduction

> "I am a C/C++ embedded software engineer focused on reliable device software. I have prepared around Axon's camera systems by studying embedded Linux, networking, testing, secure firmware update, evidence integrity, and privacy-preserving logging. I am especially interested in building device software that is robust in the field: it records locally, survives poor connectivity and power loss, uploads reliably, and provides enough diagnostics without compromising sensitive data."
