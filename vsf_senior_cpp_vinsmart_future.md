# Chuẩn bị phỏng vấn — Senior C++ Developer @ VinSmart Future (VSF)

> **Vị trí:** Senior C++ Developer  
> **Công ty:** VinSmart Future (VSF) — Vingroup  
> **Kinh nghiệm yêu cầu:** 4+ năm C++, hệ thống hiệu năng cao / real-time  
> **Cập nhật:** 2026-06-01  
> **Repo index:** [README.md](README.md) · **C++ track:** [C++ App/README.md](C++%20App/README.md)

File này là **checklist & lộ trình** — nội dung kỹ thuật chi tiết nằm trong các section `C++ App/`, `Embedded/`, v.v.

---

## 1. Tóm tắt JD — Họ đang tìm ai?

VSF gộp hệ sinh thái công nghệ Vingroup (VinApp, VinIT, VinBigdata, …), trọng tâm **AI + hạ tầng quy mô lớn**. Vai trò Senior C++ không chỉ “viết code C++” mà là người:

| Trục JD | Ý nghĩa thực tế |
|--------|------------------|
| **High-performance core / real-time** | Engine, gateway, media pipeline, trading-like systems — latency & throughput là KPI |
| **Multithreading & memory** | Thread-safe design, không leak, không data race, hiểu memory model |
| **Network real-time** | TCP/UDP, WebSocket, protocol design, backpressure, reconnect |
| **Production debugging** | Crash (core dump), deadlock, race — dùng gdb, sanitizers, logs |
| **Senior behaviors** | Code review, mentor, architecture, ownership end-to-end |
| **Nice-to-have** | Call/VoIP, WebRTC, media streaming — nếu có là lợi thế lớn |

**Gợi ý định vị khi phỏng vấn:** “Tôi xây hệ thống C++ chịu tải cao, latency thấp, debug được production, và dẫn dắt kỹ thuật cho team.”

---

## 2. Lộ trình ôn tập (4–6 tuần)

### Tuần 1–2 — Nền tảng bắt buộc (Must nail)

| Chủ đề | Mục tiêu | Tài liệu trong repo |
|--------|----------|---------------------|
| Memory & ownership | RAII, Rule of 0/3/5, move, UB | `C++ App/core_cpp_memory_and_ownership.md`, `smart_pointers.md`, `memory_layout_and_compilation.md` |
| Modern C++ | C++11–20 dùng hàng ngày | `C++ App/modern_cpp_11_14_17_20.md`, `cpp_keywords.md` |
| Concurrency | mutex, CV, atomic, deadlock | `C++ App/multithreading_and_concurrency.md` |
| Debug production | crash, deadlock, race, on-call | `C++ App/production_debugging.md`, `segmentation_fault.md` |

### Tuần 3 — Performance & systems

| Chủ đề | Mục tiêu | Tài liệu |
|--------|----------|----------|
| Low latency | CPU, cache, false sharing, jitter | `C++ App/low_latency.md` |
| Linux / I/O | epoll, signals, perf | `C++ App/linux_concepts.md` |
| Networking | TCP/UDP, socket, epoll server | `C++ App/networking.md`, `ipc_concepts.md` |

### Tuần 4 — Senior layer (gồm soft skills JD)

| Chủ đề | Mục tiêu | Tài liệu |
|--------|----------|----------|
| **Architecture + ownership + review + mentor** | STAR stories, ADR, checklist PR | `C++ App/senior_engineering_behaviors.md` |
| Architecture (scale) | CAP, chat, rate limit | `C++ App/system_design.md` |
| SQL / NoSQL / persistence | Khi nào PG vs Redis vs Cassandra; C++ không block hot path | `C++ App/database_management.md` |
| Design & OOP | SOLID, patterns, PIMPL | `C++ App/design_patterns.md`, `oop_concepts.md` |
| Interview drill | Q&A senior | `C++ App/cpp_senior_interview_concepts.md` |

### Tuần 5–6 (Nice-to-have) — Real-time comm / media

- Đọc: `C++ App/networking.md` § WebSocket (handshake, frame, reconnect Q&A)
- Đọc: `C++ App/realtime_media_voip_webrtc.md` (RTP, jitter buffer, WebRTC, SFU)

---

## 3. Map JD → Kiến thức cần trả lời được

### 3.1 C++ & memory (4+ năm)

**Phải giải thích được (không chỉ định nghĩa):**

- Khác nhau `unique_ptr` / `shared_ptr` / `weak_ptr`; khi nào tránh `shared_ptr` trên hot path
- Move vs copy; RVO/NRVO; khi nào compiler không elide
- `std::atomic` và memory order: `relaxed`, `acquire`, `release`, `seq_cst` — → `multithreading_and_concurrency.md` § Memory Orders (happens-before, SPSC, flowchart)
- Data race vs race condition; tại sao TSan quan trọng
- Allocator: `malloc` cost, pool allocator, custom allocator cho real-time (no alloc on hot path)
- `volatile` **không** thay thế atomic / mutex

**Câu hỏi mẫu:**

1. Implement thread-safe queue hoặc LRU cache (mutex + CV hoặc lock-free SPSC).
2. Viết `shared_ptr` đơn giản (control block, ref count).
3. Tại sao `vector` thường nhanh hơn `list` dù complexity insert khác?

### 3.2 Multithreading & concurrency

**Chủ đề:**

- Thread pool: work queue, shutdown graceful
- Deadlock: 4 điều kiện Coffman; lock ordering; `std::lock`
- Spurious wakeup với `condition_variable`
- Reader-writer: `shared_mutex`
- Lock-free basics: SPSC ring buffer (phù hợp JD real-time)

**Production:**

- `std::terminate` khi exception thoát destructor mutex
- Thread-local storage cho per-connection state

### 3.3 Performance optimization

**CPU:**

- Cache line (64B), false sharing — pad struct / align
- Branch prediction, hot/cold path split
- `perf top`, flame graph, PMU counters

**Memory:**

- Heap fragmentation; object pool; stack allocation cho small buffers
- Huge pages (TLB) — tham khảo `low_latency.md`

**Latency / network:**

- p50 vs p99 vs p999 — SLA thường gắn tail latency
- Batch vs single-message; zero-copy (`sendmsg`, shared buffer)
- TCP: Nagle, `TCP_NODELAY`, backlog, keepalive
- UDP: reliability layer (seq, ACK, retransmit) nếu tự build

### 3.4 TCP / UDP / WebSocket / real-time comm

| Protocol | Biết |
|----------|------|
| **TCP** | 3-way handshake, flow control, congestion, head-of-line blocking |
| **UDP** | Connectionless, MTU, fragmentation, khi nào chọn UDP |
| **WebSocket** | Upgrade HTTP, binary/text frame, masking (client), heartbeats |
| **Application** | Framing message (length-prefix), versioning, idempotent handlers |

**Scenario phỏng vấn:** Thiết kế gateway nhận 100k WebSocket clients — threading model (one thread per core + epoll), memory per connection, broadcast.

### 3.5 Debug production: crash, deadlock, race

→ Chi tiết: **`C++ App/production_debugging.md`** (playbook on-call, sanitizer matrix, hang)

| Vấn đề | File chính |
|--------|------------|
| **Crash (SIGSEGV)** | `segmentation_fault.md` + `production_debugging.md` §3 |
| **Deadlock** | `production_debugging.md` §4, `multithreading_and_concurrency.md` |
| **Race** | `production_debugging.md` §5, TSan |
| **Hang / leak** | `production_debugging.md` §6, Valgrind/ASan |

**Quy trình STAR (tóm tắt):** thu core → `thread apply all bt` → TSan/ASan repro → fix + metric + postmortem  

### 3.6 System architecture & senior soft skills ⭐ (JD bắt buộc)

→ **Đọc đầy đủ:** [`C++ App/senior_engineering_behaviors.md`](C++%20App/senior_engineering_behaviors.md)

| JD phrase | Nội dung trong file |
|-----------|-------------------|
| **Strong system architecture thinking** | §1 — framework 5 bước, NFR real-time, trade-offs, ADR, anti-patterns |
| **Proactive problem-solving** | §2 — leading indicators, spike, game day |
| **High sense of ownership** | §3 — feature/system owner, on-call, MTTR STAR |
| **Code review** | §4 — C++ checklist, blocker vs nit, cách feedback |
| **Mentoring** | §5 — SBI, onboarding junior, conflict rewrite |

**Kỹ thuật bổ sung:** scale patterns → `system_design.md` · on-call → `production_debugging.md`

**Trước phỏng vấn:** điền **3–5 STAR stories** (§6 trong file trên) — architecture, proactive, ownership, review, mentor.

---

## 4. Checklist kỹ năng trước ngày phỏng vấn

### Kỹ thuật

- [ ] Giải thích được project C++ gần nhất: throughput, latency, threading model
- [ ] Vẽ được diagram 1 service: client → gateway → worker → storage
- [ ] Code tay: thread-safe queue / singleton / smart pointer skeleton (30–45 phút)
- [ ] Đọc được stack trace từ core dump
- [ ] Nêu được 3 cách optimize latency đã làm (có số đo trước/sau)
- [ ] So sánh TCP vs UDP cho use case cụ thể của bạn

### Hành vi (Senior)

- [ ] 2–3 câu chuyện STAR: conflict kỹ thuật, incident production, mentor junior
- [ ] Câu “sai lầm lớn nhất” — có bài học và process cải thiện
- [ ] Câu hỏi ngược cho interviewer (mục 7)

### Nice-to-have (nếu có background)

- [ ] WebRTC pipeline: capture → encode → RTP → network
- [ ] Jitter buffer, PLC (packet loss concealment)
- [ ] SFU vs MCU trong conference

---

## 5. Dự án / CV — Cách kể cho khớp JD

Chuẩn bị **2–3 project** theo template:

```
Tên / vai trò / thời gian
Bài toán: (real-time? bao nhiêu user/msg/s?)
Stack C++: standard, boost?, framework?
Kiến trúc: thread model, queue, protocol
Số đo: latency p99, CPU%, memory, crash rate
Thách thức: race / deadlock / spike traffic — bạn fix thế nào
Senior: review, design doc, on-call, mentor
```

**Từ khóa nên có trong CV (nếu đúng sự thật):**

`C++17/20`, `multithreading`, `TCP/UDP`, `WebSocket`, `low latency`, `epoll`/`asio`, `performance tuning`, `gdb`/`ASan`/`TSan`, `code review`, `mentoring`, `real-time`, `VoIP`/`WebRTC`/`RTP`

---

## 6. Câu hỏi phỏng vấn thường gặp (ôn đáp)

### C++ & memory

1. Rule of Five là gì? Khi nào Rule of Zero?
2. `std::move` có move không? (cast — move xảy ra ở constructor/assignment)
3. Exception safety: basic / strong / noexcept guarantee?
4. Tại sao destructor `virtual` khi có inheritance đa hình?

### Concurrency

5. Mutex vs `shared_mutex` vs spinlock — trade-off?
6. Giải thích memory order `release`/`acquire` trên một flag `ready`
7. Deadlock giữa thread A (lock1→lock2) và B (lock2→lock1) — fix?
8. `std::async` vs thread pool — khi nào dùng?

### Performance & systems

9. False sharing là gì? Ví dụ struct hai atomic cạnh nhau
10. Làm sao đo latency p99 trong production?
11. `epoll` vs `select`/`poll` — tại sao scale?

### Networking

12. TCP reliable như thế nào? UDP dùng khi nào cho “real-time”?
13. WebSocket khác raw TCP application protocol?
14. Thiết kế protocol binary: header + payload + checksum?

### Debug & ops

15. Process crash 2am — bạn làm gì từ đầu đến cuối?
16. TSan báo race — bạn đọc report và fix thế nào?

### Architecture & senior

17. Thiết kế service gửi notification real-time cho 1M devices
18. Trade-off: consistency vs availability (CAP) trong messaging
19. Bạn review code junior thế nào? Ví dụ feedback constructively

---

## 7. Câu hỏi nên hỏi interviewer

1. Team đang maintain sản phẩm nào (platform, IoT, super-app backend, media)?
2. Stack C++: version, build (CMake/Bazel), CI, coding standard?
3. Real-time có SLA latency cụ thể không (ms/µs)?
4. On-call rotation và incident culture?
5. Tỷ lệ greenfield vs legacy; có migration sang modern C++ không?
6. Cơ hội làm WebRTC/VoIP nếu nice-to-have?

---

## 8. Nice-to-have: Call / VoIP / WebRTC / Media

→ **Đọc đầy đủ:** [`C++ App/realtime_media_voip_webrtc.md`](C++%20App/realtime_media_voip_webrtc.md)

**Ôn nhanh trước phỏng vấn:**

- VoIP: PCM → Opus → **RTP/UDP** → jitter buffer → decode  
- WebRTC: **signaling** (WebSocket) tách khỏi **media** (SRTP/UDP); ICE + STUN/TURN  
- Scale call: **SFU** forward stream, không MCU mix mọi thứ  
- C++: libwebrtc / PJSIP / GStreamer — thread riêng cho network vs decode  

**WebSocket signaling:** [`C++ App/networking.md`](C++%20App/networking.md) § WebSocket

---

## 9. Coding test — chuẩn bị thực hành

| Loại | Ví dụ | Thời gian luyện |
|------|--------|-----------------|
| DS&A medium | LRU, top K, interval merge | 2–3 bài/tuần |
| Concurrency | producer-consumer, rate limiter | 1 bài/tuần |
| System mini | length-prefix decoder, simple epoll echo | 1 bài/tuần |
| C++ specific | implement smart ptr, string_view parser | Ôn từ `cpp_senior_interview_concepts.md` |

**Lưu ý senior:** Nói ra trade-offs, complexity, test cases, thread-safety **trước khi** code.

---

## 10. Ngày phỏng vấn

### Trước 24h

- Ôn 1 trang cheat: complexity, container chọn gì, atomic orders (README `C++ App`)
- In/ghi nhớ 3 STAR stories
- Test camera/mic nếu online; môi trường code IDE sẵn

### Trong phỏng vấn

- Clarify constraints trước khi design/code
- Think aloud — họ đánh giá **quá trình** senior
- Nếu không biết: nói assumption + hướng tìm hiểu, đừng bịa

### Sau phỏng vấn

- Ghi lại câu hỏi đã gặp để cập nhật file này

---

## 11. Về VinSmart Future — context ngắn

- **VSF:** công ty công nghệ lõi Vingroup, ~4000 engineers, AI + data + hạ tầng
- **Dự án:** quy mô lớn, chiến lược group — expect process (review, CI, cross-team)
- **Benefits JD:** flexible hours, WFH Saturday, Udemy/Coursera/O’Reilly, bảo hiểm — có thể hỏi thêm grade/policy

**Định vị câu trả lời “Why VSF?”:**

- Làm hệ thống core C++ scale lớn, impact sản phẩm ecosystem Vingroup  
- Môi trường senior, học từ expert quốc tế  
- Align tech stack (real-time, performance) với kinh nghiệm của bạn  

---

## 12. Tài liệu nội bộ — đọc theo thứ tự ưu tiên

| Ưu tiên | File |
|--------|------|
| P0 | `C++ App/multithreading_and_concurrency.md` |
| P0 | `C++ App/networking.md` (gồm § WebSocket mở rộng) |
| P0 | `C++ App/low_latency.md` |
| P0 | `C++ App/production_debugging.md` |
| P0 | `C++ App/segmentation_fault.md` |
| P1 | `C++ App/core_cpp_memory_and_ownership.md` |
| P1 | `C++ App/smart_pointers.md` |
| P1 | `C++ App/linux_concepts.md` |
| P1 | `C++ App/cpp_senior_interview_concepts.md` |
| **P1** | **`C++ App/senior_engineering_behaviors.md`** ← JD soft skills |
| P2 | `C++ App/system_design.md` |
| P2 | `C++ App/database_management.md` |
| P2 | `C++ App/realtime_media_voip_webrtc.md` |
| P2 | `C++ App/ipc_concepts.md` |
| Index | `README.md`, `C++ App/README.md` |

---

## 13. Ghi chú cá nhân (điền khi ôn)

| Mục | Nội dung của bạn |
|-----|------------------|
| Project #1 (real-time / network) | |
| Số đo latency / QPS đã đạt | |
| Incident production đã xử lý | |
| Điểm yếu cần ôn thêm | |
| Câu hỏi họ hỏi (sau vòng 1) | |

---

*Chúc bạn phỏng vấn tốt. Cập nhật file sau mỗi vòng để lần sau mạnh hơn.*
