# Core C++: Memory, Move Semantics & Smart Pointers

---

## 1. Memory Model: Stack vs Heap, RAII, Ownership Semantics

### Stack vs Heap

| | Stack | Heap |
|---|---|---|
| Allocation | Automatic on scope entry | Manual (`new` / `malloc`) |
| Deallocation | Automatic on scope exit | Manual (`delete` / `free`) |
| Size | Limited (~1–8 MB default) | Large (limited by RAM/VM) |
| Speed | Extremely fast (single SP decrement) | Slower (allocator search + metadata) |
| Lifetime | Tied to enclosing scope | Until explicitly freed |
| Fragmentation | None | Possible over time |
| Thread | Each thread has its own stack | Shared across all threads |
| Access pattern | LIFO, contiguous, cache-friendly | Random, pointer-chased, cache-unfriendly |

---

### What Lives on the Stack

The stack holds everything whose **size is known at compile time** and whose **lifetime is bounded by a scope**.

```
High address
┌─────────────────────────────┐
│   main() frame              │
│   ┌──────────────────────┐  │
│   │ int argc        [4B] │  │  ← function parameters
│   │ char** argv     [8B] │  │
│   └──────────────────────┘  │
│   ┌──────────────────────┐  │
│   │ return address  [8B] │  │  ← where to jump after return
│   │ saved rbp       [8B] │  │  ← previous frame pointer
│   └──────────────────────┘  │
├─────────────────────────────┤
│   foo() frame               │
│   ┌──────────────────────┐  │
│   │ int x = 42      [4B] │  │  ← local variable
│   │ double d        [8B] │  │  ← local variable
│   │ char buf[64]   [64B] │  │  ← fixed-size local array
│   │ Point p {x,y}   [8B] │  │  ← local struct (value type)
│   │ int* ptr        [8B] │  │  ← pointer variable itself (not what it points to)
│   └──────────────────────┘  │
├─────────────────────────────┤  ← stack grows downward
│   (unused stack space)      │
└─────────────────────────────┘
Low address
```

**Stored on the stack:**

```cpp
void foo(int param) {              // param         ← stack (copy of argument)
    int x = 42;                    // x             ← stack
    double d = 3.14;               // d             ← stack
    char buf[64];                  // buf[64]       ← stack (fixed-size array)
    int arr[8] = {1,2,3,4,5,6,7,8};// arr[8]       ← stack
    Point p{1, 2};                 // p             ← stack (value type, all members)

    int* ptr = new int(99);        // ptr           ← stack (the pointer variable)
                                   // *ptr = 99     ← HEAP (what the pointer points to)

    std::string s = "hi";          // s (metadata)  ← stack (ptr+size+capacity or SSO buf)
                                   // s heap buffer ← HEAP (if string > SSO threshold ~15 chars)

    std::vector<int> v = {1,2,3};  // v (metadata)  ← stack (data*+size+capacity, 24 bytes)
                                   // v.data()      ← HEAP (the actual int elements)

    auto lam = [x]() { return x; };// lam           ← stack (captured copy of x)

    delete ptr;
}   // ← ALL stack variables freed automatically here (LIFO order)
```

**Key rule:** The *object itself* (its bytes) lives where it was declared. A `std::vector` declared as a local has its 24-byte metadata on the stack; the elements it manages are on the heap.

---

### What Lives on the Heap

The heap holds everything allocated with **`new`**, **`malloc`**, or indirectly by containers/smart pointers — anything whose **size is only known at runtime** or whose **lifetime must outlive its declaring scope**.

```
┌─────────────────────────────────────────────┐
│                   HEAP                      │
│                                             │
│  [hdr][   int[1000]   ][hdr][Widget][hdr].. │
│         ↑ new int[n]         ↑ make_unique  │
│                                             │
│  [ctrl_blk][  string data  ][  vec elems  ] │
│      ↑ shared_ptr              ↑ vector     │
│                                             │
│  fragmented, non-contiguous, heap metadata  │
│  (size headers) precede each allocation     │
└─────────────────────────────────────────────┘
```

**Stored on the heap:**

```cpp
// 1. Explicit dynamic allocation
int*    p  = new int(42);          // int value lives on heap
int*    arr = new int[n];          // runtime-sized array — must be heap
Widget* w  = new Widget();         // object lives on heap

// 2. Smart pointer managed objects
auto up = std::make_unique<Widget>();   // Widget on heap
auto sp = std::make_shared<Widget>();   // Widget + control block on heap

// 3. STL container elements
std::vector<int> v(1000);         // 1000 ints on heap (v itself on stack)
std::string s = "long string..."; // character buffer on heap (if > SSO)
std::map<int,int> m;              // red-black tree nodes, each on heap

// 4. Objects that must outlive their creating scope
std::unique_ptr<Widget> make_widget() {
    return std::make_unique<Widget>(); // Widget lives on heap → returned to caller
}   // stack frame gone, but heap object survives

// 5. Large objects (avoid stack overflow)
// int big[1000000];              // 4 MB on stack → stack overflow!
auto big = std::make_unique<int[]>(1000000); // 4 MB on heap → fine
```

---

### What Lives in Neither (Static / Global Storage)

```cpp
// Global variables — .data or .bss segment (not stack, not heap)
int g_count = 0;                   // .data  (initialized)
static double g_ratio;             // .bss   (zero-initialized)

// String literals — .rodata segment (read-only)
const char* msg = "hello";         // "hello" is in .rodata, msg ptr is on stack

// Static local variables — .data segment, initialized once
void foo() {
    static int call_count = 0;     // survives across calls, NOT on stack
    ++call_count;
}

// Thread-local storage
thread_local int tls_id = 0;       // one copy per thread, in TLS segment
```

---

### Quick Decision: Stack or Heap?

```
Is the size known at compile time?
├─ No  → heap (new / vector / unique_ptr)
└─ Yes → Does it need to outlive the current scope?
          ├─ Yes → heap (return via smart pointer)
          └─ No  → Is it small enough? (rough guide: < 1–16 KB)
                    ├─ Yes → stack (local variable, struct, small array)
                    └─ No  → heap (large array, big object)
```

```cpp
// PREFER stack when possible — faster, no leak risk, cache-friendly
int result = compute();             // stack — ideal

// USE heap when necessary
auto buf = std::make_unique<char[]>(file_size);  // runtime size → heap
return std::make_unique<Connection>(fd);          // must outlive scope → heap
```

---

### Common Mistakes

```cpp
// 1. Returning pointer/reference to stack variable → dangling
int& bad_ref() {
    int x = 42;
    return x;    // x is destroyed on return → dangling reference → UB
}

// 2. Stack overflow from large local array
void foo() {
    int data[10'000'000]; // ~40 MB on stack → immediate crash
    // Fix: std::vector<int> data(10'000'000);  → heap
}

// 3. Forgetting that container elements are on heap
std::vector<int*> ptrs;
{
    int x = 10;
    ptrs.push_back(&x);   // pushing ADDRESS of a stack variable
}
// x is gone, ptrs[0] is dangling → UB

// 4. Assuming pointer on stack means data on stack
int* p = new int(5);   // p = 8 bytes on stack, *p = 4 bytes on heap
delete p;              // must explicitly free the heap part
```

---

### RAII (Resource Acquisition Is Initialization)

The core C++ idiom: **tie resource lifetime to object lifetime**. Acquire in constructor, release in destructor.

```cpp
class FileHandle {
public:
    explicit FileHandle(const char* path) {
        f_ = fopen(path, "r");
        if (!f_) throw std::runtime_error("failed to open");
    }
    ~FileHandle() { if (f_) fclose(f_); }  // guaranteed cleanup

    // Disable copy to prevent double-close
    FileHandle(const FileHandle&) = delete;
    FileHandle& operator=(const FileHandle&) = delete;

private:
    FILE* f_;
};

void process() {
    FileHandle fh("data.txt"); // acquired
    // ... use fh ...
}   // destructor called here — even if exception thrown
```

**Why it matters**: RAII guarantees cleanup on any exit path — normal return, exception, early return. It eliminates the need for `try/finally`.

Standard library RAII wrappers: `std::unique_ptr`, `std::lock_guard`, `std::ifstream`.

---

### Ownership Semantics

Ownership = who is responsible for freeing a resource.

| Pattern | Meaning |
|---|---|
| **Unique ownership** | Exactly one owner, transfer via move |
| **Shared ownership** | Multiple owners, freed when last owner dies |
| **Borrowed/non-owning** | Observer, does not control lifetime (raw ptr, reference) |

```cpp
// Unique ownership — clear who owns
std::unique_ptr<int> owner = std::make_unique<int>(10);

// Transfer ownership — owner gives it up
std::unique_ptr<int> new_owner = std::move(owner);
// owner is now null

// Borrow — no ownership transfer
void inspect(const int* p);  // raw pointer = non-owning view
inspect(new_owner.get());
```

**Rule of thumb**: raw pointers and references are for *borrowing*, smart pointers are for *owning*.

---

## 2. Shallow Copy vs Deep Copy

### The Core Problem

The **compiler-generated copy** (copy constructor + copy assignment) performs a **memberwise copy** — each data member is copied by value. For pointer members this means copying the *address*, not the *data behind it*. Both objects now share the same heap block.

```
Original:                    Shallow copy:                Deep copy:
┌────────────────┐           ┌────────────────┐           ┌────────────────┐
│ data_ ─────────┼──┐        │ data_ ─────────┼──┐        │ data_ ─────────┼──┐
│ size_ = 3      │  │        │ size_ = 3      │  │        │ size_ = 3      │  │
└────────────────┘  │        └────────────────┘  │        └────────────────┘  │
                    ▼                             │                            ▼
              ┌──────────┐                        │                      ┌──────────┐
              │  1, 2, 3 │◀───────────────────────┘                      │  1, 2, 3 │
              └──────────┘                                               └──────────┘
           SHARED — double free!                                    independent copy
```

### Shallow Copy (compiler default)

```cpp
struct Shallow {
    int* data;
    int  size;
};

Shallow a;
a.data = new int[3]{1, 2, 3};
a.size = 3;

Shallow b = a;        // memberwise copy: b.data == a.data (same pointer!)

b.data[0] = 99;       // silently corrupts a's data too
delete[] b.data;      // frees the block
// a.data is now dangling → UB on next access or double-free on a's dtor
```

### Deep Copy — allocate + copy the resource

```cpp
class Buffer {
public:
    explicit Buffer(int n) : size_(n), data_(new int[n]{}) {}

    // Deep copy constructor
    Buffer(const Buffer& other)
        : size_(other.size_)
        , data_(new int[other.size_])           // new allocation
    {
        std::copy(other.data_, other.data_ + size_, data_);
    }

    // Copy assignment — copy-and-swap idiom (exception-safe)
    Buffer& operator=(Buffer other) noexcept {  // 'other' is already a deep copy
        swap(*this, other);                     // exchange internals
        return *this;                           // old data freed by other's dtor
    }

    // Move constructor — transfer ownership, no allocation
    Buffer(Buffer&& other) noexcept
        : size_(std::exchange(other.size_, 0))
        , data_(std::exchange(other.data_, nullptr))
    {}

    // Move assignment
    Buffer& operator=(Buffer&& other) noexcept {
        if (this != &other) {
            delete[] data_;
            data_ = std::exchange(other.data_, nullptr);
            size_ = std::exchange(other.size_, 0);
        }
        return *this;
    }

    ~Buffer() { delete[] data_; }

    friend void swap(Buffer& a, Buffer& b) noexcept {
        std::swap(a.size_, b.size_);
        std::swap(a.data_, b.data_);
    }

private:
    int  size_;
    int* data_;
};

Buffer a(3);       // [1, 2, 3] on heap
Buffer b = a;      // deep copy → b has its own [1, 2, 3]
b.data_[0] = 99;   // a is unaffected
// both dtors run safely — no double-free
```

### When Shallow Copy Is Correct

```cpp
// 1. Non-owning / observer types — shallow IS the right semantic
struct StringView {
    const char* data;   // borrows, does not own
    int         size;
};
// Copying a view copies the "window", not the underlying string — correct.

// 2. Value types with no heap resources — shallow == deep
struct Point { int x, y; };
Point p2 = p1;   // copies x and y — nothing to deep-copy

// 3. STL containers already deep copy for you
std::vector<int> v1 = {1, 2, 3};
std::vector<int> v2 = v1;    // deep copy — v2 owns its own buffer
v2[0] = 99;                  // v1[0] still 1

std::string s1 = "hello";
std::string s2 = s1;         // deep copy (SSO or heap)
```

### Copy vs Move

```cpp
Buffer a(1024);

Buffer b = a;            // COPY — new allocation + memcpy (O(n))
Buffer c = std::move(a); // MOVE — pointer swap, no allocation (O(1))
                         // a is now empty (data_ = nullptr, size_ = 0)
```

| | Shallow Copy | Deep Copy | Move |
|---|---|---|---|
| Allocates memory | No | Yes | No |
| Independent after? | No — shared | Yes | Yes |
| Cost | O(1) | O(n) | O(1) |
| Source still valid? | Yes | Yes | Valid but empty |
| Risk | Double-free | None | Don't use moved-from |

### Decision Rule

```
Does your type own a resource (heap, fd, socket, mutex)?
│
├─ No  → Rule of Zero: compiler-generated copy is correct
│
└─ Yes → Option A: Rule of Five
│         write: deep copy ctor + copy assign + move ctor + move assign + dtor
│
         Option B: Rule of Zero + RAII member (preferred)
          store resource in vector/string/unique_ptr — they handle it for you
```

### Copy-and-Swap Idiom Explained

```cpp
// copy assignment via copy-and-swap:
Buffer& operator=(Buffer other) noexcept {  // 1. 'other' is deep-copied by value
    swap(*this, other);                     // 2. swap internals
    return *this;                           // 3. other (now holding old data) is destroyed
}

// Benefits:
// - Exception safe: if the copy (step 1) throws, *this is unchanged
// - Self-assignment safe: copy of self is made, then swapped — correct
// - No code duplication: reuses copy ctor + dtor
```

---

## 3. Move Semantics: `std::move`, Rvalue References, Move Constructor/Assignment

### Lvalue vs Rvalue

- **Lvalue**: has a name/address, persists beyond the expression (`int x`)
- **Rvalue**: temporary, no persistent address (`42`, `std::string("tmp")`, function return value)

```cpp
int x = 5;       // x is lvalue
int&& r = 5;     // 5 is rvalue; r binds to it
// int&& r2 = x; // ERROR — x is lvalue
```

---

### Rvalue References (`&&`)

Enable functions to detect when they receive a temporary (disposable) value — and *steal* its resources instead of copying.

```cpp
void process(std::string& s)  {}  // lvalue overload — copy expected
void process(std::string&& s) {}  // rvalue overload — can steal from s
```

---

### `std::move`

`std::move` is just a **cast** — it converts an lvalue into an rvalue reference. It does not move anything by itself. The actual move happens in the move constructor/assignment.

```cpp
std::string a = "hello";
std::string b = std::move(a);  // cast a to rvalue, triggers move ctor
// a is now in valid but unspecified state (likely empty)
// b owns the original buffer — no heap allocation
```

**Warning**: After `std::move(x)`, do not use `x` unless you reset it first.

---

### Move Constructor & Move Assignment

```cpp
class Buffer {
public:
    explicit Buffer(size_t n) : data_(new int[n]), size_(n) {}

    // Move constructor — steal resources from other
    Buffer(Buffer&& other) noexcept
        : data_(other.data_), size_(other.size_) {
        other.data_ = nullptr;  // leave other in valid state
        other.size_ = 0;
    }

    // Move assignment
    Buffer& operator=(Buffer&& other) noexcept {
        if (this != &other) {
            delete[] data_;         // free current resources
            data_ = other.data_;
            size_ = other.size_;
            other.data_ = nullptr;
            other.size_ = 0;
        }
        return *this;
    }

    ~Buffer() { delete[] data_; }

private:
    int* data_;
    size_t size_;
};
```

**Mark move operations `noexcept`** — STL containers (e.g. `vector` resize) will only use your move constructor if it's `noexcept`, otherwise they fall back to copy.

---

### When Compiler Generates Move Operations

- Auto-generated if you don't declare copy operations, destructor, or other move operations.
- Declaring any of {copy ctor, copy assign, destructor} suppresses auto-generated move → **Rule of 5**: if you declare one, declare all five.

---

### Perfect Forwarding

Preserve lvalue/rvalue category when forwarding arguments:

```cpp
template<typename T>
void wrapper(T&& arg) {
    target(std::forward<T>(arg));  // forwards as lvalue if T=lvalue-ref, rvalue otherwise
}
```

---

## 4. Smart Pointers: `unique_ptr`, `shared_ptr`, `weak_ptr`

### `std::unique_ptr` — Single Ownership

- Zero overhead vs raw pointer (no ref count)
- Cannot be copied, only moved
- Use as the **default owning pointer**

```cpp
auto p = std::make_unique<Widget>(args...);  // prefer make_unique

// Transfer ownership
auto p2 = std::move(p);   // p is now null
p2->doWork();

// Pass to function — borrow (no ownership transfer)
void use(Widget* w);
use(p2.get());

// Pass ownership into function
void take(std::unique_ptr<Widget> w);
take(std::move(p2));
```

---

### `std::shared_ptr` — Shared Ownership

Every `shared_ptr<T>` instance is two pointers: one to the **managed object**, one to a **control block** that lives on the heap and governs the object's lifetime.

#### What the Control Block Stores

```
┌─────────────────────────────────────────────────────┐
│                   Control Block                     │
│                                                     │
│  ┌─────────────────────────────────────────────┐    │
│  │  use_count   : atomic<long>                 │    │  ← # of shared_ptr instances
│  │  weak_count  : atomic<long>                 │    │  ← # of weak_ptr + 1
│  ├─────────────────────────────────────────────┤    │
│  │  deleter     : type-erased callable         │    │  ← how to destroy the object
│  │  allocator   : type-erased allocator        │    │  ← how to free this block
│  ├─────────────────────────────────────────────┤    │
│  │  ptr to T    : T*  (or T embedded here)     │    │  ← points to managed object
│  └─────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────┘
```

| Field | Type | Purpose |
|---|---|---|
| `use_count` | `atomic<long>` | Counts live `shared_ptr` copies. Object destroyed when hits 0 |
| `weak_count` | `atomic<long>` | Counts `weak_ptr` + 1 sentinel. Control block freed when hits 0 |
| `deleter` | type-erased callable | Custom destructor/free function — stored even if default |
| `allocator` | type-erased allocator | How to free the control block itself |
| object ptr | `T*` | Pointer back to managed object (or object embedded inline with `make_shared`) |

#### Two Separate Counters — Why?

```
use_count  controls: when to DESTROY the object
weak_count controls: when to FREE the control block

Timeline:
  shared_ptr created   → use_count=1, weak_count=1 (the +1 sentinel)
  shared_ptr copied    → use_count++
  weak_ptr created     → weak_count++
  shared_ptr destroyed → use_count--
                          if use_count == 0 → destroy object (call destructor + deleter)
                          weak_count--        (remove the sentinel)
  weak_ptr destroyed   → weak_count--
                          if weak_count == 0 → free control block memory
```

```cpp
auto sp1 = std::make_shared<int>(42);
// use_count=1, weak_count=1

auto sp2 = sp1;
// use_count=2, weak_count=1

std::weak_ptr<int> wp = sp1;
// use_count=2, weak_count=2

sp1.reset();
// use_count=1, weak_count=2

sp2.reset();
// use_count=0 → *int destroyed
// weak_count=1 (sentinel removed) → weak_count=1

// wp still alive → control block still alive (weak_count=1)
wp.expired();  // true — object gone, block survives

// wp goes out of scope → weak_count=0 → control block freed
```

#### Deleter — Type Erasure

The deleter is stored in the control block with **type erasure** — `shared_ptr<T>` can hold any callable as deleter without changing the template type.

```cpp
// Default deleter: calls delete
auto sp1 = std::make_shared<Widget>();
// control block stores: [](Widget* p){ delete p; }

// Custom deleter: calls fclose
std::shared_ptr<FILE> fp(fopen("x.txt", "r"), fclose);
// control block stores: fclose (function pointer)

// Lambda deleter
auto pool_return = [&pool](Widget* p){ pool.release(p); };
std::shared_ptr<Widget> sp2(pool.acquire(), pool_return);
// control block stores: the lambda (with captured pool reference)

// Array deleter (pre-C++17)
std::shared_ptr<int> arr(new int[10], [](int* p){ delete[] p; });

// All three are shared_ptr<T> — deleter type is erased into the control block
// The type of shared_ptr<T> does NOT change based on deleter type
// (unlike unique_ptr<T, Deleter> which encodes deleter in the template)
```

#### `make_shared` vs `shared_ptr(new T)` — Memory Layout

```
shared_ptr<Widget>(new Widget):         make_shared<Widget>():
Two separate allocations:               One contiguous allocation:

  Heap alloc 1:                           Heap alloc (single):
  ┌──────────────┐                        ┌──────────────────────────┐
  │  Widget data │◄──── sp.ptr_           │  Control Block           │
  └──────────────┘                        │  ┌────────────────────┐  │
                                          │  │ use_count          │  │
  Heap alloc 2:                           │  │ weak_count         │  │
  ┌──────────────────────┐                │  │ deleter            │  │
  │  Control Block       │◄── sp.ctrl_   │  │ allocator          │  │
  │  use_count           │               │  └────────────────────┘  │
  │  weak_count          │               │  Widget data             │◄── sp.ptr_
  │  deleter             │               │  (embedded, adjacent)    │
  │  allocator           │               └──────────────────────────┘
  └──────────────────────┘◄── sp.ctrl_

  Cache miss likely between              Object and control block
  object and control block               always in same cache line
```

| | `shared_ptr(new T)` | `make_shared<T>()` |
|---|---|---|
| Allocations | 2 (object + control block) | 1 (fused) |
| Cache locality | Poor (two separate blocks) | Good (adjacent) |
| Exception safety | Risky (see below) | Safe |
| Object freed when | `use_count == 0` | `weak_count == 0` (fused memory) |
| Custom deleter | Yes | No (uses `delete`) |
| Private constructor | Works with `new` | Needs friend or passkey |

**Exception safety pitfall with `new`:**

```cpp
// UNSAFE (pre-C++17 evaluation order)
f(shared_ptr<Widget>(new Widget), compute_priority());
// Possible order: new Widget → compute_priority() [throws] → shared_ptr ctor
// Result: Widget leaked because shared_ptr was never constructed

// SAFE: make_shared is atomic — no gap between allocation and ownership
f(make_shared<Widget>(), compute_priority());
```

#### `shared_ptr` Thread Safety

```cpp
// The CONTROL BLOCK is thread-safe (atomic counters)
// The POINTED-TO OBJECT is NOT automatically thread-safe

std::shared_ptr<int> sp = std::make_shared<int>(0);

// SAFE: copying shared_ptr across threads (use_count is atomic)
std::shared_ptr<int> sp2 = sp;   // thread 1
std::shared_ptr<int> sp3 = sp;   // thread 2 — safe, atomic increment

// UNSAFE: two threads modifying the same shared_ptr variable
// Thread 1: sp = other;    // not atomic on the pointer pair
// Thread 2: sp.reset();    // data race on sp itself

// Fix: use atomic<shared_ptr<T>> (C++20)
std::atomic<std::shared_ptr<int>> atomic_sp = std::make_shared<int>(0);
atomic_sp.store(std::make_shared<int>(42));  // lock-free on most platforms

// Fix (pre-C++20): protect sp itself with a mutex
std::mutex mu;
{
    std::lock_guard lock(mu);
    sp = other;
}
```

---

### `std::weak_ptr` — Non-Owning Observer

- Does not affect ref count
- Must be converted to `shared_ptr` via `.lock()` before use (returns null if object is gone)
- Primary use: **break circular references**

```cpp
std::weak_ptr<Widget> wp;
{
    auto sp = std::make_shared<Widget>();
    wp = sp;  // weak ref, ref count still 1
}
// sp out of scope — Widget destroyed

if (auto locked = wp.lock()) {
    locked->doWork();  // safe
} else {
    // object is gone
}
```

---

### Circular Reference Problem

#### What Is a Circular Reference?

A **circular reference** (also called a **reference cycle**) occurs when two or more objects hold `shared_ptr` to each other, directly or indirectly, forming a closed loop of ownership. Because `shared_ptr` only destroys an object when its `use_count` reaches zero, and each object in the cycle keeps the other's count above zero, **neither object is ever destroyed** — even when no external code holds a reference to either one.

This is a **memory leak that the compiler cannot detect** and that sanitizers may not always catch.

#### How the Cycle Traps the Count

```
Without cycle — normal cleanup:

  external: sp_a ──────────────────► [Node A]  use_count = 1
                                        │
                                      next ──► [Node B]  use_count = 1

  sp_a goes out of scope:
    Node A: use_count 1 → 0  → destroyed → releases next → Node B: use_count 1 → 0 → destroyed ✓


With cycle — leak:

  external: sp_a ──► [Node A]  use_count = 2  (sp_a + B's prev)
                        │  ▲
                      next │ prev (strong)
                        │  │
                        ▼  │
                     [Node B]  use_count = 2  (sp_b + A's next)
                        ▲
  external: sp_b ───────┘

  sp_a goes out of scope: Node A use_count 2 → 1  (B still holds it) — NOT destroyed
  sp_b goes out of scope: Node B use_count 2 → 1  (A still holds it) — NOT destroyed

  Both nodes are now UNREACHABLE from outside but use_count = 1 ≠ 0
  → neither destructor runs → memory leaked forever
```

#### Code: The Leak

```cpp
struct Node {
    std::string name;
    std::shared_ptr<Node> next;   // strong — owns next node
    std::shared_ptr<Node> prev;   // strong — PROBLEM: owns previous node too
    ~Node() { std::cout << name << " destroyed\n"; }
};

{
    auto a = std::make_shared<Node>("A");
    auto b = std::make_shared<Node>("B");

    a->next = b;   // A holds B  → B use_count = 2
    b->prev = a;   // B holds A  → A use_count = 2
}
// scope ends: local sp's release → use_count drops to 1 each
// "A destroyed" — NEVER printed
// "B destroyed" — NEVER printed
// Both objects leak until process exits
```

#### Why `weak_ptr` Breaks the Cycle

`weak_ptr` observes an object **without participating in ownership** — it does not increment `use_count`. Breaking just one link in the cycle with `weak_ptr` is enough to allow cleanup.

```
Ownership direction (strong links only) — must be acyclic:

  [Node A] ──next (strong)──► [Node B]
  [Node B] ──prev (weak)────► [Node A]   ← no ownership, just navigation

  sp_a / sp_b go out of scope:
    Node A: use_count 1 → 0 → destroyed → releases next
    Node B: use_count 1 → 0 → destroyed ✓
    Both destructors run. No leak.
```

```cpp
struct Node {
    std::string name;
    std::shared_ptr<Node> next;   // owns the next node (strong)
    std::weak_ptr<Node>   prev;   // observes the previous node (weak — no ownership)
    ~Node() { std::cout << name << " destroyed\n"; }
};

{
    auto a = std::make_shared<Node>("A");
    auto b = std::make_shared<Node>("B");

    a->next = b;   // A owns B  → B use_count = 2
    b->prev = a;   // weak — A use_count stays 1
}
// scope ends:
// A use_count 1 → 0 → "A destroyed" → a->next released
// B use_count 2 → 1 → 0 → "B destroyed" ✓

// To navigate backwards via weak_ptr:
if (auto parent = b->prev.lock()) {   // returns shared_ptr or empty
    std::cout << parent->name;
}
```

#### Real-World Patterns That Cause Cycles

| Pattern | Cycle | Fix |
|---|---|---|
| Doubly-linked list | `next` + `prev` both strong | `prev` → `weak_ptr` |
| Parent ↔ child tree | parent owns children, children hold parent | child's `parent` → `weak_ptr` |
| Observer ↔ subject | subject owns observers, observers hold subject | observer's subject ref → `weak_ptr` |
| Callback with capture | object stores callback that captures `shared_ptr` to itself | capture `weak_ptr`, lock inside callback |
| `enable_shared_from_this` misuse | calling `shared_from_this()` and storing result in member | store as `weak_ptr` |

#### Indirect / Long Cycles

Cycles do not have to be direct (A→B→A). They can span many objects:

```
A → B → C → D → A   (4-node cycle — all leak)
```

Any closed loop in the ownership graph causes the same problem. The rule: **the ownership graph must be a DAG** (Directed Acyclic Graph) — no cycles.

#### Detecting Leaks from Cycles

```bash
# Valgrind reports "definitely lost" or "still reachable"
valgrind --leak-check=full --track-origins=yes ./app

# AddressSanitizer (ASan) — compile with:
g++ -fsanitize=address -g ./app.cpp -o app && ./app

# LeakSanitizer (LSan) — compile with:
g++ -fsanitize=leak -g ./app.cpp -o app && ./app
# LSan output: "Direct leak of N byte(s) in M object(s)"
```

Note: LSan detects that memory was not freed but cannot tell you *why* (cycle vs plain forget). Add `~Node()` print or logging to confirm destructor is never called.

#### Summary

```
Cycle rule:
  shared_ptr  = "I own this, I keep it alive"  → use_count++
  weak_ptr    = "I know about this, not responsible for it" → use_count unchanged

  If A owns B and B owns A (directly or transitively) → cycle → leak

Break the cycle:
  Identify the "back edge" — the reverse/non-primary reference
  Change it from shared_ptr → weak_ptr
  Ownership must flow in one direction (parent → child, subject → observer)
```

---

### Quick Decision Guide

```
Need ownership?
├── No  → raw pointer or reference (borrow/observe)
└── Yes
    ├── Single owner? → unique_ptr  (default choice)
    └── Shared ownership needed?
        ├── Yes → shared_ptr
        └── Need to observe shared object without owning? → weak_ptr
```

---

### Overhead Comparison

| | `unique_ptr` | `shared_ptr` | `weak_ptr` |
|---|---|---|---|
| Size | 1 pointer | 2 pointers (ptr + control block ptr) | 2 pointers |
| Copy cost | Not allowed | Atomic increment | Atomic increment |
| Destroy cost | `delete` | Atomic decrement + conditional `delete` | Atomic decrement |
| Extra allocation | None | Control block (unless `make_shared`) | None |

---

## Mock Interview Questions

### Memory Model / RAII

---

**[Mid]** What is RAII and why is it preferred over manual `try/finally` cleanup?

> RAII ties resource lifetime to object lifetime — the destructor always runs when the object goes out of scope, regardless of how the scope exits (normal return, exception, early return). C++ has no `finally` keyword; RAII replaces it entirely. The advantage over `try/finally` is that cleanup logic lives with the resource definition, not scattered at every call site. You can't forget to call `fclose` if the destructor does it for you.

---

**[Mid]** What happens to stack-allocated objects when an exception is thrown?

> The runtime performs **stack unwinding**: it walks back up the call stack and destroys every local object in reverse order of construction, calling their destructors. This is guaranteed by the standard and is the mechanism that makes RAII work under exceptions. Note: if a destructor itself throws during unwinding, `std::terminate` is called — this is why destructors should never throw.

---

**[Mid]** Why is it dangerous to store a pointer to a stack variable and return it from a function?

> The stack frame is destroyed when the function returns, so the pointer immediately becomes a **dangling pointer** — it points to memory that may be reused for the next function call. Dereferencing it is undefined behavior, and the bug is often silent because the memory content may look valid briefly.
>
> ```cpp
> int* dangerous() {
>     int x = 42;
>     return &x;  // UB — x is gone after return
> }
> ```
>
> Fix: return by value, or allocate on the heap (via smart pointer).

---

**[Senior]** You have a legacy C API that returns a `FILE*`. Design a RAII wrapper that is movable but not copyable. What are the implications of each design choice?

> **Not copyable**: copying a `FILE*` handle is meaningless — two objects would try to `fclose` the same handle, causing a double-close (undefined behavior). So we `= delete` copy constructor and copy assignment.
>
> **Movable**: allows transferring ownership (e.g. storing in a container or returning from a factory). Move sets the source pointer to `nullptr` so its destructor becomes a no-op.
>
> ```cpp
> class FileHandle {
> public:
>     explicit FileHandle(const char* path, const char* mode) {
>         f_ = fopen(path, mode);
>         if (!f_) throw std::runtime_error("cannot open file");
>     }
>
>     ~FileHandle() { if (f_) fclose(f_); }
>
>     FileHandle(const FileHandle&)            = delete;
>     FileHandle& operator=(const FileHandle&) = delete;
>
>     FileHandle(FileHandle&& other) noexcept : f_(other.f_) {
>         other.f_ = nullptr;
>     }
>
>     FileHandle& operator=(FileHandle&& other) noexcept {
>         if (this != &other) {
>             if (f_) fclose(f_);
>             f_ = other.f_;
>             other.f_ = nullptr;
>         }
>         return *this;
>     }
>
>     FILE* get() const { return f_; }
>
> private:
>     FILE* f_ = nullptr;
> };
> ```
>
> Design implications: making it non-copyable forces callers to be explicit about ownership; making it movable allows use in STL containers and factory patterns.

---

**[Senior]** Explain the difference between stack unwinding and termination. When does `std::terminate` get called during exception handling?

> **Stack unwinding** is the normal exception propagation process: when an exception is thrown, the runtime walks the call stack upward, destroying local objects (calling destructors) until it finds a matching `catch` block.
>
> **`std::terminate`** is called — bypassing further unwinding — in these cases:
> - An exception is thrown and no matching `catch` exists anywhere (uncaught exception)
> - A destructor throws during stack unwinding (double exception)
> - A `noexcept` function throws
> - `std::rethrow_exception` is called with a null exception pointer
> - Some threading violations (e.g. joining an un-joinable thread)
>
> The key distinction: terminate skips remaining destructors, so resources may not be cleaned up. This is why destructors must be `noexcept`.

---

### Move Semantics

---

**[Mid]** What does `std::move` actually do at the machine level?

> Nothing at runtime — it compiles to zero instructions. `std::move` is purely a **compile-time cast** from `T&` to `T&&`. It signals to the compiler "treat this as a temporary so you may select the move overload." The actual resource transfer happens in whichever move constructor or move assignment operator gets called as a result.

---

**[Mid]** Why should move constructors be marked `noexcept`? What breaks if they're not?

> `std::vector` must maintain a strong exception guarantee during reallocation (if reallocation fails, the original vector must remain intact). To do this safely it can only use your move constructor if it's guaranteed not to throw — otherwise it falls back to **copy** to preserve the original elements if something goes wrong mid-transfer. So a move constructor without `noexcept` silently degrades performance back to O(n) copies on every `push_back` that triggers reallocation.
>
> ```cpp
> // Without noexcept — vector uses copy on realloc
> MyType(MyType&&) { ... }
>
> // With noexcept — vector uses move on realloc (fast)
> MyType(MyType&&) noexcept { ... }
> ```

---

**[Mid]** After `std::move(x)`, is `x` valid? Can you use it?

> `x` is in a **valid but unspecified state** — the standard guarantees it can be safely destroyed or reassigned, but you cannot rely on its value. Reading from it is technically allowed but logically wrong. The safe pattern is to only reassign after moving:
>
> ```cpp
> std::string s = "hello";
> std::string t = std::move(s);
> // s is valid but likely empty — don't read it
> s = "reset";  // safe to use again after reassignment
> ```

---

**[Senior]** Implement a move-only type that wraps a raw socket file descriptor. Show the move constructor, move assignment, and destructor. Handle self-assignment.

> ```cpp
> class Socket {
> public:
>     explicit Socket(int fd) : fd_(fd) {}
>
>     ~Socket() {
>         if (fd_ != -1) close(fd_);
>     }
>
>     Socket(const Socket&)            = delete;
>     Socket& operator=(const Socket&) = delete;
>
>     Socket(Socket&& other) noexcept : fd_(other.fd_) {
>         other.fd_ = -1;  // sentinel — destructor becomes no-op
>     }
>
>     Socket& operator=(Socket&& other) noexcept {
>         if (this != &other) {          // guard against self-assignment
>             if (fd_ != -1) close(fd_); // release current resource
>             fd_ = other.fd_;
>             other.fd_ = -1;
>         }
>         return *this;
>     }
>
>     int get() const { return fd_; }
>
> private:
>     int fd_ = -1;
> };
> ```
>
> Key decisions: `-1` as sentinel (POSIX convention for invalid fd), `noexcept` on both move operations, self-assignment check in move assignment to avoid closing then using the same fd.

---

**[Senior]** Explain the difference between `T&&` as a plain parameter and `T&&` in a template. What is a forwarding/universal reference?

> - **Plain `T&&`** (e.g. `void f(std::string&& s)`) is a true rvalue reference — it only binds to rvalues. `T` is fixed.
> - **Template `T&&`** (e.g. `template<typename T> void f(T&& x)`) is a **forwarding reference** (also called universal reference). Here `T` is deduced, so reference collapsing applies:
>   - Pass an lvalue of type `X` → `T` deduces to `X&`, so `T&&` = `X& &&` = `X&` (lvalue ref)
>   - Pass an rvalue of type `X` → `T` deduces to `X`, so `T&&` = `X&&` (rvalue ref)
>
> This is why `std::forward<T>(x)` works: it casts back to lvalue or rvalue depending on what `T` was deduced as, preserving the original value category for the next call.
>
> ```cpp
> template<typename T>
> void wrapper(T&& x) {
>     // T&& is forwarding ref — x could be lvalue or rvalue
>     target(std::forward<T>(x));  // correct: preserves category
>     // target(std::move(x));     // wrong: always moves, even lvalues
> }
> ```

---

**[Senior]** What is copy elision (RVO/NRVO)? When is it guaranteed in C++17? How does it interact with move semantics?

> **Copy elision** means the compiler constructs the return value directly in the caller's storage — no copy or move happens at all.
>
> - **RVO (Return Value Optimization)**: returning a temporary (prvalue) — guaranteed in C++17. The compiler is *required* to elide, even if the copy/move constructor has side effects.
> - **NRVO (Named RVO)**: returning a named local variable — still optional (not guaranteed), but all major compilers do it in optimized builds.
>
> ```cpp
> std::string make() {
>     return std::string("hello");  // RVO — guaranteed in C++17, zero copies
> }
>
> std::string make2() {
>     std::string s = "hello";
>     return s;  // NRVO — likely elided, but not guaranteed
> }
> ```
>
> **Interaction with move semantics**: when NRVO can't apply (e.g. multiple return paths), the compiler automatically treats the named return as an rvalue and uses the move constructor — you don't need to write `return std::move(s)`. In fact, writing `return std::move(s)` explicitly can *disable* NRVO by turning the return value into an rvalue reference, so avoid it.

---

### Smart Pointers

---

**[Mid]** When would you choose `shared_ptr` over `unique_ptr`? What is the cost?

> Use `shared_ptr` only when **multiple independent owners genuinely need to share lifetime** — e.g. a cache that hands out references, or a graph node pointed to by multiple edges. If you can identify a single clear owner, prefer `unique_ptr`.
>
> Costs of `shared_ptr`:
> - **Extra allocation**: control block (unless `make_shared`, which co-allocates)
> - **Atomic ref count**: increment/decrement on every copy/destroy — significant on multi-core with high contention
> - **Size**: two pointers instead of one
> - **Object lifetime is less predictable**: destruction happens at last owner's death, which may be far from where the object was logically "done"

---

**[Mid]** What is the danger of constructing a `shared_ptr` from a raw pointer multiple times?

> Each `shared_ptr` constructed from a raw pointer creates an **independent control block** with its own ref count starting at 1. When either goes to zero, it `delete`s the object — then the second one also tries to `delete` the same address → **double free / undefined behavior**.
>
> ```cpp
> Widget* raw = new Widget();
> shared_ptr<Widget> p1(raw);
> shared_ptr<Widget> p2(raw);  // WRONG — two separate ref counts
> // when p1 dies: delete raw
> // when p2 dies: delete raw again → crash
> ```
>
> Fix: copy/assign from `p1` (shares the same control block), or use `make_shared` from the start so a raw pointer never escapes.

---

**[Mid]** Why use `make_shared` instead of `shared_ptr<T>(new T(...))`?

> Three reasons:
> 1. **Single allocation**: `make_shared` allocates the object and control block together in one block. `shared_ptr(new T)` does two allocations — one for `T`, one for the control block.
> 2. **Exception safety**: in `f(shared_ptr<T>(new T()), g())`, the compiler may evaluate `new T` then `g()` before constructing `shared_ptr` — if `g()` throws, `new T` leaks. `make_shared` wraps the allocation atomically.
> 3. **Cache locality**: object and ref count are adjacent in memory.
>
> Trade-off: with `make_shared`, the memory for `T` is not freed until **both** the strong count and weak count reach zero (because they share one block). With `shared_ptr(new T)`, `T`'s memory is freed when the strong count hits zero even if weak refs remain.

---

**[Senior]** Implement a simplified `shared_ptr` with ref counting. Include copy constructor, copy assignment, move constructor, and destructor.

> ```cpp
> template<typename T>
> class SharedPtr {
> public:
>     explicit SharedPtr(T* ptr = nullptr)
>         : ptr_(ptr), count_(ptr ? new int(1) : nullptr) {}
>
>     ~SharedPtr() { release(); }
>
>     // Copy — share ownership
>     SharedPtr(const SharedPtr& other) : ptr_(other.ptr_), count_(other.count_) {
>         if (count_) ++(*count_);
>     }
>
>     SharedPtr& operator=(const SharedPtr& other) {
>         if (this != &other) {
>             release();
>             ptr_   = other.ptr_;
>             count_ = other.count_;
>             if (count_) ++(*count_);
>         }
>         return *this;
>     }
>
>     // Move — transfer ownership, no ref count change
>     SharedPtr(SharedPtr&& other) noexcept : ptr_(other.ptr_), count_(other.count_) {
>         other.ptr_   = nullptr;
>         other.count_ = nullptr;
>     }
>
>     SharedPtr& operator=(SharedPtr&& other) noexcept {
>         if (this != &other) {
>             release();
>             ptr_   = other.ptr_;
>             count_ = other.count_;
>             other.ptr_   = nullptr;
>             other.count_ = nullptr;
>         }
>         return *this;
>     }
>
>     T* get()        const { return ptr_; }
>     T& operator*()  const { return *ptr_; }
>     T* operator->() const { return ptr_; }
>     int use_count() const { return count_ ? *count_ : 0; }
>
> private:
>     void release() {
>         if (count_ && --(*count_) == 0) {
>             delete ptr_;
>             delete count_;
>         }
>         ptr_   = nullptr;
>         count_ = nullptr;
>     }
>
>     T*   ptr_   = nullptr;
>     int* count_ = nullptr;
> };
> ```
>
> Note: this is a teaching implementation. Real `shared_ptr` uses an atomic ref count, a separate control block with deleter/allocator, and supports `weak_ptr` via a second count.

---

**[Senior]** Explain how `std::enable_shared_from_this` works and when you need it. What goes wrong if you use `shared_ptr<T>(this)` inside a member function?

> **The problem**: if you do `shared_ptr<T>(this)` inside a member function, you create a *new* control block with an independent ref count of 1. When this new `shared_ptr` dies, it deletes `this` — even though other `shared_ptr`s still own the object. Double-free follows.
>
> **`enable_shared_from_this`** fixes this by storing a `weak_ptr` to `this` inside the object at construction time (the constructor of `shared_ptr` detects the base class and initializes it). `shared_from_this()` then calls `.lock()` on that stored `weak_ptr`, returning a `shared_ptr` that shares the *existing* control block.
>
> ```cpp
> class Worker : public std::enable_shared_from_this<Worker> {
> public:
>     void schedule() {
>         // WRONG: shared_ptr<Worker>(this) — new control block → double free
>         // CORRECT:
>         auto self = shared_from_this();  // shares existing control block
>         thread_pool.post([self]() { self->run(); });
>     }
> private:
>     void run() { /* ... */ }
> };
>
> auto w = std::make_shared<Worker>(); // weak_ptr inside initialized here
> w->schedule();
> ```
>
> **Requirement**: `shared_from_this()` can only be called if the object is already managed by a `shared_ptr`. Calling it on a stack-allocated object throws `std::bad_weak_ptr`.

---

**[Senior]** You have a tree structure where children hold `shared_ptr` to parent and parent holds `shared_ptr` to children. Identify the problem and redesign ownership correctly.

> **Problem**: circular ownership — parent keeps children alive, children keep parent alive. Ref counts never reach zero even after the tree is logically unreachable → **memory leak**.
>
> **Correct ownership design**: the parent *owns* its children (children can't outlive the parent), so children do not own the parent back. Children only need to *navigate* to the parent, not keep it alive.
>
> ```cpp
> struct Node {
>     std::vector<std::shared_ptr<Node>> children;  // parent owns children (strong)
>     std::weak_ptr<Node> parent;                   // back-ref, non-owning (weak)
>
>     void addChild(std::shared_ptr<Node> child) {
>         child->parent = shared_from_this();  // requires enable_shared_from_this
>         children.push_back(std::move(child));
>     }
> };
>
> struct Tree : std::enable_shared_from_this<Tree> { ... };
> ```
>
> General rule: **ownership flows in one direction** (parent → child). Any reverse reference (child → parent, observer → subject) should be `weak_ptr`. This prevents cycles while still allowing navigation.
