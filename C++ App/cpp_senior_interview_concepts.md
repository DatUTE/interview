# C++ Senior Interview Concepts

## 1. Core C++ Language

- **Memory model**: stack vs heap, RAII, ownership semantics
- **Move semantics**: `std::move`, rvalue references (`&&`), move constructor/assignment
- **Smart pointers**: `unique_ptr`, `shared_ptr`, `weak_ptr` — when to use each, circular reference pitfalls
- **Templates**: function/class templates, template specialization, SFINAE, `if constexpr`
- **Copy elision / RVO / NRVO**
- **`const` correctness**: `const`, `constexpr`, `consteval`, `mutable`
- **ODR (One Definition Rule)** and linkage (`inline`, `extern`, `static`)
- **Undefined behavior**: common sources (signed overflow, use-after-free, strict aliasing)

---

## 2. Modern C++ (11/14/17/20)

- **Lambda captures**: by value, by reference, generalized captures (`[x = std::move(x)]`)
- **`auto`, `decltype`, `decltype(auto)`**
- **Structured bindings** (`auto [a, b] = ...`)
- **`std::optional`, `std::variant`, `std::any`**
- **Ranges (C++20)** and views
- **Concepts (C++20)**: `requires` clauses, constraining templates
- **Coroutines (C++20)**: `co_await`, `co_yield`, `co_return` — basics

---

## 3. Data Structures & Algorithms

- Big-O analysis for all STL containers (`vector`, `deque`, `list`, `map`, `unordered_map`, `set`)
- **Hash maps**: collision resolution, load factor, rehashing cost
- **Trees**: BST, AVL, Red-Black (know that `std::map` is R-B internally)
- Graph algorithms: BFS, DFS, Dijkstra, topological sort
- Dynamic programming, sliding window, two pointers
- **Cache locality**: why `vector` beats `list` in practice despite O(n) insert

---

## 4. Multithreading & Concurrency

- **`std::thread`, `std::async`, `std::future`/`std::promise`**
- **Mutex types**: `mutex`, `recursive_mutex`, `shared_mutex` (reader-writer lock)
- **`std::atomic`**: memory orders (`relaxed`, `acquire`, `release`, `seq_cst`)
- **Deadlock, livelock, starvation** — how to avoid
- **Condition variables**: spurious wakeups, `wait_for` pattern
- **Lock-free programming** basics
- **Thread pool** design

---

## 5. Object-Oriented Design

- **Rule of 0/3/5**
- **Virtual dispatch**: vtable internals, performance cost
- **CRTP** (Curiously Recurring Template Pattern) — static polymorphism
- **SOLID principles** applied to C++
- **Design patterns**: Factory, Strategy, Observer, PIMPL idiom, Builder
- **`virtual` destructor** — when mandatory and why

---

## 6. Systems & Performance

- **Cache hierarchy**: L1/L2/L3, cache line size (64 bytes), false sharing
- **Memory allocators**: `malloc` internals, custom allocators, pool allocator
- **Profiling tools**: `perf`, `valgrind`, `gprof`, sanitizers (ASan, TSan, UBSan)
- **Compilation model**: translation units, link-time optimization (LTO), `inline`
- **ABI stability** and binary compatibility concerns

---

## 7. Common Interview Problem Patterns

- Implement `shared_ptr` from scratch
- Thread-safe singleton (Meyer's singleton vs mutex-guarded)
- LRU cache implementation
- Producer-consumer with `std::queue` + mutex + condition variable
- Custom memory pool allocator
- Serialize/deserialize a binary tree

---

## Prioritization

| Priority     | Focus |
|--------------|-------|
| Must nail    | Move semantics, smart pointers, multithreading basics, Rule of 5, vtable |
| Strong on    | Templates/SFINAE, memory model, STL complexity, design patterns |
| Good to know | C++20 features, lock-free, custom allocators, ABI |
