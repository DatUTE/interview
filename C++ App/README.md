# C++ Senior Interview — Study Reference

Complete reference set for senior-level C++ engineering interviews.

---

## Study Files

| # | File | Topics |
|---|---|---|
| 1 | [multithreading_and_concurrency.md](multithreading_and_concurrency.md) | Thread pool, mutex, **memory_order** (relaxed/acquire/release/seq_cst), producer-consumer, lock-free SPSC, `std::terminate` |
| 2 | [ipc_concepts.md](ipc_concepts.md) | Pipes, shared memory, UDS, TCP, **Protobuf**, **gRPC / Boost message_queue / MQTT**, mmap, decision guide |
| 2b | [cpp_core_mobile_bridge_idl.md](cpp_core_mobile_bridge_idl.md) | IDL, JNI/Kotlin, Swift, C API, SQLite/cloud — **C++ core ↔ mobile** |
| 3 | [memory_layout_and_compilation.md](memory_layout_and_compilation.md) | Stack/heap/BSS/**text/data/rodata**, ELF, linking, virtual memory, compilation |
| 4 | [segmentation_fault.md](segmentation_fault.md) | 9 causes of SIGSEGV, ASan, gdb diagnosis, SIGBUS vs SIGABRT |
| 5 | [oop_concepts.md](oop_concepts.md) | 4 pillars, vtable, Rule of 5, virtual inheritance, CRTP, dependency injection |
| 6 | [stl_containers.md](stl_containers.md) | array, vector, **cache locality §10**, deque, list, map, unordered_map, iterators |
| 7 | [algorithms.md](algorithms.md) | Sorting, graphs (BFS/DFS/Dijkstra/Bellman-Ford/Union-Find), DP, two-pointer, KMP |
| 7b | [../leetcode_interview_prep.md](../leetcode_interview_prep.md) | LeetCode lists by concept (Easy→Medium, ~10 each) |
| 8 | [linux_concepts.md](linux_concepts.md) | Command line, file permissions, kernel/processes, epoll, signals, /proc, perf, systemd |
| 9 | [design_patterns.md](design_patterns.md) | All 23 GoF patterns with C++ implementations |
| 10 | [cpp_templates.md](cpp_templates.md) | SFINAE, concepts (C++20), variadic templates, type traits, CRTP, TMP |
| 11 | [smart_pointers.md](smart_pointers.md) | `unique_ptr`, `shared_ptr` control block, `weak_ptr`, `enable_shared_from_this`, cycles |
| 12 | [networking.md](networking.md) | TCP/UDP, socket programming, epoll server, HTTP/2 vs HTTP/3, TLS handshake |
| 13 | [system_design.md](system_design.md) | Interview framework, CAP theorem, scaling patterns, URL shortener, chat, feed, rate limiter, Snowflake ID |
| 13b | [database_management.md](database_management.md) | SQL vs NoSQL, when to use, indexes, cache-aside, C++ drivers & hot-path patterns |
| 14 | [cpp_keywords.md](cpp_keywords.md) | `const`/`constexpr`/`consteval`/`constinit`, `noexcept`, `inline`, `extern`, `static`, `auto`, `decltype`, `override`, `final`, `explicit`, `mutable`, `volatile`, `thread_local`, attributes |
| 15 | [low_latency.md](low_latency.md) | CPU pinning, NUMA, **cache & vector/array scan**, lock-free SPSC, DPDK, jitter elimination |
| 16 | [production_debugging.md](production_debugging.md) | On-call triage, sanitizer matrix (ASan/TSan/UBSan), deadlock/race/hang diagnosis, observability |
| 17 | [gtest_unit_testing.md](gtest_unit_testing.md) | GoogleTest/gMock, `ASSERT` vs `EXPECT`, mocks/stubs/fakes, CMake/CTest, sanitizer CI |
| 18 | [realtime_media_voip_webrtc.md](realtime_media_voip_webrtc.md) | RTP/RTCP, jitter buffer, Opus, WebRTC (ICE/STUN/TURN/SRTP), SFU vs MCU, C++ media stacks |
| 19 | [senior_engineering_behaviors.md](senior_engineering_behaviors.md) | Architecture thinking, ownership, proactive mindset, code review checklist, mentoring (STAR) |
| 20 | [cpp_senior_interview_concepts.md](cpp_senior_interview_concepts.md) | Topic map + prioritization |
| 21 | [embedded_systems.md](../Embedded/embedded_systems.md) *(index → 12 files)* | GPIO, interrupts, NVIC, DMA, cache coherency, UART/SPI/I2C/CAN/USB, linker scripts, volatile, RTOS (FreeRTOS), priority inversion, power management, bootloader, OTA, JTAG/SWD, hard fault debug, embedded Linux, ISO 26262, secure boot, TrustZone |
| 22 | [Rust/rust_00_index.md](../Rust/rust_00_index.md) *(index → 12 files)* | Ownership/borrowing/lifetimes, traits/generics, enums/pattern matching, iterators/closures, error handling, smart pointers (Box/Rc/Arc/RefCell), concurrency (Mutex/channels/Rayon), async/await (tokio/futures), modules/Cargo/workspaces, unsafe/FFI, advanced patterns (builder/type-state/macros), interview hot topics |
| 23 | [OpenGL/opengl_00_index.md](../OpenGL/opengl_00_index.md) *(index → 10 files)* | Rendering pipeline, GLFW/GLAD context, VAO/VBO/EBO, GLSL shaders, textures, MVP transforms/camera, Phong/Blinn-Phong/PBR lighting, FBO/post-processing/deferred rendering, shadow mapping/normal maps/HDR/bloom/SSAO, compute shaders/DSA/instancing/indirect draw, GPU profiling/batching/LOD, interview hot topics |

**Company prep:** [../vsf_senior_cpp_vinsmart_future.md](../vsf_senior_cpp_vinsmart_future.md) (VinSmart Future Senior C++) · [../README.md](../README.md) (repo index)

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
| Networking & WebSocket | 2, 8, 12, 13 |
| Performance & profiling | 1, 3, 8, 10, 11, 15 |
| Production debugging | 4, 16 |
| Unit testing / gTest | 17 |
| Real-time media (VoIP/WebRTC) | 18 |
| Databases (SQL, NoSQL, C++ integration) | 13b, 13 |
| System design & architecture | 13, 13b, 19, 20 |
| Senior behaviors (review, mentor, ownership) | 19 |
| Embedded systems (hardware → RTOS → Linux) | 21 |
| Real-time & safety-critical systems | 21 |
| Rust ownership & memory safety | 22 |
| Rust concurrency & async | 22 |
| Rust systems programming & FFI | 22 |
| OpenGL rendering pipeline & shaders | 23 |
| OpenGL lighting, shadows & post-processing | 23 |
| OpenGL performance & modern techniques | 23 |

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

**Week 4 — Architecture, Senior Behaviors & Review**
11. `senior_engineering_behaviors.md` — architecture thinking, ownership, review, mentoring, STAR stories
12. `system_design.md` — scale patterns, chat, CAP
13. `database_management.md` — SQL vs NoSQL, C++ DB integration
14. `design_patterns.md` — software design
15. `networking.md` — TCP/UDP, WebSocket, epoll server
16. `production_debugging.md` — crash, deadlock, race playbook
17. `gtest_unit_testing.md` — GoogleTest/gMock, stubs vs mocks, CTest, sanitizer test strategy
18. Mock Q&A drill across all files

**Real-Time / VSF track (Week 5+)**
15. `low_latency.md` — jitter, CPU pinning, lock-free hot path
16. `realtime_media_voip_webrtc.md` — if JD mentions call/streaming
17. [../vsf_senior_cpp_vinsmart_future.md](../vsf_senior_cpp_vinsmart_future.md) — checklist & mock questions

**Embedded Track (parallel or after Week 4)**
18. `embedded_systems.md` — hardware through RTOS through Linux, safety & security

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
