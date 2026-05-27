# Rust 01 — Ownership, Borrowing & Lifetimes

---

## 1.1 The Ownership Model

Every value in Rust has exactly one **owner**. When the owner goes out of scope, the value is **dropped** (destructor called, memory freed). No GC needed.

**Three Rules:**
1. Each value has exactly one owner.
2. There can only be one owner at a time.
3. When the owner goes out of scope, the value is dropped.

```rust
{
    let s = String::from("hello");  // s owns the heap allocation
    // use s
}   // s goes out of scope → drop(s) called → heap freed
```

---

## 1.2 Move Semantics

Assignment of heap types **moves** ownership — the original variable becomes invalid:

```rust
let s1 = String::from("hello");
let s2 = s1;          // ownership moved to s2
// println!("{}", s1); // ERROR: s1 is moved

// Copy types (stack-only, implement Copy trait): integers, bool, char, &T
let x = 5;
let y = x;            // copy, not move — both valid
println!("{} {}", x, y);
```

**Clone** for an explicit deep copy:
```rust
let s1 = String::from("hello");
let s2 = s1.clone();  // deep copy — both valid
```

---

## 1.3 Borrowing — Shared References

A **reference** lets you use a value without taking ownership:

```rust
fn length(s: &String) -> usize {  // borrows, does not own
    s.len()
}

let s = String::from("hello");
let len = length(&s);             // pass reference
println!("{} has length {}", s, len);  // s still valid
```

**Borrow rules (compile-time enforced):**
- Any number of `&T` (shared/immutable) references, OR
- Exactly one `&mut T` (exclusive/mutable) reference
- But **never both at the same time**

```rust
let mut s = String::from("hello");

let r1 = &s;      // ok
let r2 = &s;      // ok — multiple shared refs
// let r3 = &mut s; // ERROR: cannot borrow as mutable while shared refs exist
println!("{} {}", r1, r2);  // r1 and r2 last used here

let r3 = &mut s;  // ok now — r1, r2 no longer in use (NLL: Non-Lexical Lifetimes)
r3.push_str(" world");
```

---

## 1.4 Mutable References

```rust
fn append(s: &mut String) {
    s.push_str(" world");
}

let mut s = String::from("hello");
append(&mut s);
println!("{}", s);  // "hello world"
```

You cannot have two `&mut` references to the same value simultaneously — prevents data races at compile time:

```rust
let mut s = String::from("hello");
let r1 = &mut s;
// let r2 = &mut s;  // ERROR: cannot borrow `s` as mutable more than once
```

---

## 1.5 Dangling References — Prevented by the Compiler

```rust
fn dangle() -> &String {      // ERROR: returns reference to local
    let s = String::from("hi");
    &s                         // s dropped here → reference would dangle
}

fn no_dangle() -> String {    // CORRECT: return the owned value
    let s = String::from("hi");
    s
}
```

---

## 1.6 Slices

A **slice** is a reference to a contiguous sequence — fat pointer (ptr + len):

```rust
let s = String::from("hello world");
let hello = &s[0..5];   // &str — string slice
let world = &s[6..11];

let a = [1, 2, 3, 4, 5];
let slice: &[i32] = &a[1..3];  // [2, 3]
```

String literals are `&'static str` — a slice into the binary's read-only data segment.

---

## 1.7 Lifetimes

Lifetimes are the compiler's way of ensuring references don't outlive the data they point to. Most lifetimes are **inferred** (lifetime elision rules). Explicit lifetimes are needed when the compiler can't figure it out.

**Lifetime annotation syntax:** `'a` is a named lifetime parameter.

```rust
// Without annotation: compiler can't determine which input lifetime
// the output reference belongs to → error
fn longest<'a>(x: &'a str, y: &'a str) -> &'a str {
    if x.len() > y.len() { x } else { y }
}
// 'a = the shorter of the two input lifetimes
```

**Lifetime in structs** — when a struct holds a reference:
```rust
struct Important<'a> {
    part: &'a str,   // struct cannot outlive the data 'part' points to
}

impl<'a> Important<'a> {
    fn announce(&self) -> &str {
        self.part
    }
}
```

---

## 1.8 Lifetime Elision Rules

The compiler applies three rules before requiring explicit annotations:

1. Each reference parameter gets its own lifetime parameter.
2. If there is exactly one input lifetime, it is assigned to all output lifetimes.
3. If one of the parameters is `&self` or `&mut self`, its lifetime is assigned to all output lifetimes.

```rust
// These are equivalent after elision:
fn first(s: &str) -> &str { ... }
fn first<'a>(s: &'a str) -> &'a str { ... }
```

---

## 1.9 Static Lifetime

`'static` means the reference is valid for the entire program lifetime:

```rust
let s: &'static str = "I am static";  // string literal in binary

// Generic bound — T must not contain non-static references
fn needs_static<T: 'static>(val: T) { ... }
```

---

## 1.10 The `Drop` Trait

When a value goes out of scope, Rust calls its `Drop::drop` implementation:

```rust
struct Resource {
    name: String,
}

impl Drop for Resource {
    fn drop(&mut self) {
        println!("Dropping {}", self.name);
    }
}

{
    let r = Resource { name: String::from("file") };
}  // "Dropping file" printed here
```

Cannot call `drop()` manually — use `std::mem::drop(r)` (takes ownership) to force early drop.

---

## Interview Questions

### Easy

**Q: What is ownership in Rust and why does it matter?**

> Ownership is Rust's memory management model: each value has exactly one owner, and the value is freed when the owner goes out of scope. This guarantees memory safety (no use-after-free, no double-free) and enables the language to avoid a garbage collector entirely. At runtime, ownership compiles to the same code as manual `malloc`/`free` in C, but the compiler verifies correctness at compile time. It matters because it gives Rust the performance of C/C++ with the memory safety of managed languages.

**Q: What is the difference between `move` and `clone` in Rust?**

> `move` transfers ownership of a value from one variable to another. For heap-allocated types (like `String`, `Vec`), this is a shallow pointer copy — the heap data is not duplicated. After the move, the original variable is invalid (compiler error if used). `clone()` performs a deep copy — the heap data is duplicated so both variables own their own independent copy. Move is O(1); clone is O(n) in the size of the heap data. `Copy` types (integers, bool, etc.) are implicitly copied on assignment because they have no heap resources to manage.

**Q: How many mutable references can you have to a value at one time?**

> Exactly one. This is the exclusive borrowing rule: you can have any number of shared (`&T`) references OR exactly one mutable (`&mut T`) reference, but never both simultaneously. This rule prevents data races at compile time: if there were two mutable references, one could modify the value while the other is mid-read. The rule is enforced by the borrow checker at compile time — there is no runtime cost.

---

### Mid

**Q: What are Non-Lexical Lifetimes (NLL) and why were they introduced?**

> Before NLL (pre-Rust 2018 edition), a borrow's lifetime lasted until the end of the enclosing lexical scope (closing `}`), even if the reference was never used again after an earlier point. This made many safe programs fail to compile. NLL changed lifetime analysis to end a borrow at the last point the reference is actually **used**, not the end of the scope. Example: `let r1 = &s; println!("{}", r1);` — after the `println!`, the NLL borrow ends, so a subsequent `let r2 = &mut s;` is valid. NLL was a major ergonomics improvement that removed many false-positive borrow checker errors without changing any safety guarantees.

**Q: Explain the lifetime of a return reference in this function. When would it fail?**

```rust
fn first_word(s: &str) -> &str {
    let bytes = s.as_bytes();
    for (i, &item) in bytes.iter().enumerate() {
        if item == b' ' { return &s[0..i]; }
    }
    &s[..]
}
```

> By lifetime elision rule 2 (one input → one output lifetime), this is equivalent to `fn first_word<'a>(s: &'a str) -> &'a str`. The returned slice is tied to the input `s` — it cannot outlive `s`. This is correct: the returned reference points into the same string data. It would fail to compile if you tried to: (1) return a reference to a local `String` created inside the function (dangling reference), or (2) store the returned `&str` after dropping the owning `String`, e.g., `let word = first_word(&my_string); drop(my_string); println!("{}", word)` — the compiler would reject this.

**Q: What does `'a: 'b` mean in a lifetime bound?**

> `'a: 'b` means "lifetime `'a` outlives lifetime `'b`" — `'a` is at least as long as `'b`. It's a constraint that tells the compiler the reference with lifetime `'a` is guaranteed to live at least as long as `'b`. Example: `fn longer<'a: 'b, 'b>(x: &'a str, y: &'b str) -> &'b str` — the return has lifetime `'b`, but `x` must outlive `'b` (i.e., `'a: 'b`). This is used when you need to express relationships between multiple lifetimes, for instance when a longer-lived struct holds a reference that must not outlive the struct.

---

### Hard

**Q: Why does this code fail to compile, and how do you fix it?**

```rust
fn get_first(v: &Vec<String>) -> &str {
    &v[0]
}

let mut v = vec![String::from("hello")];
let first = get_first(&v);
v.push(String::from("world")); // ERROR
println!("{}", first);
```

> The `get_first` function returns a `&str` with the same lifetime as `&v` (by elision). `first` therefore borrows `v` for as long as `first` is alive (until the `println!`). `v.push(...)` requires a `&mut v`, but a shared borrow of `v` is still active. The compiler rejects this because: if `push` causes the `Vec` to reallocate its heap buffer, the old buffer (which `first` points into) is freed, making `first` a dangling reference. **Fix 1**: clone: `let first = v[0].clone()` — owned, not borrowed. **Fix 2**: restructure so `first` is no longer in scope before the `push`. **Fix 3**: use indices instead of references: `let first_idx = 0; /* later */ v[first_idx]`. The borrow checker is protecting you from a real use-after-free bug.

**Q: Implement a function that returns the longer of two string slices where one comes from a local variable inside the function. Why is this impossible without cloning?**

> ```rust
> // CANNOT compile — local variable dropped at end of function
> fn impossible<'a>(external: &'a str) -> &'a str {
>     let local = String::from("local data");
>     if external.len() > local.len() {
>         external
>     } else {
>         &local  // ERROR: `local` does not live long enough
>     }
> }
> ```
> The lifetime `'a` is determined by the caller — it could be `'static` or longer than the function body. But `local` is dropped at the end of the function. The compiler cannot guarantee the returned reference will be valid. There is no annotation trick that fixes this — the fundamental problem is that you're trying to return a reference to a value that no longer exists. **The only correct solutions**: (1) Return `String` (owned) instead of `&str`. (2) Pass the "local" data as a parameter so the caller controls its lifetime. (3) Clone and return the owned value. This illustrates why lifetime annotations can't be used to "extend" a lifetime — they only constrain relationships.

**Q: What is a `PhantomData<T>` and when do you need it?**

> `PhantomData<T>` is a zero-sized marker type that tells the compiler a struct logically "owns" or is "associated with" a `T`, even though it doesn't contain a `T` field. It's needed in two cases: (1) **Lifetime variance**: when a struct has a raw pointer `*const T` or `*mut T` (common in unsafe code), the compiler doesn't know the struct's variance over `T`'s lifetime. `PhantomData<T>` (covariant) or `PhantomData<*mut T>` (invariant) express the correct variance. (2) **Drop check**: if a struct's `Drop` implementation accesses data of type `T`, the compiler needs to know this to prevent use-after-free in the destructor. Without `PhantomData<T>`, it might allow the `T` to be dropped before the struct. Example: `std::vec::Vec<T>` uses `PhantomData` internally. Any time you use raw pointers in safe abstractions, you likely need `PhantomData`.
