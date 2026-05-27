# Rust 04 — Collections, Iterators & Closures

---

## 4.1 `Vec<T>` — Growable Array

```rust
// Construction
let mut v: Vec<i32> = Vec::new();
let v = vec![1, 2, 3, 4, 5];

// Basic operations
v.push(6);
let popped = v.pop();           // Option<T>
v.insert(2, 99);                // insert at index
v.remove(2);                    // remove by index, returns T
let len = v.len();
let is_empty = v.is_empty();

// Access
let third = &v[2];              // panics if out of bounds
let third = v.get(2);           // Option<&T>, safe
let first = v.first();          // Option<&T>

// Sorting
v.sort();                        // requires Ord
v.sort_by(|a, b| a.cmp(b));
v.sort_by_key(|x| -x);          // sort by derived key

// Dedup, retain
v.dedup();                       // remove consecutive duplicates (sort first)
v.retain(|&x| x % 2 == 0);      // keep elements matching predicate

// Iteration
for &x in &v { println!("{}", x); }  // shared refs
for x in &mut v { *x *= 2; }         // mutable refs
for x in v { println!("{}", x); }    // consumed (moves out)
```

---

## 4.2 `HashMap<K, V>`

```rust
use std::collections::HashMap;

let mut scores: HashMap<String, u32> = HashMap::new();

// Insert
scores.insert(String::from("Alice"), 100);
scores.insert(String::from("Bob"),   85);

// Get
let alice_score = scores.get("Alice");  // Option<&u32>

// Entry API — insert if absent
scores.entry(String::from("Charlie")).or_insert(75);
// Increment or insert
let count = scores.entry(String::from("Alice")).or_insert(0);
*count += 1;

// Iteration (order is random)
for (name, score) in &scores {
    println!("{}: {}", name, score);
}

// Contains, remove
if scores.contains_key("Bob") { scores.remove("Bob"); }
```

---

## 4.3 Other Collections

| Collection | Use Case |
|------------|----------|
| `VecDeque<T>` | Double-ended queue, efficient push/pop from both ends |
| `LinkedList<T>` | Rarely useful — Vec is almost always faster due to cache |
| `HashSet<T>` | Unique elements, O(1) insert/lookup/contains |
| `BTreeMap<K,V>` | Sorted map, O(log n) — needed when iteration order matters |
| `BTreeSet<T>` | Sorted set |
| `BinaryHeap<T>` | Priority queue (max-heap by default) |

```rust
use std::collections::HashSet;
let mut s: HashSet<i32> = HashSet::new();
s.insert(1); s.insert(2); s.insert(2);  // len == 2
let union: HashSet<_> = s.union(&other_set).collect();
let intersection: HashSet<_> = s.intersection(&other_set).collect();
```

---

## 4.4 Iterators

The `Iterator` trait has one required method:
```rust
trait Iterator {
    type Item;
    fn next(&mut self) -> Option<Self::Item>;
    // ~70 provided methods built on next()
}
```

**Creating iterators**:
```rust
let v = vec![1, 2, 3];
let iter = v.iter();        // yields &T
let iter = v.iter_mut();   // yields &mut T
let iter = v.into_iter();  // yields T (consumes v)

// Ranges
for i in 0..5  { }  // 0,1,2,3,4
for i in 0..=5 { }  // 0,1,2,3,4,5
```

---

## 4.5 Iterator Adaptors (Lazy)

```rust
let v = vec![1, 2, 3, 4, 5, 6];

// map — transform each element
let doubled: Vec<i32> = v.iter().map(|x| x * 2).collect();

// filter — keep elements matching predicate
let evens: Vec<&i32> = v.iter().filter(|&&x| x % 2 == 0).collect();

// filter_map — filter + map in one step
let parsed: Vec<i32> = ["1", "two", "3"].iter()
    .filter_map(|s| s.parse().ok())
    .collect();  // [1, 3]

// chain — concatenate iterators
let combined: Vec<i32> = v.iter().chain([7, 8, 9].iter()).copied().collect();

// zip — pair elements
let pairs: Vec<_> = [1,2,3].iter().zip(["a","b","c"].iter()).collect();

// enumerate — index + value
for (i, val) in v.iter().enumerate() { println!("{}: {}", i, val); }

// flat_map — map then flatten
let words = vec!["hello world", "foo bar"];
let chars: Vec<&str> = words.iter().flat_map(|s| s.split(' ')).collect();

// take / skip
let first3: Vec<_> = v.iter().take(3).collect();
let skip2: Vec<_>  = v.iter().skip(2).collect();

// take_while / skip_while
let lt4: Vec<_> = v.iter().take_while(|&&x| x < 4).collect();

// peekable — look ahead without consuming
let mut iter = v.iter().peekable();
if iter.peek() == Some(&&1) { iter.next(); }
```

---

## 4.6 Iterator Consumers (Eager — Drive the Iterator)

```rust
// collect — into Vec, HashMap, String, etc.
let sum: i32 = v.iter().sum();
let product: i32 = v.iter().product();
let max = v.iter().max();           // Option<&T>
let min = v.iter().min();
let count = v.iter().filter(|&&x| x > 3).count();

// fold — general accumulation
let sum = v.iter().fold(0, |acc, &x| acc + x);

// any / all
let has_even = v.iter().any(|&x| x % 2 == 0);
let all_pos  = v.iter().all(|&x| x > 0);

// find / position
let first_even = v.iter().find(|&&x| x % 2 == 0);  // Option<&T>
let pos         = v.iter().position(|&x| x == 3);   // Option<usize>

// for_each — like a for loop
v.iter().for_each(|x| println!("{}", x));

// partition — split into two collections
let (evens, odds): (Vec<i32>, Vec<i32>) = v.into_iter().partition(|&x| x % 2 == 0);
```

---

## 4.7 Closures

Closures capture their environment:

```rust
let multiplier = 3;
let triple = |x: i32| -> i32 { x * multiplier };  // captures multiplier
println!("{}", triple(4));  // 12
```

**Three closure traits** (automatically determined by the compiler):

| Trait | Meaning | Can call |
|-------|---------|----------|
| `FnOnce` | Consumes captured variables (moves out) | Once only |
| `FnMut` | Mutates captured variables | Multiple times |
| `Fn` | Read-only access to captured variables | Multiple times |

Every closure that is `Fn` also implements `FnMut` and `FnOnce`. Every `FnMut` also implements `FnOnce`.

```rust
// FnOnce — moves a value out
let s = String::from("hello");
let consume = move || { drop(s); };  // moves s into closure
consume();
// consume();  // ERROR: cannot call FnOnce twice

// FnMut — mutates captured variable
let mut count = 0;
let mut increment = || { count += 1; count };
println!("{}", increment());  // 1
println!("{}", increment());  // 2

// Fn — borrows immutably
let message = String::from("hello");
let greet = || println!("{}", message);
greet(); greet();  // fine, shared borrow
```

**`move` closures** — force all captures to be moved (required for threads):
```rust
let data = vec![1, 2, 3];
let handle = std::thread::spawn(move || {
    println!("{:?}", data);  // data moved into closure
});
```

---

## 4.8 Implementing `Iterator`

```rust
struct Counter {
    count: u32,
    max: u32,
}

impl Counter {
    fn new(max: u32) -> Self { Counter { count: 0, max } }
}

impl Iterator for Counter {
    type Item = u32;

    fn next(&mut self) -> Option<Self::Item> {
        if self.count < self.max {
            self.count += 1;
            Some(self.count)
        } else {
            None
        }
    }
}

// Now all iterator adapters work for free
let sum: u32 = Counter::new(5).zip(Counter::new(5).skip(1))
    .map(|(a, b)| a * b)
    .filter(|x| x % 3 == 0)
    .sum();
```

---

## Interview Questions

### Easy

**Q: What is the difference between `iter()`, `iter_mut()`, and `into_iter()`?**

> `iter()` yields shared references `&T` — borrows the collection, leaving it intact. `iter_mut()` yields mutable references `&mut T` — borrows mutably, allowing in-place modification. `into_iter()` consumes the collection and yields owned `T` values — the collection cannot be used after the loop. When writing `for x in &v`, `&v` coerces to an iterator of `&T` (same as `v.iter()`). For `for x in &mut v`, you get `&mut T`. For `for x in v`, you consume `v`. The `IntoIterator` trait is what `for` loops use under the hood — any type implementing it can be used in a `for` loop directly.

**Q: What is the `entry` API on `HashMap` and when is it useful?**

> The `entry` API provides a way to insert or modify a value only if it's absent or present, using a single map lookup. `map.entry(key).or_insert(default)` inserts `default` if the key doesn't exist and returns a `&mut V` either way. This avoids a double lookup (one `contains_key` + one `insert`). Common pattern: `*map.entry(word).or_insert(0) += 1` — word count in one line. It also avoids the borrow checker problem of checking `contains_key` then `insert` separately, since both take `&mut self` and the first borrow would expire before the second.

**Q: Are iterator adaptors like `map` and `filter` eager or lazy? Why does it matter?**

> They are **lazy** — no computation happens when you call `.map(f)`. An adaptor returns a new iterator struct that wraps the source. Work only happens when the iterator is **driven** by a consumer (`collect`, `sum`, `for_each`, etc.) pulling elements via `next()`. This matters because: (1) Processing stops as soon as the consumer stops pulling (e.g., `.take(5)` — only 5 elements from the source are processed even if the source is infinite). (2) Chained adaptors don't allocate intermediate `Vec`s — data flows element by element through the pipeline. (3) The entire chain compiles to a single tight loop — zero-cost abstraction.

---

### Mid

**Q: Implement a word frequency counter using `HashMap` and iterators.**

> ```rust
> use std::collections::HashMap;
>
> fn word_count(text: &str) -> HashMap<&str, usize> {
>     let mut counts = HashMap::new();
>     text.split_whitespace()
>         .for_each(|word| *counts.entry(word).or_insert(0) += 1);
>     counts
> }
>
> // Or using fold:
> fn word_count_fold(text: &str) -> HashMap<&str, usize> {
>     text.split_whitespace().fold(HashMap::new(), |mut acc, word| {
>         *acc.entry(word).or_insert(0) += 1;
>         acc
>     })
> }
>
> // Top N most frequent:
> fn top_n(text: &str, n: usize) -> Vec<(&str, usize)> {
>     let mut counts = word_count(text);
>     let mut pairs: Vec<_> = counts.into_iter().collect();
>     pairs.sort_by(|a, b| b.1.cmp(&a.1));
>     pairs.into_iter().take(n).collect()
> }
> ```

**Q: What is `flat_map` and when would you use it over `map`?**

> `map` applies `f: T -> U` to each element, producing one `U` per input. If `U` is itself an `Option` or `Vec`, you get a nested structure. `flat_map` (or `.map(f).flatten()`) applies `f: T -> IntoIterator<Item=U>` and flattens one level — each input produces zero or more outputs. Use `flat_map` when: (1) Expanding one element into multiple: splitting a sentence into words (each `&str` → multiple `&str`). (2) Filtering + mapping in one step: `flat_map(|x| if cond { Some(f(x)) } else { None })` (equivalent to `filter_map`). (3) Chaining optional operations: `items.iter().flat_map(|x| x.children())`.

**Q: How do you create an infinite iterator and use it safely?**

> ```rust
> // std::iter::repeat — infinite sequence of cloned values
> let first_five: Vec<i32> = std::iter::repeat(42).take(5).collect();
>
> // std::iter::successors — generate sequence by function
> let powers_of_2: Vec<u64> = std::iter::successors(Some(1u64), |&n| n.checked_mul(2))
>     .take(10)
>     .collect();
>
> // Custom infinite counter
> let naturals = (0u64..).filter(|n| n % 3 == 0).take(5);
>
> // SAFETY: always pair infinite iterators with take(), take_while(),
> // or a consumer that terminates early (find, any, all).
> // Never call collect() on an infinite iterator — it will run forever
> // and exhaust memory.
> ```

---

### Hard

**Q: Implement `MyZip` — a custom iterator that zips two iterators, terminating when the shorter one ends.**

> ```rust
> struct MyZip<A: Iterator, B: Iterator> {
>     a: A,
>     b: B,
> }
>
> impl<A: Iterator, B: Iterator> MyZip<A, B> {
>     fn new(a: A, b: B) -> Self { MyZip { a, b } }
> }
>
> impl<A: Iterator, B: Iterator> Iterator for MyZip<A, B> {
>     type Item = (A::Item, B::Item);
>
>     fn next(&mut self) -> Option<Self::Item> {
>         match (self.a.next(), self.b.next()) {
>             (Some(a), Some(b)) => Some((a, b)),
>             _                  => None,   // either exhausted
>         }
>     }
>
>     fn size_hint(&self) -> (usize, Option<usize>) {
>         let (a_lo, a_hi) = self.a.size_hint();
>         let (b_lo, b_hi) = self.b.size_hint();
>         let lo = a_lo.min(b_lo);
>         let hi = match (a_hi, b_hi) {
>             (Some(a), Some(b)) => Some(a.min(b)),
>             (Some(a), None)    => Some(a),
>             (None, Some(b))    => Some(b),
>             (None, None)       => None,
>         };
>         (lo, hi)
>     }
> }
>
> // Usage:
> let z: Vec<_> = MyZip::new(1..=3, vec!["a","b","c","d"].into_iter()).collect();
> // [(1,"a"), (2,"b"), (3,"c")]
> ```

**Q: Explain the performance difference between `collect::<Vec<_>>()` in a chain vs using a `for` loop with `push`.**

> They compile to essentially identical code. The iterator chain `.map(f).filter(g).collect::<Vec<_>>()` is monomorphized — the compiler inlines the closures and generates a single loop that reads from the source, applies transformations, and pushes to the `Vec`. There is no intermediate allocation. The `Vec` grows as elements are pushed (amortized O(1) `push`). If the iterator implements `size_hint()` with an upper bound, `collect` calls `Vec::with_capacity(hint)` first, avoiding reallocations entirely. A manual `for` loop + `push` does the same thing but you'd have to call `with_capacity` yourself. The idiomatic iterator chain is not slower — benchmarks consistently show the two are equal or the iterator chain is faster (better optimizer visibility into the full pipeline).

**Q: What is `Iterator::collect` and how does it know which collection type to produce?**

> `collect` is a consumer that calls the `FromIterator<Item>` trait on the target type. The signature is `fn collect<B: FromIterator<Self::Item>>(self) -> B`. The target type is inferred from context — either an explicit type annotation (`let v: Vec<i32> = ...`), a turbofish (`.collect::<Vec<_>>()`), or the return type of the function. `FromIterator` is implemented for `Vec<T>`, `HashMap<K,V>`, `HashSet<T>`, `String` (from `char`s), `Result<Vec<T>, E>` (collecting `Result<T, E>` items — fails fast on first `Err`), `Option<Vec<T>>` (collecting `Option<T>` items — fails on first `None`). The last two are very powerful: `let results: Result<Vec<_>, _> = items.map(fallible_op).collect()` short-circuits on the first error.
