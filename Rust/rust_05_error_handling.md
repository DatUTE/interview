# Rust 05 — Error Handling

---

## 5.1 The Error Handling Philosophy

Rust distinguishes two categories of errors:
- **Recoverable errors** — use `Result<T, E>`: file not found, network timeout, parse failure
- **Unrecoverable errors** — use `panic!`: bug in the program (index out of bounds, assertion failed)

No exceptions. No null. Every failure is a value in the type system.

---

## 5.2 Defining Custom Error Types

```rust
use std::fmt;

#[derive(Debug)]
enum AppError {
    Io(std::io::Error),
    Parse(std::num::ParseIntError),
    NotFound(String),
    InvalidInput { field: String, reason: String },
}

// Required for std::error::Error
impl fmt::Display for AppError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            AppError::Io(e)          => write!(f, "IO error: {}", e),
            AppError::Parse(e)       => write!(f, "Parse error: {}", e),
            AppError::NotFound(key)  => write!(f, "Not found: {}", key),
            AppError::InvalidInput { field, reason } =>
                write!(f, "Invalid {}: {}", field, reason),
        }
    }
}

impl std::error::Error for AppError {
    // Optional: provide the underlying source error
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        match self {
            AppError::Io(e)    => Some(e),
            AppError::Parse(e) => Some(e),
            _                  => None,
        }
    }
}

// From impls enable ? operator conversion
impl From<std::io::Error> for AppError {
    fn from(e: std::io::Error) -> Self { AppError::Io(e) }
}
impl From<std::num::ParseIntError> for AppError {
    fn from(e: std::num::ParseIntError) -> Self { AppError::Parse(e) }
}
```

---

## 5.3 `thiserror` — Idiomatic Custom Errors

The `thiserror` crate generates `Display` and `From` impls via derive macros:

```rust
use thiserror::Error;

#[derive(Error, Debug)]
enum AppError {
    #[error("IO error: {0}")]
    Io(#[from] std::io::Error),           // #[from] generates From impl

    #[error("parse error: {0}")]
    Parse(#[from] std::num::ParseIntError),

    #[error("not found: {0}")]
    NotFound(String),

    #[error("invalid {field}: {reason}")]
    InvalidInput { field: String, reason: String },
}

// Usage — ? auto-converts via From impls
fn load_config(path: &str) -> Result<u32, AppError> {
    let content = std::fs::read_to_string(path)?;   // io::Error → AppError::Io
    let value: u32 = content.trim().parse()?;        // ParseIntError → AppError::Parse
    Ok(value)
}
```

---

## 5.4 `anyhow` — Ergonomic Error Handling in Applications

For **application** code where you want to bubble up errors without defining types:

```rust
use anyhow::{Context, Result, bail, ensure, anyhow};

fn load_user(id: u32) -> Result<String> {
    // anyhow::Result<T> = Result<T, anyhow::Error>
    let data = std::fs::read_to_string("users.json")
        .context("failed to read users file")?;    // adds context message

    if id == 0 {
        bail!("user id 0 is invalid");             // return Err immediately
    }

    ensure!(id < 1000, "id {} out of valid range", id);  // assert → Err

    Ok(data)
}

// Accessing the error chain
if let Err(e) = load_user(0) {
    println!("Error: {}", e);
    println!("Caused by: {:?}", e.chain().collect::<Vec<_>>());
}
```

**Rule of thumb**: use `thiserror` for libraries (typed errors callers can match on), `anyhow` for application binaries (convenience, context, no need to match variants).

---

## 5.5 Error Conversion Patterns

```rust
// 1. map_err — convert error type
let n: Result<i32, String> = "42".parse::<i32>()
    .map_err(|e| format!("parse failed: {}", e));

// 2. Box<dyn Error> — accept any error
fn flexible() -> Result<(), Box<dyn std::error::Error>> {
    let _f = std::fs::File::open("file.txt")?;
    let _n: i32 = "42".parse()?;
    Ok(())
}

// 3. Combining Option and Result
let opt: Option<i32> = Some(42);
let res: Result<i32, &str> = opt.ok_or("was None");
let res: Result<i32, String> = opt.ok_or_else(|| format!("context: {}", "missing"));

let res: Result<i32, &str> = Ok(42);
let opt: Option<i32> = res.ok();  // discard error
```

---

## 5.6 Panics and When to Use Them

```rust
// Explicit panic
panic!("unreachable state: {}", state);

// Contract violations — panic is appropriate
fn divide(a: i32, b: i32) -> i32 {
    assert!(b != 0, "divisor must not be zero");  // panics with message
    assert_eq!(a % b, 0, "a must be divisible by b");
    a / b
}

// unreachable! — for arms that should never execute
match direction {
    North | South | East | West => handle(direction),
    _ => unreachable!("all directions covered"),
}

// todo! / unimplemented! — stubs during development
fn not_yet() -> i32 { todo!("implement this later") }
```

**When to panic**: programmer errors (violated preconditions, corrupted state, logic bugs). When to `Result`: expected failure modes (I/O, network, user input, parsing).

---

## 5.7 Error Propagation in `main`

```rust
// main can return Result — errors printed and exit code 1
fn main() -> Result<(), Box<dyn std::error::Error>> {
    let content = std::fs::read_to_string("config.txt")?;
    println!("{}", content);
    Ok(())
}

// With anyhow
fn main() -> anyhow::Result<()> {
    let content = std::fs::read_to_string("config.txt")
        .context("config.txt not found")?;
    println!("{}", content);
    Ok(())
}
```

---

## Interview Questions

### Easy

**Q: What is the difference between `panic!` and returning `Err`?**

> `panic!` unwinds (or aborts) the program — the current thread terminates. In a multi-threaded program, a panic in one thread propagates to the main thread only when that thread is `join()`ed. `panic!` is for bugs — situations that should never happen (index out of bounds, broken invariants). Returning `Err(e)` is a normal control flow path — the caller receives the error value and decides how to handle it (propagate, convert, log, use a default). The fundamental distinction: `panic` when the program cannot meaningfully continue; `Result::Err` when failure is a normal expected outcome that callers should handle. Libraries should almost never `panic` (except for true precondition violations).

**Q: What is the `std::error::Error` trait and what does it require?**

> `std::error::Error` is the standard trait for error types. It requires: (1) `Display` — a human-readable error message. (2) `Debug` — debug representation (required by the `Error` supertrait bound). Optionally, you can override `source() -> Option<&dyn Error>` to expose the underlying cause error for error chain inspection. Any type implementing `Display + Debug` can implement `Error`. This trait enables: generic error handling (`Box<dyn Error>`), error chain traversal (`.source()` / `anyhow`'s `.chain()`), and interoperability between error types from different crates.

**Q: When should you use `anyhow` vs `thiserror`?**

> **`thiserror`** is for libraries and crates that expose a public API: define typed error enums with `#[derive(Error)]` so callers can pattern-match on specific variants and handle each case. **`anyhow`** is for application binaries: provides `anyhow::Error` which can wrap any error type, adds context with `.context("message")`, and collects a full error chain. Callers of application code don't match on error variants — they just display or log the error. The common pattern: library crate uses `thiserror` to define `pub enum LibError`; binary crate uses `anyhow` to compose multiple library errors with context into a user-facing error report.

---

### Mid

**Q: How does the `?` operator interact with `From` when you have multiple error types?**

> When `?` is applied to `Result<T, E1>` inside a function returning `Result<T, E2>`, the compiler inserts `E2::from(e1)` — calling `From<E1> for E2`. This means: (1) If `From<E1> for E2` is not implemented, the code won't compile. (2) Implementing `From` impls for all error types your function encounters is what makes `?` seamless. `thiserror`'s `#[from]` attribute generates these impls automatically. When using `anyhow::Error`, it implements `From<T: Error>` for all error types, so `?` always works — but you lose the ability to match on specific variants. The conversion is explicit enough to track (vs Java exceptions where the catch site is distant from the throw), but concise enough to not be boilerplate.

**Q: What does `unwrap_or_else` do and when is it preferable to `unwrap_or`?**

> `unwrap_or(default)` evaluates `default` eagerly — even if the value is `Some`/`Ok`. This is wasteful if `default` is expensive to compute. `unwrap_or_else(|| expensive_default())` takes a closure that is only called when the value is `None`/`Err`. Use `unwrap_or_else` when the fallback involves a function call, heap allocation, or anything with a cost. Example: `option.unwrap_or_else(Vec::new)` — `Vec::new` only called on `None`. Similarly, `unwrap_or_default()` is shorthand for `unwrap_or(T::default())` — also lazy (the struct's Default impl is called only when needed, though for trivial types it compiles away).

**Q: Explain how to build an error chain that preserves the original cause across multiple layers.**

> ```rust
> #[derive(Debug, thiserror::Error)]
> enum ServiceError {
>     #[error("database error: {0}")]
>     Database(#[from] DbError),
>     #[error("not found: {0}")]
>     NotFound(String),
> }
>
> #[derive(Debug, thiserror::Error)]
> enum DbError {
>     #[error("connection failed")]
>     Connection(#[from] std::io::Error),
> }
>
> // The chain: io::Error → DbError::Connection → ServiceError::Database
> // Access chain with std::error::Error::source():
> fn print_chain(err: &dyn std::error::Error) {
>     let mut cur: Option<&dyn std::error::Error> = Some(err);
>     while let Some(e) = cur {
>         println!("  caused by: {}", e);
>         cur = e.source();
>     }
> }
> // With anyhow: anyhow::Error stores the chain automatically.
> // anyhow::Error::chain() returns an iterator over the error causes.
> ```

---

### Hard

**Q: Implement a retry function that retries a fallible operation N times, collecting all errors.**

> ```rust
> fn retry<T, E, F>(mut f: F, attempts: usize) -> Result<T, Vec<E>>
> where
>     F: FnMut() -> Result<T, E>,
> {
>     let mut errors = Vec::new();
>     for _ in 0..attempts {
>         match f() {
>             Ok(v)  => return Ok(v),
>             Err(e) => errors.push(e),
>         }
>     }
>     Err(errors)
> }
>
> // Usage
> let result = retry(|| std::fs::read_to_string("config.txt"), 3);
> match result {
>     Ok(content) => println!("{}", content),
>     Err(errors) => {
>         println!("Failed after {} attempts:", errors.len());
>         for (i, e) in errors.iter().enumerate() {
>             println!("  attempt {}: {}", i+1, e);
>         }
>     }
> }
>
> // With exponential backoff (async version with tokio):
> async fn retry_async<T, E, F, Fut>(mut f: F, attempts: usize) -> Result<T, E>
> where
>     F: FnMut() -> Fut,
>     Fut: std::future::Future<Output = Result<T, E>>,
> {
>     let mut last_err = None;
>     for i in 0..attempts {
>         match f().await {
>             Ok(v)  => return Ok(v),
>             Err(e) => {
>                 last_err = Some(e);
>                 tokio::time::sleep(std::time::Duration::from_millis(100 * (1 << i))).await;
>             }
>         }
>     }
>     Err(last_err.unwrap())
> }
> ```

**Q: What does `collect::<Result<Vec<T>, E>>()` do when one of the iterator elements is `Err`?**

> Collecting an iterator of `Result<T, E>` items into `Result<Vec<T>, E>` uses `FromIterator<Result<T, E>> for Result<Vec<T>, E>`. The behavior: it processes elements one by one. On the first `Err(e)`, it **stops immediately** and returns `Err(e)` — the remaining elements are NOT processed. On all `Ok(v)`, it collects into a `Vec<T>` and returns `Ok(vec)`. This is the standard "fail fast" behavior. If you want to collect ALL errors: use `partition_result()` from the `itertools` crate or `.fold()` manually. Example: `let (oks, errs): (Vec<_>, Vec<_>) = results.into_iter().partition_map(|r| match r { Ok(v) => Either::Left(v), Err(e) => Either::Right(e) })`.
