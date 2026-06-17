# STL Containers & Data Structures in C++

> **CPU cache & sequential scan (low-latency):** [low_latency.md](low_latency.md) § CPU Cache & Sequential Containers · §10 bên dưới

---

## Container Categories

```
STL Containers
├── Sequence          — ordered by position
│   ├── std::array          fixed-size contiguous
│   ├── std::vector         dynamic contiguous
│   ├── std::deque          double-ended dynamic
│   ├── std::list           doubly-linked list
│   └── std::forward_list   singly-linked list
│
├── Associative       — ordered by key (Red-Black Tree)
│   ├── std::set / std::multiset
│   └── std::map / std::multimap
│
├── Unordered         — hashed, no ordering
│   ├── std::unordered_set / std::unordered_multiset
│   └── std::unordered_map / std::unordered_multimap
│
└── Adaptors          — restrict interface of another container
    ├── std::stack          (default: deque)
    ├── std::queue          (default: deque)
    └── std::priority_queue (default: vector)
```

---

## Complexity Cheat Sheet

| Container | Access | Search | Insert (mid) | Insert (end) | Insert (begin) | Delete (mid) |
|---|---|---|---|---|---|---|
| `array` | O(1) | O(n) | — | — | — | — |
| `vector` | O(1) | O(n) | O(n) | **O(1)** amort. | O(n) | O(n) |
| `deque` | O(1) | O(n) | O(n) | **O(1)** amort. | **O(1)** amort. | O(n) |
| `list` | O(n) | O(n) | **O(1)** | **O(1)** | **O(1)** | **O(1)** |
| `forward_list` | O(n) | O(n) | **O(1)** after pos | **O(1)** | **O(1)** | **O(1)** |
| `set`/`map` | O(log n) | O(log n) | O(log n) | O(log n) | O(log n) | O(log n) |
| `unordered_set`/`map` | O(1) avg | O(1) avg | O(1) avg | O(1) avg | O(1) avg | O(1) avg |
| `priority_queue` | O(1) top | — | O(log n) | O(log n) | — | O(log n) |

---

## 1. `std::array`

### Theory

A **fixed-size contiguous array** — a thin wrapper around a C array. Size is part of the type and known at compile time. Lives on the stack (unless explicitly heap-allocated). No heap allocation, no dynamic resizing, no size overhead.

```
Memory layout (array<int,4>):
┌────┬────┬────┬────┐
│  1 │  2 │  3 │  4 │   ← contiguous, on stack
└────┴────┴────┴────┘
```

### Usage

```cpp
#include <array>

std::array<int, 4> a = {1, 2, 3, 4};

// Access
a[0];           // O(1) — no bounds check
a.at(0);        // O(1) — throws std::out_of_range if OOB
a.front();      // first element
a.back();       // last element
a.data();       // raw pointer to first element

// Size
a.size();       // 4 — constexpr
a.empty();      // false

// Iteration
for (auto x : a) { }
for (auto& x : a) { x *= 2; }

// Algorithms work directly
std::sort(a.begin(), a.end());
std::fill(a.begin(), a.end(), 0);

// Multi-dimensional
std::array<std::array<int,3>, 3> matrix;
matrix[1][2] = 5;
```

### When to Use

- Fixed-size collection known at compile time (lookup table, ring buffer, fixed-size frame)
- Replace C arrays `int arr[N]` — same performance, bounds checking with `.at()`, works with STL algorithms
- Stack-allocated — when heap overhead matters

### Gotchas

```cpp
std::array<int, 4> a;
std::array<int, 5> b;
// a = b;  ← compile error — different types (size is part of type)

// Unlike vector, array<int,0> is valid but has no elements
std::array<int, 0> empty;
empty.data();  // may return nullptr or unique address — implementation-defined
```

### `std::array` & cache

- Kích thước **compile-time** — thường trên **stack** hoặc embedded trong struct/class.  
- Layout **contiguous** giống `vector` → cùng lợi ích spatial locality khi `for (x : a)`.  
- Low-latency: ring buffer cố định, lookup table, small batch trên stack (tránh stack overflow).

```cpp
std::array<float, 64> coeffs;  // 256 B — vài cache lines, all in L1 after warm-up
for (float c : coeffs)
    y += c * x;
```

---

## 2. `std::vector`

### Theory

A **dynamic contiguous array** — the most used STL container. Stores elements in a single heap-allocated block. When capacity is exceeded, it allocates a new larger block (typically **2× growth factor**), copies/moves all elements, and frees the old block.

```
Memory layout:
┌────┬────┬────┬────┬────┬────┐
│  1 │  2 │  3 │  4 │    │    │   heap block
└────┴────┴────┴────┴────┴────┘
 ▲                   ▲         ▲
 data()              size=4    capacity=6

vector object (on stack or wherever declared):
┌──────────┬──────────┬──────────┐
│  data*   │  size=4  │ cap=6   │  24 bytes (3 pointers on 64-bit)
└──────────┴──────────┴──────────┘
```

### Usage

```cpp
#include <vector>

std::vector<int> v;              // empty
std::vector<int> v(5);           // 5 elements, value-initialized to 0
std::vector<int> v(5, 42);       // 5 elements, all 42
std::vector<int> v = {1,2,3,4};  // initializer list

// Adding / removing
v.push_back(5);                  // O(1) amortized — append
v.pop_back();                    // O(1) — remove last
v.insert(v.begin() + 2, 99);    // O(n) — shift elements right
v.erase(v.begin() + 2);         // O(n) — shift elements left
v.clear();                       // O(n) — destroys all elements

// Emplace — construct in-place, avoids temporary
v.emplace_back(42);              // prefer over push_back for complex types
v.emplace(v.begin(), 0);         // insert at position, construct in-place

// Access
v[2];           // O(1) — no bounds check (UB if OOB)
v.at(2);        // O(1) — throws if OOB
v.front();      // first element
v.back();       // last element
v.data();       // raw pointer — safe to pass to C APIs

// Size / capacity
v.size();       // number of elements
v.capacity();   // allocated slots
v.empty();

// Memory management
v.reserve(100);     // pre-allocate for 100 elements — avoids reallocations
v.shrink_to_fit();  // release excess capacity (non-binding hint)
v.resize(10);       // change size (fills new slots with 0)
v.resize(10, 99);   // fill new slots with 99
```

### Reallocation and Iterator Invalidation

```cpp
std::vector<int> v = {1, 2, 3};
int* ptr = v.data();           // raw pointer to internal buffer

v.push_back(4);                // may reallocate!
// ptr may now be dangling — buffer was freed and replaced

// Same for iterators and references:
auto it = v.begin();
v.push_back(5);                // it is potentially invalidated
// *it  → undefined behavior if reallocation occurred

// Safe: reserve upfront to prevent reallocation
v.reserve(100);
auto it = v.begin();
v.push_back(4);                // safe — capacity was sufficient, no reallocation
```

**Operations that invalidate iterators**:
- `push_back` / `emplace_back` — if `size == capacity`
- `insert` / `emplace` — always for elements after the insertion point
- `erase` — always for elements after the erased position
- `resize`, `reserve` (if causes reallocation), `clear`

### `emplace_back` vs `push_back`

```cpp
struct Point { int x, y; Point(int x, int y) : x(x), y(y) {} };

std::vector<Point> v;

v.push_back(Point{1, 2});    // constructs temp Point, then moves into vector
v.push_back({1, 2});         // same — braced init creates temp

v.emplace_back(1, 2);        // constructs Point DIRECTLY in vector's storage
                              // no temporary, no move — more efficient
```

### Use Cases

- Default choice for a dynamic collection of elements
- When cache performance matters — contiguous layout is cache-friendly
- Passing data to C APIs (`v.data()`, `v.size()`)
- As a stack (push_back / pop_back)

### Cache & sequential access (`vector`)

`std::vector` tối ưu **duyệt tuần tự** — core của nhiều hot path low-latency.

```
data() →  [0][1][2]...[15] | [16]...
           └── cache line 64B ──┘
for (auto x : v)  → spatial locality + hardware prefetch
```

| Yếu tố | Ảnh hưởng |
|--------|-----------|
| Contiguous | Một cache miss → nhiều phần tử |
| `reserve(n)` | Không realloc giữa hot loop |
| Small `T` | Nhiều phần tử / cache line |

```cpp
std::vector<int> v;
v.reserve(n);
for (std::size_t i = 0; i < n; ++i)
    v.emplace_back(...);
for (int x : v)
    process(x);   // prefer scan-only on hot path
```

→ So sánh đầy đủ vector/array/list: **§10 Cache Locality**

---

## 3. `std::deque`

### Theory

A **double-ended queue** — supports O(1) amortized insert and remove at **both ends**. Internally implemented as a sequence of fixed-size **chunks** (typically 512 bytes each), managed by a map of pointers. Not fully contiguous — `data()` does not exist.

```
Memory layout:
chunk 0    chunk 1    chunk 2
┌──────┐   ┌──────┐   ┌──────┐
│A B C │   │D E F │   │G H I │
└──────┘   └──────┘   └──────┘
     ▲                 ▲
     front             back

Map of chunk pointers: [ptr0, ptr1, ptr2]
```

### Usage

```cpp
#include <deque>

std::deque<int> dq = {1, 2, 3};

dq.push_back(4);     // O(1) amortized — append to back
dq.push_front(0);    // O(1) amortized — prepend to front (vector can't do this!)
dq.pop_back();       // O(1)
dq.pop_front();      // O(1)

dq[2];               // O(1) random access (slightly slower than vector — extra indirection)
dq.at(2);            // O(1) with bounds check

dq.insert(dq.begin() + 1, 99);   // O(n) — shifts elements
dq.erase(dq.begin() + 1);        // O(n)
```

### Use Cases

- **Sliding window** — add to back, remove from front: queue-like usage
- **Browser history** — efficient prepend and append
- **BFS queue** — `push_back` to enqueue, `pop_front` to dequeue
- Use when you need O(1) at both ends but also random access
- `std::queue` and `std::stack` use `deque` as their default underlying container

### vs `vector`

| | `vector` | `deque` |
|---|---|---|
| Memory | Contiguous | Chunked |
| `push_front` | O(n) | **O(1)** |
| `push_back` | O(1) amort. | O(1) amort. |
| Random access | O(1) fastest | O(1) + indirection |
| Cache friendliness | Excellent | Good within chunk |
| Iterator invalidation | On realloc | On push_front/back (pointers stable, iterators not) |

---

## 4. `std::list`

### Theory

A **doubly-linked list**. Each node is individually heap-allocated and contains the value plus two pointers (prev, next). No contiguous memory — terrible cache performance. Provides O(1) insert and erase at **any known position** without moving other elements.

```
Memory layout:
┌───────────┐   ┌───────────┐   ┌───────────┐
│prev│ 1 │next│─►│prev│ 2 │next│─►│prev│ 3 │next│
└───────────┘   └───────────┘   └───────────┘
 scattered across the heap — not contiguous
```

### Usage

```cpp
#include <list>

std::list<int> lst = {1, 2, 3, 4};

lst.push_back(5);    // O(1)
lst.push_front(0);   // O(1)
lst.pop_back();      // O(1)
lst.pop_front();     // O(1)

// Insert at iterator position — O(1) (no shifting)
auto it = std::find(lst.begin(), lst.end(), 3);
lst.insert(it, 99);  // inserts 99 before element 3

// Erase at iterator — O(1)
lst.erase(it);       // iterator it was pointing to 3 — now erased

// No random access — must iterate to find position: O(n)
// lst[2];  ← doesn't exist!

// List-specific algorithms (more efficient than generic versions)
lst.sort();                  // O(n log n) — merge sort, stable
lst.reverse();               // O(n)
lst.unique();                // remove consecutive duplicates: O(n)
lst.merge(other_lst);        // merge two sorted lists: O(n)
lst.splice(it, other_lst);   // move elements from other_lst — O(1)! No copies
```

### Iterator Stability

```cpp
std::list<int> lst = {1, 2, 3, 4, 5};
auto it = std::find(lst.begin(), lst.end(), 3);

lst.push_back(6);    // it remains valid — list never reallocates
lst.erase(lst.begin()); // it still valid — only the erased node's iterator is invalid
lst.insert(it, 99); // it still valid — pointing to 3, 99 inserted before it
```

**Iterators, pointers, and references to list nodes are never invalidated** except when the node itself is erased. This is the key advantage over vector.

### Use Cases

- **Frequent insertion/deletion in the middle** at a known position (e.g., LRU cache eviction)
- **Stable iterators** required — you hold an iterator and other threads/operations modify the list
- `splice` operations — move elements between lists in O(1) with no copying

### When NOT to Use

Despite O(1) insert, `std::list` is often **slower than `std::vector`** in practice due to:
- Cache misses per node traversal (each node is a separate heap allocation)
- High memory overhead (2 pointers = 16 bytes overhead per element on 64-bit)
- No SIMD vectorization possible

```cpp
// Benchmark reality: for n=10000, vector find + erase is often FASTER than list
// because vector's sequential scan fits in cache; list's scattered nodes do not
// See: low_latency.md § CPU Cache & Sequential Containers
```

---

## 5. `std::forward_list`

### Theory

A **singly-linked list** — each node has only a `next` pointer. More memory-efficient than `std::list` (one pointer overhead instead of two). Extremely limited interface — no `size()`, no backward traversal, no `push_back`.

```cpp
#include <forward_list>

std::forward_list<int> fl = {1, 2, 3};

fl.push_front(0);           // O(1) — only front insertion
fl.pop_front();             // O(1)

// Insert after a position (no insert-before without an extra traversal)
auto it = fl.before_begin(); // sentinel iterator before first element
fl.insert_after(it, 99);     // inserts 99 at front
fl.erase_after(it);          // erases element after it

// No random access, no size(), no push_back
```

### Use Cases

- **Memory-critical embedded systems** — minimal overhead per node
- **Intrusive linked lists** — where the node pointer is embedded in the data structure itself
- Implementing simple stacks without the overhead of `std::list`

---

## 6. `std::set` and `std::multiset`

### Theory

**Ordered set** backed by a **Red-Black Tree** (self-balancing BST). All elements are sorted by the comparator (default: `std::less<T>`). Every operation is O(log n). Elements are immutable once inserted (modifying would break ordering).

```
Red-Black Tree for {1, 3, 5, 7, 9}:

             5 (black)
           /           \
       3 (red)         7 (red)
      /    \          /    \
  1(black) 4(black) 6(black) 9(black)

Properties: balanced, height ≤ 2 log(n+1)
```

### Usage

```cpp
#include <set>

std::set<int> s = {5, 3, 1, 4, 2};
// Internally stored as: {1, 2, 3, 4, 5}  — always sorted

// Insert
s.insert(6);              // O(log n) — returns pair<iterator, bool>
auto [it, ok] = s.insert(3);  // ok=false — 3 already exists in set

// Lookup
s.count(3);               // O(log n) — 0 or 1 for set
s.contains(3);            // O(log n) — C++20
s.find(3);                // O(log n) — returns iterator or end()

// Erase
s.erase(3);               // O(log n) — erase by value
s.erase(it);              // O(1) amortized — erase by iterator

// Bounds
s.lower_bound(3);         // iterator to first element >= 3
s.upper_bound(3);         // iterator to first element > 3
s.equal_range(3);         // pair of [lower_bound, upper_bound]

// Iteration — always in sorted order
for (auto x : s) std::cout << x << " ";   // 1 2 3 4 5 6

// Custom comparator
std::set<int, std::greater<int>> desc_set = {3, 1, 4, 1, 5};
// Stored as: {5, 4, 3, 1}  (descending, duplicates removed)
```

### `multiset` — Allows Duplicates

```cpp
std::multiset<int> ms = {3, 1, 4, 1, 5, 9, 2, 6, 5};
// Stored: {1, 1, 2, 3, 4, 5, 5, 6, 9}

ms.count(5);              // returns 2
ms.erase(5);              // erases ALL occurrences of 5
ms.erase(ms.find(5));     // erases ONE occurrence of 5
```

### Use Cases

- **Maintaining a sorted unique collection** with fast insert/lookup/delete
- **Order statistics** — k-th element, rank of element (with `std::distance`, or policy-based trees)
- **Sliding window median** — two multisets (lower half, upper half) with O(log n) rebalance
- **Event scheduling** — sorted by time, process in order

---

## 7. `std::map` and `std::multimap`

### Theory

**Ordered key-value store** backed by a Red-Black Tree, sorted by key. Keys are unique in `map`, duplicated in `multimap`. Each node stores a `std::pair<const Key, Value>`.

### Usage

```cpp
#include <map>

std::map<std::string, int> scores;

// Insert
scores["Alice"] = 95;                      // operator[] — inserts with default value if missing!
scores.insert({"Bob", 87});                // insert — no effect if key exists
scores.emplace("Charlie", 92);            // construct in-place
scores.insert_or_assign("Alice", 100);    // C++17 — always updates

// Lookup
scores["Alice"];          // 95 — WARNING: inserts default if key missing!
scores.at("Alice");       // 95 — throws std::out_of_range if missing (safe)
scores.find("Dave");      // returns end() if not found — safe
scores.contains("Alice"); // C++20 — bool

// Erase
scores.erase("Bob");      // by key — O(log n)
scores.erase(it);         // by iterator — O(1) amortized

// Iteration — always in key-sorted order
for (auto& [name, score] : scores) {      // structured binding (C++17)
    std::cout << name << ": " << score << "\n";
}

// Bounds — useful for range queries
auto it = scores.lower_bound("B");    // first entry >= "B" lexicographically
auto end = scores.upper_bound("D");   // first entry > "D"
for (auto i = it; i != end; ++i)
    std::cout << i->first << "\n";    // all names in [B, D]
```

### `operator[]` Trap

```cpp
std::map<std::string, int> m;
int x = m["missing_key"];   // INSERTS "missing_key" with value 0!
                             // m is no longer empty — m.size() == 1

// Safe read:
if (auto it = m.find("missing_key"); it != m.end())
    int x = it->second;

// Or (C++20):
if (m.contains("missing_key"))
    int x = m["missing_key"];
```

### Use Cases

- **Frequency counting**: `map<string, int>` — `++word_count[word]`
- **Index / inverted index**: `map<string, vector<int>>` — word → list of line numbers
- **Interval/range queries**: sorted keys enable `lower_bound`/`upper_bound` for range searches
- **Config/settings storage**: string key → variant value, always iterated in sorted key order

---

## 8. `std::unordered_map` and `std::unordered_set`

### Theory

**Hash table** backed by an array of **buckets**. Each bucket is a linked list of entries with the same hash. Average O(1) for all operations; worst case O(n) when all keys collide into the same bucket.

```
Hash table layout:
bucket[0]: → nullptr
bucket[1]: → {key="Alice", val=95} → nullptr
bucket[2]: → {key="Bob", val=87} → {key="Dave", val=73} → nullptr  (collision!)
bucket[3]: → nullptr
bucket[4]: → {key="Charlie", val=92} → nullptr
...
bucket[N-1]: → nullptr

Load factor = num_elements / num_buckets
Rehash (doubles buckets) when load_factor > max_load_factor (default 1.0)
```

### Usage

```cpp
#include <unordered_map>

std::unordered_map<std::string, int> um;

// Same interface as map for insert/lookup/erase
um["Alice"] = 95;
um.insert({"Bob", 87});
um.find("Alice");            // O(1) average
um.count("Alice");           // O(1) average
um.erase("Bob");             // O(1) average

// Performance tuning
um.reserve(1000);            // pre-allocate for 1000 elements — avoids rehashing
um.max_load_factor(0.5);     // rehash sooner — fewer collisions, more memory
um.load_factor();            // current ratio: elements / buckets
um.bucket_count();           // number of buckets
um.bucket("Alice");          // which bucket "Alice" hashes to

// Iteration order: UNDEFINED — changes after rehash or between runs
for (auto& [k, v] : um) { }   // any order
```

### Custom Hash for User Types

```cpp
struct Point { int x, y; };

// Option 1: specialize std::hash
template<>
struct std::hash<Point> {
    size_t operator()(const Point& p) const noexcept {
        size_t hx = std::hash<int>{}(p.x);
        size_t hy = std::hash<int>{}(p.y);
        return hx ^ (hy << 32 | hy >> 32);   // combine hashes
    }
};

std::unordered_set<Point> points;

// Option 2: provide a hasher type
struct PointHash {
    size_t operator()(const Point& p) const noexcept {
        return std::hash<long long>{}((long long)p.x << 32 | p.y);
    }
};
struct PointEq {
    bool operator()(const Point& a, const Point& b) const {
        return a.x == b.x && a.y == b.y;
    }
};
std::unordered_set<Point, PointHash, PointEq> points;
```

### `map` vs `unordered_map`

| | `std::map` | `std::unordered_map` |
|---|---|---|
| **Underlying structure** | Red-Black Tree | Hash table |
| **Ordering** | Sorted by key | No ordering |
| **Lookup** | O(log n) | O(1) avg |
| **Insert** | O(log n) | O(1) avg |
| **Worst case** | O(log n) | O(n) (all collide) |
| **Memory per entry** | 3 pointers + color + data | 1 pointer + data |
| **Cache friendliness** | Poor (pointer chasing) | Better (arrays of buckets) |
| **Range queries** | ✅ lower_bound/upper_bound | ❌ not supported |
| **Custom key requirements** | `operator<` or comparator | `operator==` + hash |
| **Iteration order** | Stable, sorted | Undefined |
| **Use when** | Need ordering, range queries | Need max lookup speed |

### Use Cases

- **Frequency map / memoization cache**: O(1) lookup
- **Graph adjacency list**: `unordered_map<int, vector<int>>`
- **Deduplication**: `unordered_set` as a seen-set
- **Two-sum / complement lookup**: `unordered_map<int, int>` for O(n) total

---

## 9. Container Adaptors

Adaptors wrap a container and restrict its interface to a specific data structure contract.

### `std::stack`

LIFO — last in, first out. Default underlying container: `std::deque`.

```cpp
#include <stack>

std::stack<int> st;            // default: deque underneath
std::stack<int, std::vector<int>> st; // use vector for better cache performance

st.push(1);    // push onto top
st.push(2);
st.top();      // 2 — peek at top without removing
st.pop();      // remove top (returns void! — use top() first)
st.empty();
st.size();

// Common use: DFS, expression parsing, undo history
// Classic: balanced parentheses check
bool is_balanced(const std::string& s) {
    std::stack<char> st;
    for (char c : s) {
        if (c == '(' || c == '[' || c == '{') st.push(c);
        else {
            if (st.empty()) return false;
            char top = st.top(); st.pop();
            if ((c==')' && top!='(') || (c==']' && top!='[') || (c=='}' && top!='{'))
                return false;
        }
    }
    return st.empty();
}
```

### `std::queue`

FIFO — first in, first out. Default underlying container: `std::deque`.

```cpp
#include <queue>

std::queue<int> q;

q.push(1);     // enqueue at back
q.push(2);
q.front();     // 1 — peek at front
q.back();      // 2 — peek at back
q.pop();       // dequeue from front (returns void!)
q.empty();
q.size();

// Classic use: BFS
void bfs(int start, const std::vector<std::vector<int>>& adj) {
    std::vector<bool> visited(adj.size(), false);
    std::queue<int> q;
    q.push(start);
    visited[start] = true;

    while (!q.empty()) {
        int node = q.front(); q.pop();
        for (int neighbor : adj[node]) {
            if (!visited[neighbor]) {
                visited[neighbor] = true;
                q.push(neighbor);
            }
        }
    }
}
```

### `std::priority_queue`

Max-heap by default — `top()` returns the largest element. Default underlying container: `std::vector`.

```cpp
#include <queue>

// Max-heap (default)
std::priority_queue<int> pq;
pq.push(3); pq.push(1); pq.push(4); pq.push(1); pq.push(5);
pq.top();    // 5 — largest element
pq.pop();    // removes 5

// Min-heap — use greater<int>
std::priority_queue<int, std::vector<int>, std::greater<int>> min_pq;
min_pq.push(3); min_pq.push(1); min_pq.push(4);
min_pq.top();   // 1 — smallest element

// Custom comparator for pairs — sort by second element descending
using Item = std::pair<int, int>;  // {priority, value}
std::priority_queue<Item> task_queue;
task_queue.push({5, 100});   // priority 5
task_queue.push({1, 200});   // priority 1
task_queue.top();            // {5, 100} — highest priority first

// Classic: Dijkstra's shortest path
void dijkstra(int src, const std::vector<std::vector<std::pair<int,int>>>& adj) {
    int n = adj.size();
    std::vector<int> dist(n, INT_MAX);
    std::priority_queue<std::pair<int,int>,
                        std::vector<std::pair<int,int>>,
                        std::greater<>> pq;  // min-heap: {dist, node}
    dist[src] = 0;
    pq.push({0, src});

    while (!pq.empty()) {
        auto [d, u] = pq.top(); pq.pop();
        if (d > dist[u]) continue;   // stale entry
        for (auto [w, v] : adj[u]) {
            if (dist[u] + w < dist[v]) {
                dist[v] = dist[u] + w;
                pq.push({dist[v], v});
            }
        }
    }
}
```

---

## 10. Cache Locality — Sequence Containers (vector, array, list, deque)

### Tại sao CPU cache quan trọng hơn Big-O insert?

Thuật toán textbooks đếm **phép so sánh**. CPU thực tế đếm **cycles chờ RAM**. Duyệt `n` phần tử:

```
vector scan:  ~ n / (elements_per_cache_line)  memory accesses (sau warm L1)
list scan:    ~ n  pointer chases, often n cache misses
```

**Elements per cache line (64B):**

| `T` | ~count/line |
|-----|-------------|
| `int` | 16 |
| `double` | 8 |
| `struct` 32B | 2 |
| `struct` 128B | 0.5 (mỗi phần tử > 1 line) |

Struct **phình to** → ít phần tử/line → cân nhắc SoA ([low_latency.md](low_latency.md)).

### So sánh duyệt tuần tự

| Container | Memory layout | Sequential `for` | Prefetcher | SIMD |
|-----------|---------------|------------------|------------|------|
| **`std::array`** | Contiguous fixed | ★★★★ | ★★★★ | ★★★★ |
| **`std::vector`** | Contiguous heap | ★★★★ | ★★★★ | ★★★★ |
| **`std::deque`** | Chunked contiguous | ★★★ | ★★★ | ★★ |
| **`std::list`** | Scattered nodes | ★ | ★ | ✗ |
| **`forward_list`** | Scattered singly | ★ | ★ | ✗ |

### Khi nào vẫn chọn `list`?

- Iterator **stable** khi insert/erase giữa chừng (iterator không invalidate như vector)  
- Phần tử **rất lớn** — move cost cao, không muốn shift vector  
- Số phần tử **nhỏ** và thao tác chủ yếu insert giữa, không full scan  

Low-latency engine: thường **vector + erase-remove idiom** hoặc **index freelist** thay list.

```cpp
// Erase-remove (vector) — giữ contiguous sau purge
v.erase(std::remove_if(v.begin(), v.end(), pred), v.end());
```

### `deque` vs `vector` cho scan

`deque` map index → chunk; full scan vẫn sequential trong chunk nhưng **jump chunk** có thể miss thêm. Rule: default **vector**; `deque` khi cần `push_front` O(1) mà vẫn random access.

### Low-latency patterns

```cpp
// 1. Pre-size — no realloc during tick
std::vector<Tick> ticks;
ticks.resize(max_ticks_per_frame);

// 2. Fixed ring — array index modulo N
std::array<Tick, 4096> ring;
std::size_t head = 0, tail = 0;

// 3. Contiguous priority — vector sort/partition thay multiset khi n nhỏ
```

### Đo thực tế

```bash
perf stat -e cache-references,cache-misses ./benchmark
# High miss rate on list scan → switch to vector verified by data
```

---

## 11. Iterator Categories

Iterators are the glue between containers and algorithms. Each container provides iterators of a specific category, determining which algorithms it supports.

```
Input      → single-pass read only         (std::istream_iterator)
Output     → single-pass write only        (std::ostream_iterator)
Forward    → multi-pass, forward only      (forward_list, unordered_*)
Bidirectional → forward + backward         (list, set, map)
Random Access → jump to any position       (vector, deque, array, string)
Contiguous  → random access + contiguous   (vector, array, string) C++20
```

```cpp
// Random access iterator — supports arithmetic
std::vector<int> v = {1,2,3,4,5};
auto it = v.begin();
it += 3;        // jump 3 forward
it - v.begin(); // distance = 3
it[1];          // dereference at offset

// Bidirectional — only ++ and --
std::list<int> lst = {1,2,3};
auto it = lst.begin();
++it;  // OK
--it;  // OK
// it += 2;  ← compile error — list iterator is not random access

// std::advance and std::distance work on all iterator types
std::advance(it, 2);      // O(n) for forward/bidirectional, O(1) for random access
std::distance(lst.begin(), it); // O(n) for forward/bidirectional

// Reverse iterators
for (auto it = v.rbegin(); it != v.rend(); ++it)
    std::cout << *it << " ";  // 5 4 3 2 1
```

---

## 12. Common Algorithms with Containers

```cpp
#include <algorithm>
#include <numeric>

std::vector<int> v = {5, 3, 1, 4, 2};

// Sorting
std::sort(v.begin(), v.end());                    // O(n log n) introsort
std::sort(v.begin(), v.end(), std::greater<>());  // descending
std::stable_sort(v.begin(), v.end());             // preserves equal element order

// Searching
std::binary_search(v.begin(), v.end(), 3);        // O(log n) — requires sorted
std::find(v.begin(), v.end(), 3);                 // O(n) — returns iterator
std::lower_bound(v.begin(), v.end(), 3);          // O(log n) — requires sorted

// Transforms
std::transform(v.begin(), v.end(), v.begin(),
               [](int x) { return x * 2; });

// Aggregates
int sum = std::accumulate(v.begin(), v.end(), 0);
int max = *std::max_element(v.begin(), v.end());
int min = *std::min_element(v.begin(), v.end());

// Erase-remove idiom — O(n) remove all matching elements
v.erase(std::remove(v.begin(), v.end(), 3), v.end());

// C++20: ranges (cleaner syntax, composable)
std::ranges::sort(v);
std::ranges::sort(v, std::greater<>());
auto it = std::ranges::find(v, 3);
```

---

## 13. Choosing the Right Container

```
Do you know the size at compile time?
└── Yes → std::array

Do you need key-value lookup?
└── Yes →
    Need sorted order or range queries?
    └── Yes → std::map
    └── No  → std::unordered_map (faster O(1) avg)

Do you need unique elements (set)?
└── Yes →
    Need sorted order?
    └── Yes → std::set
    └── No  → std::unordered_set

Do you need a sequence?
└── Yes →
    Frequent insert/delete at BOTH ends?
    └── Yes → std::deque
    Frequent insert/delete in MIDDLE with stable iterators?
    └── Yes → std::list (but benchmark — vector often wins due to cache)
    Everything else?
    └── std::vector (default choice)

Do you need LIFO access?
└── std::stack (or vector with push_back/pop_back)

Do you need FIFO access?
└── std::queue (or deque)

Do you need highest/lowest priority element?
└── std::priority_queue
```

---

## Mock Interview Questions

---

**[Mid]** Why is `std::vector` usually faster than `std::list` even for insertions in the middle?

> People expect `std::list` to win because it has O(1) insertion at a known position vs O(n) for `std::vector`. But in practice, `std::vector` often wins for realistic sizes because:
>
> 1. **Cache locality**: vector elements are contiguous — the CPU prefetches them efficiently. List nodes are scattered on the heap — each `next` pointer dereference is a potential cache miss (~100 cycles penalty on a cold cache).
>
> 2. **Finding the position is O(n) for both**: list doesn't support random access, so to insert at position k you must traverse k nodes anyway. Vector's O(n) shift is highly vectorizable (SIMD `memmove`) and cache-friendly.
>
> 3. **Memory overhead**: each list node allocates 16 extra bytes (two pointers) plus allocator overhead. Vector stores elements densely.
>
> Benchmarks consistently show `std::vector` outperforming `std::list` for n < ~100,000 even for middle insertions. Use `std::list` only when you have a held iterator to the insertion point AND stable iterators are required (e.g., an LRU cache eviction list).

---

**[Mid]** What is iterator invalidation in `std::vector`? How do you avoid it?

> An iterator (or pointer or reference) into a vector is **invalidated** when the vector's internal buffer is reallocated. Reallocation happens when `push_back` / `insert` causes `size` to exceed `capacity`.
>
> After invalidation, using the old iterator is undefined behavior — it points into freed memory.
>
> ```cpp
> std::vector<int> v = {1, 2, 3};
> auto it = v.begin();
> v.push_back(4);   // may reallocate → it is potentially invalid
> *it;              // UB if reallocation occurred
> ```
>
> **Avoiding it**:
> 1. `v.reserve(N)` upfront — as long as `size < capacity`, no reallocation occurs and iterators stay valid.
> 2. Use indices instead of iterators — `v[idx]` remains valid as long as `idx < v.size()`.
> 3. Operate on the container without holding live iterators across modifying operations.
>
> Note: `erase` and `insert` also invalidate all iterators **at and after** the modified position, even without reallocation.

---

**[Mid]** What is the difference between `std::map` and `std::unordered_map`? When do you use each?

> **`std::map`** is backed by a Red-Black Tree — elements are always sorted by key. All operations are O(log n). Supports ordered iteration, `lower_bound`, `upper_bound`, and range queries.
>
> **`std::unordered_map`** is a hash table — no ordering. Average O(1) for all operations, O(n) worst case (all keys hash to the same bucket). Requires `operator==` and a hash function for the key type.
>
> **Use `std::map` when**:
> - You need to iterate keys in sorted order
> - You need range queries (`find all entries between key A and key B`)
> - Keys don't have a natural hash (complex types where writing a good hash is hard)
> - Predictable O(log n) worst-case matters (financial systems, real-time)
>
> **Use `std::unordered_map` when**:
> - Lookup speed is the priority and ordering doesn't matter
> - Keys are integers or strings (standard hash already provided)
> - The dataset is large and the O(log n) vs O(1) difference is measurable
>
> **Pitfall**: `unordered_map` has higher constant factors than people expect — hash computation, potential cache miss into the bucket chain. For small maps (< ~20 entries), a sorted `vector<pair<K,V>>` with binary search is often faster than both.

---

**[Mid]** Why is `std::map::operator[]` dangerous for const maps and read-only access?

> `operator[]` inserts a default-constructed value if the key doesn't exist. This has two consequences:
>
> 1. **It cannot be called on a `const std::map`** — inserting modifies the map, which is disallowed on a const object. The compiler rejects it.
>
> 2. **On a non-const map, it silently inserts**: `m["missing"]` doesn't return an error or throw — it inserts `{"missing", Value{}}` and returns a reference to the default value. Your map is now larger and contains a spurious entry.
>
> ```cpp
> std::map<std::string, int> m = {{"a", 1}};
> int x = m["b"];   // x = 0, but m now has {"a",1}, {"b",0}
> ```
>
> **Safe alternatives**:
> ```cpp
> // Read-only lookup — throws if missing
> int x = m.at("b");
>
> // Read-only lookup — check before access
> if (auto it = m.find("b"); it != m.end())
>     int x = it->second;
>
> // C++20
> if (m.contains("b"))
>     int x = m["b"];
> ```

---

**[Mid]** Why does iterating `std::vector` beat `std::list` even when list has cheaper insertion?

> Iteration dominates most hot loops (transform, accumulate, dispatch). Vector stores elements in one contiguous block — CPU loads 64-byte cache lines containing many elements, and the hardware prefetcher detects sequential access. List nodes are scattered on the heap — each step follows a pointer to a new address, causing cache misses and preventing SIMD. Big-O says list insert is O(1), but scan is still O(n) with a **much larger constant**. For low-latency systems, prefer vector with `reserve`, and only use list when you truly need stable iterators and rarely scan.

---

**[Senior]** Explain how cache lines affect `std::array<int, 10000>` vs `std::list<int>` with 10000 nodes.

> `array<int,10000>`: 40KB contiguous — ~625 cache lines, sequential access touches each line once in order, high L1/L2 hit rate after warm-up. `list<int>`: 10000 nodes × (sizeof(int) + 2 pointers + allocator overhead) — each `++iterator` loads unrelated addresses, ~10000 cold or conflict misses, no spatial reuse. Same O(n) loop, vector/array often **5–20× faster** in practice depending on CPU and allocator. This is why market data, game ticks, and sensor batches use `vector` or fixed `array`, not `list`.

---

**[Senior]** How does `std::vector` achieve O(1) amortized `push_back`? Prove the amortized cost.

> When `push_back` is called and `size == capacity`, the vector allocates a new buffer of size `2 × capacity`, moves all elements, and frees the old buffer. This single operation costs O(n). But amortized over n push_backs starting from capacity 1, the total cost is O(n) — O(1) per operation.
>
> **Proof by accounting method**:
> - Assign a credit of 3 units to each `push_back`.
> - 1 unit pays for the push itself.
> - 2 units are saved for the future reallocation.
>
> When capacity doubles from k to 2k, we move k elements. At this point, the k elements inserted since the last doubling have each saved 2 credits = 2k credits total — exactly enough to pay for moving all k elements (cost k) plus themselves again (already paid). So each push_back costs at most 3 units → O(1) amortized.
>
> **Growth factor matters**: a factor of 2 is common but some implementations use 1.5 (MSVC) to reduce wasted capacity at the cost of slightly more frequent reallocations. Factor must be > 1 to maintain O(1) amortized; factor of 1 (grow by 1 each time) gives O(n) per operation and O(n²) total.

---

**[Senior]** You need a data structure to find the median of a stream of integers efficiently. Which STL containers would you use?

> Use **two `std::multiset`s** (or `priority_queue`s) — one for the lower half and one for the upper half:
>
> ```cpp
> class MedianFinder {
>     std::multiset<int> lo_;  // max is lo_.rbegin() — lower half
>     std::multiset<int> hi_;  // min is hi_.begin()  — upper half
>     // Invariant: lo_.size() == hi_.size() or lo_.size() == hi_.size() + 1
>
> public:
>     void add(int num) {
>         // Step 1: place in correct half
>         if (lo_.empty() || num <= *lo_.rbegin())
>             lo_.insert(num);
>         else
>             hi_.insert(num);
>
>         // Step 2: rebalance so sizes differ by at most 1
>         if (lo_.size() > hi_.size() + 1) {
>             hi_.insert(*lo_.rbegin());
>             lo_.erase(std::prev(lo_.end()));
>         } else if (hi_.size() > lo_.size()) {
>             lo_.insert(*hi_.begin());
>             hi_.erase(hi_.begin());
>         }
>     }
>
>     double median() const {
>         if (lo_.size() == hi_.size())
>             return (*lo_.rbegin() + *hi_.begin()) / 2.0;
>         return *lo_.rbegin();
>     }
> };
> ```
>
> **Complexity**: `add` = O(log n) (multiset insert + erase). `median` = O(1) (iterator to end/begin).
>
> **Why multiset over priority_queue**: `std::priority_queue` doesn't support erasing an arbitrary element — only the top. Rebalancing requires moving the top of one heap to the other, which `multiset` handles cleanly with `erase(iterator)` in O(log n).
>
> **Alternative with priority_queue**: use a max-heap for lo and min-heap for hi. Works for add/median but cannot delete arbitrary elements — unsuitable if elements can be removed from the stream.
