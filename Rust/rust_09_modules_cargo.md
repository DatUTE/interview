# Rust 09 — Modules, Crates & Cargo

---

## 9.1 Module System

Rust organizes code into **modules** (namespacing) and **crates** (compilation units):

```
crate (root = src/lib.rs or src/main.rs)
├── mod audio
│   ├── mod codec
│   └── mod mixer
└── mod network
    └── mod http
```

```rust
// src/lib.rs
mod audio;      // loads src/audio.rs or src/audio/mod.rs
mod network;

// src/audio.rs
pub mod codec;  // loads src/audio/codec.rs
pub mod mixer;

pub fn play() { /* ... */ }
```

---

## 9.2 Visibility

| Keyword | Visible to |
|---------|-----------|
| (none) | Current module only |
| `pub` | Everyone |
| `pub(crate)` | Anywhere in the crate |
| `pub(super)` | Parent module |
| `pub(in path)` | Specific module path |

```rust
mod outer {
    pub struct Foo {
        pub bar: i32,    // public field
        baz: String,     // private field
    }

    pub(crate) fn helper() { }  // crate-internal only

    mod inner {
        fn private() { }
        pub(super) fn semi_public() { }  // only visible to `outer`
    }
}
```

---

## 9.3 `use` — Bringing Paths into Scope

```rust
// Absolute path from crate root
use crate::audio::codec::Mp3Codec;

// Standard library
use std::collections::HashMap;
use std::io::{self, Read, Write};   // multiple items

// External crate (after adding to Cargo.toml)
use serde::{Serialize, Deserialize};

// Rename with `as`
use std::fmt::Display as Fmt;

// Re-export — expose internal items as public API
pub use crate::audio::play;   // callers can use mylib::play instead of mylib::audio::play

// Glob (avoid in production — obscures where names come from)
use std::collections::*;
```

---

## 9.4 Cargo — The Build System

```toml
# Cargo.toml
[package]
name    = "my_app"
version = "0.1.0"
edition = "2021"

[dependencies]
serde       = { version = "1", features = ["derive"] }
tokio       = { version = "1", features = ["full"] }
reqwest     = "0.11"
log         = "0.4"

[dev-dependencies]
criterion   = "0.5"   # benchmarks only
mockall     = "0.11"  # tests only

[build-dependencies]
cc          = "1"     # build scripts only

[features]
default     = ["json"]
json        = ["serde/derive"]
audio       = []

[[bin]]
name = "cli"
path = "src/bin/cli.rs"

[[example]]
name = "demo"
path = "examples/demo.rs"
```

**Common Cargo commands:**
```bash
cargo new my_project       # new binary
cargo new --lib my_lib     # new library
cargo build                # debug build
cargo build --release      # optimized build
cargo run                  # build + run
cargo test                 # run all tests
cargo test -- --nocapture  # show println! output
cargo bench                # run benchmarks
cargo clippy               # lint
cargo fmt                  # format
cargo doc --open           # generate and open docs
cargo add serde            # add dependency (cargo 1.62+)
cargo update               # update Cargo.lock
cargo tree                 # show dependency tree
cargo audit                # check for security advisories
```

---

## 9.5 Workspace

A workspace groups multiple related crates sharing a single `Cargo.lock`:

```toml
# Cargo.toml (workspace root)
[workspace]
members = [
    "core",
    "cli",
    "web",
]
resolver = "2"  # recommended for feature unification

[workspace.dependencies]  # shared dependency versions (Cargo 1.64+)
serde = { version = "1", features = ["derive"] }
tokio = { version = "1", features = ["full"] }
```

```toml
# core/Cargo.toml
[package]
name = "core"

[dependencies]
serde.workspace = true  # inherits version from workspace
```

---

## 9.6 Feature Flags

```rust
// Conditional compilation with features
#[cfg(feature = "json")]
pub mod json {
    pub fn parse() { /* ... */ }
}

// cfg attribute on any item
#[cfg(target_os = "linux")]
fn linux_only() { }

#[cfg(debug_assertions)]
fn debug_check() { }
```

---

## 9.7 Semantic Versioning (SemVer)

Cargo uses SemVer for dependency versions:

| Version spec | Meaning |
|--------------|---------|
| `"1.2.3"` | `>=1.2.3, <2.0.0` |
| `"~1.2.3"` | `>=1.2.3, <1.3.0` |
| `"^1.2.3"` | `>=1.2.3, <2.0.0` (same as `"1.2.3"`) |
| `"=1.2.3"` | exactly 1.2.3 |
| `">=1, <2"` | range |

**SemVer rules for library authors**:
- Patch (1.2.**x**): bug fixes, no API changes
- Minor (1.**x**.0): new features, backwards-compatible
- Major (**x**.0.0): breaking API changes

---

## 9.8 Testing

```rust
// Unit tests — in the same file as the code
#[cfg(test)]
mod tests {
    use super::*;  // access private items from parent module

    #[test]
    fn test_add() {
        assert_eq!(add(2, 3), 5);
    }

    #[test]
    #[should_panic(expected = "divide by zero")]
    fn test_panic() {
        divide(1, 0);
    }

    #[test]
    fn test_result() -> Result<(), String> {
        let v: i32 = "42".parse().map_err(|e| format!("{}", e))?;
        assert_eq!(v, 42);
        Ok(())
    }
}

// Integration tests — in tests/ directory
// tests/integration_test.rs
use my_lib::public_function;

#[test]
fn test_public_api() {
    assert_eq!(public_function(), 42);
}
```

---

## 9.9 Benchmarks with Criterion

```rust
// benches/my_bench.rs
use criterion::{black_box, criterion_group, criterion_main, Criterion};

fn fibonacci(n: u64) -> u64 { /* ... */ }

fn bench_fib(c: &mut Criterion) {
    c.bench_function("fibonacci 20", |b| {
        b.iter(|| fibonacci(black_box(20)))
    });
}

criterion_group!(benches, bench_fib);
criterion_main!(benches);
```

`black_box` prevents the optimizer from eliminating the computation.

---

## Interview Questions

### Easy

**Q: What is the difference between a crate and a module in Rust?**

> A **crate** is Rust's compilation unit — the smallest piece of code the compiler handles. A crate compiles to either an executable (binary crate, with `fn main`) or a library (library crate, `src/lib.rs`). Crates have their own namespaces. A **module** is an organizational unit within a crate — it groups related items (functions, structs, traits) and controls visibility. Modules can be nested arbitrarily. All code in a crate belongs to some module, with the crate root being the top-level `mod`. Analogy: a crate is like a Java package (compilation and distribution unit); a module is like a Java class or sub-package (organization within that unit).

**Q: What does `pub(crate)` mean and when would you use it?**

> `pub(crate)` restricts visibility to the entire crate but makes the item invisible to external code (other crates that depend on this crate). It's the "internal" visibility level. Use it for: helper functions that are shared between modules in your crate but are not part of the public API; internal types used across modules; any implementation detail you don't want external users to depend on. This is stronger encapsulation than `pub` (which exposes to all users) but more permissive than default private (which limits to the current module). It documents intent: "this is an internal implementation detail, not a public contract."

**Q: What is `Cargo.lock` and when should you commit it?**

> `Cargo.lock` records the exact version of every dependency (direct and transitive) used in the last successful build. **Always commit `Cargo.lock` for executables/applications** — it ensures reproducible builds across machines and CI. **Do not commit `Cargo.lock` for libraries** — libraries need to be compatible with a range of dependency versions (what their users have), and locking forces users to your exact versions, potentially causing conflicts. Cargo's convention: if `Cargo.lock` is present, it's used for exact reproducibility; if absent (published library), Cargo resolves the latest compatible versions per `Cargo.toml` constraints.

---

### Mid

**Q: What is a workspace and what problem does it solve?**

> A workspace is a collection of related Rust crates that share a `Cargo.lock` and an `output/target` directory. Problem solved: in large projects with multiple crates (e.g., a core library + CLI + web server + shared test utilities), without a workspace each crate would have its own `Cargo.lock` and separate build cache. With a workspace: (1) All crates share one `Cargo.lock` — dependency versions are consistent across all crates. (2) Shared `target/` directory — builds are cached across crates, avoiding recompilation. (3) `cargo build` at the workspace root builds all members. (4) `[workspace.dependencies]` deduplicates version declarations. Common pattern: `core` (domain logic), `cli` (CLI front-end using `core`), `web` (HTTP server using `core`).

**Q: How do conditional compilation features work with `#[cfg(feature = "...")]`?**

> Features are opt-in capabilities declared in `Cargo.toml` under `[features]`. A feature is just a string flag; it enables `#[cfg(feature = "name")]` code blocks, which are either compiled in or removed at compile time. Feature flags enable optional dependencies (`serde = { version = "1", optional = true }` — only compiled when `serde` feature is enabled), platform-specific code, and capability flags. When a crate is depended on with `features = ["json"]`, only code gated on `cfg(feature = "json")` is included. The `default` feature specifies what's enabled without explicit opt-in. Features are additive — you can enable a superset of what a dependency enables, but you cannot disable a feature a dependency already has.

**Q: What is the difference between `[dependencies]`, `[dev-dependencies]`, and `[build-dependencies]`?**

> `[dependencies]`: compiled into the final library/binary and available in all code (`src/`). `[dev-dependencies]`: only compiled when running tests or benchmarks — not included in released/distributed builds. Use for test frameworks, mock libraries, property testing. Reduces final binary size and compile time for users. `[build-dependencies]`: only compiled for and available in `build.rs` (the build script). The build script runs before compiling the main crate to do code generation (bindgen, protobuf, compiling C code with `cc`). Its dependencies are not available in `src/`.

---

### Hard

**Q: How would you structure a workspace for a production Rust project with a library core, CLI, and gRPC server?**

> ```
> my-project/
> ├── Cargo.toml            # workspace root
> ├── Cargo.lock
> ├── core/                 # domain logic — pure library, no I/O
> │   ├── Cargo.toml
> │   └── src/lib.rs
> ├── cli/                  # CLI binary using core
> │   ├── Cargo.toml        # depends on core
> │   └── src/main.rs
> ├── grpc-server/          # gRPC server using core
> │   ├── Cargo.toml        # depends on core + tonic
> │   ├── build.rs          # runs protoc via tonic-build
> │   ├── proto/            # .proto files
> │   └── src/main.rs
> └── common/               # shared test utilities, error types
>     ├── Cargo.toml
>     └── src/lib.rs
> ```
> Workspace `Cargo.toml`:
> ```toml
> [workspace]
> members = ["core", "cli", "grpc-server", "common"]
> resolver = "2"
> [workspace.dependencies]
> tokio  = { version = "1", features = ["full"] }
> serde  = { version = "1", features = ["derive"] }
> tonic  = "0.10"
> prost  = "0.12"
> ```
> Key design: `core` has zero I/O — pure functions and domain types. All async runtime specifics live in `grpc-server` and `cli`. `common` holds error types and test helpers used across crates. This makes `core` easily testable without mocking any I/O.

**Q: Explain how `cargo publish` and semantic versioning affect library API design.**

> Publishing with `cargo publish` uploads to crates.io permanently (versions cannot be deleted, only `yank`ed to prevent new adoption). SemVer contract: **any removal or signature change of a public item is a breaking change** requiring a major version bump. What counts as breaking in Rust: removing a public function/type/trait, changing function signatures, adding non-`#[non_exhaustive]` variants to a public enum (callers' `match` breaks), removing trait implementations, tightening trait bounds. What is NOT breaking: adding new functions, adding `#[non_exhaustive]` enum variants (callers must use `..` catch-all), relaxing bounds. Best practices: mark extensible enums `#[non_exhaustive]`; keep the public API surface minimal; use `pub(crate)` liberally; run `cargo semver-checks` before releasing to verify no accidental breaking changes.
