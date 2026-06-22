# Memory Layout & Compilation Process in C++

> **Ownership & segment overview (stack/heap/static, RAII):** [core_cpp_memory_and_ownership.md](core_cpp_memory_and_ownership.md) § Process Memory Layout

---

## 1. Process Memory Layout

When an OS loads a C++ program, it maps the executable into a **virtual address space** divided into distinct segments. Each segment has a fixed purpose and different permissions.

```
High address
┌─────────────────────────────┐
│         Kernel Space        │  ← OS-reserved, not accessible from user code
├─────────────────────────────┤
│           Stack             │  ← grows downward ↓
│      (local vars, frames)   │
│             ↓               │
│        [guard page]         │  ← unmapped page — stack overflow → SIGSEGV
│                             │
│             ↑               │
│           Heap              │  ← grows upward ↑
│      (dynamic allocation)   │
├─────────────────────────────┤
│      Memory-mapped region   │  ← mmap(), shared libs (.so/.dll)
├─────────────────────────────┤
│         BSS Segment         │  ← uninitialized globals/statics (zeroed by OS)
├─────────────────────────────┤
│         Data Segment        │  ← initialized globals/statics
├─────────────────────────────┤
│         Text Segment        │  ← compiled machine code (read-only + execute)
│      (Code Segment)         │
├─────────────────────────────┤
│     Program headers / ELF   │
Low address (0x0)
```

### Segment Details

| Segment | Content | Permissions | Size |
|---|---|---|---|
| **Text (Code)** | Machine instructions | Read + Execute | Fixed at link time |
| **Data** | Initialized globals/statics | Read + Write | Fixed at link time |
| **BSS** | Uninitialized globals/statics | Read + Write | Fixed (zeroed by OS, no disk space) |
| **Heap** | `new`/`malloc` allocations | Read + Write | Grows at runtime |
| **Stack** | Local variables, call frames, return addresses | Read + Write | Grows at runtime (limited ~8MB) |
| **mmap region** | Shared libraries, `mmap()` calls | Varies | Dynamic |

---

### What Lives Where — Code Examples

```cpp
#include <cstdlib>

// ── TEXT SEGMENT ──────────────────────────────
// All compiled machine code lives here (functions, lambdas, vtables code)

// ── DATA SEGMENT (initialized) ───────────────
int    global_init   = 42;          // global, initialized → data
double pi            = 3.14;        // global, initialized → data

class Foo {
    static int count_;              // declared here
};
int Foo::count_ = 10;               // defined+initialized → data

// ── BSS SEGMENT (uninitialized / zero-initialized) ──
int    global_uninit;               // global, no init → BSS (zero-initialized)
static char buf[4096];              // static, no init → BSS

// ── STACK ─────────────────────────────────────
void foo() {
    int   local     = 5;            // local variable → stack
    char  arr[256];                 // local array → stack
    int*  ptr       = &local;       // pointer itself → stack
                                    // (what it points to depends on allocation)
}

// ── HEAP ──────────────────────────────────────
void bar() {
    int* p   = new int(7);          // heap (raw new)
    int* arr = new int[100];        // heap array
    auto sp  = std::make_shared<int>(9);  // heap (control block + int)
    free(malloc(64));               // malloc → heap
}

// ── TEXT: string literals are in read-only data ──
const char* s = "hello";           // "hello" → .rodata (read-only data section)
                                    // pointer s → data segment (or stack if local)
```

---

## 2. Stack in Detail

### Stack Frame Layout

Each function call pushes a **stack frame** (activation record). The frame holds everything the function needs during its execution.

```
High address
┌──────────────────────────┐
│   ... caller's frame ... │
├──────────────────────────┤  ← previous frame pointer (rbp before call)
│   return address         │  ← pushed by CALL instruction (rip of next instruction)
│   saved rbp              │  ← caller's base pointer (for frame unwinding)
│   local variable 1       │
│   local variable 2       │
│   ...                    │
│   spilled registers      │  ← register values saved across calls
│   arguments to callee    │  ← (if > 6 args on x86-64 System V ABI)
├──────────────────────────┤  ← rsp (stack pointer, current top)
│       [free stack]       │
Low address
```

**x86-64 Linux calling convention (System V AMD64 ABI)**:
- First 6 integer/pointer args: `rdi`, `rsi`, `rdx`, `rcx`, `r8`, `r9`
- First 8 float args: `xmm0`–`xmm7`
- Additional args: pushed on the stack (right to left)
- Return value: `rax` (integer/pointer), `xmm0` (float/double)

```cpp
// How the compiler sees this function's stack frame:
int add(int a, int b) {
    // a → register rdi (caller passes here)
    // b → register rsi
    int result = a + b;     // result may stay in register or be spilled to stack
    return result;          // return value in rax
}
```

### Stack Overflow

The stack has a fixed limit (typically **8 MB** on Linux, 1–4 MB on Windows). Exceeding it causes a **stack overflow** — the OS detects access to the guard page and sends `SIGSEGV`.

```cpp
// Classic causes:
void infinite_recursion() {
    infinite_recursion();   // each call pushes a frame → stack overflow
}

void large_local_array() {
    char buf[8 * 1024 * 1024];  // 8 MB on stack → instant overflow
    // Fix: use heap (std::vector, std::make_unique<char[]>)
}
```

Check/set stack size on Linux:
```bash
ulimit -s           # show stack limit (KB)
ulimit -s unlimited # remove limit (only for current shell)
```

---

## 3. Heap in Detail

### How `new` / `malloc` Work

```
User code                  Allocator (glibc/tcmalloc/jemalloc)
─────────                  ─────────────────────────────────────
new int(5)    →  malloc(4) → check free list → if available, return chunk
                           → if not, call sbrk() or mmap() to get more pages
                              from the OS → carve out chunk → return pointer
```

**Each heap allocation has metadata overhead** — glibc stores an 8–16 byte header before the returned pointer:

```
[ size | flags | prev_size ][ user data ... ]
 ← hidden from user →       ← ptr returned →
```

### Heap Fragmentation

```cpp
// External fragmentation: many small allocations scattered
for (int i = 0; i < 1000; ++i) {
    int* p = new int;
    if (i % 2 == 0) delete p;  // alternating delete leaves holes
}
// Free memory exists but in scattered chunks → large allocation may fail
// even though total free memory is sufficient

// Internal fragmentation: allocator rounds up to alignment
new char[1];    // allocator may return 16 bytes — 15 bytes wasted (internal fragmentation)
```

### Heap Alternatives

```cpp
// Stack allocation via alloca (avoid — non-standard, dangerous)
char* buf = (char*)alloca(256);   // stack-allocated, no free needed

// Pool allocator — pre-allocate fixed-size blocks (no fragmentation)
// Used in game engines, embedded systems, high-frequency trading

// Arena/bump allocator — allocate from a pre-allocated region
struct Arena {
    char   buf[1024 * 1024];   // 1MB block
    size_t offset = 0;

    void* alloc(size_t n, size_t align = 8) {
        offset = (offset + align - 1) & ~(align - 1);  // align up
        void* ptr = buf + offset;
        offset += n;
        return ptr;
    }
    void reset() { offset = 0; }   // free everything at once — O(1)
};
```

---

## 4. How `delete` Actually Works

`delete ptr` always does **exactly two things**, in this order — nothing more:

```cpp
delete ptr;

// Compiler expands to:
ptr->~T();              // 1. call the destructor
operator delete(ptr);   // 2. release memory to allocator (→ free())
```

It does **not** zero the memory. It does **not** null the pointer. It does **not** make the address inaccessible.

---

### Step 1 — Destructor Call

```cpp
struct Foo {
    int*  data;
    FILE* file;

    Foo()  : data(new int[10]), file(fopen("x.txt", "r")) {}
    ~Foo() { delete[] data; fclose(file); }
};

Foo* f = new Foo();
delete f;
// → f->~Foo() called  → delete[] f->data, fclose(f->file)
// → operator delete(f) called  → memory returned to allocator
```

For **trivial types** (no user destructor), the compiler skips the destructor call entirely:

```cpp
int* p = new int(5);
delete p;   // no destructor call — just operator delete(p)
```

---

### Step 2 — `operator delete` / `free` Internals

`operator delete` calls `free()`, which **does not return memory to the OS** in most cases. It marks the chunk as free inside the allocator's **free list**, a data structure maintained entirely within the heap region.

```
Before delete:
┌────────────────────────────────────────┐
│ [chunk header | size=32 | in_use=1]    │
│ [your Foo object ..................... ]│  ← ptr points here
└────────────────────────────────────────┘

After delete / free():
┌────────────────────────────────────────┐
│ [chunk header | size=32 | in_use=0]    │  ← marked free
│ [fd → next_free_chunk .............. ] │  ← first 8–16 bytes repurposed
│ [bk → prev_free_chunk .............. ] │     as free list pointers (glibc)
└────────────────────────────────────────┘
         ↓
    Added to size-class free bin
    Physical pages still owned by the process
```

> The freed bytes are immediately used by the allocator to maintain its free list. This is why reading a dangling pointer after `delete` often returns garbage that looks like pointer values.

---

### What Memory Looks Like After `delete`

```cpp
struct Vec3 { float x, y, z; };

Vec3* v = new Vec3{1.0f, 2.0f, 3.0f};
delete v;

// Memory is NOT zeroed — stale values may still be readable
printf("%f\n", v->x);   // may print 1.0 (luck) or garbage (allocator overwrote it)
                         // → undefined behavior either way

// Under AddressSanitizer: immediate crash with "heap-use-after-free"
// Under MSVC debug: memory filled with 0xDD ("dead")
// Under glibc debug malloc: filled with 0xFEEEFEEE
```

---

### glibc Free Bin Organization

glibc (ptmalloc2) organizes freed chunks by size into bins for fast reuse:

```
Fast bins  (≤ 160 bytes) — singly-linked, LIFO, no coalescing
Small bins (< 512 bytes) — doubly-linked, FIFO
Large bins (≥ 512 bytes) — sorted doubly-linked
Unsorted bin             — newly freed chunks land here first, sorted on next malloc
```

```cpp
// Address reuse in action
int* p = new int(42);
void* old_addr = p;
delete p;

int* q = new int(99);   // allocator pops from fast bin
printf("%d\n", p == q); // often prints 1 — same address reused!
// Dangling 'p' now aliases live object 'q'
```

---

### `delete` vs `delete[]`

```cpp
int* p   = new int(5);    delete p;     // ✅ scalar new → scalar delete
int* arr = new int[10];   delete[] arr; // ✅ array new  → array delete

int* arr = new int[10];   delete arr;   // 💥 UB — wrong size, destructors skipped
```

**Where does `delete[]` know the element count?**

The compiler stores the count in a hidden prefix before the user pointer:

```
new Foo[3] layout:

┌──────────────────────────────────────────┐
│ [alloc header] [count=3] [Foo][Foo][Foo] │
                            ↑
                    ptr returned to user
```

```cpp
struct Foo { ~Foo() { puts("dtor"); } };
Foo* arr = new Foo[3];

delete[] arr;   // reads hidden count=3, calls ~Foo() ×3 ✅
delete arr;     // calls ~Foo() ×1 only, frees wrong size → heap corruption 💥
```

---

### The Pointer Is NOT Nulled After `delete`

```cpp
int* p = new int(5);
delete p;

// p still holds the old address
printf("%p\n", (void*)p);  // prints old address — dangling pointer
*p = 10;                   // UB — use-after-free

// Best practice: null manually, or use smart pointers
p = nullptr;
delete p;   // delete nullptr → always safe, does nothing
```

**Why doesn't the compiler auto-null?**
1. **Performance** — an extra store for every `delete`
2. **Aliases** — if `q` also points to the same address, nulling `p` doesn't help

---

### Virtual Destructor — Why It Matters for `delete`

```cpp
struct Base {
    ~Base() { puts("~Base"); }   // non-virtual
};
struct Derived : Base {
    int* data = new int[100];
    ~Derived() { delete[] data; }
};

Base* b = new Derived();
delete b;
// Step 1: calls ~Base() only — ~Derived() never runs → data leaked!
// Step 2: operator delete(b, sizeof(Base)) — wrong size
```

```cpp
struct Base {
    virtual ~Base() {}   // always add virtual dtor to polymorphic base types
};

Base* b = new Derived();
delete b;
// Step 1: vtable lookup → ~Derived() → delete[] data → then ~Base()
// Step 2: operator delete(b) — correct
```

---

### Custom `operator delete`

```cpp
struct PoolObject {
    static MemoryPool pool;

    void* operator new(size_t size)      { return pool.allocate(size); }
    void  operator delete(void* ptr) noexcept { pool.deallocate(ptr); }
};

PoolObject* obj = new PoolObject();
delete obj;
// → ~PoolObject() called
// → PoolObject::operator delete(obj) → back to pool, not free()
```

---

### Full `delete` Flow

```
delete ptr
    │
    ├─ ptr == nullptr? → do nothing, return
    │
    ├─ [1] DESTRUCTOR
    │       Is T polymorphic? → vtable lookup → call most-derived ~T()
    │       Is T trivial?     → skip destructor entirely
    │       ~T() runs cleanup; chains up through base destructors
    │
    └─ [2] OPERATOR DELETE
            Calls operator delete(ptr)  [or class-specific overload if defined]
                → calls free(ptr)
                    → marks chunk free in allocator's free list
                    → may coalesce adjacent free chunks
                    → large free regions may be returned to OS via munmap()
                    → memory is NOT zeroed
                    → ptr is NOT nulled
                    → physical pages may remain in the process
```

---

## 5. Memory Alignment & Padding

### What is Alignment?

The CPU reads memory most efficiently when data starts at an address that is a **multiple of its size**. Misaligned access may be slower (extra memory bus cycles) or fault on some architectures.

```
Address:  0   1   2   3   4   5   6   7
           ┌───┬───┬───┬───┬───┬───┬───┬───┐
           │   │   │   │   │   │   │   │   │
           └───┴───┴───┴───┴───┴───┴───┴───┘

int at address 4: aligned (4 % 4 == 0) ✅ — one bus cycle
int at address 3: misaligned (3 % 4 != 0) ❌ — spans two cache lines
```

### Struct Padding Rules

The compiler inserts **padding** to align each member to its natural alignment:

```cpp
struct Bad {
    char  a;    // 1 byte    offset 0
                // 3 bytes padding (to align int to 4)
    int   b;    // 4 bytes   offset 4
    char  c;    // 1 byte    offset 8
                // 7 bytes padding (to align double to 8)
    double d;   // 8 bytes   offset 16
                // 6 bytes tail padding (struct size must be multiple of max alignment = 8)
};
// sizeof(Bad) = 24  ← NOT 14!

struct Good {
    double d;   // 8 bytes   offset 0
    int    b;   // 4 bytes   offset 8
    char   a;   // 1 byte    offset 12
    char   c;   // 1 byte    offset 13
                // 2 bytes tail padding
};
// sizeof(Good) = 16  ← 33% smaller, same data

// Rule: order members largest → smallest to minimize padding
```

### Inspecting Layout

```cpp
#include <cstddef>

struct S {
    char  a;
    int   b;
    char  c;
    double d;
};

// Print offsets at compile time
static_assert(offsetof(S, a) == 0);
static_assert(offsetof(S, b) == 4);
static_assert(offsetof(S, c) == 8);
static_assert(offsetof(S, d) == 16);

printf("sizeof(S) = %zu\n", sizeof(S));  // 24
```

### Controlling Alignment

```cpp
// Force remove padding (dangerous — may cause misaligned access)
#pragma pack(1)
struct Packed {
    char a;     // offset 0
    int  b;     // offset 1 — misaligned!
};
#pragma pack()

// Set specific alignment (C++11)
struct alignas(64) CacheLineAligned {
    int data[16];   // starts at cache-line boundary (64 bytes)
    // Avoids false sharing between threads
};

// alignas on variable
alignas(32) float simd_data[8];  // 32-byte aligned for AVX intrinsics
```

### False Sharing (Cache Line Contention)

```cpp
// PROBLEM: two thread-local counters in the same cache line
struct Counters {
    std::atomic<int> a;   // thread 1 writes this
    std::atomic<int> b;   // thread 2 writes this
    // both fit in one 64-byte cache line → writing one invalidates the other
    // → cache ping-pong between cores → severe slowdown
};

// FIX: pad each to a separate cache line
struct alignas(64) PaddedCounter {
    std::atomic<int> value;
    char _pad[60];          // or use std::hardware_destructive_interference_size
};

PaddedCounter counters[2];  // each on its own cache line
```

---

## 5. Virtual Memory

The OS gives each process the **illusion of a private, contiguous address space** — this is virtual memory. The **MMU (Memory Management Unit)** translates virtual addresses to physical addresses via a **page table**.

```
Process A                       Physical Memory
Virtual Address Space
┌─────────────┐                 ┌─────────────┐
│  0x400000   │ ──page table──► │  Frame 37   │  text
│  0x401000   │ ──page table──► │  Frame 12   │  data
│  0x7fff0000 │ ──page table──► │  Frame 98   │  stack
│  0x7ffe0000 │   (not mapped)  │             │  guard page → SIGSEGV on access
└─────────────┘                 └─────────────┘

Process B has its own page table → sees completely different physical frames
```

### Key concepts

**Page**: smallest unit of virtual memory (typically **4 KB**). The OS allocates/maps memory in pages.

**Page fault**: accessing a virtual page that isn't mapped to physical memory yet:
- **Minor fault**: page exists but not yet mapped (e.g., lazy allocation). OS maps it — no I/O needed.
- **Major fault**: page is on disk (swapped out). OS reads it from disk — slow (~10ms).

**ASLR (Address Space Layout Randomization)**: the OS randomizes the base addresses of stack, heap, and libraries each run — makes exploiting buffer overflows harder.

```bash
# Check ASLR setting (Linux)
cat /proc/sys/kernel/randomize_va_space
# 0 = disabled, 1 = partial, 2 = full
```

**Lazy allocation**: `new` / `malloc` may not immediately map physical memory. Physical pages are assigned on first access (first write triggers a minor page fault).

```cpp
// This does NOT immediately use 1 GB of physical RAM:
char* huge = new char[1024 * 1024 * 1024];
// Physical pages are mapped lazily as you access each page
huge[0] = 1;               // minor page fault → map page 0
huge[4096] = 1;            // minor page fault → map page 1
// ...

// To force commit upfront:
memset(huge, 0, 1024 * 1024 * 1024);  // touches every page → all committed
// Or use mlock() to pin pages in RAM
```

---

## 6. Segmentation Fault — When It Happens

A **segfault** (`SIGSEGV`) is the OS killing your process because the CPU attempted to access a virtual address that the MMU finds invalid — not mapped, wrong permission (write to read-only, execute to non-executable), or crossing a guard page.

```
Process access attempt
        │
        ▼
    MMU checks page table
        │
        ├─ page not mapped?          → page fault
        │       ├─ lazy alloc / COW? → minor fault, OS maps it, continue
        │       └─ truly unmapped?   → SIGSEGV
        │
        ├─ write to read-only page?  → SIGSEGV  (e.g. .rodata, .text)
        └─ access guard page?        → SIGSEGV  (stack overflow)
```

---

### 1. Null Pointer Dereference

```cpp
int* p = nullptr;
*p = 5;           // → SIGSEGV — address 0x0 is never mapped

// Hidden null dereferences
struct Node { int val; Node* next; };
Node* n = nullptr;
int x = n->val;           // → SIGSEGV
int y = n->next->val;     // → SIGSEGV (n itself is null)

// Through a method call
std::string* s = nullptr;
s->size();                // → SIGSEGV (this == nullptr inside size())
```

---

### 2. Dangling Pointer (Use-After-Free)

```cpp
int* p = new int(42);
delete p;
*p = 10;          // → SIGSEGV or silent corruption — UB

// Dangling reference to stack variable
int& get_local() {
    int x = 5;
    return x;     // x destroyed on return
}
int& r = get_local();
r = 10;           // → SIGSEGV — stack frame is gone

// Dangling after vector reallocation
std::vector<int> v = {1, 2, 3};
int& ref = v[0];
v.push_back(4);   // may reallocate — ref is now dangling
ref = 99;         // → SIGSEGV or silent corruption
```

---

### 3. Stack Overflow

```cpp
// Infinite recursion — each frame pushes until guard page is hit
void recurse() { recurse(); }   // → SIGSEGV

// Huge local allocation
void foo() {
    char buf[8 * 1024 * 1024];  // 8 MB on stack → overflows immediately
    // Fix: std::vector<char> buf(8 * 1024 * 1024);  ← heap
}
```

---

### 4. Out-of-Bounds Access

```cpp
int arr[5];
arr[5]  = 1;   // one past end — UB, may segfault
arr[-1] = 1;   // negative index — likely segfault

// std::vector::operator[] has NO bounds check
std::vector<int> v(5);
v[10] = 1;     // → UB, likely SIGSEGV or silent corruption
v.at(10);      // → throws std::out_of_range  ← safe alternative

// C-string overrun
char buf[8];
strcpy(buf, "this string is way too long");  // → stack corruption → crash later
```

---

### 5. Writing to Read-Only Memory

```cpp
// String literals live in .rodata — no write permission
char* s = "hello";   // should be const char*
s[0] = 'H';          // → SIGSEGV — .rodata page is read-only

// Correct:
char s[] = "hello";  // copy to stack — writable
s[0] = 'H';          // OK

// Writing to .text (function pointer abuse)
void foo() {}
char* p = (char*)foo;
*p = 0x90;           // → SIGSEGV — .text is read+execute, not writable
```

---

### 6. Double Free

```cpp
int* p = new int(5);
delete p;
delete p;    // → SIGSEGV or heap corruption — allocator metadata is trashed
             // Under ASan: "double-free" error with exact stack trace
```

---

### 7. Function Pointer Through Garbage / Corrupt vptr

```cpp
using Fn = void(*)();
Fn fp = (Fn)0xdeadbeef;   // garbage address
fp();     // → SIGSEGV — jump to unmapped address

// Corrupt vtable
struct Base { virtual void foo() {} };
Base* b = new Base();
memset(b, 0, sizeof(Base));  // zero out vptr
b->foo();                    // → SIGSEGV — vtable pointer is null
```

---

### 8. Thread Stack Issues

```cpp
// Thread-local stack has same limits as main thread (~8 MB)
std::thread t([]() {
    char huge[9 * 1024 * 1024];  // → SIGSEGV inside thread
    (void)huge;
});

// Accessing a thread's local after join
int* ptr;
{
    std::thread t([&ptr]() {
        int local = 42;
        ptr = &local;    // points to thread's stack frame
    });
    t.join();            // t's stack is destroyed here
}
*ptr = 1;               // → SIGSEGV — thread stack is gone
```

---

### Quick Reference Table

| Cause | Address seen in crash | Key symptom |
|---|---|---|
| Null dereference | `0x0` or small offset | `nullptr->field` |
| Use-after-free | Valid-looking but freed | Passes sometimes, fails under ASLR |
| Stack overflow | Just below stack limit | Deep recursion / huge local |
| Out-of-bounds | Random / adjacent | Off-by-one, negative index |
| Write to `.rodata` | Low address in text range | String literal write |
| Double free | — | Heap metadata trashed |
| Bad function pointer | `0xdeadbeef` etc. | Corrupt vtable / null fp |

---

### How to Diagnose

```bash
# 1. AddressSanitizer — catches most memory bugs with exact line numbers
g++ -fsanitize=address -g program.cpp && ./a.out

# 2. Core dump + gdb
ulimit -c unlimited && ./program    # generates core on crash
gdb ./program core
(gdb) bt                            # backtrace at crash point
(gdb) info registers                # see rip/rsp values
(gdb) x/10x $rsp                    # inspect stack memory

# 3. Valgrind — slower, catches more subtle issues
valgrind --leak-check=full ./program

# 4. Quick gdb run
gdb ./program
(gdb) run
(gdb) bt
```

---

## 7. Translation Units

### What is a Translation Unit?

A **translation unit (TU)** is the fundamental unit of compilation in C++. It is the result of taking one `.cpp` source file and feeding it through the preprocessor — all `#include`d headers are pasted in, macros are expanded, and the result is a single self-contained body of code that the compiler processes at once.

```
foo.cpp  ──┐
bar.h    ──┤  preprocessor  →  foo.ii  →  compiler  →  foo.o
baz.h    ──┘     (paste)       (one TU)                (one object file)
```

> **One `.cpp` file = one translation unit = one `.o` object file.**
> Headers are not translation units — they are *included into* TUs.

### Why Translation Units Matter

Each TU is compiled **in isolation** — the compiler sees only what is in that one file (after preprocessing). It has no knowledge of other `.cpp` files. This has far-reaching consequences:

```cpp
// a.cpp — TU 1
int add(int x, int y) { return x + y; }   // definition lives here

// b.cpp — TU 2
// The compiler only sees b.cpp — it doesn't know about a.cpp
// It needs a DECLARATION to know the type/signature of add()
int add(int, int);      // declaration (or #include a header that has it)
int main() {
    return add(3, 4);   // compiler trusts the declaration; linker resolves it
}
```

---

### Linkage: How Names Are Visible Across TUs

**Linkage** controls whether a name defined in one TU can be seen and used by other TUs.

#### External Linkage — visible across TUs (default for most things)

```cpp
// file.cpp
int counter = 0;           // external linkage — any TU can reference this
void increment() { ++counter; }   // external linkage

// other.cpp
extern int counter;        // declaration — refers to the same 'counter' in file.cpp
void increment();          // declaration — same function
```

#### Internal Linkage — private to the TU (`static` or anonymous namespace)

```cpp
// util.cpp
static int helper_state = 0;       // internal linkage — invisible outside util.cpp
static void helper() { ... }       // internal linkage — not visible to linker

namespace {                         // anonymous namespace = internal linkage
    int also_private = 42;
    void also_hidden() { ... }
}
```

The linker will **never** see internally-linked names — they cannot cause duplicate-definition errors and cannot be referenced from other TUs.

#### No Linkage — local to a scope

```cpp
void foo() {
    int x = 5;          // local variable — no linkage (not visible outside the function)
}
```

#### Summary Table

| Declaration | Linkage | Visible in other TUs? |
|---|---|---|
| `int x = 0;` (global) | External | Yes (via `extern int x;`) |
| `void f() {}` (global) | External | Yes |
| `static int x = 0;` (global) | Internal | No |
| `static void f() {}` (global) | Internal | No |
| `namespace { int x; }` | Internal | No |
| `int x;` (local) | None | No |
| `const int x = 0;` (global, C++) | **Internal** (special C++ rule) | No (unless `extern const`) |

> **C++ special rule**: a `const` global has internal linkage by default (unlike C). To share it across TUs, add `extern`:
> ```cpp
> // constants.h
> extern const int MAX = 100;   // external linkage — one definition across all TUs
> // or use inline (C++17):
> inline constexpr int MAX = 100;  // inline → each TU has a copy, ODR still satisfied
> ```

---

### One Definition Rule (ODR) and TUs

The **One Definition Rule** governs what can be defined once vs many times:

**Across all TUs (the whole program)**:
- Every variable and non-inline function must be **defined exactly once**
- Having zero definitions → linker error (undefined reference)
- Having two definitions → linker error (multiple definition)

```cpp
// a.cpp
int x = 10;     // definition

// b.cpp
int x = 20;     // ANOTHER definition → linker error: multiple definition of 'x'

// fix: define in one TU, declare with 'extern' in others
// a.cpp: int x = 10;
// b.cpp: extern int x;  (or put extern declaration in a shared header)
```

**Within a single TU**:
- A name may be *declared* multiple times (OK) but *defined* only once

```cpp
void foo();        // declaration
void foo();        // declaration again — OK, consistent
void foo() { }     // definition — OK, first and only definition
void foo() { }     // SECOND definition — error: redefinition in same TU
```

**Exceptions — can be defined in multiple TUs (must be identical)**:
- `inline` functions and variables
- `constexpr` / `consteval` functions
- Class/struct definitions
- Templates (function and class)
- `enum` and `enum class` definitions

```cpp
// header.h — included in many TUs, each gets a copy — ODR OK
inline int square(int x) { return x * x; }   // inline → multiple definitions allowed
struct Point { int x, y; };                   // class → multiple definitions allowed
template<typename T> T max(T a, T b) { return a > b ? a : b; }  // template → OK

// All copies must be IDENTICAL — different definitions in different TUs = undefined behavior
// (the compiler/linker may silently pick one at random)
```

---

### `inline` Variables (C++17) — Sharing Across TUs

Before C++17, sharing a `const` variable in a header required an `extern` declaration in the header and a single definition in a `.cpp`:

```cpp
// constants.h (pre-C++17, error-prone)
extern const int MAX_SIZE;   // declaration only

// constants.cpp
const int MAX_SIZE = 256;    // exactly one definition
```

C++17 `inline` variables solve this — define directly in the header, the linker merges all copies into one:

```cpp
// constants.h (C++17)
inline constexpr int MAX_SIZE = 256;   // each TU gets a copy; linker deduplicates
inline const std::string APP_NAME = "MyApp";  // works for non-trivial types too
```

---

### Templates and TUs

Templates are **instantiated per TU** — each TU that uses `std::vector<int>` generates its own copy of that instantiation. The linker deduplicates (via weak symbols).

**Explicit instantiation** controls this:

```cpp
// foo.h
template<typename T>
void process(T val);   // declaration

// foo.cpp
template<typename T>
void process(T val) { /* definition */ }

// Explicit instantiation — tells the compiler to emit this specialization here
template void process<int>(int);
template void process<double>(double);

// foo_user.cpp
#include "foo.h"
// Explicit instantiation declaration — tells this TU NOT to instantiate it
// (use the one from foo.cpp)
extern template void process<int>(int);   // C++11

void use() {
    process(42);    // resolved from foo.cpp's explicit instantiation — no duplicate
}
```

**Why this matters**: without explicit instantiation, a template used in 100 TUs generates 100 copies of the same code → link time bloat. Explicit instantiation gives you one canonical copy.

---

### Anonymous Namespaces — The Right Way to Get Internal Linkage

Prefer anonymous namespaces over `static` for internal linkage in C++:

```cpp
// OLD style (C inheritance) — marks one name at a time
static int helper_counter = 0;
static void helper_fn() { }

// PREFERRED C++ style — all names inside are internal
namespace {
    int helper_counter = 0;
    void helper_fn() { }
    struct HelperState { int x; };  // types can also have internal linkage this way
}
```

`static` cannot give internal linkage to types — anonymous namespace can.

---

### Practical Impact on Build Times

Because each TU is compiled independently, you can **parallelize compilation** (`make -j8`, `cmake --build . -j8`). But each TU must re-parse all its `#include`d headers — which is why heavy headers (`<iostream>`, `<string>`, `<windows.h>`) slow down builds.

**Strategies to reduce TU compilation time**:

| Strategy | How it helps |
|---|---|
| **Forward declarations** | Avoid including full header when a pointer/reference suffices |
| **Precompiled headers (PCH)** | Parse heavy headers once, reuse across TUs |
| **`#pragma once` / include guards** | Prevent re-inclusion within one TU |
| **Unity / jumbo builds** | Merge many `.cpp` into one TU — amortize header parsing |
| **Explicit template instantiation** | Avoid N copies of template code across N TUs |
| **Modules (C++20)** | Replace header inclusion with pre-compiled module interfaces |

```cpp
// C++20 Modules — replace the entire header/TU model
// math.cppm (module interface unit)
export module math;
export int add(int a, int b) { return a + b; }

// main.cpp
import math;           // no text inclusion — compiler reads pre-compiled module
int main() { return add(1, 2); }
```

---

## 7. The Compilation Pipeline

C++ source goes through **four distinct stages** before it becomes an executable:

```
source.cpp
    │
    ▼ (1) Preprocessor (cpp)
preprocessed.ii     — macros expanded, #include pasted, #ifdef resolved
    │
    ▼ (2) Compiler (cc1plus)
source.s            — architecture-specific assembly code
    │
    ▼ (3) Assembler (as)
source.o            — binary object file (ELF on Linux, COFF on Windows)
    │
    ▼ (4) Linker (ld)
executable / .so    — final binary, all symbols resolved
```

---

## Stage 1: Preprocessor

The preprocessor is a **text transformation** step — it knows nothing about C++ syntax.

**What it does**:
- Expands `#include` (pastes the file's content inline)
- Expands `#define` macros
- Evaluates `#if` / `#ifdef` / `#ifndef` / `#else` / `#endif`
- Strips comments
- Handles `#pragma` (passes to compiler)

```cpp
// source.cpp
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#ifdef DEBUG
#   define LOG(x) std::cout << x << "\n"
#else
#   define LOG(x)
#endif

int main() {
    int x = MAX(3, 5);   // → int x = ((3) > (5) ? (3) : (5));
    LOG("hello");        // → (empty) if not DEBUG build
}
```

```bash
# Inspect preprocessor output
g++ -E source.cpp -o source.ii
# source.ii contains thousands of lines from expanded headers + your code
```

**Common pitfalls**:
```cpp
// Macro with side effects — expanded twice!
#define SQUARE(x) ((x) * (x))
int n = 3;
int s = SQUARE(n++);   // → ((n++) * (n++)) — undefined behavior!
// Fix: use inline functions or constexpr instead of macros
```

---

## Stage 2: Compiler

The compiler translates preprocessed C++ into **assembly**. This is the most complex stage — involves:
1. **Lexing**: tokenize the source
2. **Parsing**: build an AST (Abstract Syntax Tree)
3. **Semantic analysis**: type checking, name resolution, overload resolution
4. **IR generation**: convert AST to compiler's intermediate representation (LLVM IR for Clang, GIMPLE for GCC)
5. **Optimization passes**: dead code elimination, inlining, loop unrolling, constant folding, etc.
6. **Code generation**: emit target architecture assembly

```bash
# Stop after compilation, produce assembly
g++ -S -O2 source.cpp -o source.s
# -fverbose-asm adds comments showing which C++ line each instruction came from

# View LLVM IR (Clang)
clang++ -emit-llvm -S source.cpp -o source.ll
```

### What the Compiler Can Optimize (with `-O2`)

```cpp
// Constant folding — computed at compile time
int x = 2 + 3;        // → int x = 5;

// Dead code elimination
if (false) { expensive(); }  // → removed entirely

// Function inlining — eliminates call overhead
inline int square(int x) { return x * x; }
int y = square(4);    // → int y = 16; (or just the computation, no call)

// Loop unrolling
for (int i = 0; i < 4; ++i) arr[i] = 0;
// →  arr[0] = 0; arr[1] = 0; arr[2] = 0; arr[3] = 0;

// RVO / NRVO — eliminates copy on return
std::vector<int> make_vec() {
    std::vector<int> v = {1, 2, 3};
    return v;   // NRVO: constructed directly in caller's storage — no copy/move
}
```

### Compilation Flags That Matter

```bash
# Optimization levels
-O0   # no optimization (fastest compile, easiest to debug)
-O1   # basic optimizations
-O2   # standard release (most projects)
-O3   # aggressive (may increase binary size, occasionally slower due to i-cache pressure)
-Os   # optimize for size
-Og   # optimize for debugging (keep debug info, some opts)

# Debug info
-g    # include DWARF debug info (symbols, line numbers) → used by gdb/lldb

# Warnings
-Wall -Wextra -Wpedantic   # enable most warnings
-Werror                    # treat warnings as errors

# Sanitizers (invaluable during development)
-fsanitize=address         # AddressSanitizer: detect use-after-free, buffer overflow
-fsanitize=undefined       # UBSanitizer: detect undefined behavior
-fsanitize=thread          # ThreadSanitizer: detect data races

# Standards
-std=c++17  -std=c++20  -std=c++23
```

---

## Stage 3: Assembler

The assembler converts **assembly text** into a **binary object file** (`.o`). The object file is in ELF format (Linux) or COFF (Windows) and contains:

- **Machine code** (compiled functions)
- **Symbol table**: names of defined functions/variables, and names of externally-referenced symbols (not yet resolved)
- **Relocation entries**: placeholders for addresses that the linker must fill in
- **Debug info** (DWARF) if `-g` was passed
- **Section headers**: `.text`, `.data`, `.bss`, `.rodata`, etc.

```bash
# Inspect an object file
nm source.o         # list symbols (U = undefined, T = defined in text)
objdump -d source.o # disassemble
readelf -a source.o # full ELF header + sections + symbols
```

```bash
$ nm source.o
                 U _GLOBAL_OFFSET_TABLE_
0000000000000000 T main          # T = defined in .text
                 U printf        # U = undefined (resolved by linker from libc)
```

---

## Stage 4: Linker

The linker combines multiple `.o` files and libraries into a single executable or shared library. It:
1. **Symbol resolution**: matches each undefined symbol (`U`) to a definition in another `.o` or library
2. **Relocation**: fills in the actual addresses for all symbol references
3. **Layout**: places sections from all inputs into the final binary's virtual address space

```bash
# Full pipeline (gcc does all stages automatically)
g++ -c file1.cpp -o file1.o   # compile only
g++ -c file2.cpp -o file2.o
g++ file1.o file2.o -o program  # link

# Verbose linker output
g++ file1.o file2.o -o program -Wl,--verbose
```

### Static vs Dynamic Linking

**Static linking**: the library's code is **copied into** the executable at link time.

```bash
g++ main.o -o program libmylib.a   # .a = static archive
# Result: self-contained binary, no runtime dependency on libmylib
```

**Dynamic linking**: the executable records a *dependency* on a shared library; the OS loads it at runtime.

```bash
g++ main.o -o program -lmylib     # links against libmylib.so
# At runtime: dynamic linker (ld.so) loads libmylib.so and resolves symbols
ldd program                        # list shared library dependencies
```

| | Static | Dynamic |
|---|---|---|
| **Binary size** | Larger (lib code included) | Smaller (lib not included) |
| **Startup time** | Faster (no dynamic linking) | Slower (ld.so resolves symbols) |
| **Memory** | Each process has its own copy | Shared library pages shared in RAM |
| **Updates** | Relink to pick up library fix | Replace .so — all users get update |
| **Portability** | Ships standalone | Requires correct .so version at runtime |

### One Definition Rule (ODR)

```cpp
// file1.cpp
int counter = 0;        // defines 'counter' — goes in .data

// file2.cpp
extern int counter;     // declares 'counter' — no definition, just a reference
                        // linker resolves this to file1's definition

// ERROR: two definitions (both without extern)
// file1.cpp: int counter = 0;
// file2.cpp: int counter = 0;  ← linker error: multiple definition of 'counter'
```

### Link Order Matters (Static Libraries)

```bash
# WRONG — linker processes left to right; libfoo.a not yet seen when main.o needs it
g++ -lfoo main.o     # linker error: undefined reference

# CORRECT — object files first, then libraries they depend on
g++ main.o -lfoo     # linker finds foo's symbols when resolving main.o's references

# Circular deps — use --start-group / --end-group
g++ main.o -Wl,--start-group -lfoo -lbar -Wl,--end-group
```

---

## Stage 5: Loading (Runtime)

When you run an executable, the OS **loader** (`execve` + `ld.so`) sets it up before `main()` runs:

```
execve("./program")
    │
    ▼
OS reads ELF headers
    │
    ▼
Maps segments into virtual address space
 ├── .text  → read+execute pages
 ├── .data  → read+write pages (copy from file)
 ├── .bss   → read+write pages (zeroed)
 └── Stack  → allocate initial stack pages
    │
    ▼
Dynamic linker (ld.so) — if dynamically linked
 ├── Load .so dependencies (libc.so, libstdc++.so, ...)
 └── Resolve symbols (GOT/PLT patching)
    │
    ▼
C++ runtime init (_start → __libc_start_main)
 ├── Run global constructors (.init_array)
 ├── Initialize thread-local storage
 └── Call main()
    │
    ▼
main() runs
    │
    ▼
exit() or return from main()
 ├── Run global destructors (.fini_array)
 ├── Flush stdio buffers
 └── OS reclaims process resources
```

### Global Constructors — Order Problem

```cpp
// file1.cpp
std::string g1 = "hello";   // constructor runs before main

// file2.cpp
extern std::string g1;
std::string g2 = g1 + " world";  // DANGER: g1 may not be initialized yet!
// Order of initialization across translation units is UNSPECIFIED
```

**Fix — construct-on-first-use idiom**:
```cpp
std::string& get_g1() {
    static std::string g1 = "hello";  // initialized on first call (thread-safe in C++11)
    return g1;
}

std::string g2 = get_g1() + " world";  // safe — g1 constructed on demand
```

---

## 7. ELF Binary Anatomy

```bash
# Sections in a compiled binary
readelf -S program
```

| Section | Content |
|---|---|
| `.text` | Compiled machine code |
| `.rodata` | Read-only data: string literals, `const` globals |
| `.data` | Initialized read-write globals/statics |
| `.bss` | Uninitialized globals (no disk space, zeroed at load) |
| `.init_array` | Pointers to global constructor functions |
| `.fini_array` | Pointers to global destructor functions |
| `.symtab` | Symbol table (stripped in release builds) |
| `.strtab` | String table (symbol names) |
| `.debug_*` | DWARF debug info (if compiled with `-g`) |
| `.plt` / `.got` | Procedure Linkage Table / Global Offset Table (dynamic linking) |

```bash
# Useful binary inspection commands
size program              # sizes of text/data/bss segments
nm -C program             # symbol table (demangled C++ names)
objdump -d program        # disassemble text section
strings program           # extract printable strings
strip program             # remove symbol table (reduces size)
addr2line -e program 0x4005f0  # map address to source file + line
c++filt _ZN3FooC1Ev       # demangle a C++ symbol name
```

---

## 8. Build Systems Overview

```
Source Files (.cpp/.h)
        │
        ▼
  Build System (CMake / Makefile / Bazel / Meson)
        │  generates
        ▼
  Build Rules (compile commands)
        │  executes
        ▼
  Compiler + Linker (g++ / clang++)
        │
        ▼
  Executable / Library
```

### CMake Essentials

```cmake
cmake_minimum_required(VERSION 3.20)
project(MyApp CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Executable target
add_executable(myapp
    src/main.cpp
    src/foo.cpp
)

# Compile flags
target_compile_options(myapp PRIVATE
    -Wall -Wextra
    $<$<CONFIG:Debug>:-g -O0 -fsanitize=address>
    $<$<CONFIG:Release>:-O2 -DNDEBUG>
)

# Static library
add_library(mylib STATIC lib/bar.cpp)

# Link library to executable
target_link_libraries(myapp PRIVATE mylib)

# Include directories
target_include_directories(myapp PRIVATE include/)
```

```bash
# Build workflow
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/myapp
```

---

## 9. Precompiled Headers & Unity Builds

### Precompiled Headers (PCH)

Heavy headers (`<vector>`, `<string>`, Windows.h) are parsed for **every** translation unit. PCH pre-parses them once.

```cmake
# CMake 3.16+
target_precompile_headers(myapp PRIVATE include/pch.h)
```

```cpp
// pch.h — put stable, heavy headers here
#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
```

### Unity Build (Jumbo Build)

Combines multiple `.cpp` files into one translation unit — the compiler sees them as one big file. Reduces redundant template instantiations and speeds up builds dramatically.

```bash
cmake -B build -DCMAKE_UNITY_BUILD=ON   # CMake 3.16+ unity build support
```

**Trade-off**: faster builds, but ODR violations in different `.cpp` files that happened to work separately may now become visible. Also breaks if files define conflicting `static` functions or macros.

---

## Mock Interview Questions

---

**[Mid]** What is the difference between BSS and the Data segment?

> Both hold **global and static variables**, but the distinction is initialization:
>
> - **Data segment**: variables that have an **explicit non-zero initializer** — e.g., `int x = 42;`. Their values are stored in the executable file on disk and loaded into memory at startup.
> - **BSS segment**: variables that are **uninitialized or zero-initialized** — e.g., `int x;` or `int x = 0;`. The OS guarantees they are zeroed at startup. Crucially, BSS takes **no space on disk** — the executable just records the BSS size; the OS allocates and zeros the pages at load time.
>
> This is why `int arr[1000000];` at global scope doesn't make your executable 4 MB larger — it goes into BSS.

---

**[Mid]** Why does the order of struct members matter for `sizeof`? How do you minimize padding?

> The compiler inserts **padding** between members to ensure each is aligned to its natural alignment boundary (a `double` must be at a multiple of 8, `int` at a multiple of 4, etc.). The struct's total size is also rounded up to a multiple of its largest member's alignment, so arrays of the struct stay aligned.
>
> ```cpp
> struct Padded { char a; int b; char c; };   // sizeof = 12 (3 bytes + 1 byte wasted)
> struct Packed  { int b; char a; char c; };  // sizeof = 8  (no padding needed)
> ```
>
> **Rule**: order members **largest to smallest** alignment. This minimizes padding because you never need to pad up to a larger alignment after placing a smaller member.

---

**[Mid]** Explain what happens between typing `./program` and `main()` being called.

> 1. Shell calls `execve("./program", ...)` — a syscall that replaces the current process image.
> 2. The OS kernel reads the **ELF header** to find the entry point and program headers.
> 3. Kernel maps the binary's segments into virtual memory (`.text`, `.data`, `.bss`, stack).
> 4. For a dynamically linked binary, the kernel transfers control to **`ld.so`** (the dynamic linker), whose path is stored in the `.interp` ELF section.
> 5. `ld.so` reads the `.dynamic` section, loads required shared libraries (`.so` files), and resolves all undefined symbols by patching the **GOT (Global Offset Table)**.
> 6. Control passes to **`_start`** (the C runtime entry point, in `crt0.o`), which sets up `argc`, `argv`, `envp`, and calls `__libc_start_main`.
> 7. `__libc_start_main` runs **global constructors** (functions listed in `.init_array` — e.g., constructors of global C++ objects), sets up `atexit` handlers.
> 8. Finally, `main()` is called.

---

**[Senior]** What is the difference between static and dynamic linking? When would you choose each?

> **Static linking** copies the library's compiled code directly into the executable at link time. The result is a self-contained binary with no external `.so` dependencies.
>
> **Dynamic linking** records that the executable needs a shared library. At runtime, `ld.so` locates and loads the `.so`, and multiple processes share the same physical pages of that library in RAM.
>
> **Choose static when**:
> - Deploying to environments where you can't guarantee the correct `.so` version (containers, embedded, cross-compiling)
> - Startup latency matters and you want to avoid dynamic linker overhead
> - Producing a truly hermetic binary (Go's typical approach)
>
> **Choose dynamic when**:
> - Memory is shared across many processes (e.g., `libc.so` loaded once, shared by hundreds of processes — significant RAM savings)
> - Hot-patching a security fix in a library without relinking every binary that uses it
> - Plugins loaded at runtime (`dlopen`) — necessarily dynamic
>
> **Trade-off summary**: static = simpler deployment, larger binary, no sharing. Dynamic = smaller binary, shared memory, deployment complexity (DLL hell / `.so` versioning).

---

**[Senior]** What is a linker error "undefined reference" and how do you diagnose it?

> An **undefined reference** means the linker found a symbol *used* (referenced) in one translation unit but found no *definition* of it anywhere in the linked inputs.
>
> **Common causes**:
> 1. Forgot to link a library: `g++ main.o` when it uses `pthread_create` → add `-lpthread`
> 2. Wrong link order with static libraries: `g++ -lfoo main.o` should be `g++ main.o -lfoo`
> 3. Declared a function but never defined it (or defined it in a file not compiled)
> 4. `extern "C"` mismatch: a C function linked into C++ code without `extern "C"` wrapping → name mangling produces a different symbol name
> 5. Template defined in `.cpp` instead of header — the template instantiation isn't compiled for the caller's translation unit
>
> **Diagnosis**:
> ```bash
> nm -C your.o | grep " U "        # list undefined symbols in object file
> nm -C libfoo.a | grep symbol_name  # check if library provides the symbol
> c++filt _ZN3Foo3barEv            # demangle mangled name in error message
> ```

---

**[Senior]** What is false sharing and how do you fix it?

> **False sharing** occurs when two threads write to **different variables that happen to reside on the same cache line** (typically 64 bytes). Even though neither thread is accessing the other's data, the CPU cache coherency protocol (MESI) treats the entire cache line as the unit of ownership. When thread A writes its variable, the cache line is invalidated in all other cores' caches — thread B must refetch it before writing its own variable, even though the data didn't logically conflict.
>
> This can reduce performance to near-sequential levels on multi-core systems.
>
> ```cpp
> // PROBLEM
> struct { std::atomic<int> a, b; } counters;
> // thread 1 writes counters.a, thread 2 writes counters.b
> // both in same 8-byte struct → same cache line → false sharing
>
> // FIX: pad to separate cache lines
> struct alignas(64) Counter { std::atomic<int> v; char pad[60]; };
> Counter counters[2];
> // thread 1 writes counters[0].v, thread 2 writes counters[1].v
> // 64-byte alignment → guaranteed separate cache lines
>
> // C++17: use hardware_destructive_interference_size
> struct alignas(std::hardware_destructive_interference_size) Counter {
>     std::atomic<int> v;
> };
> ```
>
> Diagnose with `perf stat -e cache-misses,cache-references ./program` or Intel VTune's memory access analysis.

---

**[Mid]** What does `delete ptr` actually do at the machine level?

> `delete ptr` does exactly two things in order:
> 1. **Calls the destructor** — `ptr->~T()`. For trivial types (no user-defined destructor), this is elided entirely. For polymorphic types, it goes through the vtable to call the most-derived destructor.
> 2. **Calls `operator delete(ptr)`**, which internally calls `free()`. `free()` marks the chunk as available in the allocator's free list — it does **not** zero the memory, does **not** return pages to the OS (usually), and does **not** null the pointer.
>
> After `delete`, the pointer is a **dangling pointer** — it still holds the old address. Accessing it is undefined behavior. The compiler is not required to null it because that would cost an extra store on every `delete`, and aliased pointers would still be left dangling anyway.

---

**[Mid]** What is the difference between `delete` and `delete[]`? What happens if you mix them?

> `delete` calls the destructor once and releases the memory for a single object.
> `delete[]` reads a hidden element count stored just before the user pointer, calls the destructor that many times, then frees the entire block.
>
> Mixing them is **undefined behavior**:
> - `delete` on a `new[]` allocation: only one destructor call instead of N — remaining objects leak their resources. Allocator also receives the wrong size.
> - `delete[]` on a `new` allocation: reads garbage as the element count — may call the destructor an arbitrary number of times, corrupting memory.
>
> Modern practice: use `std::unique_ptr<T>` (calls `delete`) and `std::unique_ptr<T[]>` (calls `delete[]`) so the correct form is encoded in the type and called automatically.

---

**[Mid]** Why does `delete` through a base pointer require a virtual destructor?

> Without a virtual destructor, `delete base_ptr` performs **static dispatch** on the destructor — it always calls `Base::~Base()`, regardless of the actual runtime type. The derived class's destructor never runs, leaking any resources it owns.
>
> With `virtual ~Base()`, the destructor call goes through the vtable, invoking the most-derived destructor first, which then chains upward through base destructors automatically.
>
> The rule: **any class intended to be used polymorphically (deleted through a base pointer) must have a virtual destructor**. If a class is not meant to be a base, mark it `final` to make the intent clear.

---

**[Mid]** What causes a segmentation fault? How do you find the root cause?

> A segfault is the OS/MMU terminating the process because it attempted an illegal memory access:
> - **Not mapped**: null dereference, use-after-free of a freed+unmapped page, accessing beyond a valid allocation
> - **Wrong permission**: writing to `.rodata` (string literals), writing to `.text`, executing non-executable memory
> - **Guard page**: stack overflow — the OS places an unmapped page below the stack; touching it triggers SIGSEGV
>
> **Diagnosis approach** (in priority order):
> 1. **AddressSanitizer** (`-fsanitize=address -g`) — catches use-after-free, out-of-bounds, double-free with exact file/line. Zero guesswork.
> 2. **gdb + core dump** — `ulimit -c unlimited`, reproduce crash, then `gdb ./prog core` → `bt` to see the call stack at the crash point, `info registers` to see the bad address.
> 3. **Valgrind** — slower but catches more subtle cases, including uninitialized reads.
>
> The crash address tells you the category: `0x0` or small offsets → null dereference. Valid-looking address in freed region → use-after-free. Address just below the stack limit → stack overflow.

---

**[Senior]** Why can reading a dangling pointer sometimes return the "correct" old value, and sometimes return garbage?

> After `delete`, the memory is returned to the allocator's free list but **not zeroed** and **not unmapped**. The physical page stays in the process's address space. So the old bytes are still there — until something overwrites them.
>
> If no allocation has reused that chunk yet, the old value is still readable (the "works by luck" case). Once the allocator reuses the chunk for a new allocation, those bytes are overwritten — now you read garbage.
>
> The allocator itself may overwrite the first 8–16 bytes of a freed chunk immediately, to store free-list forward/back pointers. So even if no new `malloc` has happened, the beginning of the freed object may already be corrupted.
>
> This non-determinism is what makes use-after-free bugs so dangerous — they often pass all tests (because the chunk isn't reused during testing) and then crash in production under a different allocation pattern or under ASLR. AddressSanitizer catches them by poisoning freed memory with a special marker and checking every access against a shadow memory map — making them fail immediately and reproducibly.
