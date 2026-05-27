# Rust 03 — Enums, Option, Result & Pattern Matching

---

## 3.1 Enums

Rust enums are **algebraic data types** — each variant can carry different data:

```rust
enum Shape {
    Circle(f64),               // tuple variant — one f64 (radius)
    Rectangle(f64, f64),       // tuple variant — width, height
    Triangle { base: f64, height: f64 },  // struct variant
    Dot,                       // unit variant — no data
}

let s = Shape::Circle(5.0);
let r = Shape::Rectangle(3.0, 4.0);
```

Enums can have methods just like structs:
```rust
impl Shape {
    fn area(&self) -> f64 {
        match self {
            Shape::Circle(r)                => std::f64::consts::PI * r * r,
            Shape::Rectangle(w, h)          => w * h,
            Shape::Triangle { base, height }=> 0.5 * base * height,
            Shape::Dot                      => 0.0,
        }
    }
}
```

---

## 3.2 `Option<T>` — Nullable Values Without Null

```rust
// Option<T> definition:
enum Option<T> {
    Some(T),
    None,
}

fn find_first_even(nums: &[i32]) -> Option<i32> {
    for &n in nums {
        if n % 2 == 0 { return Some(n); }
    }
    None
}

let result = find_first_even(&[1, 3, 4, 7]);

// Explicit match
match result {
    Some(n) => println!("Found: {}", n),
    None    => println!("Not found"),
}

// Convenience methods
let val = result.unwrap_or(0);         // 0 if None
let val = result.unwrap_or_default();  // T::default() if None
let val = result.unwrap_or_else(|| compute_default());
let doubled = result.map(|n| n * 2);  // Some(8) or None
let upper = result.and_then(|n| if n > 2 { Some(n) } else { None });

// Panics with message if None — use only when None is a bug
let val = result.expect("should have found an even number");
```

---

## 3.3 `Result<T, E>` — Recoverable Errors

```rust
// Result<T, E> definition:
enum Result<T, E> {
    Ok(T),
    Err(E),
}

use std::fs;
use std::io;

fn read_config() -> Result<String, io::Error> {
    let contents = fs::read_to_string("config.toml")?;  // ? propagates Err
    Ok(contents)
}

// Handling
match read_config() {
    Ok(contents) => println!("{}", contents),
    Err(e)       => eprintln!("Error: {}", e),
}

// Convenience
let contents = read_config().unwrap_or_default();
let contents = read_config().expect("config.toml must exist");
let len      = read_config().map(|s| s.len()).unwrap_or(0);

// Combining: convert Option to Result
let val: Option<i32> = Some(42);
let res: Result<i32, &str> = val.ok_or("value was None");
```

---

## 3.4 The `?` Operator

`?` on a `Result` either unwraps `Ok(v)` as `v` or immediately returns `Err(e)` from the function (after calling `From` to convert the error type):

```rust
fn parse_and_double(s: &str) -> Result<i32, std::num::ParseIntError> {
    let n: i32 = s.trim().parse()?;  // returns Err if parse fails
    Ok(n * 2)
}

// Equivalent to:
fn parse_and_double_verbose(s: &str) -> Result<i32, std::num::ParseIntError> {
    let n: i32 = match s.trim().parse() {
        Ok(n)  => n,
        Err(e) => return Err(e),
    };
    Ok(n * 2)
}
```

`?` also works on `Option<T>` inside a function that returns `Option`.

---

## 3.5 Pattern Matching

`match` is exhaustive — the compiler ensures all cases are covered:

```rust
let number = 7u32;

match number {
    1           => println!("one"),
    2 | 3       => println!("two or three"),
    4..=9       => println!("four through nine"),
    n if n % 2 == 0 => println!("{} is even", n),  // guard
    _           => println!("something else"),       // catch-all
}
```

**Destructuring**:
```rust
struct Point { x: i32, y: i32 }

let p = Point { x: 0, y: 7 };
match p {
    Point { x: 0, y } => println!("on y-axis at {}", y),
    Point { x, y: 0 } => println!("on x-axis at {}", x),
    Point { x, y }    => println!("({}, {})", x, y),
}

// Tuple destructuring
let (a, b, c) = (1, 2, 3);

// Enum destructuring with binding
if let Some(val) = find_first_even(&[1, 2, 3]) {
    println!("Found: {}", val);
}

// while let
let mut stack = vec![1, 2, 3];
while let Some(top) = stack.pop() {
    println!("{}", top);
}
```

---

## 3.6 `if let` and `let else`

```rust
// if let — match one variant, ignore others
if let Some(n) = result {
    println!("Got {}", n);
} else {
    println!("Nothing");
}

// let-else (Rust 1.65+) — early return on mismatch
fn process(val: Option<i32>) -> i32 {
    let Some(n) = val else {
        return 0;  // must diverge (return, panic, break, continue)
    };
    n * 2  // n is in scope here
}
```

---

## 3.7 Binding with `@`

```rust
match number {
    n @ 1..=12  => println!("Got a month number: {}", n),
    n @ 13..=19 => println!("Got a teen number: {}", n),
    _           => println!("Something else"),
}
```

---

## 3.8 Matching References

```rust
let reference = &4i32;

match reference {
    &val => println!("Got value: {}", val),  // dereference in pattern
}

// Or use ref keyword in non-reference pattern
match 4i32 {
    ref val => println!("Got ref: {}", val),  // val is &i32
}
```

---

## Interview Questions

### Easy

**Q: Why does Rust have `Option<T>` instead of null pointers?**

> Null pointers are a common source of crashes and bugs (Tony Hoare called them his "billion dollar mistake"). In Rust, `Option<T>` makes the possibility of absence **explicit in the type system**. If a function returns `Option<String>`, the caller is forced to handle both `Some` and `None` at compile time — they cannot accidentally use an absent value without checking. The compiler exhaustively enforces this. There is no implicit null — you cannot write code that silently passes a "null" and crashes at runtime. `Option<T>` is a zero-cost abstraction: `Option<NonNull<T>>` has the same size as a raw pointer because the compiler uses the null value as the discriminant for `None`.

**Q: What is the difference between `unwrap()` and `expect()`?**

> Both panic if called on `None` (for `Option`) or `Err` (for `Result`). `unwrap()` panics with a generic message like `"called Option::unwrap() on None"`. `expect("message")` panics with a custom message you provide, making it much easier to diagnose which unwrap failed. Use `expect` over `unwrap` when you believe the value is always present but want a meaningful panic message for debugging. In production code, prefer propagating errors with `?` or handling them explicitly. `unwrap` is acceptable in tests and prototypes, but should be avoided in library code where the caller deserves the chance to handle the error.

**Q: What does the `?` operator do in a function that returns `Result`?**

> `?` is syntactic sugar for: if `Ok(v)`, bind `v` and continue; if `Err(e)`, call `From::from(e)` to convert the error to the function's return error type, then `return Err(converted_e)`. It threads errors through a chain of fallible operations without writing a `match` for each one. The implicit `From` conversion means different error types can be propagated as long as `From<OrigError> for ReturnError` is implemented. This is how libraries like `anyhow` and `thiserror` work — they implement broad `From` impls so diverse error types can all be bubbled up with `?`.

---

### Mid

**Q: When should you use `match` vs `if let` vs `let-else`?**

> **`match`**: when you need to handle multiple variants or arms, especially when all variants must be covered (exhaustive matching). Best when there are 3+ cases or you need guards. **`if let`**: when you care about one variant and want to run code only in that case. Clean replacement for `match expr { Variant(v) => ..., _ => {} }`. **`let-else`** (Rust 1.65+): when the happy path must succeed to continue — failure diverges. Ideal for function-level guard clauses: destructure a value at the start of a function, or return/panic. It keeps the happy-path code without extra indentation (avoids the `if let` "pyramid of doom"). Rule: `match` for exhaustive/multi-arm, `if let` for optional branching, `let-else` for early-exit guards.

**Q: What is the difference between `map`, `and_then`, and `unwrap_or` on `Option`?**

> - `map(f)`: if `Some(v)`, returns `Some(f(v))`; if `None`, returns `None`. Use to transform the inner value without touching `None`. The closure returns a plain `T`, not an `Option`. - `and_then(f)` (flatMap): if `Some(v)`, returns `f(v)` which is itself an `Option<U>`. If `None`, returns `None`. Use for chaining fallible operations — avoids nested `Some(Some(...))`. Example: `str.parse::<i32>().ok().and_then(|n| if n > 0 { Some(n) } else { None })`. - `unwrap_or(default)`: if `Some(v)`, returns `v`; if `None`, returns `default`. Collapses `Option<T>` into `T` by providing a fallback. Use at the boundary where you must produce a concrete value.

**Q: Explain how `Result` chaining works with `?` when multiple different error types are involved.**

> When `?` is used on a `Result<T, E1>` inside a function that returns `Result<T, E2>`, the compiler calls `E2::from(e1)` using the `From` trait. For this to work, `From<E1> for E2` must be implemented. Two strategies: (1) **Box<dyn Error>**: use `Box<dyn std::error::Error>` as the error type — it accepts any error via `Box::new(e)`. Simple but loses type information. (2) **Custom enum error type**: define `enum MyError { Io(io::Error), Parse(ParseIntError) }` and implement `From<io::Error> for MyError` and `From<ParseIntError> for MyError`. The `thiserror` crate generates these impls automatically with `#[from]`. Then all `?` calls auto-convert to `MyError`. This approach preserves error variants for pattern matching.

---

### Hard

**Q: Implement a `try_parse_csv_line` that parses `"name,age,score"` into a struct, using `?` for all error propagation.**

> ```rust
> use std::num::ParseIntError;
> use std::str::ParseBoolError; // hypothetical
>
> #[derive(Debug)]
> struct Record { name: String, age: u32, score: f64 }
>
> #[derive(Debug)]
> enum ParseError {
>     WrongFieldCount(usize),
>     InvalidAge(ParseIntError),
>     InvalidScore(std::num::ParseFloatError),
> }
>
> impl std::fmt::Display for ParseError { ... }
> impl std::error::Error for ParseError {}
>
> impl From<ParseIntError>            for ParseError { fn from(e: ParseIntError) -> Self { ParseError::InvalidAge(e) } }
> impl From<std::num::ParseFloatError> for ParseError { fn from(e: std::num::ParseFloatError) -> Self { ParseError::InvalidScore(e) } }
>
> fn try_parse_csv_line(line: &str) -> Result<Record, ParseError> {
>     let fields: Vec<&str> = line.split(',').collect();
>     if fields.len() != 3 {
>         return Err(ParseError::WrongFieldCount(fields.len()));
>     }
>     Ok(Record {
>         name:  fields[0].trim().to_string(),
>         age:   fields[1].trim().parse::<u32>()?,   // ? converts via From
>         score: fields[2].trim().parse::<f64>()?,
>     })
> }
> ```
> The key: each `?` on a `parse()` call returns a different error type (`ParseIntError`, `ParseFloatError`), but both have `From` impls into `ParseError`, so `?` automatically converts them.

**Q: When matching on a `Vec`, how do you use slice patterns? What is the `..` pattern?**

> ```rust
> fn describe(nums: &[i32]) -> &str {
>     match nums {
>         []          => "empty",
>         [x]         => "single element",
>         [x, y]      => "exactly two elements",
>         [first, .., last] => "multiple — first and last bound",
>         [first, rest @ ..] => "first plus rest slice",
>     }
> }
> // `..` skips any number of elements (the "rest" pattern)
> // `rest @ ..` binds the remaining slice to `rest` as &[i32]
> //
> // Real use: head/tail decomposition
> fn sum_recursive(nums: &[i32]) -> i32 {
>     match nums {
>         []            => 0,
>         [head, tail @ ..] => head + sum_recursive(tail),
>     }
> }
> ```
> Slice patterns are stable since Rust 1.26. The `@ ..` binding is especially useful when you want to access the rest of the slice without cloning.

**Q: Explain how `match` ergonomics work — when can you omit `ref` and `&` in patterns?**

> Match ergonomics (RFC 2005, stable since Rust 1.26): when matching on a reference (e.g., `match &value`), the compiler automatically adjusts patterns to avoid requiring explicit `ref`/`&`. If the value being matched is `&T`, a pattern `x` automatically binds as `&T` (or `ref x` implicitly), and a pattern `SomeVariant(x)` treats `x` as `&InnerType`. This means you can write: `match &some_option { Some(val) => ... }` and `val` is `&T`, not `T`. Without match ergonomics, you'd need `Some(ref val)` or dereference with `match *some_option`. The rule: the compiler tracks a "binding mode" (`move`, `ref`, or `ref mut`) and applies it transitively through the pattern. When you see IDE output `val: &String` in a pattern that looks like `Some(val)`, match ergonomics is in play.
