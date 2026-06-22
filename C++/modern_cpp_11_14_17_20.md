# Modern C++ (11 / 14 / 17 / 20)

---

## 1. Lambda Expressions & Captures

### Syntax

```cpp
[capture](params) -> return_type { body }
```

### Capture Modes

| Syntax | Meaning |
|---|---|
| `[]` | Capture nothing |
| `[=]` | Capture all locals by value |
| `[&]` | Capture all locals by reference |
| `[x]` | Capture `x` by value |
| `[&x]` | Capture `x` by reference |
| `[=, &x]` | All by value, except `x` by reference |
| `[this]` | Capture current object pointer |
| `[*this]` | Capture current object by value (C++17) |

```cpp
int base = 10;
auto add = [base](int x) { return base + x; };  // base copied at lambda creation
base = 99;
add(5);  // returns 15, not 104 — captured value frozen at creation time

auto add_ref = [&base](int x) { return base + x; };
add_ref(5);  // returns 104 — uses current value of base
```

**Dangling reference danger**:
```cpp
auto make_adder(int x) {
    return [&x](int y) { return x + y; };  // DANGER — x is gone after return
    return [x](int y)  { return x + y; };  // OK — x copied into lambda
}
```

### Generalized Captures (C++14)

Allows move-only types and arbitrary init expressions in captures:

```cpp
auto ptr = std::make_unique<Widget>();

// [ptr] — can't copy unique_ptr
// [&ptr] — dangerous if lambda outlives scope

auto lambda = [p = std::move(ptr)]() { p->doWork(); };  // move into lambda
```

### `mutable` Lambdas

By default, captured-by-value variables are `const` inside the lambda. `mutable` removes that:

```cpp
int count = 0;
auto inc = [count]() mutable { return ++count; };  // ok — modifies local copy
// original count unchanged
```

### Generic Lambdas (C++14)

```cpp
auto print = [](auto x) { std::cout << x; };  // template parameter deduction
print(42);
print("hello");
```

---

## 2. `auto`, `decltype`, `decltype(auto)`

### `auto`

Deduces type from initializer. Follows template deduction rules — **strips references and cv-qualifiers** by default.

```cpp
int x = 5;
int& ref = x;

auto a = ref;    // a is int, not int& — reference stripped
auto& b = ref;   // b is int& — reference preserved by explicit &
const auto c = x; // c is const int
```

**Common pitfalls**:
```cpp
std::vector<bool> v = {true, false};
auto elem = v[0];  // NOT bool — returns proxy object, can cause bugs
                   // use: bool elem = v[0];
```

### `decltype`

Returns the **declared type** of an expression without evaluating it. Preserves references and cv-qualifiers.

```cpp
int x = 5;
int& r = x;

decltype(x) a = 0;   // int
decltype(r) b = x;   // int& — reference preserved
decltype((x)) c = x; // int& — extra parens = lvalue expression → reference
```

Rule: `decltype(expr)` where `expr` is:
- A variable name → its declared type
- An lvalue expression `(x)` → `T&`
- An rvalue expression → `T`

### `decltype(auto)` (C++14)

Like `auto` but applies `decltype` rules instead of template deduction — **preserves references**:

```cpp
int x = 5;
int& getRef() { return x; }

auto a = getRef();          // int — strips reference
decltype(auto) b = getRef(); // int& — preserves reference

// Use case: perfect return type forwarding
template<typename F, typename... Args>
decltype(auto) call(F&& f, Args&&... args) {
    return std::forward<F>(f)(std::forward<Args>(args)...);
    // returns T& if f returns T&, T if f returns T
}
```

---

## 3. Structured Bindings (C++17)

Decompose a tuple, pair, array, or aggregate into named variables:

```cpp
// pair
auto [key, value] = std::make_pair("age", 30);

// map iteration — clean syntax
std::map<std::string, int> scores;
for (auto& [name, score] : scores) {
    std::cout << name << ": " << score;
}

// array
int arr[3] = {1, 2, 3};
auto [a, b, c] = arr;

// struct (aggregate)
struct Point { int x, y; };
Point p{10, 20};
auto [px, py] = p;
```

**With references**:
```cpp
auto& [x, y] = p;  // x and y are references — modifying them modifies p
x = 99;            // p.x is now 99
```

**With `const`**:
```cpp
const auto [x, y] = p;  // x and y are const
```

---

## 4. `std::optional`, `std::variant`, `std::any` (C++17)

### `std::optional<T>` — Value or Nothing

Replaces "sentinel values" like `-1`, `nullptr`, `""` as a signal for "no value". No heap allocation.

```cpp
std::optional<int> findUser(int id) {
    if (id > 0) return 42;   // has value
    return std::nullopt;     // no value
}

auto result = findUser(1);
if (result) {
    std::cout << *result;       // dereference
    std::cout << result.value(); // throws std::bad_optional_access if empty
}
int val = result.value_or(-1);  // safe fallback
```

**When to use**: function that might not find a result, optional configuration, deferred initialization.

---

### `std::variant<T1, T2, ...>` — Type-Safe Union

Holds exactly one value of a listed set of types. Knows which type it currently holds. No undefined behavior of raw `union`.

```cpp
std::variant<int, double, std::string> v = 42;
v = 3.14;
v = "hello";

// Access — throws std::bad_variant_access if wrong type
std::string& s = std::get<std::string>(v);

// Safe access
if (auto* p = std::get_if<int>(&v)) {
    std::cout << *p;
}

// Visitor pattern
std::visit([](auto&& val) {
    std::cout << val;
}, v);
```

**Use cases**: result type (success/error), AST node types, state machines, replacing inheritance hierarchies with value semantics.

---

### `std::any` — Type-Erased Value

Holds a value of **any** type. Type checked at runtime. Heap allocates for large types.

```cpp
std::any a = 42;
a = std::string("hello");
a = 3.14;

// Access — throws std::bad_any_cast if wrong type
double d = std::any_cast<double>(a);

// Safe check
if (a.type() == typeid(double)) { ... }
```

**When to use**: plugin systems, heterogeneous containers, configuration values with unknown types at compile time. Prefer `variant` when types are known — `any` trades type safety for flexibility.

---

## 5. Ranges & Views (C++20)

### The Problem with Classic STL

```cpp
// Classic — verbose, error-prone iterator pairs
std::vector<int> result;
std::copy_if(v.begin(), v.end(), std::back_inserter(result),
             [](int x) { return x % 2 == 0; });
std::sort(result.begin(), result.end());
```

### Ranges — Composable, Lazy Pipelines

```cpp
#include <ranges>
#include <algorithm>

std::vector<int> v = {5, 1, 4, 2, 3};

// Composable with | operator
auto result = v
    | std::views::filter([](int x) { return x % 2 == 0; })
    | std::views::transform([](int x) { return x * 2; });

for (int x : result) { ... }  // lazy — computed only when iterated
```

### Common Views

```cpp
std::views::filter(pred)       // keep elements matching predicate
std::views::transform(fn)      // map each element
std::views::take(n)            // first n elements
std::views::drop(n)            // skip first n elements
std::views::reverse            // iterate in reverse
std::views::keys               // keys of a map
std::views::values             // values of a map
std::views::enumerate          // (index, value) pairs (C++23)
std::views::iota(0, 10)        // generates 0,1,...,9 lazily
```

### Lazy Evaluation

Views don't compute until iterated — no intermediate vectors:

```cpp
// No allocation — filter/transform happen on-the-fly during iteration
auto expensive = std::views::iota(0, 1'000'000)
    | std::views::filter([](int x) { return x % 3 == 0; })
    | std::views::take(10);
```

---

## 6. Concepts (C++20)

### The Problem with Raw Templates

```cpp
template<typename T>
T add(T a, T b) { return a + b; }

add("x", "y");  // error message: 50 lines of template instantiation noise
```

### Concepts — Constraints on Template Parameters

```cpp
template<typename T>
concept Addable = requires(T a, T b) {
    { a + b } -> std::convertible_to<T>;
};

template<Addable T>
T add(T a, T b) { return a + b; }

add("x", "y");  // error: "T does not satisfy Addable" — clear!
```

### Syntax Variants

```cpp
// 1. requires clause on template parameter
template<typename T> requires std::integral<T>
T twice(T x) { return x * 2; }

// 2. Concept directly in template parameter list
template<std::integral T>
T twice(T x) { return x * 2; }

// 3. Abbreviated function template (auto + concept)
std::integral auto twice(std::integral auto x) { return x * 2; }

// 4. requires expression for complex constraints
template<typename T>
concept Serializable = requires(T t, std::ostream& os) {
    { t.serialize() } -> std::same_as<std::string>;
    { os << t };
};
```

### Standard Library Concepts

```cpp
std::integral<T>           // int, char, short, ...
std::floating_point<T>     // float, double, ...
std::same_as<T, U>         // T and U are identical
std::convertible_to<T, U>  // T implicitly converts to U
std::derived_from<T, Base> // T is derived from Base
std::invocable<F, Args...> // F can be called with Args
std::ranges::range<T>      // T is a range (has begin/end)
std::copyable<T>           // T is copyable
std::equality_comparable<T>// T supports ==
```

---

## 7. Coroutines (C++20)

### What is a Coroutine?

A function that can **suspend and resume** its execution. Unlike threads, suspension is cooperative and stackless — no extra thread, minimal overhead.

A function is a coroutine if its body contains `co_await`, `co_yield`, or `co_return`.

### Three Keywords

| Keyword | Meaning |
|---|---|
| `co_return` | Return a value and finish the coroutine |
| `co_yield` | Suspend and produce a value, resume later |
| `co_await` | Suspend until an async operation completes |

### `co_yield` — Generator Pattern

```cpp
#include <generator>  // C++23, or use custom promise

std::generator<int> fibonacci() {
    int a = 0, b = 1;
    while (true) {
        co_yield a;     // suspend and produce value
        auto tmp = a;
        a = b;
        b = tmp + b;
    }
}

for (int n : fibonacci() | std::views::take(10)) {
    std::cout << n << " ";  // 0 1 1 2 3 5 8 13 21 34
}
```

### `co_await` — Async Operations

```cpp
Task<std::string> fetch_data(std::string url) {
    auto response = co_await http_get(url);  // suspend here, resume when done
    auto body     = co_await response.read_body();
    co_return body;
}

// Caller
Task<void> main_task() {
    std::string data = co_await fetch_data("https://example.com");
    std::cout << data;
}
```

### How It Works Internally

The compiler transforms a coroutine into a state machine:
1. Allocates a **coroutine frame** on the heap (holds locals + suspension point)
2. On `co_await`/`co_yield` — saves state into the frame, returns to caller
3. On resume — restores state and continues from the suspension point
4. On `co_return` / end of body — destroys the frame

### Promise Type

Coroutines require a **promise type** that defines their behavior:

```cpp
struct Task {
    struct promise_type {
        Task get_return_object() { return {}; }
        std::suspend_never  initial_suspend() { return {}; }
        std::suspend_never  final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() { std::terminate(); }
    };
};
```

**Practical note**: C++20 provides the coroutine machinery but no ready-made task/generator types (except `std::generator` in C++23). In practice, use a library like `cppcoro`, `folly::coro`, or your framework's coroutine support.

---

## Mock Interview Questions

### Lambdas & Captures

---

**[Mid]** What is the difference between capturing by value `[=]` and by reference `[&]`? When is each dangerous?

> **By value `[=]`**: copies the captured variables at lambda creation time. Mutations inside the lambda don't affect the original (unless `mutable`). Danger: accidentally copying large objects or shared_ptr (bumping ref count unintentionally).
>
> **By reference `[&]`**: the lambda holds a reference to the outer variable — reads the live value. Danger: **dangling reference** if the lambda outlives the captured variable (e.g. lambda stored in a callback queue, outer variable on a stack that's already gone).
>
> ```cpp
> auto bad = [&]() { return local_var + 1; };  // fine if called in same scope
> store_for_later(bad);                         // DANGER — local_var may be gone
>
> auto safe = [=]() { return local_var + 1; };  // copy — safe regardless of lifetime
> store_for_later(safe);
> ```

---

**[Mid]** What does `[*this]` capture do in C++17, and why is it needed?

> `[this]` captures a pointer to the current object — if the object is destroyed before the lambda executes, you get a dangling pointer.
>
> `[*this]` (C++17) **copies the entire object** into the lambda's closure. Useful when you need to schedule a lambda on another thread and the current object might not outlive the lambda:
>
> ```cpp
> struct Widget {
>     int value = 42;
>     void schedule() {
>         // [this] — lambda may outlive Widget → dangling
>         // [*this] — lambda has its own copy of Widget
>         auto task = [*this]() { return value * 2; };
>         post_to_thread_pool(task);
>     }
> };
> ```

---

**[Senior]** Why can't you capture a `unique_ptr` with `[=]`? How do you move it into a lambda?

> `[=]` copies all captured variables. `unique_ptr` is not copyable — copy constructor is `= delete`. Attempting `[=]` with a `unique_ptr` in scope is a compile error.
>
> Use a **generalized capture** (C++14) to move it in:
>
> ```cpp
> auto ptr = std::make_unique<Widget>();
>
> // [=]  → compile error: unique_ptr is not copyable
> // [&ptr] → compiles but dangerous (dangling reference risk)
>
> auto lambda = [p = std::move(ptr)]() {
>     p->doWork();  // p is now owned by the lambda
> };
> // ptr is null here
> ```
>
> The lambda now owns the `unique_ptr` — it's effectively a move-only lambda, which means it can't be stored in `std::function` (requires copyability). Use `std::move_only_function` (C++23) or a hand-rolled wrapper instead.

---

### `auto`, `decltype`, `decltype(auto)`

---

**[Mid]** What is the difference between `auto` and `decltype(auto)` for return types?

> `auto` applies template type deduction — it strips references and top-level cv-qualifiers. `decltype(auto)` applies `decltype` rules — it preserves them.
>
> ```cpp
> int x = 5;
> int& getRef() { return x; }
>
> auto          f1() { return getRef(); }  // return type: int (ref stripped)
> decltype(auto) f2() { return getRef(); }  // return type: int& (ref kept)
>
> f1() = 99;  // ERROR — f1 returns a copy
> f2() = 99;  // OK — f2 returns a reference, modifies x
> ```
>
> Use `decltype(auto)` when writing generic wrappers (e.g. `call()`, `invoke()`) that must preserve the exact return type of the wrapped function.

---

**[Senior]** Explain why `decltype((x))` (double parentheses) gives a different type than `decltype(x)`.

> `decltype` has two rules:
> - If the operand is an **unparenthesized id-expression** (a plain variable name) → yields its **declared type**.
> - If the operand is any other expression → yields the value category's reference type:
>   - lvalue expression → `T&`
>   - xvalue expression → `T&&`
>   - prvalue expression → `T`
>
> `x` (a variable name) → unparenthesized id → `int` (declared type).
> `(x)` (parenthesized) → lvalue expression → `int&`.
>
> ```cpp
> int x = 5;
> decltype(x)   a = 0;  // int
> decltype((x)) b = x;  // int& — b is a reference to x
> ```
>
> This distinction matters in `decltype(auto)` return types:
> ```cpp
> decltype(auto) f() {
>     int x = 5;
>     return x;    // returns int (value)
>     return (x);  // returns int& — DANGLING reference to local!
> }
> ```

---

### Structured Bindings

---

**[Mid]** Can you use structured bindings with a custom struct? What are the requirements?

> Yes. For a simple aggregate (no private members, no base classes with members, no user-declared constructors), structured bindings work automatically:
>
> ```cpp
> struct RGB { uint8_t r, g, b; };
> RGB color{255, 128, 0};
> auto [red, green, blue] = color;
> ```
>
> For non-aggregates (classes with private members), you must provide a tuple-like interface:
> 1. Specialize `std::tuple_size<T>`
> 2. Specialize `std::tuple_element<I, T>`
> 3. Provide `get<I>(t)` (as member or free function)
>
> ```cpp
> class Point {
>     int x_, y_;
> public:
>     Point(int x, int y) : x_(x), y_(y) {}
>     template<std::size_t I> auto get() const {
>         if constexpr (I == 0) return x_;
>         else return y_;
>     }
> };
> template<> struct std::tuple_size<Point> : std::integral_constant<size_t, 2> {};
> template<size_t I> struct std::tuple_element<I, Point> { using type = int; };
> ```

---

### `std::optional`, `std::variant`, `std::any`

---

**[Mid]** Why prefer `std::optional` over returning a pointer or using a sentinel value like `-1`?

> Three reasons:
> 1. **Semantics are explicit in the type**: the signature `optional<int> find()` documents "may not return a value" — a plain `int` with a `-1` sentinel requires reading documentation.
> 2. **No heap allocation** (unlike returning a pointer to a heap-allocated value), and no ownership ambiguity.
> 3. **Compiler-enforced check**: `optional` forces the caller to handle the empty case to safely access the value (`value()` throws, `*` is UB if empty). Sentinel values are easy to forget to check.
>
> ```cpp
> int findBad(int id)                 { return found ? val : -1; }  // caller may ignore -1
> std::optional<int> findGood(int id) { return found ? val : std::nullopt; }  // explicit
> ```

---

**[Mid]** When would you choose `std::variant` over inheritance/virtual dispatch?

> `variant` gives **value semantics** (stack-allocated, copyable, no heap) while virtual dispatch requires heap allocation and pointer/reference indirection.
>
> Prefer `variant` when:
> - The set of types is **closed and known at compile time**
> - You want value semantics (copy, move, store in containers without pointers)
> - Performance matters (no vtable lookup, better cache behavior)
> - You want exhaustive pattern matching via `std::visit` (compiler warns on missing cases)
>
> Prefer virtual dispatch when:
> - Types are **open** — users can add new derived classes
> - Shared ownership/polymorphism across module boundaries
>
> ```cpp
> // variant — no heap, exhaustive visit
> using Shape = std::variant<Circle, Square, Triangle>;
> double area(Shape s) {
>     return std::visit([](auto& s) { return s.area(); }, s);
> }
> ```

---

**[Senior]** Implement a `Result<T, E>` type (like Rust's `Result`) using `std::variant`. Support `map()` to transform the success value.

> ```cpp
> template<typename T, typename E>
> class Result {
> public:
>     static Result ok(T value)    { return Result(std::in_place_index<0>, std::move(value)); }
>     static Result err(E error)   { return Result(std::in_place_index<1>, std::move(error)); }
>
>     bool has_value() const { return data_.index() == 0; }
>     bool has_error() const { return data_.index() == 1; }
>
>     T&       value() &  { return std::get<0>(data_); }
>     const T& value() const& { return std::get<0>(data_); }
>     E&       error() &  { return std::get<1>(data_); }
>
>     // map: transform T → U if ok, pass error through
>     template<typename F>
>     auto map(F&& fn) -> Result<std::invoke_result_t<F, T>, E> {
>         using U = std::invoke_result_t<F, T>;
>         if (has_value()) return Result<U, E>::ok(fn(value()));
>         else             return Result<U, E>::err(error());
>     }
>
> private:
>     explicit Result(auto tag, auto&& v) : data_(tag, std::forward<decltype(v)>(v)) {}
>     std::variant<T, E> data_;
> };
>
> // Usage
> Result<int, std::string> parse(std::string s) {
>     try { return Result<int, std::string>::ok(std::stoi(s)); }
>     catch (...) { return Result<int, std::string>::err("not a number"); }
> }
>
> auto doubled = parse("21").map([](int x) { return x * 2; });
> ```

---

### Ranges & Views

---

**[Mid]** What does "lazy evaluation" mean in the context of `std::views`? Why does it matter?

> A view does not compute or store results — it wraps the source range and applies the transformation **on demand**, element by element, during iteration. No intermediate `vector` is created.
>
> ```cpp
> auto result = v
>     | std::views::filter([](int x) { return x > 0; })
>     | std::views::transform([](int x) { return x * 2; });
>
> // Nothing has happened yet — no elements processed
>
> for (int x : result) { ... }  // filter + transform applied per element here
> ```
>
> **Why it matters**:
> - **Memory**: processing a million elements with 3 transforms allocates zero intermediate vectors
> - **Short-circuit**: `views::take(5)` stops after 5 elements regardless of source size
> - **Composability**: chain any number of views with no penalty per intermediate step

---

**[Senior]** What are the ownership and lifetime rules for views? What is a dangling view?

> Views are **non-owning** — they hold references or iterators into the underlying range. If the underlying range is destroyed or modified while the view is alive, the view becomes invalid.
>
> ```cpp
> auto get_view() {
>     std::vector<int> v = {1, 2, 3, 4, 5};
>     return v | std::views::filter([](int x){ return x % 2 == 0; });
>     // DANGLING — v is destroyed, view holds dangling iterators
> }
>
> // Safe: bind range to a variable first
> std::vector<int> v = {1, 2, 3, 4, 5};
> auto view = v | std::views::filter([](int x){ return x % 2 == 0; });
> ```
>
> Also: invalidating iterators (e.g. `push_back` causing reallocation) while a view is iterating causes undefined behavior — same rules as raw iterators.
>
> **Owning views**: `std::ranges::owning_view` (C++20) and `std::ranges::to<std::vector>()` (C++23) materialize a view into a container to break the lifetime dependency.

---

### Concepts

---

**[Mid]** What problem do concepts solve over `static_assert` or SFINAE?

> All three restrict which types can be used with a template, but differ in usability:
>
> | | Error message | Readability | Overload selection |
> |---|---|---|---|
> | Raw template | Instantiation noise | Poor | No |
> | `static_assert` | Custom message | OK | No — function still participates in overload |
> | SFINAE | Poor | Very poor | Yes |
> | Concepts | Clear constraint violation | Excellent | Yes |
>
> ```cpp
> // SFINAE — works but unreadable
> template<typename T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
> void f(T x);
>
> // Concepts — same semantics, readable
> template<std::integral T>
> void f(T x);
> ```
>
> Crucially, concepts also participate in **overload resolution and template specialization** — two overloads can differ by concept, and the more constrained one wins.

---

**[Senior]** Write a concept `Container` that requires a type to have `begin()`, `end()`, `size()`, and `value_type`. Then constrain a function `print_all` with it.

> ```cpp
> template<typename T>
> concept Container = requires(T c) {
>     { c.begin() } -> std::input_or_output_iterator;
>     { c.end()   } -> std::sentinel_for<decltype(c.begin())>;
>     { c.size()  } -> std::convertible_to<std::size_t>;
>     typename T::value_type;  // associated type must exist
> };
>
> // Constrained function
> void print_all(const Container auto& c) {
>     std::cout << "size=" << c.size() << ": ";
>     for (const auto& elem : c) {
>         std::cout << elem << " ";
>     }
>     std::cout << "\n";
> }
>
> // Usage
> print_all(std::vector<int>{1, 2, 3});        // OK
> print_all(std::list<std::string>{"a", "b"}); // OK
> print_all(42);  // error: int does not satisfy Container — clear message
> ```

---

### Coroutines

---

**[Mid]** What is the difference between a coroutine and a thread? When would you use one over the other?

> | | Coroutine | Thread |
> |---|---|---|
> | Scheduling | Cooperative (explicit `co_await`) | Preemptive (OS scheduler) |
> | Stack | Stackless (frame on heap) | Full stack (~1–8 MB) |
> | Overhead | Very low (~hundreds of bytes) | High (stack + OS resources) |
> | Parallelism | None by default (single thread) | True parallelism |
> | Synchronization | Not needed (single-threaded) | Mutexes, atomics needed |
>
> **Use coroutines** for:
> - High-volume async I/O (thousands of concurrent network requests)
> - Generators / lazy sequences
> - Avoiding callback hell — linear code that suspends instead of nesting callbacks
>
> **Use threads** for:
> - CPU-bound parallel work
> - True concurrency on multiple cores
>
> Coroutines and threads are complementary: a thread pool can *run* coroutines — a coroutine suspends on I/O, freeing the thread to run another coroutine.

---

**[Senior]** What is the coroutine promise type, and what customization points does it provide?

> Every coroutine's behavior is controlled by an associated **promise type** — the compiler looks it up via `std::coroutine_traits`. It defines:
>
> ```cpp
> struct MyPromise {
>     // 1. What object the caller gets back
>     MyTask get_return_object();
>
>     // 2. Should the coroutine suspend immediately on entry?
>     std::suspend_always  initial_suspend();  // lazy start
>     std::suspend_never   initial_suspend();  // eager start
>
>     // 3. Should it suspend before final cleanup?
>     std::suspend_always  final_suspend() noexcept;  // caller can retrieve result
>     std::suspend_never   final_suspend() noexcept;  // self-destruct on completion
>
>     // 4. Handle co_return value / void
>     void return_value(T val);
>     void return_void();
>
>     // 5. Handle co_yield
>     auto yield_value(T val);
>
>     // 6. Exception handling
>     void unhandled_exception();
>
>     // 7. Customize co_await behavior (optional)
>     auto await_transform(Awaitable a);
> };
> ```
>
> The promise type is the **policy class** for the coroutine — it determines scheduling, lifetime, value propagation, and exception behavior. Library authors implement this; application developers just write coroutine bodies using `co_await`/`co_yield`.

---

**[Senior]** What is the `co_await` expression evaluation sequence? Explain the three methods an awaitable must provide.

> When the compiler sees `co_await expr`:
> 1. Obtains an **awaiter** (either `expr` itself, or via `operator co_await`, or via `promise.await_transform`)
> 2. Calls `awaiter.await_ready()` — if `true`, skip suspension (optimization path)
> 3. If not ready, calls `awaiter.await_suspend(handle)` — suspends the coroutine and passes control (typically schedules `handle.resume()` on I/O completion)
> 4. When resumed, calls `awaiter.await_resume()` — returns the value of the `co_await` expression
>
> ```cpp
> struct MyAwaitable {
>     // Can we skip suspension? (optimization — avoids suspend overhead if already done)
>     bool await_ready() { return is_complete_; }
>
>     // Called when suspending — schedules the resume
>     void await_suspend(std::coroutine_handle<> handle) {
>         on_complete_ = [handle]() mutable { handle.resume(); };
>         start_async_op();
>     }
>
>     // Called on resume — provides the result value
>     int await_resume() { return result_; }
>
>     bool is_complete_ = false;
>     int  result_;
>     std::function<void()> on_complete_;
> };
> ```
>
> The three-method contract is what makes coroutines composable — any type that implements `await_ready`, `await_suspend`, `await_resume` is directly `co_await`-able.
