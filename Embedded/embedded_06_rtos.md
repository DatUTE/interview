# Embedded Systems 06 — RTOS Concepts

---

## 6.1 Scheduling

**Preemptive vs Cooperative**

| Feature           | Preemptive                       | Cooperative               |
|-------------------|----------------------------------|---------------------------|
| Context switch    | At any time (timer tick or IRQ)  | Only when task yields     |
| Timing guarantees | Strong                           | Weak                      |
| Risk              | Race conditions on shared data   | Task must yield regularly |
| Examples          | FreeRTOS, Zephyr, ThreadX        | Arduino loop, baremetal   |

**Scheduling Algorithms**

- **Round-robin**: equal time slice per task at same priority
- **Priority preemptive**: highest priority ready task always runs
- **Rate Monotonic (RMA)**: assign priorities by period (shorter period = higher priority). Provably optimal for fixed-priority periodic tasks. CPU utilization bound: `n(2^(1/n) - 1)` (→ ~69% as n→∞)
- **EDF (Earliest Deadline First)**: dynamic priorities, optimal utilization up to 100%, complex to implement

**Context Switch Mechanics (ARM Cortex-M)**

On exception entry, hardware automatically saves {xPSR, PC, LR, R12, R0–R3} on stack. RTOS saves remaining registers (R4–R11) manually. Switch SP to new task's stack. On exception return, hardware restores registers.

---

## 6.2 Synchronization Primitives

**Semaphore**

Signaling mechanism. Binary (0 or 1) or counting (0 to N). No ownership concept.

```c
// ISR signals task — binary semaphore
SemaphoreHandle_t sem;

void ISR_Handler(void) {
    BaseType_t higher_woken;
    xSemaphoreGiveFromISR(sem, &higher_woken);
    portYIELD_FROM_ISR(higher_woken);
}

void task(void *pvParam) {
    while (1) {
        xSemaphoreTake(sem, portMAX_DELAY);  // blocks until ISR signals
        process_event();
    }
}
```

**Mutex**

Like binary semaphore but with **ownership** and **priority inheritance**. Only the task that took the mutex can give it back.

```c
MutexHandle_t mutex = xSemaphoreCreateMutex();

void task_a(void *p) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    // critical section — shared resource
    shared_counter++;
    xSemaphoreGive(mutex);
}
```

**Priority Inversion**

Classic problem:

```
Time →
T_high (P=3):   ──BLOCKED by mutex──────────RUNS
T_medium (P=2):         RUNS (preempts T_low!)──
T_low (P=1):  RUNS─holds mutex─(preempted)──gives mutex
```

T_high blocked waiting for T_low to release mutex. T_medium preempts T_low. T_high effectively runs at T_low's priority.

**Priority Inheritance Protocol (PIP)**

When T_high blocks on T_low's mutex, T_low temporarily inherits T_high's priority. T_medium can no longer preempt T_low. T_low finishes, releases mutex, T_high runs.

FreeRTOS mutexes implement PIP automatically.

**Priority Ceiling Protocol (PCP)**

Each mutex has a fixed ceiling = highest priority of any task that will ever lock it. A task's priority is raised to ceiling immediately upon locking. Prevents deadlock AND priority inversion. Requires knowing all lock relationships at design time.

---

## 6.3 Deadlock

Four necessary conditions (Coffman):
1. **Mutual exclusion**: resource held exclusively
2. **Hold and wait**: task holds resource while waiting for another
3. **No preemption**: resource cannot be forcibly taken
4. **Circular wait**: T1 waits for T2's resource, T2 waits for T1's resource

**Prevention:**
- Always acquire mutexes in the same fixed global order
- Try-lock with timeout and back off
- Use a single "super-mutex" (coarser granularity)

---

## 6.4 FreeRTOS Specifics

**Task Creation**

```c
void my_task(void *pvParam) {
    uint32_t *period_ms = (uint32_t*)pvParam;
    TickType_t last_wake = xTaskGetTickCount();

    while (1) {
        do_work();
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(*period_ms)); // precise period
    }
}

// Create with 512-word stack, priority 3
xTaskCreate(my_task, "MyTask", 512, &period_ms, 3, NULL);
```

**vTaskDelay vs vTaskDelayUntil**

```c
// vTaskDelay: delay FROM end of execution → drift accumulates
vTaskDelay(pdMS_TO_TICKS(10));

// vTaskDelayUntil: delay to next absolute tick → no drift (periodic tasks)
TickType_t last = xTaskGetTickCount();
vTaskDelayUntil(&last, pdMS_TO_TICKS(10));
```

**Queues**

```c
QueueHandle_t q = xQueueCreate(10, sizeof(uint32_t));

// Producer
uint32_t val = 42;
xQueueSend(q, &val, portMAX_DELAY);

// Consumer
uint32_t received;
xQueueReceive(q, &received, portMAX_DELAY);
```

**Critical Sections**

```c
// In task context: disables all interrupts at and below configMAX_SYSCALL_INTERRUPT_PRIORITY
taskENTER_CRITICAL();
shared_var++;
taskEXIT_CRITICAL();

// In ISR: uses BASEPRI, must be called from ISR
UBaseType_t saved = taskENTER_CRITICAL_FROM_ISR();
shared_var++;
taskEXIT_CRITICAL_FROM_ISR(saved);
```

**Stack Watermark (debugging)**

```c
// High water mark: minimum stack free space since task started (words)
UBaseType_t water = uxTaskGetStackHighWaterMark(NULL);
if (water < 20) {
    // danger: stack nearly exhausted, increase task stack size
}
```

---

## 6.5 Real-Time Concepts

**Definitions**

- **Period (T)**: how often a task executes
- **Deadline (D)**: by when must it complete (often D = T)
- **WCET (C)**: worst-case execution time
- **Utilization (U)**: C/T — fraction of CPU consumed

**Hard vs Soft Real-Time**

| Type   | Missing deadline             | Example                    |
|--------|------------------------------|----------------------------|
| Hard   | System failure / safety issue | Airbag controller, ABS     |
| Soft   | Degraded experience          | Video streaming, UI refresh|
| Firm   | Result useless but no failure | Industrial monitoring      |

**Jitter**

Variation in actual execution time from scheduled time. Sources:
- Interrupt latency variations
- Cache misses
- Bus contention
- Scheduler overhead

---

## Interview Questions

### Easy

**Q: What is the difference between preemptive and cooperative scheduling?**

> **Preemptive**: the scheduler can switch tasks at any time — on a timer tick or when a higher-priority task becomes ready. Guarantees response time for high-priority tasks. Risk: need mutex/critical sections to protect shared data. **Cooperative**: tasks run until they voluntarily yield (`vTaskDelay`, `xQueueReceive`). Simpler — no races if tasks don't share state. Risk: a task that never yields starves everyone. FreeRTOS is preemptive by default.

**Q: What is the difference between a binary semaphore and a mutex in FreeRTOS?**

> **Binary semaphore**: a signaling primitive — any task (or ISR) can give it, any task can take it. No ownership. No priority inheritance. Used for ISR-to-task signaling. **Mutex**: a mutual exclusion primitive with ownership — only the task that took it can give it back. FreeRTOS mutexes implement priority inheritance (PIP) to prevent priority inversion. Never take a mutex in an ISR — use `xSemaphoreGiveFromISR` with a binary semaphore for ISR signaling.

**Q: What does `vTaskDelayUntil` do and why is it better than `vTaskDelay` for periodic tasks?**

> `vTaskDelay(N)` delays N ticks from the end of the current execution — drift accumulates over time because task execution time adds to the period. `vTaskDelayUntil(&last_wake, N)` delays until an absolute tick count, so the period is exactly N ticks regardless of how long the task took. Use `vTaskDelayUntil` for anything with a fixed period (sensor polling, control loops, communication timeouts).

---

### Mid

**Q: Describe priority inversion with a concrete example involving three tasks.**

> Tasks: T_high (P=3), T_medium (P=2), T_low (P=1). Scenario: T_low acquires mutex M. T_high preempts T_low and tries to acquire M — blocks. T_medium (which doesn't use M) becomes ready and preempts T_low. T_low can't run because T_medium has higher priority. T_high is now effectively blocked at T_low's priority. T_medium runs indefinitely, delaying T_high. This is "priority inversion" — a high-priority task is delayed by a medium-priority task it doesn't even interact with.

**Q: What are the four Coffman conditions for deadlock? Which is easiest to break in embedded?**

> (1) **Mutual exclusion** — resources held exclusively. (2) **Hold and wait** — task holds a resource while waiting for another. (3) **No preemption** — resource cannot be forcibly taken. (4) **Circular wait** — T1 waits for T2's resource, T2 waits for T1's. Easiest to break in embedded: **circular wait** — enforce a global lock ordering (always acquire mutex A before mutex B, never the other way). Document the ordering in code. `static_assert` or a lock hierarchy checker (e.g., lockdep) can enforce this at test time.

**Q: After adding a new high-priority task, your 10ms sensor task starts drifting. Why?**

> `vTaskDelay(10)` causes drift: if the sensor task runs for 2ms, the next execution starts at tick 12, not tick 10. Over time, drift accumulates. The new high-priority task may be stealing CPU time during the sensor task's execution, making it longer. Fix: use `vTaskDelayUntil` — it delays to the next absolute tick regardless of execution time. Also audit whether the new high-priority task can preempt the sensor task mid-execution and whether that affects its timing.

---

### Hard

**Q: Tasks A (P=3) and C (P=1) share mutex M1. Tasks B (P=2) and C share mutex M2. Describe a deadlock scenario and the prevention.**

> Deadlock scenario: C acquires M2. A preempts, acquires M1. B becomes ready and preempts C. B tries to acquire M2 — blocks on C. A tries to acquire M2 — blocks on C. C is blocked by B on M2. C cannot run because B has higher priority and is blocked waiting for C's M2. A waits for M2 (held by C), B waits for M2 (held by C). C can't run because... actually this is a priority inversion, not deadlock. For deadlock: C holds M2, tries M1. A holds M1, tries M2. → Circular wait. Fix: enforce lock ordering: always acquire M1 before M2. C must release M2, acquire M1 first, then M2.

**Q: In FreeRTOS, `configMAX_SYSCALL_INTERRUPT_PRIORITY` is set to priority level 5. An interrupt at hardware priority 3 calls `xQueueSendFromISR`. What happens?**

> This is a bug. `configMAX_SYSCALL_INTERRUPT_PRIORITY` = 5 means only interrupts at priority 5 or lower (numerically ≥ 5 on ARM where lower numbers = higher priority) may safely call FreeRTOS `FromISR` APIs. Priority 3 is above (more urgent than) the max syscall priority — FreeRTOS's internal critical sections (`taskENTER_CRITICAL`) do NOT mask this interrupt. The interrupt can fire inside FreeRTOS's own internal state manipulation, corrupting the kernel data structures. Result: random hard faults or corrupted queue state, often non-reproducible. Fix: either raise the interrupt priority to ≥ 5, or use a higher-priority ISR to set a flag and process in a lower-priority ISR/task.

**Q: Why does Rate Monotonic Analysis give a utilization bound of ~69%? What does it mean practically for a 4-task system with total utilization of 75%?**

> The RMA bound is `U_bound = n × (2^(1/n) - 1)`. As n → ∞, this converges to `ln(2) ≈ 0.693`. For n=4: `U_bound = 4 × (2^(0.25) - 1) = 4 × 0.1892 ≈ 75.7%`. So a 4-task system at 75% total utilization is provably schedulable under RMA — all deadlines will be met. If utilization exceeds the bound, it does NOT mean the system will miss deadlines — it means schedulability is not provably guaranteed by this test alone. You'd need a response time analysis or simulation to determine if it actually works. Practical implication: design for <70% utilization to leave headroom for interrupts, OS overhead, and future tasks.
