# C++ Senior Interview — Study Reference

Complete reference set for senior-level C++ engineering interviews.

---

## Study Files

| # | File | Topics |
|---|---|---|
| 1 | [multithreading_and_concurrency.md](multithreading_and_concurrency.md) | Thread pool, mutex, condition_variable, producer-consumer, lock-free, `std::terminate` |
| 2 | [ipc_concepts.md](ipc_concepts.md) | Pipes, message queues, shared memory, semaphores, signals, Unix sockets, TCP/UDP, mmap |
| 3 | [memory_layout_and_compilation.md](memory_layout_and_compilation.md) | Stack/heap/BSS layout, ELF, linking, virtual memory, `delete` internals, translation units |
| 4 | [segmentation_fault.md](segmentation_fault.md) | 9 causes of SIGSEGV, ASan, gdb diagnosis, SIGBUS vs SIGABRT |
| 5 | [oop_concepts.md](oop_concepts.md) | 4 pillars, vtable, Rule of 5, virtual inheritance, CRTP, dependency injection |
| 6 | [stl_containers.md](stl_containers.md) | array, vector, deque, list, set, map, unordered_map, priority_queue, iterator categories |
| 7 | [algorithms.md](algorithms.md) | Sorting, graphs (BFS/DFS/Dijkstra/Bellman-Ford/Union-Find), DP, two-pointer, KMP |
| 8 | [linux_concepts.md](linux_concepts.md) | Kernel/user space, processes, epoll, signals, /proc, perf, daemons |
| 9 | [design_patterns.md](design_patterns.md) | All 23 GoF patterns with C++ implementations |
| 10 | [cpp_templates.md](cpp_templates.md) | SFINAE, concepts (C++20), variadic templates, type traits, CRTP, TMP |
| 11 | [smart_pointers.md](smart_pointers.md) | `unique_ptr`, `shared_ptr` control block, `weak_ptr`, `enable_shared_from_this`, cycles |
| 12 | [networking.md](networking.md) | TCP/UDP, socket programming, epoll server, HTTP/2 vs HTTP/3, TLS handshake |
| 13 | [system_design.md](system_design.md) | Interview framework, CAP theorem, scaling patterns, URL shortener, chat, feed, rate limiter, Snowflake ID |
| 14 | [cpp_keywords.md](cpp_keywords.md) | `const`/`constexpr`/`consteval`/`constinit`, `noexcept`, `inline`, `extern`, `static`, `auto`, `decltype`, `override`, `final`, `explicit`, `mutable`, `volatile`, `thread_local`, attributes |
| 15 | [low_latency.md](low_latency.md) | CPU pinning, NUMA, huge pages, lock-free SPSC, kernel bypass (DPDK), RDTSC timing, jitter elimination, startup sequence |

---

## Topic Coverage Matrix

| Topic | File(s) |
|---|---|
| Memory management | 3, 4, 5, 11 |
| Concurrency & threading | 1, 2 |
| C++ language features | 5, 6, 10, 11 |
| System programming | 2, 3, 8 |
| Data structures | 6, 7 |
| Algorithms & complexity | 7 |
| Software design | 5, 9, 13 |
| Networking | 2, 8, 13 |
| Performance & profiling | 1, 3, 8, 10, 11 |
| System design & architecture | 13 |

---

## Difficulty Guide

Each file contains Q&As tagged `[Mid]` and `[Senior]`.

**Mid-level** — core knowledge: knows the API and basic behaviour  
**Senior-level** — internals, trade-offs, system design, failure modes

---

## Quick-Revision Cheat Sheet

### Complexity in 30 seconds
```
O(1)      hash table lookup, array index
O(log n)  binary search, balanced BST (map/set)
O(n)      linear scan, single pass
O(n log n) merge sort, heap sort, std::sort
O(n²)    bubble/insertion sort, naive nested loops
O(2^n)   brute-force subsets / exponential DP
```

### Container Cheat Sheet
```
Random access, grow:    vector
Both-end push:          deque
Sorted unique keys:     set / map
Fastest lookup:         unordered_map
Pointer stability:      list (or deque)
Priority queue:         priority_queue (max-heap by default)
```

### Concurrency Rules
```
Data race = UB
mutex  → protect shared state
condition_variable → wait for condition (always loop-check)
atomic → single variable, no ordering complexity
shared_ptr copy → atomic ref count (NOT object protection)
```

### Memory Rules
```
new  → delete        never mix
new[] → delete[]
malloc → free
unique_ptr → one owner, zero overhead
shared_ptr → shared ownership, control block (16 bytes + object)
weak_ptr  → observe, breaks cycles
```

### TLS 1.3 Handshake in 5 seconds
```
Client → ClientHello + DH public key
Server → ServerHello + DH public key + Certificate + Finished (1 RTT)
Client → Finished
═══════ Encrypted data ═══════
```

### Design Pattern One-liners
```
Singleton     → one global instance (Meyers: static local)
Factory       → create object without knowing exact type
Builder       → step-by-step complex object construction
Observer      → notify many on state change (weak_ptr for cleanup)
Strategy      → swap algorithm at runtime
Decorator     → stack behaviors without subclassing
CRTP          → static polymorphism, zero vtable overhead
```

---

## Suggested Study Order

**Week 1 — Foundations**
1. `oop_concepts.md` — base for everything
2. `memory_layout_and_compilation.md` — how C++ works under the hood
3. `segmentation_fault.md` — debugging and safety

**Week 2 — Language Features**
4. `smart_pointers.md` — ownership model
5. `cpp_templates.md` — generic programming
6. `stl_containers.md` — standard library

**Week 3 — Systems & Algorithms**
7. `algorithms.md` — coding interview prep
8. `linux_concepts.md` — systems programming
9. `multithreading_and_concurrency.md` — parallel systems
10. `ipc_concepts.md` — inter-process communication

**Week 4 — Architecture & Review**
11. `design_patterns.md` — software design
12. `networking.md` — network programming
13. Mock Q&A drill across all files

---

## Common Senior Interview Themes

| Theme | Key Points to Hit |
|---|---|
| Why virtual destructor? | vtable slot, polymorphic delete, slicing |
| shared_ptr thread safety | atomic use_count, NOT object thread-safe |
| epoll vs select | O(1) ready list vs O(n) scan, edge vs level triggered |
| SFINAE vs Concepts | substitution failure, readable errors, C++20 |
| Rule of 5 | copy/move ctor+assign + destructor, copy-and-swap |
| Template instantiation | explicit instantiation, ODR, extern template |
| Lock-free vs mutex | atomics, ABA, memory ordering, use mutex first |
| Memory leak diagnosis | valgrind, AddressSanitizer, /proc/PID/maps |
| TLS Perfect Forward Secrecy | ephemeral DH, cannot decrypt past sessions |
| HOL blocking | HTTP/2 (TCP level), HTTP/3 (QUIC per-stream reliability) |
