# Rust — Interview Preparation Index

## Learning Path (12 Tiers)

| File | Topic | Level |
|------|-------|-------|
| [rust_01_ownership_borrowing.md](rust_01_ownership_borrowing.md) | Ownership, Borrowing & Lifetimes | Fundamental |
| [rust_02_type_system_traits.md](rust_02_type_system_traits.md) | Type System & Traits | Fundamental |
| [rust_03_enums_pattern_matching.md](rust_03_enums_pattern_matching.md) | Enums, Option, Result & Pattern Matching | Fundamental |
| [rust_04_collections_iterators.md](rust_04_collections_iterators.md) | Collections, Iterators & Closures | Core |
| [rust_05_error_handling.md](rust_05_error_handling.md) | Error Handling | Core |
| [rust_06_memory_smart_pointers.md](rust_06_memory_smart_pointers.md) | Memory Model & Smart Pointers | Core |
| [rust_07_concurrency.md](rust_07_concurrency.md) | Concurrency & Parallelism | Intermediate |
| [rust_08_async_await.md](rust_08_async_await.md) | Async / Await & Futures | Intermediate |
| [rust_09_modules_cargo.md](rust_09_modules_cargo.md) | Modules, Crates & Cargo | Intermediate |
| [rust_10_unsafe_ffi.md](rust_10_unsafe_ffi.md) | Unsafe Rust & FFI | Advanced |
| [rust_11_advanced_patterns.md](rust_11_advanced_patterns.md) | Advanced Patterns & Macros | Advanced |
| [rust_12_interview_hot_topics.md](rust_12_interview_hot_topics.md) | Senior Interview Hot Topics | Interview |

---

## What Makes Rust Different

Rust's three core promises:
1. **Memory safety without GC** — ownership + borrow checker enforces at compile time
2. **Fearless concurrency** — `Send`/`Sync` traits prevent data races at compile time
3. **Zero-cost abstractions** — iterators, generics, traits compile to efficient machine code

The learning curve is front-loaded: once you internalize ownership, the rest of the language becomes coherent.

---

## 4-Week Suggested Study Order

**Week 1 — Core Language**
- File 01: Ownership, Borrowing & Lifetimes ← most important, spend extra time here
- File 02: Type System & Traits
- File 03: Enums, Option, Result & Pattern Matching

**Week 2 — Practical Rust**
- File 04: Collections, Iterators & Closures
- File 05: Error Handling
- File 06: Memory Model & Smart Pointers

**Week 3 — Concurrency & Async**
- File 07: Concurrency & Parallelism
- File 08: Async / Await & Futures
- File 09: Modules, Crates & Cargo

**Week 4 — Advanced & Interview Prep**
- File 10: Unsafe Rust & FFI
- File 11: Advanced Patterns & Macros
- File 12: Senior Interview Hot Topics

---

## Topic Coverage Matrix

| Topic | Files |
|-------|-------|
| Ownership / borrow checker | 01, 06 |
| Lifetimes | 01, 11 |
| Traits / generics | 02, 11 |
| Pattern matching | 03 |
| Error handling | 03, 05 |
| Iterators / closures | 04 |
| Smart pointers | 06 |
| Concurrency / threads | 07 |
| Async / tokio | 08 |
| Cargo / modules | 09 |
| Unsafe / FFI | 10 |
| Macros | 11 |
| Interview questions | 01–11 (each) + 12 |
