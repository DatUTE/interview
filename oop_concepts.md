# Object-Oriented Programming (OOP) in C++

---

## What is OOP?

**Object-Oriented Programming** is a programming paradigm that organises code around **objects** — self-contained units that combine **data** (state) and **behaviour** (functions that operate on that state) — rather than around functions and logic alone.

The central idea: model your problem as a set of interacting objects that mirror real-world or domain entities. Each object is responsible for managing its own state and exposing a well-defined interface to the outside world.

```
Procedural (C style):          OOP (C++ style):
──────────────────────         ──────────────────────
Data: plain structs            Data: private members inside a class
Functions: free functions      Behaviour: methods that own and guard the data
No invariant enforcement       Class enforces invariants in every operation
Caller manages everything      Object manages its own lifetime and state
```

### OOP vs Other Paradigms

| Paradigm | Core unit | Focus | Examples |
|---|---|---|---|
| **Procedural** | Function | Step-by-step instructions | C, Pascal |
| **Object-Oriented** | Object | Entities with state + behaviour | C++, Java, Python |
| **Functional** | Function (pure) | Transformations, immutability | Haskell, Erlang |
| **Generic** | Template / type parameter | Algorithms independent of type | STL, Rust traits |

> C++ is **multi-paradigm** — it supports OOP, procedural, generic, and functional styles. You choose the right tool per problem.

### Why OOP?

| Goal | How OOP achieves it |
|---|---|
| **Manage complexity** | Break large systems into self-contained objects |
| **Code reuse** | Inheritance and composition share behaviour without duplication |
| **Flexibility** | Polymorphism lets new types plug into existing code |
| **Safety** | Encapsulation prevents accidental corruption of state |
| **Maintainability** | Single Responsibility means changing one object doesn't break others |

### One-line interview answer

> *"OOP is a paradigm that models software as a collection of objects — each combining state and behaviour — and uses encapsulation, abstraction, inheritance, and polymorphism to manage complexity, promote reuse, and keep systems flexible and maintainable."*

---

## The Four Pillars

| Pillar | Core idea | C++ mechanism |
|---|---|---|
| **Encapsulation** | Bundle data + behaviour; hide internal details | `private`/`protected`/`public`, classes |
| **Abstraction** | Expose only what is necessary; hide complexity | Abstract classes, pure virtual, interfaces |
| **Inheritance** | Reuse and extend existing types | `class Derived : Base`, virtual functions |
| **Polymorphism** | Same interface, different behaviour at runtime | Virtual dispatch, overriding, templates |

---

## 1. Classes and Objects

### Class Anatomy

```cpp
class BankAccount {
public:
    // Constructor
    BankAccount(std::string owner, double balance)
        : owner_(std::move(owner)), balance_(balance) {}

    // Public interface (abstraction layer)
    void    deposit(double amount);
    bool    withdraw(double amount);
    double  balance() const { return balance_; }
    const std::string& owner() const { return owner_; }

protected:
    // Accessible to derived classes
    void set_balance(double b) { balance_ = b; }

private:
    // Hidden implementation detail (encapsulation)
    std::string owner_;
    double      balance_;

    bool validate_amount(double amount) const { return amount > 0; }
};

// Definition outside class body
void BankAccount::deposit(double amount) {
    if (validate_amount(amount))
        balance_ += amount;
}

bool BankAccount::withdraw(double amount) {
    if (!validate_amount(amount) || amount > balance_)
        return false;
    balance_ -= amount;
    return true;
}
```

### Access Specifiers

```cpp
class Foo {
public:      // accessible by anyone
    int pub;

protected:   // accessible by Foo and its derived classes
    int prot;

private:     // accessible only by Foo (and friends)
    int priv;
};

struct Bar {
    int x;   // struct defaults to public
};

class Baz {
    int x;   // class defaults to private
};
```

### `struct` vs `class`

The only difference in C++ is the **default access**:
- `struct`: members and base classes default to `public`
- `class`: members and base classes default to `private`

Convention: use `struct` for passive data aggregates (POD-like), `class` for types with invariants and behaviour.

---

## 2. Encapsulation

Encapsulation **bundles state with the operations that govern it** and restricts direct external access. This allows the class to enforce invariants — internal consistency rules that must always hold.

```cpp
class Temperature {
public:
    // Invariant: temperature cannot be below absolute zero (-273.15°C)
    explicit Temperature(double celsius) {
        set(celsius);   // use setter to enforce invariant in constructor too
    }

    void set(double celsius) {
        if (celsius < -273.15)
            throw std::domain_error("below absolute zero");
        celsius_ = celsius;
    }

    double celsius()    const { return celsius_; }
    double fahrenheit() const { return celsius_ * 9.0 / 5.0 + 32.0; }
    double kelvin()     const { return celsius_ + 273.15; }

private:
    double celsius_;    // direct access bypasses invariant check — must be private
};

Temperature t(100.0);
t.celsius_ = -300;  // compile error — private. Invariant preserved.
t.set(-300);        // runtime exception. Invariant preserved.
```

### `friend` — Controlled Encapsulation Break

```cpp
class Matrix;

class Vector {
    double data_[3];
    friend class Matrix;           // Matrix can access Vector's private members
    friend Vector operator*(double scalar, const Vector& v);  // free function
};

// friend free function — needed when left operand is not of class type
Vector operator*(double scalar, const Vector& v) {
    return Vector{scalar * v.data_[0],   // accesses private data_
                  scalar * v.data_[1],
                  scalar * v.data_[2]};
}
```

### PIMPL Idiom (Pointer to IMPLementation)

Hides implementation details from the header — reduces compile-time coupling and binary compatibility issues.

```cpp
// widget.h — public header (stable ABI)
#include <memory>
class Widget {
public:
    Widget();
    ~Widget();
    void render();

private:
    struct Impl;                    // forward declaration — no details leaked
    std::unique_ptr<Impl> pimpl_;   // pointer to hidden implementation
};

// widget.cpp — implementation (can change freely without breaking ABI)
#include "widget.h"
#include "heavy_graphics_lib.h"   // only included here, not in user's TU

struct Widget::Impl {
    HeavyGraphicsContext ctx;
    std::vector<Vertex>  vertices;
    void render_internal() { /* ... */ }
};

Widget::Widget()  : pimpl_(std::make_unique<Impl>()) {}
Widget::~Widget() = default;   // must be in .cpp — Impl must be complete here
void Widget::render() { pimpl_->render_internal(); }
```

---

## 3. Abstraction

Abstraction means exposing **what** an object does without revealing **how** it does it. In C++, this is achieved through abstract base classes and pure virtual functions.

```cpp
// Abstract interface — defines the "what"
class Shape {
public:
    virtual double area()      const = 0;   // pure virtual — no implementation here
    virtual double perimeter() const = 0;
    virtual void   draw()      const = 0;
    virtual ~Shape() = default;             // always virtual dtor in abstract base
};

// Concrete class — defines the "how"
class Circle : public Shape {
public:
    explicit Circle(double radius) : r_(radius) {}
    double area()      const override { return M_PI * r_ * r_; }
    double perimeter() const override { return 2 * M_PI * r_; }
    void   draw()      const override { /* render circle */ }
private:
    double r_;
};

class Rectangle : public Shape {
public:
    Rectangle(double w, double h) : w_(w), h_(h) {}
    double area()      const override { return w_ * h_; }
    double perimeter() const override { return 2 * (w_ + h_); }
    void   draw()      const override { /* render rectangle */ }
private:
    double w_, h_;
};

// Client code only knows about Shape — abstracted from the concrete type
void print_info(const Shape& s) {
    std::cout << "area=" << s.area()
              << " perimeter=" << s.perimeter() << "\n";
}
```

---

## 4. Inheritance

### Syntax and Access Rules

```cpp
class Animal {
public:
    std::string name;
    void breathe() { puts("breathing"); }
protected:
    int age_;
private:
    std::string dna_;  // never accessible in derived class
};

class Dog : public Animal {           // public inheritance (is-a)
    // Animal::name      → public    in Dog
    // Animal::breathe() → public    in Dog
    // Animal::age_      → protected in Dog
    // Animal::dna_      → inaccessible
};

class Secret : private Animal {       // private inheritance (implemented-in-terms-of)
    // Animal::name      → private in Secret
    // Animal::breathe() → private in Secret
    // External code: Secret s; s.breathe();  ← compile error
};

class Controlled : protected Animal { // protected inheritance (rare)
    // Animal::name      → protected in Controlled
};
```

### Constructor/Destructor Chaining

```cpp
class Base {
public:
    Base(int x) : x_(x) { std::cout << "Base(" << x << ")\n"; }
    virtual ~Base()      { std::cout << "~Base\n"; }
protected:
    int x_;
};

class Derived : public Base {
public:
    Derived(int x, int y)
        : Base(x)       // MUST call base constructor explicitly if no default ctor
        , y_(y)
    { std::cout << "Derived(" << x << "," << y << ")\n"; }

    ~Derived() override { std::cout << "~Derived\n"; }
private:
    int y_;
};

// Construction order:  Base → Derived  (most base first)
// Destruction order:   Derived → Base  (most derived first)
Derived d(1, 2);
// Output:
// Base(1)
// Derived(1,2)
// ~Derived
// ~Base
```

### Inherited Member Hiding vs Overriding

```cpp
class Base {
public:
    virtual void foo(int x) { puts("Base::foo(int)"); }
    void bar() { puts("Base::bar"); }    // non-virtual
};

class Derived : public Base {
public:
    // OVERRIDE — replaces Base::foo through virtual dispatch
    void foo(int x) override { puts("Derived::foo(int)"); }

    // HIDE — shadows Base::bar, but only when called on Derived type
    void bar() { puts("Derived::bar"); }
};

Base* b = new Derived();
b->foo(1);  // "Derived::foo(int)" — virtual dispatch picks Derived's version
b->bar();   // "Base::bar"         — non-virtual, static dispatch on Base*

Derived* d = new Derived();
d->bar();   // "Derived::bar"      — static dispatch on Derived*
```

### `override` and `final`

```cpp
class Base {
    virtual void foo(int) {}
    virtual void bar() {}
};

class Derived : public Base {
    void foo(int)  override {}  // ✅ compiler verifies this overrides a base virtual
    void foo(long) override {}  // ❌ compile error — no Base::foo(long) to override

    void bar() final {}  // override + prevent further overriding in sub-subclasses
};

class LeafClass final : public Derived {
    // void bar() override {} // ❌ compile error — Derived::bar() is final
};
// class Further : public LeafClass {}; // ❌ compile error — LeafClass is final
```

---

## 5. Polymorphism

### Compile-Time Polymorphism (Static)

Resolved at compile time — zero runtime overhead.

```cpp
// Function overloading
void print(int x)         { std::cout << "int: "    << x << "\n"; }
void print(double x)      { std::cout << "double: " << x << "\n"; }
void print(std::string x) { std::cout << "string: " << x << "\n"; }

print(42);       // "int: 42"
print(3.14);     // "double: 3.14"
print("hello");  // "string: hello"

// Templates (generic polymorphism)
template<typename T>
T max_val(T a, T b) { return a > b ? a : b; }

max_val(3, 5);          // instantiated as max_val<int>
max_val(3.14, 2.71);    // instantiated as max_val<double>

// Operator overloading
class Vec2 {
public:
    double x, y;
    Vec2 operator+(const Vec2& o) const { return {x+o.x, y+o.y}; }
    Vec2 operator*(double s)      const { return {x*s, y*s}; }
    bool operator==(const Vec2& o) const { return x==o.x && y==o.y; }
};
```

### Runtime Polymorphism (Dynamic)

Resolved at runtime through virtual dispatch. Enables writing code that works on any derived type.

```cpp
class Logger {
public:
    virtual void log(const std::string& msg) = 0;
    virtual ~Logger() = default;
};

class ConsoleLogger : public Logger {
public:
    void log(const std::string& msg) override {
        std::cout << "[CONSOLE] " << msg << "\n";
    }
};

class FileLogger : public Logger {
    std::ofstream file_;
public:
    explicit FileLogger(const std::string& path) : file_(path) {}
    void log(const std::string& msg) override {
        file_ << "[FILE] " << msg << "\n";
    }
};

class NullLogger : public Logger {
public:
    void log(const std::string&) override {}  // discard — useful for tests
};

// Client code is independent of which Logger is used
void process(Logger& log) {
    log.log("starting process");
    do_work();
    log.log("process complete");
}

// Choose implementation at runtime
std::unique_ptr<Logger> make_logger(const std::string& type) {
    if (type == "file")    return std::make_unique<FileLogger>("app.log");
    if (type == "null")    return std::make_unique<NullLogger>();
    return std::make_unique<ConsoleLogger>();
}
```

---

## 6. Virtual Dispatch — The vtable Mechanism

Every class with at least one virtual function gets a **vtable** (virtual function table) — a static array of function pointers. Every instance gets a hidden **vptr** (virtual pointer) as its first member, pointing to the class's vtable.

```
Memory layout of a Derived instance:

┌──────────────────────────────────┐
│  vptr → Derived's vtable         │  ← hidden pointer (8 bytes on 64-bit)
│  Base member: x_                 │
│  Derived member: y_              │
└──────────────────────────────────┘

Derived's vtable (read-only, in .rodata):
┌──────────────────────────────────┐
│  [0] → &Derived::foo             │  ← overridden
│  [1] → &Base::bar                │  ← inherited (not overridden)
│  [2] → &Derived::~Derived        │  ← virtual destructor
└──────────────────────────────────┘
```

```cpp
// What the compiler generates for: base_ptr->foo()
// 1. Load vptr from *base_ptr           (one memory read)
// 2. Load function pointer from vtable[0] (one memory read)
// 3. Call the function pointer           (indirect call)

// Assembly (simplified x86-64):
// mov rax, [rdi]        ; load vptr
// call [rax + offset]   ; call through vtable slot
```

### Cost of Virtual Dispatch

| | Direct call | Virtual call |
|---|---|---|
| **Instruction** | `call 0x4012ab` (compile-time address) | `call [rax + N]` (runtime load + indirect call) |
| **Inlineable** | Yes (if visible) | No (generally) |
| **Branch prediction** | Perfectly predictable | Potentially mispredicted |
| **Cache** | I-cache warm after first call | May cause I-cache miss + D-cache miss for vptr |

Virtual call overhead is **~1–3 ns** in a cache-warm scenario. The real cost is **missed inlining** — the compiler cannot see through the virtual call to optimize the callee.

### vptr Initialization Order

```cpp
struct Base {
    Base()  { foo(); }    // vptr = &Base's vtable during Base()
    virtual void foo() { puts("Base::foo"); }
};

struct Derived : Base {
    Derived() { foo(); }  // vptr = &Derived's vtable during Derived()
    void foo() override { puts("Derived::foo"); }
};

Derived d;
// Output:
// Base::foo    ← Base ctor: vptr points to Base's vtable → calls Base::foo
// Derived::foo ← Derived ctor: vptr updated → calls Derived::foo
```

**Never call virtual functions in constructors or destructors** — the derived class is not yet (or no longer) fully constructed.

---

## 7. Special Member Functions — Rule of 0 / 3 / 5

C++ automatically generates six special member functions if you don't declare them:

```cpp
class Foo {
    Foo();                              // default constructor
    ~Foo();                             // destructor
    Foo(const Foo&);                    // copy constructor
    Foo& operator=(const Foo&);         // copy assignment
    Foo(Foo&&) noexcept;                // move constructor      (C++11)
    Foo& operator=(Foo&&) noexcept;     // move assignment       (C++11)
};
```

### Rule of Zero

If your class only holds members that manage their own resources (smart pointers, `std::string`, `std::vector`), **declare none** of the special members — the compiler-generated versions do the right thing.

```cpp
class Person {
    std::string name_;     // manages its own copy/move/destroy
    int         age_;
    // No special members declared — compiler generates correct versions
};
```

### Rule of Five

If you **must** declare a destructor, copy constructor, or copy assignment, you almost certainly need to declare all five — the compiler's implicit generation of move operations is suppressed when you declare a destructor.

```cpp
class Buffer {
public:
    explicit Buffer(size_t size)
        : data_(new char[size]), size_(size) {}

    // Destructor — must free the resource
    ~Buffer() { delete[] data_; }

    // Copy constructor — deep copy
    Buffer(const Buffer& other)
        : data_(new char[other.size_]), size_(other.size_) {
        std::memcpy(data_, other.data_, size_);
    }

    // Copy assignment — deep copy with self-assignment check
    Buffer& operator=(const Buffer& other) {
        if (this == &other) return *this;
        delete[] data_;
        data_ = new char[other.size_];
        size_ = other.size_;
        std::memcpy(data_, other.data_, size_);
        return *this;
    }

    // Move constructor — steal the resource, leave source empty
    Buffer(Buffer&& other) noexcept
        : data_(other.data_), size_(other.size_) {
        other.data_ = nullptr;
        other.size_ = 0;
    }

    // Move assignment — steal resource
    Buffer& operator=(Buffer&& other) noexcept {
        if (this == &other) return *this;
        delete[] data_;
        data_ = other.data_;
        size_ = other.size_;
        other.data_ = nullptr;
        other.size_ = 0;
        return *this;
    }

private:
    char*  data_;
    size_t size_;
};
```

### Copy-and-Swap Idiom (Combines Copy + Move Assignment)

```cpp
class Buffer {
public:
    // ... constructors as above ...

    friend void swap(Buffer& a, Buffer& b) noexcept {
        using std::swap;
        swap(a.data_, b.data_);
        swap(a.size_, b.size_);
    }

    // One assignment operator handles both copy and move
    Buffer& operator=(Buffer other) noexcept {  // takes by value (copy or move)
        swap(*this, other);                      // exchange internals
        return *this;
        // 'other' (holding old data) is destroyed here → clean release
    }
};

Buffer a(64);
Buffer b = a;         // copy: copies into parameter → swap → clean
Buffer c = std::move(a);  // move: moves into parameter → swap → clean
```

### Controlling Generation: `= default` and `= delete`

```cpp
class NoCopy {
public:
    NoCopy() = default;
    NoCopy(const NoCopy&)            = delete;  // disallow copying
    NoCopy& operator=(const NoCopy&) = delete;
    NoCopy(NoCopy&&)                 = default; // allow moving
    NoCopy& operator=(NoCopy&&)      = default;
};

class ValueType {
public:
    ValueType()                               = default;
    ValueType(const ValueType&)               = default;
    ValueType& operator=(const ValueType&)    = default;
    ValueType(ValueType&&)                    = default;
    ValueType& operator=(ValueType&&)         = default;
    ~ValueType()                              = default;
    // All explicitly defaulted — documents intent, compiler still generates them
};
```

---

## 8. Multiple Inheritance & the Diamond Problem

### Multiple Inheritance

```cpp
class Flyable {
public:
    virtual void fly() { puts("flying"); }
    virtual ~Flyable() = default;
};

class Swimmable {
public:
    virtual void swim() { puts("swimming"); }
    virtual ~Swimmable() = default;
};

class Duck : public Flyable, public Swimmable {
public:
    void fly()  override { puts("duck flying"); }
    void swim() override { puts("duck swimming"); }
};

Duck d;
d.fly();   // "duck flying"
d.swim();  // "duck swimming"

Flyable*   f = &d;   // valid
Swimmable* s = &d;   // valid (different base subobject offset)
```

### The Diamond Problem

```cpp
struct Animal    { int age; };
struct Mammal    : Animal {};   // has its own Animal subobject
struct WingedAnimal : Animal {}; // has its own Animal subobject

struct Bat : Mammal, WingedAnimal {
    // Has TWO Animal subobjects — ambiguous!
};

Bat b;
b.age = 5;   // ❌ compile error: 'age' is ambiguous
             // Is it Mammal::Animal::age or WingedAnimal::Animal::age?

b.Mammal::age = 5;          // must qualify explicitly
b.WingedAnimal::age = 10;   // two separate 'age' members
```

### Virtual Inheritance — Diamond Solution

```cpp
struct Animal       { int age; };
struct Mammal       : virtual Animal {};   // virtual base
struct WingedAnimal : virtual Animal {};   // virtual base

struct Bat : Mammal, WingedAnimal {
    // Only ONE Animal subobject shared between Mammal and WingedAnimal
};

Bat b;
b.age = 5;   // ✅ unambiguous — exactly one Animal subobject
```

**Cost of virtual inheritance**:
- An extra indirection (vbase pointer) to locate the shared base subobject
- Non-trivial construction order: virtual bases are constructed **first** by the most-derived class, before any non-virtual bases
- Generally: avoid multiple inheritance and the diamond problem through better design (prefer composition or interfaces)

---

## 9. Abstract Classes & Interfaces

### Abstract Class

A class with **at least one pure virtual function** — cannot be instantiated directly.

```cpp
class Serializable {
public:
    virtual std::string serialize()   const = 0;
    virtual void        deserialize(const std::string& data) = 0;
    virtual ~Serializable() = default;
};

// Must implement ALL pure virtuals to be concrete
class Config : public Serializable {
    std::map<std::string, std::string> data_;
public:
    std::string serialize() const override {
        // convert data_ to JSON/YAML/...
    }
    void deserialize(const std::string& s) override {
        // parse s into data_
    }
};
```

### Interface Pattern (Pure Abstract Base)

C++ has no `interface` keyword. Convention: an interface is an abstract class with **only pure virtual functions** and a virtual destructor — no data members, no implementation.

```cpp
// Interface (pure abstract base class)
class IDrawable {
public:
    virtual void draw(Canvas&) const = 0;
    virtual ~IDrawable() = default;
};

class IClickable {
public:
    virtual void on_click(int x, int y) = 0;
    virtual ~IClickable() = default;
};

// Multiple interface implementation
class Button : public IDrawable, public IClickable {
public:
    void draw(Canvas& c)      const override { /* render button */ }
    void on_click(int x, int y)    override { /* handle click */ }
};
```

### Partial Implementation (Template Method Pattern)

```cpp
class DataProcessor {
public:
    // Template method — defines the algorithm skeleton
    void process(const Data& d) {
        validate(d);       // hook — must override
        transform(d);      // hook — must override
        log_result(d);     // common step — implemented here
    }

    virtual ~DataProcessor() = default;

protected:
    virtual void validate(const Data& d)  = 0;   // pure virtual hooks
    virtual void transform(const Data& d) = 0;

private:
    void log_result(const Data& d) {              // fixed step, not overridable
        std::cout << "processed: " << d.id() << "\n";
    }
};

class ImageProcessor : public DataProcessor {
protected:
    void validate(const Data& d)  override { /* check image format */ }
    void transform(const Data& d) override { /* apply filters */ }
};
```

---

## 10. Object Relationships

UML defines five relationships between classes, each with different coupling strength and ownership semantics:

```
Strength (weakest → strongest):
  Dependency → Association → Aggregation → Composition → Inheritance
```

### 1. Dependency (uses-a, temporary)

A class **uses** another only within a method — no stored reference.

```cpp
class Printer {
public:
    // Printer depends on Document only for this call
    void print(const Document& doc) {
        doc.render();   // use, then forget
    }
};
// Printer does NOT store a Document member
```

### 2. Association (knows-a, non-owning)

A class **holds a reference or pointer** to another but does **not own** its lifetime.

```cpp
class Teacher {
public:
    explicit Teacher(const Department& dept) : dept_(dept) {}
    std::string department_name() const { return dept_.name(); }
private:
    const Department& dept_;   // associated — Teacher doesn't create/destroy Department
};
```

### 3. Aggregation (has-a, shared ownership)

A "whole–part" relationship where the **part can exist independently** of the whole. The whole does not destroy the parts.

```cpp
class Team {
public:
    void add_member(Employee* e) { members_.push_back(e); }
    // Team is destroyed → employees still exist elsewhere
private:
    std::vector<Employee*> members_;   // non-owning — raw pointers or weak_ptr
};
```

### 4. Composition (owns-a, exclusive ownership)

A "whole–part" relationship where the **part cannot exist without the whole**. The whole owns and destroys the parts.

```cpp
class Car {
    Engine   engine_;    // Engine lives and dies with Car
    Gearbox  gearbox_;
public:
    Car() : engine_(2000), gearbox_(6) {}
    // ~Car() destroys engine_ and gearbox_ automatically
};
```

### 5. Inheritance (is-a)

Already covered in Section 4. Strongest coupling — use only for true taxonomic is-a relationships.

### Relationship Summary

| Relationship | Keyword | Lifetime | C++ idiom |
|---|---|---|---|
| Dependency | uses | Temporary (parameter) | Method parameter |
| Association | knows | Independent | Raw pointer / reference member |
| Aggregation | has (shared) | Part survives whole | `vector<T*>` / `weak_ptr` |
| Composition | owns | Part dies with whole | Value member / `unique_ptr` |
| Inheritance | is-a | Coupled | `: public Base` |

---

## 11. SOLID Principles

Five design principles for maintainable OOP code:

```
S — Single Responsibility Principle
O — Open/Closed Principle
L — Liskov Substitution Principle
I — Interface Segregation Principle
D — Dependency Inversion Principle
```

### S — Single Responsibility Principle (SRP)

> A class should have **one reason to change**.

```cpp
// VIOLATION — three reasons to change: parsing, validation, persistence
class UserManager {
public:
    User  parse_json(const std::string& json);
    bool  validate(const User& u);
    void  save_to_db(const User& u);
    void  send_welcome_email(const User& u);
};

// BETTER — each class has one responsibility
class UserParser     { User parse(const std::string& json); };
class UserValidator  { bool validate(const User& u); };
class UserRepository { void save(const User& u); };
class EmailService   { void send_welcome(const User& u); };
```

**Symptom of violation**: a class that needs to change whenever the DB schema, the email template, OR the JSON format changes.

---

### O — Open/Closed Principle (OCP)

> Classes should be **open for extension, closed for modification**.

```cpp
// VIOLATION — adding a new shape requires modifying existing code
class AreaCalculator {
public:
    double calculate(const Shape& s) {
        if (s.type == "circle")    return M_PI * s.r * s.r;
        if (s.type == "rectangle") return s.w * s.h;
        // Adding triangle → must modify this function
    }
};

// BETTER — extend via polymorphism, no modification needed
class Shape {
public:
    virtual double area() const = 0;
    virtual ~Shape() = default;
};
class Circle    : public Shape { double area() const override { return M_PI*r_*r_; } double r_; };
class Rectangle : public Shape { double area() const override { return w_*h_; }      double w_,h_; };
class Triangle  : public Shape { double area() const override { return 0.5*b_*h_; }  double b_,h_; };

// AreaCalculator never changes — new shapes just add new classes
double total(const std::vector<std::unique_ptr<Shape>>& shapes) {
    double sum = 0;
    for (auto& s : shapes) sum += s->area();
    return sum;
}
```

**C++ OCP mechanisms**: virtual functions, templates, std::function, strategy pattern.

---

### L — Liskov Substitution Principle (LSP)

> Derived class objects must be **substitutable** for base class objects without changing program correctness.

```cpp
// VIOLATION — Square breaks Rectangle's contract
class Rectangle {
public:
    virtual void set_width(int w)  { w_ = w; }
    virtual void set_height(int h) { h_ = h; }
    int area() const { return w_ * h_; }
protected:
    int w_, h_;
};

class Square : public Rectangle {
public:
    void set_width(int w)  override { w_ = h_ = w; } // side effect!
    void set_height(int h) override { w_ = h_ = h; }
};

void test(Rectangle& r) {
    r.set_width(4);
    r.set_height(5);
    assert(r.area() == 20);   // FAILS for Square — both sides became 5
}

Square sq;
test(sq);   // breaks the invariant assumed by test()

// FIX — don't inherit; make them siblings under IShape
class IShape    { virtual int area() const = 0; };
class Rectangle : public IShape { /* ... */ };
class Square    : public IShape { /* ... */ };
```

**LSP checklist:**
- Preconditions: derived may only **weaken** (accept more)
- Postconditions: derived may only **strengthen** (guarantee more)
- Invariants: derived must **preserve** all base invariants
- No new exceptions that base doesn't throw

---

### I — Interface Segregation Principle (ISP)

> Clients should **not be forced to depend on interfaces they do not use**. Split fat interfaces into small, focused ones.

```cpp
// VIOLATION — fat interface forces all implementors to define everything
class IWorker {
public:
    virtual void work()  = 0;
    virtual void eat()   = 0;   // Robot can't eat — forced to stub
    virtual void sleep() = 0;   // Robot can't sleep — forced to stub
    virtual ~IWorker() = default;
};

class Robot : public IWorker {
    void work()  override { /* work */ }
    void eat()   override {}    // forced stub — meaningless
    void sleep() override {}    // forced stub — meaningless
};

// BETTER — segregate into focused interfaces
class IWorkable { public: virtual void work()  = 0; virtual ~IWorkable() = default; };
class IEatable  { public: virtual void eat()   = 0; virtual ~IEatable()  = default; };
class ISleepable{ public: virtual void sleep() = 0; virtual ~ISleepable()= default; };

class Human : public IWorkable, public IEatable, public ISleepable {
    void work()  override { /* work */ }
    void eat()   override { /* eat  */ }
    void sleep() override { /* sleep*/ }
};

class Robot : public IWorkable {
    void work()  override { /* work */ }
    // Only implements what it actually does
};
```

**Rule of thumb**: if a class has to implement methods with empty bodies or throw `NotImplemented`, the interface is too fat — split it.

---

### D — Dependency Inversion Principle (DIP)

> High-level modules should **not depend on low-level modules**. Both should depend on **abstractions**.

```cpp
// VIOLATION — OrderService (high-level) depends on MySQLDatabase (low-level)
class MySQLDatabase {
public:
    void save_order(const Order& o) { /* mysql specific */ }
};

class OrderService {
    MySQLDatabase db_;   // tightly coupled to MySQL
public:
    void place(const Order& o) { db_.save_order(o); }
};
// Switching to PostgreSQL → must change OrderService

// CORRECT — both depend on IDatabase abstraction
class IDatabase {
public:
    virtual void save_order(const Order& o) = 0;
    virtual ~IDatabase() = default;
};

class MySQLDatabase    : public IDatabase { void save_order(const Order&) override { /*mysql*/ } };
class PostgresDatabase : public IDatabase { void save_order(const Order&) override { /*pg*/   } };

class OrderService {
    IDatabase& db_;   // depends on abstraction
public:
    explicit OrderService(IDatabase& db) : db_(db) {}
    void place(const Order& o) { db_.save_order(o); }
};

// Wire-up (composition root):
MySQLDatabase db;
OrderService  svc(db);   // inject concrete implementation
```

**DIP ≠ DI**: DIP is the *architectural principle*. DI (Dependency Injection) is one *technique* to achieve it.

### SOLID Quick Reference

| | Principle | Symptom of violation | Fix |
|---|---|---|---|
| **S** | Single Responsibility | Class changes for multiple unrelated reasons | Split into focused classes |
| **O** | Open/Closed | Adding a feature requires modifying existing code | Extend via polymorphism / strategy |
| **L** | Liskov Substitution | Derived class breaks base class contracts | Fix hierarchy or remove inheritance |
| **I** | Interface Segregation | Clients implement empty/stub methods | Split fat interfaces |
| **D** | Dependency Inversion | High-level depends on concrete low-level | Depend on abstractions, inject implementations |

---

## 12. Type Casting in OOP

### The Four C++ Casts

```cpp
static_cast<T>(expr)      // compile-time checked, related types
dynamic_cast<T>(expr)     // runtime checked, polymorphic types only
const_cast<T>(expr)       // add/remove const (use sparingly)
reinterpret_cast<T>(expr) // bit-level reinterpret (unsafe, rarely needed)
```

### `static_cast` — Compile-Time Cast

```cpp
// Upcasting (derived → base) — always safe, implicit
Derived* d = new Derived();
Base* b = d;               // implicit upcast — fine
Base* b2 = static_cast<Base*>(d);  // explicit — also fine

// Downcasting (base → derived) — unsafe if wrong type, NO runtime check
Base* b = new Base();      // actually a Base, not Derived
Derived* d = static_cast<Derived*>(b);  // compiles, but accessing d is UB!
d->derived_only_method();  // UB — b is not a Derived

// Safe use: when you KNOW the type from context
void process(Base* b) {
    // some logic guarantees b is always Derived here
    Derived* d = static_cast<Derived*>(b);  // ok if truly guaranteed
}
```

### `dynamic_cast` — Runtime Type Check

Requires at least one virtual function in the hierarchy (needs vtable/RTTI).

```cpp
class Base    { public: virtual ~Base() {} };
class Derived : public Base { public: void special() {} };
class Other   : public Base {};

Base* b = new Derived();

// Pointer cast — returns nullptr on failure (does NOT throw)
if (Derived* d = dynamic_cast<Derived*>(b)) {
    d->special();   // safe — b really is a Derived
}

// Reference cast — throws std::bad_cast on failure
try {
    Derived& d = dynamic_cast<Derived&>(*b);
    d.special();
} catch (const std::bad_cast& e) {
    // b is not a Derived
}

// Failed cast
Base* b2 = new Other();
Derived* d2 = dynamic_cast<Derived*>(b2);   // returns nullptr
assert(d2 == nullptr);

// Sideways cast (between sibling classes in multiple inheritance)
class IA { public: virtual ~IA() {} };
class IB { public: virtual ~IB() {} };
class C : public IA, public IB {};

IA* ia = new C();
IB* ib = dynamic_cast<IB*>(ia);   // sideways — only dynamic_cast can do this
```

### `static_cast` vs `dynamic_cast`

| | `static_cast` | `dynamic_cast` |
|---|---|---|
| Check | Compile-time | Runtime (RTTI) |
| Speed | Zero overhead | ~10–50 ns (vtable lookup) |
| Failure on wrong type | UB (silent) | nullptr / `bad_cast` (safe) |
| Requires virtual? | No | Yes |
| Downcast when type known | ✓ Preferred | Works but wasteful |
| Downcast when type unknown | Dangerous | ✓ Correct tool |
| Sideways cast | ✗ Impossible | ✓ Works |

```cpp
// Prefer static_cast: when you control the type (factory returned it)
std::unique_ptr<Base> make() { return std::make_unique<Derived>(); }
auto b = make();
Derived* d = static_cast<Derived*>(b.get());  // we know it's Derived

// Prefer dynamic_cast: visitor-style, unknown runtime type
void handle(Base* b) {
    if (auto* d = dynamic_cast<Derived*>(b)) { /* ... */ }
    else if (auto* o = dynamic_cast<Other*>(b)) { /* ... */ }
}
// But: this pattern is usually a sign you need virtual dispatch instead
```

### `const_cast` — Remove/Add const

```cpp
void legacy_c_api(char* str);          // old API doesn't take const char*

void call_legacy(const std::string& s) {
    // Remove const to call C API — safe only if API doesn't modify str
    legacy_c_api(const_cast<char*>(s.c_str()));
}

// NEVER remove const from a truly const object — UB
const int x = 42;
int* p = const_cast<int*>(&x);
*p = 99;   // UB — x was declared const, modification is undefined
```

### RTTI — `typeid`

```cpp
#include <typeinfo>

Base* b = new Derived();

// typeid on pointer dereferences through virtual dispatch
std::cout << typeid(*b).name();         // implementation-defined, e.g. "7Derived"

// Exact type comparison
if (typeid(*b) == typeid(Derived)) { /* b points to exactly a Derived */ }
// Note: typeid gives exact type, NOT "is-a" — use dynamic_cast for is-a check

// Disable RTTI: -fno-rtti (GCC/Clang) → dynamic_cast and typeid won't work
```

---

## 13. Covariant Return Types

A derived class can **narrow the return type** of a virtual function — the return type must be covariant (a derived pointer/reference of the base's return type).

```cpp
class Base {
public:
    virtual Base* clone() const {
        return new Base(*this);
    }
    virtual ~Base() = default;
};

class Derived : public Base {
public:
    // Covariant return: returns Derived* instead of Base*
    Derived* clone() const override {
        return new Derived(*this);   // caller gets Derived* directly — no cast needed
    }
};

Derived d;
Derived* d2 = d.clone();    // no static_cast needed — covariant return type
Base*    b  = d.clone();    // also fine — implicit upcast

// Without covariant return:
// Base* clone() override { return new Derived(*this); }
// caller: Derived* d2 = static_cast<Derived*>(d.clone()); // extra cast
```

**Use case**: prototype/clone pattern — the derived `clone()` is called via virtual dispatch but returns the exact type when called on a derived reference directly.

---

## 15. Composition vs Inheritance

**Prefer composition over inheritance** (Effective C++ Item 38) — inheritance is the strongest coupling in OOP.

```cpp
// INHERITANCE approach — "is-a"
class Stack : public std::vector<int> {
    // Problem: exposes push_back, insert, erase — Stack shouldn't have these!
    // std::vector has no virtual destructor — UB if deleted through vector*
};

// COMPOSITION approach — "has-a" / "implemented-in-terms-of"
class Stack {
public:
    void   push(int x)   { data_.push_back(x); }
    void   pop()         { data_.pop_back(); }
    int    top()   const { return data_.back(); }
    bool   empty() const { return data_.empty(); }
    size_t size()  const { return data_.size(); }
private:
    std::vector<int> data_;  // implementation detail — hidden
    // Only push/pop/top are exposed — Stack invariant enforced
};
```

**Inherit when**:
- There is a true **is-a** relationship (a Circle **is a** Shape)
- You need **runtime polymorphism** (virtual dispatch)
- The base class was **designed for inheritance** (virtual dtor, protected members)

**Compose when**:
- You want to **reuse implementation** but not the interface
- The relationship is **has-a** (a Car **has an** Engine)
- You want **flexibility** to swap implementations at runtime

---

## 16. Composition vs Dependency Injection

They are **not opposites** — DI is a *way of doing* composition. The distinction is in **who creates the dependency**.

```
Composition  = structural mechanism  — "A owns a B"
DI           = assembly strategy     — "who creates B and hands it to A"

DI is always composition. Composition is not always DI.
```

---

### Composition — Class Creates Its Own Dependency

The class **creates and owns** its dependency internally. The dependency is a pure implementation detail.

```cpp
class MessageService {
public:
    MessageService() : logger_(std::make_unique<FileLogger>("app.log")) {}

    void send(const std::string& msg) {
        logger_->log("sending: " + msg);
    }

private:
    std::unique_ptr<FileLogger> logger_;  // concrete type, created internally
};

// Caller has no say in what Logger is used
MessageService svc;
svc.send("hello");
```

**Problem**: `MessageService` is **hardwired** to `FileLogger`:
- Cannot test without writing to a real file
- Cannot swap for `ConsoleLogger` or `NullLogger` without changing the class
- Adding a second logger type requires modifying `MessageService`

---

### Dependency Injection — Caller Provides the Dependency

The class **receives** its dependency from outside. The caller decides what concrete type is used.

#### Constructor Injection (Preferred)

```cpp
class MessageService {
public:
    explicit MessageService(std::unique_ptr<ILogger> logger)
        : logger_(std::move(logger)) {}

    void send(const std::string& msg) {
        logger_->log("sending: " + msg);
    }

private:
    std::unique_ptr<ILogger> logger_;  // owns the interface, not the concrete type
};

// Production — inject real logger
auto svc = MessageService(std::make_unique<FileLogger>("app.log"));

// Test — inject fake logger, zero file I/O
auto svc = MessageService(std::make_unique<NullLogger>());

// Swap freely — caller decides
auto svc = MessageService(std::make_unique<ConsoleLogger>());
```

#### Setter Injection (Optional / Late-Bound Dependencies)

```cpp
class MessageService {
public:
    MessageService() : logger_(std::make_unique<NullLogger>()) {} // sensible default

    void set_logger(std::unique_ptr<ILogger> logger) {
        logger_ = std::move(logger);
    }

    void send(const std::string& msg) { logger_->log(msg); }

private:
    std::unique_ptr<ILogger> logger_;
};

MessageService svc;
svc.send("quiet");                                    // uses NullLogger
svc.set_logger(std::make_unique<FileLogger>("app.log"));
svc.send("logged");                                   // uses FileLogger
```

#### Interface Injection (Non-Owning / Borrowed)

The class holds a **reference** — the caller owns the dependency and manages its lifetime.

```cpp
class MessageService {
public:
    explicit MessageService(ILogger& logger) : logger_(logger) {}

    void send(const std::string& msg) { logger_.log(msg); }

private:
    ILogger& logger_;   // borrows — must outlive MessageService
};

// Caller owns the logger — no heap allocation needed
ConsoleLogger logger;
MessageService svc(logger);
svc.send("hello");
```

---

### Side-by-Side: Tight vs Loose Coupling

```cpp
// ── COMPOSITION — tight coupling ─────────────────────────────────────────
class OrderService {
    PaymentProcessor processor_{"stripe_key"};  // concrete, hardwired
    EmailNotifier    notifier_{"smtp.host"};    // concrete, hardwired
public:
    void place_order(const Order& o) {
        processor_.charge(o.total());
        notifier_.send(o.email(), "order confirmed");
    }
};
// Impossible to test without hitting real Stripe and real SMTP

// ── DEPENDENCY INJECTION — loose coupling ─────────────────────────────────
class OrderService {
    std::unique_ptr<IPaymentProcessor> processor_;
    std::unique_ptr<INotifier>         notifier_;
public:
    OrderService(std::unique_ptr<IPaymentProcessor> p,
                 std::unique_ptr<INotifier>         n)
        : processor_(std::move(p)), notifier_(std::move(n)) {}

    void place_order(const Order& o) {
        processor_->charge(o.total());
        notifier_->send(o.email(), "order confirmed");
    }
};

// Production
auto svc = OrderService(
    std::make_unique<StripeProcessor>("key"),
    std::make_unique<SmtpNotifier>("host")
);

// Test — no real payment, no real email
auto svc = OrderService(
    std::make_unique<MockProcessor>(),
    std::make_unique<MockNotifier>()
);
```

---

### Comparison Table

| | **Composition (no DI)** | **Dependency Injection** |
|---|---|---|
| **Who creates the dependency** | The class itself | The caller / container |
| **Coupling** | Tight (to concrete type) | Loose (to interface) |
| **Testability** | Hard — real deps always used | Easy — inject mocks/fakes |
| **Flexibility** | Fixed at compile time | Swappable at runtime |
| **Simplicity** | Fewer abstractions | More wiring required |
| **Ownership** | Clear — class owns and creates | Must be explicit (`unique_ptr` vs ref) |
| **Best for** | Truly internal details | Deps with multiple impls or side effects |

---

### When to Use Each

**Use plain composition when the dependency IS the implementation detail:**

```cpp
// A mutex is always a mutex — no meaningful alternative
class Logger {
    std::mutex    mutex_;   // no abstraction needed
    std::ofstream file_;    // Logger owns its file entirely
public:
    explicit Logger(std::string path) : file_(std::move(path)) {}
};
```

**Use DI when:**

```cpp
// 1. Multiple implementations exist or will exist
class ReportGenerator {
    std::unique_ptr<IExporter> exporter_;  // PDF, CSV, HTML — inject which one
};

// 2. Dependency has side effects to avoid in tests
class UserService {
    IDatabase& db_;   // inject mock DB — no real DB connection in unit tests
};

// 3. Dependency is shared across multiple objects
//    EventBus, Logger, Config — inject the same shared instance everywhere

// 4. Dependency is expensive to construct
class SearchService {
    ISearchIndex& index_;  // pre-built, shared — inject, don't recreate
};
```

---

### Manual DI Container — Composition Root

For larger systems, wire all dependencies in **one place** (the composition root):

```cpp
struct AppContainer {
    // Shared singletons — created once
    std::shared_ptr<IDatabase>  db     = std::make_shared<PostgresDB>("conn_str");
    std::shared_ptr<ILogger>    logger = std::make_shared<FileLogger>("app.log");
    std::shared_ptr<IEventBus>  bus    = std::make_shared<EventBus>();

    // Services — assembled from their dependencies
    std::unique_ptr<UserService>  users  = std::make_unique<UserService>(*db, *logger);
    std::unique_ptr<OrderService> orders = std::make_unique<OrderService>(*db, *bus, *logger);
    std::unique_ptr<HttpServer>   server = std::make_unique<HttpServer>(*users, *orders);
};

int main() {
    AppContainer app;
    app.server->run(8080);
    // All wiring in one place, every class is independently testable
}
```

---

### Key Takeaway

```
Ask: who is responsible for choosing the concrete type?

If the class itself → plain composition (acceptable for internal details)
If the caller      → dependency injection (required for testability + flexibility)
```

DI does not replace composition — it **refines it** by inverting control over which implementation is composed in. This is the **Dependency Inversion Principle** (the D in SOLID): high-level modules should not depend on low-level modules; both should depend on abstractions.

---

## 17. Common OOP Design Patterns

### Singleton — One Instance

```cpp
class Config {
public:
    static Config& instance() {
        static Config cfg;  // initialized once, thread-safe (C++11)
        return cfg;
    }
    std::string get(const std::string& key) const;

    Config(const Config&)            = delete;
    Config& operator=(const Config&) = delete;

private:
    Config() { /* load config file */ }
    std::map<std::string, std::string> data_;
};

Config::instance().get("db_host");
```

### Factory Method — Decouple Creation

```cpp
class Connection { public: virtual void send(const std::string&) = 0; virtual ~Connection() = default; };
class TcpConnection  : public Connection { void send(const std::string& m) override { /* TCP  */ } };
class UnixConnection : public Connection { void send(const std::string& m) override { /* Unix */ } };

// Factory — client code doesn't know which concrete type it gets
std::unique_ptr<Connection> make_connection(const std::string& type) {
    if (type == "tcp")  return std::make_unique<TcpConnection>();
    if (type == "unix") return std::make_unique<UnixConnection>();
    throw std::invalid_argument("unknown type: " + type);
}

auto conn = make_connection("tcp");
conn->send("hello");   // works regardless of concrete type
```

### Observer — Event Notification

```cpp
class Observer {
public:
    virtual void on_event(const std::string& event) = 0;
    virtual ~Observer() = default;
};

class EventSource {
    std::vector<Observer*> observers_;
public:
    void subscribe(Observer* o)   { observers_.push_back(o); }
    void unsubscribe(Observer* o) {
        observers_.erase(std::remove(observers_.begin(), observers_.end(), o),
                         observers_.end());
    }
protected:
    void notify(const std::string& event) {
        for (auto* o : observers_) o->on_event(event);
    }
};

class Button : public EventSource {
public:
    void click() { notify("click"); }
};

class Logger : public Observer {
    void on_event(const std::string& e) override {
        std::cout << "Event: " << e << "\n";
    }
};

Button btn;
Logger log;
btn.subscribe(&log);
btn.click();   // "Event: click"
```

### CRTP — Curiously Recurring Template Pattern (Static Polymorphism)

Achieves polymorphic behaviour at compile time — no vtable, no virtual call overhead.

```cpp
// Base class parameterised by the derived type
template<typename Derived>
class Printable {
public:
    void print() const {
        // Static dispatch — no virtual call
        static_cast<const Derived*>(this)->print_impl();
    }
};

class Point : public Printable<Point> {
    int x_, y_;
public:
    Point(int x, int y) : x_(x), y_(y) {}
    void print_impl() const {
        std::cout << "Point(" << x_ << "," << y_ << ")\n";
    }
};

class Circle : public Printable<Circle> {
    double r_;
public:
    explicit Circle(double r) : r_(r) {}
    void print_impl() const { std::cout << "Circle(r=" << r_ << ")\n"; }
};

Point  p{1, 2};  p.print();   // "Point(1,2)"  — no vtable lookup
Circle c{5.0};   c.print();   // "Circle(r=5)" — no vtable lookup
```

---

## Mock Interview Questions

---

**[Mid]** What is the difference between `public`, `protected`, and `private` inheritance?

> The inheritance access specifier controls how the base class's members appear in the derived class:
>
> | Base member | `public` inheritance | `protected` inheritance | `private` inheritance |
> |---|---|---|---|
> | `public` | `public` | `protected` | `private` |
> | `protected` | `protected` | `protected` | `private` |
> | `private` | inaccessible | inaccessible | inaccessible |
>
> **`public`** inheritance expresses **is-a**: a `Dog` is an `Animal`. The full interface of `Animal` is available through a `Dog*` or `Dog&`. This is the most common form.
>
> **`private`** inheritance expresses **implemented-in-terms-of**: `Stack : private vector<int>` reuses vector's implementation but does not expose the vector interface. Functionally similar to composition, but allows access to `protected` members of the base and enables overriding virtual functions. Usually composition is preferred.
>
> **`protected`** inheritance: base's public members become protected in derived — rarely used.

---

**[Mid]** Why must a base class have a virtual destructor?

> Without a virtual destructor, deleting a derived object through a base pointer uses **static dispatch** — it calls only `Base::~Base()`. The derived class's destructor never runs, leaking any resources it owns.
>
> ```cpp
> class Base { ~Base() {} };          // non-virtual dtor
> class Derived : public Base {
>     int* data_ = new int[100];
>     ~Derived() { delete[] data_; }  // never called!
> };
> Base* b = new Derived();
> delete b;   // calls ~Base() only → data_ leaked
> ```
>
> With `virtual ~Base()`, the vtable routes the destructor call through to `~Derived()` first, then `~Base()` automatically.
>
> **Rule**: any class that is (or may be) used polymorphically — i.e., objects deleted through a base pointer — must have a virtual destructor. If you don't intend the class to be a base, mark it `final`.

---

**[Mid]** What is the difference between function overloading, function overriding, and function hiding?

> **Overloading**: multiple functions with the **same name but different parameter lists** in the **same scope**. Resolved at compile time by the compiler choosing the best match.
> ```cpp
> void print(int);    // overload 1
> void print(double); // overload 2 — same scope, different signature
> ```
>
> **Overriding**: a derived class provides a new implementation for a **virtual function** inherited from a base class. Must have the **same signature**. Resolved at runtime through virtual dispatch. Use `override` to let the compiler verify.
> ```cpp
> struct Base    { virtual void foo(int); };
> struct Derived : Base { void foo(int) override; }; // overrides Base::foo
> ```
>
> **Hiding**: a derived class declares a function with the **same name** as a base class function (regardless of signature). The base class versions are **hidden** — not overloaded with, not overriding. Can be accidentally introduced when you mean to override but forget `virtual` or change the signature.
> ```cpp
> struct Base    { void bar(int); };
> struct Derived : Base { void bar(double); };  // hides ALL Base::bar overloads
> Derived d; d.bar(1);    // calls Derived::bar(double), Base::bar(int) is hidden
> ```
> Restore hidden names with a `using` declaration:
> ```cpp
> struct Derived : Base { using Base::bar; void bar(double); }; // both visible
> ```

---

**[Mid]** Explain the vtable. When is it created? What is stored in it?

> A **vtable** (virtual dispatch table) is a **per-class static array of function pointers**, one entry per virtual function in the class. It lives in the `.rodata` section of the binary — one table per class, not per instance.
>
> Every instance of a class with virtual functions contains a hidden **vptr** as its first member, pointing to its class's vtable. The vptr is set by the constructor and updated as each constructor in the chain runs.
>
> **Contents**: pointers to the most-derived override for each virtual function. If `Derived` overrides `foo()`, the vtable entry for `foo` points to `Derived::foo`. If it doesn't override `bar()`, the entry points to `Base::bar`.
>
> **When created**: the vtable is generated by the compiler at compile time and embedded in the binary. The vptr is initialized at runtime in the constructor.
>
> A virtual function call `base_ptr->foo()` compiles to: load vptr from `*base_ptr`, load function pointer from `vtable[slot]`, call it. Two extra memory reads vs a direct call — but what matters more is the lost opportunity to inline.

---

**[Senior]** What is object slicing and when does it happen?

> **Object slicing** occurs when a derived class object is **copied or assigned into a base class object** — the derived-class-specific members are "sliced off" because the base has no room for them, and the base's copy constructor/assignment runs instead of the derived one.
>
> ```cpp
> struct Base   { int x;      virtual void foo() { puts("Base"); } };
> struct Derived : Base { int y; void foo() override { puts("Derived"); } };
>
> Derived d; d.x = 1; d.y = 2;
> Base    b = d;       // SLICING — copies only Base part, y is discarded
> b.foo();             // "Base" — b is a Base, not a Derived. vptr points to Base's vtable.
>
> // Also happens in function calls
> void process(Base b) { b.foo(); }  // takes by value — slices!
> process(d);          // "Base" — Derived info lost
>
> // Fix: always take by reference or pointer for polymorphic types
> void process(const Base& b) { b.foo(); }  // virtual dispatch preserved
> process(d);          // "Derived" — works correctly
> ```
>
> Slicing is a **silent data loss** bug — the compiler doesn't warn. Prevent it by making the base class abstract (pure virtual), or by deleting or protecting the base's copy constructor.

---

**[Senior]** Composition vs inheritance — when would you choose each?

> **Inheritance (is-a)**: choose it when:
> - There is a genuine **taxonomic relationship** — a `Circle` is a `Shape` in a way that every `Shape` operation makes sense for `Circle`
> - You need **runtime polymorphism** — to store `Shape*` and call `area()` on whatever concrete type it points to
> - The base class was **designed for inheritance** — has a virtual destructor, protected hooks, and stable interface
>
> **Composition (has-a / implemented-in-terms-of)**: prefer it when:
> - You want to **reuse implementation** without inheriting the interface (a `Stack` uses a `vector` but shouldn't expose `insert`)
> - You want to **swap implementations at runtime** — hold a `std::unique_ptr<StoragePolicy>` and replace it without recompiling
> - The base class was **not designed for inheritance** — no virtual destructor, inheriting would expose dangerous operations
> - The relationship is better described as ownership (`Car` **has an** `Engine`)
>
> **Practical rule**: if you find yourself inheriting from a concrete class (not an abstract base / interface), that's usually a sign to switch to composition. Inheritance from a concrete class is the tightest coupling in OOP — changes to the base break the derived class in unexpected ways (fragile base class problem).
>
> **Liskov Substitution Principle** is the key test: if you can substitute a `Derived` object wherever a `Base` is expected without changing the program's correctness, inheritance is appropriate. If not — use composition.

---

**[Senior]** What is the Liskov Substitution Principle (LSP) and how can it be violated in C++?

> **LSP** (the L in SOLID): if `S` is a subtype of `T`, then objects of type `T` may be replaced with objects of type `S` without altering the correctness of the program. In other words, a derived class must be usable everywhere the base class is expected, without surprises.
>
> **Classic violation — the Square/Rectangle problem**:
> ```cpp
> class Rectangle {
> public:
>     virtual void set_width (int w) { w_ = w; }
>     virtual void set_height(int h) { h_ = h; }
>     int area() const { return w_ * h_; }
> protected:
>     int w_, h_;
> };
>
> class Square : public Rectangle {
> public:
>     void set_width (int w) override { w_ = h_ = w; } // must keep equal
>     void set_height(int h) override { w_ = h_ = h; } // must keep equal
> };
>
> void scale_width(Rectangle& r) {
>     int old_h = r.area() / 10;  // assume width is 10
>     r.set_width(20);
>     assert(r.area() == old_h * 20);  // FAILS for Square — height also changed
> }
> ```
> `Square` violates LSP — it cannot be substituted for `Rectangle` without breaking the caller's invariants.
>
> **Other C++ violations**:
> - Derived class throws exceptions the base doesn't declare (pre-C++17)
> - Derived class weakens a precondition: base accepts any positive number, derived only accepts even numbers
> - Derived class strengthens a postcondition in a way callers don't expect
> - Overriding a non-virtual function (hiding) — caller using a base pointer gets base behaviour, caller using derived pointer gets different behaviour
>
> **Fix**: if `Square` and `Rectangle` cannot share the `set_width`/`set_height` interface without violating LSP, they should **not** be in an inheritance relationship — make them siblings under a common `Shape` interface that doesn't expose mutable width/height separately.

---

**[Mid]** What is the difference between composition and dependency injection?

> **Composition** is the structural relationship — class A *owns* a member of type B. It describes what A *has*.
>
> **Dependency injection** is the assembly strategy — it answers *who creates B and hands it to A*. When A creates B itself internally, that is composition without DI. When the caller creates B and passes it to A, that is composition *with* DI.
>
> DI is always a form of composition. The difference is control:
>
> ```cpp
> // Composition only — A creates B, tightly coupled to FileLogger
> class A {
>     std::unique_ptr<FileLogger> logger_ = std::make_unique<FileLogger>("app.log");
> };
>
> // Composition + DI — caller decides which ILogger to use
> class A {
>     std::unique_ptr<ILogger> logger_;
> public:
>     explicit A(std::unique_ptr<ILogger> l) : logger_(std::move(l)) {}
> };
> ```
>
> The practical consequence: without DI, you cannot unit-test A without hitting a real file. With DI you inject a `NullLogger` or `MockLogger` in tests — no side effects.

---

**[Mid]** What are the three forms of dependency injection? When would you choose each?

> **Constructor injection** — dependency passed in the constructor. Preferred because:
> - The dependency is always present after construction (no null-check needed)
> - The class's dependencies are visible at the call site
> - Works naturally with `const` members and `unique_ptr`
> ```cpp
> explicit Service(std::unique_ptr<IDatabase> db) : db_(std::move(db)) {}
> ```
>
> **Setter injection** — dependency set after construction via a method. Use when:
> - The dependency is optional (has a sensible default)
> - The dependency needs to change at runtime
> - Circular dependencies make constructor injection impossible
> ```cpp
> void set_logger(std::unique_ptr<ILogger> l) { logger_ = std::move(l); }
> ```
> Risk: the object may be used before the dependency is set — need null checks or invariant enforcement.
>
> **Interface injection (reference/pointer parameter)** — the caller owns the dependency and passes a reference. Use when:
> - The dependency is shared and long-lived (same logger used by many objects)
> - You want to avoid heap allocation
> - Lifetime is guaranteed by the caller's scope
> ```cpp
> explicit Service(ILogger& logger) : logger_(logger) {} // borrows, doesn't own
> ```
> Risk: dangling reference if the caller's object is destroyed while Service is still alive.

---

**[Senior]** What is the Dependency Inversion Principle and how does it relate to DI?

> The **Dependency Inversion Principle** (DIP, the D in SOLID) states:
> 1. High-level modules should not depend on low-level modules — both should depend on **abstractions**
> 2. Abstractions should not depend on details — details should depend on abstractions
>
> Without DIP:
> ```
> OrderService ──depends on──► StripePaymentProcessor  (concrete, low-level)
> ```
> `OrderService` (high-level business logic) is directly coupled to a Stripe-specific implementation. Changing payment provider requires changing `OrderService`.
>
> With DIP:
> ```
> OrderService ──depends on──► IPaymentProcessor  (abstraction)
>                                      ▲
>                         StripePaymentProcessor  (detail depends on abstraction)
> ```
> Both sides depend on the interface. `OrderService` never knows about Stripe.
>
> **DI is the mechanism that enforces DIP** at runtime: by injecting `IPaymentProcessor` into `OrderService` from outside, you ensure the high-level module only ever touches the abstraction, not the concrete detail.
>
> **DIP ≠ DI**: DIP is the *principle* (what your architecture should look like). DI is the *technique* (how you wire it up). You can violate DIP even while using DI — if you inject concrete types instead of interfaces, you get the plumbing of DI without the decoupling benefit.

---

**[Senior]** What are the downsides of dependency injection, and when would you avoid it?

> **Downsides**:
>
> 1. **Increased indirection**: every dependency becomes an interface + at least two classes (interface + implementation). Simple classes accumulate boilerplate.
>
> 2. **Harder to trace at a glance**: `service.process()` calls `db_->query()` — which `IDatabase` implementation is running? You must trace back to where the object was constructed.
>
> 3. **Lifetime complexity**: injected references must outlive the receiving object. With many injected dependencies, the dependency graph's lifetime ordering becomes a maintenance burden.
>
> 4. **Construction ceremony**: wiring up a complex object graph manually is verbose. DI frameworks (not available in standard C++) solve this but add their own complexity.
>
> 5. **Over-engineering for simple cases**: injecting a `std::mutex` or a `std::ofstream` that will never be swapped gains nothing and adds noise.
>
> **When to avoid**:
> - The dependency is an **internal algorithmic detail** with no meaningful alternative (e.g., the internal buffer of a ring queue)
> - The class is a **low-level utility** that will never be tested in isolation
> - You are writing **performance-critical code** where the virtual dispatch from an injected interface is measurably costly — use templates (CRTP or policy-based design) for compile-time injection instead
>
> ```cpp
> // Compile-time DI via template — zero virtual overhead
> template<typename Logger = NullLogger>
> class HotPathProcessor {
>     Logger logger_;
> public:
>     void process() { logger_.log("tick"); }  // direct call, inlineable
> };
>
> HotPathProcessor<FileLogger> prod;   // production
> HotPathProcessor<NullLogger> test;   // test — no overhead, no virtual
> ```
