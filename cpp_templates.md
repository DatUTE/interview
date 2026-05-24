# C++ Templates & Metaprogramming

## Quick Reference

| Feature | Introduced | Purpose |
|---|---|---|
| Function / class templates | C++98 | Generic code |
| Partial specialization | C++98 | Customize for type families |
| SFINAE (`enable_if`) | C++11 | Conditional overloads |
| Variadic templates | C++11 | Arbitrary-arity generics |
| `type_traits` | C++11 | Compile-time type queries |
| Variable templates | C++14 | `pi<double>`, `is_void_v<T>` |
| Fold expressions | C++17 | Collapse parameter packs |
| `if constexpr` | C++17 | Compile-time branching |
| Concepts | C++20 | Readable constraints |
| Abbreviated function templates | C++20 | `auto` parameters |

---

## 1. Function Templates

```cpp
// Basic
template <typename T>
T max_val(T a, T b) { return a > b ? a : b; }

// Multiple type params
template <typename T, typename U>
auto add(T a, U b) -> decltype(a + b) { return a + b; }

// C++14: return type deduction
template <typename T, typename U>
auto add14(T a, U b) { return a + b; }

// Non-type template param
template <int N>
constexpr int factorial() {
    if constexpr (N <= 1) return 1;
    else return N * factorial<N - 1>();
}
static_assert(factorial<5>() == 120);

// Template template param
template <template <typename> class Container, typename T>
void fill(Container<T>& c, T val) {
    for (auto& x : c) x = val;
}
```

### Template Argument Deduction (TAD)

```cpp
template <typename T>
void foo(T x);          // x is T by value        — const/& stripped
void foo(T& x);         // x is T by lvalue ref   — & preserved, const preserved
void foo(T&& x);        // forwarding reference   — perfect forwarding

// Explicit override
foo<int>(3.14);          // forces T = int, 3.14 converted

// Class Template Argument Deduction (CTAD) C++17
std::pair p{1, 2.0};    // deduces pair<int, double>
std::vector v{1, 2, 3}; // deduces vector<int>

// Deduction guides
template <typename T>
struct Wrapper { T val; };
Wrapper(const char*) -> Wrapper<std::string>; // guide
Wrapper w{"hello"};  // Wrapper<string>
```

---

## 2. Class Templates

```cpp
template <typename T, std::size_t N = 16>
class RingBuffer {
public:
    void push(T val) {
        buf_[head_ % N] = std::move(val);
        ++head_;
    }
    T pop() {
        return std::move(buf_[tail_++ % N]);
    }
    bool empty() const { return head_ == tail_; }
    std::size_t size() const { return head_ - tail_; }

private:
    std::array<T, N> buf_{};
    std::size_t head_{0}, tail_{0};
};

RingBuffer<int, 32> rb;
rb.push(42);
```

### Member Function Templates

```cpp
template <typename T>
class Container {
public:
    template <typename U>
    void assign(const Container<U>& other); // cross-type assignment

    // template specialization of member — must be outside class
};
```

---

## 3. Template Specialization

### Full Specialization

```cpp
template <typename T>
struct TypeName { static const char* name() { return "unknown"; } };

template <>
struct TypeName<int>    { static const char* name() { return "int"; } };
template <>
struct TypeName<double> { static const char* name() { return "double"; } };

std::cout << TypeName<int>::name(); // "int"
```

### Partial Specialization (class templates only)

```cpp
// Primary
template <typename T, typename U>
struct Pair { /* ... */ };

// Partial: both same type
template <typename T>
struct Pair<T, T> { /* optimized */ };

// Partial: second is pointer
template <typename T, typename U>
struct Pair<T, U*> { /* pointer version */ };

// Partial: reverse order
template <typename T>
struct Pair<bool, T> { /* bool-first version */ };
```

### Template Specialization for std::hash

```cpp
struct Point { int x, y; };

template <>
struct std::hash<Point> {
    std::size_t operator()(const Point& p) const noexcept {
        std::size_t h1 = std::hash<int>{}(p.x);
        std::size_t h2 = std::hash<int>{}(p.y);
        return h1 ^ (h2 << 1);
    }
};

std::unordered_map<Point, int> m; // now works
```

---

## 4. SFINAE (Substitution Failure Is Not An Error)

When template substitution fails, the overload is silently removed — no compile error.

```
try instantiation
      |
      v
substitution fails? ──yes──> remove from overload set (no error)
      |
      no
      v
keep overload
```

### `std::enable_if`

```cpp
// Pre-C++20 — enable only for integral types
template <typename T>
typename std::enable_if<std::is_integral<T>::value, T>::type
next_even(T n) {
    return (n % 2 == 0) ? n : n + 1;
}

// C++14 shorthand
template <typename T>
std::enable_if_t<std::is_integral_v<T>, T>
next_even(T n) { return (n % 2 == 0) ? n : n + 1; }

// In template param (non-type bool)
template <typename T, std::enable_if_t<std::is_floating_point_v<T>, int> = 0>
T round_to(T val, int decimals) { /* ... */ }
```

### `std::void_t` — detect member existence

```cpp
// Detect ::value_type
template <typename T, typename = void>
struct has_value_type : std::false_type {};

template <typename T>
struct has_value_type<T, std::void_t<typename T::value_type>> : std::true_type {};

static_assert(has_value_type<std::vector<int>>::value);
static_assert(!has_value_type<int>::value);

// Detect callable
template <typename F, typename Arg, typename = void>
struct is_callable_with : std::false_type {};

template <typename F, typename Arg>
struct is_callable_with<F, Arg,
    std::void_t<decltype(std::declval<F>()(std::declval<Arg>()))>>
    : std::true_type {};
```

### Detection Idiom (C++17)

```cpp
template <typename T>
using serialize_t = decltype(std::declval<T>().serialize());

template <typename T>
constexpr bool has_serialize = std::is_detected_v<serialize_t, T>;
// (std::experimental::is_detected or roll your own)
```

---

## 5. C++20 Concepts

Concepts are named predicates checked at compile time — better errors than SFINAE.

```cpp
// Define a concept
template <typename T>
concept Integral = std::is_integral_v<T>;

template <typename T>
concept Addable = requires(T a, T b) {
    { a + b } -> std::convertible_to<T>;
};

template <typename T>
concept Printable = requires(T x, std::ostream& os) {
    { os << x } -> std::same_as<std::ostream&>;
};

// Compound concept
template <typename T>
concept Number = std::is_arithmetic_v<T>;

// Use concept to constrain
template <Integral T>
T gcd(T a, T b) { return b == 0 ? a : gcd(b, a % b); }

// requires clause
template <typename T>
    requires Addable<T> && Printable<T>
void process(T val) { std::cout << val + val; }

// Abbreviated function template (C++20)
void process2(Printable auto val) { std::cout << val; }
auto add(Number auto a, Number auto b) { return a + b; }

// requires expression inline
template <typename T>
void serialize(T& obj)
    requires requires(T x) { x.to_json(); }
{
    obj.to_json();
}
```

### Standard Library Concepts (C++20)

```cpp
// <concepts>
std::same_as<T, U>
std::derived_from<Derived, Base>
std::convertible_to<From, To>
std::common_with<T, U>
std::integral<T>
std::signed_integral<T>
std::floating_point<T>
std::copyable<T>
std::movable<T>
std::default_initializable<T>

// <iterator>
std::input_iterator<I>
std::output_iterator<I, T>
std::forward_iterator<I>
std::bidirectional_iterator<I>
std::random_access_iterator<I>

// <ranges>
std::ranges::range<R>
std::ranges::sized_range<R>
std::ranges::contiguous_range<R>
```

---

## 6. Variadic Templates

### Parameter Packs

```cpp
// sizeof... operator
template <typename... Ts>
constexpr std::size_t count() { return sizeof...(Ts); }
static_assert(count<int, double, char>() == 3);

// Recursive unpacking (C++11/14)
template <typename T>
void print(T last) { std::cout << last << '\n'; }

template <typename T, typename... Rest>
void print(T first, Rest... rest) {
    std::cout << first << ' ';
    print(rest...);
}

print(1, 2.5, "hello"); // 1 2.5 hello
```

### Fold Expressions (C++17)

```cpp
// Unary right fold: (pack op ...)
template <typename... Ts>
auto sum(Ts... vals) { return (vals + ...); }      // ((v1+v2)+v3)

// Unary left fold: (... op pack)
template <typename... Ts>
auto sum_l(Ts... vals) { return (... + vals); }    // (v1+(v2+v3))

// Binary right fold: (pack op ... op init)
template <typename... Ts>
void print(Ts... vals) { ((std::cout << vals << ' '), ...); }

// Logical fold
template <typename... Ts>
bool all_positive(Ts... vals) { return ((vals > 0) && ...); }

// Apply to each
template <typename F, typename... Args>
void for_each(F f, Args&&... args) { (f(std::forward<Args>(args)), ...); }
```

### Perfect Forwarding

```cpp
template <typename T>
void wrapper(T&& arg) {
    inner(std::forward<T>(arg)); // T&&: forwarding reference, not rvalue ref
}

// Forwarding pack
template <typename... Args>
void make_and_call(Args&&... args) {
    auto obj = MyClass(std::forward<Args>(args)...);
    obj.run();
}

// emplace_back internally:
template <typename... Args>
void emplace_back(Args&&... args) {
    new (end_ptr) T(std::forward<Args>(args)...);
}
```

### Tuple implementation sketch

```cpp
template <typename... Ts>
struct Tuple {};

template <typename Head, typename... Tail>
struct Tuple<Head, Tail...> : Tuple<Tail...> {
    Head head;
    Tuple(Head h, Tail... t) : Tuple<Tail...>(t...), head(h) {}
};

// Access via index
template <std::size_t I, typename Head, typename... Tail>
auto& get(Tuple<Head, Tail...>& t) {
    if constexpr (I == 0) return t.head;
    else return get<I-1>(static_cast<Tuple<Tail...>&>(t));
}
```

---

## 7. Type Traits

### Common Traits (`<type_traits>`)

```cpp
// Primary type categories
std::is_void_v<T>
std::is_integral_v<T>
std::is_floating_point_v<T>
std::is_array_v<T>
std::is_pointer_v<T>
std::is_reference_v<T>
std::is_function_v<T>
std::is_class_v<T>
std::is_enum_v<T>

// Composite categories
std::is_arithmetic_v<T>      // integral || floating_point
std::is_fundamental_v<T>     // arithmetic || void || nullptr_t
std::is_compound_v<T>        // array, pointer, reference, function, class, union, enum

// Type properties
std::is_const_v<T>
std::is_volatile_v<T>
std::is_trivial_v<T>
std::is_pod_v<T>             // deprecated C++20
std::is_standard_layout_v<T>
std::is_empty_v<T>           // no non-static data members
std::is_abstract_v<T>
std::is_final_v<T>

// Constructor/assignment properties
std::is_default_constructible_v<T>
std::is_copy_constructible_v<T>
std::is_move_constructible_v<T>
std::is_copy_assignable_v<T>
std::is_move_assignable_v<T>
std::is_destructible_v<T>
std::is_trivially_copyable_v<T>
std::is_nothrow_move_constructible_v<T>

// Relationships
std::is_same_v<T, U>
std::is_base_of_v<Base, Derived>
std::is_convertible_v<From, To>

// Transformations
std::remove_cv_t<T>          // remove const/volatile
std::remove_reference_t<T>   // remove & or &&
std::remove_cvref_t<T>       // C++20: remove both
std::add_pointer_t<T>
std::add_lvalue_reference_t<T>
std::make_signed_t<T>
std::make_unsigned_t<T>
std::decay_t<T>              // array→pointer, function→pointer, remove cv&
std::common_type_t<T, U>     // common type for arithmetic
std::underlying_type_t<Enum>

// Conditional
std::conditional_t<condition, TrueType, FalseType>
std::enable_if_t<condition, T = void>

// Queries
std::alignment_of_v<T>       // = alignof(T)
std::rank_v<T>               // array dimensions
std::extent_v<T, N>          // array dimension size
```

### Writing Custom Traits

```cpp
// Trait to detect if T has .size() method
template <typename T, typename = void>
struct has_size : std::false_type {};

template <typename T>
struct has_size<T, std::void_t<decltype(std::declval<T>().size())>>
    : std::true_type {};

// Helper variable template
template <typename T>
inline constexpr bool has_size_v = has_size<T>::value;

// Usage
template <typename Container>
std::size_t safe_size(const Container& c) {
    if constexpr (has_size_v<Container>)
        return c.size();
    else
        return 0;
}
```

---

## 8. `if constexpr`

Discards the branch at compile time — even if the branch wouldn't compile for this type.

```cpp
template <typename T>
std::string to_string(T val) {
    if constexpr (std::is_same_v<T, std::string>)
        return val;
    else if constexpr (std::is_arithmetic_v<T>)
        return std::to_string(val);
    else if constexpr (has_size_v<T>)
        return "[container of size " + std::to_string(val.size()) + "]";
    else
        return "unknown";
}

// Without if constexpr this would fail to compile for non-arithmetic T
// The discarded branch is not instantiated at all
```

---

## 9. Template Metaprogramming (TMP)

Computation at compile time using template recursion + specialization.

### Compile-time values

```cpp
// Fibonacci at compile time
template <int N>
struct Fib {
    static constexpr int value = Fib<N-1>::value + Fib<N-2>::value;
};
template <> struct Fib<0> { static constexpr int value = 0; };
template <> struct Fib<1> { static constexpr int value = 1; };

static_assert(Fib<10>::value == 55);

// Modern: constexpr function (preferred over TMP)
consteval int fib(int n) {
    return n <= 1 ? n : fib(n-1) + fib(n-2);
}
static_assert(fib(10) == 55);
```

### Type lists

```cpp
// Type list manipulation
template <typename... Ts>
struct TypeList {};

// Prepend
template <typename T, typename List>
struct Prepend;
template <typename T, typename... Ts>
struct Prepend<T, TypeList<Ts...>> { using type = TypeList<T, Ts...>; };

// Length
template <typename List> struct Length;
template <typename... Ts>
struct Length<TypeList<Ts...>> : std::integral_constant<std::size_t, sizeof...(Ts)> {};

// Contains
template <typename T, typename List> struct Contains : std::false_type {};
template <typename T, typename Head, typename... Tail>
struct Contains<T, TypeList<Head, Tail...>>
    : std::conditional_t<std::is_same_v<T, Head>,
                         std::true_type,
                         Contains<T, TypeList<Tail...>>> {};
```

### Policy-Based Design

```cpp
// Thread safety policy
struct SingleThreaded {
    struct Lock { Lock() = default; };  // no-op
};
struct MultiThreaded {
    using Lock = std::lock_guard<std::mutex>;
    mutable std::mutex mtx_;
};

template <typename ThreadingPolicy = SingleThreaded>
class Cache : private ThreadingPolicy {
public:
    void insert(int key, int val) {
        typename ThreadingPolicy::Lock lk(this->mtx_);
        data_[key] = val;
    }
private:
    std::unordered_map<int, int> data_;
};

Cache<>              single;   // zero overhead
Cache<MultiThreaded> multi;    // thread-safe
```

---

## 10. CRTP (Curiously Recurring Template Pattern)

```cpp
// Static polymorphism — no vtable, inlined at compile time
template <typename Derived>
struct Printable {
    void print() const {
        static_cast<const Derived*>(this)->print_impl();
    }
};

struct Point : Printable<Point> {
    int x, y;
    void print_impl() const {
        std::cout << "(" << x << ", " << y << ")\n";
    }
};

// Mixin / interface injection via CRTP
template <typename Derived>
struct Comparable {
    bool operator!=(const Derived& o) const {
        return !static_cast<const Derived&>(*this).operator==(o);
    }
    bool operator<=(const Derived& o) const {
        auto& d = static_cast<const Derived&>(*this);
        return d == o || d < o;
    }
    bool operator>(const Derived& o) const  { return o < static_cast<const Derived&>(*this); }
    bool operator>=(const Derived& o) const { return o <= static_cast<const Derived&>(*this); }
};

struct Version : Comparable<Version> {
    int major, minor;
    bool operator==(const Version& o) const { return major==o.major && minor==o.minor; }
    bool operator<(const Version&  o) const { return major<o.major || (major==o.major && minor<o.minor); }
};
// Gets !=, <=, >, >= for free
```

### CRTP vs Virtual

| | CRTP | Virtual |
|---|---|---|
| Dispatch | Compile-time | Runtime |
| vtable overhead | None | 1 pointer + indirection |
| Inlining | Yes | Rarely (devirtualization) |
| Heterogeneous container | No | Yes |
| Code bloat | More instantiations | Shared code |

---

## 11. Explicit Template Instantiation

```cpp
// header: MyTemplate.h
template <typename T>
class MyTemplate {
public:
    void compute(T val);
};

// source: MyTemplate.cpp
#include "MyTemplate.h"
template <typename T>
void MyTemplate<T>::compute(T val) { /* ... */ }

// Explicit instantiation — generates object code for int/double only
template class MyTemplate<int>;
template class MyTemplate<double>;

// In other .cpp — suppress re-instantiation (extern)
extern template class MyTemplate<int>;
extern template class MyTemplate<double>;
```

**Why**: reduces compile time and binary size in large codebases.

---

## 12. Common Patterns Summary

```cpp
// Tag dispatch
struct slow_tag {};
struct fast_tag {};

template <typename T>
void process(T val, slow_tag) { /* O(n²) */ }
template <typename T>
void process(T val, fast_tag) { /* O(n log n) */ }

template <typename T>
void process(T val) {
    using tag = std::conditional_t<is_small_v<T>, fast_tag, slow_tag>;
    process(val, tag{});
}

// SFINAE vs Concepts vs if constexpr — when to use
// SFINAE       → overload resolution (pre-C++20 libraries, ABI compat)
// Concepts     → readable constraints, better errors (C++20+)
// if constexpr → single function body with type-driven branches
```

---

## Interview Q&As

**Q [Mid]: What is SFINAE?**

"Substitution Failure Is Not An Error" — when the compiler tries to substitute template arguments and a substitution fails (e.g., a type doesn't have the required member), it simply removes that overload from the candidate set without a compile error. This lets you conditionally enable/disable templates based on type properties. A failed substitution becomes an error only if no valid overload remains.

---

**Q [Mid]: What's the difference between `typename` and `class` in a template parameter?**

Semantically identical for type parameters. `typename` is preferred for clarity since `class` implies "must be a class type" but it doesn't. Use `typename` inside templates when accessing a dependent type: `typename T::value_type` (tells the compiler it's a type, not a static member).

---

**Q [Senior]: How do fold expressions differ from recursive variadic templates?**

Recursive unpacking requires `N+1` template instantiations (one per recursion level + base case), each potentially generating separate code. Fold expressions expand the entire pack in a single instantiation with a compiler-generated loop — fewer instantiations, simpler code, same result. Example: `(vals + ...)` expands to `v1 + v2 + v3` in one shot vs three recursive calls.

---

**Q [Senior]: What is the difference between `std::decay_t<T>` and `std::remove_cvref_t<T>`?**

`decay_t` does: remove reference, remove cv-qualifiers, AND apply array-to-pointer and function-to-pointer conversions (mimicking pass-by-value decay). `remove_cvref_t` (C++20) only strips references and cv-qualifiers — no array/function conversion. Use `remove_cvref_t` when you want the base type without pointer decay, e.g., checking `is_same_v<remove_cvref_t<T>, int>` for both `const int&` and `int&&`.

---

**Q [Senior]: Explain CRTP vs virtual dispatch trade-offs in a real scenario.**

A numeric vector library benefits from CRTP: `Vector3<float>`, `Vector4<double>` — all operations known at compile time, fully inlined, no vtable. A plugin system benefits from virtual: plugins loaded at runtime as shared libraries can't be instantiated at compile time, so dynamic dispatch is required. CRTP can't handle heterogeneous collections (`std::vector<Shape*>`) — that needs virtual. The key insight: CRTP is resolved at compile time so it can't support runtime polymorphism, but it has zero overhead and enables inlining.

---

**Q [Senior]: How would you implement `std::tuple` using variadic templates?**

Using recursive inheritance: `Tuple<Head, Tail...>` inherits from `Tuple<Tail...>`, storing `Head` as a member. The base case `Tuple<>` is empty. Access via `get<I>()` uses `if constexpr` or recursive casting to the correct base. The `sizeof...(Ts)` gives tuple size. C++17 fold expressions can simplify construction. Real `std::tuple` uses an index-based approach for EBO (Empty Base Optimization) and to handle multiple same-type members.
