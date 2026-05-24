# Smart Pointers Internals

## Overview

| Smart Pointer | Ownership | Overhead | Copyable | Use Case |
|---|---|---|---|---|
| `unique_ptr<T>` | Exclusive | Zero (same as raw ptr) | No (move only) | Single owner |
| `shared_ptr<T>` | Shared | Control block (2× atomic) | Yes | Shared ownership |
| `weak_ptr<T>` | Non-owning observer | Shares control block | Yes | Break cycles, cache |
| `scoped_ptr` (Boost) | Exclusive | Zero | No | Non-movable RAII |

---

## 1. `unique_ptr` Internals

### Memory Layout

```
Stack frame:
┌──────────────────────┐
│  unique_ptr<T>       │
│  ┌────────────────┐  │
│  │  T* ptr_       │  │  ← 8 bytes (same as raw pointer)
│  └────────────────┘  │
│  [deleter — EBO]     │  ← 0 bytes if stateless deleter (default_delete)
└──────────────────────┘
         │
         ▼ (heap)
┌─────────────────┐
│   T object      │
└─────────────────┘
```

**Empty Base Optimization (EBO)**: `unique_ptr` stores `{ptr, deleter}` as a compressed pair. Since `default_delete` is a stateless struct (0 bytes), EBO makes `unique_ptr<T>` exactly 8 bytes — same as `T*`.

```cpp
// sizeof proof
static_assert(sizeof(std::unique_ptr<int>) == sizeof(int*));

// With stateful lambda deleter — NOT zero overhead
auto deleter = [fd = 42](int* p) { close(fd); delete p; };
std::unique_ptr<int, decltype(deleter)> up(new int{1}, deleter);
sizeof(up);  // 8 (ptr) + sizeof(lambda) = 16+
```

### Custom Deleters

```cpp
// Function pointer deleter (8 bytes overhead for ptr)
void file_closer(FILE* f) { fclose(f); }
std::unique_ptr<FILE, decltype(&file_closer)> fp(fopen("x.txt","r"), file_closer);

// Struct deleter (0 bytes via EBO if stateless)
struct FreeDeleter {
    void operator()(void* p) const { std::free(p); }
};
std::unique_ptr<int, FreeDeleter> buf(
    static_cast<int*>(std::malloc(sizeof(int))));

// For C APIs — common pattern
struct SDL_SurfaceDeleter {
    void operator()(SDL_Surface* s) const { SDL_FreeSurface(s); }
};
using SurfacePtr = std::unique_ptr<SDL_Surface, SDL_SurfaceDeleter>;
```

### Move Semantics

```cpp
auto p1 = std::make_unique<int>(42);
auto p2 = std::move(p1);   // p1 = nullptr, p2 owns
// p1.get() == nullptr after move

// Return — NRVO or implicit move
std::unique_ptr<Widget> make_widget() {
    return std::make_unique<Widget>(); // no std::move needed
}

// Into container
std::vector<std::unique_ptr<Base>> v;
v.push_back(std::make_unique<Derived>());  // move into vector

// Transfer to shared_ptr
std::shared_ptr<int> sp = std::move(p2);  // ok — unique → shared
```

---

## 2. `shared_ptr` Internals

### Control Block Layout

```
shared_ptr<T> on stack:
┌───────────────────────┐
│  T*       ptr_        │  ← 8 bytes (pointer to managed object)
│  CtrlBlk* ctrl_       │  ← 8 bytes (pointer to control block)
└───────────────────────┘
     │              │
     │              └──────────────────────────────────┐
     ▼                                                  ▼
┌──────────┐           ┌─────────────────────────────────────────┐
│ T object │           │  Control Block                          │
└──────────┘           │  ┌───────────────────────────────────┐  │
                       │  │ use_count  (atomic<long>)  = 1    │  │
                       │  │ weak_count (atomic<long>)  = 1    │  │
                       │  │ deleter    (type-erased)          │  │
                       │  │ allocator  (type-erased)          │  │
                       │  └───────────────────────────────────┘  │
                       └─────────────────────────────────────────┘
```

**Two atomic counters** (not one):
- `use_count` — number of `shared_ptr` instances
- `weak_count` — number of `weak_ptr` + 1 (the +1 keeps block alive while `use_count > 0`)

```cpp
auto sp1 = std::make_shared<int>(42);
auto sp2 = sp1;                         // use_count = 2, weak_count = 1
std::weak_ptr<int> wp = sp1;            // use_count = 2, weak_count = 2
sp1.reset();                            // use_count = 1, weak_count = 2
sp2.reset();                            // use_count = 0 → destructor called
                                        // weak_count = 1 → control block stays alive
wp.expired();                           // true
// wp goes out of scope → weak_count = 0 → control block freed
```

### `make_shared` vs `shared_ptr(new T)`

```cpp
// Option 1: Two allocations
std::shared_ptr<Widget> sp(new Widget{});
// Allocation 1: new Widget  → heap block for Widget
// Allocation 2: make_shared control block separately

// Option 2: One allocation (preferred)
auto sp = std::make_shared<Widget>();
// Single allocation: [control block | Widget data] contiguous
```

**`make_shared` advantages:**
1. Single allocation — faster, less fragmentation
2. Exception safe (no leak if constructor throws after `new` but before `shared_ptr` ctor)
3. Better cache locality (object and control block adjacent)

**`make_shared` disadvantage:**
- Object memory not freed until `weak_count` drops to 0 (control block and object are fused) — matters if `weak_ptr` lives long

```cpp
// When you MUST use shared_ptr(new T):
// 1. Custom deleter
std::shared_ptr<FILE> fp(fopen("x","r"), fclose);

// 2. Adopting existing raw pointer (e.g., from C API)
Widget* raw = factory_create();
std::shared_ptr<Widget> sp(raw);

// 3. Need to separate object lifetime from block lifetime
```

### Atomic operations on counters

`use_count` increments/decrements are atomic but **not sequentially consistent** — they use `memory_order_relaxed` for increment and `memory_order_acq_rel` for decrement (to ensure destructor sees all writes before destruction).

```cpp
// shared_ptr is NOT thread-safe for the pointed-to object
// shared_ptr itself (copying, resetting) IS thread-safe
// (multiple threads can copy the same shared_ptr — use_count is atomic)

// But: two threads modifying the SAME shared_ptr variable need external sync
std::shared_ptr<int> sp = std::make_shared<int>(0);
// Thread 1: sp = other_sp;   // NOT safe without mutex
// Thread 2: sp.reset();       // NOT safe without mutex

// Use atomic<shared_ptr<T>> (C++20) for lock-free shared_ptr swaps
std::atomic<std::shared_ptr<int>> atomic_sp;
```

---

## 3. `weak_ptr` Internals

```cpp
auto sp = std::make_shared<int>(42);
std::weak_ptr<int> wp = sp;

// Check and use (safe pattern)
if (auto locked = wp.lock()) {    // returns shared_ptr (empty if expired)
    std::cout << *locked;
}
// Never: if (!wp.expired()) { wp.lock(); }  // TOCTOU race!

// Observer pattern
struct Cache {
    std::unordered_map<int, std::weak_ptr<Data>> entries;

    std::shared_ptr<Data> get(int key) {
        auto it = entries.find(key);
        if (it != entries.end())
            if (auto sp = it->second.lock())
                return sp;
        // Expired or not found — reload
        auto sp = load(key);
        entries[key] = sp;
        return sp;
    }
};
```

---

## 4. `enable_shared_from_this`

Problem: getting a `shared_ptr` to `this` from inside a method.

```cpp
// WRONG — creates a second independent control block → double free!
struct Node {
    std::shared_ptr<Node> get_self() {
        return std::shared_ptr<Node>(this);  // WRONG
    }
};

// CORRECT — inherit from enable_shared_from_this
struct Node : public std::enable_shared_from_this<Node> {
    std::shared_ptr<Node> get_self() {
        return shared_from_this();           // correct
    }
    std::weak_ptr<Node> get_weak() {
        return weak_from_this();             // C++17
    }
};

// REQUIREMENT: object must already be owned by a shared_ptr
auto n = std::make_shared<Node>();
auto self = n->get_self();  // OK — same control block
// Node n2; n2.get_self();  // throws std::bad_weak_ptr!

// How it works:
// enable_shared_from_this<T> stores a weak_ptr<T> mutable member
// shared_ptr<T> constructor detects enable_shared_from_this and sets the weak_ptr
// shared_from_this() calls weak_ptr.lock()
```

**Common use case — async callback that keeps object alive:**

```cpp
struct Connection : std::enable_shared_from_this<Connection> {
    void start_read() {
        socket_.async_read(buffer_, 
            [self = shared_from_this()](auto ec, auto n) {
                // 'self' keeps Connection alive during async op
                if (!ec) self->process(n);
            });
    }
    boost::asio::tcp::socket socket_;
    std::array<char, 4096> buffer_;
};
```

---

## 5. Cyclic References

```cpp
// PROBLEM: cycle → use_count never reaches 0 → memory leak
struct Node {
    std::shared_ptr<Node> next;  // strong reference
    std::shared_ptr<Node> prev;  // strong reference
    ~Node() { std::cout << "~Node\n"; }
};
{
    auto a = std::make_shared<Node>();
    auto b = std::make_shared<Node>();
    a->next = b;
    b->prev = a;   // cycle: a→b, b→a
}   // use_count(a)=1, use_count(b)=1 → neither destructor called!
// Memory leaked!

// SOLUTION: break cycle with weak_ptr
struct Node {
    std::shared_ptr<Node> next;  // "owns" next
    std::weak_ptr<Node>   prev;  // "observes" prev, doesn't own
    ~Node() { std::cout << "~Node\n"; }
};
{
    auto a = std::make_shared<Node>();
    auto b = std::make_shared<Node>();
    a->next = b;
    b->prev = a;   // weak — doesn't increment use_count
}   // use_count(b)→0 via a's destructor, use_count(a)→0 → both freed
// "~Node\n~Node\n" printed
```

### Parent-Child Pattern

```cpp
class Child;
class Parent : public std::enable_shared_from_this<Parent> {
public:
    std::vector<std::shared_ptr<Child>> children;
    void add_child(std::shared_ptr<Child> c);
};

class Child {
public:
    std::weak_ptr<Parent> parent;  // observe, don't own
    void notify_parent() {
        if (auto p = parent.lock()) p->on_child_event();
    }
};
```

---

## 6. Common Pitfalls

### Pitfall 1: Creating two control blocks from same raw ptr

```cpp
int* raw = new int{42};
std::shared_ptr<int> sp1(raw);
std::shared_ptr<int> sp2(raw);  // DOUBLE FREE — two separate control blocks!

// Safe way to alias
auto sp1 = std::make_shared<int>(42);
auto sp2 = sp1;  // shares control block
```

### Pitfall 2: Storing `this` without `enable_shared_from_this`

```cpp
// Already shown above — creates second control block → UB
```

### Pitfall 3: `shared_ptr` in performance-critical code

```cpp
// Each copy increments atomic counter — can be bottleneck under contention
// Prefer passing by const reference when no ownership transfer needed

void process(const std::shared_ptr<Widget>& sp) { sp->run(); }  // no copy
void process(std::shared_ptr<Widget> sp)         { sp->run(); }  // atomic inc+dec
```

### Pitfall 4: `unique_ptr` to array

```cpp
// unique_ptr has array specialization
auto arr = std::make_unique<int[]>(10);  // calls delete[]
arr[3] = 42;  // operator[] available

// shared_ptr<T[]> — C++17
auto sarr = std::make_shared<int[]>(10);
```

### Pitfall 5: `make_shared` with private constructor

```cpp
class Token {
    Token(int id) : id_(id) {}
    int id_;
public:
    // make_shared needs constructor access
    static std::shared_ptr<Token> create(int id) {
        // Can't use make_shared directly
        return std::shared_ptr<Token>(new Token(id)); // two allocs
        // Alternative: friend struct, or PassKey idiom
    }
};
```

---

## 7. Performance Comparison

```cpp
// Benchmark guide (approximate, varies by hardware)
raw pointer access    :  1 ns   (no overhead)
unique_ptr access     :  1 ns   (zero overhead, inlined)
shared_ptr access     :  1 ns   (dereferencing)
shared_ptr copy       :  10-40 ns (atomic increment, cache miss if control block cold)
shared_ptr destroy    :  10-40 ns (atomic decrement + conditional destructor)
```

**Rule of thumb**: `unique_ptr` everywhere, `shared_ptr` only when sharing is genuinely needed, `weak_ptr` to break cycles / observe.

---

## 8. `std::allocate_shared` and Custom Allocators

```cpp
// Use custom allocator for control block + object
template <typename T>
using PoolAllocator = /* ... */;

auto sp = std::allocate_shared<Widget>(PoolAllocator<Widget>{}, ctor_args...);
// Control block + Widget allocated via the custom allocator
// Useful for: memory pools, arenas, small-object optimizers
```

---

## 9. Smart Pointer Decision Flowchart

```
Do you need to manage heap memory?
│
├─ No → don't use a smart pointer (use reference or raw ptr non-owning)
│
└─ Yes → do you need shared ownership?
         │
         ├─ No → use unique_ptr<T>
         │       (can transfer ownership with std::move)
         │
         └─ Yes → use shared_ptr<T>
                  │
                  └─ do you need to observe without owning?
                           │
                           └─ use weak_ptr<T>
```

---

## Interview Q&As

**Q [Mid]: What is the difference between `make_shared` and `shared_ptr(new T)`?**

`make_shared` performs a **single allocation** for both the control block and the object (contiguous in memory), giving better cache locality, fewer allocations, and exception safety (no leak if constructor throws). `shared_ptr(new T)` allocates `T` first (via `new`), then allocates the control block separately — two allocations, potential leak if control block allocation throws. Downside of `make_shared`: the object memory isn't freed until `weak_count` also drops to 0 (since control block and object share the same allocation), which can delay memory reclamation when `weak_ptr` instances outlive the `shared_ptr`.

---

**Q [Mid]: How does `weak_ptr` avoid dangling pointers?**

`weak_ptr` stores a pointer to the control block (not the object) and doesn't increment `use_count`. Accessing the object requires calling `.lock()`, which atomically checks `use_count > 0` and either returns a valid `shared_ptr` or an empty one. The control block itself remains alive (via `weak_count`) until all `weak_ptr`s are also destroyed. This prevents the dangling pointer problem: you can never accidentally dereference a `weak_ptr` — you must go through `.lock()`.

---

**Q [Senior]: Why does `shared_ptr` have two reference counts?**

One count (`use_count`) tracks how many `shared_ptr`s own the object — when it reaches 0, the **object** is destroyed. The second count (`weak_count`) tracks `weak_ptr` instances plus 1 (for the `shared_ptr` group) — when it reaches 0, the **control block** is freed. This split allows `weak_ptr` to work correctly: it needs the control block alive to check expiry, even after the object is gone. If a single count were used, either the control block would be freed too early (breaking `weak_ptr`) or the object would stay alive too long.

---

**Q [Senior]: What happens if you call `shared_from_this()` on an object not owned by a `shared_ptr`?**

It throws `std::bad_weak_ptr`. The mechanism behind `enable_shared_from_this` stores an internal `weak_ptr<T>` that is set by the `shared_ptr` constructor when it detects the `enable_shared_from_this` base. If the object was constructed on the stack or via `new` without immediately wrapping in a `shared_ptr`, that internal `weak_ptr` is empty (default-constructed), and `.lock()` returns null, causing `shared_from_this()` to throw. Always create objects using `std::make_shared` when you intend to use `shared_from_this`.

---

**Q [Senior]: How is `unique_ptr` zero overhead if it calls a custom function on destruction?**

The deleter is stored using the **Empty Base Optimization** (EBO) via a compressed pair. A stateless deleter (e.g., a function object with no data members like `default_delete<T>`) takes 0 bytes — the compiler merges it with the pointer member via EBO. The actual deletion call is inlined at compile time (no virtual dispatch, no function pointer indirection). A stateful deleter (like a lambda capturing variables) does add size, but `unique_ptr` is still cheaper than `shared_ptr` since there's no control block or atomic operations.
