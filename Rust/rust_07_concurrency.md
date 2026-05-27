# Rust 07 — Concurrency & Parallelism

---

## 7.1 Threads

```rust
use std::thread;
use std::time::Duration;

// Spawn a thread — returns JoinHandle<T>
let handle = thread::spawn(|| {
    println!("Hello from thread!");
    42  // return value
});

// Wait for thread to finish and get result
let result = handle.join().unwrap();  // Result<T, Box<dyn Any>>
println!("Thread returned: {}", result);

// Move ownership into thread — required for captured variables
let data = vec![1, 2, 3];
let handle = thread::spawn(move || {
    println!("{:?}", data);  // data moved into closure
});
handle.join().unwrap();
```

---

## 7.2 `Send` and `Sync` — The Thread-Safety Markers

Two auto-traits that the compiler uses to enforce thread safety:

| Trait | Meaning |
|-------|---------|
| `Send` | Safe to transfer ownership to another thread |
| `Sync` | Safe to share a reference (`&T`) across threads |

- `T: Sync` ↔ `&T: Send`
- Most types are `Send + Sync` automatically
- `Rc<T>` is `!Send + !Sync` (non-atomic ref count)
- `RefCell<T>` is `!Sync` (runtime borrows not thread-safe)
- `*mut T` and `*const T` are `!Send + !Sync`
- `Mutex<T>: Send + Sync` if `T: Send`

The compiler **prevents you at compile time** from accidentally sharing non-thread-safe types across threads — unlike C++ where data races are UB.

---

## 7.3 `Mutex<T>` — Mutual Exclusion

```rust
use std::sync::{Arc, Mutex};

let counter = Arc::new(Mutex::new(0u32));

let handles: Vec<_> = (0..10).map(|_| {
    let counter = Arc::clone(&counter);
    thread::spawn(move || {
        let mut lock = counter.lock().unwrap();  // blocks until acquired
        *lock += 1;
        // lock released when `lock` goes out of scope (RAII)
    })
}).collect();

for h in handles { h.join().unwrap(); }
println!("Final: {}", *counter.lock().unwrap());  // 10
```

`Mutex::lock()` returns `LockResult<MutexGuard<T>>` — `Err` if a thread panicked while holding the lock (poisoned mutex). Guard drops at end of scope, releasing the lock.

**Avoid deadlock**: always acquire multiple locks in a consistent order. Use `try_lock()` for non-blocking attempt.

---

## 7.4 `RwLock<T>` — Readers-Writer Lock

Allows multiple concurrent readers or one exclusive writer:

```rust
use std::sync::{Arc, RwLock};

let data = Arc::new(RwLock::new(vec![1, 2, 3]));

// Multiple readers at once
let r1 = data.read().unwrap();
let r2 = data.read().unwrap();
println!("{:?} {:?}", *r1, *r2);
drop(r1); drop(r2);

// One writer — blocks all readers
let mut w = data.write().unwrap();
w.push(4);
```

Use `RwLock` when reads vastly outnumber writes and reads are long enough to justify the overhead.

---

## 7.5 Channels — Message Passing

```rust
use std::sync::mpsc;  // multiple producer, single consumer

let (tx, rx) = mpsc::channel();

// Sender can be cloned for multiple producers
let tx2 = tx.clone();

thread::spawn(move || {
    tx.send(String::from("hello")).unwrap();
});
thread::spawn(move || {
    tx2.send(String::from("world")).unwrap();
});

// Receive (blocks until message available)
let msg1 = rx.recv().unwrap();
let msg2 = rx.recv().unwrap();

// Iterator — drains channel until all senders dropped
for msg in rx { println!("{}", msg); }
```

**Bounded channel** (`sync_channel`): backs up sender when buffer full:
```rust
let (tx, rx) = mpsc::sync_channel(10);  // buffer size 10
tx.send(42).unwrap();  // blocks if buffer full
```

---

## 7.6 `Condvar` — Condition Variables

For threads to wait on a condition:

```rust
use std::sync::{Arc, Mutex, Condvar};

let pair = Arc::new((Mutex::new(false), Condvar::new()));
let pair2 = Arc::clone(&pair);

// Thread waits until signaled
thread::spawn(move || {
    let (lock, cvar) = &*pair2;
    let mut ready = lock.lock().unwrap();
    while !*ready {
        ready = cvar.wait(ready).unwrap();  // atomically releases lock + sleeps
    }
    println!("Go!");
});

// Signal the waiting thread
{
    let (lock, cvar) = &*pair;
    let mut ready = lock.lock().unwrap();
    *ready = true;
    cvar.notify_one();
}
```

---

## 7.7 `Rayon` — Data Parallelism

`rayon` provides parallel iterators — drop-in replacement for standard iterators:

```rust
use rayon::prelude::*;

let v: Vec<i32> = (0..1_000_000).collect();

// Sequential
let sum: i32 = v.iter().map(|x| x * x).sum();

// Parallel — automatically splits work across CPU cores
let par_sum: i32 = v.par_iter().map(|x| x * x).sum();

// Parallel sort
let mut data = vec![5, 3, 1, 4, 2];
data.par_sort();
```

Rayon uses a work-stealing thread pool. The parallel version is identical code with `par_iter()` instead of `iter()`.

---

## 7.8 `Atomic` Types

Lock-free primitive operations:

```rust
use std::sync::atomic::{AtomicU32, Ordering};
use std::sync::Arc;

let counter = Arc::new(AtomicU32::new(0));

let handles: Vec<_> = (0..10).map(|_| {
    let counter = Arc::clone(&counter);
    thread::spawn(move || {
        counter.fetch_add(1, Ordering::SeqCst);  // atomic increment
    })
}).collect();

for h in handles { h.join().unwrap(); }
println!("{}", counter.load(Ordering::SeqCst));  // 10
```

**Memory orderings** (from weakest to strongest):
| Ordering | Meaning |
|----------|---------|
| `Relaxed` | No sync, only atomicity — for counters |
| `Acquire` | Subsequent loads see all writes before the Release |
| `Release` | Preceding writes visible to Acquire loads |
| `AcqRel` | Acquire + Release in one op |
| `SeqCst` | Full sequential consistency — safest, most expensive |

---

## Interview Questions

### Easy

**Q: What does `Send` mean and why can't `Rc<T>` be sent across threads?**

> `Send` means it is safe to transfer ownership of a value to another thread. If `T: Send`, you can `move` a `T` into `thread::spawn(move || ...)`. `Rc<T>` is `!Send` because its reference count is **not atomic** — two threads could increment/decrement the count simultaneously, corrupting it (data race). If an `Rc` were sent to another thread and both threads held a clone, the non-atomic count would be unsound. `Arc<T>` uses atomic operations for the count, making it safe to send (`Arc<T>: Send` if `T: Send`). The compiler enforces this: trying to use `Rc` in `thread::spawn` is a compile-time error.

**Q: What is a mutex guard and how does locking work in Rust?**

> `Mutex<T>` wraps its data so the data is inaccessible without acquiring the lock. Calling `mutex.lock()` returns a `MutexGuard<T>` — a smart pointer that gives access to the inner `T` and **releases the lock when dropped** (RAII). You cannot access the `T` without going through the lock. This is different from C/Java where you can access the data and forget to lock. If the guard is not explicitly dropped, it drops at the end of its scope. If you need to release early: use `drop(guard)` explicitly or put it in a block `{ let g = m.lock()...; }`. `lock()` blocks if the lock is held by another thread.

**Q: What is a channel and what is the difference between `channel` and `sync_channel`?**

> A channel is a communication mechanism where a sender (`Sender<T>`) sends values and a receiver (`Receiver<T>`) receives them. `mpsc::channel()` creates an **unbounded** channel — the sender never blocks (it always succeeds). Values queue up in memory until consumed. Risk: if the receiver is slow, memory grows unboundedly. `mpsc::sync_channel(n)` creates a **bounded** channel with buffer size `n`. If the buffer is full, `send()` blocks until the receiver consumes a value. This provides backpressure. Use `sync_channel` when you need to limit memory usage or throttle production. The `mpsc` in the name stands for "multiple producer, single consumer" — you can clone the sender for multiple threads, but there's only one receiver.

---

### Mid

**Q: What is a deadlock and how do you avoid it in Rust?**

> A deadlock occurs when two or more threads are each waiting for a lock that the other holds, and none can proceed. Example: Thread 1 holds lock A, waits for lock B. Thread 2 holds lock B, waits for lock A — both block forever. Rust's type system prevents data races but **cannot prevent deadlocks** — they are a logic error, not a type error. Prevention strategies: (1) **Lock ordering**: always acquire multiple locks in the same global order across all threads. (2) **Try-lock with timeout**: use `try_lock()` — if it fails, release all held locks and retry later. (3) **Minimize lock scope**: hold locks for the shortest possible time. (4) **Single global lock**: controversial but deadlock-free. (5) **Avoid nested locks**: restructure code so you never hold one lock while acquiring another. (6) **Use channels instead of shared state**: message passing eliminates the need for many locks.

**Q: Explain the difference between `Ordering::Acquire`, `Release`, and `SeqCst` in atomics.**

> These are **memory ordering** guarantees for atomic operations: `Release` (on a store): all previous writes in this thread are visible to any thread that subsequently performs an `Acquire` load on the same atomic. Used in a producer: write data, then `store(..., Release)`. `Acquire` (on a load): if this load sees the value stored by a `Release` store, then all writes preceding that `Release` store are also visible to this thread. Used in a consumer: `load(..., Acquire)`, then read data. Together, `Release`/`Acquire` forms a **happens-before** relationship. `SeqCst` (Sequential Consistency): the strongest guarantee — all `SeqCst` operations appear in a single total order across all threads. Every thread sees all `SeqCst` operations in the same order. Use when you need multiple atomics to be seen in a consistent order. `Relaxed`: only guarantees atomicity (no torn reads/writes), no ordering — used for metrics/counters where ordering doesn't matter.

**Q: What is work-stealing in the context of `rayon`?**

> Work-stealing is a scheduling algorithm where threads that finish their work can "steal" tasks from the queues of other threads. Rayon divides the work (e.g., a parallel iterator's elements) into chunks and distributes them across a thread pool. If thread A finishes its chunk before thread B, thread A steals half of thread B's remaining work. This achieves near-optimal load balancing without a central dispatcher. Practically: Rayon's `.par_iter()` automatically parallelizes across all available CPU cores with no tuning needed. The work-stealing overhead is small for items > ~1µs of work; for trivially small items (adding two ints), sequential is faster. Rayon is best for CPU-bound tasks; for I/O-bound parallelism, use async/await with tokio.

---

### Hard

**Q: Implement a thread-safe, lock-free counter using `AtomicU64` and explain why `Relaxed` ordering is sufficient.**

> ```rust
> use std::sync::atomic::{AtomicU64, Ordering};
> use std::sync::Arc;
>
> struct Counter(AtomicU64);
>
> impl Counter {
>     fn new() -> Self { Counter(AtomicU64::new(0)) }
>
>     fn increment(&self) { self.0.fetch_add(1, Ordering::Relaxed); }
>
>     fn get(&self) -> u64 { self.0.load(Ordering::Relaxed) }
> }
>
> // Relaxed is sufficient because:
> // We only care that each increment is atomic (no torn write).
> // We do NOT need any ordering guarantee relative to other memory:
> //   - Each thread's individual +1 is atomic
> //   - The final value after all join()s is correct regardless of order
> // If the counter were used as a flag to signal readiness of other data
> // (e.g., "when counter > 0, the data buffer is ready"), then we'd need
> // Release/Acquire to ensure the buffer writes are visible after the load.
> // Pure accumulation: Relaxed is correct and fastest.
>
> let c = Arc::new(Counter::new());
> let handles: Vec<_> = (0..100).map(|_| {
>     let c = Arc::clone(&c);
>     std::thread::spawn(move || c.increment())
> }).collect();
> for h in handles { h.join().unwrap(); }
> assert_eq!(c.get(), 100);
> ```

**Q: What is a mutex poison and how should you handle it?**

> A mutex becomes "poisoned" when a thread panics while holding the lock. Rust detects this because when the `MutexGuard` is dropped during an unwind, the mutex is marked as poisoned. Subsequent calls to `lock()` return `Err(PoisonError)` instead of `Ok(MutexGuard)`. This is a safety mechanism: the data behind the mutex may be in an inconsistent state (the panic may have left it mid-update). **Handling strategies**: (1) `lock().unwrap()` — panics if poisoned, propagating the failure. Appropriate when any poison means the program is in a bad state. (2) `lock().unwrap_or_else(|e| e.into_inner())` — recover the guard even from a poisoned mutex. Use when you know the data is still valid or when you can repair it. (3) `.is_poisoned()` check before locking. (4) Avoid panics while holding locks — replace `panic!` with `Result` returns in locked sections.

**Q: Design a producer-consumer pipeline with backpressure using `sync_channel`. How do you ensure no data is lost on shutdown?**

> ```rust
> use std::sync::mpsc::{self, SyncSender, Receiver};
>
> fn pipeline() {
>     let (tx, rx): (SyncSender<u32>, Receiver<u32>) = mpsc::sync_channel(64);
>
>     // Producer
>     let producer = std::thread::spawn(move || {
>         for i in 0..1000 {
>             if tx.send(i).is_err() {
>                 // Receiver dropped — shutdown signal
>                 println!("Producer: receiver gone, stopping");
>                 break;
>             }
>         }
>         // tx dropped here → channel closes → rx.recv() returns Err
>     });
>
>     // Consumer
>     let consumer = std::thread::spawn(move || {
>         let mut sum = 0u64;
>         for val in rx {   // iterates until tx dropped
>             sum += val as u64;
>         }
>         sum
>     });
>
>     producer.join().unwrap();
>     let total = consumer.join().unwrap();
>     println!("Sum: {}", total);  // 499500
>     // Shutdown guarantee: producer drops tx → channel closes → consumer
>     // drains remaining items → exits loop → no data lost.
>     // Backpressure: sync_channel(64) blocks producer when 64 items queued.
> }
> ```
