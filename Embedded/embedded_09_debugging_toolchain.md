# Embedded Systems 09 — Debugging & Toolchain

---

## 9.1 Debug Interfaces

**JTAG vs SWD**

| Feature      | JTAG                    | SWD (Serial Wire Debug)      |
|--------------|-------------------------|------------------------------|
| Pins         | TDI, TDO, TCK, TMS, TRST | SWDIO, SWCLK (2 pins!)       |
| Chain        | Daisy-chain multiple devices | Point-to-point only      |
| Speed        | Good                    | Similar to JTAG             |
| Additional   | Boundary scan           | SWO trace output            |
| ARM support  | All cores               | Cortex-M preferred          |

**SWO (Serial Wire Output)**

Single-wire trace output. Used for ITM (Instrumentation Trace Macrocell):

```c
// printf over SWO — no UART needed
#include <stdint.h>
void ITM_SendChar(uint8_t ch) {
    while (!(ITM->PORT[0].u32 & 1)) { }  // wait until port ready
    ITM->PORT[0].u8 = ch;
}
```

**GDB + OpenOCD Workflow**

```bash
# Terminal 1: start OpenOCD (gdbserver on port 3333)
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg

# Terminal 2: connect GDB
arm-none-eabi-gdb firmware.elf
(gdb) target remote :3333
(gdb) monitor reset halt
(gdb) load                  # flash the firmware
(gdb) break main
(gdb) continue
(gdb) info registers        # inspect ARM registers
(gdb) x/10x 0x20000000     # examine memory
(gdb) watch variable        # hardware watchpoint
```

**Hard Fault Debugging**

```c
// Hard fault handler — dump registers from stacked frame
void HardFault_Handler(void) {
    __asm volatile (
        "tst lr, #4          \n"  // test bit 2 of LR (EXC_RETURN)
        "ite eq              \n"
        "mrseq r0, msp       \n"  // bit2=0: MSP was used
        "mrsne r0, psp       \n"  // bit2=1: PSP was used
        "b hard_fault_handler_c \n"
    );
}

void hard_fault_handler_c(uint32_t *frame) {
    uint32_t r0  = frame[0];
    uint32_t r1  = frame[1];
    uint32_t r2  = frame[2];
    uint32_t r3  = frame[3];
    uint32_t r12 = frame[4];
    uint32_t lr  = frame[5];
    uint32_t pc  = frame[6];  // address of faulting instruction
    uint32_t psr = frame[7];

    // Read fault status registers
    uint32_t hfsr  = SCB->HFSR;   // Hard Fault Status Register
    uint32_t cfsr  = SCB->CFSR;   // Configurable Fault Status Register
    uint32_t mmfar = SCB->MMFAR;  // MemManage Fault Address
    uint32_t bfar  = SCB->BFAR;   // BusFault Address

    // Log or breakpoint here
    while (1) { }
}
```

**Interpreting CFSR**

```
CFSR [31:16] = UFSR (Usage Fault):
  bit 25: DIVBYZERO  — divide by zero (if DIV_0_TRP set in CCR)
  bit 24: UNALIGNED  — unaligned access
  bit 19: NOCP       — coprocessor access (FPU not enabled)
  bit 18: INVPC      — invalid PC load
  bit 16: UNDEFINSTR — undefined instruction

CFSR [15:8] = BFSR (Bus Fault):
  bit 15: BFARVALID  — BFAR contains valid address
  bit 13: STKERR     — bus fault on stacking
  bit 12: UNSTKERR   — bus fault on unstacking
  bit 11: IMPRECISERR — imprecise bus fault
  bit 10: PRECISERR  — precise bus fault (BFAR valid)
  bit 8:  IBUSERR    — instruction bus error

CFSR [7:0] = MMFSR (MemManage Fault):
  bit 7: MMARVALID  — MMFAR contains valid address
  bit 4: MSTKERR    — fault on stacking
  bit 3: MUNSTKERR  — fault on unstacking
  bit 1: DACCVIOL   — data access violation
  bit 0: IACCVIOL   — instruction access violation
```

**Hard Fault Root Cause Analysis Steps**

1. Read `PC` from stacked frame → address of faulting instruction
2. `arm-none-eabi-addr2line -e firmware.elf 0x0800xxxx` → source file:line
3. Read `CFSR` for fault type (IACCVIOL / DACCVIOL / PRECISERR / UNDEFINSTR)
4. If `BFARVALID` or `MMARVALID` set → read BFAR/MMFAR for faulting address
5. Common causes:
   - Null pointer dereference → MMFAR = 0x00000000 + offset
   - Stack overflow → PC in BSS/heap region, SP past stack limit
   - Unaligned access → CFSR UNALIGNED bit, or bad typecast
   - Missing FPU enable → NOCP bit (forgot `SCB->CPACR |= 0xF << 20`)

---

## 9.2 Build Toolchain

**GCC Optimization Flags**

| Flag   | Level      | Notes                                      |
|--------|------------|--------------------------------------------|
| `-O0`  | None       | Fastest compile, largest binary, best debug|
| `-O1`  | Light      | Basic optimizations                        |
| `-O2`  | Standard   | Inlining, loop opt, no size-speed tradeoff |
| `-O3`  | Aggressive | Vectorization, may increase code size      |
| `-Os`  | Size       | Optimize for minimum code size             |
| `-Og`  | Debug      | Optimizations compatible with debugging    |

**Dead Code Elimination**

```makefile
# Compile each function/variable in its own section
CFLAGS += -ffunction-sections -fdata-sections
# Linker removes unreferenced sections
LDFLAGS += -Wl,--gc-sections
```

**Map File Analysis**

```bash
arm-none-eabi-ld ... -Wl,-Map=output.map
# Then inspect:
# - Section sizes (.text, .data, .bss)
# - Which object file contributes what symbols
# - Large unexpected symbols (e.g., printf pulls in 4KB of formatting code)
```

**Binary Analysis Tools**

```bash
# Section sizes
arm-none-eabi-size firmware.elf

# Disassemble
arm-none-eabi-objdump -d firmware.elf | grep -A5 "hard_fault"

# List symbols with sizes
arm-none-eabi-nm --print-size --size-sort firmware.elf

# Dump all ELF info
arm-none-eabi-readelf -a firmware.elf
```

---

## 9.3 Unit Testing for Embedded

**Testing Strategy**

```
Host PC tests (fast, no hardware needed):
├── Unit tests with mocked hardware (Unity/CppUTest/Catch2)
├── Algorithm tests (DSP, control, protocol parsers)
└── Logic tests (state machines, data structures)

Hardware-in-the-loop (HIL):
├── Full integration on real hardware
├── Automated GPIO stimulation
└── Logic analyzer capture for protocol verification
```

**Example: Testing a CRC function on host**

```cpp
// CRC implementation (target + host)
// test_crc.cpp (compiles on x86 with Catch2)
#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include "crc.h"

TEST_CASE("CRC32 known value") {
    const uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
    REQUIRE(crc32(data, 4) == 0xB63CFBCD);
}

TEST_CASE("CRC32 empty") {
    REQUIRE(crc32(nullptr, 0) == 0xFFFFFFFF ^ 0xFFFFFFFF);
}
```

**WCET Measurement**

```c
// Method 1: DWT cycle counter (Cortex-M3+)
CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
DWT->CYCCNT = 0;
DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;

uint32_t start = DWT->CYCCNT;
critical_function();
uint32_t cycles = DWT->CYCCNT - start;
float time_us = (float)cycles / SystemCoreClock * 1e6f;

// Method 2: Toggle GPIO and measure with logic analyzer
HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_SET);
critical_function();
HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_RESET);
```

WCET analysis must cover:
- Worst-case input data (longest code path)
- Cache cold vs warm state
- Interrupt preemption (if WCET includes worst-case interrupt latency)
- Branch predictor misses (on high-end cores)

---

## Interview Questions

### Easy

**Q: What is the difference between JTAG and SWD?**

> **JTAG**: 5 pins (TDI, TDO, TCK, TMS, TRST). Supports daisy-chaining multiple devices. Also enables boundary scan for PCB testing. **SWD**: 2 pins (SWDIO, SWCLK). Point-to-point only, but frees up 3 MCU pins. Also supports SWO (Serial Wire Output) for trace output on a third optional pin. Same debug capability as JTAG for Cortex-M. SWD is standard for most ARM embedded work; JTAG is used when boundary scan or multi-device chains are needed.

**Q: What do `-ffunction-sections` and `--gc-sections` do? Why use them together?**

> `-ffunction-sections` (and `-fdata-sections`): compiles each function/variable into its own named ELF section (e.g., `.text.foo`). `--gc-sections` (linker flag): at link time, removes all sections that are never referenced from any entry point. Together they eliminate dead code and unused data. Without them, if any object file is linked, all of its code is included even if 90% is unused. With them, only the reachable call graph is included — critical for keeping firmware size minimal.

**Q: Which tool do you use to find which function contributes the most flash usage?**

> `arm-none-eabi-nm --print-size --size-sort firmware.elf` — lists all symbols sorted by size, showing the largest code and data contributors. For a section-level view: `arm-none-eabi-size firmware.elf`. For detailed per-file breakdown: inspect the `.map` file generated with `-Wl,-Map=output.map`. The map file shows exactly which object file contributed each symbol to each section.

---

### Mid

**Q: Your firmware crashes randomly after 2–3 hours with the PC at 0x00000000. What happened and how do you debug it?**

> PC = 0 is a **null pointer function call** — a function pointer was NULL when called, or a vtable slot was corrupt. The CPU fetched the reset vector (usually at 0x08000000) from address 0, but address 0 in flash might decode as a `B 0` branch, spinning or immediately crashing. Debug: (1) Enable the MemManage fault handler — accessing address 0 should trigger IACCVIOL if MPU is configured with no-access at 0x00000000. (2) Add a null-pointer check on all function pointers before calling. (3) Check for heap corruption that overwrites a function pointer table. (4) Enable the watchdog and add a trace log before the last known-good state.

**Q: Explain how to use a GDB watchpoint to catch the exact moment a variable is corrupted.**

> ```gdb
> (gdb) target remote :3333
> (gdb) watch global_var         # hardware write watchpoint
> (gdb) watch -l *0x20001234     # watch memory address directly
> (gdb) rwatch global_var        # read watchpoint
> (gdb) awatch global_var        # read/write watchpoint
> (gdb) continue                 # run until watchpoint fires
> (gdb) backtrace                # see who wrote the value
> ```
> Cortex-M supports 4 hardware watchpoints (DWT comparators). Unlike software breakpoints, hardware watchpoints require zero code modification and add no overhead. They fire on the exact instruction that writes the corrupt value.

**Q: Your build with `-O2` passes all tests but `-O0` fails. What is likely wrong?**

> Classic symptoms of **missing `volatile`**: with `-O0`, the compiler re-reads every variable from memory on every access (no optimization). With `-O2`, the compiler caches values in registers and elides "redundant" loads. If an ISR-shared variable or hardware register is accessed without `volatile`, `-O2` may use a stale register value. Other causes: (1) **Uninitialized variable** — `-O0` tends to zero stack memory by coincidence; `-O2` reuses stack space aggressively. (2) **Timing-dependent bug** — `-O2` runs faster and exposes race conditions. Fix: add `volatile` to all ISR-shared and hardware register variables; initialize all locals explicitly.

---

### Hard

**Q: A hard fault fires with CFSR = 0x00000200 (PRECISERR set) and BFAR = 0x20008400. The linker map shows SRAM ends at 0x20008000. What happened?**

> CFSR bit 9 = PRECISERR (Bus Fault Status Register): a precise data access bus error. BFARVALID (bit 15) should also be set for BFAR to be valid. BFAR = 0x20008400 = SRAM end (0x20008000) + 0x400. This is **0x400 bytes past the end of SRAM** — a stack overflow. The stack (growing downward from the top of SRAM) has grown past the SRAM boundary into unmapped address space, causing a bus fault on the next stack push. To find the root cause in code: (1) Check `arm-none-eabi-addr2line -e firmware.elf PC_from_stacked_frame`. (2) Look for large stack allocations (local arrays, `alloca`). (3) Check recursive functions without depth limits. (4) Add an MPU guard page at the stack bottom.

**Q: You have a reproducible 5µs jitter spike on a GPIO toggled in a timer ISR. No RTOS. Several other interrupts active. Describe how to isolate and eliminate it.**

> Systematic isolation: (1) **Measure with DWT cycle counter** inside the ISR: `t1 = DWT->CYCCNT` at entry, `t2` at toggle — log worst-case delta. (2) **Disable interrupts one-by-one**: temporarily mask each other IRQ and re-measure — when jitter disappears, the masked IRQ was the cause. (3) **Check interrupt priority**: if the guilty interrupt has equal or higher priority, it can preempt the timer ISR. Raise the timer ISR to the highest priority. (4) **Check flash wait states**: if the timer ISR is in flash and another IRQ is fetching from flash simultaneously, the I-bus can stall. Move the ISR to RAM (`__attribute__((section(".RamFunc")))`). (5) **Check DMA burst transfers**: DMA can hold the AHB bus, stalling the CPU — configure DMA to lower bus priority or use non-bursting mode.

**Q: Your cross-compiled firmware grew 30% after a GCC 10 → GCC 13 upgrade. How do you systematically identify and reduce it?**

> (1) **Compare map files**: `diff` the two `.map` files. Sort symbols by size in each and compare. Look for new symbols, grown symbols, or new library functions pulled in. (2) **Check default flags**: GCC 13 may enable new auto-vectorization or loop unrolling by default under `-O2`. Add `-fno-tree-vectorize` or `-fno-unroll-loops` to test. (3) **Check LTO (Link-Time Optimization)**: if LTO was working in GCC 10 but broke in GCC 13, dead code elimination may be reduced. (4) **Inspect new warnings-as-errors**: GCC 13 may require additional code paths for new safety checks. (5) **Add `-Wl,--print-size-sections`** and compare section sizes. (6) **Use `bloaty`** (open-source binary size profiler) for visual diff between the two ELFs. Address the top 3 growth contributors first — usually one or two functions account for most of the increase.
