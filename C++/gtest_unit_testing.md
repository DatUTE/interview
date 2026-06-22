# GoogleTest / gMock for C++ Unit Testing

**Session goal:** document practical GoogleTest knowledge for senior C++ interviews and production codebases: how to structure tests, isolate dependencies, run/debug failures, and connect tests with sanitizers/CI.

**Related:** [production_debugging.md](production_debugging.md) · [multithreading_and_concurrency.md](multithreading_and_concurrency.md) · [cpp_core_mobile_bridge_idl.md](cpp_core_mobile_bridge_idl.md)

---

## 1. What is GoogleTest?

**GoogleTest** (`gtest`) is a C++ unit testing framework. **GoogleMock** (`gmock`) adds mocks, matchers, and call expectations.

| Tool | Purpose |
|------|---------|
| `gtest` | Test cases, assertions, fixtures, parameterized tests |
| `gmock` | Mock interfaces, verify interactions, rich matchers |
| `ctest` | CMake test runner; runs test executables in CI |
| Sanitizers | ASan/TSan/UBSan catch runtime bugs during tests |

Typical interview answer:

> gTest verifies **behavior** at function/class boundaries. gMock isolates external dependencies such as DB, network, clock, filesystem, and Android/iOS bridge callbacks. In production C++, tests should run under ASan/UBSan regularly and TSan for concurrency-heavy paths.

---

## 2. Minimal Test

```cpp
#include <gtest/gtest.h>

int add(int a, int b) {
    return a + b;
}

TEST(MathTest, AddReturnsSum) {
    EXPECT_EQ(add(2, 3), 5);
    EXPECT_EQ(add(-1, 1), 0);
}
```

### `EXPECT_*` vs `ASSERT_*`

Both families check a condition and report a test failure, but they differ in **control flow**.

| Aspect | `EXPECT_*` | `ASSERT_*` |
|--------|------------|------------|
| Failure type | Non-fatal failure | Fatal failure |
| After failure | Test continues | Current test function returns immediately |
| Best for | Independent checks where more failure info is useful | Preconditions required for safe execution |
| Common use | Validate multiple fields in a result | Stop before dereference, indexing, parsing-dependent checks |
| Risk if overused | Test may continue after invalid state | Test may hide additional useful failures |

Rule of thumb:

- Use `ASSERT_*` for **guard conditions**: pointer not null, `optional` has value, file opened, parse succeeded, vector has enough elements.
- Use `EXPECT_*` for **business expectations**: values, flags, counts, output fields.
- In helper functions that return `void`, `ASSERT_*` can return from the helper and silently skip caller logic. Prefer returning `::testing::AssertionResult` or checking in the test body.

```cpp
auto user = repo.find_user("duy");
ASSERT_TRUE(user.has_value());       // must stop if missing
EXPECT_EQ(user->name, "duy");        // safe after ASSERT
```

Bad:

```cpp
auto p = factory.create();
EXPECT_NE(p, nullptr);
EXPECT_EQ(p->id(), 42);  // may crash if p is null
```

Better:

```cpp
auto p = factory.create();
ASSERT_NE(p, nullptr);
EXPECT_EQ(p->id(), 42);
```

Common assertions:

```cpp
EXPECT_TRUE(ok);
EXPECT_FALSE(error);
EXPECT_EQ(actual, expected);
EXPECT_NE(ptr, nullptr);
EXPECT_THROW(parse("bad"), ParseError);
EXPECT_NO_THROW(parse("good"));
EXPECT_NEAR(value, 3.14, 1e-6);
```

Custom assertion helper:

```cpp
::testing::AssertionResult IsValidId(std::string_view id) {
    if (id.empty()) {
        return ::testing::AssertionFailure() << "id is empty";
    }
    if (id.size() > 32) {
        return ::testing::AssertionFailure() << "id too long: " << id.size();
    }
    return ::testing::AssertionSuccess();
}

TEST(UserTest, GeneratesValidId) {
    EXPECT_TRUE(IsValidId(make_user_id()));
}
```

---

## 3. Fixtures

Use a fixture when multiple tests share setup.

```cpp
#include <gtest/gtest.h>
#include <memory>

class LruCacheTest : public ::testing::Test {
protected:
    void SetUp() override {
        cache = std::make_unique<LruCache<int, std::string>>(2);
    }

    std::unique_ptr<LruCache<int, std::string>> cache;
};

TEST_F(LruCacheTest, EvictsLeastRecentlyUsedItem) {
    cache->put(1, "one");
    cache->put(2, "two");
    cache->get(1);
    cache->put(3, "three");

    EXPECT_TRUE(cache->contains(1));
    EXPECT_FALSE(cache->contains(2));
    EXPECT_TRUE(cache->contains(3));
}
```

### When are `SetUp()` and `TearDown()` called?

For every `TEST_F(FixtureName, TestName)`:

1. gTest creates a **new** fixture object.
2. `SetUp()` runs **before** the test body.
3. The test body runs.
4. `TearDown()` runs **after** the test body, even if the test failed or used `ASSERT_*`.
5. The fixture object is destroyed.

```
per TEST_F:
  construct fixture -> SetUp() -> test body -> TearDown() -> destroy fixture
```

| API | Calls `SetUp()` / `TearDown()`? |
|-----|----------------------------------|
| `TEST(...)` | No fixture; neither is called |
| `TEST_F(...)` | Yes; once per test |
| `TEST_P(...)` | Yes; once per parameter value |

Important details:

- **Fresh state per test:** two `TEST_F(LruCacheTest, ...)` cases each get their own fixture instance and their own `SetUp()` call. State from test A does not leak into test B.
- **`TearDown()` always runs** after the test body when using a fixture, including on failure. Do not rely on manual cleanup only at the end of the test function.
- **Constructor vs `SetUp()`:** member initialization and constructor logic run first; put per-test preparation in `SetUp()`. Prefer RAII members (`std::unique_ptr`, temp dirs with destructors) over large `TearDown()` blocks.
- **No guaranteed order across tests:** `SetUp()`/`TearDown()` order between different tests is not part of the API contract unless you enforce it with fixtures that depend on each other (avoid that).

```cpp
class DemoTest : public ::testing::Test {
protected:
    void SetUp() override { /* runs before each TEST_F */ }
    void TearDown() override { /* runs after each TEST_F, pass or fail */ }
};

TEST_F(DemoTest, First) { /* own fixture instance */ }
TEST_F(DemoTest, Second) { /* another fresh fixture instance */ }
```

Rules of thumb:

- Keep `SetUp()` small; avoid building a hidden mini-application.
- Prefer fresh state per test; tests should not depend on execution order.
- Use `TearDown()` only when RAII is not enough.

---

## 4. Parameterized Tests

Use parameterized tests for the same behavior across many inputs.

```cpp
#include <gtest/gtest.h>

struct Case {
    std::string input;
    bool valid;
};

class EmailValidatorTest : public ::testing::TestWithParam<Case> {};

TEST_P(EmailValidatorTest, ValidatesEmailFormat) {
    const auto& c = GetParam();
    EXPECT_EQ(is_valid_email(c.input), c.valid);
}

INSTANTIATE_TEST_SUITE_P(
    BasicCases,
    EmailValidatorTest,
    ::testing::Values(
        Case{"a@b.com", true},
        Case{"missing-at", false},
        Case{"", false}));
```

Good for parsers, validators, boundary values, algorithms, and protocol framing.

---

## 5. Testing Private Logic

Do **not** test private methods directly by default. Test behavior through public API.

Better options:

1. Move complex pure logic into a small free function/class with public tests.
2. Test observable behavior through the public method.
3. Use `FRIEND_TEST` only when the design cost is lower than refactoring.

```cpp
class Parser {
    FRIEND_TEST(ParserTest, HandlesEscapedDelimiter);
    // private parse helper...
};
```

Senior answer:

> If private logic is hard to test, it may be doing too much. I first look for a smaller abstraction. I use `FRIEND_TEST` sparingly because it couples tests to implementation details and makes refactors noisy.

---

## 6. Test Doubles: Dummy, Stub, Fake, Spy, Mock

A **test double** replaces a real dependency in a test. The key distinction is whether the test double only **provides data** or also **verifies interactions**.

| Type | Purpose | Verifies calls? | Example |
|------|---------|-----------------|---------|
| **Dummy** | Passed only to satisfy a parameter | No | Unused callback object |
| **Stub** | Returns predefined answers | No | Clock always returns `1000` |
| **Fake** | Working lightweight implementation | Usually no | In-memory repository |
| **Spy** | Records calls for later inspection | Yes, manually | Vector of published events |
| **Mock** | Pre-programmed expectations | Yes, via framework | `EXPECT_CALL(repo, save(_))` |

### Dummy

A **dummy** is the simplest test double. It is passed into the system under test only because the function, constructor, or interface requires it. The test does not depend on its behavior, return values, or call count.

Use a dummy when:

- The production API requires a dependency, but the tested path should not use it.
- You need to satisfy a constructor or function signature.
- The object is irrelevant to the assertion you are making.

Example:

```cpp
class ILogger {
public:
    virtual ~ILogger() = default;
    virtual void log(std::string_view message) = 0;
};

class DummyLogger : public ILogger {
public:
    void log(std::string_view) override {}
};

TEST(PriceCalculatorTest, AppliesTax) {
    DummyLogger logger;
    PriceCalculator calculator(logger);

    EXPECT_DOUBLE_EQ(calculator.total_with_tax(100.0, 0.1), 110.0);
}
```

In this test, logging is not part of the behavior being tested. The logger only exists because `PriceCalculator` needs one. The dummy implementation does nothing, and the test should not assert anything about it.

How to write a dummy:

- Implement only the required interface methods.
- Keep the methods empty when the tested code should not use them.
- If a method must return something, return the safest neutral value, such as `false`, `0`, `nullptr`, or an empty container.
- Do not add `EXPECT_CALL`, counters, vectors, or state. Once you verify interactions, it is no longer a dummy; it becomes a spy or mock.

If the dummy unexpectedly gets used in a path where it should not be used, consider making that method fail fast:

```cpp
class DummyPaymentGateway : public IPaymentGateway {
public:
    bool charge(int cents) override {
        ADD_FAILURE() << "Payment gateway should not be used in this test";
        return false;
    }
};
```

Use this style when calling the dependency would indicate the test took the wrong branch.

### Stub

A **stub** makes the dependency deterministic. It answers queries but usually does not assert that it was called.

```cpp
class IClock {
public:
    virtual ~IClock() = default;
    virtual int64_t now_ms() const = 0;
};

class StubClock : public IClock {
public:
    explicit StubClock(int64_t now) : now_(now) {}
    int64_t now_ms() const override { return now_; }

private:
    int64_t now_;
};

TEST(SessionTest, NewSessionIsNotExpired) {
    StubClock clock(1000);
    Session s(clock, 5000);

    EXPECT_FALSE(s.expired());
}
```

Use a stub when the dependency is only an **input provider**: time, config, feature flag, environment, a successful DB lookup.

### Fake

A **fake** has real behavior but simplified storage/transport. It is often better than a mock for stateful dependencies.

```cpp
class InMemoryUserRepo : public IUserRepo {
public:
    std::optional<User> find(std::string_view id) override {
        auto it = users_.find(std::string(id));
        if (it == users_.end()) return std::nullopt;
        return it->second;
    }

    void save(User user) override {
        users_[user.id] = std::move(user);
    }

private:
    std::unordered_map<std::string, User> users_;
};
```

Use a fake when behavior matters more than exact call count, for example repository semantics, cache behavior, or retry state.

### Spy

A **spy** records what happened, then the test asserts afterward.

```cpp
class SpyEventSink : public IEventSink {
public:
    bool publish(std::string_view topic, std::string_view payload) override {
        events.push_back({std::string(topic), std::string(payload)});
        return true;
    }

    std::vector<Event> events;
};

TEST(GatewayTest, PublishesTemperatureEvent) {
    SpyEventSink sink;
    Gateway gateway(sink);

    gateway.on_temperature(36.5);

    ASSERT_EQ(sink.events.size(), 1);
    EXPECT_EQ(sink.events[0].topic, "sensor/temperature");
}
```

Use a spy when you want clear test code without tightly coupling to exact call order.

### Mock

A **mock** verifies interaction with a dependency. Use it when the important behavior is not only the final return value or state, but also **which collaborator was called, with what arguments, how many times, or in what order**.

Common mock use cases:

- A service must call `repo.save(user)` after validation succeeds.
- A gateway must publish an event exactly once.
- A retry component must call an API three times after failures.
- A command must not send email when input is invalid.

Conceptually:

```cpp
TEST(UserServiceTest, SavesUserAfterValidation) {
    MockUserRepository repo;
    UserService service(repo);

    EXPECT_CALL(repo, save(UserHasEmail("a@example.com")))
        .Times(1);

    service.register_user("a@example.com");
}
```

This test cares about the collaboration between `UserService` and `UserRepository`. The expectation says: when `register_user()` runs, `save()` must be called once with the right user.

Use a mock when:

- The dependency has a side effect: database write, event publish, email send, HTTP request.
- The result is difficult or impossible to observe directly.
- Calling or not calling the dependency is part of the required behavior.
- You need to verify retries, call count, argument values, or call order.

Avoid mocks when:

- A simple return value is enough; use a stub.
- You need realistic stateful behavior; use a fake.
- You only want to inspect what happened after the call; a spy may be simpler.
- The test becomes too coupled to implementation details.

In short: a **stub gives answers**, a **fake behaves like a small real implementation**, a **spy records what happened**, and a **mock sets expectations before the action and verifies them during the test**.

---

## 7. gMock Basics

Design production code around interfaces when behavior crosses boundaries. A **mock** is best when the collaboration itself is part of the behavior: "must save exactly once", "must not publish on validation failure", "must retry twice".

```cpp
class IEventSink {
public:
    virtual ~IEventSink() = default;
    virtual bool publish(std::string_view topic, std::string_view payload) = 0;
};

class MockEventSink : public IEventSink {
public:
    MOCK_METHOD(bool, publish, (std::string_view topic, std::string_view payload), (override));
};

TEST(GatewayTest, PublishesTemperatureEvent) {
    using ::testing::HasSubstr;
    using ::testing::Return;
    using ::testing::StrictMock;

    StrictMock<MockEventSink> sink;
    EXPECT_CALL(sink, publish("sensor/temperature", HasSubstr("36.5")))
        .Times(1)
        .WillOnce(Return(true));

    Gateway gateway(sink);
    gateway.on_temperature(36.5);
}
```

Useful matchers/actions:

```cpp
using ::testing::_;
using ::testing::Return;
using ::testing::StrictMock;

EXPECT_CALL(repo, save(_)).Times(1).WillOnce(Return(true));
```

| Mock style | Meaning |
|------------|---------|
| `NiceMock<T>` | Ignore uninteresting calls |
| `NaggyMock<T>` | Warn on uninteresting calls (default-ish behavior) |
| `StrictMock<T>` | Fail on unexpected calls |

Prefer `StrictMock` for small unit tests where unexpected calls signal a real bug.

### `ON_CALL` vs `EXPECT_CALL`

| API | Meaning | Use when |
|-----|---------|----------|
| `ON_CALL(mock, method(...)).WillByDefault(...)` | Default behavior | Test needs a dependency response but call count is not the point |
| `EXPECT_CALL(mock, method(...))` | Verification + optional behavior | The call itself is part of the requirement |

```cpp
ON_CALL(repo, find("u1")).WillByDefault(Return(User{"u1"}));
EXPECT_CALL(audit, record_login("u1")).Times(1);
```

Senior rule:

> Stub default answers with `ON_CALL`; verify meaningful interactions with `EXPECT_CALL`. Avoid asserting every internal call because it makes tests brittle.

---

## 8. Mock vs Stub: Decision Guide

```
Need dependency to return stable data?
  -> Stub

Need lightweight real behavior and state?
  -> Fake

Need to inspect emitted events afterward?
  -> Spy

Need to enforce exact interaction contract?
  -> Mock
```

| Situation | Prefer | Why |
|-----------|--------|-----|
| Expiry logic depends on time | Stub clock | Deterministic input |
| Cache uses storage-like API | Fake repository | Natural stateful behavior |
| Event publisher output is the result | Spy sink | Assert produced events |
| Validation failure must not save | Mock repository | Interaction contract matters |
| Retry logic must call service 3 times | Mock service | Call count/order matters |
| Parsing pure function | No double | Just assert input/output |

Interview phrasing:

> A stub controls **inputs**. A mock verifies **interactions**. A fake provides a lightweight working implementation. I prefer the simplest double that proves the behavior; too many mocks make tests fragile.

---

## 9. Testing C++ Core Boundaries

### Network / DB / Filesystem

```cpp
class IEventSink {
public:
    virtual ~IEventSink() = default;
    virtual bool publish(std::string_view topic, std::string_view payload) = 0;
};
```

Test the core with a test double (`Stub`, `Spy`, or `Mock`) for `IEventSink`, not a real MQTT broker or gRPC server.

Integration tests can start real services later, but unit tests should be fast and deterministic.

### Protobuf Golden Test

For stable wire formats:

```cpp
TEST(BridgeProtoTest, EncodesStableGoldenBytes) {
    BridgeRequest req;
    req.set_request_id("r1");
    req.set_payload("hello");

    std::string bytes;
    ASSERT_TRUE(req.SerializeToString(&bytes));
    EXPECT_EQ(to_hex(bytes), "0a027231120568656c6c6f");
}
```

Use golden bytes carefully: they catch wire compatibility regressions but can become brittle if the schema is still changing.

---

## 10. CMake Setup

### FetchContent

```cmake
include(FetchContent)

FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE) # Windows/MSVC
FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(core_tests
    cache_test.cpp
    parser_test.cpp
)

target_link_libraries(core_tests
    PRIVATE
        core_lib
        GTest::gtest_main
        GTest::gmock
)

include(GoogleTest)
gtest_discover_tests(core_tests)
```

### Running

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

Run a single test:

```bash
./build/core_tests --gtest_filter=LruCacheTest.EvictsLeastRecentlyUsedItem
```

Repeat a flaky test:

```bash
./build/core_tests --gtest_filter=RaceTest.* --gtest_repeat=1000 --gtest_break_on_failure
```

---

## 11. Tests + Sanitizers

Recommended CI matrix:

| Job | Purpose |
|-----|---------|
| Debug + gTest | Fast feedback |
| ASan + UBSan | Use-after-free, overflow, UB |
| TSan | Data races; often separate because it is slower |
| Release smoke | Catch optimizer-dependent issues |

Example flags:

```cmake
target_compile_options(core_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
target_link_options(core_tests PRIVATE -fsanitize=address,undefined)
```

For TSan:

```cmake
target_compile_options(core_tests PRIVATE -fsanitize=thread -fno-omit-frame-pointer)
target_link_options(core_tests PRIVATE -fsanitize=thread)
```

Do not mix ASan and TSan in the same binary.

---

## 12. Flaky Test Playbook

When a gTest fails only sometimes:

1. Run with `--gtest_repeat=1000 --gtest_break_on_failure`.
2. Add deterministic clocks/random seeds.
3. Remove sleeps as synchronization; use condition variables, latches, futures.
4. Run under TSan.
5. Log thread IDs and state transitions.
6. Check test isolation: temp files, ports, global singletons, shared mocks.

Bad:

```cpp
std::this_thread::sleep_for(std::chrono::milliseconds(50));
EXPECT_TRUE(worker.done());
```

Better:

```cpp
ASSERT_TRUE(done.wait_for(std::chrono::seconds(1)));
EXPECT_TRUE(worker.done());
```

---

## 13. Anti-patterns

| Anti-pattern | Why it hurts | Better |
|--------------|--------------|--------|
| Testing only happy path | Misses boundary bugs | Add invalid input, empty, max-size, timeout |
| Overusing mocks | Tests implementation, not behavior | Use stubs/fakes/spies unless call contract matters |
| Confusing stub with mock | Tests become too strict | Stub answers data; mock verifies calls |
| Sleeping in tests | Flaky and slow | Use deterministic synchronization |
| Shared global state | Order-dependent failures | Fresh fixture per test |
| Ignoring sanitizer tests | Memory/race bugs escape | Run ASan/TSan in CI |
| Huge fixture setup | Hard to debug | Split into smaller units |

---

## 14. Interview Q&A

**[Mid]** What is the difference between `EXPECT_*` and `ASSERT_*`?

> `EXPECT_*` records a **non-fatal** failure and continues the test. `ASSERT_*` records a **fatal** failure and returns from the current test function. Use `ASSERT_*` for preconditions where continuing is unsafe, for example null pointer checks, parse success, or vector size before indexing. Use `EXPECT_*` for independent result fields so one test run gives more diagnostic information.

---

**[Mid]** How do you test code that uses a database or network?

> Put the DB/network behind an interface and inject it. Unit tests use gMock to verify behavior without real I/O. Integration tests can use a real DB/broker/server separately. This keeps unit tests fast, deterministic, and suitable for CI.

---

**[Mid]** What is the difference between a stub and a mock?

> A **stub** provides predefined responses so the code under test can run deterministically. It usually does not care whether it was called. A **mock** verifies interactions: expected method, arguments, count, sometimes order. Example: a fixed clock is a stub; a repository that must receive exactly one `save()` call is a mock.

---

**[Mid]** What are fakes and spies?

> A **fake** is a lightweight working implementation, like an in-memory repository instead of PostgreSQL. A **spy** records calls or events for later assertions, like collecting published MQTT topics in a vector. They are often less brittle than strict mocks when exact call order is not part of the behavior.

---

**[Mid]** What is a test fixture?

> A fixture is a class derived from `::testing::Test` that provides shared setup and state for multiple tests. gTest constructs a fresh fixture object per test, so state should not leak between tests.

---

**[Senior]** When are mocks harmful?

> Mocks are harmful when they verify internal implementation details instead of observable behavior. If every refactor breaks tests even though behavior is unchanged, the tests are too coupled. Use mocks mainly for meaningful external interactions: "must publish once", "must not write to DB", "must retry three times". For input-only dependencies, use stubs; for stateful behavior, use fakes.

---

**[Senior]** How do you decide between `ON_CALL` and `EXPECT_CALL`?

> Use `ON_CALL` to define default stub behavior when the call itself is not being tested. Use `EXPECT_CALL` when the interaction is part of the requirement. Overusing `EXPECT_CALL` for every dependency call makes tests brittle because implementation refactors change call patterns without changing behavior.

---

**[Senior]** How would you test a rare race condition?

> First write a focused stress test with many iterations using `--gtest_repeat`. Replace sleeps with deterministic synchronization so the test is not accidentally flaky. Run under TSan, add logging around happens-before edges, and reduce shared global state. Once fixed, keep the regression test and add a sanitizer CI job if the area is concurrency-heavy.

---

**[Senior]** Should private methods be tested directly?

> Usually no. Test public behavior. If private logic is complex enough to need direct tests, extract it into a smaller public component or pure function. `FRIEND_TEST` is acceptable for exceptional cases, but it couples tests to implementation details.

---

## 15. Quick Commands

```bash
# Run all tests
ctest --test-dir build --output-on-failure

# Run only one suite
./build/core_tests --gtest_filter=ParserTest.*

# Repeat until failure
./build/core_tests --gtest_repeat=1000 --gtest_break_on_failure

# List tests
./build/core_tests --gtest_list_tests

# Shuffle order to detect hidden coupling
./build/core_tests --gtest_shuffle --gtest_repeat=100
```

