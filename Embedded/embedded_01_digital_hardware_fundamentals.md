# Embedded Systems 01 — Digital & Hardware Fundamentals

---

## 1.1 Number Systems & Arithmetic

**Binary and Hexadecimal**

Every embedded engineer works directly with bit patterns. A byte `0xA5` = `1010 0101` in binary. Reading hex fluently is essential for register maps and memory addresses.

| Decimal | Binary    | Hex  |
|---------|-----------|------|
| 0       | 0000 0000 | 0x00 |
| 127     | 0111 1111 | 0x7F |
| 128     | 1000 0000 | 0x80 |
| 255     | 1111 1111 | 0xFF |

**Two's Complement**

Standard representation for signed integers. To negate: invert all bits, add 1.

```
+5  = 0000 0101
-5  = 1111 1010 + 1 = 1111 1011
```

For an N-bit value:
- Range: -2^(N-1) to 2^(N-1) - 1
- `int8_t`: -128 to 127
- `int16_t`: -32768 to 32767

Overflow wraps around silently — undefined behavior in C for signed types, defined for unsigned.

**Fixed-Point Arithmetic**

Used in DSP and control loops when floating-point hardware is absent. Format `Qm.n`: m integer bits, n fractional bits.

```c
// Q16.16 format: 16 integer bits, 16 fractional bits
typedef int32_t q16_t;
#define Q16_ONE   (1 << 16)           // 1.0 in Q16.16
#define FLOAT_TO_Q16(x) ((q16_t)((x) * Q16_ONE))
#define Q16_MUL(a, b)   ((q16_t)(((int64_t)(a) * (b)) >> 16))

q16_t a = FLOAT_TO_Q16(1.5);   // 0x00018000
q16_t b = FLOAT_TO_Q16(2.0);   // 0x00020000
q16_t c = Q16_MUL(a, b);       // 3.0 = 0x00030000
```

**Endianness**

- **Little-endian** (ARM Cortex-M default, x86): LSB at lowest address
- **Big-endian** (network byte order, some DSPs): MSB at lowest address

```c
// Detecting endianness at runtime
bool is_little_endian(void) {
    uint16_t x = 0x0001;
    return *(uint8_t*)&x == 0x01;
}

// Byte swap (when receiving big-endian data on little-endian CPU)
uint32_t bswap32(uint32_t x) {
    return ((x & 0xFF000000) >> 24) |
           ((x & 0x00FF0000) >>  8) |
           ((x & 0x0000FF00) <<  8) |
           ((x & 0x000000FF) << 24);
}
```

---

## 1.2 Logic Design & State Machines

**Boolean Algebra Laws**
- De Morgan: `!(A && B) == !A || !B`
- Complement: `A & ~A = 0`, `A | ~A = 1`
- Absorption: `A | (A & B) = A`

**Combinational vs Sequential Circuits**
- **Combinational**: output depends only on current inputs (gates, mux, adder)
- **Sequential**: output depends on inputs AND current state (flip-flops, counters, FSMs)

**Finite State Machine (FSM) in firmware:**

```c
typedef enum { STATE_IDLE, STATE_RUNNING, STATE_ERROR, STATE_COUNT } State;

typedef struct {
    State current;
} FSM;

void fsm_update(FSM *fsm, Event event) {
    switch (fsm->current) {
        case STATE_IDLE:
            if (event == EVT_START) fsm->current = STATE_RUNNING;
            break;
        case STATE_RUNNING:
            if (event == EVT_STOP)  fsm->current = STATE_IDLE;
            if (event == EVT_ERROR) fsm->current = STATE_ERROR;
            break;
        case STATE_ERROR:
            if (event == EVT_RESET) fsm->current = STATE_IDLE;
            break;
        default: break;
    }
}
```

**Moore vs Mealy FSM**
- **Moore**: output depends only on state (safer in hardware, easier to reason about)
- **Mealy**: output depends on state AND current input (fewer states, but harder to debug)

---

## 1.3 Microcontroller Architecture

**Harvard vs Von Neumann**

| Feature         | Von Neumann      | Harvard           |
|-----------------|------------------|-------------------|
| Memory buses    | Single shared    | Separate (code/data) |
| Simultaneous    | No               | Fetch + load same cycle |
| Examples        | x86, ARM Cortex-A | AVR, ARM Cortex-M (modified Harvard) |

ARM Cortex-M is *modified* Harvard: physically separate internal instruction/data buses, but unified address space visible to programmer.

**CPU Pipeline (ARM Cortex-M)**

Cortex-M3/M4 use a 3-stage pipeline: Fetch → Decode → Execute. Pipeline hazards:
- **Data hazard**: instruction needs result not yet written back
- **Control hazard**: branch flushes pipeline (branch prediction mitigates this on M4/M7)

**Cortex-M Core Registers**

| Register | Name              | Purpose                          |
|----------|-------------------|----------------------------------|
| R0–R12   | General purpose   | Computation, argument passing    |
| R13 (SP) | Stack Pointer     | MSP (main) / PSP (process)       |
| R14 (LR) | Link Register     | Return address from subroutine   |
| R15 (PC) | Program Counter   | Next instruction address         |
| xPSR     | Program Status    | Flags: N,Z,C,V,Q, ISR number     |
| PRIMASK  | Priority mask     | Disable all interrupts           |
| CONTROL  | Control           | Thread/Handler mode, stack select|

**Instruction Set — ARM Thumb-2**

Cortex-M uses Thumb-2: mix of 16-bit and 32-bit instructions. No need to switch modes (unlike ARMv4T). Key instructions:
- `MOV`, `LDR`, `STR` — data movement
- `ADD`, `SUB`, `MUL` — arithmetic
- `AND`, `ORR`, `EOR`, `BIC` — bitwise
- `B`, `BL`, `BX`, `BLX` — branches
- `PUSH`, `POP` — stack
- `MRS`, `MSR` — access special registers

**Clock System**

```
HSE (crystal) ─┐
HSI (internal) ─┤─→ PLL ─→ SYSCLK ─→ AHB prescaler ─→ APB1/APB2 prescalers
LSE (32kHz)   ─┤                                          │
LSI (internal) ─┘                                    Peripherals
```

- **PLL**: multiplies reference clock (e.g., 8 MHz HSE × 9 = 72 MHz SYSCLK on STM32F1)
- **AHB**: Advanced High-performance Bus — CPU, DMA, SRAM
- **APB**: Advanced Peripheral Bus — GPIO, UART, SPI, I2C

**Reset Types**

| Type    | Cause                            | Clears                    |
|---------|----------------------------------|---------------------------|
| POR     | Power-on (voltage crosses threshold) | Everything           |
| External| NRST pin asserted                | Almost everything         |
| WDT     | Watchdog timeout                 | Almost everything         |
| Soft    | SYSRESETREQ bit in AIRCR         | Peripherals, most state   |
| Lockup  | CPU hard fault during hard fault | CPU + peripherals         |

---

## 1.4 Memory Types

| Memory    | Volatile | Writable | Speed    | Use Case               |
|-----------|----------|----------|----------|------------------------|
| SRAM      | Yes      | Yes      | Fastest  | Stack, heap, variables |
| DRAM      | Yes      | Yes      | Slower   | Large RAM (Linux SBC)  |
| NOR Flash | No       | Erase+Write | Fast read | Code execution      |
| NAND Flash| No       | Erase+Write | Slow read | Mass storage (eMMC)  |
| EEPROM    | No       | Byte-level | Slow   | Config, calibration    |
| ROM       | No       | No       | Fast     | Mask-programmed bootrom|

**NOR vs NAND Flash**

| Feature         | NOR                 | NAND                |
|-----------------|---------------------|---------------------|
| Read            | Random access, fast | Sequential, slower  |
| Erase unit      | Sector (~64KB)      | Block (~128KB)      |
| XIP             | Yes (execute-in-place) | No              |
| Endurance       | ~100K cycles        | ~10K–100K cycles    |
| Bit errors      | Rare                | Needs ECC           |
| Capacity        | KB–MB               | MB–TB               |

**Memory-Mapped I/O**

Peripheral registers appear as memory addresses. Access them via volatile pointers:

```c
// Direct register access — no HAL
#define GPIOA_BASE   0x40010800U
#define GPIOA_ODR    (*(volatile uint32_t*)(GPIOA_BASE + 0x0C))

GPIOA_ODR |= (1U << 5);   // Set PA5 high
```

The `volatile` tells the compiler to never optimize away or reorder these accesses.

---

## Interview Questions

### Easy

**Q: What is two's complement and how do you negate a number using it?**

> Invert all bits, then add 1. Example: `+5 = 0000 0101` → invert → `1111 1010` → +1 → `1111 1011 = -5`. This makes subtraction identical to addition in hardware. For N-bit: range is -2^(N-1) to 2^(N-1)-1. `INT8_MIN = -128` has no positive counterpart — negating it overflows (UB for signed in C).

**Q: What is the difference between little-endian and big-endian? Which does ARM Cortex-M use by default?**

> Little-endian: least-significant byte at the lowest memory address. Big-endian: most-significant byte first. Cortex-M is little-endian by default. Matters when casting `uint32_t*` to `uint8_t*`, or when sending multi-byte values over a network (which uses big-endian / network byte order).

**Q: What are the differences between SRAM, NOR Flash, and NAND Flash for embedded use?**

> SRAM: volatile, fastest, byte-writable — used for stack, heap, variables. NOR Flash: non-volatile, random-access readable (supports XIP — execute in place), sector-erase before write — used for code. NAND Flash: non-volatile, higher density, no XIP, needs ECC — used for mass storage (eMMC).

**Q: What is a finite state machine (FSM) and why is it useful in firmware?**

> An FSM is always in exactly one of a finite set of states and transitions based on events/inputs. Useful in firmware because it makes control logic explicit, testable, and easy to reason about. Common uses: protocol parsers, button handling, motor sequencing, power management.

---

### Mid

**Q: What registers does Cortex-M hardware automatically push to the stack on exception entry, and why does this matter for ISR code?**

> Hardware auto-saves {R0, R1, R2, R3, R12, LR, PC, xPSR} — 8 words / 32 bytes. The RTOS must manually save the remaining callee-saved registers {R4–R11}. This matters because: (1) the stacked PC in a HardFault handler points to the faulting instruction, (2) ISR code can freely use R0–R3 and R12 without saving them, (3) tail-chaining and late-arrival optimizations rely on this fixed stacking layout.

**Q: What is the difference between Moore and Mealy FSMs? When would you choose one over the other?**

> Moore: output depends only on current state — outputs are stable and don't glitch on input changes. Mealy: output depends on state AND current input — fewer states needed but can produce transient glitches. In firmware: Moore for output control (actuator commands, LEDs). Mealy acceptable for pure data processing (protocol decoders) where glitches don't matter.

**Q: On an STM32, what is the AHB bus and how does it relate to the APB bus?**

> AHB (Advanced High-performance Bus): high-speed bus connecting the CPU core, DMA controller, and SRAM. APB (Advanced Peripheral Bus): lower-speed bus hanging off AHB via a bridge, connecting most peripherals (GPIO, UART, SPI, I2C, ADC). APB typically runs at AHB/2 or AHB/4. Peripheral register access path: CPU → AHB matrix → APB bridge → peripheral register. You must enable the correct RCC clock for each peripheral before accessing its registers.

---

### Hard

**Q: Configure a PLL on an STM32F4 to achieve 168 MHz from an 8 MHz HSE. VCO input must be 1–2 MHz, VCO output must be 192–432 MHz. What are valid PLL_M, PLL_N, and PLL_P values?**

> VCO input = HSE / PLL_M → need 1–2 MHz: **PLL_M = 8** → VCO input = 1 MHz.
> VCO output = VCO input × PLL_N → need 192–432 MHz: **PLL_N = 336** → VCO = 336 MHz.
> SYSCLK = VCO / PLL_P (must be 2/4/6/8): **PLL_P = 2** → SYSCLK = 168 MHz ✓.
> USB clock check: need 48 MHz → **PLL_Q = 7** → 336/7 = 48 MHz ✓.

**Q: Represent 3.14159 in Q8.24 fixed-point. What is the hex value and the precision error?**

> Q8.24: multiply by 2^24 = 16,777,216 → 3.14159 × 16,777,216 = 52,707,178.7 → truncate to **52,707,178 = 0x03243F6A**.
> Recovered value: 52,707,178 / 16,777,216 = 3.14158988...
> Error ≈ 1.2 × 10⁻⁷ (less than 1 LSB = 5.96 × 10⁻⁸). Suitable for trig/control. Q8.24 gives ~7 decimal digits of precision, similar to single-precision float mantissa.

**Q: A watchdog resets the MCU during an EEPROM write that takes 5ms. The WDT timeout is 4ms. Increasing the WDT timeout is not allowed. How do you redesign the system?**

> Option 1: **Kick WDT inside the write loop** at safe checkpoints — only valid if the driver controls the write loop byte-by-byte. Option 2: **Defer writes asynchronously** — buffer writes and flush from a low-priority task that can be preempted, with a high-priority task kicking the WDT. Option 3: **Use a write-through cache** over NVS — accumulate dirty bytes, write once per cycle when safe. Option 3 is architecturally cleanest: it decouples the slow EEPROM from the real-time loop entirely and avoids special-casing the WDT for one slow operation.
