# Segmentation Fault in C++

> **Related:** [production_debugging.md](production_debugging.md) — on-call playbook (deadlock, race, hang, sanitizer matrix)

---

## What Is a Segmentation Fault?

A **segmentation fault** (`SIGSEGV`) is a signal sent by the OS to a process when it attempts to access a memory location that is not permitted — either not mapped into the process's virtual address space, or mapped with the wrong permissions.

The **MMU (Memory Management Unit)** hardware enforces memory protection on every access. When a violation is detected, the CPU raises a **page fault exception**, the OS kernel inspects it, and if it cannot be resolved (no lazy allocation, not a copy-on-write page, etc.), it delivers `SIGSEGV` to the process.

```
Process attempts memory access
           │
           ▼
    MMU checks page table
           │
   ┌───────┴────────────────────────┐
   │                                │
Page is mapped                 Page not mapped
   │                                └──► OS checks if it can resolve
   │                                         │
   ├─ correct permission?                    ├─ lazy alloc / COW? → minor fault, map it, continue
   │   └─ access proceeds ✅                 └─ truly unmapped?   → SIGSEGV 💥
   │
   └─ wrong permission?
       ├─ write to read-only?  → SIGSEGV 💥
       └─ exec non-exec page?  → SIGSEGV 💥
```

**Default action**: terminate the process and (if enabled) produce a core dump.

---

## Process Memory Map — Which Regions Are Valid

Understanding which regions are mapped helps predict where a segfault will occur:

```
High address
┌─────────────────────────────┐
│        Kernel Space         │  No access from user code → SIGSEGV
├─────────────────────────────┤
│           Stack             │  Read + Write (grows down)
│             ↓               │
│        [guard page]         │  Unmapped → stack overflow → SIGSEGV
│             ↑               │
│           Heap              │  Read + Write (grows up)
├─────────────────────────────┤
│       Shared Libraries      │  Read + Execute (.text), Read (.rodata)
├─────────────────────────────┤
│     BSS / Data / Text       │  Text = Read+Exec, Data/BSS = Read+Write
│    .rodata = Read-Only      │  Write to .rodata → SIGSEGV
└─────────────────────────────┘
Low address (0x0 — unmapped)
```

---

## Cause 1: Null Pointer Dereference

Address `0x0` is **never mapped**. The OS deliberately leaves it unmapped so null dereferences are caught immediately.

```cpp
// Direct null dereference
int* p = nullptr;
*p = 5;               // → SIGSEGV at address 0x0

// Null pointer + offset — crash at small non-zero address
struct Node { int val; Node* next; };
Node* n = nullptr;
int x = n->val;       // → SIGSEGV at 0x0  (offsetof val == 0)
int y = n->next->val; // → SIGSEGV at 0x8  (offsetof next == 8 on 64-bit)

// Through a member function call
std::string* s = nullptr;
size_t len = s->size(); // → SIGSEGV — 'this' pointer is null inside size()

// Hidden null: uninitialized pointer
int* p;               // indeterminate value (not nullptr — could be anything)
*p = 1;               // → UB, likely SIGSEGV or silent corruption
```

**Diagnosis clue**: crash address is `0x0` or a small offset (< a few hundred bytes).

---

## Cause 2: Use-After-Free (Dangling Pointer)

After `delete`/`free()`, the memory is returned to the allocator but **not unmapped** in most cases. The pointer becomes dangling — accessing it is undefined behavior and may:
- Return stale data (chunk not reused yet)
- Return garbage (chunk reused, allocator overwrote it)
- Crash with SIGSEGV (chunk returned to OS via `munmap`)

```cpp
// Basic use-after-free
int* p = new int(42);
delete p;
*p = 10;              // UB — may crash or corrupt silently

// Dangling reference to a local variable
int& get_ref() {
    int x = 5;
    return x;         // x is destroyed on return
}
int& r = get_ref();
r = 10;               // → SIGSEGV — stack frame is gone

// Dangling after container reallocation
std::vector<int> v = {1, 2, 3};
int& ref = v[0];      // points into v's internal buffer
v.push_back(4);       // may reallocate → old buffer freed
ref = 99;             // → SIGSEGV or corruption

// Dangling in a callback
std::function<void()> cb;
{
    int local = 42;
    cb = [&local]() { local = 0; }; // captures local by reference
}                     // local destroyed here
cb();                 // → SIGSEGV — local is gone
```

**Diagnosis clue**: crash address looks valid but is in a freed region. Non-deterministic — depends on allocator reuse timing and ASLR.

---

## Cause 3: Stack Overflow

The stack has a fixed size limit (typically **8 MB** on Linux, **1–4 MB** on Windows). The OS places an **unmapped guard page** just below the lowest stack page. Touching it triggers `SIGSEGV`.

```cpp
// Infinite recursion
void recurse() {
    recurse();        // each call: ~100-200 bytes of frame pushed
}                     // → SIGSEGV after ~40,000–80,000 frames

// Huge local array
void bad() {
    char buf[9 * 1024 * 1024];  // 9 MB > 8 MB stack limit → immediate SIGSEGV
    buf[0] = 0;                 // first touch hits guard page
}

// Subtle: mutually recursive functions
bool is_even(int n) { return n == 0 ? true  : is_odd(n - 1); }
bool is_odd (int n) { return n == 0 ? false : is_even(n - 1); }
is_even(1000000);   // → stack overflow

// Fix large buffers: move to heap
void good() {
    std::vector<char> buf(9 * 1024 * 1024);  // heap — no stack limit
    buf[0] = 0;
}
```

**Diagnosis clue**: `bt` in gdb shows hundreds or thousands of identical frames. Crash address is just below the stack's lowest mapped page.

```bash
ulimit -s           # show stack limit (KB), default 8192
ulimit -s unlimited # temporarily remove limit
```

---

## Cause 4: Out-of-Bounds Access

Accessing memory outside the bounds of an allocated object. May crash immediately or silently corrupt adjacent memory (causing a crash much later).

```cpp
// Raw array — no bounds checking
int arr[5] = {0};
arr[5]  = 1;   // one past end — UB, may hit adjacent stack variable or guard
arr[-1] = 1;   // negative index — goes into some other memory region
arr[1000] = 1; // far OOB — likely SIGSEGV

// std::vector::operator[] — no bounds check (unlike .at())
std::vector<int> v(5);
v[10]  = 1;    // UB — may crash or corrupt allocator metadata
v[-1]  = 1;    // UB
v.at(10);      // throws std::out_of_range ← safe alternative

// C-string overrun
char buf[8];
strcpy(buf, "this is definitely more than 8 bytes");
// corrupts adjacent stack memory → crash at unpredictable later point

// Stack buffer overflow → return address corruption → crash on return
void foo() {
    char buf[8];
    scanf("%s", buf);  // no length limit → overflow → corrupt return address
}                      // → SIGSEGV when foo returns (jumping to garbage address)
```

**Diagnosis clue**: crash address is random or in adjacent memory. ASan gives exact OOB report.

---

## Cause 5: Writing to Read-Only Memory

Certain segments are mapped **read-only** by the OS. Any write triggers SIGSEGV.

```cpp
// String literals live in .rodata — read+no-write
char* s = "hello";    // implicit cast from const char* (C-style, dangerous)
s[0] = 'H';           // → SIGSEGV — .rodata page has no write permission

// Correct approach 1: stack copy
char s[] = "hello";   // copies literal to stack — writable
s[0] = 'H';           // OK

// Correct approach 2: enforce const-ness
const char* s = "hello";
s[0] = 'H';           // compile error — caught at compile time ✅

// Writing to .text (function code) — e.g. JIT patching without mprotect
void foo() {}
char* code = reinterpret_cast<char*>(reinterpret_cast<uintptr_t>(foo));
*code = 0x90;         // → SIGSEGV — .text is read+execute, not writable

// Fix for JIT: use mprotect() to add write permission first
mprotect(page_aligned_ptr, page_size, PROT_READ | PROT_WRITE | PROT_EXEC);
*code = 0x90;         // now OK
mprotect(page_aligned_ptr, page_size, PROT_READ | PROT_EXEC);  // restore
```

**Diagnosis clue**: crash address is in the `.rodata` or `.text` range (low virtual address, same area as the executable itself).

---

## Cause 6: Double Free

Calling `delete`/`free()` on the same pointer twice corrupts the allocator's internal metadata, which typically triggers a crash on the second free or a subsequent allocation.

```cpp
int* p = new int(5);
delete p;
delete p;             // → heap corruption → SIGSEGV or abort()
                      // glibc: "double free or corruption" + abort
                      // Under ASan: "double-free" with exact stack trace

// Subtle double free via two owners
int* raw = new int(5);
std::shared_ptr<int> sp1(raw);
std::shared_ptr<int> sp2(raw); // two shared_ptrs, each thinks it owns raw
// sp2 destroyed → delete raw
// sp1 destroyed → delete raw again → double free

// Fix: use make_shared
auto sp1 = std::make_shared<int>(5);
auto sp2 = sp1;   // shared ownership — reference counted, one delete
```

**Diagnosis clue**: glibc prints a message before aborting. ASan reports "double-free" with the original allocation and both free call stacks.

---

## Cause 7: Corrupt vtable / Function Pointer

Jumping to an invalid address through a corrupt function pointer or vtable pointer.

```cpp
// Null function pointer
using Fn = void(*)();
Fn fp = nullptr;
fp();             // → SIGSEGV at 0x0

// Garbage function pointer
Fn fp2 = reinterpret_cast<Fn>(0xdeadbeef);
fp2();            // → SIGSEGV at 0xdeadbeef

// Corrupt vptr — zeroing an object's vtable pointer
struct Base { virtual void foo() { puts("foo"); } };
Base* b = new Base();
memset(b, 0, sizeof(*b));  // zero out vptr
b->foo();                  // → SIGSEGV — vtable pointer is null, dereference 0x0

// Heap overflow into adjacent object's vptr
struct Victim { virtual void bar() {} };
char* buf = new char[8];
Victim* v = new Victim();
memcpy(buf, "AAAAAAAABBBBBBBB", 16);  // overflows into v's vptr
v->bar();  // → SIGSEGV — vptr is now "BBBBBBBB"
```

**Diagnosis clue**: crash inside a virtual dispatch, crash address looks like text (`0x4141414141414141` = "AAAAAAAA") or a small offset from null. Classic sign of memory corruption bug upstream.

---

## Cause 8: Thread-Related Segfaults

```cpp
// 1. Thread stack overflow — each thread gets its own stack
std::thread t([]() {
    void recurse_in_thread(); recurse_in_thread(); // → SIGSEGV in thread
});

// 2. Dangling reference to a destroyed thread's local
int* dangling;
{
    std::thread t([&]() {
        int local = 42;
        dangling = &local;   // stack variable of thread t
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    });
    t.join();                // t's stack torn down here
}
*dangling = 1;              // → SIGSEGV — thread stack is gone

// 3. Data race causing a corrupted pointer
std::string* shared = new std::string("hello");
std::thread writer([&]() { delete shared; shared = nullptr; });
std::thread reader([&]() { shared->size(); }); // may see deleted ptr → SIGSEGV
writer.join(); reader.join();

// Fix: protect shared state
std::mutex mtx;
// or use std::atomic<std::string*>
```

---

## Cause 9: Misaligned Access (SIGBUS on Some Architectures)

On **x86/x64** misaligned access is slow but works. On **ARM, MIPS, SPARC** it raises `SIGBUS` (similar to SIGSEGV — process is terminated).

```cpp
#pragma pack(1)
struct Packed { char a; int b; };   // b at offset 1 — misaligned

Packed p;
int* ptr = &p.b;
*ptr = 42;   // x86: works (slow)
             // ARM: SIGBUS — bus error

// Also triggered by casting
char buf[8];
int* ip = reinterpret_cast<int*>(buf + 1);  // misaligned int*
*ip = 0;     // SIGBUS on strict-alignment hardware
```

---

## SIGSEGV vs SIGBUS vs SIGABRT

| Signal | Trigger | Example |
|---|---|---|
| `SIGSEGV` | Unmapped address or wrong permission | Null deref, write to `.rodata`, stack overflow |
| `SIGBUS` | Misaligned access (architecture-dependent) | Unaligned int* dereference on ARM |
| `SIGABRT` | `abort()` called explicitly | `assert()` failure, `double free` detected by glibc, `std::terminate` |
| `SIGILL` | Illegal CPU instruction | Corrupt code pointer jumped to non-instruction bytes |

---

## Quick Reference — Crash Address Tells You the Cause

| Crash address | Likely cause |
|---|---|
| `0x0` or small offset (< 0x1000) | Null pointer dereference |
| Plausible heap address, random | Use-after-free |
| Just below stack pointer | Stack overflow (guard page hit) |
| In `.text`/`.rodata` range | Write to read-only memory |
| `0xdeadbeef`, `0xcdcdcdcd`, etc. | Debug fill pattern or garbage pointer |
| `0xffffffffffff...` | Negative array index wrapped |
| Inside a `vtable` | Corrupt vptr or double-free of object |

---

## How to Diagnose

### 1. AddressSanitizer (Best First Step)

```bash
g++ -fsanitize=address -fsanitize=undefined -g -O1 program.cpp -o program
./program

# Output example:
# ==12345==ERROR: AddressSanitizer: heap-use-after-free on address 0x602000000010
# READ of size 4 at 0x602000000010 thread T0
#     #0 0x401234 in main program.cpp:15
#     #1 0x7f... in __libc_start_main
#
# 0x602000000010 was freed by thread T0 here:
#     #0 0x... in operator delete(void*)
#     #1 0x401200 in main program.cpp:12
```

Catches: use-after-free, heap/stack buffer overflow, double-free, use of uninitialized memory.

### 2. gdb + Core Dump

```bash
# Enable core dumps
ulimit -c unlimited

# Run until crash
./program
# Produces: core (or core.PID)

# Load in gdb
gdb ./program core

# Key commands
(gdb) bt                   # full backtrace at crash point
(gdb) bt full              # backtrace with local variables
(gdb) frame 3              # switch to frame 3
(gdb) info locals          # variables in current frame
(gdb) info registers       # register values (rip = instruction pointer at crash)
(gdb) x/10x $rsp           # examine 10 words at stack pointer
(gdb) x/s 0x402010         # examine memory at address as string
(gdb) p *ptr               # print dereferenced pointer
(gdb) watch ptr            # watchpoint — break when ptr changes
```

### 3. Valgrind

```bash
valgrind --leak-check=full \
         --show-leak-kinds=all \
         --track-origins=yes \
         --verbose \
         ./program

# Catches: use-after-free, uninitialized reads, invalid frees, leaks
# ~10-30x slower than native — best for correctness testing
```

### 4. Compile-Time Hardening

```bash
# Stack protector — detects stack-based buffer overflows at runtime
g++ -fstack-protector-strong program.cpp

# Fortify — adds bounds checks to common string/memory functions
g++ -D_FORTIFY_SOURCE=2 -O2 program.cpp

# Control flow integrity (Clang) — prevents indirect call to invalid address
clang++ -fsanitize=cfi -flto -fvisibility=hidden program.cpp
```

### 5. `/proc/PID/maps` — Inspect Process Memory Map

```bash
# While process is running, see all mapped regions
cat /proc/$(pgrep program)/maps

# Output:
# 00400000-00401000 r-xp  ... program   ← .text
# 00600000-00601000 r--p  ... program   ← .rodata
# 00601000-00602000 rw-p  ... program   ← .data/.bss
# 7fff00000000-7fff00021000 rw-p ...    ← stack
```

---

## Prevention Patterns

### Use Smart Pointers — Eliminate Dangling

```cpp
// Raw pointer — manual lifetime, easy to dangle
int* p = new int(5);
use(p);
delete p;       // forget this → leak; use after → dangle

// unique_ptr — automatic delete, move-only (no aliasing)
auto p = std::make_unique<int>(5);
use(*p);
// deleted automatically on scope exit — never dangling

// shared_ptr — shared ownership, reference-counted
auto p = std::make_shared<int>(5);
auto q = p;     // both share ownership
// deleted when LAST owner goes out of scope
```

### Bounds-Checked Containers

```cpp
// operator[] — no check (fast)
v[i];       // UB if out of range

// .at() — throws std::out_of_range (safe)
v.at(i);

// span (C++20) — bounds-checked view over contiguous memory
std::span<int> s(arr, 5);
s[10];      // UB in release; debug span implementations check bounds
```

### Avoid Raw Pointer Arithmetic

```cpp
// Dangerous
int* p = arr;
p += 10;    // did you account for the array size?

// Prefer iterators and range-based for
for (auto& x : arr) { use(x); }

// Or algorithms
std::for_each(arr.begin(), arr.end(), use);
```

### Never Return References/Pointers to Locals

```cpp
// WRONG
int& bad() {
    int x = 5;
    return x;       // x destroyed on return — dangling reference
}

// OK — return by value (NRVO eliminates copy)
int good() {
    int x = 5;
    return x;
}

// OK — return pointer to heap (caller must manage lifetime)
int* also_ok() {
    return new int(5);   // but prefer unique_ptr
}
```

---

## Mock Interview Questions

---

**[Mid]** What is the difference between a segmentation fault and a bus error?

> Both terminate the process, but they have different hardware causes:
>
> **SIGSEGV (segfault)**: the virtual address is either unmapped (null dereference, stack overflow past guard page, use-after-free of unmapped pages, write to read-only `.rodata`) or the access violates the page's permission bits. The MMU cannot translate the virtual address to a physical one with the requested access type.
>
> **SIGBUS (bus error)**: the virtual address is mapped and the permission is correct, but the **physical access is illegal** — most commonly a misaligned access. For example, dereferencing an `int*` that points to an odd address on an ARM processor, which requires 4-byte alignment. On x86/x64, misaligned access is handled in hardware and just runs slower — no SIGBUS. On RISC architectures (ARM, MIPS, SPARC), it's a hard fault.
>
> Practically: segfault = wrong address or permissions. Bus error = right address, wrong alignment.

---

**[Mid]** A program crashes with SIGSEGV but only in production, never in development. What are the likely causes?

> The most common reasons a segfault is not reproducible in development:
>
> 1. **ASLR (Address Space Layout Randomization)**: production may have ASLR enabled while development has it disabled (or vice versa). A dangling pointer may accidentally point to valid mapped memory in one layout but crash in another.
>
> 2. **Different allocation patterns**: the allocator reuses freed memory based on allocation order. In development, a freed chunk may never get reused during testing, so the dangling pointer reads stale-but-valid data. In production under load, the chunk is reused immediately, causing a crash.
>
> 3. **Optimization level**: debug builds (`-O0`) keep variables alive longer, preventing some use-after-scope bugs from manifesting. Release builds (`-O2/-O3`) may reuse stack slots or elide stores, making dangling access more likely to crash.
>
> 4. **Thread count / timing**: data races only manifest under concurrent access. Production has more concurrent requests than a developer's local test.
>
> 5. **Stack depth**: production has deeper call chains (more middleware, logging, authentication layers) — a function that's fine locally causes stack overflow under the real call depth.
>
> **Fix**: run with AddressSanitizer in staging. ASan makes these bugs deterministic regardless of ASLR or allocation patterns.

---

**[Mid]** Can a segfault be caught and recovered from?

> Technically yes — you can install a `SIGSEGV` handler with `sigaction()` and use `siglongjmp()` to restore execution. But **this is almost always wrong** in practice:
>
> ```cpp
> // DANGEROUS — rarely the right solution
> #include <csignal>
> #include <csetjmp>
> sigjmp_buf jmp_env;
>
> void sigsegv_handler(int) {
>     siglongjmp(jmp_env, 1);   // jump back to safe point
> }
>
> int main() {
>     signal(SIGEV, sigsegv_handler);
>     if (sigsetjmp(jmp_env, 1) == 0) {
>         int* p = nullptr;
>         *p = 5;               // segfaults → handler fires → longjmp
>     } else {
>         puts("recovered");    // resumes here
>     }
> }
> ```
>
> **Why this is wrong**:
> - The program state is undefined after the segfault. The fault may have occurred mid-way through modifying a data structure — recovering continues with a corrupt heap/stack.
> - You cannot call most functions from a signal handler (not async-signal-safe).
> - Stack unwinding does not happen — destructors don't run — RAII breaks.
>
> **Legitimate uses**: sandboxes and JIT compilers intercept SIGSEGV to handle lazy memory mapping or guard page violations in a controlled way (e.g., the JVM handles null checks via SIGSEGV in optimized code). These are highly specialized and require careful signal-safe code.

---

**[Senior]** Explain how AddressSanitizer detects use-after-free without the OS unmapping the freed memory.

> ASan uses **shadow memory** — a parallel memory region that records the state of every 8 bytes of application memory as one "shadow byte":
> - Shadow value `0`: all 8 bytes are accessible
> - Shadow value `n` (1–7): first n bytes accessible, rest are redzone
> - Shadow value `< 0`: entire 8-byte region is poisoned (freed, redzone, or unaddressable)
>
> **On `free()`**: ASan poisons the freed region in shadow memory (sets shadow bytes to a "freed heap" poison value) and quarantines the chunk — it is **not returned to the allocator** immediately. Instead it sits in a quarantine queue.
>
> **On every load/store** (instrumented by the compiler at compile time): ASan inserts a check:
> ```cpp
> // Compiler transforms:   *p = 5;
> // Into:
> if (IsPoisoned(p)) ReportError(p);
> *p = 5;
> ```
>
> **On `malloc()`**: ASan places red zones (poisoned 8-byte guards) before and after every allocation. Overflows into the redzone are caught immediately.
>
> **Quarantine** is key: by keeping freed chunks in quarantine rather than immediately recycling them, ASan ensures the freed region stays poisoned long enough to catch use-after-free bugs that would be invisible if the chunk were immediately reused. The quarantine is a FIFO — old chunks are eventually released to the allocator when the quarantine exceeds a size limit (default 256 MB).
>
> This is why ASan has ~2× memory overhead and ~2× slowdown — shadow memory + quarantine + instrumented checks on every access.

---

**[Senior]** A multithreaded server crashes with SIGSEGV under high load. Walk through how you'd diagnose it.

> **Step 1 — Reproduce and capture**
> ```bash
> ulimit -c unlimited           # enable core dumps
> # Run the server and trigger the load
> gdb ./server core             # open the core dump
> (gdb) thread apply all bt     # print backtrace for ALL threads
> ```
> Look for which thread crashed and what it was doing. Check other threads for context — one may hold a lock the crashing thread needed.
>
> **Step 2 — Identify the crash category**
> - `0x0` offset → null dereference → which pointer was null and why?
> - Valid-looking heap address → use-after-free or data race corrupting a pointer
> - Repeating address pattern (`0x4141...`) → buffer overflow into a pointer
>
> **Step 3 — Run with ThreadSanitizer**
> ```bash
> g++ -fsanitize=thread -g -O1 server.cpp -o server_tsan
> ./server_tsan   # TSan reports data races with exact location and both threads
> ```
> Under load, a data race may corrupt a shared pointer before it's dereferenced.
>
> **Step 4 — Run with AddressSanitizer**
> ```bash
> g++ -fsanitize=address -g -O1 server.cpp -o server_asan
> ./server_asan   # ASan reports exact use-after-free or OOB
> ```
>
> **Step 5 — Common patterns in server crashes**
> - **Shared object deleted while in use by another thread**: a request handler holds a raw pointer to an object that another thread deletes. Fix: `shared_ptr` for shared ownership.
> - **Vector/map invalidated during iteration**: one thread inserts into a container while another is iterating it. Fix: reader-writer lock or lock-free structure.
> - **Callback outliving its captured reference**: an async callback captures a local by reference; the local goes out of scope before the callback fires. Fix: capture by `shared_ptr`.
> - **Thread stack overflow under deep call chains**: production has more middleware layers. Fix: `ulimit -s`, or move large locals to heap.
