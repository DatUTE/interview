# Senior Engineering Behaviors — Architecture, Ownership, Review, Mentoring

Tài liệu cho JD kiểu **Senior C++**: không chỉ code đúng mà còn **thiết kế hệ thống**, **chủ động giải quyết vấn đề**, **ownership end-to-end**, **review & mentor**.

**Liên quan kỹ thuật:** [system_design.md](system_design.md) (scale, CAP, patterns) · [production_debugging.md](production_debugging.md) (on-call) · [vsf_senior_cpp_vinsmart_future.md](../vsf_senior_cpp_vinsmart_future.md)

---

## 1. “Strong system architecture thinking” — nghĩa là gì?

Không phải vẽ diagram đẹp. Là **đặt câu hỏi đúng trước khi code** và **nói rõ trade-off**.

### 1.1 Framework 5 bước (dùng trong phỏng vấn design)

```
1. Requirements   — functional + non-functional (QPS, latency p99, availability)
2. Constraints    — team size, timeline, existing stack, compliance
3. High-level     — components, data flow, sync vs async
4. Deep dive      — bottleneck, failure mode, consistency
5. Trade-offs     — what you chose AND what you rejected
```

**Non-functional cho C++ real-time (VSF-style):**


| NFR              | Câu hỏi phải hỏi                             |
| ---------------- | -------------------------------------------- |
| **Latency**      | p50/p99 target? end-to-end hay per-hop?      |
| **Throughput**   | messages/s, connections, bytes/s?            |
| **Availability** | chấp nhận downtime bao lâu? degraded mode?   |
| **Consistency**  | at-most-once / at-least-once / exactly-once? |
| **Durability**   | mất message khi crash có chấp nhận không?    |
| **Operability**  | metric, log, trace, rollout, rollback?       |


### 1.2 Nguyên tắc kiến trúc cho hệ C++ hiệu năng cao


| Nguyên tắc                   | Ý nghĩa thực tế                                                        |
| ---------------------------- | ---------------------------------------------------------------------- |
| **Separation of concerns**   | I/O thread ≠ business thread ≠ encode thread                           |
| **Boundaries rõ**            | API module: input/output type, error contract, thread-safety doc       |
| **Fail fast, fail visible**  | Assert/dev, metric+alert prod; không swallow exception im lặng         |
| **Design for failure**       | timeout, retry có backoff, circuit breaker, idempotent handler         |
| **Observability by default** | conn_id, trace_id, latency histogram từ v1                             |
| **Evolution**                | versioning protocol/API; feature flag; không breaking silent           |
| **Simplicity first**         | mutex trước lock-free; monolith module trước microservice nếu team nhỏ |


### 1.3 Cách trình bày trade-off (mẫu câu)

> “Chúng tôi chọn **fan-out on write** cho group ≤100 vì read path O(1) và p99 ổn định. Với celebrity channel chúng tôi **fan-out on read** để tránh write amplification. Trade-off: read phức tạp hơn và cache miss cao hơn khi hot user post.”

> “Hot path **không allocate**: pool buffer + SPSC queue. Trade-off: memory cố định cao hơn và code phức tạp hơn `std::queue` + mutex.”

### 1.4 ADR (Architecture Decision Record) — ngắn gọn

```markdown
# ADR-012: WebSocket gateway dùng epoll + SO_REUSEPORT

## Context
100k concurrent connections, 1 region, Linux.

## Decision
N event-loop threads, mỗi core một loop; business logic qua thread pool.

## Consequences
+ Scale CPU tốt, ít lock trên I/O path
- Sticky LB hoặc shared pub/sub khi multi-node
- Khó debug hơn single-threaded prototype
```

Senior được kỳ vọng **viết hoặc đề xuất ADR** cho quyết định ảnh hưởng >1 team.

### 1.5 Anti-patterns (nói được trong phỏng vấn)

- **Big ball of mud** — mọi thứ include mọi thứ  
- **Distributed monolith** — microservice nhưng deploy chung, shared DB lock  
- **Resume-driven architecture** — Kafka khi chưa cần queue  
- **Golden hammer** — lock-free mọi nơi  
- **Invisible failures** — catch(...) rỗng, log không structured

→ Chi tiết scale patterns: [system_design.md](system_design.md)

---

## 2. Proactive problem-solving mindset

**Reactive:** đợi ticket / đợi production cháy.  
**Proactive:** thấy rủi ro / metric xấu → hành động trước khi thành incident.

### 2.1 Hành vi cụ thể


| Proactive                      | Ví dụ                                          |
| ------------------------------ | ---------------------------------------------- |
| **Monitor leading indicators** | queue depth tăng trước khi latency nổ          |
| **Chạy game day / load test**  | trước campaign, trước merge lớn                |
| **Tech debt có owner**         | ticket + deadline, không chỉ complain          |
| **Postmortem action items**    | close loop trong sprint                        |
| **Clarify ambiguity sớm**      | hỏi PM/BA khi requirement mơ hồ                |
| **Spike trước commit lớn**     | 2 ngày POC epoll vs asio → quyết định có số đo |


### 2.2 Template “phát hiện → giải quyết”

```
Signal:     metric / log / review comment / customer report
Hypothesis: nguyên nhân có thể (ranked)
Experiment: repro, benchmark, canary
Fix:        code + test + rollout plan
Prevent:    alert, runbook, guardrail CI
```

### 2.3 Câu trả lời mẫu phỏng vấn

> “Tôi thấy p99 latency tăng 40% sau deploy nhưng error rate vẫn 0. Tôi so sánh flame graph trước/sau, phát hiện thêm lock contention trên connection map. Tôi đề xuất sharded map + đo lại — p99 về baseline trong tuần đó, không đợi user complain.”

---

## 3. High sense of ownership

**Ownership** = chịu trách nhiệm **kết quả cho user và hệ thống**, không chỉ “merge PR của mình”.

### 3.1 Phạm vi ownership (Senior)

```
Feature spec ──▶ Design ──▶ Implement ──▶ Test ──▶ Deploy ──▶ Monitor ──▶ Incident ──▶ Postmortem
     ▲___________________________________________________________________________________________|
                              bạn vẫn quan tâm sau khi ship
```


| Level                | Mô tả                                              |
| -------------------- | -------------------------------------------------- |
| **Task owner**       | Làm xong ticket                                    |
| **Feature owner**    | End-to-end một capability                          |
| **System owner**     | Một service/domain: SLO, roadmap kỹ thuật, on-call |
| **Initiative owner** | Tự đề xuất cải thiện không ai giao                 |


### 3.2 Ownership vs “làm hết mọi thứ”

- **Delegate** phần rõ ràng; **review** và **unblock**  
- **Document** để người khác on-call được  
- **Escalate sớm** khi risk timeline / security — ownership không phải im lặng đến deadline trượt

### 3.3 Ngôn ngữ STAR cho ownership

- **Situation:** service mới, không ai có runbook on-call  
- **Task:** đảm bảo team support được production  
- **Action:** viết runbook, dashboard, alert; train 2 người backup  
- **Result:** MTTR giảm từ 2h → 25 phút; zero “chỉ mình A biết fix”

---

## 4. Code review — Senior làm gì?

Review không phải “tìm lỗi cú pháp” (CI/linter làm). Senior review **thiết kế, rủi ro, maintainability**.

### 4.1 Checklist C++ / systems (copy khi review)

**Correctness & safety**

- Thread-safety documented và đúng (mutex scope, atomic order)  
- Exception safety: destructor noexcept? RAII?  
- Lifetime: raw pointer/reference capture trong async/callback?  
- Bounds: buffer size, integer overflow  
- UB: signed overflow, strict aliasing, data race

**Performance (hot path)**

- Alloc trên hot path?  
- Copy không cần thiết? (`std::move`, string_view)  
- Lock scope tối thiểu?  
- False sharing struct layout?

**API & maintainability**

- Public API rõ: pre/post condition, error type  
- Breaking change có migration note?  
- Test: happy path + failure + concurrency nếu cần

**Operability**

- Log có context (conn_id, user_id)?  
- Metric cho failure path?  
- Config có default an toàn?

### 4.2 Cách feedback (không toxic)


| Tránh                    | Nên                                                                 |
| ------------------------ | ------------------------------------------------------------------- |
| “Code này tệ”            | “Concern: lock held khi gọi callback — risk deadlock”               |
| Chỉ nói vấn đề           | Đề xuất hướng: “Có thể dùng `scoped_lock` hoặc pass work qua queue” |
| Nitpick style 50 comment | Phân **blocker** vs **nit** vs **optional**                         |
| Review 2000 dòng một lúc | Yêu cầu chia PR nhỏ                                                 |


**Blocker:** data race, security, breaking prod without rollback.  
**Nit:** naming, style (nếu team có formatter thì ít debate style).

### 4.3 Câu hỏi phỏng vấn thường gặp

**[Mid]** Bạn review PR như thế nào?

> Đọc description + ticket trước. Scan architecture (file structure, public API). Đọc test trước hoặc song song implementation. Comment theo mức độ ưu tiên. Approve khi blocker resolved; không approve “để cho qua” nếu chưa có test concurrency cho code sensitive.

---

**[Senior]** Junior PR dùng `shared_ptr` everywhere và copy nặng trên hot path. Bạn phản hồi thế nào?

> Giải thích **measurement first** — profile trước/sau. Dạy ownership model: `unique_ptr` default, `shared_ptr` khi lifetime thật sự shared. Pair nếu cần refactor lớn. Ghi guideline ngắn vào team wiki để lặp lại không thành debate cá nhân.

---

## 5. Mentoring — Senior làm gì?

Mentoring ≠ làm hộ. Là **tăng tốc người khác** và **giảm bus factor**.

### 5.1 Hoạt động mentoring


| Hoạt động                | Mục tiêu                                         |
| ------------------------ | ------------------------------------------------ |
| **1:1 định kỳ**          | Career, blockers, feedback hai chiều             |
| **Pair programming**     | Transfer context hệ thống                        |
| **Review as teaching**   | Giải thích *why*, link doc nội bộ                |
| **Delegation có hỗ trợ** | Junior own subtask, senior review design trước   |
| **Runbook / brown bag**  | Chia sẻ debug production, perf tuning            |
| **Growth plan**          | Skill gap rõ: “3 tháng tới: TSan + epoll server” |


### 5.2 Framework phản hồi (SBI)

```
Situation: Trong incident X, em handle log chưa có correlation id
Behavior:   Em grep thủ công 2 giờ
Impact:     MTTR kéo dài, on-call stress
Next:       Lần sau thêm trace_id theo checklist runbook §2
```

### 5.3 Onboarding junior vào codebase C++

1. Map module: entry point, thread model, config
2. First PR nhỏ: test + doc fix (build confidence)
3. PR medium: bug có repro + reviewer pair
4. Own một metric/alert
5. Tham gia on-call shadow → primary với backup

### 5.4 Câu hỏi phỏng vấn

**[Mid]** Kể lần bạn giúp junior grow.

> Dùng STAR: junior stuck deadlock — bạn không fix hộ mà cùng vẽ lock graph, hướng dẫn TSan, để junior submit fix. Kết quả: junior tự handle incident tương tự sau đó.

---

**[Senior]** Conflict: junior muốn rewrite service; bạn nghĩ không cần. Xử lý?

> Nghe lý do (pain point cụ thể). Đặt tiêu chí thành công đo được (latency, bug rate, velocity). Đề xuất **spike time-boxed** 1 tuần thay vì rewrite 6 tháng. Quyết định bằng data + ADR; nếu spike fail thì có bằng chứng cho cả team. Giữ quan hệ — disagree and commit sau khi quyết định.

---

## 6. STAR story bank — chuẩn bị trước phỏng vấn

Điền **3–5 câu chuyện** (mỗi câu 2–3 phút nói):


| #   | Chủ đề JD        | Gợi ý tình huống                       | Metric kết quả          |
| --- | ---------------- | -------------------------------------- | ----------------------- |
| 1   | **Architecture** | Redesign gateway / protocol / sharding | p99, QPS, cost          |
| 2   | **Proactive**    | Phát hiện degradation trước incident   | alert, prevented outage |
| 3   | **Ownership**    | On-call, runbook, ship under deadline  | MTTR, SLA               |
| 4   | **Code review**  | Catch race/security trước prod         | bug avoided             |
| 5   | **Mentoring**    | Junior ship feature độc lập            | time-to-productivity    |


### Template điền trước

```markdown
### Story: [tên ngắn]
- **S:** Bối cảnh team / hệ thống / constraint
- **T:** Trách nhiệm của bạn (chủ động hay được giao?)
- **A:** 3–5 hành động cụ thể (kỹ thuật + collaboration)
- **R:** Số đo + bài học process (không chỉ "thành công")
```

### Ví dụ khung (thay bằng project thật)

**Architecture + ownership**

> S: Gateway WebSocket 20k conn, p99 tăng sau feature mới.  
> T: Tôi feature owner, phải ship campaign trong 3 tuần.  
> A: Profile → lock contention; design sharded conn map; ADR; load test; rollout canary.  
> R: p99 120ms→45ms; zero rollback; doc ADR cho team khác reuse pattern.

**Mentoring + review**

> S: Junior mới join, chưa quen concurrency.  
> T: Tôi mentor, cùng module messaging.  
> A: Pair trên producer-consumer; checklist review; junior fix race TSan found.  
> R: Junior lead PR module B sau 2 tháng; tôi giảm review iteration từ 5→2 vòng.

---

## 7. Map câu hỏi HR / Hiring Manager


| Câu hỏi                   | Hướng trả lời                                                     |
| ------------------------- | ----------------------------------------------------------------- |
| Why senior?               | Scope rộng hơn: design + unblock team + production accountability |
| Conflict với đồng nghiệp? | Data, ADR, escalate sớm, disagree & commit                        |
| Sai lầm lớn nhất?         | Thật + remediation + process đổi (blameless)                      |
| Làm việc áp lực?          | Prioritize, communicate risk, không hide                          |
| Mentor style?             | Socratic + checklist + measure                                    |


---

## 8. Ôn tập 1 tuần (soft skills)


| Ngày | Việc                                                                                |
| ---- | ----------------------------------------------------------------------------------- |
| 1    | Đọc §1 + làm 1 bài design nhỏ (chat gateway) — [system_design.md](system_design.md) |
| 2    | Viết 1 ADR giả định cho project cũ                                                  |
| 3    | Điền 3 STAR stories vào §6                                                          |
| 4    | Ôn checklist review §4 — review 1 PR open source hoặc code cũ của bạn               |
| 5    | Mock: “Tell me about a time you showed ownership” 3 phút                            |
| 6    | Mock: conflict + mentoring                                                          |
| 7    | Đọc lại [vsf](../vsf_senior_cpp_vinsmart_future.md) — align câu Why VSF             |


---

## 9. Interview Q&A tổng hợp

**[Senior]** What does "strong system architecture thinking" mean to you?

> Understanding requirements and constraints before choosing patterns; making trade-offs explicit; designing for failure and operability; validating with metrics and load tests—not guessing. In C++ real-time systems, that includes thread boundaries, latency budgets, and protocol evolution—not only box diagrams.

---

**[Senior]** How do you balance speed of delivery and technical quality?

> Ship thin vertical slices with clear SLO and feature flags. Use ADR for irreversible decisions. Pay down debt on touched code (“boy scout rule”). Block release on: data race in new code, missing rollback, no observability for new path—not on perfect abstraction day one.

---

**[Senior]** Describe your code review philosophy.

> Reviews are team quality gates and teaching moments. I prioritize correctness and production risk over style. I label severity, suggest alternatives, and approve when blockers are fixed. For seniors I focus on API contracts and cross-service impact; for juniors I add learning links and offer pairing.

---

## 10. Cross-references


| Topic                  | File                                                                         |
| ---------------------- | ---------------------------------------------------------------------------- |
| System design patterns | [system_design.md](system_design.md)                                         |
| Production / on-call   | [production_debugging.md](production_debugging.md)                           |
| VSF checklist          | [../vsf_senior_cpp_vinsmart_future.md](../vsf_senior_cpp_vinsmart_future.md) |
| Topic map              | [cpp_senior_interview_concepts.md](cpp_senior_interview_concepts.md)         |


