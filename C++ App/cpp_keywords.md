# Modern C++ Keywords & Specifiers

## Quick Reference

| Keyword | Since | Category |
|---|---|---|
| `auto` | C++11 | Type deduction |
| `decltype` | C++11 | Type deduction |
| `nullptr` | C++11 | Null pointer |
| `override` | C++11 | Virtual control |
| `final` | C++11 | Virtual control |
| `explicit` | C++11 (conv. ops) | Conversion control |
| `noexcept` | C++11 | Exception spec |
| `constexpr` | C++11 | Compile-time |
| `static_assert` | C++11 | Compile-time |
| `thread_local` | C++11 | Storage duration |
| `mutable` | C++11 (lambdas) | Const bypass |
| `= default` / `= delete` | C++11 | Special members |
| `alignas` / `alignof` | C++11 | Alignment |
| `inline` (variables) | C++17 | Linkage |
| `consteval` | C++20 | Compile-time |
| `constinit` | C++20 | Init guarantee |
| `[[nodiscard]]` | C++17 | Attributes |
| `[[likely]]` / `[[unlikely]]` | C++20 | Branch hints |
| `co_await` / `co_yield` / `co_return` | C++20 | Coroutines |

---

## 1. `const` vs `constexpr` vs `consteval` vs `constinit`

### `const` — runtime immutability

```cpp
const int x = 42;           // value fixed after init (can be runtime)
const int y = rand();        // fine — const, but not compile-time

void foo(const int n) {      // caller's copy is immutable inside foo
    // n = 5;  ← error
}

// Top-level const on pointer vs low-level const
int* const p = &x;           // p is const (can't reassign ptr)
const int* q = &x;           // *q is const (can't modify through q)
const int* const r = &x;     // both
```

### `constexpr` — compile-time capable (C++11)

```cpp
constexpr int square(int n) { return n * n; }
constexpr int s = square(7);    // evaluated at compile time
static_assert(s == 49);

int runtime_n = 7;
int r = square(runtime_n);     // OK — constexpr can still run at runtime

// C++14: constexpr functions may have multiple statements
constexpr int factorial(int n) {
    int result = 1;
    for (int i = 2; i <= n; ++i) result *= i;
    return result;
}
static_assert(factorial(5) == 120);

// constexpr object: must be initialized with a constant expression
constexpr double pi = 3.14159265358979;

// constexpr constructor (literal type)
struct Point {
    constexpr Point(int x, int y) : x_(x), y_(y) {}
    constexpr int x() const { return x_; }
    constexpr int y() const { return y_; }
    int x_, y_;
};
constexpr Point origin{0, 0};
static_assert(origin.x() == 0);
```

### `consteval` — must evaluate at compile time (C++20)

```cpp
// consteval: calling with a runtime value is a compile ERROR
consteval int compile_only_square(int n) { return n * n; }

constexpr int a = compile_only_square(5);  // OK
// int runtime = 5;
// int b = compile_only_square(runtime);   // ERROR — not constant

// Use case: enforce that a function is never used at runtime
// e.g., compile-time format string validation
consteval bool validate_format(const char* fmt) {
    // check format string at compile time
    return fmt[0] == '%' || fmt[0] != '\0';
}
static_assert(validate_format("%d"));
```

### `constinit` — initialization guarantee, no runtime init (C++20)

```cpp
// Guarantees variable is initialized with a constant expression
// Prevents the "static initialization order fiasco" for globals
constinit int g_counter = 0;   // must be constant-initialized
// constinit int bad = rand();  // ERROR — not a constant expression

// Key distinction from constexpr:
// constexpr → implicitly const, must be constant expression
// constinit → NOT const (can be modified later), just guarantees init is compile-time
constinit int g_value = 42;
g_value = 100;  // OK — constinit doesn't make it const

// Typical use: global/static variables where you want to avoid dynamic init
static constinit std::atomic<int> ref_count{0};
```

### Summary Table

| | `const` | `constexpr` | `consteval` | `constinit` |
|---|---|---|---|---|
| Immutable? | Yes | Yes | Yes | No |
| Compile-time eval | Maybe | Maybe | Always | Init only |
| Runtime callable | Yes | Yes | No | Yes |
| Use for | Read-only vars/params | Compile-time capable | Must be compile-time | Global init safety |

---

## 2. `noexcept`

Specifies that a function does not throw exceptions.

```cpp
void safe_func() noexcept { /* guaranteed no-throw */ }
void maybe_throws();          // may throw (default)
void definitely_throws() noexcept(false); // explicit: may throw

// Conditional noexcept (C++11)
template <typename T>
void swap(T& a, T& b) noexcept(std::is_nothrow_move_constructible_v<T> &&
                                 std::is_nothrow_move_assignable_v<T>) {
    T tmp = std::move(a);
    a = std::move(b);
    b = std::move(tmp);
}

// noexcept operator — compile-time check (returns bool)
static_assert(noexcept(safe_func()));
static_assert(!noexcept(maybe_throws()));

// Propagate noexcept-ness
template <typename T>
void wrapper(T& x) noexcept(noexcept(x.compute())) {
    x.compute();
}
```

### Why `noexcept` matters

```cpp
// std::vector uses move only if move constructor is noexcept
// Otherwise it falls back to copy (strong exception guarantee)

struct Cheap {
    Cheap(Cheap&&) noexcept = default;  // vector will MOVE on realloc
};
struct Expensive {
    Expensive(Expensive&&) = default;   // no noexcept → vector COPIES on realloc!
};

// Always mark move ctor/assign noexcept if they truly don't throw:
class Buffer {
public:
    Buffer(Buffer&& other) noexcept
        : data_(std::exchange(other.data_, nullptr))
        , size_(std::exchange(other.size_, 0)) {}
    Buffer& operator=(Buffer&& other) noexcept {
        if (this != &other) {
            delete[] data_;
            data_ = std::exchange(other.data_, nullptr);
            size_ = std::exchange(other.size_, 0);
        }
        return *this;
    }
private:
    int* data_;
    std::size_t size_;
};
```

### `noexcept` violation → `std::terminate`

```cpp
void must_not_throw() noexcept {
    throw std::runtime_error("oops"); // calls std::terminate immediately
    // stack may NOT be unwound (implementation-defined)
}
```

---

## 3. `inline`

### Function `inline` (hint since C++98, ODR since always)

```cpp
// Primary meaning: suppress ODR (One Definition Rule) violation
// Allows definition in header files included in multiple translation units

// header.h
inline int add(int a, int b) { return a + b; }
// Every TU that includes header.h gets its own copy — linker merges them

// Performance hint: compiler usually ignores this (inlines what it wants)
// __attribute__((always_inline)) / __forceinline — actual forcing
// [[gnu::noinline]] / __declspec(noinline)       — actual suppression
```

### Inline variables (C++17)

```cpp
// Before C++17: static data members had to be defined in exactly one .cpp
// C++17: inline variable — ODR-safe definition in header
struct Config {
    inline static int max_retries = 3;   // defined here, not in .cpp
    inline static const std::string version = "1.0.0";
};

// Global constants in headers (replaces extern const in .cpp pattern)
inline constexpr double pi = 3.14159265358979;
// Each TU gets a copy, linker collapses to one address

// Singleton via inline variable (C++17)
inline MyService& get_service() {
    static MyService instance;  // guaranteed single instance
    return instance;
}
```

---

## 4. `extern`

### `extern` — declaration without definition

```cpp
// In header or top of file: declare without allocating storage
extern int g_count;      // declared here
// In exactly one .cpp:
int g_count = 0;         // defined here

// Typical pattern (header.h):
extern int g_config_flags;
// config.cpp:
int g_config_flags = 0;
```

### `extern "C"` — C linkage (no name mangling)

```cpp
// Expose C++ functions to C code, or call C libraries
extern "C" {
    void c_callback(int x);          // no C++ name mangling
    int c_lib_function(const char*); // call from C
}

// Single function
extern "C" void plugin_init();

// Header guard for C/C++ interop
#ifdef __cplusplus
extern "C" {
#endif

void my_c_api(int x);

#ifdef __cplusplus
}
#endif

// Why: C++ mangles names (foo(int) → _Z3fooi) for overloading support
// C has no mangling. extern "C" tells the linker to use C-style symbol names.
```

### `extern template` — suppress implicit instantiation

```cpp
// In header (shared):
extern template class std::vector<MyType>;  // don't instantiate here

// In exactly one .cpp:
template class std::vector<MyType>;         // instantiate here only

// Benefit: reduce compile time and binary size for commonly used templates
// All TUs share one instantiation instead of each creating their own
```

---

## 5. `static`

Four completely different meanings depending on context:

```cpp
// 1. Static local variable — initialized once, persists across calls
void counter() {
    static int count = 0;   // zero-initialized, thread-safe init (C++11)
    ++count;
    std::cout << count;
}

// 2. Static class member — shared across all instances
struct Config {
    static int max_connections;   // declaration (one per class, not per object)
    static void reset();
};
int Config::max_connections = 100;   // definition in .cpp

// 3. Static free function / variable — internal linkage (file-scope)
static int helper() { return 42; }  // invisible outside this TU
static int module_counter = 0;      // not visible to other TUs
// Modern replacement: anonymous namespace (see below)

// 4. Static member function — no 'this' pointer
struct Math {
    static int square(int n) { return n * n; }  // call without object
};
Math::square(5);
```

### Anonymous namespace vs `static`

```cpp
// Preferred over static for internal linkage in C++
namespace {
    int internal_helper() { return 42; }  // only visible in this TU
    struct InternalType { /* ... */ };    // works for types too (static doesn't)
}

// static: works for functions and variables but not types
```

---

## 6. `auto` and `decltype`

### `auto` — type deduction (C++11)

```cpp
auto x = 42;             // int
auto y = 3.14;           // double
auto z = "hello";        // const char*
auto s = std::string{}; // std::string

// Range-for (always use auto& or const auto&)
std::vector<int> v{1,2,3};
for (auto& elem : v) elem *= 2;         // modify
for (const auto& elem : v) use(elem);   // read

// Iterator type (avoids typing)
auto it = v.begin();     // std::vector<int>::iterator

// Structured bindings (C++17)
std::map<int,std::string> m{{1,"a"}};
for (auto& [key, val] : m) { /* ... */ }

auto [x2, y2] = std::pair{1, 2.0};  // x2=int, y2=double

// auto strips top-level const and reference!
const int ci = 5;
auto a = ci;       // a is int, not const int
auto& b = ci;      // b is const int& (reference preserves const)
```

### `decltype` — exact type of expression (C++11)

```cpp
int x = 5;
decltype(x) y = 10;       // int (same type as x)
decltype(x + 0.0) z;      // double

const int& r = x;
decltype(r) r2 = x;       // const int& (preserves const and ref — unlike auto)

// decltype vs auto — key difference
auto a = r;       // int (strips ref and const)
decltype(r) b = x; // const int& (exact type)

// Trailing return type (C++11, before C++14 return type deduction)
template <typename T, typename U>
auto add(T a, U b) -> decltype(a + b) { return a + b; }

// decltype(auto) — deduce like decltype (C++14)
decltype(auto) f() {
    int x = 42;
    return x;    // returns int (not int&)
}
decltype(auto) g(int& r) {
    return r;    // returns int& (preserves reference — plain auto would strip it)
}

// Use case: perfect return type forwarding in generic code
template <typename F, typename... Args>
decltype(auto) invoke_and_forward(F&& f, Args&&... args) {
    return std::forward<F>(f)(std::forward<Args>(args)...);
}
```

---

## 7. `override` and `final`

```cpp
struct Base {
    virtual void foo(int);
    virtual void bar();
    virtual ~Base() = default;
};

struct Derived : Base {
    void foo(int) override;   // GOOD: compile error if signature doesn't match
    // void foo(float) override; ← ERROR: no virtual foo(float) in Base
    // void foo(int);            ← silent bug: hides without overriding (no override)

    void bar() final;         // no further override in subclasses
};

struct LeafClass final : Derived {
    // void bar() override;   ← ERROR: bar() is final in Derived
};
// struct AnotherDerived : LeafClass {}; ← ERROR: LeafClass is final

// override — always use it, prevents silent hiding bugs
// final — use sparingly (hurts extensibility)
```

---

## 8. `explicit`

Prevents implicit (single-argument) conversions.

```cpp
struct Wrapper {
    explicit Wrapper(int n) : n_(n) {}
    explicit operator bool() const { return n_ != 0; }
    explicit operator int() const { return n_; }
    int n_;
};

void takes_wrapper(Wrapper w) {}

takes_wrapper(42);              // ERROR: implicit conversion blocked
takes_wrapper(Wrapper{42});    // OK: explicit

Wrapper w{5};
if (w) { }            // OK: bool context allows explicit operator bool
// int i = w;         // ERROR: implicit conversion to int blocked
int i = static_cast<int>(w);  // OK: explicit cast

// Without explicit:
struct ImplicitBad {
    ImplicitBad(int n) {}          // allows: ImplicitBad x = 5; — usually wrong
    operator bool() { return true; } // allows: int x = obj + 1; — usually wrong
};

// Rule: mark all single-argument constructors explicit unless conversion is intentional
// Notable exception: std::string(const char*) is intentionally implicit
```

### `explicit` with multiple params (C++20)

```cpp
// explicit(bool) — conditionally explicit
template <typename T>
struct Optional {
    template <typename U>
    explicit(!std::is_convertible_v<U, T>)
    Optional(U&& val) : val_(std::forward<U>(val)) {}
    T val_;
};
```

---

## 9. `mutable`

### `mutable` data member — modifiable even in `const` method

```cpp
class Cache {
public:
    int get(int key) const {
        auto it = cache_.find(key);      // modifies cache_
        if (it == cache_.end()) {
            cache_[key] = expensive_compute(key);  // OK: cache_ is mutable
        }
        return cache_[key];
    }
private:
    mutable std::unordered_map<int,int> cache_;
    mutable std::mutex mtx_;             // mutable: lockable in const methods
};

// Semantic rule: mutable = "not part of logical const-ness"
// Cache state is implementation detail, not observable state
```

### `mutable` lambda — allow modifying captured copies

```cpp
int count = 0;
auto incrementer = [count]() mutable {
    ++count;   // modifies the captured COPY (not the outer count)
    return count;
};
incrementer();  // inner count=1
incrementer();  // inner count=2
// outer count still 0

// Without mutable: captured copies are const inside lambda
auto bad = [count]() {
    // ++count;  // ERROR: captured copy is const
};
```

---

## 10. `= default` and `= delete`

```cpp
struct MyClass {
    // Explicitly request compiler-generated versions
    MyClass() = default;
    MyClass(const MyClass&) = default;
    MyClass(MyClass&&) = default;
    MyClass& operator=(const MyClass&) = default;
    MyClass& operator=(MyClass&&) = default;
    ~MyClass() = default;

    // Suppress generation
    MyClass(const MyClass&) = delete;   // non-copyable
    MyClass& operator=(const MyClass&) = delete;

    // Delete on overloads to prevent unintended implicit conversions
    void process(int) {}
    void process(double) = delete;  // prevent: process(3.14)
    void process(bool) = delete;    // prevent: process(true)
};

// Non-copyable, movable (unique ownership pattern)
struct UniqueResource {
    UniqueResource() = default;
    UniqueResource(const UniqueResource&) = delete;
    UniqueResource& operator=(const UniqueResource&) = delete;
    UniqueResource(UniqueResource&&) noexcept = default;
    UniqueResource& operator=(UniqueResource&&) noexcept = default;
};

// Prevent heap allocation
struct StackOnly {
    void* operator new(std::size_t) = delete;
    void* operator new[](std::size_t) = delete;
};
```

---

## 11. `volatile`

Tells the compiler: **every access to this variable must go to memory — no caching in register, no reordering with other volatile accesses.**

```cpp
volatile int hardware_register;       // memory-mapped I/O
volatile bool stop_flag;              // signal handler sets this

// Use case: memory-mapped hardware registers
volatile uint32_t* const UART_STATUS = reinterpret_cast<volatile uint32_t*>(0x40013800);
while (!(*UART_STATUS & TX_READY)) {}  // must re-read each loop iteration

// What volatile does NOT do:
// - NOT atomic (no race condition protection)
// - NOT a memory barrier for other CPUs (use std::atomic or std::memory_order)
// - NOT a substitute for mutex
// Modern C++: prefer std::atomic for multi-threaded flags
```

**`volatile` vs `std::atomic`:**

| | `volatile` | `std::atomic` |
|---|---|---|
| Prevents register caching | Yes | Yes |
| Prevents compiler reorder | Only vol-vol ops | Yes (with ordering) |
| Thread-safe | No | Yes |
| CPU memory barrier | No | Yes (acq/rel) |
| Use for | Hardware registers, setjmp | Shared flags, counters |

---

## 12. `thread_local`

Storage duration: one copy per thread, initialized per-thread on first use.

```cpp
thread_local int tls_counter = 0;          // each thread gets its own copy
thread_local std::string tls_name;         // default-constructed per thread

// Useful for: per-thread caches, per-thread error codes, per-thread buffers
thread_local char format_buffer[4096];

// errno is thread_local in POSIX (why errno is thread-safe)

// Per-thread cache (no mutex needed!)
std::string& get_thread_name() {
    thread_local std::string name = generate_name();  // called once per thread
    return name;
}

// thread_local static in function: initialized once per thread on first call
void per_thread_init() {
    thread_local static bool initialized = []{
        // expensive setup, runs once per thread
        setup_thread_context();
        return true;
    }();
    (void)initialized;
}
```

---

## 13. `alignas` and `alignof`

```cpp
// alignof — query alignment requirement
static_assert(alignof(int)    == 4);
static_assert(alignof(double) == 8);
static_assert(alignof(char)   == 1);

// alignas — set alignment of variable or type
alignas(64) int cache_line_aligned;       // align to cache line
alignas(16) float simd_vec[4];            // SSE alignment

struct alignas(32) AVXData {
    float data[8];
};

// False sharing prevention (C++17)
struct PaddedCounter {
    alignas(std::hardware_destructive_interference_size)  // usually 64
    std::atomic<int> count{0};
};

// Stack-allocated aligned storage (C++11)
alignas(alignof(std::string)) char buf[sizeof(std::string)];
std::string* p = new(buf) std::string("placement new");
p->~string();
```

---

## 14. `[[nodiscard]]`, `[[maybe_unused]]`, `[[likely]]`, `[[unlikely]]`

### `[[nodiscard]]` (C++17) — warn if return value ignored

```cpp
[[nodiscard]] int open_file(const char* path);
[[nodiscard("check error code")]] std::error_code send(const char* data);

open_file("x.txt");           // WARNING: nodiscard return ignored
auto fd = open_file("x.txt"); // OK

// On class: warn if object returned and discarded
struct [[nodiscard]] ErrorCode {
    int code;
    operator bool() const { return code == 0; }
};

// Standard library examples:
// std::async, std::empty(), std::launder — all nodiscard
```

### `[[maybe_unused]]` (C++17) — suppress unused warning

```cpp
[[maybe_unused]] int debug_value = compute();  // no warning even if unused

void process([[maybe_unused]] int debug_hint) {
    // debug_hint intentionally unused in release builds
}

// Useful with NDEBUG
void func(int x) {
    [[maybe_unused]] bool result = assert_invariant(x); // only used in debug
}
```

### `[[likely]]` / `[[unlikely]]` (C++20) — branch prediction hints

```cpp
int divide(int a, int b) {
    if (b == 0) [[unlikely]] {
        throw std::domain_error("div by zero");
    }
    return a / b;
}

while (has_data()) [[likely]] {
    process_next();
}

// Compiler arranges code so "likely" path has fewer branch mispredictions
// Use only when profiling confirms a branch is strongly biased
```

### Other Useful Attributes

```cpp
[[deprecated("use new_func instead")]]
void old_func();              // warning at call sites

[[deprecated]]
struct OldClass {};

[[noreturn]]
void fatal_error(const char* msg) {  // tells compiler: never returns
    std::fprintf(stderr, "%s\n", msg);
    std::terminate();
}
// Allows compiler to omit "return" warning and optimize subsequent dead code

[[fallthrough]]               // suppress fall-through warning in switch
switch (x) {
    case 1:
        do_something();
        [[fallthrough]];      // intentional fall-through
    case 2:
        do_more();
        break;
}

[[gnu::always_inline]]        // GCC/Clang: always inline regardless of optimizations
[[gnu::noinline]]             // GCC/Clang: never inline
[[clang::annotate("custom")]] // Clang: custom annotation (tooling)
[[msvc::forceinline]]         // MSVC
```

---

## 15. `using` Declarations and Aliases

```cpp
// Type alias (modern replacement for typedef)
using Byte = unsigned char;
using IntVec = std::vector<int>;
using Callback = std::function<void(int)>;

// Template alias (typedef can't do this)
template <typename T>
using Vec = std::vector<T>;
Vec<int> v;                  // std::vector<int>

template <typename Key, typename Val>
using HashMap = std::unordered_map<Key, Val, MyHash<Key>>;

// using in derived class — expose base member
struct Base {
    void foo(int);
    void foo(double);
    int x;
};
struct Derived : Base {
    using Base::foo;         // bring both overloads into scope
    using Base::x;           // change access (private → public)
    void foo(std::string);   // add overload (now all 3 visible)
};

// using in namespace
using std::cout;
using std::endl;
using namespace std;  // (not recommended in headers)
```

---

## 16. `sizeof`, `sizeof...`, `declval`

```cpp
// sizeof — size in bytes at compile time
static_assert(sizeof(int) == 4);
static_assert(sizeof(double) == 8);
sizeof(expr);   // doesn't evaluate expr — just its type's size

// sizeof... — variadic pack size
template <typename... Ts>
constexpr std::size_t pack_size = sizeof...(Ts);
static_assert(pack_size<int, double, char> == 3);

// std::declval — get reference to T without constructing it
// Use inside decltype/sizeof when T has no default constructor
template <typename T>
using member_type = decltype(std::declval<T>().member());

template <typename T, typename U>
using add_result = decltype(std::declval<T>() + std::declval<U>());

// add_result<int, double> → double
```

---

## 17. Lambda Specifiers

```cpp
// Full lambda syntax:
[captures](params) specifiers -> return_type { body }

// Specifiers:
auto f = [x = 5]() mutable -> int { return ++x; };  // mutable: can modify captures

// constexpr lambda (C++17) — callable in constant expressions
auto sq = [](int n) constexpr { return n * n; };
static_assert(sq(7) == 49);

// noexcept lambda
auto safe = []() noexcept { /* guaranteed no-throw */ };

// C++20: template lambda
auto identity = []<typename T>(T x) { return x; };

// C++23: explicit object parameter (deducing this)
struct Counter {
    int count = 0;
    auto increment(this Counter& self) { ++self.count; }  // explicit this
    auto get_adder(this auto& self) {                      // deducing this
        return [&self]{ ++self.count; };
    }
};
```

---

## 18. `new` / `delete` Placement and Forms

```cpp
// Placement new — construct in pre-allocated memory (no heap allocation)
alignas(T) char buf[sizeof(T)];
T* obj = new(buf) T{args};     // constructs in buf
obj->~T();                     // must manually call destructor!
// delete obj;                 // WRONG — buf not heap-allocated

// nothrow new — returns nullptr instead of throwing
T* p = new(std::nothrow) T{};
if (!p) { /* allocation failed */ }

// Array new/delete (must match!)
int* arr = new int[10]{};
delete[] arr;   // not delete arr!

// Global new/delete vs class-specific
// Override global new for tracking:
void* operator new(std::size_t size) {
    std::printf("alloc %zu bytes\n", size);
    return std::malloc(size);
}
void operator delete(void* p) noexcept {
    std::printf("free\n");
    std::free(p);
}

// Class-specific (overrides only for this type)
struct PoolAlloc {
    static void* operator new(std::size_t size);
    static void operator delete(void* p) noexcept;
};
```

---

## Interview Q&As

**Q [Mid]: What is the difference between `const` and `constexpr`?**

`const` means the variable is read-only after initialization — but initialization can happen at runtime (`const int n = rand()`). `constexpr` requires the value to be computable at compile time (the expression must be a constant expression). A `constexpr` variable is implicitly `const`, but a `const` variable is NOT implicitly `constexpr`. Use `constexpr` when you want compile-time evaluation (array sizes, template arguments, `static_assert`). Use `const` for runtime immutability (parameters, local variables you don't intend to modify).

---

**Q [Mid]: When should you mark a function `noexcept`?**

Mark a function `noexcept` when it genuinely cannot throw — or when you're willing to call `std::terminate` if it does. The most important cases: move constructors and move assignment operators (so `std::vector` uses moves on reallocation instead of copies), destructors (already implicitly `noexcept` in C++11), and `swap`. Use the conditional form `noexcept(noexcept(...))` to propagate the property generically. Don't mark a function `noexcept` speculatively — if it throws, `std::terminate` is called with no stack unwinding, which is worse than a regular exception.

---

**Q [Mid]: What does `[[nodiscard]]` do and why is it useful?**

`[[nodiscard]]` causes a compiler warning if the return value of a function is discarded (result not stored or used). It prevents bugs where callers ignore error codes, resource handles, or important results. Classic examples: `fopen()` returning `FILE*` (ignore it → resource leak), `send()` returning bytes sent (ignore it → silent data loss), `std::async()` (ignore it → runs synchronously, defeating the purpose). Since C++20 you can add a message explaining why: `[[nodiscard("check error code")]]`.

---

**Q [Senior]: Why does `std::vector` sometimes copy instead of move during reallocation, and how do you fix it?**

`std::vector` provides the **strong exception guarantee** on reallocation: if anything throws during reallocation, the original vector is unchanged. Moving elements is only safe if moves can't throw — otherwise a mid-reallocation failure would leave the vector in a broken state. So `std::vector` checks `std::is_nothrow_move_constructible_v<T>`: if true, it moves; if false, it copies (which can be expensive). Fix: mark your type's move constructor `noexcept`. This is the single biggest practical reason to add `noexcept` to move operations — it can change `O(n)` copy during reallocation to `O(1)` move.

---

**Q [Senior]: Explain `decltype(auto)` vs `auto` for return types.**

`auto` as a return type deduces the returned value type and strips references and top-level const — similar to pass-by-value deduction. `decltype(auto)` deduces the return type using `decltype` rules, preserving references and const. This matters when you want to forward a return value without changing its value category. Example: `auto f() { int x=0; return x; }` returns `int`. `decltype(auto) f() { int& r = ...; return r; }` returns `int&`. Use `decltype(auto)` in perfect-forwarding wrappers where you need to preserve whether the wrapped function returns a value, reference, or const reference. Using plain `auto` in that case would silently copy the return value.
