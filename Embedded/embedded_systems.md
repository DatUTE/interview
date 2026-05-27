# Embedded Systems — Index

Study guide split into 12 focused files, from hardware fundamentals to professional-level safety and security.

---

## Files

| # | File | Topics |
|---|------|--------|
| 01 | [embedded_01_digital_hardware_fundamentals.md](embedded_01_digital_hardware_fundamentals.md) | Number systems, two's complement, fixed-point, endianness, FSMs, Cortex-M architecture, pipeline, registers, clock tree, reset types, memory types (NOR/NAND/SRAM) |
| 02 | [embedded_02_peripheral_fundamentals.md](embedded_02_peripheral_fundamentals.md) | GPIO (push-pull/open-drain/AF mux), NVIC, ISR best practices, interrupt latency, edge vs level, timers (PWM/input capture), ADC (SAR/Σ-Δ, Nyquist, anti-aliasing), DMA (ping-pong buffer, cache coherency) |
| 03 | [embedded_03_communication_protocols.md](embedded_03_communication_protocols.md) | UART (frame, baud tolerance, RS-485, flow control), SPI (CPOL/CPHA modes), I2C (START/STOP/ACK, clock stretching, pull-up sizing), CAN (arbitration, error confinement, CAN FD), USB (enumeration, transfer types) |
| 04 | [embedded_04_cpp_specifics.md](embedded_04_cpp_specifics.md) | `volatile` (what it does/doesn't do), fixed-width types, bitfields pitfalls, bit manipulation macros, linker scripts, startup code (.data/.bss init), C++ in embedded (CRTP, constexpr, placement new, no-heap ring buffer) |
| 05 | [embedded_05_memory_management.md](embedded_05_memory_management.md) | Stack overflow detection (watermark), heap pitfalls, memory pool allocator, MPU regions/permissions, Cortex-M7 D-cache coherency with DMA (clean/invalidate), non-cached regions |
| 06 | [embedded_06_rtos.md](embedded_06_rtos.md) | Preemptive vs cooperative, RMA/EDF scheduling, context switch mechanics, semaphore vs mutex, priority inversion (PIP/PCP), deadlock (Coffman conditions), FreeRTOS API (tasks/queues/critical sections/watermark), WCET/jitter |
| 07 | [embedded_07_power_management.md](embedded_07_power_management.md) | Sleep modes (sleep/stop/standby/shutdown), `__WFI`/`__WFE`, tickless FreeRTOS, event-driven vs polling, clock gating, LDO vs DCDC, 10-point low-power checklist |
| 08 | [embedded_08_bootloader_ota.md](embedded_08_bootloader_ota.md) | Bootloader responsibilities, jump-to-app (VTOR/SP/PC), flash layout, full boot sequence (POR → main), A/B OTA partitioning, CRC32 verification, secure boot chain (eFuse/OTP → ROM BL → app) |
| 09 | [embedded_09_debugging_toolchain.md](embedded_09_debugging_toolchain.md) | JTAG vs SWD, SWO/ITM printf, GDB+OpenOCD workflow, hard fault handler (register dump, CFSR decoding, addr2line), GCC optimization flags, dead-code elimination, map file analysis, binary tools, host-side unit testing, WCET measurement |
| 10 | [embedded_10_embedded_linux.md](embedded_10_embedded_linux.md) | Boot sequence (ROM BL → SPL → U-Boot → kernel → init), device tree, platform driver skeleton, character devices, top/bottom half IRQ (tasklet/workqueue/threaded IRQ), kernel memory allocation, PREEMPT_RT, RT process scheduling |
| 11 | [embedded_11_safety_security.md](embedded_11_safety_security.md) | ISO 26262 ASIL A–D, IEC 61508 SIL, MISRA C:2012 key rules, watchdog/CRC/redundancy fault tolerance, secure boot chain, TrustZone (ARMv8-M, TF-M API), common vulnerabilities (debug exposure, overflow, side-channel), RDP levels |
| 12 | [embedded_12_interview_hot_topics.md](embedded_12_interview_hot_topics.md) | 10 fully worked senior Q&As: priority inversion, `volatile` bugs, lockless SPSC ring buffer, boot sequence, DMA cache bug, hard fault analysis, debounce, mutex vs semaphore, WCET measurement, low-power checklist |

---

## Suggested Study Order

**Week 1 — Hardware Layer**
1. `embedded_01` — architecture and memory
2. `embedded_02` — peripherals and interrupts
3. `embedded_03` — communication protocols

**Week 2 — Software Layer**
4. `embedded_04` — C/C++ embedded specifics
5. `embedded_05` — memory management and MPU
6. `embedded_06` — RTOS (most interview-dense topic)

**Week 3 — Systems**
7. `embedded_07` — power management
8. `embedded_08` — bootloader and OTA
9. `embedded_09` — debugging and toolchain

**Week 4 — Professional Level**
10. `embedded_10` — embedded Linux
11. `embedded_11` — safety and security
12. `embedded_12` — interview Q&A drill (review daily)
