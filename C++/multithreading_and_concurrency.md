# Multithreading & Concurrency in C++

> **Related:** [production_debugging.md](production_debugging.md) — deadlock/race diagnosis in production

---

## 1. `std::thread`, `std::async`, `std::future` / `std::promise`

### `std::thread` — Raw Thread

```cpp
#include <thread>

void worker(int id) {
    std::cout << "thread " << id << "\n";
}

std::thread t(worker, 42);  // launch thread
t.join();                   // block until t finishes
// t.detach();              // let t run independently (fire and forget)
```

**Rules**:

- A `std::thread` object must be either `join()`ed or `detach()`ed before it's destroyed — otherwise `std::terminate` is called.
- Use RAII wrapper (`jthread` in C++20) to avoid forgetting.

### `std::jthread` (C++20) — Auto-joining Thread

```cpp
{
    std::jthread t(worker, 42);
    // automatically joined on destruction — no explicit join() needed
}  // t.join() called here
```

Also supports **cooperative cancellation** via `std::stop_token`:

```cpp
std::jthread t([](std::stop_token st) {
    while (!st.stop_requested()) {
        do_work();
    }
});
t.request_stop();  // signal the thread to stop
```

---

### `std::async` — Task-Based Parallelism

Higher level than `std::thread` — returns a `std::future` to retrieve the result:

```cpp
#include <future>

auto fut = std::async(std::launch::async, []() {
    return compute_heavy_result();
});

// do other work here...

int result = fut.get();  // blocks until done, returns value or rethrows exception
```

**Launch policies**:


| Policy                                       | Behavior                                                  |
| -------------------------------------------- | --------------------------------------------------------- |
| `std::launch::async`                         | Always runs on a new thread                               |
| `std::launch::deferred`                      | Runs lazily on the calling thread when `.get()` is called |
| `std::launch::async | std::launch::deferred` | Implementation-defined (default) — **avoid this**         |


**Always specify `launch::async` explicitly** — the default policy may run deferred, masking race conditions.

---

### `std::future` / `std::promise` — Communication Channel

`promise` is the write end, `future` is the read end. They share a state.

```cpp
std::promise<int> prom;
std::future<int>  fut = prom.get_future();

std::thread producer([&prom]() {
    int result = do_work();
    prom.set_value(result);           // unblocks fut.get()
    // prom.set_exception(eptr);      // or propagate an exception
});

int val = fut.get();  // blocks until set_value called
producer.join();
```

`**std::shared_future**`: multiple threads can wait on the same result:

```cpp
std::shared_future<int> sf = prom.get_future().share();
// sf can be copied and .get() called from multiple threads
```

---

## 2. Mutex Types

### `std::mutex` — Basic Exclusive Lock

```cpp
#include <mutex>

std::mutex mtx;
int shared_data = 0;

void increment() {
    std::lock_guard<std::mutex> lock(mtx);  // RAII lock — released on scope exit
    ++shared_data;
}
```

**Never lock manually** — always use RAII wrappers:


| Wrapper                    | Behavior                                                                             |
| -------------------------- | ------------------------------------------------------------------------------------ |
| `std::lock_guard`          | Lock on construct, unlock on destroy. Not movable.                                   |
| `std::unique_lock`         | Like `lock_guard` but deferred locking, timed locking, movable, unlockable mid-scope |
| `std::scoped_lock` (C++17) | Lock multiple mutexes atomically — deadlock-safe                                     |


```cpp
// unique_lock — deferred locking
std::unique_lock<std::mutex> lk(mtx, std::defer_lock);
// ... do stuff ...
lk.lock();    // lock later
lk.unlock();  // unlock mid-scope if needed
lk.lock();    // re-lock

// scoped_lock — acquire multiple mutexes safely
std::scoped_lock lock(mtx1, mtx2);  // no deadlock from ordering
```

---

### `std::recursive_mutex`

Allows the **same thread** to lock the mutex multiple times without deadlocking:

```cpp
std::recursive_mutex rmtx;

void outer() {
    std::lock_guard lock(rmtx);
    inner();  // calls inner which also locks rmtx — ok with recursive_mutex
}

void inner() {
    std::lock_guard lock(rmtx);  // same thread — allowed
    // ...
}
```

**Use sparingly** — usually signals a design problem (can be replaced by splitting public/private methods where private ones assume the lock is held).

---

### `std::shared_mutex` (C++17) — Reader-Writer Lock

Multiple readers OR one exclusive writer:

```cpp
#include <shared_mutex>

std::shared_mutex rw_mtx;
std::map<int, std::string> cache;

// Multiple readers can hold this simultaneously
std::string read(int key) {
    std::shared_lock lock(rw_mtx);  // shared (read) lock
    return cache.at(key);
}

// Only one writer at a time, blocks all readers
void write(int key, std::string val) {
    std::unique_lock lock(rw_mtx);  // exclusive (write) lock
    cache[key] = std::move(val);
}
```

**When to use**: high read-to-write ratio. If writes are frequent, the overhead of `shared_mutex` may exceed a plain `mutex`.

---

## Atomic vs Mutex vs Semaphore — Quick Comparison

These three solve different synchronization problems:

- `std::atomic`: protect **one variable / one small state machine** without blocking.
- `std::mutex`: protect a **critical section / shared object invariant** with exclusive ownership.
- `std::counting_semaphore`: control **how many threads may enter** or signal that **N resources/events are available**.

### Summary Table

| Tool | Protects / Controls | Waiting Behavior | Best For | Not Good For |
|------|---------------------|------------------|----------|--------------|
| `std::atomic<T>` | One atomic value or lock-free state | Usually no sleep; may spin/retry | Counters, flags, reference counts, SPSC indexes | Protecting multi-field object invariants |
| `std::mutex` | Exclusive access to a critical section | Blocks/sleeps if locked | Shared containers, complex state, multi-step invariants | Very hot tiny counters, signal/counting use cases |
| `std::counting_semaphore<N>` | A count of available permits | Blocks until permit available | Resource pools, rate limits, producer-consumer slots/items | Protecting arbitrary shared data by itself |

Rule of thumb:

```text
Need atomic increment / publish flag?
  -> atomic

Need to update multiple fields consistently?
  -> mutex

Need to limit concurrency or count available items?
  -> semaphore
```

### `std::atomic` Example — Counter / Flag

```cpp
#include <atomic>

std::atomic<uint64_t> requests{0};

void on_request() {
    requests.fetch_add(1, std::memory_order_relaxed);
}
```

Use atomic when the operation itself is the synchronization boundary. A relaxed counter is safe because no other data depends on the counter's ordering.

Bad atomic use:

```cpp
std::atomic<bool> ready{false};
std::vector<int> data;

// Atomic flag alone does not make vector mutation safe if multiple threads write/read data.
```

If multiple fields must change together, use a mutex.

### `std::mutex` Example — Protect Object Invariant

```cpp
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>

std::mutex mtx;
std::unordered_map<int, std::string> users;

void update_user(int id, std::string name) {
    std::lock_guard lock(mtx);
    users[id] = std::move(name);
}
```

The mutex protects the container's internal structure and the invariant that readers/writers see a consistent map.

### `std::counting_semaphore` Example — Limit Concurrency

`std::counting_semaphore` is available in C++20 via `<semaphore>`.

```cpp
#include <semaphore>

std::counting_semaphore<8> slots{8};  // at most 8 concurrent users

void call_remote_service() {
    slots.acquire();   // block until one permit is available
    try {
        do_rpc();
    } catch (...) {
        slots.release();
        throw;
    }
    slots.release();
}
```

Better with RAII:

```cpp
class SemaphorePermit {
public:
    explicit SemaphorePermit(std::counting_semaphore<8>& sem) : sem_(sem) {
        sem_.acquire();
    }

    ~SemaphorePermit() {
        sem_.release();
    }

    SemaphorePermit(const SemaphorePermit&) = delete;
    SemaphorePermit& operator=(const SemaphorePermit&) = delete;

private:
    std::counting_semaphore<8>& sem_;
};

void call_remote_service() {
    SemaphorePermit permit(slots);
    do_rpc();
}
```

Semaphore by itself does **not** protect shared data. It only manages permits. If threads enter and then mutate shared state, that state may still need a mutex or atomics.

### Semaphore vs Condition Variable

| Aspect | Semaphore | Condition Variable |
|--------|-----------|--------------------|
| Stores count? | Yes, permits are remembered | No, notification can be lost without predicate |
| Needs mutex? | Not for the permit count itself | Yes, protects predicate/state |
| Typical use | Limit concurrency, count available items | Wait until a condition on shared state becomes true |
| Example | "Only 8 RPCs at once" | "Queue is not empty" |

Producer-consumer can be implemented with either:

- `mutex + condition_variable`: protects the queue and waits on predicates.
- `semaphore + mutex`: semaphores count slots/items; mutex still protects the queue.

### Choosing Correctly

| Scenario | Use |
|----------|-----|
| Global metrics counter | `std::atomic<uint64_t>` |
| Stop flag / readiness flag | `std::atomic<bool>` with correct memory order, or `mutex + condition_variable` if blocking |
| Shared `std::map`, `std::vector`, object graph | `std::mutex` |
| Multiple readers, rare writers | `std::shared_mutex` |
| Bounded connection pool | `std::counting_semaphore` + mutex-protected pool |
| Thread pool task queue | `mutex + condition_variable`, or semaphore for item count plus mutex for queue |
| Lock-free SPSC ring buffer | `std::atomic<size_t>` indexes |

### Common Mistakes

| Mistake | Why Wrong |
|---------|-----------|
| Using `atomic` for one flag while reading non-atomic shared data incorrectly | Atomicity of the flag does not automatically protect the payload |
| Using `mutex` for a hot statistics counter | Correct but may be slower than relaxed atomic |
| Using `semaphore` as if it protected a container | Semaphore controls entry/count; container still needs synchronization |
| Calling `release()` too many times on a semaphore | Permit count becomes wrong, allowing too much concurrency |
| Forgetting RAII around `acquire()` / `release()` | Exceptions/early returns can leak permits |

---

## 3. `std::atomic` and Memory Orders

### `std::atomic<T>` — Lock-Free Shared Variable

Guarantees that read-modify-write operations on `T` are indivisible — no partial reads or torn writes.

```cpp
#include <atomic>

std::atomic<int> counter{0};

void thread_fn() {
    counter.fetch_add(1, std::memory_order_relaxed);  // atomic increment
    ++counter;  // equivalent — uses seq_cst by default
}
```

Common operations: `load`, `store`, `fetch_add`, `fetch_sub`, `exchange`, `compare_exchange_weak/strong`.

---

### Memory Orders — The Core Concept

#### Vấn đề gốc: compiler và CPU reorder

Trên **một thread**, compiler/CPU có thể **đổi thứ tự** read/write (miễn không đổi kết quả observable của thread đó) để tối ưu.

Trên **nhiều thread**, reorder + cache khiến thread khác **không thấy** thứ tự bạn “viết trong code”:

```cpp
// Thread 1                    // Thread 2 có thể thấy:
data = 42;                     ready == true  nhưng data vẫn 0  (reorder!)
ready = true;
```

`std::mutex` ngăn reorder qua lock/unlock. `std::atomic` + **memory order** cho phép sync **nhẹ hơn** mutex khi pattern rõ (SPSC queue, flag publish).

#### Happens-before (quan hệ bạn cần nhớ)

**A happens-before B** ⇒ mọi side effect của A **visible** với thread thực hiện B.

Nguồn happens-before trong C++:

- Cùng thread: thứ tự program  
- `mutex::unlock()` → `mutex::lock()` trên cùng mutex  
- **release** trên atomic X → **acquire** trên cùng X (cùng giá trị / CAS success)  
- `thread::join()` sau khi thread kết thúc  

**Data race** = hai thread truy cập cùng vùng nhớ, ít nhất một ghi, **không** có happens-before → **UB**.

#### Bảng `std::memory_order` (C++11)

```
Yếu (nhanh, ít fence)  ←————————————————————————→  Mạnh (chậm hơn trên ARM/POWER)
relaxed    consume    acquire    release    acq_rel    seq_cst
```

| Order | Dùng cho | Đảm bảo |
|-------|----------|---------|
| **relaxed** | Counter thống kê | Chỉ atomicity op đó; **không** order với memory khác |
| **consume** | Đọc con trỏ rồi dereference data phụ thuộc | Acquire yếu trên **data dependency** (hiếm dùng trực tiếp) |
| **acquire** | **Load** sau khi thread khác **release** | Mọi **read** sau load thấy write trước release khớp |
| **release** | **Store** “publish” dữ liệu đã ghi xong | Mọi **write** trước store visible sau acquire khớp |
| **acq_rel** | RMW (`fetch_add`, `compare_exchange`) | Vừa acquire vừa release trên một op |
| **seq_cst** | Default; đồng thuận global | Total order trên mọi `seq_cst` — mạnh nhất |

**Quy tắc thực hành:**

```
relaxed     → chỉ đếm / stat, không publish data khác
release     → store sau khi ghi xong payload (producer)
acquire     → load trước khi đọc payload (consumer)
acq_rel     → CAS / fetch trên biến “điều phối”
seq_cst     → mặc định; dùng khi chưa chắc / nhiều atomic liên quan logic
```

**`volatile` ≠ memory order** — chỉ chống compiler cache register; không thread-safe, không happens-before → [cpp_keywords.md](cpp_keywords.md) § volatile.

---

### `memory_order_relaxed`

Chỉ đảm bảo **một** thao tác atomic không bị torn; **không** tạo happens-before với biến thường khác.

```cpp
std::atomic<uint64_t> packets_processed{0};

void on_packet() {
    packets_processed.fetch_add(1, std::memory_order_relaxed);
    // OK: chỉ cần đếm đúng, không thread nào đọc biến khác "dựa vào" thứ tự với counter
}
```

**Không an toàn:**

```cpp
std::atomic<bool> ready{false};
int data = 0;
data = 42;
ready.store(true, std::memory_order_relaxed);  // BUG: consumer có thể thấy ready mà data chưa 42
```

---

### `memory_order_release` + `memory_order_acquire` (pattern phổ biến nhất)

**Release-store:** mọi **write** trước đó trong thread producer → visible với consumer thực hiện **acquire-load** thấy giá trị đó.

**Acquire-load:** mọi **read** sau load → thấy write đã release.

```cpp
std::atomic<bool> ready{false};
int data = 0;

// Producer
void producer() {
    data = 42;                                        // (A) plain write — chỉ publish qua (B)
    ready.store(true, std::memory_order_release);     // (B) release
}

// Consumer
void consumer() {
    while (!ready.load(std::memory_order_acquire)) {} // (C) acquire
    assert(data == 42);                               // (D) OK — (A) happens-before (D)
}
```

Timeline (logic):

```
Producer CPU:     [ write data=42 ] ----release----> ready=true
Consumer CPU:                      acquire<-------- [ read data==42 ]
```

**Hình ảnh:** **release = publish**, **acquire = subscribe** (chỉ cần đồng bộ **một cặp** thread, không cần global order).

**Publish pointer (lock-free handoff):**

```cpp
std::atomic<int*> ptr{nullptr};
int data = 0;

// Producer
data = 42;
ptr.store(&data, std::memory_order_release);

// Consumer
int* p;
while (!(p = ptr.load(std::memory_order_acquire))) {}
assert(*p == 42);
```

---

### `memory_order_consume` (hiếu ít dùng trực tiếp)

Giống acquire nhưng chỉ order các op **phụ thuộc data** từ giá trị load (dependency chain). Compiler có thể reorder các read **không** phụ thuộc.

- Trên thực tế nhiều compiler implement **consume ≈ acquire**  
- Chuẩn khó implement tối ưu → **thường dùng `acquire` thay consume**  
- `atomic_load_explicit(p, memory_order_consume)` khi đọc con trỏ publish

---

### `memory_order_acq_rel`

Cho **read-modify-write** vừa “nhận” state cũ vừa “publish” state mới:

```cpp
// compare_exchange: nếu success → release previous writes + acquire sees prior publishes
head_.compare_exchange_weak(
    expected, new_head,
    std::memory_order_release,
    std::memory_order_relaxed);  // failure: chỉ cần relaxed reload expected
```

`fetch_add` với `memory_order_acq_rel` khi counter điều phối logic phức tạp (ít gặp hơn release/acquire tách).

---

### `memory_order_seq_cst` (default)

```cpp
counter.fetch_add(1);  // = memory_order_seq_cst
```

- Mọi `seq_cst` op trên **mọi** atomic tạo **một thứ tự tổng** mọi thread đồng ý  
- Mạnh hơn release/acquire (chỉ sync **cặp** release→acquire)  
- Trên **x86** load/store thường đã khá mạnh; **ARM** chênh lệch seq_cst vs relaxed rõ hơn  

**Khi cần seq_cst:** nhiều flag/atomic tạo invariant logic chéo (ví dụ Dekker-style), hoặc **chưa chứng minh** release/acquire đủ — dùng default rồi optimize sau khi profile.

**Ví dụ cần global order (seq_cst):**

```cpp
std::atomic<bool> flag_a{false}, flag_b{false};
// Thread 1: flag_a=true; if (!flag_b.load()) { /* ... */ }
// Thread 2: flag_b=true; if (!flag_a.load()) { /* ... */ }
// seq_cst: không cả hai nhánh "cả hai false" cùng lúc (với pattern cụ thể)
```

---

### SPSC ring buffer — tại sao relaxed + acquire + release?

```cpp
// Producer try_push:
size_t tail = tail_.load(relaxed);           // chỉ đọc index local
if (next == head_.load(acquire)) return false; // acquire: thấy consumer đã advance head
buf_[tail] = item;                           // write payload
tail_.store(next, release);                  // release: consumer acquire tail thấy item

// Consumer try_pop: đối xứng (acquire tail, release head)
```

| Op | Order | Lý do |
|----|-------|-------|
| load index “của mình” | relaxed | Chỉ thread đó ghi index đó |
| load index “của bên kia” | acquire | Đồng bộ với release store bên kia |
| store index sau ghi buffer | release | Publish item đã ghi |

---

### Lỗi thường gặp

| Lỗi | Hậu quả |
|-----|---------|
| Plain `bool ready` + không mutex | Data race — UB |
| `relaxed` cho flag publish data | Consumer thấy flag trước data |
| Quên acquire sau khi CAS success | Đọc payload stale |
| Mặc định `seq_cst` mọi nơi | Đúng nhưng chậm hơn cần thiết trên ARM |
| Nghĩ `atomic` bảo vệ **object** mà `shared_ptr` trỏ tới | Chỉ ref count atomic; object vẫn cần sync |
| Dùng `volatile` thay `atomic` | Không đủ multi-core |

---

### Chọn memory order — flowchart

```
Chỉ tăng counter / metric, không ai đọc data liên quan?
  YES → relaxed

Một thread ghi data, thread khác đợi signal/ index rồi đọc?
  Producer store flag/index → release
  Consumer load flag/index  → acquire
  (ghi buffer TRƯỚC release)

RMW (CAS, fetch) trên biến điều phối?
  → acq_rel (success) / relaxed (failure) tùy algorithm

Nhiều atomic tạo invariant phức tạp / chưa chắc?
  → seq_cst (default), optimize sau
```

---

### `compare_exchange` — Foundation of Lock-Free

```cpp
// Atomically: if (*this == expected) { *this = desired; return true; }
//             else { expected = *this; return false; }

std::atomic<int> val{0};
int expected = 0;

if (val.compare_exchange_strong(expected, 1)) {
    // we atomically changed 0 → 1
} else {
    // val was not 0 — expected now holds the actual value
}

// compare_exchange_weak — may fail spuriously (use in a loop)
// compare_exchange_strong — never spurious (use for single-shot)
while (!val.compare_exchange_weak(expected, expected + 1)) {}
```

---

## 4. Deadlock, Livelock, Starvation

### Deadlock

Two or more threads each hold a resource the other needs — everyone waits forever.

**Classic example**:

```cpp
std::mutex mtxA, mtxB;

void thread1() {
    std::lock_guard lockA(mtxA);  // holds A
    std::lock_guard lockB(mtxB);  // waits for B → deadlock if thread2 holds B
}

void thread2() {
    std::lock_guard lockB(mtxB);  // holds B
    std::lock_guard lockA(mtxA);  // waits for A → deadlock
}
```

**Four conditions for deadlock** (all must hold):

1. Mutual exclusion — resource can't be shared
2. Hold and wait — holding one while waiting for another
3. No preemption — can't forcibly take a resource
4. Circular wait — A waits for B, B waits for A

**Prevention strategies**:

```cpp
// 1. Consistent lock ordering — always acquire in the same order
void thread1() { lock(mtxA); lock(mtxB); }
void thread2() { lock(mtxA); lock(mtxB); }  // same order — no cycle

// 2. std::scoped_lock — acquires multiple mutexes atomically (uses try-and-backoff)
std::scoped_lock lock(mtxA, mtxB);  // safe regardless of order

// 3. std::lock + std::unique_lock (pre-C++17)
std::unique_lock lkA(mtxA, std::defer_lock);
std::unique_lock lkB(mtxB, std::defer_lock);
std::lock(lkA, lkB);  // atomic acquisition
```

---

### Livelock

Threads keep changing state in response to each other but make no progress. Technically not blocked — CPU spins.

```cpp
// Classic example: two threads both "politely" yield to each other
while (true) {
    if (try_lock()) break;
    yield();  // both yield → both retry → neither progresses
}
```

**Fix**: add randomized backoff or a priority mechanism.

---

### Starvation

A thread is perpetually denied access to a resource because other threads always get priority.

**Example**: A `shared_mutex` with continuous readers — a writer waits forever because new readers keep arriving before the writer gets the exclusive lock.

**Fix**: use fair scheduling (e.g. `std::shared_mutex` implementations typically have writer priority), or a queue-based lock.

---

## 5. Condition Variables

Used to block a thread until a **condition becomes true**, without busy-waiting.

### Basic Pattern

```cpp
#include <condition_variable>

std::mutex mtx;
std::condition_variable cv;
bool ready = false;

// Waiter thread
void consumer() {
    std::unique_lock lock(mtx);
    cv.wait(lock, []{ return ready; });  // atomically unlock + sleep until ready == true
    // lock is re-acquired here
    process();
}

// Notifier thread
void producer() {
    {
        std::lock_guard lock(mtx);
        ready = true;
    }
    cv.notify_one();  // wake one waiting thread
    // cv.notify_all() — wake all waiting threads
}
```

### Spurious Wakeups

`cv.wait()` can return even if nobody called `notify` — this is **spurious wakeup**, allowed by POSIX and the C++ standard. Always use the **predicate overload**:

```cpp
// WRONG — spurious wakeup causes bug
cv.wait(lock);
if (ready) process();  // may skip due to spurious wakeup

// CORRECT — predicate re-checked automatically after spurious wakeup
cv.wait(lock, []{ return ready; });
// equivalent to: while (!ready) cv.wait(lock);
```

### Timed Wait

```cpp
auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);

bool signaled = cv.wait_until(lock, deadline, []{ return ready; });
if (!signaled) {
    // timeout — ready still false
}

// Or with duration
bool ok = cv.wait_for(lock, std::chrono::milliseconds(100), []{ return ready; });
```

### Producer-Consumer Queue

```cpp
template<typename T>
class BlockingQueue {
public:
    void push(T val) {
        {
            std::lock_guard lock(mtx_);
            queue_.push(std::move(val));
        }
        cv_.notify_one();
    }

    T pop() {
        std::unique_lock lock(mtx_);
        cv_.wait(lock, [this]{ return !queue_.empty(); });
        T val = std::move(queue_.front());
        queue_.pop();
        return val;
    }

private:
    std::queue<T>           queue_;
    std::mutex              mtx_;
    std::condition_variable cv_;
};
```

---

## 6. Lock-Free Programming

### Why Lock-Free?

Locks have costs: context switching, kernel calls, priority inversion risk. Lock-free algorithms use atomics to guarantee progress without blocking.

**Progress guarantees**:


| Guarantee     | Meaning                                              |
| ------------- | ---------------------------------------------------- |
| **Blocking**  | A suspended thread blocks others                     |
| **Lock-free** | At least one thread makes progress — others may spin |
| **Wait-free** | Every thread makes progress in bounded steps         |


---

### Lock-Free Stack (Treiber Stack)

```cpp
template<typename T>
class LockFreeStack {
    struct Node {
        T data;
        Node* next;
        Node(T d) : data(std::move(d)), next(nullptr) {}
    };

    std::atomic<Node*> head_{nullptr};

public:
    void push(T val) {
        Node* new_node = new Node(std::move(val));
        new_node->next = head_.load(std::memory_order_relaxed);
        // CAS loop: set head = new_node only if head == new_node->next
        while (!head_.compare_exchange_weak(
                   new_node->next, new_node,
                   std::memory_order_release,
                   std::memory_order_relaxed)) {}
    }

    std::optional<T> pop() {
        Node* old_head = head_.load(std::memory_order_acquire);
        while (old_head) {
            if (head_.compare_exchange_weak(
                    old_head, old_head->next,
                    std::memory_order_acquire,
                    std::memory_order_relaxed)) {
                T val = std::move(old_head->data);
                delete old_head;  // NOTE: safe here but in general needs hazard pointers
                return val;
            }
        }
        return std::nullopt;  // empty
    }
};
```

**ABA Problem**: between reading `head` and CAS, another thread pops and pushes the same node back — the address looks unchanged but the state is different. Fix: tagged pointers or hazard pointers.

---

### `std::call_once` — Thread-Safe One-Time Initialization

```cpp
std::once_flag init_flag;

void initialize() { /* expensive setup */ }

void use_resource() {
    std::call_once(init_flag, initialize);  // initialize runs exactly once
    // ...
}
```

### Meyers Singleton (Thread-Safe in C++11)

```cpp
class Singleton {
public:
    static Singleton& instance() {
        static Singleton inst;  // C++11 guarantees this is thread-safe
        return inst;
    }
    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;
private:
    Singleton() = default;
};
```

---

## 7. Thread Pool Design

### When to Use a Thread Pool

**Use a thread pool when**:


| Scenario                      | Why pool helps                                                                                                 |
| ----------------------------- | -------------------------------------------------------------------------------------------------------------- |
| **Many short-lived tasks**    | Creating/destroying threads per task is expensive (~100µs each); pool amortizes that cost                      |
| **Controlled parallelism**    | Cap concurrency to `hardware_concurrency()` — spawning more threads than cores causes context-switch thrashing |
| **Task queuing**              | Naturally absorbs bursts; excess tasks wait in the queue instead of spawning unbounded threads                 |
| **Server / request handling** | Each incoming request is a task — pool size tuned to expected load                                             |
| **Parallel algorithms**       | Divide work into chunks, submit all, collect futures                                                           |
| **Async I/O callbacks**       | Dispatch completion handlers onto pool threads                                                                 |


**Do NOT use a thread pool when**:


| Scenario                                 | Better alternative                                                                 |
| ---------------------------------------- | ---------------------------------------------------------------------------------- |
| **Long-running dedicated threads**       | Explicit `std::thread` or `std::jthread` — pool threads should not be monopolized  |
| **Real-time / latency-sensitive work**   | Dedicated pinned thread with elevated priority — pool scheduling adds jitter       |
| **Single one-off task**                  | `std::async(launch::async, ...)` is simpler                                        |
| **Tasks that block on I/O indefinitely** | Blocks a pool thread; use async I/O (epoll/IOCP) + pool for callbacks instead      |
| **Tasks needing thread-local affinity**  | Pool threads are recycled — thread-local state may carry over from a previous task |


### Sizing the Thread Pool

```cpp
// CPU-bound tasks — match hardware concurrency (no benefit beyond this)
size_t num_threads = std::thread::hardware_concurrency();

// I/O-bound tasks — can exceed hardware_concurrency because threads block on I/O
// A common heuristic:
// num_threads = hardware_concurrency * (1 + wait_time / compute_time)
// e.g. if tasks spend 90% waiting: 8 cores × (1 + 9) = 80 threads
```

**Rule of thumb**:

- **CPU-bound**: `N` threads where `N = hardware_concurrency()`
- **I/O-bound**: `N * k` threads, where `k` is the blocking ratio (profile to find it)
- **Mixed**: use separate pools for CPU and I/O work

### Common Pitfalls

```cpp
// 1. Submitting a task that blocks waiting for another task in the same pool
//    → deadlock if all threads are blocked waiting
pool.submit([&pool]() {
    auto inner = pool.submit([]{ return 42; });
    return inner.get();  // DEADLOCK if pool is full — all threads waiting
});

// Fix: use a separate pool for nested tasks, or use a work-stealing pool

// 2. Capturing references to stack variables in submitted tasks
int local = 10;
pool.submit([&local]{ return local * 2; });
// local may be gone by the time the task runs → dangling reference
// Fix: capture by value

// 3. Ignoring the returned future — exceptions are silently swallowed
pool.submit([]{ throw std::runtime_error("oops"); });
// exception stored in future, never retrieved → silently lost
// Fix: always store and .get() the future, or add a catch inside the task
```

### When to Choose Thread Pool vs Other Primitives

```
Task is one-off and simple?
└── Yes → std::async(launch::async)

Many tasks, need to control concurrency?
└── Yes → Thread Pool

Task runs forever (event loop, daemon)?
└── Yes → Dedicated std::jthread

Tasks are async I/O with callbacks?
└── Yes → Async I/O (epoll/IOCP) + Thread Pool for callbacks

Need structured parallelism with automatic work division?
└── Yes → std::execution::par (C++17 parallel algorithms) or TBB
```

---

### Implementation

A thread pool reuses a fixed number of threads to execute many tasks — avoids the overhead of creating/destroying threads per task.

```cpp
#include <thread>
#include <queue>
#include <functional>
#include <future>
#include <vector>
#include <mutex>
#include <condition_variable>

class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads) {
        for (size_t i = 0; i < num_threads; ++i) {
            workers_.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock lock(mtx_);
                        cv_.wait(lock, [this]{
                            return !tasks_.empty() || stop_;
                        });
                        if (stop_ && tasks_.empty()) return;
                        task = std::move(tasks_.front());
                        tasks_.pop();
                    }
                    task();  // execute outside the lock
                }
            });
        }
    }

    // Submit a task, returns a future to get the result
    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>>
    {
        using R = std::invoke_result_t<F, Args...>;
        auto task = std::make_shared<std::packaged_task<R()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        std::future<R> fut = task->get_future();
        {
            std::lock_guard lock(mtx_);
            if (stop_) throw std::runtime_error("pool is stopped");
            tasks_.emplace([task]{ (*task)(); });
        }
        cv_.notify_one();
        return fut;
    }

    ~ThreadPool() {
        {
            std::lock_guard lock(mtx_);
            stop_ = true;
        }
        cv_.notify_all();
        for (auto& t : workers_) t.join();
    }

private:
    std::vector<std::thread>          workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex                        mtx_;
    std::condition_variable           cv_;
    bool                              stop_ = false;
};

// Usage
ThreadPool pool(4);
auto fut = pool.submit([](int x) { return x * x; }, 7);
std::cout << fut.get();  // 49
```

---

## 8. Producer-Consumer Pattern

The producer-consumer pattern decouples data **producers** (threads that generate work) from **consumers** (threads that process it) via a shared buffer/queue. Whether the waiting is blocking or non-blocking determines whether it's sync or async.

```
Producer Thread(s) → [Bounded Queue] → Consumer Thread(s)
```

**The buffer serves two purposes**:

1. **Decoupling**: producer and consumer run at their own pace
2. **Backpressure**: a bounded queue slows the producer when the consumer is overwhelmed

---

### Sync (Blocking) Implementation

Both sides **block** on a `condition_variable` when the queue is full (producer) or empty (consumer). The thread sleeps — no busy waiting.

```cpp
#include <queue>
#include <mutex>
#include <condition_variable>

template<typename T>
class BoundedQueue {
    std::queue<T>           q_;
    std::mutex              mtx_;
    std::condition_variable cv_not_empty_;
    std::condition_variable cv_not_full_;
    const size_t            max_size_;
    bool                    closed_ = false;

public:
    explicit BoundedQueue(size_t max) : max_size_(max) {}

    // Producer — BLOCKS if queue is full
    bool push(T item) {
        std::unique_lock lock(mtx_);
        cv_not_full_.wait(lock, [this]{
            return q_.size() < max_size_ || closed_;
        });
        if (closed_) return false;
        q_.push(std::move(item));
        cv_not_empty_.notify_one();
        return true;
    }

    // Consumer — BLOCKS if queue is empty
    bool pop(T& item) {
        std::unique_lock lock(mtx_);
        cv_not_empty_.wait(lock, [this]{
            return !q_.empty() || closed_;
        });
        if (q_.empty()) return false;   // closed and drained
        item = std::move(q_.front());
        q_.pop();
        cv_not_full_.notify_one();
        return true;
    }

    // Signal consumers to stop after draining
    void close() {
        std::lock_guard lock(mtx_);
        closed_ = true;
        cv_not_empty_.notify_all();
        cv_not_full_.notify_all();
    }
};
```

### Usage — Multiple Producers / Multiple Consumers

```cpp
BoundedQueue<int> q(100);   // buffer up to 100 items

// 2 producer threads
std::vector<std::thread> producers;
for (int p = 0; p < 2; ++p) {
    producers.emplace_back([&q, p] {
        for (int i = 0; i < 500; ++i)
            q.push(p * 1000 + i);   // blocks if queue full
        // don't close here — other producer still running
    });
}

// 4 consumer threads
std::vector<std::thread> consumers;
for (int c = 0; c < 4; ++c) {
    consumers.emplace_back([&q] {
        int item;
        while (q.pop(item))         // blocks if empty; returns false when closed+drained
            process(item);
    });
}

for (auto& t : producers) t.join();
q.close();                          // signal consumers: no more items coming
for (auto& t : consumers) t.join();
```

---

### Async (Non-Blocking) — Lock-Free Ring Buffer

Neither side blocks. `try_push`/`try_pop` return immediately — the caller decides what to do on full/empty. Best for **single-producer, single-consumer (SPSC)** high-throughput scenarios.

```cpp
template<typename T, size_t N>
class SPSCQueue {
    static_assert((N & (N - 1)) == 0, "N must be power of 2");
    std::array<T, N>     buf_;
    std::atomic<size_t>  head_{0};   // consumer reads from here
    std::atomic<size_t>  tail_{0};   // producer writes to here

public:
    // Returns false if full — NON-BLOCKING
    bool try_push(T item) {
        size_t tail = tail_.load(std::memory_order_relaxed);
        size_t next = (tail + 1) & (N - 1);
        if (next == head_.load(std::memory_order_acquire))
            return false;       // full
        buf_[tail] = std::move(item);
        tail_.store(next, std::memory_order_release);
        return true;
    }

    // Returns false if empty — NON-BLOCKING
    bool try_pop(T& item) {
        size_t head = head_.load(std::memory_order_relaxed);
        if (head == tail_.load(std::memory_order_acquire))
            return false;       // empty
        item = std::move(buf_[head]);
        head_.store((head + 1) & (N - 1), std::memory_order_release);
        return true;
    }
};

// Usage — producer yields CPU on full instead of blocking
SPSCQueue<int, 1024> q;

std::thread producer([&]{
    for (int i = 0; i < 100000; ++i) {
        while (!q.try_push(i))
            std::this_thread::yield();  // busy-wait with yield — not sleeping
    }
});

std::thread consumer([&]{
    int item;
    int count = 0;
    while (count < 100000) {
        if (q.try_pop(item)) {
            process(item);
            ++count;
        } else {
            std::this_thread::yield();
        }
    }
});
```

> **Memory ordering**: `release` on write + `acquire` on read ensures the consumer sees the data written before the tail was updated. No mutex needed for SPSC.

---

### Sync vs Async Comparison


|                             | **Sync (Blocking)**               | **Async (Lock-Free)**             |
| --------------------------- | --------------------------------- | --------------------------------- |
| **Thread while waiting**    | Sleeping                          | Free (yield) or spinning          |
| **Synchronization**         | `condition_variable` + mutex      | Atomic acquire/release            |
| **Backpressure**            | Automatic (producer blocks)       | Manual (`try_push` returns false) |
| **Latency**                 | ~10µs (OS wakeup overhead)        | ~100ns (no kernel call)           |
| **CPU usage**               | Low (thread sleeps)               | Higher if spin-looping            |
| **Complexity**              | Low                               | High                              |
| **Multi-producer/consumer** | Easy                              | Needs MPMC algorithm (harder)     |
| **Best for**                | General work queues, thread pools | Audio, market data, game loops    |


---

### Backpressure Strategies (When Queue Is Full)

```cpp
// 1. Block — producer waits (sync queue default)
q.push(item);               // sleeps until space available

// 2. Drop — producer discards (acceptable for real-time sensors, metrics)
if (!q.try_push(item))
    ++dropped_count_;

// 3. Drop oldest — overwrite the oldest item
if (!q.try_push(item)) {
    T discarded;
    q.try_pop(discarded);   // evict oldest
    q.try_push(item);       // make room
}

// 4. Slow producer — signal upstream to slow down
if (!q.try_push(item))
    throttle_upstream();

// 5. Grow the queue — increase buffer (only if memory allows)
```

---

### Use Cases


| Scenario                     | Implementation choice                                               |
| ---------------------------- | ------------------------------------------------------------------- |
| **Thread pool task queue**   | Sync bounded queue — workers block on empty                         |
| **Log aggregation**          | Sync queue — log threads block if aggregator is slow                |
| **Audio pipeline**           | Lock-free SPSC — must not block audio callback thread               |
| **Market data feed**         | Lock-free SPSC or MPSC — microsecond latency, drop on full          |
| **Video frame pipeline**     | Sync queue with small buffer (2-3 frames) — backpressure to decoder |
| **Web server request queue** | Sync bounded — shed load by blocking or returning 503               |
| **Sensor data aggregation**  | Lock-free + drop-on-full — sensors can't block                      |


---

## Mock Interview Questions

### Threads & Futures

---

**[Mid]** What happens if you destroy a `std::thread` object without calling `join()` or `detach()`?

> `std::terminate()` is called — the program aborts. A joinable thread (one that hasn't been joined or detached) cannot be destroyed safely. The C++ standard enforces this to avoid silent resource leaks or undefined behavior from a detached thread accessing destroyed objects.
>
> ```cpp
> {
>     std::thread t(worker);
>     // forgot join/detach → std::terminate on scope exit
> }
> ```
>
> Fix: always use `std::jthread` (C++20) which auto-joins, or wrap `std::thread` in a RAII guard:
>
> ```cpp
> struct JoinGuard {
>     std::thread t;
>     ~JoinGuard() { if (t.joinable()) t.join(); }
> };
> ```

---

**[Mid]** What is the difference between `std::async(launch::async, f)` and `std::thread`?

>
> |                       | `std::async(launch::async)`                   | `std::thread`                                 |
> | --------------------- | --------------------------------------------- | --------------------------------------------- |
> | Return value          | `std::future<T>` — retrieves result/exception | None — must use promise/shared state manually |
> | Exception propagation | Stored in future, rethrown on `.get()`        | `std::terminate` if uncaught in thread        |
> | Lifecycle             | Future destructor **blocks** until done       | Must explicitly `join()` or `detach()`        |
> | Thread reuse          | Implementation may use a pool                 | Always creates a new OS thread                |
>
>
> **Key trap**: the destructor of a `std::future` returned by `std::async(launch::async)` **blocks** until the task completes — so fire-and-forget with `std::async` doesn't work:
>
> ```cpp
> std::async(launch::async, long_running_task);
> // future destroyed immediately → blocks here until task finishes!
> // Must store the future: auto f = std::async(...)
> ```

---

**[Senior]** Explain `std::packaged_task` vs `std::promise`. When would you use each?

> Both provide a way to set a value into a `future`, but at different levels:
>
> `**std::promise<T>`**: manual write end of a future. You call `set_value()` or `set_exception()` yourself. Use when the result is produced by arbitrary logic — not necessarily a simple function call.
>
> `**std::packaged_task<F>**`: wraps a callable. Calling `operator()` on the task executes the function and automatically sets the result (or exception) into the associated future. Use when you want to wrap a function for deferred execution (e.g. in a task queue).
>
> ```cpp
> // packaged_task — wrap a function
> std::packaged_task<int(int)> task([](int x) { return x * 2; });
> std::future<int> fut = task.get_future();
> std::thread(std::move(task), 21).detach();
> std::cout << fut.get();  // 42
>
> // promise — set result from arbitrary logic
> std::promise<int> prom;
> std::future<int> fut2 = prom.get_future();
> std::thread([&prom](){
>     // complex logic...
>     prom.set_value(compute());
> }).detach();
> ```

---

### Mutexes & Locking

---

**[Mid]** What is the difference between `std::lock_guard` and `std::unique_lock`?

> `lock_guard` is a minimal RAII wrapper — it locks on construction and unlocks on destruction. It cannot be moved, unlocked early, or used with condition variables.
>
> `unique_lock` is a flexible RAII wrapper — it supports deferred locking, timed locking, manual unlock/re-lock, and is movable. Required for use with `std::condition_variable` (which needs to unlock the mutex during `wait()`).
>
> ```cpp
> // lock_guard — simple, zero overhead
> { std::lock_guard lock(mtx); modify_data(); }
>
> // unique_lock — needed for condition_variable
> std::unique_lock lock(mtx);
> cv.wait(lock, []{ return ready; });  // unlocks during wait
>
> // unique_lock — deferred then manually timed
> std::unique_lock lock(mtx, std::defer_lock);
> if (lock.try_lock_for(100ms)) { ... }
> ```
>
> **Rule**: use `lock_guard` by default; reach for `unique_lock` only when you need its extra capabilities.

---

**[Mid]** Why should you never call `notify_one()` while holding the mutex?

> You *can* hold the mutex when calling `notify`, and it's technically correct. However it's a performance anti-pattern: the notified thread wakes up, tries to acquire the mutex, and immediately blocks again because the notifier still holds it — a wasted context switch.
>
> Best practice: release the lock first, then notify:
>
> ```cpp
> // Suboptimal
> {
>     std::lock_guard lock(mtx);
>     ready = true;
>     cv.notify_one();  // waiter wakes, immediately blocks on mtx
> }
>
> // Better
> {
>     std::lock_guard lock(mtx);
>     ready = true;
> }
> cv.notify_one();  // waiter wakes and acquires mtx immediately
> ```
>
> Exception: some use cases *require* notifying under the lock to prevent a race between checking the condition and the notification (though the `wait(lock, pred)` pattern makes this unnecessary).

---

**[Senior]** Explain priority inversion. How does it occur with mutexes, and how is it mitigated?

> **Priority inversion** occurs when a high-priority thread is blocked waiting for a resource held by a low-priority thread, and a medium-priority thread keeps running — effectively the medium thread runs before the high one.
>
> ```
> Low-priority thread L holds mutex M
> High-priority thread H tries to acquire M → blocks
> Medium-priority thread X preempts L (X has higher priority than L)
> X runs indefinitely → H is starved — inversion!
> ```
>
> Real-world impact: the Mars Pathfinder mission had a famous priority inversion bug causing system resets.
>
> **Mitigations**:
>
> - **Priority inheritance**: when H blocks on M held by L, L temporarily inherits H's priority until it releases M. Supported in POSIX with `PTHREAD_PRIO_INHERIT`.
> - **Priority ceiling**: the mutex is assigned the highest priority of all threads that could lock it; any thread locking it runs at that ceiling.
> - **Lock-free design**: eliminate mutexes in real-time paths — no blocking, no inversion possible.
> - **Design**: keep critical sections short; avoid acquiring mutexes in high-priority threads where possible.

---

### `std::atomic` & Memory Model

---

**[Mid]** Compare `std::atomic`, `std::mutex`, and `std::semaphore`. When would you use each?

> `std::atomic` is for one variable or a small lock-free state transition. It gives atomic load/store/read-modify-write operations and optional memory ordering. Use it for counters, flags, reference counts, and SPSC queue indexes. It does not automatically protect a larger object.
>
> `std::mutex` protects a critical section. Use it when multiple fields or a container must be updated consistently, such as `std::map`, `std::vector`, cache state, or object invariants. It can block the thread and has risks such as deadlock and priority inversion, but it is simpler and correct for complex shared state.
>
> `std::counting_semaphore` controls permits. Use it to limit concurrency or count resources/events, such as "only 8 RPCs at once" or "N items available." It does not by itself protect shared data; a queue or pool usually still needs a mutex.
>
> Rule: atomic for a single value, mutex for shared invariants, semaphore for counting/permits.

---

**[Mid]** What is a data race, and why is it undefined behavior in C++?

> A **data race** occurs when two threads access the same memory location concurrently, at least one access is a write, and there is no synchronization between them.
>
> ```cpp
> int x = 0;
> std::thread t1([&]{ x = 1; });  // write
> std::thread t2([&]{ x = 2; });  // write — data race!
> ```
>
> C++ declares this **undefined behavior** — not just "wrong answer." The compiler and CPU are allowed to assume data races don't exist, enabling optimizations (register caching, reordering, dead-store elimination) that break completely under races. You may get wrong values, crashes, or seemingly unrelated code behaving incorrectly.
>
> **Fix**: use `std::atomic<int>`, or protect with a mutex, or ensure only one thread accesses `x` at a time.

---

**[Mid]** What is the difference between `memory_order_relaxed` and `memory_order_seq_cst`?

> - `relaxed`: only guarantees atomicity of the individual operation — no ordering constraints relative to other memory operations. Other reads/writes can be reordered around it by CPU or compiler. Cheapest.
> - `seq_cst`: imposes a **single total order** on all `seq_cst` atomic operations across all threads. Every thread agrees on the same global sequence. Most expensive — requires a full memory fence on ARM/Power architectures.
>
> ```cpp
> // relaxed — only safe when ordering doesn't matter
> std::atomic<int> counter{0};
> counter.fetch_add(1, std::memory_order_relaxed);  // just increment atomically
>
> // seq_cst — needed when other threads must see operations in a consistent order
> std::atomic<bool> flag_a{false}, flag_b{false};
> // Thread 1:
> flag_a.store(true);          // seq_cst
> bool b = flag_b.load();      // seq_cst
> // Thread 2:
> flag_b.store(true);          // seq_cst
> bool a = flag_a.load();      // seq_cst
> // Guaranteed: not both b==false and a==false
> ```

---

**[Senior]** Explain the acquire-release memory model with a concrete example. Why is it sufficient for a producer-consumer and why is `seq_cst` overkill there?

> Acquire-release establishes a **happens-before** relationship between two specific threads without imposing a global total order.
>
> - A **release-store** on atomic `A` guarantees: all writes before the store are visible to any thread that does an **acquire-load** of `A` and sees the stored value.
>
> ```cpp
> std::atomic<int*> ptr{nullptr};
> int data = 0;
>
> // Producer
> data = 42;                                         // (1) plain write
> ptr.store(&data, std::memory_order_release);       // (2) release — (1) visible after (2)
>
> // Consumer
> int* p;
> while (!(p = ptr.load(std::memory_order_acquire))) {} // (3) acquire
> assert(*p == 42);                                  // (4) guaranteed — (1) happened-before (4)
> ```
>
> **Why `seq_cst` is overkill here**: we only need the producer's writes to be visible to the consumer *after* the consumer sees the pointer. That's exactly what release/acquire does — a directed synchronization between two specific threads. `seq_cst` would additionally ensure all other threads see operations in the same global order, which we don't need and pay for unnecessarily.
>
> **Rule of thumb**: use `relaxed` for independent counters, `release`/`acquire` for producer-consumer / flag publication, `seq_cst` only when you need multi-thread consensus on operation ordering.

---

**[Mid]** `memory_order_acquire` và `memory_order_release` dùng trên **load** hay **store**?

> - **Release** chỉ hợp lệ trên **store** (và RMW success với acq_rel): “tôi đã publish xong mọi write trước đó”.  
> - **Acquire** chỉ hợp lệ trên **load** (và RMW): “tôi subscribe — mọi read sau này thấy data đã publish”.  
> - Cặp điển hình: producer `ready.store(true, release)` + consumer `while (!ready.load(acquire))`.  
> - Dùng nhầm (ví dụ `load(..., release)`) là lỗi compile hoặc semantics sai.

---

**[Mid]** Tại sao `++counter` trên `atomic` mặc định là `seq_cst` trong khi counter thống kê chỉ cần `relaxed`?

> `operator++` không nhận memory order — nó hard-code `seq_cst` để an toàn cho người mới. Với metric chỉ cần atomicity, dùng `fetch_add(1, memory_order_relaxed)` rõ ràng và có thể nhanh hơn trên ARM. Đừng dùng `relaxed` cho biến vừa là counter vừa là “ready flag” cho data khác.

---

**[Senior]** Giải thích lại SPSC `try_push`/`try_pop`: tại sao `tail_.load(relaxed)` nhưng `head_.load(acquire)`?

> Mỗi thread **chỉ ghi** index của mình (`tail` producer, `head` consumer) — load relaxed index “của mình” không cần sync với thread kia. Khi producer kiểm tra **head** (do consumer sửa), cần **acquire** để thấy mọi thứ consumer đã **release** khi advance head (đã đọc/xử lý slot). Sau khi ghi `buf_[tail]`, producer **release** store `tail` để consumer **acquire** load `tail` thấy item mới. Đây là directed sync hai chiều, không cần `seq_cst` global.

---

**[Senior]** `compare_exchange` có hai memory order — giải thích.

> ```cpp
> bool compare_exchange_weak(T& expected, T desired,
>                            std::memory_order success,
>                            std::memory_order failure);
> ```
> - **Success:** thường `acq_rel` hoặc `release`/`acquire` tùy algorithm — publish state mới, thấy state cũ đúng.  
> - **Failure:** thường `relaxed` — chỉ reload `expected`, chưa thay đổi shared state, không cần sync payload.  
> Ví dụ lock-free stack: success `acq_rel`, failure `relaxed`.

---

**[Senior]** x86 vs ARM — memory order có khác thực tế không?

> **x86** có **TSO** (Total Store Order): load-load, load-store, store-store không reorder với nhau (store-load có thể reorder). Nhiều code “tưởng” plain bool flag đủ vẫn chạy trên x86 dev machine — **sai trên ARM**.  
> **ARM/POWER** reorder mạnh → **phải** dùng atomic + đúng order; `seq_cst` và acquire/release phát huy fence thật.  
> **Bài học:** không benchmark ordering chỉ trên x86; CI hoặc test trên ARM / dùng TSan.

---

### Deadlock & Condition Variables

---

**[Mid]** You have a deadlock in production. Walk through how you would diagnose it.

> 1. **Get a thread dump**: on Linux, `kill -SIGABRT <pid>` or attach with `gdb` and run `thread apply all bt` — shows all thread stacks and what lock each is waiting on.
> 2. **Use thread sanitizer (TSan)**: recompile with `-fsanitize=thread`. TSan detects deadlocks and data races at runtime with detailed reports including stack traces.
> 3. **Look for the cycle**: in the stack trace, identify threads stuck in `pthread_mutex_lock` (or similar). Map out which mutex each holds and which each is waiting for. A cycle confirms deadlock.
> 4. **Common causes to check**:
>   - Lock ordering inconsistency (most common)
>   - Lock held across a callback that re-enters the same lock
>   - `recursive_mutex` needed but plain `mutex` used
>   - Lock held while calling external code that locks the same mutex
> 5. **Fix**: enforce consistent lock ordering, use `std::scoped_lock` for multi-lock acquisition, prefer lock hierarchies.

---

**[Mid]** Why must the condition variable predicate be checked in a loop (or use the predicate overload)?

> Two reasons:
>
> 1. **Spurious wakeups**: `cv.wait()` can return without `notify` being called — this is an explicit allowance in POSIX and the C++ standard (for implementor flexibility on certain platforms). Without a loop, spurious wakeups cause the consumer to proceed on a false condition.
> 2. **Lost wakeup / TOCTOU**: between `wait()` returning and the lock being re-acquired, another thread may have consumed the item. The condition may no longer be true.
>
> ```cpp
> // WRONG
> cv.wait(lock);
> process(queue.front());  // queue might be empty after spurious wakeup!
>
> // CORRECT — predicate re-evaluated every time wait returns
> cv.wait(lock, [&]{ return !queue.empty(); });
> // equivalent to: while (queue.empty()) cv.wait(lock);
> process(queue.front());  // always safe
> ```

---

**[Senior]** Implement a thread-safe bounded blocking queue (producer blocks when full, consumer blocks when empty).

> ```cpp
> template<typename T>
> class BoundedQueue {
> public:
>     explicit BoundedQueue(size_t capacity) : capacity_(capacity) {}
>
>     // Blocks if full
>     void push(T val) {
>         std::unique_lock lock(mtx_);
>         cv_not_full_.wait(lock, [this]{ return queue_.size() < capacity_; });
>         queue_.push(std::move(val));
>         cv_not_empty_.notify_one();
>     }
>
>     // Blocks if empty
>     T pop() {
>         std::unique_lock lock(mtx_);
>         cv_not_empty_.wait(lock, [this]{ return !queue_.empty(); });
>         T val = std::move(queue_.front());
>         queue_.pop();
>         cv_not_full_.notify_one();
>         return val;
>     }
>
>     // Non-blocking try variants
>     bool try_push(T val) {
>         std::lock_guard lock(mtx_);
>         if (queue_.size() >= capacity_) return false;
>         queue_.push(std::move(val));
>         cv_not_empty_.notify_one();
>         return true;
>     }
>
>     std::optional<T> try_pop() {
>         std::lock_guard lock(mtx_);
>         if (queue_.empty()) return std::nullopt;
>         T val = std::move(queue_.front());
>         queue_.pop();
>         cv_not_full_.notify_one();
>         return val;
>     }
>
>     size_t size() const {
>         std::lock_guard lock(mtx_);
>         return queue_.size();
>     }
>
> private:
>     std::queue<T>               queue_;
>     mutable std::mutex          mtx_;
>     std::condition_variable     cv_not_full_;
>     std::condition_variable     cv_not_empty_;
>     size_t                      capacity_;
> };
> ```
>
> Key design notes: two separate condition variables (`not_full` and `not_empty`) avoids waking up the wrong side; using two CVs is more efficient than one CV with `notify_all`. The `mutable mutex` allows `size()` to be `const`.

---

### Lock-Free & Advanced

---

**[Senior]** What is the ABA problem in lock-free programming? How is it prevented?

> **ABA**: Thread 1 reads value `A` from an atomic. Thread 2 changes it `A → B → A`. Thread 1 does a CAS expecting `A` — it succeeds, but the intermediate state `B` (a different object at the same address, or modified state) is missed. Thread 1 incorrectly assumes nothing changed.
>
> **Classic example** — lock-free stack:
>
> ```
> Stack: A → B → C
> Thread 1 reads head = A, plans to pop A, set head = B
> Thread 2 pops A and B, pushes A back: head = A → C (B is freed)
> Thread 1 CAS succeeds (head == A), sets head = B — dangling pointer!
> ```
>
> **Solutions**:
>
> 1. **Tagged pointer / version counter**: pack a version number with the pointer in a 128-bit atomic. CAS on the pair — even if address matches, the version number won't.
>
> ```cpp
> struct TaggedPtr { Node* ptr; uint64_t tag; };
> std::atomic<TaggedPtr> head;
> // CAS checks both ptr and tag — ABA prevented
> ```
>
> 1. **Hazard pointers**: before accessing a node, publish its address in a thread-local "hazard" slot. Reclamation checks all hazard slots — if a pointer is hazardous, defer free. Node is never reallocated while any thread has it hazard-registered.
> 2. **RCU (Read-Copy-Update)**: readers run lock-free, writers create a copy, update, then swap atomically. Old version freed after a "grace period" when all readers have passed through.
> 3. **Epoch-based reclamation**: track which "epoch" each thread is in; only reclaim memory from epochs all threads have advanced past.

---

**[Senior]** Design a thread-safe singleton that is lazy-initialized, safe under concurrent access, and avoids the overhead of locking on every call.

> The **Meyers Singleton** using a local static variable is the canonical C++11 answer — the standard guarantees static local initialization is thread-safe (§6.7):
>
> ```cpp
> class Config {
> public:
>     static Config& instance() {
>         static Config inst;  // initialized exactly once, thread-safe by C++11
>         return inst;
>     }
>     Config(const Config&) = delete;
>     Config& operator=(const Config&) = delete;
> private:
>     Config() { /* expensive init */ }
> };
> ```
>
> After the first call, `instance()` is essentially a single branch + return — no lock overhead.
>
> **If you need pointer semantics or explicit control over destruction order**, use `std::call_once`:
>
> ```cpp
> class Config {
> public:
>     static Config* instance() {
>         std::call_once(init_flag_, []{ inst_ = new Config(); });
>         return inst_;
>     }
> private:
>     Config() = default;
>     inline static Config*      inst_      = nullptr;
>     inline static std::once_flag init_flag_;
> };
> ```
>
> **What not to do** — the broken double-checked locking pattern (pre-C++11):
>
> ```cpp
> if (!inst_) {           // unsynchronized read — data race before C++11
>     lock();
>     if (!inst_) inst_ = new Config();
>     unlock();
> }
> ```
>
> In C++11 this can be fixed with `std::atomic<Config*>` and release/acquire semantics, but the Meyers singleton is simpler and preferred.

