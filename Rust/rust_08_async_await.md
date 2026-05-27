# Rust 08 — Async / Await & Futures

---

## 8.1 What is Async?

Async/await enables **non-blocking I/O** — a task that is waiting for I/O can yield the thread to other tasks, instead of blocking.

```
Sync (blocking):          Async (non-blocking):
Thread 1: Read file──────►Thread 1: Start read → yield
          (blocked)                  Do other work ←────────┐
          ◄──read done              ◄──read done → resume──┘
```

Async is for **I/O-bound** work. For **CPU-bound** work, use threads or Rayon.

---

## 8.2 `async fn` and `.await`

```rust
// An async function returns impl Future<Output = T>
async fn fetch_data(url: &str) -> Result<String, reqwest::Error> {
    let body = reqwest::get(url).await?.text().await?;  // .await suspends here
    Ok(body)
}

// Calling an async function returns a Future — it does NOT run immediately
let future = fetch_data("https://example.com");  // no work done yet

// To run it, you need an executor (runtime)
#[tokio::main]
async fn main() {
    let result = fetch_data("https://example.com").await;
}
```

---

## 8.3 The `Future` Trait

```rust
pub trait Future {
    type Output;
    fn poll(self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Self::Output>;
}

pub enum Poll<T> {
    Ready(T),    // computation complete, here's the result
    Pending,     // not ready yet, call me again when woken
}
```

When `poll` returns `Pending`, it registers a **waker** (`cx.waker()`) so the executor knows to call `poll` again when the task can make progress.

`async fn` desugars to a state machine (enum) that the compiler generates. Each `.await` point becomes a state transition.

---

## 8.4 Tokio Runtime

`tokio` is the most popular async runtime:

```toml
# Cargo.toml
[dependencies]
tokio = { version = "1", features = ["full"] }
```

```rust
use tokio::time::{sleep, Duration};
use tokio::fs;

#[tokio::main]
async fn main() {
    // Spawn a task — runs concurrently
    let handle = tokio::spawn(async {
        sleep(Duration::from_millis(100)).await;
        println!("task done");
        42
    });

    // Do other work while task runs
    println!("main continues");

    // Wait for task
    let result = handle.await.unwrap();
    println!("got: {}", result);

    // Async file I/O
    let content = fs::read_to_string("file.txt").await.unwrap();
}
```

---

## 8.5 Concurrency with `join!` and `select!`

```rust
use tokio::{join, select, time::{sleep, Duration}};

// join! — run futures concurrently, wait for ALL
async fn concurrent() {
    let (r1, r2) = join!(
        async { sleep(Duration::from_millis(100)).await; 1 },
        async { sleep(Duration::from_millis(200)).await; 2 },
    );
    // Total time ~200ms (not 300ms)
}

// select! — wait for FIRST to complete, cancel others
async fn race() {
    select! {
        _ = sleep(Duration::from_secs(1)) => println!("timeout"),
        result = do_operation() => println!("got: {:?}", result),
    }
}
```

---

## 8.6 Channels in Async (tokio)

```rust
use tokio::sync::{mpsc, oneshot, broadcast, watch};

// mpsc — multi-producer single-consumer (async version)
let (tx, mut rx) = mpsc::channel::<String>(32);
tokio::spawn(async move {
    tx.send("hello".to_string()).await.unwrap();
});
while let Some(msg) = rx.recv().await {
    println!("{}", msg);
}

// oneshot — one value, one time (request/response)
let (tx, rx) = oneshot::channel::<u32>();
tokio::spawn(async move { tx.send(42).unwrap(); });
let val = rx.await.unwrap();

// broadcast — one value to many receivers
let (tx, mut rx1) = broadcast::channel::<String>(16);
let mut rx2 = tx.subscribe();

// watch — latest value (for configuration, state)
let (tx, rx) = watch::channel("initial");
tx.send("updated").unwrap();
println!("{}", *rx.borrow());
```

---

## 8.7 Async Mutex and Synchronization

```rust
use tokio::sync::{Mutex, RwLock, Semaphore};

// Async Mutex — .lock().await
let data = Arc::new(Mutex::new(vec![1, 2, 3]));
{
    let mut guard = data.lock().await;
    guard.push(4);
}  // lock released

// Semaphore — limit concurrent operations
let semaphore = Arc::new(Semaphore::new(5));  // max 5 concurrent
let permit = semaphore.acquire().await.unwrap();
// do work
drop(permit);  // release
```

---

## 8.8 `Pin<T>` and Why It's Needed

Async state machines can hold self-references (a future may point to data it also owns). Moving such a type would invalidate the self-reference. `Pin<P>` prevents moving the value behind the pointer after it's pinned:

```rust
use std::pin::Pin;
use std::future::Future;

// Most async code uses Pin transparently via Box::pin or tokio::pin!
async fn returns_future() -> impl Future<Output = i32> {
    async { 42 }
}

// When you need to store a dyn Future
let f: Pin<Box<dyn Future<Output = i32>>> = Box::pin(async { 42 });

// tokio::pin! for stack-pinning
tokio::pin! {
    let my_future = some_async_fn();
}
my_future.await;
```

---

## 8.9 Stream (Async Iterator)

```rust
use tokio_stream::{Stream, StreamExt};
use tokio::sync::mpsc;

// Creating a stream from channel
let (tx, rx) = mpsc::channel(10);
let stream = tokio_stream::wrappers::ReceiverStream::new(rx);

// Consuming
stream
    .filter(|x| std::future::ready(*x > 0))
    .map(|x| x * 2)
    .for_each(|x| async move { println!("{}", x); })
    .await;
```

---

## Interview Questions

### Easy

**Q: What is the difference between async/await and threads for concurrency?**

> **Threads**: OS-managed, each thread has its own stack (~8MB by default), pre-emptive scheduling, context switch is expensive (~1–10µs). Good for CPU-bound work and when you need true parallelism. **Async/await**: cooperative, thousands of tasks share a few OS threads, each task is tiny (only the state it actually needs between `.await` points). Switching between async tasks is cheap (a few instructions). Good for I/O-bound work where tasks spend most time waiting. In Rust, `tokio` can handle 100K+ concurrent connections on a few threads. The rule: CPU-bound → threads (or Rayon); I/O-bound → async. They can be combined: `tokio::task::spawn_blocking` runs blocking/CPU work on a thread pool from async code.

**Q: What does `.await` do to a Future?**

> `.await` suspends the current async function until the `Future` is ready. Under the hood, it repeatedly calls `poll()` on the future. If `poll()` returns `Pending`, the current task yields — the executor can run other tasks. When the waker registered by the future signals that the future can progress (e.g., data arrived from the network), the executor polls it again. Eventually `poll()` returns `Ready(value)`, and `.await` evaluates to that value. Critically, `.await` only suspends the current **task** — it does not block the OS thread. The thread is free to run other tasks. This is why async is efficient: one thread can drive thousands of futures.

**Q: What is `tokio::spawn` and what are the requirements for the spawned future?**

> `tokio::spawn(future)` creates a new async task that runs concurrently with the current task on the tokio runtime. It returns `JoinHandle<T>` that you can `.await` to get the result. Requirements: the future must be `Send + 'static`. `'static` is required because the task may run on a different thread and may outlive the current scope — it cannot borrow data from the caller's stack. `Send` is required for multi-threaded runtimes (tokio's default) because the task may be moved between threads. This means all captured variables must be owned (or use `Arc` for shared data). For single-threaded use, use `tokio::task::LocalSet` with `task::spawn_local`.

---

### Mid

**Q: What is the difference between `join!` and `select!`?**

> `join!(f1, f2, f3)` runs all futures **concurrently** and waits for **all** to complete. If any future is slow, you wait for it. Returns a tuple of all results. `select!(branch1 => ..., branch2 => ...)` waits for **the first** future to complete and **cancels the others**. Use `join!` when you need all results (fetch multiple URLs simultaneously). Use `select!` when you want the first response, or to implement a timeout (`select! { _ = sleep(1s) => Err(Timeout), r = op() => r }`). Cancellation via `select!` is cooperative: the dropped future is dropped, its destructor runs, but any in-flight I/O may or may not be cancelled depending on the runtime/driver.

**Q: What is a task vs a thread in tokio?**

> A **tokio task** (`tokio::spawn`) is a green thread — a lightweight unit of concurrent work managed by the tokio runtime. Many tasks share a small pool of OS threads (by default, `num_cpus` threads). Tasks are cooperative: they yield at `.await` points. Tasks have small overhead (a few hundred bytes for the state machine vs ~8MB for an OS thread stack). A tokio task is analogous to a goroutine in Go. Tasks are scheduled by the tokio executor using a work-stealing queue. An **OS thread** (`std::thread::spawn`) is managed by the OS with full pre-emptive scheduling. Threads are appropriate for blocking/CPU-bound work. For blocking calls inside async code, use `tokio::task::spawn_blocking` which runs the closure on a dedicated blocking thread pool without starving the async executor.

**Q: What problem does `Pin` solve in async Rust?**

> Async functions desugar to state machines (enums). These state machines may contain self-referential data: if a future yields at a `.await` point while holding a reference to a local variable, the state machine's field `ptr` points into its own field `data`. If the state machine is then **moved** (to a new memory location), `ptr` still points to the old address — a dangling pointer. `Pin<P>` is a wrapper that guarantees the value it contains will never be moved after it's pinned. This makes it safe to hold self-references. In practice, most async code pins futures implicitly via `Box::pin` or via the compiler's async machinery. You need to think about `Pin` explicitly when implementing `Future` manually or when storing futures in structs.

---

### Hard

**Q: Implement a timeout wrapper for any `Future` using `select!`.**

> ```rust
> use std::future::Future;
> use std::time::Duration;
> use tokio::time::sleep;
>
> #[derive(Debug)]
> struct TimeoutError;
>
> async fn with_timeout<F, T>(
>     future: F,
>     duration: Duration,
> ) -> Result<T, TimeoutError>
> where
>     F: Future<Output = T>,
> {
>     tokio::select! {
>         result = future    => Ok(result),
>         _ = sleep(duration) => Err(TimeoutError),
>     }
> }
>
> // Usage
> #[tokio::main]
> async fn main() {
>     let result = with_timeout(
>         async { tokio::time::sleep(Duration::from_millis(500)).await; 42 },
>         Duration::from_millis(200),
>     ).await;
>     // result = Err(TimeoutError) — 500ms > 200ms
>
>     // tokio provides this: tokio::time::timeout(duration, future)
>     // Our manual implementation illustrates the select! pattern.
> }
> ```

**Q: What is the difference between `async move` and `async` closures? How do you share state between concurrent tokio tasks?**

> `async move { ... }` is an async block that captures its environment by move — all captured variables are moved into the future. Required for `tokio::spawn` since tasks need `'static` (can't borrow from the caller's stack). To share state between tasks, use `Arc`: ```rust let shared = Arc::new(Mutex::new(0u32)); let shared2 = Arc::clone(&shared); tokio::spawn(async move { *shared2.lock().await += 1; }); *shared.lock().await += 1;``` Async closures (`async fn(x) -> T` or `|x| async move { ... }`) are regular closures returning futures — useful for passing async callbacks to combinators. The trait bounds for async callbacks are verbose: `F: Fn(T) -> Fut, Fut: Future<Output = U>`. The `async_fn_in_trait` feature (stabilized in Rust 1.75) makes async trait methods ergonomic.

**Q: Describe how you would implement rate limiting for 100 concurrent async tasks to a maximum of 10 active at a time.**

> ```rust
> use tokio::sync::Semaphore;
> use std::sync::Arc;
>
> async fn rate_limited_batch<F, Fut, T>(
>     items: Vec<F>,
>     concurrency: usize,
> ) -> Vec<T>
> where
>     F: FnOnce() -> Fut + Send + 'static,
>     Fut: Future<Output = T> + Send + 'static,
>     T: Send + 'static,
> {
>     let semaphore = Arc::new(Semaphore::new(concurrency));
>     let mut handles = Vec::new();
>
>     for item in items {
>         let sem = Arc::clone(&semaphore);
>         let handle = tokio::spawn(async move {
>             let _permit = sem.acquire().await.unwrap();
>             // Only `concurrency` tasks can run this section at once
>             item().await
>             // permit dropped → slot released for next task
>         });
>         handles.push(handle);
>     }
>
>     let mut results = Vec::new();
>     for h in handles {
>         results.push(h.await.unwrap());
>     }
>     results
> }
> // The Semaphore::new(10) creates 10 permits.
> // Each task acquires a permit before doing work and releases on drop.
> // All 100 tasks are spawned immediately but only 10 run concurrently.
> // In practice: use the `futures::stream::buffer_unordered(10)` combinator.
> ```
