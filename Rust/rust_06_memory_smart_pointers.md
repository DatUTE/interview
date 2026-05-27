# Rust 06 — Memory Model & Smart Pointers

---

## 6.1 Stack vs Heap in Rust

**Stack**: fixed-size, compile-time known, LIFO. All local variables go here by default.
**Heap**: dynamic size, runtime allocation via `Box`, `Vec`, `String`, etc.

```
Stack (fast, automatic cleanup)    Heap (flexible, owned via Box/Vec/etc.)
┌─────────────────────────────┐    ┌──────────────────────────────┐
│ main: x: i32 = 5            │    │  [h][e][l][l][o]             │
│ main: s: String = {         │───►│  (owned by s via Box-like ptr)│
│   ptr, len=5, cap=5         │    └──────────────────────────────┘
│ }                           │
└─────────────────────────────┘
```

Types with known size at compile time → `Sized` — can live on the stack.
Types with unknown size (`str`, `[T]`, `dyn Trait`) → `!Sized` — must be behind a pointer.

---

## 6.2 `Box<T>` — Heap Allocation

`Box<T>` allocates `T` on the heap and owns it. When the `Box` is dropped, the heap memory is freed:

```rust
// Put large data on heap to avoid stack overflow
let large = Box::new([0u8; 1_000_000]);

// Recursive types — Box breaks the cycle
#[derive(Debug)]
enum List {
    Cons(i32, Box<List>),
    Nil,
}
let list = List::Cons(1, Box::new(List::Cons(2, Box::new(List::Nil))));

// Trait objects
let shape: Box<dyn Shape> = Box::new(Circle { radius: 5.0 });

// Deref coercion: Box<T> acts like &T
fn takes_str(s: &str) {}
let boxed: Box<String> = Box::new(String::from("hello"));
takes_str(&boxed);  // Box<String> → &String → &str (deref coercions)
```

---

## 6.3 `Rc<T>` — Reference-Counted Single-Thread Sharing

`Rc` (Reference Counted) allows **multiple owners** in single-threaded code:

```rust
use std::rc::Rc;

let a = Rc::new(String::from("hello"));
let b = Rc::clone(&a);   // increments reference count — no deep copy
let c = Rc::clone(&a);

println!("count = {}", Rc::strong_count(&a));  // 3
// Dropped in reverse order; count decrements; freed when count hits 0
```

- `Rc` is NOT thread-safe (`!Send`, `!Sync`)
- Use for graph-like structures, shared read-only data in single-threaded code
- Weak references: `Rc::downgrade(&rc)` → `Weak<T>`, breaks reference cycles

```rust
use std::rc::{Rc, Weak};

struct Node {
    value: i32,
    parent: Weak<Node>,       // Weak to avoid cycle
    children: Vec<Rc<Node>>,
}
```

---

## 6.4 `Arc<T>` — Atomic Reference Counting (Thread-Safe)

```rust
use std::sync::Arc;
use std::thread;

let data = Arc::new(vec![1, 2, 3]);

let handles: Vec<_> = (0..3).map(|_| {
    let data = Arc::clone(&data);  // clone the Arc (not the Vec)
    thread::spawn(move || {
        println!("{:?}", data);    // each thread has a shared ref
    })
}).collect();

for h in handles { h.join().unwrap(); }
```

`Arc` uses atomic operations for the reference count — safe across threads but has more overhead than `Rc`. Use `Rc` in single-threaded contexts, `Arc` when sharing across threads.

---

## 6.5 `RefCell<T>` — Interior Mutability

`RefCell` allows mutation through a shared reference at runtime (borrow rules checked at runtime, not compile time):

```rust
use std::cell::RefCell;

let val = RefCell::new(5);

{
    let mut r = val.borrow_mut();   // runtime check: panics if already borrowed
    *r += 1;
}  // mutable borrow released here

println!("{}", val.borrow());       // 6
```

- Panics if borrow rules are violated at runtime (vs compile-time with normal refs)
- Use with `Rc<RefCell<T>>` for shared mutable state in single-threaded code

```rust
use std::rc::Rc;
use std::cell::RefCell;

type SharedMutable<T> = Rc<RefCell<T>>;

let data: SharedMutable<Vec<i32>> = Rc::new(RefCell::new(vec![1, 2]));
let data2 = Rc::clone(&data);

data.borrow_mut().push(3);
println!("{:?}", data2.borrow());  // [1, 2, 3]
```

---

## 6.6 `Cell<T>` — Copy-Based Interior Mutability

`Cell<T>` is simpler than `RefCell` — it works by copying values in and out, so it only works for `Copy` types:

```rust
use std::cell::Cell;

struct Config {
    value: i32,
    access_count: Cell<u32>,  // mutable counter despite &self methods
}

impl Config {
    fn get_value(&self) -> i32 {
        self.access_count.set(self.access_count.get() + 1);
        self.value
    }
}
```

---

## 6.7 Smart Pointer Summary

| Type | Ownership | Thread-safe | Mutability | Use case |
|------|-----------|-------------|------------|----------|
| `Box<T>` | Single | Yes | `&mut` | Heap alloc, recursive types, trait objects |
| `Rc<T>` | Multiple | No (`!Send`) | Read-only | Single-threaded shared ownership |
| `Arc<T>` | Multiple | Yes | Read-only | Multi-threaded shared ownership |
| `RefCell<T>` | Single | No | Runtime borrow check | Interior mutability (single-thread) |
| `Mutex<T>` | Multiple | Yes | Lock guard | Multi-threaded mutable shared state |
| `RwLock<T>` | Multiple | Yes | Read/write lock | Many readers or one writer |
| `Cell<T>` | Single | No | Copy in/out | Cheap interior mutability for Copy types |

---

## 6.8 `Deref` and Deref Coercions

The `Deref` trait enables `*` operator and coercion chains:

```rust
// Deref coercion chain: Box<String> → String → str
fn print(s: &str) { println!("{}", s); }

let boxed = Box::new(String::from("hello"));
print(&boxed);   // &Box<String> → &String (Deref) → &str (Deref)
```

Coercions apply automatically in:
- Function/method argument passing
- `let` bindings with explicit type
- `return` statements

---

## 6.9 Memory Layout

```rust
// Size of types — std::mem::size_of
size_of::<i32>()          // 4
size_of::<Box<i32>>()     // 8 (pointer on 64-bit)
size_of::<Option<Box<i32>>>()  // 8 (null pointer optimization!)
size_of::<Rc<i32>>()      // 8 (pointer to heap: value + 2 counters)

// Alignment
align_of::<u64>()         // 8
align_of::<u8>()          // 1

// Struct layout with padding
struct Padded { a: u8, b: u32, c: u8 }
// Layout: a(1) + pad(3) + b(4) + c(1) + pad(3) = 12 bytes
// Reordered: b(4) + a(1) + c(1) + pad(2) = 8 bytes
```

---

## Interview Questions

### Easy

**Q: What is `Box<T>` and when do you need it?**

> `Box<T>` is a smart pointer that allocates `T` on the heap and owns it. You need it when: (1) The type's size is unknown at compile time — `Box<dyn Trait>` (trait objects). (2) The type is too large for the stack or you want it on the heap explicitly. (3) Building recursive types: a `List::Cons(i32, List)` would be infinitely sized; `List::Cons(i32, Box<List>)` has a fixed size because `Box` is just a pointer. (4) Transferring ownership without moving large data — you move the `Box` (8 bytes) not the data. When the `Box` is dropped (goes out of scope), `drop` is called on the inner value and the heap memory is freed. No manual `free` needed.

**Q: What is the difference between `Rc<T>` and `Arc<T>`?**

> Both provide shared ownership with reference counting. `Rc` (Reference Counted) uses **non-atomic** operations on the reference count — fast but not safe to share across threads (implements `!Send` + `!Sync`). `Arc` (Atomically Reference Counted) uses **atomic** operations — safe across threads (`Send + Sync`) but has higher cost per `clone`/`drop` due to memory fence instructions. Rule: use `Rc` in single-threaded code (e.g., building a graph in a single thread), use `Arc` when sharing data across multiple threads. If you have `Rc<T>` and decide you need threads later, change `Rc` → `Arc` — the API is identical.

**Q: What is interior mutability and why is it needed?**

> Interior mutability is a design pattern where you mutate data through a shared (`&`) reference — the mutation is "inside" the type, invisible to the borrow checker. It's needed when the borrow checker's static rules are too conservative for your use case. Example: a structure that counts how many times a method is called, even through `&self`. Without interior mutability, the counter field would require `&mut self`, which prevents having any other references. With `Cell<u32>` or `RefCell<u32>`, the counter can be mutated through `&self`. It trades compile-time borrow checking for runtime borrow checking (or for `Cell`, simply copying values in and out). The golden rule: interior mutability is a last resort after borrow restructuring fails.

---

### Mid

**Q: What is a reference cycle with `Rc` and how do you break it?**

> A reference cycle occurs when two `Rc` values point to each other. Since `Rc` drops its allocation only when the count reaches zero, and each `Rc` in the cycle holds a reference count of at least 1 from the other, neither is ever dropped — **memory leak**. Example: `Node { children: Vec<Rc<Node>>, parent: Rc<Node> }` — parent holds child, child holds parent → neither is freed. **Fix**: use `Weak<T>` for the back-reference: `parent: Weak<Node>`. `Weak` does not increment the strong count. When all `Rc` (strong) references are dropped, the allocation is freed even if `Weak` references remain. Access via `weak.upgrade()` returns `Option<Rc<T>>` — `None` if the data was already freed.

**Q: When would you use `Rc<RefCell<T>>` vs `Arc<Mutex<T>>`?**

> `Rc<RefCell<T>>`: single-threaded shared mutable state. `Rc` provides multiple owners (no thread safety), `RefCell` provides runtime-checked interior mutability. Use in single-threaded code (GUI widgets, graph algorithms). `Arc<Mutex<T>>`: multi-threaded shared mutable state. `Arc` provides thread-safe reference counting, `Mutex` provides exclusive access with blocking. Use when multiple threads must mutate shared data. The pattern is identical — `Rc` → `Arc`, `RefCell` → `Mutex` — but you'd never mix them (compiler enforces via `Send`/`Sync`). There's also `Arc<RwLock<T>>` when concurrent reads are common and writes are rare.

**Q: Explain null pointer optimization in Rust with `Option<Box<T>>`.**

> The Rust compiler guarantees that `Option<Box<T>>` has the same size as `Box<T>` (8 bytes on 64-bit). This is the **null pointer optimization** (NPO). It works because `Box<T>` is a non-null pointer — the `Box<T>` value can never be the null address `0x0`. So the compiler uses `null` as the bit pattern for `None`. When `Option<Box<T>>` is `None`, the pointer field is zero; when it's `Some(b)`, the pointer field is non-null. No extra discriminant byte needed. The same optimization applies to `Option<&T>`, `Option<NonNull<T>>`, `Option<fn()>`, and other types that guarantee non-null. This is why Rust's `Option` is zero-cost for pointer types.

---

### Hard

**Q: Implement a simple singly-linked list with `Box<T>` in Rust. What challenges arise?**

> ```rust
> #[derive(Debug)]
> enum List<T> {
>     Cons(T, Box<List<T>>),
>     Nil,
> }
>
> impl<T> List<T> {
>     fn new() -> Self { List::Nil }
>
>     fn prepend(self, val: T) -> Self {
>         List::Cons(val, Box::new(self))
>     }
>
>     fn len(&self) -> usize {
>         match self {
>             List::Nil         => 0,
>             List::Cons(_, rest) => 1 + rest.len(),
>         }
>     }
> }
> // Challenges:
> // 1. Recursive type requires Box — otherwise infinite size
> // 2. `drop` is recursive by default — deep lists cause stack overflow.
> //    Fix: implement Drop with an iterative loop
> // 3. Ownership makes "insert in middle" awkward — need to take ownership
> //    of the rest of the list
> // 4. No O(1) tail access — use VecDeque instead if performance matters
>
> impl<T> Drop for List<T> {
>     fn drop(&mut self) {
>         let mut cur = std::mem::replace(self, List::Nil);
>         while let List::Cons(_, mut next) = cur {
>             cur = std::mem::replace(&mut next, List::Nil);
>         }
>     }
> }
> ```

**Q: What does `std::mem::replace` and `std::mem::swap` do and when are they needed?**

> Both work around the borrow checker's restriction on moving out of references. `replace(dest: &mut T, src: T) -> T`: atomically replaces `*dest` with `src`, returning the old value. The location is always valid (never uninitialized). Use it when you need to take ownership of a value behind `&mut` — e.g., in a custom `Drop` impl or moving from a struct field. `swap(a: &mut T, b: &mut T)`: exchanges the values of `a` and `b`. Neither can be done by assignment alone because `*a = *b` would move out of `*b` (which is behind a mutable ref). Both are zero-cost — they're essentially `memcpy` under the hood. Example: the standard `linked list drop` pattern uses `replace(node, Nil)` to iteratively flatten the list without recursion.

**Q: What is the difference between `Cow<'a, T>` and a plain `String`/`&str`? When would you use it?**

> `Cow<'a, T>` ("Clone on Write") is an enum: either `Borrowed(&'a T)` or `Owned(T::Owned)`. For strings: `Cow<'a, str>` is either `&'a str` or `String`. Use it when: you sometimes need to return a borrowed string (no allocation), and sometimes need to return an owned one (after modification). Example: a function that appends a suffix only if a condition is met: ```rust fn ensure_newline(s: &str) -> Cow<str> { if s.ends_with('\n') { Cow::Borrowed(s) } else { Cow::Owned(format!("{}\n", s)) } }``` This avoids allocating when the string already ends with a newline. The tradeoff: API complexity for callers (they get `Cow<str>` not `&str` or `String`). Use `Cow` in performance-sensitive code where you know most paths won't need allocation.
