# Embedded Systems 05 — Memory Management

---

## 5.1 Stack vs Heap Tradeoffs

**Stack in Embedded**

- Fixed size determined at link time (or configured in RTOS task)
- Grows downward (ARM convention)
- Overflow: no OS guard page → silently corrupts adjacent memory (BSS or heap)
- Detection: stack canary pattern, MPU stack limit register (Cortex-M33+)

```c
// Stack overflow detection via watermark pattern
void fill_stack_with_pattern(void) {
    // Called at startup — fills unused stack with magic value
    extern uint32_t _stack_start;
    uint32_t *p = &_stack_start;
    while (p < (uint32_t*)__get_MSP() - 64) *p++ = 0xDEADBEEF;
}

uint32_t measure_stack_highwater(void) {
    extern uint32_t _stack_start, _stack_end;
    uint32_t *p = &_stack_start;
    while (*p == 0xDEADBEEF) p++;
    return (uint32_t)(&_stack_end) - (uint32_t)p;  // bytes used
}
```

**Heap in Embedded**

Problems with `malloc`/`free`:
- **Fragmentation**: repeated alloc/free leaves unusable holes
- **Non-deterministic**: allocation time varies based on heap state
- **Memory leaks**: no GC — easy to forget `free`
- **Thread safety**: standard `malloc` not reentrant (needs lock)

**Memory Pool Allocator**

Fixed-size blocks: no fragmentation, O(1) alloc/free, deterministic.

```c
#define POOL_BLOCK_SIZE  64
#define POOL_BLOCKS      32

typedef struct Block { struct Block *next; } Block;

static uint8_t pool_mem[POOL_BLOCK_SIZE * POOL_BLOCKS];
static Block   *pool_head;

void pool_init(void) {
    pool_head = NULL;
    for (int i = 0; i < POOL_BLOCKS; i++) {
        Block *b = (Block*)(pool_mem + i * POOL_BLOCK_SIZE);
        b->next = pool_head;
        pool_head = b;
    }
}

void* pool_alloc(void) {
    if (!pool_head) return NULL;
    Block *b = pool_head; pool_head = b->next;
    return b;
}

void pool_free(void *ptr) {
    Block *b = ptr; b->next = pool_head; pool_head = b;
}
```

---

## 5.2 MPU (Memory Protection Unit)

Cortex-M3/M4/M7 include an optional MPU. Up to 8 or 16 regions, each with:
- Base address (must be power-of-2 aligned)
- Size (must be power-of-2, ≥32 bytes)
- Access permissions: no access, read-only, read-write
- Execute Never (XN): prevents code execution from region (useful for data regions)
- Cacheable / shareable / bufferable attributes

**Use cases:**
- Mark stack region as no-execute + guard page → detect stack overflow via MemManage fault
- Mark Flash as read-only → detect unintended writes
- Separate tasks' RAM in RTOS → inter-task memory protection

```c
// Cortex-M CMSIS MPU setup
ARM_MPU_SetRegion(
    ARM_MPU_RBAR(0, 0x20000000),          // region 0, base = SRAM start
    ARM_MPU_RASR(0,                        // execute never = 0 (allowed)
                 ARM_MPU_AP_FULL,          // full access
                 0, 0, 1, 0,              // TEX, S, C, B
                 ARM_MPU_REGION_SIZE_128KB)
);
ARM_MPU_Enable(MPU_CTRL_PRIVDEFENA_Msk);
```

---

## 5.3 Cache Management (Cortex-M7)

Cortex-M7 has 4-way set-associative I-cache and D-cache (up to 64KB each).

**Cache Coherency Problem**

```
Scenario:
1. CPU writes data to buf[] → goes to D-cache, not yet in RAM
2. You start DMA transfer FROM buf[] → DMA reads stale RAM data!

Fix: SCB_CleanDCache() before DMA reads from memory

Opposite scenario:
1. DMA writes result to buf[] → updates RAM
2. CPU reads buf[] → gets stale cached value!

Fix: SCB_InvalidateDCache() before CPU reads DMA destination
```

**Cache Operations**
- `SCB_EnableDCache()` / `SCB_DisableDCache()`
- `SCB_CleanDCache_by_Addr(addr, size)` — flush dirty cache lines to RAM
- `SCB_InvalidateDCache_by_Addr(addr, size)` — mark cache lines invalid
- `SCB_CleanInvalidateDCache()` — both operations globally

**Alternative: Place DMA buffers in non-cached region**

Mark a RAM region as non-cacheable in MPU, place DMA buffers there. Eliminates need for explicit cache management at the cost of slower CPU access.

```c
// Mark buffer as non-cacheable via linker section + MPU
__attribute__((section(".DMA_BUFFER"))) uint8_t rx_buf[1024];

---

## Interview Questions

### Easy

**Q: What is the difference between stack and heap in an embedded system?**

> **Stack**: statically sized at link time, grows/shrinks automatically with function calls, LIFO, holds local variables and return addresses. Fast, deterministic. **Heap**: dynamically allocated at runtime via `malloc`/`free`, arbitrary size and lifetime, but requires a heap manager. In embedded: stack is always present; heap is optional and often avoided due to fragmentation and non-deterministic allocation time.

**Q: Why is `malloc` often avoided in embedded / safety-critical systems?**

> (1) **Fragmentation**: repeated alloc/free cycles leave unusable holes — system eventually fails to allocate even when total free memory is sufficient. (2) **Non-deterministic**: allocation time varies by heap state — violates WCET analysis for real-time systems. (3) **Memory leaks**: no GC — one missed `free` grows memory usage permanently. (4) **Thread safety**: standard `malloc` is not reentrant without a lock. (5) **MISRA C Rule 21.3**: explicitly prohibits `malloc`/`free` in safety-critical code.

**Q: What is a memory pool allocator and what problem does it solve?**

> A memory pool pre-allocates a fixed number of fixed-size blocks. `alloc` pops a block from the free list (O(1)). `free` pushes it back (O(1)). Solves: no fragmentation (all blocks are the same size), deterministic allocation time, bounded memory usage. Trade-off: must know max object size and count at design time; wastes memory if objects are much smaller than the block size.

---

### Mid

**Q: How do you detect a stack overflow on bare-metal Cortex-M without an OS?**

> **Watermark pattern**: at startup, fill the stack region from bottom to current SP with a magic value (`0xDEADBEEF`). After running, scan from the bottom until you find a non-magic word — the distance consumed is the high-water mark. Limitation: only detects after the fact; a brief spike might corrupt memory without being detected. **Better**: configure the MPU to mark the bottom N bytes of the stack as no-access. A stack overflow triggers a MemManage fault instead of silently corrupting BSS/heap.

**Q: What are MPU regions and how do they help catch memory bugs?**

> The MPU divides the address space into up to 8 (or 16) regions, each with access permissions (read-only, read-write, no-access) and attributes (cacheable, executable, shareable). Bugs it catches: (1) **Stack overflow** — mark the guard zone at the bottom of the stack as no-access → MemManage fault on overflow. (2) **Code executing from RAM** — mark data regions as Execute Never (XN) → UsageFault if code jumps into data. (3) **Inter-task protection** — in an RTOS, give each task its own MPU region and switch regions on context switch.

**Q: What is the Execute Never (XN) MPU attribute and when do you use it?**

> XN marks a memory region as non-executable — any attempt to execute code from that region causes a UsageFault (IACCVIOL). Use on: all RAM regions that should only hold data (stack, heap, BSS) to prevent ROP/code-injection attacks and catch NULL pointer function calls. Leave XN off only for Flash regions and any RAM overlay used for self-modifying code (rare in embedded).

---

### Hard

**Q: On Cortex-M7 with D-cache enabled, DMA writes to a buffer but the CPU reads stale data. Explain the root cause and give two solutions.**

> Root cause: Cortex-M7 D-cache is write-allocate and write-back. When the CPU previously read `rx_buf`, the cache loaded that cache line. DMA writes new data to RAM directly, bypassing the cache. The CPU reads `rx_buf` and gets the stale cached value — cache and RAM are incoherent.
> Solution 1: **Cache invalidation** — before reading DMA destination buffer, call `SCB_InvalidateDCache_by_Addr(rx_buf, size)`. This marks the cache lines as invalid, forcing the next CPU read to go to RAM.
> Solution 2: **Non-cached region** — configure an MPU region covering `rx_buf` as non-cacheable (TEX=1, C=0, B=0 in RASR). Place all DMA buffers in this region. No manual cache management needed, but every CPU access goes directly to RAM (slower).

**Q: Design a fixed-size memory pool for an RTOS with 8 tasks, each needing up to 3 message objects of 32 bytes. How do you size and protect the pool in a multi-task environment?**

> Pool needs: 8 tasks × 3 messages × 32 bytes = **768 bytes**, allocate 24 blocks of 32 bytes. Thread safety: `pool_alloc`/`pool_free` must be atomic. In FreeRTOS: wrap with a mutex, or use `taskENTER_CRITICAL`/`taskEXIT_CRITICAL` (only if ISRs don't allocate). For ISR-safe pools: use an atomic compare-exchange on the free list pointer (lock-free stack). Also configure the MPU to make the pool memory `no-execute` and consider adding a canary word at the end of each block to detect overflow into adjacent blocks.

**Q: Your system has been running for 48 hours and `pool_alloc` returns NULL. The pool has 24 blocks. Describe how you diagnose the leak.**

> 1. Add a **usage counter**: increment on `alloc`, decrement on `free`. Log a warning when counter > 20. 2. Add a **caller tag**: store the return address (`__builtin_return_address(0)`) in each block's header on alloc. Dump all in-use blocks and their allocator addresses when the pool is exhausted. 3. Cross-reference with the message queue drain logic — are consumers always calling `pool_free` after processing? 4. Check error paths — if a task exits early due to an error, does it still call `pool_free`? This is the most common cause: the `free` call is skipped on exception paths.
```
