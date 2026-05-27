# Production Debugging — Crash, Deadlock, Race, Hang

Playbook cho hệ thống C++ multithreaded / real-time chạy production. Bổ sung cho [segmentation_fault.md](segmentation_fault.md) (SIGSEGV chi tiết) và [multithreading_and_concurrency.md](multithreading_and_concurrency.md) (deadlock theory).

---

## 1. Triage trong 15 phút đầu

```
Impact?     → bao nhiêu user / service down / SLO breach
Regression? → deploy / config / traffic spike gần đây
Signal?     → SIGSEGV, SIGABRT, hang (no CPU), 100% CPU
Artifact?   → core dump, logs, metrics (latency p99), trace id
Isolate?    → canary host, reduce traffic, feature flag off
```

**Không làm:** restart hàng loạt trước khi lấy core dump / thread dump (mất bằng chứng).

---

## 2. Sanitizer & tool matrix


| Tool         | Phát hiện                | Build flags                 | Ghi chú                         |
| ------------ | ------------------------ | --------------------------- | ------------------------------- |
| **ASan**     | UAF, heap OOB, leak      | `-fsanitize=address -g -O1` | ~2× RAM, chậm ~2×               |
| **TSan**     | Data race, deadlock      | `-fsanitize=thread -g -O1`  | Cần rebuild; không mix với ASan |
| **UBSan**    | UB (shift, null, …)      | `-fsanitize=undefined`      | Nhẹ hơn ASan                    |
| **LSan**     | Leak (thường kèm ASan)   | part of ASan                |                                 |
| **Valgrind** | leak, race (Helgrind)    | không cần rebuild           | Rất chậm                        |
| **gdb**      | post-mortem, live attach | `-g`                        | `thread apply all bt`           |
| **perf**     | CPU hot spot, cache miss | runtime                     | `perf record -g`                |


```bash
# Core dumps (Linux)
ulimit -c unlimited
echo '/tmp/core.%e.%p' | sudo tee /proc/sys/kernel/core_pattern

g++ -std=c++17 -g -O1 -fsanitize=thread server.cpp -o server_tsan -pthread
./server_tsan
```

**CI gợi ý:** chạy integration tests với ASan/TSan trên nightly; không bắt buộc mọi PR (thời gian build).

---

## 3. Crash (SIGSEGV / SIGABRT)

→ Chi tiết nguyên nhân SIGSEGV: [segmentation_fault.md](segmentation_fault.md)

### Post-mortem workflow

```bash
gdb ./service /path/to/core
(gdb) thread apply all bt full
(gdb) info threads
(gdb) frame N
(gdb) info locals
(gdb) p *ptr
```

**Đọc backtrace:**


| Dấu hiệu                          | Hướng                                     |
| --------------------------------- | ----------------------------------------- |
| `0x0` / offset nhỏ                | null deref                                |
| `0x41414141`, `0xdeadbeef`        | buffer overflow / corruption              |
| `malloc` / `free` trong stack     | heap corruption, double-free              |
| Nhiều frame giống nhau            | stack overflow                            |
| Crash thread khác thread giữ lock | deadlock hoặc use-after-free cross-thread |


### Multithreaded crash under load

Xem Q&A **[Senior]** trong [segmentation_fault.md](segmentation_fault.md) — thứ tự: core → phân loại địa chỉ → TSan → ASan → pattern (shared_ptr, iterator invalidation, callback lifetime).

---

## 4. Deadlock

### Coffman (cần cả 4)

1. Mutual exclusion
2. Hold and wait
3. No preemption
4. Circular wait

**Phá vòng:** lock ordering toàn cục, `std::scoped_lock(m1,m2)`, giảm scope lock, tránh lock trong callback.

### Chẩn đoán production

```bash
# Attach gdb (process vẫn chạy nhưng hang)
gdb -p <pid>
(gdb) thread apply all bt
# Tìm nhiều thread blocked trong pthread_mutex_lock / std::mutex::lock
```

```bash
# TSan deadlock report (ví dụ)
WARNING: ThreadSanitizer: lock-order-inversion (potential deadlock)
```

**pstack / lldb** (macOS): tương tự `thread backtrace all`.

### Phòng khi thiết kế

- Document lock hierarchy (Level 0: config, Level 1: connection map, Level 2: session)  
- Không gọi user callback khi đang giữ lock nội bộ  
- `try_lock` + timeout chỉ là **mitigation**, không thay thế ordering  
- Ưu tiên message passing (queue) thay vì nhiều mutex lồng nhau

→ Theory & Q&A: [multithreading_and_concurrency.md](multithreading_and_concurrency.md) § Deadlock

---

## 5. Data race vs race condition


|                | Data race                                                                 | Race condition                                  |
| -------------- | ------------------------------------------------------------------------- | ----------------------------------------------- |
| **Định nghĩa** | Hai thread truy cập cùng biến, ít nhất một ghi, không sync → **UB** (C++) | Kết quả phụ thuộc thứ tự scheduling (logic bug) |
| **Tool**       | TSan                                                                      | Code review, stress test, model checking        |
| **Fix**        | mutex, atomic, immutable data                                             | redesign protocol/state machine                 |


**TSan report đọc nhanh:**

```
WARNING: ThreadSanitizer: data race
  Write of size 8 at 0x... by thread T2
  Previous read of size 8 at 0x... by thread T1
    #0 Handler::on_message() server.cpp:142
```

Fix: bảo vệ vùng nhớ đó hoặc dùng atomic với ordering phù hợp.

`**shared_ptr`:** ref count atomic — **object** vẫn không thread-safe.

---

## 6. Hang (không crash)


| Triệu chứng                   | Nguyên nhân thường gặp                   |
| ----------------------------- | ---------------------------------------- |
| CPU ~0%, nhiều thread blocked | deadlock, chờ I/O, chờ lock              |
| CPU 100% một core             | spin loop không yield, busy poll sai     |
| CPU 100% nhiều core           | thundering herd, lock contention cực cao |
| Tăng latency, không crash     | GC-like pause, kernel, disk, lock convoy |


```bash
top -H -p <pid>          # thread CPU
strace -p <pid>          # syscall đang chờ gì
perf record -p <pid> -g -- sleep 30 && perf report
```

**Convoy:** thread priority inversion — low priority giữ lock, high priority chờ dài.

---

## 7. Observability cho real-time services

```cpp
// Structured log + correlation id
LOG_INFO("ws_connect", "conn_id", conn_id, "user_id", uid);

// Metrics (Prometheus-style)
connections_active.inc();
message_latency_ms.observe(elapsed);
dropped_messages.inc();
```

**Metric nên có:**

- Active connections, messages/s in/out  
- Queue depth (backpressure sớm)  
- p50/p99/p999 latency  
- Error rate by type (parse, auth, timeout)  
- Process: RSS, fds open, thread count

---

## 8. On-call playbook (STAR-friendly)

1. **Detect** — alert SLO / error rate / crash loop
2. **Mitigate** — rollback, scale, circuit breaker, drain node
3. **Diagnose** — core + logs + TSan repro in staging
4. **Fix** — code + test (unit + load + sanitizer CI)
5. **Postmortem** — timeline, root cause, action items (không đổ lỗi cá nhân)

**Câu phỏng vấn:** “Server crash 2am dưới load — bạn làm gì?”  
→ Thu core trước restart → `thread apply all bt` → TSan repro → fix ownership/race → thêm metric + regression test.

---

## 9. Interview Q&As

**[Mid]** Khi nào dùng ASan vs TSan?

> ASan: memory errors (UAF, overflow, leak). TSan: concurrency (race, deadlock). Build riêng từng bản. Trong CI có thể nightly cả hai. Production binary thường không ship sanitizer — dùng symbolized core dump (`-g` + separate debug symbols).

---

**[Senior]** Production hang, gdb cho thấy 8/8 worker threads trong `pthread_cond_wait` và main thread trong `mutex::lock`. Ý nghĩa?

> Workers đang chờ work hoặc điều kiện; main (hoặc một thread) đang chờ mutex mà có thể do worker nào đó giữ — hoặc ngược lại main giữ lock mà worker cần. Vẽ lock graph từ từng thread’s `bt`: ai giữ `mutex` nào, ai đang `lock()`. Tìm cycle. Fix ordering hoặc tách critical section.

---

**[Senior]** Làm sao tái hiện race hiếm gặp?

> TSan trên test load; stress test (nhiều thread, random timing); `std::this_thread::sleep_for` trong test để widen window; Thread Fuzzing (LLVM); giảm `-O` trong test build; chạy test lặp 1000× trong CI (`--gtest_repeat`).

---

## 10. Cross-references


| Topic                             | File                                                                         |
| --------------------------------- | ---------------------------------------------------------------------------- |
| SIGSEGV causes, ASan internals    | [segmentation_fault.md](segmentation_fault.md)                               |
| Deadlock, lock-free, backpressure | [multithreading_and_concurrency.md](multithreading_and_concurrency.md)       |
| epoll server, TCP tuning          | [networking.md](networking.md)                                               |
| Latency, jitter                   | [low_latency.md](low_latency.md)                                             |
| VSF checklist                     | [../vsf_senior_cpp_vinsmart_future.md](../vsf_senior_cpp_vinsmart_future.md) |


