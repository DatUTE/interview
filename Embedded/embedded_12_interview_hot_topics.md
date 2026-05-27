# Embedded Systems 12 — Senior Interview Hot Topics

---

## Q1: Explain Priority Inversion and Solutions

Priority inversion: high-priority task blocked by low-priority task holding a shared resource, while medium-priority tasks run freely.

```
Time →
T_high (P=3):   ──BLOCKED by mutex──────────RUNS
T_medium (P=2):         RUNS (preempts T_low!)──
T_low (P=1):  RUNS─holds mutex─(preempted)──gives mutex
```

Three solutions:

1. **Unbounded inversion (no fix)**: medium-priority tasks can delay high-priority task indefinitely
2. **Priority Inheritance (PIP)**: T_low temporarily inherits T_high's priority while holding mutex. T_medium can no longer preempt T_low. FreeRTOS mutexes implement this automatically.
3. **Priority Ceiling (PCP)**: each mutex has a fixed ceiling = highest priority of any task that will lock it. Tasks raise to ceiling immediately on lock. Prevents deadlock AND inversion. Requires static analysis of lock usage.

---

## Q2: Identify `volatile` Bugs

```c
// Bug 1: missing volatile on ISR-shared flag
bool flag = false;           // WRONG
volatile bool flag = false;  // CORRECT

// Bug 2: volatile on wrong thing
volatile uint32_t *ptr = (uint32_t*)0x40000000;  // CORRECT (pointer to volatile)
uint32_t * volatile ptr = ...;  // volatile pointer (rarely needed)

// Bug 3: volatile doesn't guarantee atomicity on multi-byte types
volatile uint64_t counter;  // 64-bit read may be two 32-bit reads — not atomic on 32-bit CPU
// Fix: disable interrupts around the access, or use atomic_uint64_t (C11)

// Bug 4: volatile doesn't prevent compiler reordering relative to other volatiles
// Need memory barriers for ordering:
__DMB();  // Data Memory Barrier (ARM)
```

---

## Q3: Lockless Ring Buffer (Single Producer, Single Consumer)

```c
// Works without mutex for SPSC — producer writes head, consumer reads tail
// Key: head and tail are only written by one side each

#define BUF_SIZE 256  // must be power of 2 for mask trick

typedef struct {
    volatile uint32_t head;
    volatile uint32_t tail;
    uint8_t buf[BUF_SIZE];
} RingBuf;

bool rb_push(RingBuf *rb, uint8_t val) {
    uint32_t next_head = (rb->head + 1) & (BUF_SIZE - 1);
    if (next_head == rb->tail) return false;  // full
    rb->buf[rb->head] = val;
    __DMB();                                   // ensure write before head update
    rb->head = next_head;
    return true;
}

bool rb_pop(RingBuf *rb, uint8_t *val) {
    if (rb->tail == rb->head) return false;   // empty
    *val = rb->buf[rb->tail];
    __DMB();                                   // ensure read before tail update
    rb->tail = (rb->tail + 1) & (BUF_SIZE - 1);
    return true;
}
```

---

## Q4: Boot Sequence from Power-On to `main()`

1. **POR**: voltage crosses threshold → release reset
2. **ROM bootloader** (if present): clock init, load/verify bootloader from flash
3. **Bootloader / startup_stm32xx.s**:
   - Stack pointer loaded from vector table word 0
   - `Reset_Handler` invoked from vector table word 1
   - Copy `.data` section from Flash LMA → SRAM VMA
   - Zero `.bss` section
   - Call `SystemInit()`: configure PLL, clocks, Flash wait states
   - Call C++ static constructors (`__libc_init_array`)
4. **`main()`** begins

---

## Q5: DMA Cache Coherency Bug

**Symptom**: DMA transfer completes successfully (DMA reports done), but CPU reads stale/wrong data from destination buffer.

**Root cause**: Cortex-M7 D-cache. CPU previously read buf → cache line loaded. DMA writes new data to RAM → cache still holds old data. CPU read hits cache → returns stale value.

**Fix**:
```c
// Before DMA FROM memory (CPU → Peripheral):
SCB_CleanDCache_by_Addr((uint32_t*)tx_buf, buf_size);  // flush CPU writes to RAM

// After DMA TO memory (Peripheral → CPU):
SCB_InvalidateDCache_by_Addr((uint32_t*)rx_buf, buf_size);  // discard stale cache
```

**Alternative fix**: place DMA buffers in MPU-marked non-cacheable region.

---

## Q6: Hard Fault Root Cause Analysis

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

## Q7: Debounce Algorithm

```c
// Software debounce: stable for N consecutive samples
#define DEBOUNCE_MS      20
#define SAMPLE_PERIOD_MS  1

typedef struct {
    bool     state;
    bool     raw;
    uint32_t stable_count;
} Debounce;

// Call every SAMPLE_PERIOD_MS ms
bool debounce_update(Debounce *d, bool raw_input) {
    bool changed = false;
    if (raw_input == d->raw) {
        if (d->stable_count < DEBOUNCE_MS / SAMPLE_PERIOD_MS)
            d->stable_count++;
        if (d->stable_count == DEBOUNCE_MS / SAMPLE_PERIOD_MS) {
            if (raw_input != d->state) {
                d->state  = raw_input;
                changed   = true;
            }
        }
    } else {
        d->stable_count = 0;
        d->raw = raw_input;
    }
    return changed;
}
```

---

## Q8: Mutex vs Semaphore

| Property         | Mutex                               | Semaphore                          |
|------------------|-------------------------------------|------------------------------------|
| Ownership        | Yes — only owner can release        | No — any task can give/take        |
| Priority inherit | Yes (FreeRTOS)                      | No                                 |
| Initial state    | Unlocked (1)                        | Any count (0 for signaling)        |
| Use case         | Mutual exclusion of shared resource | Signaling, counting resources      |
| ISR safe         | No (can't take mutex in ISR)        | Yes (`xSemaphoreGiveFromISR`)      |

**Rule of thumb:**
- Protecting shared data → **mutex**
- ISR notifying a task → **binary semaphore**
- Pool of N resources → **counting semaphore**

---

## Q9: Measure Worst-Case Execution Time (WCET)

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

## Q10: Low-Power Design for Battery-Powered Device

Design checklist:
1. **Event-driven** — never poll; use interrupts and `WFI/WFE`
2. **Minimize active time** — wake → work → sleep as fast as possible
3. **Tickless RTOS** — disable SysTick when idle
4. **Clock gating** — disable peripheral clocks when unused
5. **Reduce SYSCLK** — run at lowest frequency that meets timing requirements
6. **Optimize peripherals** — turn off ADC, DAC between conversions
7. **Use low-power peripherals** — LPTIM, LPUART operate in Stop mode
8. **Voltage scaling** — many MCUs support lower VDD at lower clock speeds
9. **Avoid large pull resistors** on unused pins (floating inputs drain current)
10. **Profile current** — use power profiler (e.g., Nordic PPK2, Otii Arc) to find unexpected active periods

---

## Quick Reference: Common Embedded Interview Themes

| Theme | Key Points |
|-------|-----------|
| `volatile` | Prevents optimization, does NOT give atomicity or ordering |
| Priority inversion | PIP (FreeRTOS default) vs PCP (static ceiling) |
| DMA + cache | Clean before TX, Invalidate after RX on Cortex-M7 |
| Mutex vs semaphore | Mutex = ownership + PIP; Semaphore = ISR signaling |
| Hard fault | PC from stacked frame + CFSR decode + addr2line |
| Boot sequence | POR → ROM BL → startup.s → main() |
| Stack overflow | Watermark pattern, MPU guard page, FreeRTOS high-water mark |
| WCET | DWT counter or GPIO toggle + logic analyzer |
| Debounce | Sample at fixed rate, require N stable readings |
| OTA update | A/B partition, CRC verify, rollback counter |

---

## Additional Practice Questions by Difficulty

### Easy

**Q: What is the difference between an ISR and a regular function in terms of calling convention?**

> A regular function: caller saves R0–R3, R12, LR before call; callee saves R4–R11 if it uses them. An ISR: triggered by hardware, not called by any code. Cortex-M hardware automatically saves {R0–R3, R12, LR, PC, xPSR} on the stack (exception frame). The ISR can freely use R0–R3 and R12 without saving them. It must return via `BX LR` with a special `EXC_RETURN` value (not a normal return address), which triggers hardware unstacking. ISRs should never use `return` in C — the compiler generates the correct `BX LR` via the `__attribute__((interrupt))` directive or `naked` function attribute in some ports.

**Q: A variable is declared `static` inside a function. Where is it stored and what is its lifetime?**

> A function-scope `static` variable is stored in the `.data` section (if initialized) or `.bss` section (if zero-initialized) — i.e., in RAM just like a global variable. Its lifetime is the entire program execution (it persists across calls). Its scope is limited to the function. In embedded: useful for state that must persist across calls (counters, previous state for debounce, first-call flags) without polluting the global namespace.

**Q: What happens if you call `xSemaphoreGive` (not `FromISR`) from an ISR in FreeRTOS?**

> Undefined behavior — corrupts the kernel state. The non-ISR API versions call the scheduler and access internal data structures without disabling the scheduler or using atomic primitives. If called from an ISR, they can be interrupted by another ISR mid-way, leaving the scheduler state inconsistent. Always use `xSemaphoreGiveFromISR` / `xQueueSendFromISR` inside interrupt handlers, and call `portYIELD_FROM_ISR(higher_priority_task_woken)` at the end to trigger a context switch if needed.

---

### Mid

**Q: You have two tasks that share a global `uint64_t` counter on a 32-bit MCU. One task increments it, the other reads it. No mutex. What can go wrong?**

> On a 32-bit MCU, a 64-bit read/write requires two 32-bit operations (high word and low word separately). If a context switch occurs between the two operations: the reader may see a high word from after the increment but a low word from before (or vice versa), producing a corrupted value (e.g., `0x0000000100000000` wraps to give `0xFFFFFFFF00000001` if read across the carry). Fix: use `taskENTER_CRITICAL`/`taskEXIT_CRITICAL` around 64-bit accesses, or use `atomic_uint64_t` (C11 atomics) if the platform supports it (usually requires a lock internally on 32-bit MCUs).

**Q: What is the SysTick timer and what happens in FreeRTOS when it fires?**

> SysTick is a 24-bit down-counter built into every Cortex-M core, typically configured to fire at `configTICK_RATE_HZ` (e.g., 1000 Hz = 1ms). In FreeRTOS, the SysTick ISR calls `xPortSysTickHandler()` which: (1) increments the tick count, (2) unblocks any tasks whose delay has expired, (3) triggers a context switch via PendSV if a higher-priority task is now ready. PendSV is used (not SysTick itself) to do the actual register save/restore, because PendSV has the lowest priority and can be safely deferred until no other ISR is running.

**Q: How do you transfer a 1MB file over UART at 115200 baud with no data loss on a bare-metal MCU?**

> At 115200 baud, throughput ≈ 11.5 KB/s. 1MB takes ~89 seconds. Strategy: (1) **DMA in circular mode**: configure UART RX DMA to fill a large ring buffer (e.g., 4KB) continuously. CPU processes data from the ring buffer in the main loop or a task. (2) **Hardware flow control (RTS/CTS)**: sender pauses transmission when the MCU's buffer is almost full. (3) **Protocol with ACK/NACK and retransmit**: for guaranteed delivery over noisy links, implement windowed acknowledgment. (4) **Increase baud rate** if the hardware supports it — 921600 baud reduces transfer time to ~11 seconds. Never use blocking polling (`while (!(UART->SR & RXNE))`) for large transfers — use DMA + interrupt.

---

### Hard

**Q: Implement a software UART transmitter (bit-bang) on a GPIO pin that is interrupt-safe and produces accurate timing at 9600 baud on a 72 MHz MCU. What are the key constraints?**

> 9600 baud → bit period = 104.2 µs = 7500 cycles at 72 MHz. Key constraints:
> (1) **Timing accuracy**: use the DWT cycle counter (`DWT->CYCCNT`) for timing — more accurate than SysTick which may be shared. Busy-wait each bit period: `while ((DWT->CYCCNT - start) < bit_cycles)`.
> (2) **Interrupt safety**: a higher-priority interrupt during a bit period causes a timing error. Disable interrupts during the entire frame (`__disable_irq()`/`__enable_irq()`), or raise the UART task to highest priority.
> (3) **Frame format**: start bit (low), 8 data bits LSB-first, stop bit (high). Total = 10 bits × 104.2µs = 1.042ms per byte.
> (4) **GPIO toggle time**: `BSRR` register write takes 1–2 cycles at AHB speed — negligible at 72 MHz vs 7500-cycle bit period.
> This approach works for occasional debug output. For continuous data, use a hardware UART.

**Q: You are designing a safe-state machine for an ASIL-D system. On detection of any fault, the system must reach a safe state within 5ms. Describe how you verify this claim during development and testing.**

> Verification strategy: (1) **Static WCET analysis**: use tools like Rapita RVS, AbsInt `aiT`, or manual analysis to compute the worst-case path from fault detection to safe-state actuation. Must be < 5ms including interrupt latency. (2) **Hardware fault injection**: use a test jig to inject faults (short pins, inject noise on analog inputs) and measure safe-state response time with a logic analyzer. (3) **Requirement tracing**: each code path to the safe-state function is traced back to the safety requirement in the requirements management tool (DOORS, Polarion). (4) **Code coverage (MC/DC)**: ASIL-D requires Modified Condition/Decision Coverage — every independent condition in decisions must be shown to independently affect the outcome. (5) **Regression testing**: fault injection tests run in CI on each merge. Any change to the safe-state path triggers a full re-run of timing tests.

**Q: A system has four periodic tasks with the following parameters. Is it schedulable under RMA? If not, what is the minimum change that makes it schedulable?**

> | Task | Period (ms) | WCET (ms) |
> |------|-------------|-----------|
> | T1   | 5           | 1.0       |
> | T2   | 10          | 2.0       |
> | T3   | 20          | 4.0       |
> | T4   | 50          | 5.0       |
>
> Total utilization U = 1/5 + 2/10 + 4/20 + 5/50 = 0.2 + 0.2 + 0.2 + 0.1 = **0.70**.
> RMA bound for n=4: `4(2^0.25 - 1) = 4 × 0.1892 = 0.757`.
> U = 0.70 < 0.757 → **provably schedulable** under RMA.
> Additionally, verify with response time analysis (RTA) for each task:
> R(T4): 5 + ceil(5/5)×1 + ceil(5/10)×2 + ceil(5/20)×4 = 5+1+2+4 = 12ms (iterate until convergence).
> Full RTA gives tighter schedulability guarantees than the utilization bound alone.
