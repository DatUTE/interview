# C++ Senior Interview Concepts

## 1. Core C++ Language

- **Memory model**: stack vs heap, RAII, ownership semantics
- **Move semantics**: `std::move`, rvalue references (`&&`), move constructor/assignment
- **Smart pointers**: `unique_ptr`, `shared_ptr`, `weak_ptr` — when to use each, circular reference pitfalls
- **Templates**: function/class templates, template specialization, SFINAE, `if constexpr`
- **Copy elision / RVO / NRVO**
- `**const` correctness**: `const`, `constexpr`, `consteval`, `mutable`
- **ODR (One Definition Rule)** and linkage (`inline`, `extern`, `static`)
- **Undefined behavior**: common sources (signed overflow, use-after-free, strict aliasing)

---

## 2. Modern C++ (11/14/17/20)

- **Lambda captures**: by value, by reference, generalized captures (`[x = std::move(x)]`)
- `**auto`, `decltype`, `decltype(auto)`**
- **Structured bindings** (`auto [a, b] = ...`)
- `**std::optional`, `std::variant`, `std::any`**
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

- `**std::thread`, `std::async`, `std::future`/`std::promise`**
- **Mutex types**: `mutex`, `recursive_mutex`, `shared_mutex` (reader-writer lock)
- `**std::atomic`**: memory orders (`relaxed`, `acquire`, `release`, `seq_cst`) → [multithreading_and_concurrency.md](multithreading_and_concurrency.md) § Memory Orders
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
- `**virtual` destructor** — when mandatory and why

---

## 6. Systems & Performance

- **Cache hierarchy**: L1/L2/L3, cache line size (64 bytes), false sharing
- **Sequential containers & cache**: `vector`/`array` contiguous scan vs `list` pointer chase → [low_latency.md](low_latency.md), [stl_containers.md](stl_containers.md) §10
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
- Length-prefix protocol parser over TCP/WebSocket
- epoll echo server / 100k connection gateway design

---

## 8. Databases (SQL & NoSQL)

- **SQL:** tables, ACID, JOIN, PostgreSQL/MySQL/SQLite
- **NoSQL types:** KV (Redis), document (Mongo), wide-column (Cassandra), search, time-series
- **When to use which** — decision flowchart
- **C++ core:** connection pool, async writes, no DB on I/O thread → [database_management.md](database_management.md)

---

## 9. Mobile bridge (C++ core ↔ Android / iOS)

- **IDL:** Protobuf `.proto` (default), FlatBuffers, JSON
- **Android:** JNI, `jbyteArray`, NDK, Kotlin protobuf-lite
- **iOS:** C API / ObjC++, SwiftProtobuf, main-thread callbacks
- **DB:** core-owned SQLite vs app Room — single writer
- → [cpp_core_mobile_bridge_idl.md](cpp_core_mobile_bridge_idl.md)

---

## 10. Real-Time Networking

- **TCP vs UDP** — reliability, latency, head-of-line blocking
- **WebSocket** — upgrade, frames, ping/pong, masking, reconnect → [networking.md](networking.md) § WebSocket
- **Application framing** — length-prefix, versioning, backpressure
- **epoll reactor** — edge-triggered, drain until `EAGAIN`, `SO_REUSEPORT`
- **Socket tuning** — `TCP_NODELAY`, keepalive, buffer sizes
- **VoIP / WebRTC (nice-to-have)** — RTP, jitter buffer, ICE, SFU → [realtime_media_voip_webrtc.md](realtime_media_voip_webrtc.md)

---

## 11. Production Debugging

- **Crash** — core dump, `gdb thread apply all bt`, ASan → [segmentation_fault.md](segmentation_fault.md)
- **Deadlock** — lock graph, TSan, lock ordering → [production_debugging.md](production_debugging.md), [multithreading_and_concurrency.md](multithreading_and_concurrency.md)
- **Data race** — TSan, happens-before, `shared_ptr` ≠ thread-safe object
- **Hang** — `top -H`, `strace`, perf
- **On-call** — mitigate → diagnose → fix → postmortem
- **Regression tests** — GoogleTest/gMock, `ASSERT` vs `EXPECT`, stubs/mocks, CTest, sanitizer CI → [gtest_unit_testing.md](gtest_unit_testing.md)

---

## 12. Unit Testing with GoogleTest

- **gTest basics:** `TEST`, `TEST_F`, `EXPECT_*` vs `ASSERT_*`
- **Test doubles:** dummy, stub, fake, spy, mock
- **gMock:** `EXPECT_CALL`, `ON_CALL`, strict vs nice mocks
- **CTest/CMake:** `gtest_discover_tests`, `ctest --output-on-failure`
- **Flaky/race tests:** `--gtest_repeat`, `--gtest_filter`, TSan
- → [gtest_unit_testing.md](gtest_unit_testing.md)

---

## 13. Senior Behaviors (architecture, ownership, review, mentoring)

→ **Full guide:** [senior_engineering_behaviors.md](senior_engineering_behaviors.md)


| JD theme                     | Ôn trong file trên                                  |
| ---------------------------- | --------------------------------------------------- |
| System architecture thinking | §1 framework 5 bước, trade-offs, ADR, anti-patterns |
| Proactive problem-solving    | §2 signal → experiment → prevent                    |
| Sense of ownership           | §3 end-to-end, on-call, MTTR                        |
| Code review                  | §4 C++ checklist, feedback style                    |
| Mentoring                    | §5 SBI, onboarding, conflict                        |
| STAR stories                 | §6 story bank + template                            |


---

## Prioritization


| Priority           | Focus                                                                                   |
| ------------------ | --------------------------------------------------------------------------------------- |
| Must nail          | Move semantics, smart pointers, multithreading basics, Rule of 5, vtable                |
| Strong on          | Templates/SFINAE, memory model, STL complexity, design patterns                         |
| VSF / real-time JD | networking + WebSocket, low_latency, production_debugging, gtest_unit_testing, senior_engineering_behaviors |
| Good to know       | C++20 features, lock-free, WebRTC/VoIP, custom allocators, ABI                          |


---

## File map (quick links)


| Topic            | File                                                                         |
| ---------------- | ---------------------------------------------------------------------------- |
| Full study index | [README.md](README.md)                                                       |
| VSF company prep | [../vsf_senior_cpp_vinsmart_future.md](../vsf_senior_cpp_vinsmart_future.md) |
| Repo root index  | [../README.md](../README.md)                                                 |


