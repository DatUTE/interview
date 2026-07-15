# Embedded Systems 04 — Embedded C/C++ Specifics

---

## 4.1 The `volatile` Keyword

Tells the compiler: "this variable may change at any time outside your control — never optimize it away or reorder accesses."

**When to use:**
1. **Hardware registers** — peripheral changes the value
2. **ISR-shared variables** — ISR writes, main reads (or vice versa)
3. **Busy-wait polling** — spin on a flag set by ISR
4. **MMIO** — memory-mapped peripheral registers

```c
// WRONG — compiler may optimize the loop away (it sees 'ready' never changes in this loop)
bool ready = false;
while (!ready) { }  // optimizer: "ready is always false here → infinite loop or no loop"

// CORRECT
volatile bool ready = false;
while (!ready) { }  // compiler re-reads 'ready' from memory every iteration
```

**What `volatile` does NOT do:**
- Does not provide atomicity (a 32-bit read may still be torn on 8-bit MCU)
- Does not provide memory ordering / synchronization between threads
- Is not a substitute for a mutex in RTOS contexts

For RTOS: use `atomic_` types or disable interrupts around the critical section.

---

## 4.2 Fixed-Width Types & Portability

Always use `<stdint.h>` types in embedded:

```c
#include <stdint.h>
#include <stdbool.h>

uint8_t  u8;   // exactly 8-bit unsigned
int16_t  s16;  // exactly 16-bit signed
uint32_t u32;  // exactly 32-bit unsigned
uint64_t u64;  // exactly 64-bit unsigned
uintptr_t ptr; // unsigned, wide enough for a pointer
```

Never assume `int` or `long` size — it varies by platform and compiler.

---

## 4.3 Bitfields

```c
typedef struct {
    uint8_t status  : 1;  // 1 bit
    uint8_t mode    : 3;  // 3 bits
    uint8_t channel : 4;  // 4 bits
} __attribute__((packed)) CtrlReg;
```

**Pitfalls:**
- Bit order (MSB-first vs LSB-first) is implementation-defined
- Padding between fields is implementation-defined
- Not safe for direct hardware register overlays without checking compiler output
- Prefer explicit bit manipulation for register access; use bitfields only for internal data structures

---

## 4.4 Bit Manipulation Patterns

```c
#define BIT(n)          (1U << (n))
#define SET_BIT(r, n)   ((r) |= BIT(n))
#define CLR_BIT(r, n)   ((r) &= ~BIT(n))
#define TGL_BIT(r, n)   ((r) ^= BIT(n))
#define GET_BIT(r, n)   (((r) >> (n)) & 1U)

// Extract a multi-bit field: bits [high:low]
#define FIELD_GET(r, high, low) (((r) >> (low)) & ((1U << ((high)-(low)+1)) - 1U))
// Set a multi-bit field
#define FIELD_SET(r, high, low, val) \
    ((r) = ((r) & ~(((1U<<((high)-(low)+1))-1U)<<(low))) | (((val)&((1U<<((high)-(low)+1))-1U))<<(low)))

// Practical example: configure bits [5:3] = 0b101 in a register
uint32_t reg = 0x00000000;
FIELD_SET(reg, 5, 3, 0b101);  // reg = 0x00000028
```

---

## 4.5 Memory Sections & Linker Script

**Default Sections**

| Section   | Contents                               | Location (typical)       |
|-----------|----------------------------------------|--------------------------|
| `.text`   | Machine code, ISR vectors              | Flash                    |
| `.rodata` | `const` globals, string literals, lookup tables | Flash           |
| `.data`   | Initialized globals/statics            | Flash (LMA) → RAM (VMA) |
| `.bss`    | Zero-initialized globals/statics       | RAM (zero-filled at startup) |
| `.stack`  | Call stack                             | RAM (top)                |
| `.heap`   | Dynamic allocation                     | RAM (above .bss)         |

**Why `const` strings and `const` globals are in `.rodata`, not `.text`**

The compiler puts **executable instructions** in `.text` and **read-only data** in `.rodata`. They are different ELF input sections:

```c
const char msg[] = "hello";   // → .rodata
const int table[] = {1, 2, 3}; // → .rodata

void foo(void) {
    puts("hello");            // string literal "hello" → .rodata
}
// foo()'s machine code      → .text
```

So strictly speaking, `const` strings and `const` globals belong in **`.rodata`**, not `.text`.

Why the confusion?

1. **Linker scripts often merge them into one Flash output section.** The output section may be *named* `.text`, but the linker still pulls in `*(.rodata*)` alongside `*(.text*)`. Both end up in Flash — only the section *name* in the script is `.text`.
2. **People say "text region" loosely** to mean "everything read-only in Flash" (code + constants), even though ELF distinguishes `.text` vs `.rodata`.
3. **Small constants may be embedded in instructions** as immediate operands (part of the code stream), but that is not the same as placing a global/string object in the `.text` data section.

```bash
# Verify with the toolchain
arm-none-eabi-gcc -c -o main.o main.c
arm-none-eabi-objdump -h main.o    # shows separate .text and .rodata
arm-none-eabi-nm main.o            # 't' = .text, 'r' = .rodata
```

**C++ note:** `constexpr` data may be emitted as immediates (no storage) or in `.rodata` if it needs an address. Objects with static constructors usually live in `.data`/`.bss` plus init routines in `.init_array`, not in `.rodata`.

**Linker Script Skeleton**

```ld
MEMORY {
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 512K
    SRAM  (rwx) : ORIGIN = 0x20000000, LENGTH = 128K
}

SECTIONS {
    .text : {
        KEEP(*(.isr_vector))   /* interrupt vector table — must be first */
        *(.text*)
        *(.rodata*)
    } > FLASH

    .data : {
        _data_start = .;
        *(.data*)
        _data_end = .;
    } > SRAM AT > FLASH         /* VMA=SRAM, LMA=FLASH */

    _data_load = LOADADDR(.data);  /* used by startup to copy .data */

    .bss : {
        _bss_start = .;
        *(.bss*)
        *(COMMON)
        _bss_end = .;
    } > SRAM
}
```

**Startup Code (what happens before `main`)**

```c
// startup.c — executed from reset vector
extern uint32_t _data_start, _data_end, _data_load;
extern uint32_t _bss_start, _bss_end;
extern int main(void);

void Reset_Handler(void) {
    // 1. Copy .data from Flash (LMA) to SRAM (VMA)
    uint32_t *src = &_data_load;
    uint32_t *dst = &_data_start;
    while (dst < &_data_end) *dst++ = *src++;

    // 2. Zero .bss
    dst = &_bss_start;
    while (dst < &_bss_end) *dst++ = 0;

    // 3. Optional: init FPU, enable caches, call constructors (C++)
    SystemInit();

    // 4. Jump to main
    main();

    // 5. Should never return — trap
    while (1) { }
}
```

---

## 4.6 C++ in Embedded

**Costs to Avoid**

```cpp
// RTTI — disabled with -fno-rtti
typeid(obj);           // requires typeinfo tables in flash
dynamic_cast<T*>(ptr); // requires RTTI

// Exceptions — disabled with -fno-exceptions
try { } catch (...) { } // adds unwinding tables, overhead

// Virtual dispatch — generally OK, but vtable takes RAM/flash
class Base { virtual void foo(); };  // vtable pointer per object instance
```

**Preferred C++ Patterns in Embedded**

```cpp
// CRTP for static polymorphism (zero virtual overhead)
template<typename Derived>
class Driver {
public:
    void init() { static_cast<Derived*>(this)->init_impl(); }
};

class SPI_Driver : public Driver<SPI_Driver> {
public:
    void init_impl() { /* SPI-specific init */ }
};

// constexpr for compile-time computation
constexpr uint32_t compute_psc(uint32_t clk, uint32_t freq, uint32_t arr) {
    return clk / (freq * (arr + 1)) - 1;
}
constexpr uint32_t PSC = compute_psc(72'000'000, 50, 999);  // evaluated at compile time

// std::span for buffer views (C++20, no ownership, no heap)
#include <span>
void process(std::span<const uint8_t> data) {
    for (auto byte : data) { /* ... */ }
}

// Placement new (avoid heap, construct in static buffer)
alignas(MyObject) uint8_t obj_storage[sizeof(MyObject)];
MyObject* obj = new (obj_storage) MyObject(args);
obj->~MyObject();  // explicit destructor call when done
```

**Avoiding Dynamic Memory**

```cpp
// Ring buffer with static storage
template<typename T, size_t N>
class RingBuffer {
    T buf_[N];
    size_t head_ = 0, tail_ = 0, count_ = 0;
public:
    bool push(const T& v) {
        if (count_ == N) return false;
        buf_[tail_] = v;
        tail_ = (tail_ + 1) % N;
        ++count_;
        return true;
    }
    bool pop(T& v) {
        if (count_ == 0) return false;
        v = buf_[head_];
        head_ = (head_ + 1) % N;
        --count_;
        return true;
    }
    size_t size() const { return count_; }
};
```

---

## 4.7 ISR Implementation in C and C++

### Declaring ISRs

**C: weak symbol override**

CMSIS startup files define every handler as a weak alias to `Default_Handler` (infinite loop). Your file simply defines the same name — the linker replaces the weak symbol with yours.

```c
// startup_stm32f4xx.s defines:
//   .weak USART1_IRQHandler
//   .thumb_set USART1_IRQHandler, Default_Handler

// Your file overrides it:
void USART1_IRQHandler(void) {
    if (USART1->SR & USART_SR_RXNE) {
        rx_buf[rx_head++] = (uint8_t)USART1->DR;
    }
}
```

**C++: `extern "C"` is mandatory**

C++ name-mangles every function. `USART1_IRQHandler` becomes `_Z19USART1_IRQHandlerv` — the vector table entry pointing to the C-linkage name `USART1_IRQHandler` never matches.

```cpp
// WRONG — mangled symbol never matches the vector table entry
void USART1_IRQHandler(void) { ... }      // compiled as _Z19USART1_IRQHandlerv

// CORRECT — suppress mangling for this symbol
extern "C" void USART1_IRQHandler(void) {
    // C++ code is fully legal here
    rx_queue_.push_from_isr((uint8_t)USART1->DR);
}
```

Without `extern "C"`, the ISR silently never fires — the weak default (infinite loop) runs instead. This is one of the most common C++ embedded bugs.

---

### ISR-Safe Data Sharing

**Pattern 1 — `volatile` flag (simplest, single byte/word)**

```c
volatile bool     rx_ready = false;
volatile uint8_t  rx_byte;

void USART1_IRQHandler(void) {
    rx_byte  = (uint8_t)USART1->DR;  // write data first
    rx_ready = true;                  // then set flag — main sees consistent pair
}

void main_loop(void) {
    if (rx_ready) {
        rx_ready = false;             // clear before reading — prevents double-process
        process(rx_byte);
    }
}
```

**Pattern 2 — C11 atomics (ordering guarantees)**

```c
#include <stdatomic.h>

atomic_bool        rx_ready = false;
atomic_uint_fast8_t rx_byte;

void USART1_IRQHandler(void) {
    atomic_store(&rx_byte, USART1->DR);
    atomic_store_explicit(&rx_ready, true, memory_order_release);
    // release: all prior writes visible before rx_ready=true is observed
}

void main_loop(void) {
    if (atomic_load_explicit(&rx_ready, memory_order_acquire)) {
        // acquire: guaranteed to see rx_byte written before the release
        atomic_store(&rx_ready, false);
        process(atomic_load(&rx_byte));
    }
}
```

**Pattern 3 — disable interrupts for multi-word structs**

```c
typedef struct { uint32_t timestamp; uint16_t adc_val; } Sample;
volatile Sample latest;

void TIM2_IRQHandler(void) {
    latest.timestamp = DWT->CYCCNT;
    latest.adc_val   = (uint16_t)ADC1->DR;
}

Sample read_sample_safe(void) {
    Sample s;
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    s = latest;           // atomic copy under interrupt lock
    __set_PRIMASK(primask);
    return s;
}
```

Using `__get_PRIMASK`/`__set_PRIMASK` instead of `__enable_irq()` preserves the original interrupt state (important if the caller itself might be inside a critical section).

**Pattern 4 — lockless SPSC ring buffer (ISR producer, task consumer)**

```c
#define RING_SZ 256   // power-of-2 for mask trick

typedef struct {
    volatile uint32_t head;      // written only by ISR
    volatile uint32_t tail;      // written only by consumer
    uint8_t           buf[RING_SZ];
} RingISR;

static RingISR uart_ring;

// Called from ISR
bool ring_push_isr(RingISR *r, uint8_t val) {
    uint32_t next = (r->head + 1) & (RING_SZ - 1);
    if (next == r->tail) return false;   // full
    r->buf[r->head] = val;
    __DMB();                             // data visible before head advances
    r->head = next;
    return true;
}

// Called from task/main
bool ring_pop(RingISR *r, uint8_t *val) {
    if (r->tail == r->head) return false; // empty
    *val = r->buf[r->tail];
    __DMB();
    r->tail = (r->tail + 1) & (RING_SZ - 1);
    return true;
}
```

No mutex needed: `head` is exclusively owned by the ISR, `tail` by the consumer.

---

### ISR Reentrancy

An ISR is **reentrant** if it is safe for a higher-priority ISR to interrupt it mid-execution and call the same code path.

**Non-reentrant patterns to avoid in ISRs:**

```c
// BAD: static local — shared state between all invocations
void ADC_IRQHandler(void) {
    static uint32_t count = 0;  // if two ADC IRQs nest, count is corrupted
    count++;
}

// BAD: non-reentrant library calls
void UART_IRQHandler(void) {
    printf("rx\n");   // printf uses a static buffer — not reentrant
    malloc(32);       // heap allocator has global state — not reentrant
}

// BAD: modifying global without lock
void IRQHandler_A(void) { shared_counter++; }  // read-modify-write not atomic
```

**Rule on Cortex-M**: same-priority IRQs never preempt each other. A higher-priority IRQ *can* preempt a lower one. Any function callable from both must be reentrant or protected.

---

### Stack Considerations: MSP vs PSP

Cortex-M has two stack pointers:

| Pointer | Used in | Configured by |
|---------|---------|---------------|
| **MSP** (Main Stack Pointer) | Handler mode (all ISRs), Thread mode without RTOS | `__set_MSP()`, linker script `_stack_end` |
| **PSP** (Process Stack Pointer) | Thread mode with RTOS (each task has its own PSP) | RTOS sets PSP on context switch |

**Exception entry with PSP active (RTOS task running):**

```
1. Hardware pushes exception frame {R0-R3, R12, LR, PC, xPSR} onto PSP stack
2. CPU switches to Handler mode → uses MSP
3. ISR executes using MSP
4. On return: hardware unstacks from PSP, resumes task
```

**Implication:** ISR local variables and nested exception frames all consume MSP. With 8 nesting levels, each adding a 32-byte exception frame + 100 bytes of locals, the MSP needs ~1KB minimum. Size the main stack accordingly in the linker script.

---

### Tail-Chaining and Late Arrival

**Tail-chaining:** when ISR_A completes and IRQ_B is already pending, the CPU skips the unstack+restack cycle (saves ~12 cycles) and fetches ISR_B's vector immediately. The exception frame from ISR_A is reused on the stack.

```
Normal:          ISR_A exit → [unstack 12c] → [stack 12c] → ISR_B
Tail-chain:      ISR_A exit →                              → ISR_B
```

**Late arrival:** if IRQ_B (higher priority) becomes pending during the stacking phase for IRQ_A (lower priority), the CPU preempts the stacking, handles IRQ_B first, then tail-chains to IRQ_A.

```
IRQ_A pending → CPU starts stacking → IRQ_B arrives (higher priority)
→ CPU preempts stacking → IRQ_B ISR runs → tail-chains to IRQ_A
```

These are automatic hardware optimizations; no software changes needed to benefit.

---

### Running ISR from RAM

MCUs above ~24 MHz typically add 1–5 flash wait states. A cache miss during ISR instruction fetch adds latency. Place timing-critical ISRs in SRAM for deterministic execution:

```c
// Place in .RamFunc section (must be copied from flash at startup like .data)
__attribute__((section(".RamFunc")))
void TIM1_IRQHandler(void) {
    // executes from SRAM — zero flash wait state stall
    GPIOA->BSRR = GPIO_PIN_5;       // fast GPIO toggle
}
```

Linker script addition:

```ld
.RamFunc : {
    *(.RamFunc*)
} > SRAM AT > FLASH    /* copied to RAM by startup code */
```

---

### FPU Lazy Stacking

Cortex-M4F/M7 with FPU enabled: if an ISR uses floating-point, the hardware must save/restore S0–S15 and FPSCR — adding 72 bytes to the exception frame.

**Lazy stacking (default, FPCCR.LSPEN=1):** the extra space is reserved on entry but FPU registers are saved only if the ISR actually executes an FP instruction. ISRs that don't use FP incur zero extra overhead.

```c
// Check lazy stacking is enabled (should be default after reset):
assert(SCB->FPCCR & FPU_FPCCR_LSPEN_Msk);  // bit 30

// ISR using float — compiler may emit VMOV/VADD instructions:
__attribute__((section(".RamFunc")))
void ADC_IRQHandler(void) {
    float val = (float)ADC1->DR * (3.3f / 4095.0f);  // triggers FPU lazy save
    latest_voltage = val;
}
```

**Stack budget with FPU ISR:** standard exception frame = 32 bytes. Extended (with FPU) = 104 bytes. Multiply by max nesting depth and add task stack for worst-case MSP sizing.

---

### ISR-to-Task Notification (FreeRTOS)

```c
static TaskHandle_t g_uart_task;

void USART1_IRQHandler(void) {
    BaseType_t woken = pdFALSE;
    uint8_t byte = (uint8_t)USART1->DR;

    // Option A: direct-to-task notification (single value, faster than semaphore)
    xTaskNotifyFromISR(g_uart_task, byte, eSetValueWithOverwrite, &woken);

    // Option B: queue (multiple pending bytes)
    xQueueSendFromISR(g_uart_queue, &byte, &woken);

    portYIELD_FROM_ISR(woken);   // if a higher-priority task was woken, switch now
}

void uart_task(void *pv) {
    uint32_t byte;
    while (1) {
        // Blocks until ISR sends notification
        xTaskNotifyWait(0, UINT32_MAX, &byte, portMAX_DELAY);
        process((uint8_t)byte);
    }
}
```

`portYIELD_FROM_ISR` is critical: without it, the woken high-priority task waits until the next SysTick tick — adding up to 1ms latency.

---

### ISR Best Practices Checklist

| Rule | Why |
|------|-----|
| `extern "C"` for C++ handlers | C++ name mangling breaks the vector table lookup |
| Keep ISR body < 1µs where possible | Blocks all equal/lower priority interrupts |
| Never call `HAL_Delay()` in ISR | SysTick may be masked; causes infinite spin |
| Never call blocking FreeRTOS API | Use `FromISR` variants only |
| Use `volatile` or atomics on shared data | Compiler may cache ISR-written values in registers |
| Clear IRQ flag **before** processing | For level-triggered: prevents re-entry race; clear at top |
| Avoid `float` in ISR if stack is limited | FPU lazy stacking adds 72 bytes to exception frame |
| Save/restore PRIMASK, not `__enable_irq()` | Preserves nesting: if IRQs were already masked, don't unmask |
| Call `portYIELD_FROM_ISR(woken)` | Ensures woken high-priority task runs immediately, not at next tick |

---

## Interview Questions

### Easy

**Q: What does the `volatile` keyword tell the compiler? Give two examples of when you need it in embedded code.**

> Tells the compiler: "this variable may change at any time outside my control — never cache it in a register, never optimize away reads/writes, never reorder accesses to it." Example 1: hardware register pointer — `volatile uint32_t *reg = (volatile uint32_t*)0x40000000;`. Example 2: ISR-shared flag — `volatile bool data_ready = false;` — without `volatile`, the compiler may hoist the read out of a polling loop because it sees no write in the loop body.

**Q: Why should you use `uint32_t` instead of `unsigned int` in embedded code?**

> `unsigned int` size is platform-defined: 16 bits on some 8/16-bit MCUs, 32 bits on 32-bit MCUs. Code using `unsigned int` to hold a 32-bit register value silently breaks when ported. `uint32_t` from `<stdint.h>` is always exactly 32 bits on every platform that supports it. Use fixed-width types everywhere peripheral registers, protocol fields, or binary data layouts are involved.

**Q: What are the `.text`, `.data`, and `.bss` sections? Where does each live in memory?**

> `.text`: compiled **machine code** — lives in Flash. `.rodata`: **read-only data** — `const` globals, string literals, lookup tables — also lives in Flash. Linker scripts often merge `*(.text*)` and `*(.rodata*)` into one Flash output section, which causes beginners to think constants are "in `.text`", but the compiler emits them as separate `.rodata` input sections. `.data`: initialized global/static variables (e.g., `int x = 5;`) — stored in Flash at load time (LMA), copied to RAM at startup (VMA). `.bss`: zero-initialized globals/statics — lives only in RAM; startup code zeroes this region. The linker script defines these sections and the startup code performs the copy/zero.

---

### Mid

**Q: What is `const volatile` and when would you use it in embedded?**

> `const volatile` means: the compiler cannot write to this variable, but it may change outside the compiler's control (so reads must not be cached/optimized). Use case: a read-only hardware status register — you never write to it in software, but the peripheral updates it. Example: `const volatile uint32_t* STATUS = (const volatile uint32_t*)0x40000004;`. Without `const`: compiler won't catch accidental writes. Without `volatile`: compiler may optimize repeated reads into a single cached read.

**Q: A global variable is initialized to `5` in your C code. Trace exactly how it gets that value at MCU boot.**

> 1. The linker places the value `5` in the Flash image at the LMA (Load Memory Address) of the `.data` section. 2. The startup code (`Reset_Handler`) runs before `main()`. It reads from `_data_load` (flash address) and writes to `_data_start..._data_end` (RAM address) in a copy loop. 3. After the copy, the RAM location holds `5`. 4. If the variable were uninitialized (`int x;` at file scope), it would go into `.bss`, and startup zeros it instead.

**Q: Why is using C bitfields for hardware register overlays dangerous?**

> C standard leaves several things implementation-defined for bitfields: (1) **Bit allocation order** — whether bit 0 is the LSB or MSB is compiler-specific. (2) **Padding** — compiler may insert padding bits between fields. (3) **Storage unit** — which underlying integer type is used. Accessing a peripheral register through a bitfield struct may read/write different bits than intended, depending on the compiler and target. Prefer explicit bit manipulation macros (`reg |= BIT(3)`) for register access. Use bitfields only for internal data structures where portability to exact bit positions is not required.

---

### Hard

**Q: Your firmware binary grows by 4 KB after adding a single `printf("%d\n", val)` call. Explain why and how to fix it without removing debug output.**

> `printf` pulls in the entire newlib/newlib-nano formatted I/O library: integer formatting, floating-point support (if `%f` is used), buffer management — several KB of code. Fixes: (1) Use **newlib-nano** (`-specs=nano.specs`) — reduces printf to ~1 KB. (2) Write a custom **lightweight printf** that handles only `%d`, `%x`, `%s` — 200–400 bytes. (3) Use **ITM/SWO trace** (`ITM_SendChar`) combined with a host-side decoder — zero library overhead. (4) Replace with a **ring buffer + deferred UART output** using only `putchar`. Always check the map file before and after adding large library calls.

**Q: Write a `constexpr` check that fails compilation if a computed timer prescaler exceeds 16 bits.**

> ```cpp
> constexpr uint32_t compute_psc(uint32_t clk, uint32_t freq, uint32_t arr) {
>     return clk / (freq * (arr + 1)) - 1;
> }
> constexpr uint32_t PSC = compute_psc(72'000'000, 50, 999);
> static_assert(PSC <= 0xFFFF, "PSC value exceeds 16-bit timer register range");
> ```
> `static_assert` evaluates at compile time. If `PSC > 65535`, compilation fails with the message. This is far better than a runtime check that only triggers if the specific frequency is tested.

**Q: You're using CRTP to implement a driver interface. A derived class's `init_impl()` calls a virtual function in the base class. Why is this a problem, and how do you fix it?**

> During `Driver<Derived>::init()`, which calls `static_cast<Derived*>(this)->init_impl()`, the object is fully constructed as `Derived`. However, if `init_impl()` then calls a virtual function through a base pointer, the call dispatches through the vtable — introducing the virtual dispatch overhead that CRTP was meant to eliminate. Worse, if the virtual call is made during the base constructor (before the vtable is fully initialized), it dispatches to the base version, not the derived version. **Fix**: remove virtual functions from the CRTP hierarchy entirely and use only CRTP static dispatch. If you need runtime polymorphism in addition to CRTP, separate the two concerns: use CRTP for the hot path and a separate virtual interface (`IDevice`) for runtime selection.
```
