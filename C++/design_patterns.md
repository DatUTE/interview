# Design Patterns in C++

---

## Overview

Design patterns are **proven, reusable solutions** to commonly recurring design problems. They are not code — they are templates for how to structure code to solve a class of problems.

Originated from the "Gang of Four" (GoF) book: *Design Patterns: Elements of Reusable Object-Oriented Software* (Gamma, Helm, Johnson, Vlissides, 1994).

```
23 GoF Patterns
├── Creational  — how objects are created
│   ├── Singleton
│   ├── Factory Method
│   ├── Abstract Factory
│   ├── Builder
│   └── Prototype
│
├── Structural  — how objects are composed
│   ├── Adapter
│   ├── Bridge
│   ├── Composite
│   ├── Decorator
│   ├── Facade
│   ├── Flyweight
│   └── Proxy
│
└── Behavioral  — how objects communicate
    ├── Chain of Responsibility
    ├── Command
    ├── Iterator
    ├── Mediator
    ├── Memento
    ├── Observer
    ├── State
    ├── Strategy
    ├── Template Method
    └── Visitor
```

---

# CREATIONAL PATTERNS

---

## 1. Singleton

### Intent
Ensure a class has **exactly one instance** and provide a global point of access to it.

### When to Use
- A process-wide service must have exactly one owner: logger, metrics registry, tracing exporter, crash reporter
- A resource is expensive or order-sensitive to initialize: configuration loader, feature flag registry, hardware/device manager
- One object coordinates shared state for the whole process: thread pool, scheduler, event loop, global cache manager
- Access is needed from application wiring or infrastructure code, not from deep business logic

### Practical Use Cases

| Use Case | Why Singleton Can Fit | Caveat |
|----------|-----------------------|--------|
| Logger | One output pipeline, shared formatting, synchronized writes | Prefer injecting `ILogger&` into business logic for testability |
| Configuration registry | Load once, expose read-only settings everywhere | Avoid mutable runtime config hidden behind global access |
| Metrics/tracing exporter | Central place to publish counters/spans | Keep API non-blocking; do not let telemetry slow hot paths |
| Thread pool / scheduler | Coordinates shared worker threads | Lifetime and shutdown order must be explicit |
| Hardware/device manager | Only one process-level owner should control a device | In multi-process systems, OS-level locking may still be required |
| Plugin/service registry | Central lookup table for registered factories/services | Registration order and static initialization can become fragile |

### When to Avoid

- Do not use Singleton just to avoid passing dependencies.
- Avoid it for business objects, request/user/session state, or anything that should vary per test.
- Avoid mutable global state unless there is a clear ownership and synchronization policy.
- In libraries, prefer dependency injection; the application can decide whether the dependency is singleton-scoped.

### Implementation (Meyers Singleton — thread-safe in C++11)

```cpp
class Logger {
public:
    // Thread-safe: static local variable initialized exactly once (C++11 guarantee)
    static Logger& instance() {
        static Logger inst;
        return inst;
    }

    void log(const std::string& msg) {
        std::lock_guard lock(mtx_);
        std::cout << "[LOG] " << msg << "\n";
    }

    // Prevent copy and move
    Logger(const Logger&)            = delete;
    Logger& operator=(const Logger&) = delete;

private:
    Logger()  { std::cout << "Logger created\n"; }
    ~Logger() { std::cout << "Logger destroyed\n"; }

    std::mutex mtx_;
};

// Usage
Logger::instance().log("application started");
Logger::instance().log("event occurred");
```

### Lazy vs Eager Initialization

```cpp
// Lazy (Meyers) — created on first use
static Logger& instance() {
    static Logger inst;   // created when first called
    return inst;
}

// Eager — created at program start (static member)
class Logger {
    inline static Logger inst_;           // C++17 inline static
public:
    static Logger& instance() { return inst_; }
};
```

### Testability Problem & Fix

```cpp
// Problem: Singleton is hardwired — hard to mock in tests
void process() {
    Logger::instance().log("processing");   // cannot inject a mock logger
}

// Fix: use Singleton only at the top level; inject as interface elsewhere
void process(ILogger& logger) {             // testable — inject mock
    logger.log("processing");
}
// In production: process(Logger::instance());
// In tests:      process(mock_logger);
```

### Trade-offs
- ✅ Guarantees single instance, lazy initialization
- ❌ Global state — hidden dependencies, hard to test
- ❌ Violates Single Responsibility Principle (manages own lifetime AND does work)
- ❌ Difficult in multi-process environments

---

## 2. Factory Method

### Intent
Define an interface for creating an object, but let **subclasses decide which class to instantiate**. Decouples object creation from the class that uses it.

### When to Use
- A class cannot anticipate what type of objects it needs to create
- Subclasses should control what gets created
- You want to provide a hook for subclasses to extend object creation

```cpp
// Product interface
class Button {
public:
    virtual void render() = 0;
    virtual void on_click() = 0;
    virtual ~Button() = default;
};

// Concrete products
class WindowsButton : public Button {
public:
    void render()   override { std::cout << "Rendering Windows button\n"; }
    void on_click() override { std::cout << "Windows click event\n"; }
};

class MacButton : public Button {
public:
    void render()   override { std::cout << "Rendering Mac button\n"; }
    void on_click() override { std::cout << "Mac click event\n"; }
};

// Creator (abstract) — declares the factory method
class Dialog {
public:
    // Factory method — subclasses override this
    virtual std::unique_ptr<Button> create_button() = 0;
    virtual ~Dialog() = default;

    // Template method using the factory method
    void render_dialog() {
        auto btn = create_button();  // calls overridden factory method
        btn->render();
        btn->on_click();
    }
};

// Concrete creators — override factory method
class WindowsDialog : public Dialog {
    std::unique_ptr<Button> create_button() override {
        return std::make_unique<WindowsButton>();
    }
};

class MacDialog : public Dialog {
    std::unique_ptr<Button> create_button() override {
        return std::make_unique<MacButton>();
    }
};

// Client code — works with any Dialog
void client(Dialog& dialog) {
    dialog.render_dialog();   // doesn't know which button is created
}

// Wiring (based on OS or config)
auto dialog = std::make_unique<WindowsDialog>();
client(*dialog);
```

### Simpler Form — Static Factory Function

```cpp
// No inheritance needed — just a function that returns polymorphic types
std::unique_ptr<ILogger> make_logger(const std::string& type) {
    if (type == "file")    return std::make_unique<FileLogger>("app.log");
    if (type == "console") return std::make_unique<ConsoleLogger>();
    if (type == "null")    return std::make_unique<NullLogger>();
    throw std::invalid_argument("unknown logger type: " + type);
}
```

---

## 3. Abstract Factory

### Intent
Provide an interface for creating **families of related objects** without specifying their concrete classes. Ensures products from the same factory are compatible.

### When to Use
- System must work with multiple families of products (e.g., Windows vs Mac UI widgets)
- Products within a family must be used together
- You want to enforce consistency among products

```cpp
// Abstract product family — Button + Checkbox must match (both Windows or both Mac)
class Button   { public: virtual void paint() = 0; virtual ~Button() = default; };
class Checkbox { public: virtual void paint() = 0; virtual ~Checkbox() = default; };

// Concrete Windows family
class WinButton   : public Button   { void paint() override { puts("Win Button");   } };
class WinCheckbox : public Checkbox { void paint() override { puts("Win Checkbox"); } };

// Concrete Mac family
class MacButton   : public Button   { void paint() override { puts("Mac Button");   } };
class MacCheckbox : public Checkbox { void paint() override { puts("Mac Checkbox"); } };

// Abstract Factory — creates a complete compatible family
class GUIFactory {
public:
    virtual std::unique_ptr<Button>   create_button()   = 0;
    virtual std::unique_ptr<Checkbox> create_checkbox()  = 0;
    virtual ~GUIFactory() = default;
};

// Concrete factories — each creates a consistent family
class WindowsFactory : public GUIFactory {
public:
    std::unique_ptr<Button>   create_button()   override { return std::make_unique<WinButton>();   }
    std::unique_ptr<Checkbox> create_checkbox()  override { return std::make_unique<WinCheckbox>(); }
};

class MacFactory : public GUIFactory {
public:
    std::unique_ptr<Button>   create_button()   override { return std::make_unique<MacButton>();   }
    std::unique_ptr<Checkbox> create_checkbox()  override { return std::make_unique<MacCheckbox>(); }
};

// Application — only knows about the abstract factory
class Application {
    std::unique_ptr<Button>   btn_;
    std::unique_ptr<Checkbox> chk_;
public:
    Application(GUIFactory& factory)
        : btn_(factory.create_button())
        , chk_(factory.create_checkbox()) {}

    void render() { btn_->paint(); chk_->paint(); }
};

// Wiring at composition root
auto factory = std::make_unique<WindowsFactory>();
Application app(*factory);
app.render();
```

### Factory Method vs Abstract Factory

| | **Factory Method** | **Abstract Factory** |
|---|---|---|
| Creates | One product | A family of related products |
| How | Subclass overrides a method | Inject a factory object |
| Use when | Need to vary one product type | Need to vary a product family |

---

## 4. Builder

### Intent
Separate the **construction** of a complex object from its **representation**. The same construction process can create different representations.

### When to Use
- Object requires many optional parameters (avoids telescoping constructors)
- Construction involves multiple steps that must happen in a specific order
- Same construction process should produce different types

```cpp
// Complex object with many optional fields
struct HttpRequest {
    std::string              url;
    std::string              method  = "GET";
    std::string              body;
    std::map<std::string, std::string> headers;
    int                      timeout = 30;
    bool                     follow_redirects = true;
};

// Builder — fluent interface
class HttpRequestBuilder {
    HttpRequest req_;
public:
    HttpRequestBuilder& url(std::string u)     { req_.url = std::move(u); return *this; }
    HttpRequestBuilder& method(std::string m)  { req_.method = std::move(m); return *this; }
    HttpRequestBuilder& body(std::string b)    { req_.body = std::move(b); return *this; }
    HttpRequestBuilder& header(std::string k, std::string v) {
        req_.headers[std::move(k)] = std::move(v); return *this;
    }
    HttpRequestBuilder& timeout(int t)         { req_.timeout = t; return *this; }
    HttpRequestBuilder& no_redirect()          { req_.follow_redirects = false; return *this; }

    HttpRequest build() {
        if (req_.url.empty())
            throw std::logic_error("URL is required");
        return std::move(req_);
    }
};

// Usage — only set what you need, readable, no 10-arg constructor
auto req = HttpRequestBuilder{}
    .url("https://api.example.com/users")
    .method("POST")
    .header("Content-Type", "application/json")
    .header("Authorization", "Bearer token123")
    .body(R"({"name":"Alice"})")
    .timeout(10)
    .build();
```

### Director Pattern (Optional)

```cpp
// Director encodes a specific construction sequence
class HttpRequestDirector {
    HttpRequestBuilder& builder_;
public:
    explicit HttpRequestDirector(HttpRequestBuilder& b) : builder_(b) {}

    void build_json_post(const std::string& url, const std::string& body) {
        builder_
            .url(url)
            .method("POST")
            .header("Content-Type", "application/json")
            .header("Accept", "application/json")
            .body(body);
    }
};
```

---

## 5. Prototype

### Intent
Create new objects by **copying (cloning) an existing object**. Avoids the cost of creation from scratch when initialization is expensive.

### When to Use
- Object creation is expensive and a similar object already exists
- Classes to instantiate are specified at runtime
- Need copies of objects with slight variations

```cpp
class Shape {
public:
    virtual std::unique_ptr<Shape> clone() const = 0;   // the clone method
    virtual void draw() const = 0;
    virtual ~Shape() = default;

    int x = 0, y = 0;
    std::string color = "black";
};

class Circle : public Shape {
public:
    double radius;

    std::unique_ptr<Shape> clone() const override {
        return std::make_unique<Circle>(*this);  // copy constructor
    }
    void draw() const override {
        std::cout << "Circle r=" << radius
                  << " at (" << x << "," << y << ") color=" << color << "\n";
    }
};

class Rectangle : public Shape {
public:
    double w, h;
    std::unique_ptr<Shape> clone() const override {
        return std::make_unique<Rectangle>(*this);
    }
    void draw() const override {
        std::cout << "Rect " << w << "x" << h << "\n";
    }
};

// Prototype registry — cache of ready-to-clone templates
class ShapeRegistry {
    std::unordered_map<std::string, std::unique_ptr<Shape>> cache_;
public:
    void add(std::string key, std::unique_ptr<Shape> shape) {
        cache_[key] = std::move(shape);
    }

    std::unique_ptr<Shape> get(const std::string& key) const {
        auto it = cache_.find(key);
        if (it == cache_.end()) throw std::out_of_range("shape not found");
        return it->second->clone();   // return a COPY, not the original
    }
};

// Usage — clone instead of creating from scratch
ShapeRegistry registry;
auto proto = std::make_unique<Circle>();
proto->radius = 10; proto->color = "red";
registry.add("red_circle", std::move(proto));

auto c1 = registry.get("red_circle");   // clone 1
auto c2 = registry.get("red_circle");   // clone 2 — independent copy
c2->x = 50; c2->y = 100;               // modify without affecting the prototype
```

---

# STRUCTURAL PATTERNS

---

## 6. Adapter

### Intent
Convert the **interface of a class** into another interface that clients expect. Lets incompatible interfaces work together.

### When to Use
- Integrating a third-party library with an incompatible interface
- Reusing existing code that doesn't match the required interface
- Legacy code integration

```cpp
// Target interface — what the client expects
class IJsonParser {
public:
    virtual std::string parse(const std::string& json) = 0;
    virtual ~IJsonParser() = default;
};

// Adaptee — existing class with incompatible interface (e.g., third-party library)
class XmlLibrary {
public:
    // Completely different interface
    void load_xml(const char* data, int len) { /* parse XML */ }
    std::string get_value(const char* xpath) { return "value"; }
};

// Adapter — wraps XmlLibrary to look like IJsonParser
class XmlToJsonAdapter : public IJsonParser {
    XmlLibrary xml_lib_;
public:
    std::string parse(const std::string& json) override {
        // Convert JSON to XML format, feed to XmlLibrary, return result
        std::string xml = json_to_xml(json);
        xml_lib_.load_xml(xml.c_str(), xml.size());
        return xml_lib_.get_value("//result");
    }
private:
    std::string json_to_xml(const std::string& json) { return "<xml>" + json + "</xml>"; }
};

// Client — only knows about IJsonParser
void client(IJsonParser& parser) {
    auto result = parser.parse(R"({"key":"value"})");
}

XmlToJsonAdapter adapter;
client(adapter);   // client doesn't know it's talking to XmlLibrary
```

### Object Adapter vs Class Adapter

```cpp
// Object Adapter (above) — wraps an instance — preferred in C++
class Adapter : public Target {
    Adaptee adaptee_;   // composition
};

// Class Adapter — multiple inheritance — more coupling
class Adapter : public Target, private Adaptee {
    void target_method() override {
        adaptee_method();  // directly call inherited method
    }
};
```

---

## 7. Bridge

### Intent
**Decouple an abstraction from its implementation** so the two can vary independently. Avoids a permanent binding between abstraction and implementation via composition.

### When to Use
- Want to avoid a permanent binding between abstraction and implementation
- Both abstraction and implementation should be extensible via subclassing
- Implementation details should be hidden from the client

```cpp
// Implementation interface (the "implementor" side of the bridge)
class IRenderer {
public:
    virtual void draw_circle(float x, float y, float r) = 0;
    virtual void draw_rect(float x, float y, float w, float h) = 0;
    virtual ~IRenderer() = default;
};

class OpenGLRenderer : public IRenderer {
public:
    void draw_circle(float x, float y, float r) override { /* OpenGL calls */ }
    void draw_rect(float x, float y, float w, float h) override { /* OpenGL calls */ }
};

class VulkanRenderer : public IRenderer {
public:
    void draw_circle(float x, float y, float r) override { /* Vulkan calls */ }
    void draw_rect(float x, float y, float w, float h) override { /* Vulkan calls */ }
};

// Abstraction — delegates to implementation via bridge (pointer)
class Shape {
protected:
    IRenderer& renderer_;   // the bridge
public:
    explicit Shape(IRenderer& r) : renderer_(r) {}
    virtual void draw() = 0;
    virtual ~Shape() = default;
};

// Refined abstractions — extend the shape hierarchy independently
class Circle : public Shape {
    float x_, y_, r_;
public:
    Circle(IRenderer& r, float x, float y, float radius)
        : Shape(r), x_(x), y_(y), r_(radius) {}

    void draw() override { renderer_.draw_circle(x_, y_, r_); }
};

class Square : public Shape {
    float x_, y_, side_;
public:
    Square(IRenderer& r, float x, float y, float side)
        : Shape(r), x_(x), y_(y), side_(side) {}

    void draw() override { renderer_.draw_rect(x_, y_, side_, side_); }
};

// Can combine any shape with any renderer without N×M subclasses
OpenGLRenderer ogl;
VulkanRenderer vk;

Circle c1(ogl, 0, 0, 5);    // Circle + OpenGL
Circle c2(vk,  10, 10, 3);  // Circle + Vulkan
Square s1(ogl, 5, 5, 4);    // Square + OpenGL
// Without bridge: need CircleOpenGL, CircleVulkan, SquareOpenGL, SquareVulkan classes
```

---

## 8. Composite

### Intent
Compose objects into **tree structures** to represent part-whole hierarchies. Lets clients treat individual objects and compositions of objects **uniformly**.

### When to Use
- Hierarchical tree structures (file systems, UI widget trees, scene graphs, org charts)
- Clients should ignore the difference between compositions and individual objects

```cpp
// Component interface — both leaf and composite implement this
class FileSystemNode {
public:
    virtual void   print(int indent = 0) const = 0;
    virtual size_t size() const = 0;
    virtual ~FileSystemNode() = default;
    std::string name;
};

// Leaf — no children
class File : public FileSystemNode {
    size_t size_;
public:
    File(std::string n, size_t s) : size_(s) { name = std::move(n); }
    void   print(int indent) const override {
        std::cout << std::string(indent, ' ') << name << " (" << size_ << " bytes)\n";
    }
    size_t size() const override { return size_; }
};

// Composite — contains other nodes (leaves or composites)
class Directory : public FileSystemNode {
    std::vector<std::unique_ptr<FileSystemNode>> children_;
public:
    explicit Directory(std::string n) { name = std::move(n); }

    void add(std::unique_ptr<FileSystemNode> node) {
        children_.push_back(std::move(node));
    }

    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "[" << name << "]\n";
        for (auto& c : children_) c->print(indent + 2);
    }

    size_t size() const override {
        size_t total = 0;
        for (auto& c : children_) total += c->size();
        return total;
    }
};

// Client treats File and Directory uniformly
auto root = std::make_unique<Directory>("root");
root->add(std::make_unique<File>("readme.txt", 1024));
auto src = std::make_unique<Directory>("src");
src->add(std::make_unique<File>("main.cpp", 4096));
src->add(std::make_unique<File>("util.cpp", 2048));
root->add(std::move(src));

root->print();          // prints entire tree
std::cout << root->size();  // total size of all files
```

---

## 9. Decorator

### Intent
**Attach additional responsibilities to an object dynamically**. Decorators provide a flexible alternative to subclassing for extending functionality.

### When to Use
- Add behaviour to individual objects without affecting others of the same class
- Behaviour can be combined and stacked in various configurations
- Extending by subclassing is impractical (too many combinations)

```cpp
// Component interface
class IDataSource {
public:
    virtual void write_data(const std::string& data) = 0;
    virtual std::string read_data() = 0;
    virtual ~IDataSource() = default;
};

// Concrete component — base implementation
class FileDataSource : public IDataSource {
    std::string filename_;
public:
    explicit FileDataSource(std::string name) : filename_(std::move(name)) {}
    void write_data(const std::string& data) override { /* write to file */ }
    std::string read_data() override { return "raw file data"; }
};

// Base decorator — wraps a component, delegates by default
class DataSourceDecorator : public IDataSource {
protected:
    std::unique_ptr<IDataSource> wrapped_;
public:
    explicit DataSourceDecorator(std::unique_ptr<IDataSource> ds)
        : wrapped_(std::move(ds)) {}
    void write_data(const std::string& data) override { wrapped_->write_data(data); }
    std::string read_data() override { return wrapped_->read_data(); }
};

// Concrete decorators — each adds one behaviour
class EncryptionDecorator : public DataSourceDecorator {
public:
    using DataSourceDecorator::DataSourceDecorator;
    void write_data(const std::string& data) override {
        wrapped_->write_data(encrypt(data));  // encrypt before writing
    }
    std::string read_data() override {
        return decrypt(wrapped_->read_data()); // decrypt after reading
    }
private:
    std::string encrypt(const std::string& s) { return "ENC(" + s + ")"; }
    std::string decrypt(const std::string& s) { return "RAW(" + s + ")"; }
};

class CompressionDecorator : public DataSourceDecorator {
public:
    using DataSourceDecorator::DataSourceDecorator;
    void write_data(const std::string& data) override {
        wrapped_->write_data(compress(data));
    }
    std::string read_data() override { return decompress(wrapped_->read_data()); }
private:
    std::string compress(const std::string& s)   { return "ZIP(" + s + ")"; }
    std::string decompress(const std::string& s) { return "RAW(" + s + ")"; }
};

// Stack decorators in any order — compose behaviours at runtime
auto source = std::make_unique<FileDataSource>("data.dat");

// Wrap with compression, then encrypt the compressed data
auto compressed = std::make_unique<CompressionDecorator>(std::move(source));
auto secure     = std::make_unique<EncryptionDecorator>(std::move(compressed));

secure->write_data("hello world");  // → encrypt(compress("hello world")) → file
secure->read_data();                // → file → decompress(decrypt(data))
```

---

## 10. Facade

### Intent
Provide a **simplified interface** to a complex subsystem. Doesn't prevent access to the subsystem — just provides a convenient front door.

### When to Use
- Provide a simple interface to a complex or poorly designed subsystem
- Layer your subsystem — facade is the entry point for each layer
- Reduce dependencies between client code and subsystem internals

```cpp
// Complex subsystem classes
class VideoDecoder   { public: void decode(const std::string& file) {} };
class AudioDecoder   { public: void decode(const std::string& file) {} };
class AudioMixer     { public: void mix() {} };
class VideoRenderer  { public: void render() {} };
class SubtitleParser { public: void load(const std::string& file) {} };
class DisplayManager { public: void show() {} };

// Facade — one simple interface over the whole subsystem
class VideoPlayerFacade {
    VideoDecoder   video_dec_;
    AudioDecoder   audio_dec_;
    AudioMixer     mixer_;
    VideoRenderer  renderer_;
    SubtitleParser subtitles_;
    DisplayManager display_;

public:
    // Client only needs to call play() — facade orchestrates the subsystem
    void play(const std::string& movie_file) {
        video_dec_.decode(movie_file);
        audio_dec_.decode(movie_file);
        subtitles_.load(movie_file + ".srt");
        mixer_.mix();
        renderer_.render();
        display_.show();
    }

    void stop() { /* coordinate shutdown sequence */ }
};

// Client code — simple
VideoPlayerFacade player;
player.play("inception.mkv");  // one call instead of 6
```

---

## 11. Flyweight

### Intent
Use **sharing** to support a large number of fine-grained objects efficiently. Separates intrinsic state (shared, immutable) from extrinsic state (per-object, passed in).

### When to Use
- Large number of similar objects consuming too much memory
- Object state can be split into shared (intrinsic) and context-specific (extrinsic) parts
- Game entities, characters in a text editor, particles

```cpp
// Intrinsic state — shared, immutable (the flyweight)
struct CharStyle {
    std::string font;
    int         size;
    bool        bold;
    // Potentially loaded from disk — expensive to duplicate per character
    void render(char c, int x, int y) const {
        printf("Render '%c' at (%d,%d) font=%s size=%d bold=%d\n",
               c, x, y, font.c_str(), size, bold);
    }
};

// Flyweight factory — cache and reuse intrinsic state
class CharStyleCache {
    std::map<std::tuple<std::string,int,bool>, std::shared_ptr<CharStyle>> cache_;
public:
    std::shared_ptr<CharStyle> get(const std::string& font, int size, bool bold) {
        auto key = std::make_tuple(font, size, bold);
        if (!cache_.count(key))
            cache_[key] = std::make_shared<CharStyle>(CharStyle{font, size, bold});
        return cache_[key];   // return shared instance
    }
    size_t cache_size() const { return cache_.size(); }
};

// Document character — extrinsic state (position, actual char) + flyweight reference
struct DocChar {
    char                        ch;    // extrinsic
    int                         x, y;  // extrinsic
    std::shared_ptr<CharStyle>  style; // intrinsic — shared
};

// Usage — 1 million 'A' chars in Arial 12pt bold share ONE CharStyle object
CharStyleCache cache;
std::vector<DocChar> document;

auto style = cache.get("Arial", 12, true);  // one object
for (int i = 0; i < 1'000'000; ++i)
    document.push_back({'A', i % 80, i / 80, style});  // all share same style ptr

// vs storing CharStyle inline: 1M × ~32 bytes = 32MB
// With flyweight:              1M ×  8 bytes (ptr) + 32 bytes (one style) ≈ 8MB
```

---

## 12. Proxy

### Intent
Provide a **surrogate or placeholder** for another object to control access to it.

### Types
- **Virtual Proxy**: lazy initialization — create expensive object only when needed
- **Protection Proxy**: access control — check permissions before forwarding
- **Remote Proxy**: represent remote object (RPC, network service)
- **Caching Proxy**: cache results of expensive operations
- **Smart Pointer**: `std::shared_ptr` / `std::unique_ptr` are proxies

```cpp
// Subject interface
class IImage {
public:
    virtual void display() = 0;
    virtual ~IImage() = default;
};

// Real subject — expensive to load
class HighResImage : public IImage {
    std::string filename_;
    std::vector<uint8_t> pixel_data_;  // large buffer
public:
    explicit HighResImage(const std::string& f) : filename_(f) {
        std::cout << "Loading " << f << " from disk... (expensive)\n";
        pixel_data_.resize(1024 * 1024 * 4);  // 4MB per image
    }
    void display() override { std::cout << "Displaying " << filename_ << "\n"; }
};

// Virtual Proxy — lazy initialization
class ImageProxy : public IImage {
    std::string filename_;
    mutable std::unique_ptr<HighResImage> real_image_;  // loaded on demand
public:
    explicit ImageProxy(std::string f) : filename_(std::move(f)) {}

    void display() override {
        if (!real_image_) {
            real_image_ = std::make_unique<HighResImage>(filename_); // load only now
        }
        real_image_->display();
    }
};

// Protection Proxy — access control
class SecureImageProxy : public IImage {
    std::unique_ptr<IImage> real_;
    std::string user_role_;
public:
    SecureImageProxy(std::unique_ptr<IImage> img, std::string role)
        : real_(std::move(img)), user_role_(std::move(role)) {}

    void display() override {
        if (user_role_ != "admin")
            throw std::runtime_error("Access denied");
        real_->display();
    }
};

// Caching Proxy
class CachedQueryProxy {
    std::function<std::string(const std::string&)> real_query_;
    std::unordered_map<std::string, std::string>   cache_;
public:
    explicit CachedQueryProxy(std::function<std::string(const std::string&)> q)
        : real_query_(std::move(q)) {}

    std::string query(const std::string& sql) {
        if (auto it = cache_.find(sql); it != cache_.end())
            return it->second;             // cache hit
        return cache_[sql] = real_query_(sql); // cache miss → call real, store
    }
};

// Usage
ImageProxy proxy("wallpaper.png");
// Image not loaded yet — proxy is lightweight
proxy.display();   // "Loading wallpaper.png..." only NOW
proxy.display();   // subsequent calls: already loaded, fast
```

---

# BEHAVIORAL PATTERNS

---

## 13. Strategy

### Intent
Define a **family of algorithms**, encapsulate each one, and make them **interchangeable**. Lets the algorithm vary independently from clients that use it.

### When to Use
- Multiple algorithms for the same task (sorting, compression, payment, routing)
- Need to switch algorithms at runtime
- Replace conditionals with polymorphism

```cpp
// Strategy interface
class ISortStrategy {
public:
    virtual void sort(std::vector<int>& data) = 0;
    virtual ~ISortStrategy() = default;
};

// Concrete strategies
class QuickSortStrategy : public ISortStrategy {
public:
    void sort(std::vector<int>& data) override {
        std::sort(data.begin(), data.end());  // introsort (quicksort-based)
        std::cout << "QuickSort applied\n";
    }
};

class BucketSortStrategy : public ISortStrategy {
public:
    void sort(std::vector<int>& data) override {
        // Efficient for uniformly distributed data in [0, 1000)
        std::cout << "BucketSort applied\n";
    }
};

// Context — uses a strategy, can switch at runtime
class DataSorter {
    std::unique_ptr<ISortStrategy> strategy_;
public:
    explicit DataSorter(std::unique_ptr<ISortStrategy> s)
        : strategy_(std::move(s)) {}

    void set_strategy(std::unique_ptr<ISortStrategy> s) {
        strategy_ = std::move(s);
    }

    void sort(std::vector<int>& data) {
        strategy_->sort(data);
    }
};

// Runtime strategy selection based on data characteristics
DataSorter sorter(std::make_unique<QuickSortStrategy>());
sorter.sort(data);

// Switch strategy based on context
if (data.size() > 1'000'000)
    sorter.set_strategy(std::make_unique<BucketSortStrategy>());

// Modern C++: strategy as std::function — no inheritance needed
class DataProcessor {
    std::function<void(std::vector<int>&)> sort_fn_;
public:
    explicit DataProcessor(std::function<void(std::vector<int>&)> fn)
        : sort_fn_(std::move(fn)) {}

    void process(std::vector<int>& data) { sort_fn_(data); }
};

DataProcessor proc([](auto& d){ std::sort(d.begin(), d.end()); });
DataProcessor proc2([](auto& d){ std::stable_sort(d.begin(), d.end()); });
```

---

## 14. Observer

### Intent
Define a **one-to-many dependency** between objects so that when one object changes state, all its dependents are **notified and updated automatically**.

### When to Use
- When changes to one object require changing others, and you don't know how many objects need to change
- Event systems, pub/sub, reactive programming, MVC (Model notifies View)

```cpp
// Observer interface
class IObserver {
public:
    virtual void update(const std::string& event, const std::any& data) = 0;
    virtual ~IObserver() = default;
};

// Subject (Observable) — maintains observer list
class EventEmitter {
    std::unordered_map<std::string,
        std::vector<std::weak_ptr<IObserver>>> listeners_;
public:
    void subscribe(const std::string& event, std::shared_ptr<IObserver> obs) {
        listeners_[event].push_back(obs);
    }

    void emit(const std::string& event, const std::any& data = {}) {
        auto it = listeners_.find(event);
        if (it == listeners_.end()) return;

        auto& vec = it->second;
        // Remove expired weak_ptrs, notify valid ones
        vec.erase(std::remove_if(vec.begin(), vec.end(),
            [&](auto& wp) {
                if (auto sp = wp.lock()) { sp->update(event, data); return false; }
                return true;  // expired — remove
            }), vec.end());
    }
};

// Stock market example
class StockMarket : public EventEmitter {
    std::unordered_map<std::string, double> prices_;
public:
    void update_price(const std::string& ticker, double price) {
        prices_[ticker] = price;
        emit("price_changed", std::make_pair(ticker, price));
    }
};

class Portfolio : public IObserver {
    std::string name_;
public:
    explicit Portfolio(std::string n) : name_(std::move(n)) {}
    void update(const std::string& event, const std::any& data) override {
        auto [ticker, price] = std::any_cast<std::pair<std::string,double>>(data);
        std::cout << name_ << " notified: " << ticker << " = $" << price << "\n";
    }
};

StockMarket market;
auto p1 = std::make_shared<Portfolio>("Alice's Portfolio");
auto p2 = std::make_shared<Portfolio>("Bob's Portfolio");

market.subscribe("price_changed", p1);
market.subscribe("price_changed", p2);

market.update_price("AAPL", 175.5);   // both portfolios notified
market.update_price("GOOG", 140.2);
```

---

## 15. Command

### Intent
**Encapsulate a request as an object**, allowing parameterization, queuing, logging, and undoable operations.

### When to Use
- Parameterize objects with operations
- Queue or schedule operations
- Support undo/redo
- Transaction system (rollback)

```cpp
// Command interface
class ICommand {
public:
    virtual void execute() = 0;
    virtual void undo()    = 0;
    virtual ~ICommand() = default;
};

// Receiver
class TextEditor {
    std::string text_;
public:
    void insert(int pos, const std::string& s) {
        text_.insert(pos, s);
        std::cout << "Text: " << text_ << "\n";
    }
    void erase(int pos, int len) {
        text_.erase(pos, len);
        std::cout << "Text: " << text_ << "\n";
    }
    const std::string& text() const { return text_; }
};

// Concrete command
class InsertCommand : public ICommand {
    TextEditor& editor_;
    int         position_;
    std::string text_;
public:
    InsertCommand(TextEditor& e, int pos, std::string t)
        : editor_(e), position_(pos), text_(std::move(t)) {}

    void execute() override { editor_.insert(position_, text_); }
    void undo()    override { editor_.erase(position_, text_.size()); }
};

// Invoker — manages command history for undo/redo
class CommandHistory {
    std::stack<std::unique_ptr<ICommand>> history_;
    std::stack<std::unique_ptr<ICommand>> redo_stack_;
public:
    void execute(std::unique_ptr<ICommand> cmd) {
        cmd->execute();
        history_.push(std::move(cmd));
        while (!redo_stack_.empty()) redo_stack_.pop();  // clear redo on new command
    }

    void undo() {
        if (history_.empty()) return;
        history_.top()->undo();
        redo_stack_.push(std::move(history_.top()));
        history_.pop();
    }

    void redo() {
        if (redo_stack_.empty()) return;
        redo_stack_.top()->execute();
        history_.push(std::move(redo_stack_.top()));
        redo_stack_.pop();
    }
};

TextEditor     editor;
CommandHistory history;

history.execute(std::make_unique<InsertCommand>(editor, 0, "Hello"));
history.execute(std::make_unique<InsertCommand>(editor, 5, " World"));
history.undo();    // removes " World"
history.redo();    // re-inserts " World"
```

---

## 16. Template Method

### Intent
Define the **skeleton of an algorithm** in a base class, deferring some steps to subclasses. Subclasses can override specific steps without changing the algorithm's structure.

### When to Use
- Multiple classes share the same algorithm structure but differ in specific steps
- Avoid code duplication in similar algorithms
- Control which parts of an algorithm can be overridden

```cpp
class DataMigrator {
public:
    // Template method — defines the algorithm skeleton (non-virtual)
    void migrate() {
        connect();
        extract_data();    // hook — must override
        transform_data();  // hook — optional override
        load_data();       // hook — must override
        disconnect();
        log_summary();     // common step — not overridable
    }
    virtual ~DataMigrator() = default;

protected:
    virtual void extract_data()   = 0;   // pure virtual — must implement
    virtual void load_data()      = 0;   // pure virtual — must implement
    virtual void transform_data() { }    // optional hook — default does nothing

private:
    void connect()     { std::cout << "Connecting to databases\n"; }
    void disconnect()  { std::cout << "Disconnecting\n"; }
    void log_summary() { std::cout << "Migration complete\n"; }
};

class MySQLToPostgresMigrator : public DataMigrator {
protected:
    void extract_data() override   { std::cout << "Extracting from MySQL\n"; }
    void transform_data() override { std::cout << "Transforming schema\n"; }
    void load_data() override      { std::cout << "Loading into Postgres\n"; }
};

class CsvToMongoMigrator : public DataMigrator {
protected:
    void extract_data() override { std::cout << "Reading CSV files\n"; }
    void load_data() override    { std::cout << "Inserting into MongoDB\n"; }
    // transform_data not overridden — default empty implementation used
};

MySQLToPostgresMigrator m1;
m1.migrate();   // connect → extract(MySQL) → transform → load(Postgres) → disconnect → log
```

---

## 17. State

### Intent
Allow an object to **alter its behaviour when its internal state changes**. The object will appear to change its class.

### When to Use
- Object behaviour depends on state, and must change at runtime
- Large number of conditionals based on state — replace with state objects
- Finite state machines (vending machine, traffic light, TCP connection, game character)

```cpp
class TrafficLight;  // forward declaration

class IState {
public:
    virtual void handle(TrafficLight& ctx) = 0;
    virtual std::string name() const = 0;
    virtual ~IState() = default;
};

class TrafficLight {
    std::unique_ptr<IState> state_;
public:
    explicit TrafficLight(std::unique_ptr<IState> initial)
        : state_(std::move(initial)) {}

    void transition_to(std::unique_ptr<IState> new_state) {
        std::cout << "  [" << state_->name() << " → " << new_state->name() << "]\n";
        state_ = std::move(new_state);
    }

    void tick() { state_->handle(*this); }
    std::string current() const { return state_->name(); }
};

// States
class GreenState : public IState {
public:
    std::string name() const override { return "Green"; }
    void handle(TrafficLight& ctx) override;   // defined after YellowState
};

class YellowState : public IState {
public:
    std::string name() const override { return "Yellow"; }
    void handle(TrafficLight& ctx) override;
};

class RedState : public IState {
public:
    std::string name() const override { return "Red"; }
    void handle(TrafficLight& ctx) override {
        std::cout << "Stop!\n";
        ctx.transition_to(std::make_unique<GreenState>());
    }
};

void GreenState::handle(TrafficLight& ctx) {
    std::cout << "Go!\n";
    ctx.transition_to(std::make_unique<YellowState>());
}

void YellowState::handle(TrafficLight& ctx) {
    std::cout << "Slow down!\n";
    ctx.transition_to(std::make_unique<RedState>());
}

TrafficLight light(std::make_unique<GreenState>());
light.tick();  // Go!    → Yellow
light.tick();  // Slow!  → Red
light.tick();  // Stop!  → Green
```

---

## 18. Chain of Responsibility

### Intent
Pass a request along a **chain of handlers**. Each handler decides to process it or pass it to the next. Decouples sender from receiver.

### When to Use
- More than one handler may handle a request, and the handler isn't known a priori
- Issue a request to one of several handlers without specifying the receiver explicitly
- Middleware pipelines, event bubbling, logging levels, auth chains

```cpp
class HttpRequest { public: std::string path, method, auth_token; int body_size = 0; };
class HttpResponse { public: int status = 200; std::string body; };

// Handler base
class Middleware {
protected:
    std::shared_ptr<Middleware> next_;
public:
    void set_next(std::shared_ptr<Middleware> n) { next_ = std::move(n); }
    virtual HttpResponse handle(const HttpRequest& req) {
        if (next_) return next_->handle(req);
        return {200, "OK"};
    }
    virtual ~Middleware() = default;
};

// Concrete handlers
class AuthMiddleware : public Middleware {
public:
    HttpResponse handle(const HttpRequest& req) override {
        if (req.auth_token.empty())
            return {401, "Unauthorized"};
        return Middleware::handle(req);  // pass to next
    }
};

class RateLimitMiddleware : public Middleware {
    int requests_ = 0, limit_;
public:
    explicit RateLimitMiddleware(int limit) : limit_(limit) {}
    HttpResponse handle(const HttpRequest& req) override {
        if (++requests_ > limit_)
            return {429, "Too Many Requests"};
        return Middleware::handle(req);
    }
};

class BodySizeMiddleware : public Middleware {
    int max_bytes_;
public:
    explicit BodySizeMiddleware(int max) : max_bytes_(max) {}
    HttpResponse handle(const HttpRequest& req) override {
        if (req.body_size > max_bytes_)
            return {413, "Payload Too Large"};
        return Middleware::handle(req);
    }
};

// Build the chain: Auth → RateLimit → BodySize → (actual handler)
auto auth      = std::make_shared<AuthMiddleware>();
auto rate      = std::make_shared<RateLimitMiddleware>(100);
auto body_size = std::make_shared<BodySizeMiddleware>(1024 * 1024);

auth->set_next(rate);
rate->set_next(body_size);

HttpRequest req; req.auth_token = "valid"; req.body_size = 512;
auto response = auth->handle(req);   // passes through all middleware
std::cout << response.status << " " << response.body << "\n";
```

---

## 19. Mediator

### Intent
Define an object that **encapsulates how a set of objects interact**. Promotes loose coupling by keeping objects from referring to each other explicitly.

### When to Use
- Many objects communicate in complex ways, creating tight coupling
- Centralise complex communications and control logic (chat rooms, air traffic control, UI form validation)

```cpp
class ChatRoom;  // mediator

class User {
    std::string  name_;
    ChatRoom*    room_ = nullptr;
public:
    explicit User(std::string n) : name_(std::move(n)) {}
    void join(ChatRoom& room);
    void send(const std::string& msg);
    void receive(const std::string& from, const std::string& msg) {
        std::cout << name_ << " received from " << from << ": " << msg << "\n";
    }
    const std::string& name() const { return name_; }
};

// Mediator
class ChatRoom {
    std::vector<User*> users_;
public:
    void join(User& u) {
        users_.push_back(&u);
        broadcast("System", u.name() + " joined the room");
    }

    void broadcast(const std::string& from, const std::string& msg) {
        for (User* u : users_)
            if (u->name() != from)
                u->receive(from, msg);
    }
};

void User::join(ChatRoom& room) { room_ = &room; room.join(*this); }
void User::send(const std::string& msg) {
    if (room_) room_->broadcast(name_, msg);
}

ChatRoom room;
User alice("Alice"), bob("Bob"), charlie("Charlie");
alice.join(room);
bob.join(room);
charlie.join(room);
alice.send("Hello everyone!");  // Bob and Charlie receive it — not Alice
```

---

## 20. Visitor

### Intent
Represent an **operation to be performed on elements** of an object structure. Lets you define a new operation without changing the classes of the elements on which it operates.

### When to Use
- Need to perform many distinct and unrelated operations on an object structure
- Don't want to pollute the classes with these operations
- The class hierarchy is stable but operations frequently change (AST traversal, document export)

```cpp
// Forward declarations
class Circle; class Rectangle; class Triangle;

// Visitor interface — one visit() per concrete element
class IShapeVisitor {
public:
    virtual void visit(Circle&)    = 0;
    virtual void visit(Rectangle&) = 0;
    virtual void visit(Triangle&)  = 0;
    virtual ~IShapeVisitor() = default;
};

// Element interface
class Shape {
public:
    virtual void accept(IShapeVisitor& v) = 0;
    virtual ~Shape() = default;
};

// Concrete elements — double dispatch via accept()
class Circle : public Shape {
public:
    double radius;
    explicit Circle(double r) : radius(r) {}
    void accept(IShapeVisitor& v) override { v.visit(*this); }
};

class Rectangle : public Shape {
public:
    double w, h;
    Rectangle(double w, double h) : w(w), h(h) {}
    void accept(IShapeVisitor& v) override { v.visit(*this); }
};

class Triangle : public Shape {
public:
    double base, height;
    Triangle(double b, double h) : base(b), height(h) {}
    void accept(IShapeVisitor& v) override { v.visit(*this); }
};

// Concrete visitors — new operations without modifying shapes
class AreaCalculator : public IShapeVisitor {
public:
    double total = 0;
    void visit(Circle& c)    override { total += M_PI * c.radius * c.radius; }
    void visit(Rectangle& r) override { total += r.w * r.h; }
    void visit(Triangle& t)  override { total += 0.5 * t.base * t.height; }
};

class JsonExporter : public IShapeVisitor {
    std::string json_ = "[";
public:
    void visit(Circle& c)    override { json_ += "{\"type\":\"circle\",\"r\":" + std::to_string(c.radius) + "},"; }
    void visit(Rectangle& r) override { json_ += "{\"type\":\"rect\"},"; }
    void visit(Triangle& t)  override { json_ += "{\"type\":\"tri\"},"; }
    std::string result() { return json_ + "]"; }
};

std::vector<std::unique_ptr<Shape>> shapes;
shapes.push_back(std::make_unique<Circle>(5));
shapes.push_back(std::make_unique<Rectangle>(4, 6));
shapes.push_back(std::make_unique<Triangle>(3, 8));

AreaCalculator area;
JsonExporter   exporter;

for (auto& s : shapes) {
    s->accept(area);
    s->accept(exporter);
}

std::cout << "Total area: " << area.total << "\n";
std::cout << "JSON: " << exporter.result() << "\n";
// Adding new operation (e.g., SVGExporter): only add a new visitor class
// Adding new shape type: must update ALL existing visitors — trade-off
```

---

## Pattern Comparison and Selection Guide

```
Problem                                          Pattern
─────────────────────────────────────────────    ─────────────────────────────
Ensure only one instance                         Singleton
Create objects without specifying exact class    Factory Method / Abstract Factory
Build complex object step by step                Builder
Clone existing objects                           Prototype
Adapt incompatible interfaces                    Adapter
Decouple abstraction from implementation         Bridge
Tree structure, treat nodes uniformly            Composite
Add behaviour dynamically without subclassing    Decorator
Simplify complex subsystem                       Facade
Share many fine-grained objects                  Flyweight
Control access to another object                 Proxy
Family of interchangeable algorithms             Strategy
Notify many objects when state changes           Observer
Encapsulate request as object (undo/queue)       Command
Algorithm skeleton, steps overridable            Template Method
Object changes behaviour based on state          State
Decouple chain of handlers                       Chain of Responsibility
Centralise complex object interactions           Mediator
Add operations to object hierarchy               Visitor
```

---

## Mock Interview Questions

---

**[Mid]** What is the difference between Strategy and State patterns?

> Both patterns use a similar structure — a context object holds a reference to a polymorphic interface — but their **intent** and **who controls the transition** differ.
>
> **Strategy**: the algorithm is **selected by the client** and typically doesn't change during an operation. The context delegates work to the strategy but doesn't know or care which one is active. Strategies are usually **stateless** and interchangeable.
>
> ```cpp
> sorter.set_strategy(std::make_unique<QuickSort>()); // client sets it
> sorter.sort(data);  // context uses it, strategy doesn't change itself
> ```
>
> **State**: the state **transitions itself** (or asks the context to transition). The context's behaviour changes as a result of the state change. States are **aware of each other** and manage the FSM lifecycle.
>
> ```cpp
> light.tick();  // inside GreenState::handle: ctx.transition_to(YellowState)
>                // state drives the transition — client just calls tick()
> ```
>
> **Simple test**: who initiates the change? Client → Strategy. Object itself (based on internal logic) → State.

---

**[Mid]** What is the difference between Decorator and Inheritance for extending behaviour?

> **Inheritance** extends behaviour **statically** — at compile time, by subclassing. Each combination of behaviours requires a new class. With 3 features (A, B, C), you might need `AB`, `AC`, `BC`, `ABC` subclasses — combinatorial explosion.
>
> **Decorator** extends behaviour **dynamically** — at runtime, by wrapping. Stack any combination of decorators without new classes:
>
> ```cpp
> // Inheritance — need a class for every combination
> class FileSource {};
> class EncryptedFileSource : public FileSource {};       // one combination
> class CompressedFileSource : public FileSource {};     // another
> class EncryptedCompressedFileSource : public FileSource {}; // yet another
>
> // Decorator — compose at runtime
> auto src = make_unique<FileSource>();
> auto enc = make_unique<EncryptionDecorator>(move(src));
> auto cmp = make_unique<CompressionDecorator>(move(enc)); // compose freely
> ```
>
> **Choose Decorator when**:
> - Behaviours are optional and composable
> - Combinations are unknown at compile time
> - You need to add/remove responsibilities at runtime
>
> **Choose Inheritance when**:
> - The extension is a fundamental type distinction (a `Square` IS a `Shape`)
> - Virtual dispatch is needed for polymorphism
> - The combination space is small and fixed

---

**[Mid]** When would you use the Builder pattern instead of a constructor with many parameters?

> A **telescoping constructor** problem arises when a class has many optional parameters, leading to many overloads or a constructor with 10+ arguments:
>
> ```cpp
> // BAD — which bool is which? Easy to swap arguments silently
> Connection conn("host", 5432, "db", "user", "pass", true, false, true, 30, 10);
>
> // GOOD — Builder: named, readable, only set what you need
> auto conn = ConnectionBuilder{}
>     .host("db.prod.com")
>     .port(5432)
>     .database("mydb")
>     .credentials("user", "pass")
>     .ssl(true)
>     .timeout(30)
>     .build();
> ```
>
> **Use Builder when**:
> - Object has many optional parameters (> 4–5 is a code smell for constructors)
> - Parameters have the same type — easy to accidentally swap them
> - Construction involves validation that should fail at `build()` not incrementally
> - You want to create different "presets" via a Director
>
> **Note**: In modern C++ (C++20), **designated initializers** solve simple cases:
> ```cpp
> struct Config { std::string host; int port = 5432; bool ssl = false; };
> Config cfg { .host = "localhost", .ssl = true };  // no Builder needed for simple POD
> ```
> Use Builder when you need validation, method chaining, or the object is not a simple aggregate.

---

**[Senior]** Explain the Visitor pattern. What problem does double dispatch solve?

> **The problem**: C++ virtual dispatch is single dispatch — the method called depends on the runtime type of **one** object (the receiver). But sometimes you need dispatch on **two** types simultaneously — e.g., "call the right `visit` method based on both the visitor type AND the shape type."
>
> ```cpp
> // Single dispatch — only works for the shape's type
> void process(Shape& s) {
>     s.draw();  // dispatches on s's runtime type — OK
> }
>
> // Problem: we also want dispatch on the visitor's type
> void export_shape(Shape& s, Visitor& v) {
>     // How do we call v.visit(s) where BOTH types matter?
>     // v.visit(s) only dispatches on v's type — s is just a Shape& (base type)
> }
> ```
>
> **Double dispatch solution**: the `accept()` method on the element performs the second dispatch:
>
> ```cpp
> // First dispatch: virtual call on shape → routes to Circle::accept or Rectangle::accept
> shape->accept(visitor);
>
> // Inside Circle::accept:
> void Circle::accept(IVisitor& v) {
>     v.visit(*this);  // Second dispatch: virtual call on visitor, passing *this as Circle&
>     //                  Now BOTH the visitor type AND the shape type are resolved
> }
> ```
>
> **Trade-off**: adding a new element type (e.g., `Hexagon`) requires updating **every existing visitor** — open/closed principle is violated for elements. The pattern works best when the element hierarchy is **stable** and operations change frequently.

---

**[Senior]** You need to add logging, retry logic, and rate limiting to multiple existing service classes without modifying them. Which patterns apply, and how would you design it?

> This is a **cross-cutting concern** problem — three patterns are relevant:
>
> **1. Decorator** — wrap each service to add behaviour non-invasively:
> ```cpp
> class IPaymentService { virtual bool charge(double amount) = 0; };
>
> class LoggingDecorator : public IPaymentService {
>     std::unique_ptr<IPaymentService> inner_;
>     ILogger& logger_;
> public:
>     bool charge(double amount) override {
>         logger_.log("Charging $" + std::to_string(amount));
>         bool ok = inner_->charge(amount);
>         logger_.log(ok ? "Success" : "Failed");
>         return ok;
>     }
> };
>
> class RetryDecorator : public IPaymentService {
>     std::unique_ptr<IPaymentService> inner_;
>     int max_attempts_;
> public:
>     bool charge(double amount) override {
>         for (int i = 0; i < max_attempts_; ++i) {
>             if (inner_->charge(amount)) return true;
>             std::this_thread::sleep_for(std::chrono::seconds(1 << i)); // backoff
>         }
>         return false;
>     }
> };
>
> // Stack decorators:
> auto svc = make_unique<RateLimitDecorator>(
>              make_unique<RetryDecorator>(
>                make_unique<LoggingDecorator>(
>                  make_unique<StripePaymentService>(), logger), 3), 100);
> ```
>
> **2. Proxy** — if you want a single class that controls access (auth, rate limiting, caching) rather than layering multiple wrappers. Proxy is simpler than a decorator stack when all concerns live in one place.
>
> **3. Chain of Responsibility** — if concerns are independent and you want each to be able to short-circuit (rate limiter returns 429 without calling retry):
> ```cpp
> RateLimitHandler → RetryHandler → LoggingHandler → ActualService
> ```
>
> **Design recommendation**:
> - Use **Decorator** when behaviours compose (logging + retry + rate limit all apply, all pass through)
> - Use **Chain of Responsibility** when any link can short-circuit the rest
> - Use **Proxy** when it's one controlled gate (single concern: auth or caching)
> - With `std::function`, consider a **middleware pipeline** (like Express.js / ASP.NET middleware) — simpler than class hierarchies for simple cases:
> ```cpp
> using Handler = std::function<bool(double)>;
> Handler with_logging(Handler next, ILogger& log) {
>     return [next, &log](double amt) {
>         log.log("charging"); bool ok = next(amt); log.log(ok?"ok":"fail"); return ok;
>     };
> }
> ```
