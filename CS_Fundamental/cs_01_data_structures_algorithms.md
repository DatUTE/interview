# Data Structures and Algorithms

## 1. Big-O

Big-O describes how runtime or memory grows as input size grows.

| Complexity | Meaning | Common Examples |
|------------|---------|-----------------|
| O(1) | Constant | Array index, hash lookup average |
| O(log n) | Halve search space | Binary search, balanced tree lookup |
| O(n) | Single pass | Linear scan |
| O(n log n) | Divide and merge | Merge sort, heap sort, good comparison sort |
| O(n²) | Pairwise comparison | Nested loops, naive duplicate check |
| O(2^n) | Exponential states | Brute-force subsets |

Interview rule:

> Always discuss both time and space complexity, including whether the complexity is average-case or worst-case.

---

## 2. Arrays and Vectors

Arrays store elements contiguously.

Strengths:

- O(1) random access.
- Excellent cache locality.
- Simple memory layout.

Weaknesses:

- Insert/delete in middle is O(n).
- Dynamic growth may reallocate and invalidate pointers/iterators.

Production example:

> Hot-path C++ code often prefers `std::vector` over linked lists because sequential memory access is cache-friendly.

---

## 3. Hash Tables

Hash tables map keys to buckets using a hash function.

Typical complexity:

- Average lookup/insert/delete: O(1).
- Worst case: O(n), if many keys collide.

Important concepts:

- Hash function quality.
- Load factor.
- Collision handling: chaining vs open addressing.
- Rehashing cost.

Interview answer:

> Use a hash table for exact lookup when ordering is not required. Use a tree when sorted iteration or range queries matter.

---

## 4. Stacks and Queues

Stack: LIFO.

Use cases:

- Function call stack.
- DFS.
- Undo operation.
- Parentheses validation.

Queue: FIFO.

Use cases:

- BFS.
- Producer-consumer.
- Task scheduling.
- Network packet processing.

---

## 5. Heaps

A heap gives fast access to min or max priority item.

Complexity:

- Push: O(log n).
- Pop min/max: O(log n).
- Peek min/max: O(1).

Use cases:

- Priority queue.
- Top K.
- Dijkstra.
- Scheduling.

---

## 6. Trees

Binary search tree:

- Left subtree smaller.
- Right subtree larger.

Balanced trees keep height O(log n), so lookup/insert/delete remain O(log n).

Use trees when:

- Sorted traversal matters.
- Range query matters.
- You need stable ordered behavior.

---

## 7. Graphs

Graph representation:

| Representation | Good For | Cost |
|----------------|----------|------|
| Adjacency list | Sparse graphs | O(V + E) memory |
| Adjacency matrix | Dense graphs, O(1) edge check | O(V²) memory |

Core algorithms:

- BFS: shortest path in unweighted graph.
- DFS: traversal, cycle detection, topological sort.
- Dijkstra: shortest path with non-negative weights.
- Bellman-Ford: supports negative weights, detects negative cycles.
- Union-Find: connectivity and grouping.

---

## 8. Sorting

| Algorithm | Average | Worst | Notes |
|-----------|---------|-------|-------|
| Quick sort | O(n log n) | O(n²) | Fast in practice, partition-based |
| Merge sort | O(n log n) | O(n log n) | Stable, extra memory |
| Heap sort | O(n log n) | O(n log n) | In-place, less cache-friendly |
| Counting sort | O(n + k) | O(n + k) | Non-comparison, small integer range |

Interview note:

> Comparison sorting has O(n log n) lower bound. To do better, exploit constraints such as bounded integer range.

---

## 9. Dynamic Programming

DP applies when a problem has:

- Overlapping subproblems.
- Optimal substructure.

Common steps:

1. Define state.
2. Define transition.
3. Define base cases.
4. Decide iteration order.
5. Optimize memory if possible.

Example state:

```text
dp[i] = best answer using first i items
dp[i][j] = best answer using first i items with capacity j
```

---

## Interview Questions

- When would you choose hash table vs balanced tree?
- Why can `std::vector` outperform linked list even for many inserts?
- How does BFS differ from DFS?
- Why does Dijkstra fail with negative edge weights?
- How do you detect a cycle in a directed graph?
- What makes a problem suitable for dynamic programming?
