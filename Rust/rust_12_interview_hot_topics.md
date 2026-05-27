# Rust 12 — Senior Interview Hot Topics

---

## Q1: Explain Ownership and the Borrow Checker

The borrow checker enforces three rules at compile time:
1. Each value has one owner; dropped when owner goes out of scope.
2. You can have any number of `&T` OR exactly one `&mut T` — never both.
3. References must always be valid (no dangling).

```
Move:      s1 → s2        (s1 invalid)
Borrow:    &s1 + &s1      (read-only, multiple ok)
Mut borrow: &mut s1       (exclusive, one at a time)
```

**Why it matters**: eliminates entire classes of bugs — use-after-free, double-free, data races, iterator invalidation — **at compile time, with zero runtime cost**.

---

## Q2: `String` vs `&str` — When to Use Each

```rust
// String — owned, heap-allocated, growable
let s: String = String::from("hello");
let s: String = "hello".to_string();
let s: String = format!("{} {}", "hello", "world");

// &str — borrowed, immutable view into string data
let s: &str = "hello";              // 'static — points into binary
let s: &str = &my_string[0..5];    // slice of a String
fn read_name(name: &str) { }       // prefer &str for read-only params

// Rule of thumb:
// - Return String when you need to give ownership
// - Take &str when you only need to read (most flexible)
// - Store String in structs (unless you can guarantee lifetime)
```

---

## Q3: Why Rust Doesn't Need a GC

```
C/C++:   Manual malloc/free → use-after-free, double-free
Java/Go: GC → safe, but stop-the-world pauses, memory overhead
Rust:    Ownership + Drop → memory freed exactly when owner drops, no GC needed

Performance: Rust ≈ C/C++
Safety:      Rust ≥ Java/Go
```

The key: `Drop` is called deterministically at scope exit. The compiler inserts the equivalent of `free()` calls at the exact right place. No GC pauses, no overhead.

---

## Q4: `Rc<RefCell<T>>` vs `Arc<Mutex<T>>`

```
                Single-threaded    Multi-threaded
Shared read:       Rc<T>              Arc<T>
Shared mutable: Rc<RefCell<T>>    Arc<Mutex<T>>

Rc<RefCell<T>>:
  + Fast (no atomic ops, no syscall)
  + Borrow checked at runtime (panics on violation)
  - NOT Send — cannot use across threads

Arc<Mutex<T>>:
  + Thread-safe (atomic ref count + OS mutex)
  + Deadlock-safe if you follow lock ordering
  - Blocking — waiting threads sleep
  - Consider Arc<RwLock<T>> for many readers
```

---

## Q5: How Async/Await Works Under the Hood

```rust
// This async function:
async fn example() -> u32 {
    let x = fetch().await;
    x + 1
}

// Desugars to roughly:
enum ExampleFuture { Start, WaitingForFetch(FetchFuture), Done }
impl Future for ExampleFuture {
    type Output = u32;
    fn poll(self: Pin<&mut Self>, cx: &mut Context) -> Poll<u32> {
        match self {
            Start => {
                *self = WaitingForFetch(fetch());
                self.poll(cx)  // re-poll immediately
            }
            WaitingForFetch(fut) => match fut.poll(cx) {
                Poll::Ready(x) => { *self = Done; Poll::Ready(x + 1) }
                Poll::Pending  => Poll::Pending  // yield to executor
            }
            Done => panic!("polled after completion"),
        }
    }
}
```

---

## Q6: Lifetime Elision — Three Rules

The compiler infers lifetimes via three rules before requiring explicit annotations:

1. Each reference parameter gets its own lifetime.
2. If exactly one input lifetime → assigned to all outputs.
3. If `&self`/`&mut self` → its lifetime assigned to all outputs.

```rust
fn first(s: &str) -> &str         // rule 2: input → output
fn len(s: &str, t: &str) -> &str  // fails rules → must annotate
fn split(&self, c: char) -> &str  // rule 3: &self → output
```

---

## Q7: When to Use `Box`, `Rc`, `Arc`

```
Box<T>:
  - Heap allocate a single value
  - Break recursive type sizes
  - Trait objects: Box<dyn Trait>
  - Single owner

Rc<T>:
  - Multiple owners in single-threaded code
  - Graph/tree with shared nodes
  - Weak<T> to break cycles

Arc<T>:
  - Multiple owners across threads
  - Shared read-only config
  - Arc<Mutex<T>> for shared mutable across threads
```

---

## Q8: Error Handling Decision Tree

```
Is this a bug (programmer error)?
  └─ Yes → panic! / assert! / unreachable!

Is this an expected failure?
  ├─ Library code → Result<T, thiserror::Error>
  └─ Application  → anyhow::Result<T>

Need multiple errors? → enum MyError with #[derive(thiserror::Error)]
Need one error type for all? → anyhow::Error

Calling into external code? → map_err or From impl to convert
```

---

## Q9: The `Send` + `Sync` + `'static` Trinity for Threads

```rust
// thread::spawn requires: F: FnOnce() + Send + 'static
// tokio::spawn requires:  F: Future + Send + 'static

// 'static = no borrowed references (must own or use Arc)
// Send = safe to move to another thread (not Rc, not *mut, etc.)

// Common pattern:
let shared = Arc::new(data);  // Arc: Clone + Send + Sync
let s2 = Arc::clone(&shared);
tokio::spawn(async move {     // move: own everything
    use_data(&s2).await;
});
```

---

## Q10: Trait Objects vs Generics — Decision Guide

```
Use Generics (impl Trait / T: Trait) when:
  ✓ Performance critical (monomorphized — no vtable)
  ✓ All types known at compile time
  ✓ Need to call multiple trait methods frequently
  ✓ The type is a return type that can be named

Use dyn Trait when:
  ✓ Need heterogeneous collection (Vec<Box<dyn Plugin>>)
  ✓ Types determined at runtime (plugins, config-driven)
  ✓ Reducing binary bloat (one version of the function)
  ✓ Trait is object-safe
  ✓ Simpler code more important than maximum performance
```

---

## Additional Practice Questions by Difficulty

### Easy

**Q: What is a closure and how does Rust determine whether it's `Fn`, `FnMut`, or `FnOnce`?**

> A closure is an anonymous function that captures variables from its surrounding scope. Rust automatically determines which of the three traits the closure implements based on how it uses its captures: **`FnOnce`**: the closure moves or drops a captured variable — it can only be called once. **`FnMut`**: the closure mutates a captured variable — can be called multiple times, requires `&mut self`. **`Fn`**: the closure only reads captured variables — can be called any number of times with `&self`. These form a hierarchy: every `Fn` is also `FnMut` and `FnOnce`. When you use `move`, the closure captures by value, but that alone doesn't determine the trait — it's determined by what the closure *does* with those values.

**Q: What happens when a struct that holds a `String` is dropped?**

> Rust drops values in reverse order of creation. For a struct, fields are dropped in declaration order. When the struct goes out of scope, Rust calls `Drop::drop` on the struct (if implemented), then recursively drops each field. For a `String` field: the `String`'s drop calls `Vec<u8>`'s drop, which calls the allocator's `dealloc` to free the heap buffer. This is all automatic — no `free()` call needed, no GC. The sequence is: struct's `Drop::drop` → field `Drop::drop` → allocator `dealloc`. If the struct implements `Copy`, it cannot implement `Drop` (copy semantics and destructors are incompatible — copied types are not "moved", so no single owner gets the drop).

**Q: What is the difference between `clone` and `copy` in Rust?**

> `Copy` is a marker trait indicating a type can be duplicated by simply copying its bits (memcpy). It's for types with no heap data: `i32`, `f64`, `bool`, `char`, `(A, B)` where both are `Copy`, `[T; N]` where `T: Copy`. Assignment and passing to functions implicitly copy without moving. `Clone` is for types that need a potentially expensive deep copy: `String`, `Vec<T>`, etc. `clone()` must be called explicitly. All `Copy` types also implement `Clone` (but not vice versa). A type cannot implement `Copy` if it implements `Drop` (since `Drop` implies resource ownership — copying would double-free). If a struct contains a `String`, it cannot be `Copy` because `String` is not `Copy`.

---

### Mid

**Q: Explain why Rust's iterator adapters are zero-cost abstractions.**

> Zero-cost abstraction means the high-level code compiles to the same machine code as the low-level equivalent. Iterator adaptors are structs that implement `Iterator`. Each `.map(f)` creates a `Map<I, F>` struct; each `.filter(p)` creates a `Filter<I, P>`. These are all generics — monomorphized for specific types. The compiler inlines the closures and eliminates the struct overhead. The entire chain `v.iter().filter(p).map(f).sum()` compiles to a single loop: `for ptr in range { let x = *ptr; if p(x) { acc += f(x); } }`. No intermediate `Vec`, no function call overhead, no runtime dispatch. Benchmarks consistently show iterator chains match or beat equivalent for loops. This is the "zero-cost abstraction" Bjarne Stroustrup originally defined: "you don't pay for what you don't use; what you do use, you couldn't hand-code better."

**Q: What is the difference between `std::sync::Mutex` and `tokio::sync::Mutex`?**

> **`std::sync::Mutex`**: blocking — if the lock is unavailable, the calling **thread** sleeps via an OS syscall. This is correct for sync/thread code but MUST NOT be held across `.await` points in async code. If you hold a `std::Mutex` guard and `.await`, you yield the async task but still hold the lock — other async tasks on the same thread cannot acquire the lock, causing a deadlock if the task that would release the lock is on the same thread. **`tokio::sync::Mutex`**: async-aware — `lock().await` suspends the async **task** (not the thread) if the lock is unavailable. Other tasks can run on the thread while waiting. Use `tokio::sync::Mutex` inside async code with `lock().await`. Use `std::sync::Mutex` for quick critical sections in async code where you don't hold the guard across await points (and prefer this when possible — it's faster).

**Q: Describe the memory layout of a `String` in Rust.**

> A `String` is a three-word struct on the stack: `(ptr: *mut u8, len: usize, cap: usize)`. On a 64-bit system: 24 bytes total. `ptr` points to a heap-allocated buffer of `cap` bytes, of which `len` bytes contain valid UTF-8 data. `len <= cap` always. When you `push_str` and `len == cap`, the `Vec<u8>` inside `String` reallocates: allocates a new buffer (usually 2× cap), copies the data, frees the old buffer. `&str` is a fat pointer: `(ptr: *const u8, len: usize)` — just the pointer and length, no capacity. String literals (`"hello"`) are `&'static str` — the pointer points into the binary's read-only data section.

---

### Hard

**Q: Implement a thread-safe event bus where multiple publishers can send events and multiple subscribers receive them.**

> ```rust
> use std::sync::{Arc, Mutex};
> use std::collections::HashMap;
>
> type EventHandler<T> = Box<dyn Fn(&T) + Send + Sync + 'static>;
>
> struct EventBus<T> {
>     handlers: Arc<Mutex<Vec<EventHandler<T>>>>,
> }
>
> impl<T: Send + Sync + 'static> EventBus<T> {
>     fn new() -> Self {
>         EventBus { handlers: Arc::new(Mutex::new(Vec::new())) }
>     }
>
>     fn subscribe<F: Fn(&T) + Send + Sync + 'static>(&self, f: F) {
>         self.handlers.lock().unwrap().push(Box::new(f));
>     }
>
>     fn publish(&self, event: &T) {
>         let handlers = self.handlers.lock().unwrap();
>         for h in handlers.iter() { h(event); }
>     }
>
>     fn clone_bus(&self) -> Self {
>         EventBus { handlers: Arc::clone(&self.handlers) }
>     }
> }
>
> // Usage
> let bus: EventBus<String> = EventBus::new();
> bus.subscribe(|e| println!("Handler 1: {}", e));
> bus.subscribe(|e| println!("Handler 2: {}", e));
>
> let bus2 = bus.clone_bus();
> std::thread::spawn(move || {
>     bus2.publish(&"event from thread".to_string());
> }).join().unwrap();
> // For async: replace Mutex with tokio::sync::Mutex and handlers
> // with async Box<dyn Fn(&T) -> Pin<Box<dyn Future<...>>> + Send>
> ```

**Q: When would you choose to write unsafe Rust, and what invariants must you document?**

> Write unsafe Rust when: (1) Implementing fundamental data structures that the borrow checker's rules are too conservative for (e.g., doubly-linked lists, concurrent queues). (2) FFI boundaries — calling C functions or exposing Rust to C. (3) Performance: `get_unchecked`, `ptr::copy_nonoverlapping` when bounds are provably correct and the hot path needs to skip the check. (4) Implementing `Send`/`Sync` for types with non-standard sharing semantics. Every `unsafe` block or `unsafe fn` must document its **safety invariants** in a `# Safety` comment: what the caller must guarantee, why the operation is actually safe. Example: ```rust /// # Safety /// - `ptr` must be valid and aligned for `T` /// - `ptr` must not be aliased by any other mutable reference /// - `ptr` must point to initialized memory of type `T` unsafe fn write_unchecked<T>(ptr: *mut T, val: T) { ... }``` Without documentation, an unsafe block is a ticking time bomb — the next reader has no idea what must remain true.

**Q: Design a compile-time-verified state machine for a TCP connection lifecycle.**

> ```rust
> use std::marker::PhantomData;
>
> // States as ZSTs (zero-sized types)
> struct Closed; struct Listening; struct Connected; struct Closing;
>
> struct TcpConnection<S> {
>     fd: i32,       // socket file descriptor
>     _s: PhantomData<S>,
> }
>
> impl TcpConnection<Closed> {
>     fn new() -> Self { TcpConnection { fd: 0, _s: PhantomData } }
>
>     fn listen(self, port: u16) -> Result<TcpConnection<Listening>, std::io::Error> {
>         // bind + listen syscalls
>         Ok(TcpConnection { fd: self.fd, _s: PhantomData })
>     }
> }
>
> impl TcpConnection<Listening> {
>     fn accept(self) -> Result<TcpConnection<Connected>, std::io::Error> {
>         // accept syscall
>         Ok(TcpConnection { fd: self.fd, _s: PhantomData })
>     }
> }
>
> impl TcpConnection<Connected> {
>     fn send(&self, data: &[u8]) -> Result<usize, std::io::Error> { todo!() }
>     fn recv(&self, buf: &mut [u8]) -> Result<usize, std::io::Error> { todo!() }
>     fn shutdown(self) -> TcpConnection<Closing> {
>         TcpConnection { fd: self.fd, _s: PhantomData }
>     }
> }
>
> impl TcpConnection<Closing> {
>     fn close(self) -> TcpConnection<Closed> {
>         // close syscall
>         TcpConnection { fd: 0, _s: PhantomData }
>     }
> }
>
> // These are COMPILE ERRORS — cannot call send on a non-Connected socket:
> // let conn = TcpConnection::<Closed>::new();
> // conn.send(b"hello");   // method not found on TcpConnection<Closed>
>
> // Valid usage:
> let conn = TcpConnection::<Closed>::new()
>     .listen(8080).unwrap()
>     .accept().unwrap();
> conn.send(b"hello").unwrap();
> conn.shutdown().close();
> ```

---

## Quick Reference: Rust Concepts That Trip Up Interviewers

| Concept | Key Point |
|---------|-----------|
| Ownership | One owner, freed on drop, move vs copy |
| Borrowing | Multiple `&T` OR one `&mut T`, never both |
| Lifetimes | References can't outlive what they point to |
| `String` vs `&str` | Owned vs borrowed, heap vs pointer |
| `Box/Rc/Arc` | Heap / shared(single-thread) / shared(multi-thread) |
| `RefCell/Mutex` | Runtime borrow check / thread-safe lock |
| `Send + Sync` | Compiler-enforced thread safety |
| `?` operator | Propagates Err via From conversion |
| Iterator | Lazy, zero-cost, implemented via `next()` |
| `async/await` | Future state machine, needs runtime |
| `unsafe` | 5 specific unlocks, not "skip the compiler" |
| Orphan rule | Trait OR type must be local to implement |
| RAII | Resources tied to lifetimes via Drop |
| `PhantomData` | Zero-cost variance and drop-check markers |
| Monomorphization | Generics → separate compiled copies |
