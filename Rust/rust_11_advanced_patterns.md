# Rust 11 — Advanced Patterns & Macros

---

## 11.1 The Builder Pattern

Construct complex objects step-by-step with validation:

```rust
#[derive(Debug)]
struct Request {
    url: String,
    timeout_ms: u64,
    retries: u32,
    headers: Vec<(String, String)>,
}

#[derive(Default)]
struct RequestBuilder {
    url: Option<String>,
    timeout_ms: u64,
    retries: u32,
    headers: Vec<(String, String)>,
}

impl RequestBuilder {
    fn url(mut self, url: impl Into<String>) -> Self {
        self.url = Some(url.into()); self
    }
    fn timeout_ms(mut self, ms: u64) -> Self { self.timeout_ms = ms; self }
    fn retries(mut self, n: u32) -> Self { self.retries = n; self }
    fn header(mut self, k: impl Into<String>, v: impl Into<String>) -> Self {
        self.headers.push((k.into(), v.into())); self
    }
    fn build(self) -> Result<Request, &'static str> {
        Ok(Request {
            url: self.url.ok_or("url is required")?,
            timeout_ms: self.timeout_ms,
            retries: self.retries,
            headers: self.headers,
        })
    }
}

let req = RequestBuilder::default()
    .url("https://example.com")
    .timeout_ms(5000)
    .header("Authorization", "Bearer token")
    .build()
    .unwrap();
```

---

## 11.2 Type-State Pattern

Encode state machine transitions in the type system — invalid transitions become compile errors:

```rust
struct Locked;
struct Unlocked;

struct Safe<State> {
    secret: String,
    _state: std::marker::PhantomData<State>,
}

impl Safe<Locked> {
    fn new(secret: String) -> Self {
        Safe { secret, _state: std::marker::PhantomData }
    }

    fn unlock(self, key: &str) -> Result<Safe<Unlocked>, Safe<Locked>> {
        if key == "correct_key" {
            Ok(Safe { secret: self.secret, _state: std::marker::PhantomData })
        } else {
            Err(self)
        }
    }
}

impl Safe<Unlocked> {
    fn get_secret(&self) -> &str { &self.secret }

    fn lock(self) -> Safe<Locked> {
        Safe { secret: self.secret, _state: std::marker::PhantomData }
    }
}

// safe.get_secret() doesn't exist on Safe<Locked> — compile error!
let safe = Safe::<Locked>::new("treasure".to_string());
let unlocked = safe.unlock("correct_key").unwrap();
println!("{}", unlocked.get_secret());
let locked = unlocked.lock();
// locked.get_secret();  // ERROR: method not found in `Safe<Locked>`
```

---

## 11.3 RAII — Resource Acquisition Is Initialization

Resources are tied to object lifetimes — automatically cleaned up when dropped:

```rust
struct TempFile {
    path: std::path::PathBuf,
}

impl TempFile {
    fn create(name: &str) -> std::io::Result<Self> {
        let path = std::env::temp_dir().join(name);
        std::fs::File::create(&path)?;
        Ok(TempFile { path })
    }
}

impl Drop for TempFile {
    fn drop(&mut self) {
        let _ = std::fs::remove_file(&self.path);  // cleanup on drop
    }
}

{
    let tmp = TempFile::create("test.txt").unwrap();
    // use tmp
}  // file deleted here — even if code above panicked
```

---

## 11.4 Newtype Pattern

Wrap a type in a struct to add type safety and implement traits:

```rust
struct Meters(f64);
struct Kilograms(f64);

// These are different types — you can't add Meters + Kilograms
impl std::ops::Add for Meters {
    type Output = Meters;
    fn add(self, other: Meters) -> Meters { Meters(self.0 + other.0) }
}

impl std::fmt::Display for Meters {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        write!(f, "{}m", self.0)
    }
}

let a = Meters(5.0);
let b = Meters(3.0);
let c = a + b;  // Meters(8.0)
// let d = a + Kilograms(3.0);  // compile error — type mismatch
```

Also used to implement foreign traits on foreign types (orphan rule workaround):
```rust
struct Wrapper(Vec<String>);
impl std::fmt::Display for Wrapper {  // allowed — Wrapper is local
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        write!(f, "[{}]", self.0.join(", "))
    }
}
```

---

## 11.5 Declarative Macros (`macro_rules!`)

Pattern-based code generation at compile time:

```rust
macro_rules! vec_of_strings {
    ( $( $x:expr ),* ) => {
        {
            let mut v = Vec::new();
            $( v.push($x.to_string()); )*
            v
        }
    };
}

let v = vec_of_strings!["hello", "world", "rust"];

// assert_eq! is a macro:
macro_rules! my_assert_eq {
    ($left:expr, $right:expr) => {
        if $left != $right {
            panic!("assertion failed: {:?} != {:?}", $left, $right);
        }
    };
    ($left:expr, $right:expr, $msg:literal) => {
        if $left != $right {
            panic!("{}: {:?} != {:?}", $msg, $left, $right);
        }
    };
}
```

**Macro fragment specifiers**: `expr` (expression), `ident` (identifier), `ty` (type), `literal`, `pat` (pattern), `stmt` (statement), `item`, `block`, `tt` (token tree), `path`.

---

## 11.6 Procedural Macros

Three types:
1. **Custom derive** — `#[derive(MyTrait)]`
2. **Attribute macro** — `#[my_attr]`
3. **Function-like macro** — `my_macro!(...)`

```rust
// Using proc-macro crates (serde, thiserror, tokio)
use serde::{Serialize, Deserialize};

#[derive(Serialize, Deserialize, Debug)]
struct Config {
    host: String,
    port: u16,
    #[serde(default)]
    timeout: u32,
    #[serde(rename = "retry_count")]
    retries: u32,
}

let json = r#"{"host":"localhost","port":8080,"retry_count":3}"#;
let config: Config = serde_json::from_str(json).unwrap();
println!("{:?}", config);
```

---

## 11.7 Higher-Kinded Behavior with GATs

Generic Associated Types (GATs) allow associated types to have their own generics:

```rust
trait Iterable {
    type Item<'a> where Self: 'a;   // GAT — lifetime parameter
    fn iter<'a>(&'a self) -> Self::Item<'a>;
}

struct MyVec<T>(Vec<T>);

impl<T> Iterable for MyVec<T> {
    type Item<'a> = std::slice::Iter<'a, T> where T: 'a;
    fn iter<'a>(&'a self) -> std::slice::Iter<'a, T> {
        self.0.iter()
    }
}
```

---

## 11.8 `std::borrow::Borrow` and `AsRef`

```rust
use std::collections::HashMap;

// HashMap<String, V>::get takes &Q where String: Borrow<Q>
// String: Borrow<str>, so you can look up by &str
let mut map: HashMap<String, i32> = HashMap::new();
map.insert("hello".to_string(), 1);
let val = map.get("hello");  // &str, not &String — works via Borrow

// AsRef — view data as a reference to another type
fn print_path(path: impl AsRef<std::path::Path>) {
    println!("{:?}", path.as_ref());
}
print_path("string literal");    // &str: AsRef<Path>
print_path(std::path::PathBuf::from("path"));  // PathBuf: AsRef<Path>
```

---

## 11.9 Trait Object Upcasting & Downcast

```rust
use std::any::Any;

trait Plugin: Any {
    fn name(&self) -> &str;
}

fn downcast_plugin<T: Plugin + 'static>(plugin: &dyn Plugin) -> Option<&T> {
    (plugin as &dyn Any).downcast_ref::<T>()
}

struct AudioPlugin { codec: String }
impl Plugin for AudioPlugin { fn name(&self) -> &str { "audio" } }
impl Any for AudioPlugin {}

let plugin: Box<dyn Plugin> = Box::new(AudioPlugin { codec: "mp3".into() });
if let Some(audio) = downcast_plugin::<AudioPlugin>(plugin.as_ref()) {
    println!("codec: {}", audio.codec);
}
```

---

## Interview Questions

### Easy

**Q: What is the newtype pattern and what problems does it solve?**

> The newtype pattern wraps an existing type in a single-field tuple struct: `struct Meters(f64)`. It solves three problems: (1) **Type safety**: `Meters` and `Kilograms` are distinct types even though both wrap `f64` — the compiler prevents mixing them. (2) **Trait implementation**: you can implement traits on `Meters` even though you can't implement foreign traits on `f64` (orphan rule). (3) **Abstraction**: hide the inner type and only expose a curated API. The newtype is zero-cost — the compiler eliminates the wrapper and treats it identically to the inner type. It's a purely compile-time distinction.

**Q: What is the RAII pattern and how does Rust enforce it?**

> RAII (Resource Acquisition Is Initialization) binds resource lifetime to object lifetime: acquire the resource in the constructor, release it in the destructor. In Rust, the `Drop` trait is the destructor. When a value goes out of scope, Rust automatically calls `drop()`. This is guaranteed even across early returns and panics (with `panic = unwind`). Examples: `MutexGuard` releases the lock when dropped, `File` closes on drop, `TempDir` deletes the directory on drop. Unlike C++ where exceptions can skip destructors in some cases, Rust's drop is deterministic. You can force early drop with `std::mem::drop(val)` (takes ownership). The language design makes resource leaks very hard to create accidentally.

**Q: What is a declarative macro (`macro_rules!`) and how is it different from a function?**

> `macro_rules!` defines a pattern-matching code generator that operates at compile time on the token stream. Differences from functions: (1) **Operate on syntax, not values** — macros take arbitrary syntax (expressions, types, patterns, identifiers) and generate new syntax. Functions take typed values. (2) **Variable argument count** — `vec![1, 2, 3]` takes any number of arguments. (3) **Hygiene** — macro-defined variables don't leak into the caller's scope. (4) **Context-sensitive** — macros can generate items, statements, or expressions depending on where they're called. (5) **Compile-time** — zero runtime overhead; all expansion happens before code generation. Downsides: harder to debug (error messages point to macro, not call site), no type checking of inputs until after expansion.

---

### Mid

**Q: Explain the type-state pattern and its advantage over runtime state checks.**

> The type-state pattern encodes the valid states of an object in the type system using phantom type parameters or distinct structs. Instead of `if self.state == Unlocked { ... }` at runtime, the compiler enforces that `get_secret()` is only callable on `Safe<Unlocked>` — calling it on `Safe<Locked>` is a compile error. Advantages: (1) **Zero runtime cost** — no state field, no branch, the type is erased at runtime. (2) **Impossible to misuse** — invalid sequences of operations are rejected at compile time, not discovered via panics or tests. (3) **Self-documenting** — the types tell you what operations are available in each state. Limitations: requires restructuring the code around state transitions (functions consume `Self` and return the new state type), verbose for complex state machines.

**Q: What is the `Borrow` trait and how does it differ from `AsRef`?**

> `Borrow<B>` and `AsRef<T>` both provide reference views, but with different semantic contracts. `Borrow<B>`: the borrowed form `B` must have the same hash and equality as the owning type — used for collection lookups. `HashMap<String, V>::get` accepts `&Q where String: Borrow<Q>`, meaning `String: Borrow<str>`, so you can look up with `"hello"` instead of `&"hello".to_string()`. `AsRef<T>`: a cheap reference-to-reference conversion with no equality guarantee — used for API ergonomics. `fn write(path: impl AsRef<Path>)` accepts `&str`, `String`, `PathBuf` all in one. The key distinction: if `Borrow` is implemented, hashing and equality must be consistent between the owned and borrowed forms; `AsRef` has no such requirement.

**Q: What is a procedural macro and how does it differ from `macro_rules!`?**

> A procedural macro is a Rust function that takes a `TokenStream` and returns a `TokenStream` — essentially a compiler plugin. Unlike `macro_rules!` (which uses declarative pattern matching on the token tree), a procedural macro has full programmatic control over the code it generates, using crates like `syn` for parsing and `quote!` for generating code. Three flavors: (1) **Custom derive**: `#[derive(Serialize)]` — generates trait impls based on the struct/enum definition. (2) **Attribute macro**: `#[route(GET, "/path")]` — transforms any item. (3) **Function-like macro**: `html!()` — arbitrary code generation. Procedural macros live in their own crate (`proc-macro = true`) because they run at compile time in the compiler's process. They're more powerful but also harder to write than `macro_rules!`.

---

### Hard

**Q: Implement a `retry!` macro that retries an expression up to N times and returns `Result`.**

> ```rust
> macro_rules! retry {
>     ($n:expr, $e:expr) => {{
>         let mut last_err = None;
>         let mut result = None;
>         for _ in 0..$n {
>             match $e {
>                 Ok(v)  => { result = Some(v); break; }
>                 Err(e) => { last_err = Some(e); }
>             }
>         }
>         result.ok_or_else(|| last_err.unwrap())
>     }};
> }
>
> // Usage
> let mut attempt = 0;
> let result: Result<i32, &str> = retry!(3, {
>     attempt += 1;
>     if attempt < 3 { Err("not yet") } else { Ok(42) }
> });
> assert_eq!(result, Ok(42));
>
> // The macro re-evaluates $e each iteration (unlike a function parameter
> // which evaluates once). This is a key difference between macros and
> // functions — macros capture the *syntax*, not the *value*.
> ```

**Q: How do you use `GATs` (Generic Associated Types) to implement a lending iterator?**

> A "lending iterator" yields references into itself — the standard `Iterator` cannot do this because `Item` doesn't carry a lifetime tied to `&self`. GATs fix this: ```rust trait LendingIterator { type Item<'a> where Self: 'a; fn next<'a>(&'a mut self) -> Option<Self::Item<'a>>; } // Windows: yields overlapping slices struct Windows<'s, T> { slice: &'s [T], size: usize, pos: usize } impl<'s, T> LendingIterator for Windows<'s, T> { type Item<'a> = &'a [T] where Self: 'a; fn next<'a>(&'a mut self) -> Option<&'a [T]> { if self.pos + self.size > self.slice.len() { return None; } let win = &self.slice[self.pos..self.pos + self.size]; self.pos += 1; Some(win) } }``` Without GATs, `type Item = &'? [T]` has no lifetime to fill in — you can't express that the item borrows from `self`. GATs (stable in Rust 1.65) make this possible. The trade-off: a lending iterator doesn't compose with standard iterator adaptors (you can't call `.map()` etc.) since those don't handle the self-borrowing.

**Q: What is the orphan rule and how do you work around it?**

> The orphan rule: you can only implement a trait for a type if **either the trait or the type is defined in your crate**. You cannot implement `Display` (foreign, from `std`) for `Vec<T>` (foreign, from `std`). This rule prevents conflicting implementations from different crates from breaking downstream code. Workarounds: (1) **Newtype pattern**: wrap the foreign type in your own struct: `struct MyVec(Vec<String>)`, then implement `Display for MyVec`. (2) **Blanket implementation via your own trait**: define `trait MyDisplay` in your crate, implement it for `Vec<T>`, then write a blanket `impl<T: MyDisplay> Display for Wrapper<T>`. (3) **Extension traits**: define `trait VecExt` in your crate with additional methods — callers bring it into scope with `use`. The orphan rule is a deliberate design choice for ecosystem coherence — without it, crate A and crate B could both implement `Serialize for Vec<T>` and cause ambiguity.
