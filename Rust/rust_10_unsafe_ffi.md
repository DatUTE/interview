# Rust 10 — Unsafe Rust & FFI

---

## 10.1 What `unsafe` Unlocks

Rust's safety guarantees come from restrictions the borrow checker enforces. `unsafe` lifts **five specific restrictions**, nothing more:

1. Dereference raw pointers (`*const T`, `*mut T`)
2. Call `unsafe` functions or methods
3. Access or modify mutable static variables
4. Implement `unsafe` traits
5. Access fields of `union`s

```rust
unsafe {
    // All five become possible here
}
```

The compiler still enforces types, lifetimes, and everything else — `unsafe` is NOT a "turn off the compiler" mode.

---

## 10.2 Raw Pointers

```rust
let x = 5i32;
let ptr: *const i32 = &x;       // raw const pointer
let ptr_mut: *mut i32 = &mut { 5i32 };

// Creating raw pointers is SAFE
let y = 10i32;
let p = &y as *const i32;        // safe
let p2 = y as *const i32;        // ERROR: cast needs &T first

// Dereferencing raw pointers is UNSAFE
unsafe {
    println!("{}", *ptr);        // must be valid, aligned, initialized
    *ptr_mut = 42;
}

// Pointer arithmetic (unsafe)
let arr = [1i32, 2, 3, 4, 5];
let p = arr.as_ptr();
unsafe {
    let third = *p.add(2);  // points to arr[2] = 3
}
```

---

## 10.3 `unsafe` Functions and Safe Wrappers

The pattern: mark the dangerous operation `unsafe`, document the invariants the caller must uphold, and wrap it in a safe public API that checks the invariants:

```rust
// Unsafe function — caller must ensure index is in bounds
unsafe fn get_unchecked(slice: &[i32], index: usize) -> i32 {
    *slice.as_ptr().add(index)
}

// Safe wrapper — check the invariant
fn get(slice: &[i32], index: usize) -> Option<i32> {
    if index < slice.len() {
        Some(unsafe { get_unchecked(slice, index) })
    } else {
        None
    }
}
```

---

## 10.4 `unsafe` Traits

A trait is `unsafe` if implementing it incorrectly can cause UB:

```rust
// Send and Sync are unsafe traits
unsafe impl Send for MyType { }
unsafe impl Sync for MyType { }

// Custom unsafe trait
unsafe trait MemorySafe {
    // Invariant: the implementor must guarantee X
}

unsafe impl MemorySafe for MyStruct {
    // We promise to uphold the invariant
}
```

---

## 10.5 FFI — Calling C from Rust

```rust
// Declare external C functions
extern "C" {
    fn abs(x: i32) -> i32;
    fn strlen(s: *const std::os::raw::c_char) -> usize;
    fn malloc(size: usize) -> *mut std::os::raw::c_void;
    fn free(ptr: *mut std::os::raw::c_void);
}

fn safe_abs(x: i32) -> i32 {
    unsafe { abs(x) }
}

// Strings require null terminator
use std::ffi::CString;
fn call_c_with_string(s: &str) {
    let c_str = CString::new(s).unwrap();  // adds null terminator
    unsafe {
        println!("len = {}", strlen(c_str.as_ptr()));
    }
}
```

**Cargo.toml** + **build.rs** for linking:
```toml
[build-dependencies]
cc = "1"
```
```rust
// build.rs
fn main() {
    cc::Build::new().file("src/native.c").compile("native");
    println!("cargo:rustc-link-lib=native");
}
```

---

## 10.6 Calling Rust from C

```rust
// Rust function callable from C
#[no_mangle]  // prevent name mangling
pub extern "C" fn rust_add(a: i32, b: i32) -> i32 {
    a + b
}

// Panic across FFI boundaries is UB — catch panics at the boundary
#[no_mangle]
pub extern "C" fn safe_entry(input: i32) -> i32 {
    match std::panic::catch_unwind(|| { process(input) }) {
        Ok(result) => result,
        Err(_)     => -1,  // return sentinel on panic
    }
}
```

C header to use Rust:
```c
// my_lib.h
int32_t rust_add(int32_t a, int32_t b);
int32_t safe_entry(int32_t input);
```

---

## 10.7 `bindgen` — Auto-Generate Bindings

`bindgen` reads a C header and generates Rust FFI declarations:

```toml
[build-dependencies]
bindgen = "0.68"
```

```rust
// build.rs
fn main() {
    let bindings = bindgen::Builder::default()
        .header("wrapper.h")
        .parse_callbacks(Box::new(bindgen::CargoCallbacks::new()))
        .generate()
        .expect("bindgen failed");
    bindings.write_to_file("src/bindings.rs").unwrap();
}
```

```rust
// src/lib.rs
#![allow(non_upper_case_globals, non_camel_case_types, non_snake_case)]
include!("bindings.rs");
```

---

## 10.8 C Types in Rust

```rust
use std::os::raw::{c_int, c_char, c_void, c_double, c_float};
use std::ffi::{CStr, CString};

// Convert C string to Rust &str
unsafe fn c_str_to_str<'a>(ptr: *const c_char) -> &'a str {
    CStr::from_ptr(ptr).to_str().expect("invalid UTF-8")
}

// Convert Rust &str to C string
fn str_to_c(s: &str) -> CString {
    CString::new(s).unwrap()
}
```

---

## 10.9 `std::mem` — Memory Manipulation

```rust
use std::mem;

// Size and alignment
println!("{}", mem::size_of::<u64>());      // 8
println!("{}", mem::align_of::<u64>());     // 8

// Swap without clone
let mut a = String::from("hello");
let mut b = String::from("world");
mem::swap(&mut a, &mut b);

// Replace — take ownership from behind &mut
let old = mem::replace(&mut a, String::from("new"));

// Forget — prevent drop (use carefully — leaks memory intentionally)
let v = vec![1, 2, 3];
mem::forget(v);  // heap not freed — use for FFI ownership transfer

// Transmute — reinterpret bits (extremely unsafe)
let bits: u32 = 0x3F800000;
let f: f32 = unsafe { mem::transmute(bits) };  // 1.0
```

---

## 10.10 Miri — Detecting Undefined Behavior

`miri` is an interpreter for Rust's MIR that detects UB in unsafe code:

```bash
rustup component add miri
cargo miri test
cargo miri run
```

Detects: out-of-bounds memory access, use-after-free, uninitialized memory reads, data races, invalid values.

---

## Interview Questions

### Easy

**Q: What are the five things `unsafe` allows in Rust?**

> (1) **Dereference raw pointers** (`*const T`, `*mut T`) — raw pointers can be null, dangling, or misaligned. (2) **Call `unsafe` functions or methods** — functions that require the caller to uphold invariants the compiler can't verify. (3) **Access or modify mutable static variables** — global mutable state requires the programmer to ensure no concurrent access. (4) **Implement `unsafe` traits** — traits where incorrect implementation can cause UB (e.g., `Send`, `Sync`). (5) **Access fields of `union`s** — unions have overlapping fields; the correct variant must be tracked manually. Everything else — type checking, borrow checking, lifetime checking — still applies inside `unsafe`. It is not a general "skip all checks" escape hatch.

**Q: What is a raw pointer and how does it differ from a reference?**

> A raw pointer (`*const T` or `*mut T`) is a memory address with no compile-time safety guarantees. Differences from references: (1) **No borrow checker** — raw pointers can be null, dangling, or point to freed memory. (2) **No lifetime** — no tracking of whether the pointed-to data is still alive. (3) **No exclusivity** — multiple `*mut T` can exist simultaneously (unlike `&mut T`). (4) **Explicit unsafe** — dereferencing requires an `unsafe` block. Creating a raw pointer from a reference is safe; dereferencing it is unsafe. Raw pointers are used in FFI (C APIs use pointers) and in building safe abstractions (like `Vec` or `HashMap` internally use raw pointer arithmetic).

**Q: What is `#[no_mangle]` and when do you need it?**

> By default, Rust performs **name mangling** — it encodes type information and the module path into the compiled symbol name. For example, `fn add(a: i32, b: i32)` might compile to `_ZN7my_crate3add17h1234abcd5678efghE`. This makes the symbol unrecognizable to C code or a dynamic linker looking for `add`. `#[no_mangle]` tells the compiler to emit the symbol with its literal Rust name — `rust_add` stays as `rust_add`. You need it whenever exposing a Rust function to: C FFI callers, dynamic linking, or any system that looks up symbols by name. Combine with `pub extern "C" fn` to also set the calling convention to C's ABI.

---

### Mid

**Q: What is undefined behavior (UB) in Rust and how does `unsafe` relate to it?**

> Undefined behavior is a state where the language spec makes no guarantees about what the program will do — the compiler is free to optimize based on the assumption that UB never occurs, which can produce wildly incorrect results. In safe Rust, the compiler prevents UB by construction (borrow checker, type system). In `unsafe` Rust, the programmer can violate the rules that prevent UB. Common UB in unsafe Rust: dereferencing a null or dangling pointer, data race (simultaneous mutable + any access from two threads), using memory after it's freed, creating two `&mut T` to the same data, reading uninitialized memory, producing an invalid value for a type (e.g., `bool` with value `2`). The key obligation: when you write `unsafe`, you must manually ensure none of these occur. The invariants must be documented as `# Safety` comments on `unsafe` functions.

**Q: How do you safely wrap a C callback that takes a function pointer?**

> C callbacks often take `void* context` plus a function pointer, letting you store Rust state in the context pointer: ```rust extern "C" { fn c_lib_register_callback(cb: unsafe extern "C" fn(*mut std::ffi::c_void), ctx: *mut std::ffi::c_void); } unsafe extern "C" fn my_callback(ctx: *mut std::ffi::c_void) { let rust_data = &*(ctx as *mut MyData); // Safety: ctx is a valid &MyData, cast from Box::into_raw rust_data.process(); } fn register(data: Box<MyData>) { let raw = Box::into_raw(data) as *mut _; // Safety: raw is valid for the lifetime of the callback registration unsafe { c_lib_register_callback(my_callback, raw); } // Must call Box::from_raw(raw) when unregistering to reclaim ownership }``` The key pattern: `Box::into_raw` transfers ownership to C (no drop), `Box::from_raw` reclaims ownership. The raw pointer is valid as long as you don't call `Box::from_raw` prematurely.

**Q: What is `mem::transmute` and when is it safe to use?**

> `mem::transmute::<T, U>` reinterprets the bits of a `T` as if they were a `U` — a full bit-cast with no conversion. It is **extremely unsafe**: (1) `T` and `U` must have the same size (compile error otherwise). (2) The bit pattern of `T` must be a valid `U` — otherwise UB. Valid uses: reinterpreting `f32` bits as `u32` (for bit manipulation), converting between `*const T` and `*mut T` (though `as` cast is preferred), checking layout of repr(C) types. Invalid uses: transmuting between types of different sizes, converting `&T` to `&U` across differing lifetimes (creates lifetime extension — UB), transmuting an `enum` to an integer without checking the value. Prefer `f32::to_bits()` / `f32::from_bits()` over transmute for the float case. Use transmute only as a last resort when no safe alternative exists.

---

### Hard

**Q: Implement a safe `split_at_mut` for slices, which is normally `unsafe` internally. Explain why it's safe.**

> ```rust
> fn split_at_mut<T>(slice: &mut [T], mid: usize) -> (&mut [T], &mut [T]) {
>     let len = slice.len();
>     assert!(mid <= len, "mid out of bounds");
>     let ptr = slice.as_mut_ptr();
>     unsafe {
>         (
>             // Safety: ptr[0..mid] and ptr[mid..len] are non-overlapping.
>             // Both are within bounds (mid <= len, len is valid).
>             // The raw pointer ptr comes from a valid &mut [T].
>             // No aliasing: the two slices cover disjoint ranges.
>             std::slice::from_raw_parts_mut(ptr, mid),
>             std::slice::from_raw_parts_mut(ptr.add(mid), len - mid),
>         )
>     }
> }
> // Why the borrow checker can't prove this safe without unsafe:
> // `slice[..mid]` and `slice[mid..]` would each require &mut slice — two
> // &mut to the same thing, which the borrow checker rejects even though
> // the ranges don't overlap. We know they're disjoint (by the assert),
> // but the borrow checker doesn't reason about range disjointness.
> // The unsafe block is our promise: "I've checked these don't overlap."
> ```

**Q: What is a vtable and how does Rust implement `dyn Trait` internally?**

> A vtable ("virtual table") is a compile-time-generated struct of function pointers for each method of a trait. When you have `Box<dyn Shape>`, Rust represents it as a **fat pointer**: two words — a pointer to the data and a pointer to the vtable for that concrete type. Structure: ```rust // Compiler-generated (conceptual) struct ShapeVTable { drop_in_place: unsafe fn(*mut ()),      // destructor size: usize, align: usize, area: fn(*const ()) -> f64,              // method pointers perimeter: fn(*const ()) -> f64, describe: fn(*const ()) -> String, }``` Calling `shape.area()` desugars to: load the vtable pointer from the fat pointer, look up `area` in the vtable, call it passing the data pointer as `self`. Cost: one extra pointer dereference (cache miss) per virtual call. This is equivalent to C++ virtual function calls (which also use vtables), but Rust's vtables include the destructor and size/align for the type — enabling uniform `drop` without knowing the concrete type.

**Q: Implement a custom `Vec<T>` using raw pointers. What invariants must you maintain?**

> ```rust
> use std::{alloc::{self, Layout}, ptr::{self, NonNull}, mem};
>
> struct MyVec<T> {
>     ptr: NonNull<T>,
>     len: usize,
>     cap: usize,
> }
>
> impl<T> MyVec<T> {
>     fn new() -> Self {
>         MyVec { ptr: NonNull::dangling(), len: 0, cap: 0 }
>     }
>
>     fn push(&mut self, val: T) {
>         if self.len == self.cap { self.grow(); }
>         unsafe { ptr::write(self.ptr.as_ptr().add(self.len), val); }
>         self.len += 1;
>     }
>
>     fn pop(&mut self) -> Option<T> {
>         if self.len == 0 { return None; }
>         self.len -= 1;
>         Some(unsafe { ptr::read(self.ptr.as_ptr().add(self.len)) })
>     }
>
>     fn grow(&mut self) {
>         let new_cap = if self.cap == 0 { 1 } else { self.cap * 2 };
>         let layout = Layout::array::<T>(new_cap).unwrap();
>         let raw = if self.cap == 0 {
>             unsafe { alloc::alloc(layout) }
>         } else {
>             let old = Layout::array::<T>(self.cap).unwrap();
>             unsafe { alloc::realloc(self.ptr.as_ptr() as *mut u8, old, layout.size()) }
>         };
>         self.ptr = NonNull::new(raw as *mut T).expect("allocation failed");
>         self.cap = new_cap;
>     }
> }
>
> impl<T> Drop for MyVec<T> {
>     fn drop(&mut self) {
>         if self.cap > 0 {
>             while let Some(_) = self.pop() {}  // drop elements
>             let layout = Layout::array::<T>(self.cap).unwrap();
>             unsafe { alloc::dealloc(self.ptr.as_ptr() as *mut u8, layout); }
>         }
>     }
> }
> // Invariants: ptr is valid and aligned for T (when cap > 0);
> // ptr[0..len] are initialized; ptr[len..cap] are uninitialized;
> // cap >= len; when cap==0, ptr is dangling (never deref'd).
> ```
