# Interview Study Repository

Tài liệu ôn phỏng vấn kỹ thuật — tập trung **Senior C++ / systems / real-time**.

---

## Company-specific prep

| File | Mục đích |
|------|----------|
| [vsf_senior_cpp_vinsmart_future.md](vsf_senior_cpp_vinsmart_future.md) | Lộ trình & checklist cho **VinSmart Future — Senior C++** |

---

## Sections

### [C++ App/](C++%20App/README.md) — Core track (19 files)

Hệ thống C++, concurrency, networking, low latency, Linux, system design.

**Ưu tiên cho JD real-time / high-performance:**

| P0 | P1 | P2 (nice-to-have) |
|----|----|-------------------|
| multithreading_and_concurrency | smart_pointers, core_cpp_memory | realtime_media_voip_webrtc |
| networking | linux_concepts | design_patterns |
| low_latency | production_debugging, senior_engineering_behaviors | |
| segmentation_fault | system_design | |

### [Embedded/](Embedded/embedded_systems.md) — 12 files

MCU, RTOS, buses, embedded Linux, safety (ISO 26262). Hữu ích nếu CV có embedded / IoT Vingroup.

### [Rust/](Rust/rust_00_index.md) — 12 files

Ownership, async, FFI — khi phỏng vấn có Rust hoặc so sánh memory safety.

### [OpenGL/](OpenGL/opengl_00_index.md) — 10 files

Graphics pipeline — khi role liên quan rendering / game engine.

---

## JD mapping (VSF Senior C++)

| JD requirement | Primary files |
|----------------|---------------|
| C++, memory, 4+ years | `core_cpp_memory_and_ownership`, `smart_pointers`, `modern_cpp_11_14_17_20` |
| Multithreading, concurrency | `multithreading_and_concurrency` |
| Performance (CPU, memory, latency, network) | `low_latency`, `linux_concepts` (perf) |
| Linux CLI, files, permissions | `linux_concepts` §2–§3 |
| TCP/UDP, WebSocket | `networking` § WebSocket |
| Production: crash, deadlock, race | `segmentation_fault`, `production_debugging` |
| System architecture | `system_design`, `senior_engineering_behaviors` |
| SQL / NoSQL / DB + C++ | `database_management` |
| Architecture thinking, ownership, proactive mindset | `senior_engineering_behaviors` |
| Code review, mentoring | `senior_engineering_behaviors` |
| VoIP / WebRTC / streaming (nice) | `realtime_media_voip_webrtc` |
| C++ core ↔ Android/iOS (IDL, JNI, Swift) | `cpp_core_mobile_bridge_idl`, `examples/mobile_bridge/` |

---

## Suggested study order (VSF track)

1. `C++ App/cpp_senior_interview_concepts.md` — bản đồ chủ đề  
2. Tuần 1–2: memory + concurrency + `production_debugging`  
3. Tuần 3: `low_latency` + `networking` + `linux_concepts`  
4. Tuần 4: `senior_engineering_behaviors` + `system_design` + `database_management` + mock Q&A / STAR  
5. Tuần 5+: `realtime_media_voip_webrtc` nếu JD/media  

Chi tiết checklist: [vsf_senior_cpp_vinsmart_future.md](vsf_senior_cpp_vinsmart_future.md).

---

## File index (C++ App — mới / mở rộng)

| File | Topics |
|------|--------|
| [production_debugging.md](C++%20App/production_debugging.md) | On-call playbook, deadlock/race/hang, sanitizer matrix, core dump workflow |
| [senior_engineering_behaviors.md](C++%20App/senior_engineering_behaviors.md) | Architecture, ownership, proactive mindset, code review, mentoring, STAR |
| [realtime_media_voip_webrtc.md](C++%20App/realtime_media_voip_webrtc.md) | RTP, jitter buffer, WebRTC, SFU, C++ stacks (libwebrtc, GStreamer) |
| [database_management.md](C++%20App/database_management.md) | SQL vs NoSQL, khi nào dùng, Redis+PG, C++ core không block hot path |
| [cpp_core_mobile_bridge_idl.md](C++%20App/cpp_core_mobile_bridge_idl.md) | Protobuf IDL, JNI, Swift, DB sync, Q&A |
| [gtest_unit_testing.md](C++%20App/gtest_unit_testing.md) | GoogleTest/gMock, `ASSERT` vs `EXPECT`, mocks/stubs/fakes, CMake/CTest, flaky tests |
| [leetcode_interview_prep.md](leetcode_interview_prep.md) | ~10 bài Easy→Medium / concept (LeetCode PV) |
| [examples/mobile_bridge/](C++%20App/examples/mobile_bridge/) | `bridge.proto` + C++ sketch |

Các file còn lại: xem [C++ App/README.md](C++%20App/README.md).
