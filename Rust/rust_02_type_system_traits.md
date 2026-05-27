# Rust 02 — Type System & Traits

---

## 2.1 Structs

```rust
// Named-field struct
struct Point {
    x: f64,
    y: f64,
}

// Tuple struct
struct Color(u8, u8, u8);

// Unit struct (no fields — useful as marker type)
struct Marker;

let p = Point { x: 1.0, y: 2.0 };
let c = Color(255, 0, 0);

// Struct update syntax
let p2 = Point { x: 3.0, ..p };  // x=3.0, y=p.y
```

**Derived traits** — automatic implementations:
```rust
#[derive(Debug, Clone, PartialEq, Default)]
struct Config {
    timeout_ms: u32,
    retries: u8,
}
let c = Config::default();  // { timeout_ms: 0, retries: 0 }
println!("{:?}", c);
```

---

## 2.2 Implementing Methods

```rust
impl Point {
    // Associated function (no self) — constructor pattern
    fn new(x: f64, y: f64) -> Self {
        Point { x, y }
    }

    // Method — takes &self (immutable) or &mut self (mutable)
    fn distance(&self, other: &Point) -> f64 {
        ((self.x - other.x).powi(2) + (self.y - other.y).powi(2)).sqrt()
    }

    fn translate(&mut self, dx: f64, dy: f64) {
        self.x += dx;
        self.y += dy;
    }
}

let mut p = Point::new(0.0, 0.0);
p.translate(3.0, 4.0);
println!("distance from origin: {}", p.distance(&Point::new(0.0, 0.0)));
```

---

## 2.3 Traits — Defining Shared Behavior

A trait defines a set of methods a type must implement:

```rust
trait Shape {
    fn area(&self) -> f64;
    fn perimeter(&self) -> f64;

    // Default implementation — can be overridden
    fn describe(&self) -> String {
        format!("area={:.2}, perimeter={:.2}", self.area(), self.perimeter())
    }
}

struct Circle { radius: f64 }
struct Rectangle { width: f64, height: f64 }

impl Shape for Circle {
    fn area(&self) -> f64 { std::f64::consts::PI * self.radius * self.radius }
    fn perimeter(&self) -> f64 { 2.0 * std::f64::consts::PI * self.radius }
}

impl Shape for Rectangle {
    fn area(&self) -> f64 { self.width * self.height }
    fn perimeter(&self) -> f64 { 2.0 * (self.width + self.height) }
}
```

---

## 2.4 Trait Bounds — Generics

```rust
// Monomorphized at compile time — zero cost
fn largest<T: PartialOrd>(list: &[T]) -> &T {
    let mut largest = &list[0];
    for item in list {
        if item > largest { largest = item; }
    }
    largest
}

// Multiple bounds with +
fn print_both<T: Display + Debug>(val: T) {
    println!("{} {:?}", val, val);
}

// Where clause (cleaner for complex bounds)
fn compare<T, U>(t: &T, u: &U) -> bool
where
    T: Display + PartialOrd,
    U: Display,
{ ... }
```

---

## 2.5 Trait Objects — Dynamic Dispatch

When you need a collection of different types that share a trait:

```rust
// Static dispatch — monomorphized, zero overhead
fn print_area(shape: &impl Shape) { println!("{}", shape.area()); }

// Dynamic dispatch — single function, vtable lookup
fn print_area_dyn(shape: &dyn Shape) { println!("{}", shape.area()); }

// Owned trait object — heap-allocated, type-erased
let shapes: Vec<Box<dyn Shape>> = vec![
    Box::new(Circle { radius: 1.0 }),
    Box::new(Rectangle { width: 2.0, height: 3.0 }),
];

for s in &shapes { println!("{}", s.area()); }
```

**Object safety rules** — a trait is object-safe if:
- Methods don't use `Self` by value (except in `where Self: Sized` bounds)
- No generic type parameters on methods
- No associated functions without `self`

---

## 2.6 `impl Trait` Syntax

```rust
// In argument position (like a bound, monomorphized)
fn notify(item: &impl Summary) { println!("{}", item.summarize()); }

// In return position (opaque type — caller can't name the type)
fn make_adder(x: i32) -> impl Fn(i32) -> i32 {
    move |y| x + y   // returns a closure without naming its type
}

let add5 = make_adder(5);
println!("{}", add5(3));  // 8
```

---

## 2.7 Generics and Monomorphization

Rust generates separate machine code for each concrete type used with a generic. This is called **monomorphization**:

```rust
fn max<T: PartialOrd>(a: T, b: T) -> T { if a > b { a } else { b } }

// Compiler generates:
// fn max_i32(a: i32, b: i32) -> i32 { ... }
// fn max_f64(a: f64, b: f64) -> f64 { ... }
// etc.
```

Zero runtime overhead — identical to writing each by hand. Trade-off: larger binary size if many types used.

---

## 2.8 Common Standard Traits

| Trait | Purpose |
|-------|---------|
| `Clone` / `Copy` | Deep copy / implicit bitwise copy |
| `Debug` / `Display` | `{:?}` / `{}` formatting |
| `PartialEq` / `Eq` | `==` / equivalence relation |
| `PartialOrd` / `Ord` | `<, >` / total ordering |
| `Hash` | Required for `HashMap` keys |
| `Default` | `T::default()` constructor |
| `From` / `Into` | Infallible type conversion |
| `TryFrom` / `TryInto` | Fallible type conversion |
| `Iterator` | `.next()` — enables all iterator adapters |
| `Deref` / `DerefMut` | `*` operator, coercions |
| `Drop` | Custom destructor |
| `Send` / `Sync` | Thread safety markers |

---

## 2.9 `From` and `Into`

```rust
struct Celsius(f64);
struct Fahrenheit(f64);

impl From<Celsius> for Fahrenheit {
    fn from(c: Celsius) -> Self {
        Fahrenheit(c.0 * 9.0 / 5.0 + 32.0)
    }
}

let boiling = Celsius(100.0);
let f: Fahrenheit = boiling.into();   // Into::into() auto-impl'd from From
let f2 = Fahrenheit::from(Celsius(0.0));
```

---

## 2.10 Associated Types

```rust
trait Container {
    type Item;               // associated type — determined by each impl
    fn first(&self) -> Option<&Self::Item>;
}

struct Stack<T> { items: Vec<T> }

impl<T> Container for Stack<T> {
    type Item = T;
    fn first(&self) -> Option<&T> { self.items.first() }
}
```

**Associated types vs generics**: Use associated types when each implementor has exactly one natural "output type" (like `Iterator::Item`). Use generic parameters when a type can implement a trait multiple times with different types.

---

## Interview Questions

### Easy

**Q: What is the difference between `impl Trait` and `dyn Trait`?**

> `impl Trait` is **static dispatch** (monomorphization): the compiler generates a specialized version of the function for each concrete type at compile time. No runtime overhead, but each type produces separate code. The caller cannot mix different concrete types in a collection. `dyn Trait` is **dynamic dispatch**: the concrete type is erased, and method calls go through a **vtable** (a pointer to a table of function pointers). This has a small indirection overhead (~1 extra pointer dereference per call) but allows heterogeneous collections (`Vec<Box<dyn Trait>>`). Use `impl Trait` for maximum performance; use `dyn Trait` when you need runtime polymorphism or trait objects in collections.

**Q: What does `#[derive(Debug)]` do?**

> It instructs the compiler to automatically generate an implementation of the `Debug` trait for the type using a procedural macro. The generated implementation formats the struct or enum using `{:?}` — it shows field names and their values recursively. Without `#[derive(Debug)]`, you'd have to implement `fn fmt(&self, f: &mut Formatter) -> fmt::Result` manually. `derive` only works when all fields also implement `Debug`. Common derived traits: `Debug`, `Clone`, `PartialEq`, `Eq`, `Hash`, `Default`, `PartialOrd`, `Ord`.

**Q: What is the `From` trait and why is it useful?**

> `From<T>` defines an infallible conversion from type `T` into `Self`. Implementing `From<T> for U` automatically provides `Into<U> for T` (via the blanket impl). This is useful because: (1) It makes type conversions ergonomic with `.into()`. (2) Error handling: implementing `From<IoError> for MyError` lets the `?` operator automatically convert `IoError` into `MyError`. (3) It documents the conversion explicitly without needing a custom constructor name. The conversion is guaranteed to succeed (for fallible conversions, use `TryFrom`/`TryInto`).

---

### Mid

**Q: Explain the difference between `Iterator::map` (which is lazy) and `Iterator::collect` (which is eager). Why does this matter?**

> `map` returns an **iterator adaptor** — it wraps the original iterator in a struct that stores the closure and applies it lazily one element at a time when consumed. No allocation occurs, no work is done until an element is requested. `collect` is a **terminal operation** that drives the iterator to completion, collecting all values into a concrete collection (`Vec`, `HashMap`, etc.). This matters for performance: chaining `.filter().map().take(5)` only processes 5 elements from the source, even if the source has a million. You pay only for what you consume. This is the "zero-cost abstraction" promise: the iterator chain compiles to the same loop as hand-written code.

**Q: When would you use a trait object (`dyn Trait`) over an enum for polymorphism?**

> Use `dyn Trait` when: (1) You don't know all the concrete types at compile time (plugin architecture, user-provided types). (2) You need to store heterogeneous types in a collection and adding a new variant would require modifying a central enum. (3) The trait is object-safe. Use an **enum** when: (1) You control all variants (finite, known set). (2) Pattern matching is needed to handle each case differently. (3) Performance is critical (enums have no vtable overhead, no heap allocation for the object itself). Rule of thumb: enums for closed sets (known variants), `dyn Trait` for open sets (extensible, unknown implementations).

**Q: What is object safety and why can't `Clone` be made into a trait object?**

> A trait is **object-safe** if it can be used as `dyn Trait`. The rules: all methods must take `self` by reference (not by value), methods cannot have generic type parameters, and the trait cannot have associated functions without `self`. `Clone` is not object-safe because it has `fn clone(&self) -> Self` — the return type `Self` is the concrete type, which is erased in a trait object. The vtable would need to know the concrete size to return by value, which is impossible with type erasure. Workaround: define a `CloneDyn` trait with `fn clone_box(&self) -> Box<dyn CloneDyn>`, or use `dyn-clone` crate.

---

### Hard

**Q: Implement a generic `Stack<T>` with an associated type and a method that returns an iterator. Explain the lifetime annotations needed.**

> ```rust
> struct Stack<T> {
>     items: Vec<T>,
> }
>
> impl<T> Stack<T> {
>     fn new() -> Self { Stack { items: Vec::new() } }
>     fn push(&mut self, item: T) { self.items.push(item); }
>     fn pop(&mut self) -> Option<T> { self.items.pop() }
>     fn peek(&self) -> Option<&T> { self.items.last() }
>
>     // Returns an iterator — lifetime 'a ties the iterator to &self
>     fn iter(&self) -> impl Iterator<Item = &T> + '_ {
>         self.items.iter()
>     }
> }
> // The '_ lifetime in `impl Iterator<Item = &T> + '_` tells the compiler
> // the returned iterator borrows from &self. Without it, the compiler
> // doesn't know what lifetime the &T references inside the iterator
> // belong to, and rejects the code.
> ```
> The `'_` (anonymous lifetime) in the return position is required because the iterator yields `&T` references that borrow from `self`. The compiler needs to know the iterator cannot outlive the `Stack`. Without `'_`, the trait bound doesn't express this relationship.

**Q: What is the difference between `T: Trait` and `where T: Trait + 'static`? When must you add `'static`?**

> `T: Trait` means `T` implements `Trait` — it places no restriction on `T`'s lifetimes, so `T` could contain references with any lifetime (including short-lived ones). `T: 'static` means `T` does not contain any non-static references — either `T` is a fully owned type (no borrows), or it contains only `'static` references. You must add `'static` when: (1) Spawning threads: `std::thread::spawn` requires `T: 'static` because the thread may outlive the current scope, making any borrowed data potentially dangling. (2) Storing in a global/static context. (3) Using `Box<dyn Error + 'static>` for error handling across thread boundaries. (4) `tokio::spawn` requires `'static` futures. The bound is about the data's ability to live arbitrarily long, not about it actually being in static memory.

**Q: Implement the `Add` operator for a `Vector2D` struct to support `v1 + v2` syntax.**

> ```rust
> use std::ops::Add;
>
> #[derive(Debug, Clone, Copy, PartialEq)]
> struct Vector2D {
>     x: f64,
>     y: f64,
> }
>
> impl Add for Vector2D {
>     type Output = Vector2D;
>
>     fn add(self, other: Vector2D) -> Vector2D {
>         Vector2D { x: self.x + other.x, y: self.y + other.y }
>     }
> }
>
> // Also useful: Add<f64> for scalar addition
> impl Add<f64> for Vector2D {
>     type Output = Vector2D;
>     fn add(self, scalar: f64) -> Vector2D {
>         Vector2D { x: self.x + scalar, y: self.y + scalar }
>     }
> }
>
> let v1 = Vector2D { x: 1.0, y: 2.0 };
> let v2 = Vector2D { x: 3.0, y: 4.0 };
> let v3 = v1 + v2;  // Vector2D { x: 4.0, y: 6.0 }
> ```
> Note: `Add` takes `self` by value, so `Copy` is implemented so `v1` and `v2` remain usable. The `Output` associated type lets the result type differ from the operands (e.g., `Vector2D + f64 = Vector2D`).
