# Algorithms in C++

> **LeetCode ôn PV (Easy→Medium, theo concept):** [../leetcode_interview_prep.md](../leetcode_interview_prep.md)

---

## Complexity Reference

### Big-O Notation

Describes how runtime or memory grows relative to input size n as n → ∞. Only the **dominant term** matters; constants are dropped.

```
O(1)       constant   — hash lookup, array access
O(log n)   logarithmic— binary search, balanced BST ops
O(n)       linear     — single scan, linear search
O(n log n) linearithmic— merge sort, heap sort, STL sort
O(n²)      quadratic  — bubble/insertion/selection sort, naive string matching
O(n³)      cubic      — Floyd-Warshall, naive matrix multiply
O(2ⁿ)      exponential— power set, brute-force subset enumeration
O(n!)      factorial  — all permutations
```

```
Growth (n = 1000):
O(1)      → 1
O(log n)  → ~10
O(n)      → 1,000
O(n log n)→ ~10,000
O(n²)     → 1,000,000
O(2ⁿ)     → 10^301  ← practically infinite
```

### Analysis Types

| Type | Meaning | Example |
|---|---|---|
| **Worst case** | Maximum cost over all inputs | QuickSort: O(n²) on sorted input |
| **Average case** | Expected cost over random inputs | QuickSort: O(n log n) average |
| **Best case** | Minimum cost | Insertion sort: O(n) on already sorted |
| **Amortized** | Average cost per op over a sequence | `vector::push_back`: O(1) amortized |

---

## 1. Sorting Algorithms

### Quicksort

**Divide and conquer** — pick a pivot, partition array into elements < pivot and elements > pivot, recurse.

- Average: O(n log n) | Worst: O(n²) (sorted input with bad pivot) | Space: O(log n) stack
- **Not stable** — equal elements may swap relative order
- Best for: general purpose in-memory sort (cache-friendly, low constant factor)

```cpp
// Lomuto partition scheme
int partition(std::vector<int>& arr, int lo, int hi) {
    int pivot = arr[hi];   // pivot = last element
    int i = lo - 1;        // index of smaller element

    for (int j = lo; j < hi; ++j) {
        if (arr[j] <= pivot) {
            ++i;
            std::swap(arr[i], arr[j]);
        }
    }
    std::swap(arr[i + 1], arr[hi]);
    return i + 1;
}

void quicksort(std::vector<int>& arr, int lo, int hi) {
    if (lo >= hi) return;
    int p = partition(arr, lo, hi);
    quicksort(arr, lo, p - 1);
    quicksort(arr, p + 1, hi);
}

// Median-of-three pivot — avoids O(n²) on sorted input
int median_of_three(std::vector<int>& arr, int lo, int hi) {
    int mid = lo + (hi - lo) / 2;
    if (arr[lo] > arr[mid]) std::swap(arr[lo], arr[mid]);
    if (arr[lo] > arr[hi])  std::swap(arr[lo], arr[hi]);
    if (arr[mid] > arr[hi]) std::swap(arr[mid], arr[hi]);
    return mid;   // median is now at mid
}
```

### Merge Sort

**Divide and conquer** — split in half, recursively sort, merge sorted halves.

- All cases: O(n log n) | Space: O(n) auxiliary
- **Stable** — equal elements preserve relative order
- Best for: linked lists (no random access needed), external sort (disk), when stability required

```cpp
void merge(std::vector<int>& arr, int lo, int mid, int hi) {
    std::vector<int> tmp(arr.begin() + lo, arr.begin() + hi + 1);
    int left = 0, right = mid - lo + 1;
    int k = lo;

    while (left <= mid - lo && right <= hi - lo) {
        if (tmp[left] <= tmp[right])   // <= for stability
            arr[k++] = tmp[left++];
        else
            arr[k++] = tmp[right++];
    }
    while (left  <= mid - lo) arr[k++] = tmp[left++];
    while (right <= hi  - lo) arr[k++] = tmp[right++];
}

void merge_sort(std::vector<int>& arr, int lo, int hi) {
    if (lo >= hi) return;
    int mid = lo + (hi - lo) / 2;
    merge_sort(arr, lo, mid);
    merge_sort(arr, mid + 1, hi);
    merge(arr, lo, mid, hi);
}
```

### Heap Sort

**In-place** — build a max-heap, repeatedly extract max to end of array.

- All cases: O(n log n) | Space: O(1)
- **Not stable**
- Best for: when guaranteed O(n log n) AND O(1) space are both required

```cpp
void heapify(std::vector<int>& arr, int n, int i) {
    int largest = i;
    int l = 2 * i + 1, r = 2 * i + 2;
    if (l < n && arr[l] > arr[largest]) largest = l;
    if (r < n && arr[r] > arr[largest]) largest = r;
    if (largest != i) {
        std::swap(arr[i], arr[largest]);
        heapify(arr, n, largest);
    }
}

void heap_sort(std::vector<int>& arr) {
    int n = arr.size();
    // Build max-heap: O(n)
    for (int i = n / 2 - 1; i >= 0; --i)
        heapify(arr, n, i);
    // Extract elements: O(n log n)
    for (int i = n - 1; i > 0; --i) {
        std::swap(arr[0], arr[i]);
        heapify(arr, i, 0);
    }
}
```

### Counting Sort / Radix Sort

**Non-comparison** sorts — beat the O(n log n) lower bound when keys are integers in a bounded range.

```cpp
// Counting sort — O(n + k) where k = range of values
void counting_sort(std::vector<int>& arr, int max_val) {
    std::vector<int> count(max_val + 1, 0);
    for (int x : arr) ++count[x];
    int idx = 0;
    for (int v = 0; v <= max_val; ++v)
        while (count[v]-- > 0)
            arr[idx++] = v;
}

// Radix sort — O(d * (n + k)) where d = digits, k = base (10)
void radix_sort(std::vector<int>& arr) {
    int max_val = *std::max_element(arr.begin(), arr.end());
    for (int exp = 1; max_val / exp > 0; exp *= 10) {
        std::vector<int> output(arr.size());
        std::vector<int> count(10, 0);
        for (int x : arr) ++count[(x / exp) % 10];
        for (int i = 1; i < 10; ++i) count[i] += count[i-1];
        for (int i = arr.size()-1; i >= 0; --i) {
            output[--count[(arr[i]/exp)%10]] = arr[i];
        }
        arr = output;
    }
}
```

### Sorting Algorithm Comparison

| Algorithm | Best | Average | Worst | Space | Stable | Notes |
|---|---|---|---|---|---|---|
| QuickSort | O(n log n) | O(n log n) | O(n²) | O(log n) | No | Fastest in practice |
| MergeSort | O(n log n) | O(n log n) | O(n log n) | O(n) | **Yes** | Best for linked lists |
| HeapSort | O(n log n) | O(n log n) | O(n log n) | O(1) | No | Guaranteed + in-place |
| InsertionSort | O(n) | O(n²) | O(n²) | O(1) | **Yes** | Best for tiny / nearly sorted |
| CountingSort | O(n+k) | O(n+k) | O(n+k) | O(k) | **Yes** | Integer keys, small range |
| `std::sort` | O(n log n) | O(n log n) | O(n log n) | O(log n) | No | Introsort (quick+heap+insertion) |
| `std::stable_sort` | O(n log n) | O(n log n) | O(n log n) | O(n) | **Yes** | Merge sort based |

---

## 2. Searching Algorithms

### Binary Search

Requires a **sorted** collection. Eliminates half the search space each iteration.

- O(log n) time | O(1) space

```cpp
// Returns index of target, or -1 if not found
int binary_search(const std::vector<int>& arr, int target) {
    int lo = 0, hi = (int)arr.size() - 1;
    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;   // avoids overflow vs (lo+hi)/2
        if      (arr[mid] == target) return mid;
        else if (arr[mid]  < target) lo = mid + 1;
        else                          hi = mid - 1;
    }
    return -1;
}

// STL binary search variants (all O(log n), require sorted range)
auto it = std::lower_bound(v.begin(), v.end(), 5); // first element >= 5
auto it = std::upper_bound(v.begin(), v.end(), 5); // first element >  5
bool found = std::binary_search(v.begin(), v.end(), 5);
auto [lo, hi] = std::equal_range(v.begin(), v.end(), 5); // range of 5s
```

### Binary Search on Answer (Parametric Search)

Apply binary search to the **answer space** when the predicate is monotone.

```cpp
// "Find minimum capacity k such that you can ship packages in D days"
bool can_ship(const std::vector<int>& weights, int days, int capacity) {
    int day = 1, load = 0;
    for (int w : weights) {
        if (load + w > capacity) { ++day; load = 0; }
        load += w;
    }
    return day <= days;
}

int min_capacity(const std::vector<int>& weights, int days) {
    int lo = *std::max_element(weights.begin(), weights.end()); // must carry heaviest
    int hi = std::accumulate(weights.begin(), weights.end(), 0); // carry all in 1 day

    while (lo < hi) {
        int mid = lo + (hi - lo) / 2;
        if (can_ship(weights, days, mid)) hi = mid;  // mid works → try smaller
        else                              lo = mid + 1;
    }
    return lo;
}
```

---

## 3. Graph Algorithms

### Representations

```cpp
// Adjacency Matrix — O(V²) space, O(1) edge lookup
std::vector<std::vector<int>> matrix(V, std::vector<int>(V, 0));
matrix[u][v] = 1;  // or weight

// Adjacency List — O(V + E) space, O(degree) edge lookup
std::vector<std::vector<int>> adj(V);
adj[u].push_back(v);

// Weighted adjacency list
std::vector<std::vector<std::pair<int,int>>> adj(V); // {neighbor, weight}
adj[u].push_back({v, w});
```

### BFS — Breadth-First Search

Explores level by level. Finds **shortest path in unweighted graphs**.

- O(V + E) time | O(V) space

```cpp
std::vector<int> bfs(int src, const std::vector<std::vector<int>>& adj) {
    int V = adj.size();
    std::vector<int> dist(V, -1);
    std::queue<int> q;

    dist[src] = 0;
    q.push(src);

    while (!q.empty()) {
        int u = q.front(); q.pop();
        for (int v : adj[u]) {
            if (dist[v] == -1) {
                dist[v] = dist[u] + 1;
                q.push(v);
            }
        }
    }
    return dist;  // dist[v] = shortest hops from src to v, -1 if unreachable
}
```

### DFS — Depth-First Search

Explores as deep as possible before backtracking. Used for connectivity, cycle detection, topological sort.

- O(V + E) time | O(V) stack space

```cpp
// Iterative DFS
void dfs_iterative(int src, const std::vector<std::vector<int>>& adj) {
    std::vector<bool> visited(adj.size(), false);
    std::stack<int> st;
    st.push(src);

    while (!st.empty()) {
        int u = st.top(); st.pop();
        if (visited[u]) continue;
        visited[u] = true;
        for (int v : adj[u])
            if (!visited[v]) st.push(v);
    }
}

// Recursive DFS — with pre/post order tracking
void dfs(int u, const std::vector<std::vector<int>>& adj,
         std::vector<bool>& visited, std::vector<int>& order) {
    visited[u] = true;
    for (int v : adj[u])
        if (!visited[v])
            dfs(v, adj, visited, order);
    order.push_back(u);  // post-order: u is "finished" after all its descendants
}
```

### Topological Sort (Kahn's Algorithm — BFS)

Linear ordering of vertices such that for every edge u→v, u comes before v. Only for **DAGs** (directed acyclic graphs).

```cpp
std::vector<int> topological_sort(int V, const std::vector<std::vector<int>>& adj) {
    std::vector<int> in_degree(V, 0);
    for (int u = 0; u < V; ++u)
        for (int v : adj[u]) ++in_degree[v];

    std::queue<int> q;
    for (int i = 0; i < V; ++i)
        if (in_degree[i] == 0) q.push(i);  // all sources (no incoming edges)

    std::vector<int> order;
    while (!q.empty()) {
        int u = q.front(); q.pop();
        order.push_back(u);
        for (int v : adj[u])
            if (--in_degree[v] == 0) q.push(v);
    }

    if ((int)order.size() != V)
        throw std::runtime_error("Graph has a cycle — no topological order");
    return order;
}
// Use case: task scheduling, build systems (Make/CMake), course prerequisites
```

### Dijkstra's Algorithm

**Shortest path** from a source to all vertices in a **non-negative weighted** graph.

- O((V + E) log V) with a priority queue

```cpp
std::vector<int> dijkstra(int src,
    const std::vector<std::vector<std::pair<int,int>>>& adj) {
    int V = adj.size();
    std::vector<int> dist(V, INT_MAX);
    // min-heap: {distance, vertex}
    std::priority_queue<std::pair<int,int>,
                        std::vector<std::pair<int,int>>,
                        std::greater<>> pq;
    dist[src] = 0;
    pq.push({0, src});

    while (!pq.empty()) {
        auto [d, u] = pq.top(); pq.pop();
        if (d > dist[u]) continue;  // stale entry — skip

        for (auto [w, v] : adj[u]) {
            if (dist[u] + w < dist[v]) {
                dist[v] = dist[u] + w;
                pq.push({dist[v], v});
            }
        }
    }
    return dist;
}
// Limitation: does NOT work with negative edge weights — use Bellman-Ford instead
```

### Bellman-Ford

Shortest paths with **negative edges**. Detects negative cycles.

- O(V × E) time

```cpp
struct Edge { int u, v, w; };

std::vector<int> bellman_ford(int src, int V,
                              const std::vector<Edge>& edges) {
    std::vector<int> dist(V, INT_MAX);
    dist[src] = 0;

    // Relax all edges V-1 times
    for (int i = 0; i < V - 1; ++i)
        for (auto& [u, v, w] : edges)
            if (dist[u] != INT_MAX && dist[u] + w < dist[v])
                dist[v] = dist[u] + w;

    // V-th relaxation: if any dist changes, a negative cycle exists
    for (auto& [u, v, w] : edges)
        if (dist[u] != INT_MAX && dist[u] + w < dist[v])
            throw std::runtime_error("Negative cycle detected");

    return dist;
}
```

### Floyd-Warshall

**All-pairs** shortest paths. Works with negative edges (not negative cycles).

- O(V³) time | O(V²) space

```cpp
void floyd_warshall(std::vector<std::vector<int>>& dist) {
    int V = dist.size();
    // dist[i][j] = direct edge weight, INT_MAX/2 if no edge, 0 if i==j

    for (int k = 0; k < V; ++k)        // intermediate vertex
        for (int i = 0; i < V; ++i)
            for (int j = 0; j < V; ++j)
                if (dist[i][k] + dist[k][j] < dist[i][j])
                    dist[i][j] = dist[i][k] + dist[k][j];
    // After: dist[i][j] = shortest path from i to j via any intermediate
}
```

### Union-Find (Disjoint Set Union)

Efficiently tracks which elements belong to the same group. Core operations: `find` (which group?) and `union` (merge two groups).

- Nearly O(1) per operation with path compression + union by rank

```cpp
struct DSU {
    std::vector<int> parent, rank_;

    DSU(int n) : parent(n), rank_(n, 0) {
        std::iota(parent.begin(), parent.end(), 0);  // parent[i] = i
    }

    int find(int x) {
        if (parent[x] != x)
            parent[x] = find(parent[x]);  // path compression
        return parent[x];
    }

    bool unite(int x, int y) {
        int px = find(x), py = find(y);
        if (px == py) return false;  // already in same set

        // Union by rank — attach smaller tree under larger
        if (rank_[px] < rank_[py]) std::swap(px, py);
        parent[py] = px;
        if (rank_[px] == rank_[py]) ++rank_[px];
        return true;
    }

    bool connected(int x, int y) { return find(x) == find(y); }
};

// Use case: Kruskal's MST, detecting cycles, number of connected components
int count_components(int n, std::vector<std::pair<int,int>>& edges) {
    DSU dsu(n);
    for (auto [u, v] : edges) dsu.unite(u, v);
    std::set<int> roots;
    for (int i = 0; i < n; ++i) roots.insert(dsu.find(i));
    return roots.size();
}
```

### Kruskal's MST

**Minimum Spanning Tree** — minimum weight set of edges that connects all vertices.

- O(E log E) — dominated by sorting edges

```cpp
int kruskal_mst(int V, std::vector<std::tuple<int,int,int>>& edges) {
    // edges: {weight, u, v}
    std::sort(edges.begin(), edges.end());  // sort by weight

    DSU dsu(V);
    int total_weight = 0, edges_used = 0;

    for (auto [w, u, v] : edges) {
        if (dsu.unite(u, v)) {         // add edge only if it connects different components
            total_weight += w;
            if (++edges_used == V - 1) break;  // MST has exactly V-1 edges
        }
    }
    return total_weight;
}
```

---

## 4. Two Pointers & Sliding Window

### Two Pointers

Use two indices moving toward each other or in the same direction to reduce O(n²) to O(n).

```cpp
// Two-sum in sorted array — O(n)
std::pair<int,int> two_sum_sorted(const std::vector<int>& arr, int target) {
    int lo = 0, hi = (int)arr.size() - 1;
    while (lo < hi) {
        int sum = arr[lo] + arr[hi];
        if      (sum == target) return {lo, hi};
        else if (sum  < target) ++lo;
        else                    --hi;
    }
    return {-1, -1};
}

// Remove duplicates from sorted array in-place — O(n)
int remove_duplicates(std::vector<int>& arr) {
    if (arr.empty()) return 0;
    int slow = 0;
    for (int fast = 1; fast < (int)arr.size(); ++fast)
        if (arr[fast] != arr[slow])
            arr[++slow] = arr[fast];
    return slow + 1;
}

// Container with most water (LeetCode 11) — O(n)
int max_water(const std::vector<int>& h) {
    int lo = 0, hi = (int)h.size() - 1, best = 0;
    while (lo < hi) {
        best = std::max(best, std::min(h[lo], h[hi]) * (hi - lo));
        if (h[lo] < h[hi]) ++lo; else --hi;
    }
    return best;
}
```

### Sliding Window

Maintain a window [lo, hi] over a sequence. Expand hi, shrink lo to maintain a constraint.

```cpp
// Maximum sum subarray of size k — O(n)
int max_sum_k(const std::vector<int>& arr, int k) {
    int sum = 0;
    for (int i = 0; i < k; ++i) sum += arr[i];  // initial window
    int best = sum;
    for (int i = k; i < (int)arr.size(); ++i) {
        sum += arr[i] - arr[i - k];              // slide: add new, drop old
        best = std::max(best, sum);
    }
    return best;
}

// Longest substring without repeating characters — O(n)
int length_of_longest_substring(const std::string& s) {
    std::unordered_map<char, int> last_seen;
    int best = 0, lo = 0;
    for (int hi = 0; hi < (int)s.size(); ++hi) {
        if (last_seen.count(s[hi]) && last_seen[s[hi]] >= lo)
            lo = last_seen[s[hi]] + 1;  // shrink window past last occurrence
        last_seen[s[hi]] = hi;
        best = std::max(best, hi - lo + 1);
    }
    return best;
}

// Minimum window substring containing all chars of t — O(n + m)
std::string min_window(const std::string& s, const std::string& t) {
    std::unordered_map<char,int> need, have;
    for (char c : t) ++need[c];
    int formed = 0, required = need.size();
    int lo = 0, best_lo = 0, best_len = INT_MAX;

    for (int hi = 0; hi < (int)s.size(); ++hi) {
        ++have[s[hi]];
        if (need.count(s[hi]) && have[s[hi]] == need[s[hi]]) ++formed;

        while (formed == required) {
            if (hi - lo + 1 < best_len) { best_len = hi - lo + 1; best_lo = lo; }
            if (--have[s[lo]] < need[s[lo]]) --formed;
            ++lo;
        }
    }
    return best_len == INT_MAX ? "" : s.substr(best_lo, best_len);
}
```

---

## 5. Dynamic Programming

DP solves problems with **overlapping subproblems** and **optimal substructure** by storing results of subproblems to avoid recomputation.

### Top-Down (Memoization)

Recursive + cache. Natural to write, easy to see the recurrence.

```cpp
// Fibonacci — O(n) time, O(n) space
std::unordered_map<int, long long> memo;
long long fib(int n) {
    if (n <= 1) return n;
    if (memo.count(n)) return memo[n];
    return memo[n] = fib(n-1) + fib(n-2);
}
```

### Bottom-Up (Tabulation)

Iterative, fills a table from smallest subproblems up. Often more cache-friendly, no recursion overhead.

```cpp
// Fibonacci — O(n) time, O(1) space (optimized)
long long fib(int n) {
    if (n <= 1) return n;
    long long prev2 = 0, prev1 = 1;
    for (int i = 2; i <= n; ++i) {
        long long curr = prev1 + prev2;
        prev2 = prev1; prev1 = curr;
    }
    return prev1;
}
```

### Classic DP Problems

#### Longest Common Subsequence (LCS)

```cpp
// dp[i][j] = LCS length of s[0..i-1] and t[0..j-1]
int lcs(const std::string& s, const std::string& t) {
    int m = s.size(), n = t.size();
    std::vector<std::vector<int>> dp(m+1, std::vector<int>(n+1, 0));

    for (int i = 1; i <= m; ++i)
        for (int j = 1; j <= n; ++j)
            dp[i][j] = (s[i-1] == t[j-1])
                ? dp[i-1][j-1] + 1
                : std::max(dp[i-1][j], dp[i][j-1]);

    return dp[m][n];
}
// Time: O(mn) | Space: O(mn), reducible to O(min(m,n)) with rolling array
```

#### 0/1 Knapsack

```cpp
// dp[i][w] = max value using first i items with capacity w
int knapsack(const std::vector<int>& weights,
             const std::vector<int>& values, int W) {
    int n = weights.size();
    std::vector<std::vector<int>> dp(n+1, std::vector<int>(W+1, 0));

    for (int i = 1; i <= n; ++i)
        for (int w = 0; w <= W; ++w) {
            dp[i][w] = dp[i-1][w];  // don't take item i
            if (weights[i-1] <= w)
                dp[i][w] = std::max(dp[i][w],
                                    dp[i-1][w - weights[i-1]] + values[i-1]);
        }
    return dp[n][W];
}
// Time: O(nW) | Space: O(nW), reducible to O(W) with 1D rolling array
```

#### Coin Change (Minimum Coins)

```cpp
// dp[i] = minimum coins to make amount i
int coin_change(const std::vector<int>& coins, int amount) {
    std::vector<int> dp(amount + 1, amount + 1);  // init with "impossible"
    dp[0] = 0;
    for (int i = 1; i <= amount; ++i)
        for (int c : coins)
            if (c <= i) dp[i] = std::min(dp[i], dp[i - c] + 1);
    return dp[amount] > amount ? -1 : dp[amount];
}
// Time: O(amount × coins) | Space: O(amount)
```

#### Longest Increasing Subsequence (LIS)

```cpp
// O(n²) DP
int lis_n2(const std::vector<int>& arr) {
    int n = arr.size();
    std::vector<int> dp(n, 1);  // dp[i] = LIS ending at index i
    for (int i = 1; i < n; ++i)
        for (int j = 0; j < i; ++j)
            if (arr[j] < arr[i])
                dp[i] = std::max(dp[i], dp[j] + 1);
    return *std::max_element(dp.begin(), dp.end());
}

// O(n log n) — patience sorting with binary search
int lis_nlogn(const std::vector<int>& arr) {
    std::vector<int> tails;  // tails[i] = smallest tail of IS of length i+1
    for (int x : arr) {
        auto it = std::lower_bound(tails.begin(), tails.end(), x);
        if (it == tails.end()) tails.push_back(x);
        else                   *it = x;
    }
    return tails.size();
}
```

#### Edit Distance (Levenshtein)

```cpp
// Minimum insert/delete/replace to transform s into t
int edit_distance(const std::string& s, const std::string& t) {
    int m = s.size(), n = t.size();
    std::vector<std::vector<int>> dp(m+1, std::vector<int>(n+1));

    for (int i = 0; i <= m; ++i) dp[i][0] = i;  // delete all of s
    for (int j = 0; j <= n; ++j) dp[0][j] = j;  // insert all of t

    for (int i = 1; i <= m; ++i)
        for (int j = 1; j <= n; ++j) {
            if (s[i-1] == t[j-1])
                dp[i][j] = dp[i-1][j-1];         // match — no cost
            else
                dp[i][j] = 1 + std::min({dp[i-1][j],    // delete from s
                                         dp[i][j-1],    // insert into s
                                         dp[i-1][j-1]}); // replace
        }
    return dp[m][n];
}
```

#### Maximum Subarray (Kadane's Algorithm)

```cpp
// O(n) — dp[i] = max subarray sum ending at index i
int max_subarray(const std::vector<int>& arr) {
    int current = arr[0], best = arr[0];
    for (int i = 1; i < (int)arr.size(); ++i) {
        current = std::max(arr[i], current + arr[i]);  // extend or restart
        best    = std::max(best, current);
    }
    return best;
}
```

---

## 6. Greedy Algorithms

Make the **locally optimal** choice at each step. Correct when the greedy choice leads to a globally optimal solution (proven by exchange argument or matroid theory).

### Activity Selection (Interval Scheduling)

```cpp
// Select maximum number of non-overlapping intervals
int activity_selection(std::vector<std::pair<int,int>> intervals) {
    // Sort by end time — greedy: finish earliest first
    std::sort(intervals.begin(), intervals.end(),
              [](auto& a, auto& b) { return a.second < b.second; });

    int count = 0, last_end = INT_MIN;
    for (auto [start, end] : intervals) {
        if (start >= last_end) {  // no overlap with last selected
            ++count;
            last_end = end;
        }
    }
    return count;
}
```

### Interval Merging

```cpp
// Merge all overlapping intervals — O(n log n)
std::vector<std::pair<int,int>> merge_intervals(
    std::vector<std::pair<int,int>> intervals) {

    std::sort(intervals.begin(), intervals.end());
    std::vector<std::pair<int,int>> result;

    for (auto [s, e] : intervals) {
        if (!result.empty() && s <= result.back().second)
            result.back().second = std::max(result.back().second, e);
        else
            result.push_back({s, e});
    }
    return result;
}
```

### Huffman Coding

```cpp
// Build optimal prefix-free encoding using a min-heap
struct HNode {
    char ch; int freq;
    HNode *left, *right;
    bool operator>(const HNode& o) const { return freq > o.freq; }
};

HNode* build_huffman(const std::unordered_map<char,int>& freq_map) {
    auto cmp = [](HNode* a, HNode* b){ return a->freq > b->freq; };
    std::priority_queue<HNode*, std::vector<HNode*>, decltype(cmp)> pq(cmp);

    for (auto [c, f] : freq_map)
        pq.push(new HNode{c, f, nullptr, nullptr});

    while (pq.size() > 1) {
        auto left  = pq.top(); pq.pop();
        auto right = pq.top(); pq.pop();
        pq.push(new HNode{'\0', left->freq + right->freq, left, right});
    }
    return pq.top();
}
```

---

## 7. Backtracking

Explore all possibilities by building candidates incrementally and abandoning (pruning) those that fail constraints.

```
Template:
void backtrack(state) {
    if (is_solution(state)) { record(); return; }
    for (each choice) {
        if (is_valid(choice)) {
            apply(choice);
            backtrack(state);
            undo(choice);       ← restore state (the "backtrack" step)
        }
    }
}
```

### Permutations

```cpp
void permute(std::vector<int>& nums, int start,
             std::vector<std::vector<int>>& result) {
    if (start == (int)nums.size()) { result.push_back(nums); return; }
    for (int i = start; i < (int)nums.size(); ++i) {
        std::swap(nums[start], nums[i]);
        permute(nums, start + 1, result);
        std::swap(nums[start], nums[i]);   // undo
    }
}
// Time: O(n × n!) — n! permutations, each takes O(n) to copy
```

### N-Queens

```cpp
bool is_safe(const std::vector<int>& board, int row, int col) {
    for (int r = 0; r < row; ++r) {
        if (board[r] == col) return false;               // same column
        if (std::abs(board[r] - col) == row - r) return false; // diagonal
    }
    return true;
}

void solve_n_queens(int n, int row, std::vector<int>& board,
                    std::vector<std::vector<int>>& solutions) {
    if (row == n) { solutions.push_back(board); return; }
    for (int col = 0; col < n; ++col) {
        if (is_safe(board, row, col)) {
            board[row] = col;
            solve_n_queens(n, row + 1, board, solutions);
            board[row] = -1;  // undo
        }
    }
}
```

### Subset Sum

```cpp
void subsets(const std::vector<int>& nums, int start,
             std::vector<int>& current,
             std::vector<std::vector<int>>& result) {
    result.push_back(current);   // every partial state is a valid subset
    for (int i = start; i < (int)nums.size(); ++i) {
        current.push_back(nums[i]);
        subsets(nums, i + 1, current, result);
        current.pop_back();      // undo
    }
}
// Generates all 2^n subsets
```

---

## 8. String Algorithms

### KMP — Knuth-Morris-Pratt Pattern Matching

Find all occurrences of a pattern in a text. Never re-examines matched characters.

- O(n + m) time | O(m) space

```cpp
// Build failure function: lps[i] = length of longest proper prefix of
// pattern[0..i] that is also a suffix
std::vector<int> build_lps(const std::string& pattern) {
    int m = pattern.size();
    std::vector<int> lps(m, 0);
    int len = 0, i = 1;
    while (i < m) {
        if (pattern[i] == pattern[len]) { lps[i++] = ++len; }
        else if (len > 0) { len = lps[len - 1]; }
        else              { lps[i++] = 0; }
    }
    return lps;
}

std::vector<int> kmp_search(const std::string& text,
                            const std::string& pattern) {
    auto lps = build_lps(pattern);
    std::vector<int> matches;
    int i = 0, j = 0;
    while (i < (int)text.size()) {
        if (text[i] == pattern[j]) { ++i; ++j; }
        if (j == (int)pattern.size()) {
            matches.push_back(i - j);  // match at index i-j
            j = lps[j - 1];
        } else if (i < (int)text.size() && text[i] != pattern[j]) {
            j > 0 ? j = lps[j-1] : ++i;
        }
    }
    return matches;
}
```

### Rabin-Karp (Rolling Hash)

Multiple pattern search or detecting duplicate substrings.

```cpp
// Check if any permutation of pattern exists in s — O(n)
bool check_inclusion(const std::string& pattern, const std::string& s) {
    if (pattern.size() > s.size()) return false;
    std::array<int,26> need{}, have{};
    for (char c : pattern) ++need[c-'a'];
    int k = pattern.size(), formed = 0;
    int required = std::count_if(need.begin(), need.end(), [](int x){return x>0;});

    for (int i = 0; i < (int)s.size(); ++i) {
        ++have[s[i]-'a'];
        if (need[s[i]-'a'] && have[s[i]-'a'] == need[s[i]-'a']) ++formed;
        if (i >= k) {
            char out = s[i-k];
            if (need[out-'a'] && have[out-'a'] == need[out-'a']) --formed;
            --have[out-'a'];
        }
        if (formed == required) return true;
    }
    return false;
}
```

### Longest Palindromic Substring (Manacher's Algorithm)

O(n) — finds the longest palindromic substring.

```cpp
std::string longest_palindrome(const std::string& s) {
    // Expand around center — O(n²) (simpler to understand)
    int start = 0, max_len = 1;
    auto expand = [&](int l, int r) {
        while (l >= 0 && r < (int)s.size() && s[l] == s[r])
            { --l; ++r; }
        if (r - l - 1 > max_len) { max_len = r-l-1; start = l+1; }
    };
    for (int i = 0; i < (int)s.size(); ++i) {
        expand(i, i);    // odd length
        expand(i, i+1);  // even length
    }
    return s.substr(start, max_len);
}
```

---

## 9. Bit Manipulation

```cpp
// Test if bit k is set
bool is_set(int x, int k) { return (x >> k) & 1; }

// Set bit k
int set_bit(int x, int k) { return x | (1 << k); }

// Clear bit k
int clear_bit(int x, int k) { return x & ~(1 << k); }

// Toggle bit k
int toggle_bit(int x, int k) { return x ^ (1 << k); }

// Count set bits (popcount)
int popcount(int x) { return __builtin_popcount(x); }      // GCC/Clang intrinsic
int popcount(int x) { return std::popcount((unsigned)x); } // C++20

// Check power of 2
bool is_power_of_2(int x) { return x > 0 && (x & (x-1)) == 0; }

// Lowest set bit
int lowest_set_bit(int x) { return x & (-x); }

// Clear lowest set bit
int clear_lowest(int x) { return x & (x-1); }

// Common DP with bitmask: enumerate all subsets of a set S
for (int mask = 0; mask < (1 << n); ++mask) {
    // process subset 'mask' of n elements
    for (int i = 0; i < n; ++i)
        if ((mask >> i) & 1)
            /* element i is in this subset */;
}

// Enumerate all non-empty subsets of mask
for (int sub = mask; sub > 0; sub = (sub-1) & mask)
    /* process sub */;
```

### Bitmask DP — Travelling Salesman Problem

```cpp
// dp[mask][i] = min cost to visit exactly the cities in 'mask', ending at city i
int tsp(const std::vector<std::vector<int>>& dist) {
    int n = dist.size();
    int FULL = (1 << n) - 1;
    std::vector<std::vector<int>> dp(1<<n, std::vector<int>(n, INT_MAX/2));
    dp[1][0] = 0;  // start at city 0

    for (int mask = 1; mask <= FULL; ++mask)
        for (int u = 0; u < n; ++u) {
            if (!(mask & (1<<u)) || dp[mask][u] == INT_MAX/2) continue;
            for (int v = 0; v < n; ++v) {
                if (mask & (1<<v)) continue;  // already visited
                int next_mask = mask | (1<<v);
                dp[next_mask][v] = std::min(dp[next_mask][v],
                                            dp[mask][u] + dist[u][v]);
            }
        }

    int ans = INT_MAX;
    for (int u = 1; u < n; ++u)
        ans = std::min(ans, dp[FULL][u] + dist[u][0]);
    return ans;
}
// Time: O(2^n × n²) | Space: O(2^n × n) — feasible for n ≤ 20
```

---

## 10. STL Algorithm Library

```cpp
#include <algorithm>
#include <numeric>

// ── Sorting ──────────────────────────────────────────────────────────────
std::sort(v.begin(), v.end());                  // O(n log n) introsort
std::stable_sort(v.begin(), v.end());           // O(n log n) stable
std::partial_sort(v.begin(), v.begin()+k, v.end()); // first k sorted: O(n log k)
std::nth_element(v.begin(), v.begin()+k, v.end());  // k-th element: O(n) average

// ── Searching ────────────────────────────────────────────────────────────
std::find(v.begin(), v.end(), val);             // O(n) linear search
std::binary_search(v.begin(), v.end(), val);    // O(log n) — sorted required
std::lower_bound(v.begin(), v.end(), val);      // O(log n) — first >= val
std::upper_bound(v.begin(), v.end(), val);      // O(log n) — first > val
std::min_element(v.begin(), v.end());           // O(n)
std::max_element(v.begin(), v.end());           // O(n)
std::minmax_element(v.begin(), v.end());        // O(n) — returns pair of iterators

// ── Modification ─────────────────────────────────────────────────────────
std::fill(v.begin(), v.end(), 0);
std::transform(v.begin(), v.end(), out.begin(), [](int x){ return x*2; });
std::copy(src.begin(), src.end(), dst.begin());
std::reverse(v.begin(), v.end());
std::rotate(v.begin(), v.begin()+k, v.end());  // rotate left by k
std::shuffle(v.begin(), v.end(), std::mt19937{std::random_device{}()});

// ── Erase-remove idiom ───────────────────────────────────────────────────
v.erase(std::remove(v.begin(), v.end(), val), v.end());             // remove value
v.erase(std::remove_if(v.begin(), v.end(), pred), v.end());         // remove if

// ── Aggregates ───────────────────────────────────────────────────────────
std::accumulate(v.begin(), v.end(), 0);           // sum
std::reduce(v.begin(), v.end());                  // C++17, parallel-friendly
std::count(v.begin(), v.end(), val);
std::count_if(v.begin(), v.end(), pred);
std::all_of(v.begin(), v.end(), pred);
std::any_of(v.begin(), v.end(), pred);
std::none_of(v.begin(), v.end(), pred);

// ── Set operations (on sorted ranges) ────────────────────────────────────
std::set_union(a.begin(), a.end(), b.begin(), b.end(), out.begin());
std::set_intersection(a.begin(), a.end(), b.begin(), b.end(), out.begin());
std::set_difference(a.begin(), a.end(), b.begin(), b.end(), out.begin());

// ── Permutations ─────────────────────────────────────────────────────────
std::next_permutation(v.begin(), v.end());  // advance to next lexicographic permutation
std::prev_permutation(v.begin(), v.end());

// ── C++20 Ranges (composable, lazy) ──────────────────────────────────────
namespace rv = std::ranges::views;
auto evens = v | rv::filter([](int x){ return x % 2 == 0; })
               | rv::transform([](int x){ return x * x; });
std::ranges::sort(v);
```

---

## Algorithm Selection Guide

```
Problem type                    Algorithm to reach for
──────────────────              ─────────────────────────────────────────
Find element in sorted array    Binary search                O(log n)
Find element in unsorted        Linear scan / hash set       O(n) / O(1)
Shortest path, no weights       BFS                          O(V+E)
Shortest path, non-neg weights  Dijkstra                     O((V+E) log V)
Shortest path, neg weights      Bellman-Ford                 O(VE)
All-pairs shortest path         Floyd-Warshall               O(V³)
Minimum spanning tree           Kruskal (DSU) / Prim         O(E log E)
Task ordering with dependencies Topological sort             O(V+E)
Connected components            Union-Find / BFS/DFS         O(α(n)) / O(V+E)
Overlapping subproblems         Dynamic programming          varies
Count/enumerate arrangements    Backtracking                 exponential
Pattern in text                 KMP / Rabin-Karp             O(n+m)
Sort general data               std::sort (introsort)        O(n log n)
Sort integers, bounded range    Counting / Radix sort        O(n+k)
Contiguous subarray extreme     Sliding window / Kadane's    O(n)
Pair sum in sorted array        Two pointers                 O(n)
Subset / bitmask over small n   Bitmask DP                   O(2^n × n)
```

---

## Mock Interview Questions

---

**[Mid]** When would you use BFS over DFS, and vice versa?

> **BFS** — use when you need the **shortest path in an unweighted graph** (fewest edges). BFS guarantees that the first time it reaches a vertex, it has found the minimum number of hops. It processes vertices level by level using a queue.
>
> Use BFS for: shortest path in unweighted graphs, level-order traversal of a tree, finding all vertices within a given distance, web crawlers (discover pages breadth-first).
>
> **DFS** — use when you need to explore the **full structure** of the graph: cycle detection, topological ordering, strongly connected components, finding any path (not necessarily shortest), generating permutations or combinations via backtracking.
>
> Use DFS for: topological sort, cycle detection, maze solving, connected components, generating all paths.
>
> **Memory**: BFS can use O(V) queue memory on a wide graph (many nodes at same depth). DFS uses O(depth) stack space — better for deep sparse graphs but risks stack overflow on very deep recursion.

---

**[Mid]** Explain how Dijkstra's algorithm works and what its limitation is.

> Dijkstra maintains a **min-heap** (priority queue) of `{tentative_distance, vertex}` pairs and a `dist[]` array initialized to infinity for all vertices except the source (dist[src] = 0).
>
> Each iteration: extract the vertex u with the smallest tentative distance. For each neighbor v, if `dist[u] + weight(u,v) < dist[v]`, update `dist[v]` and push `{dist[v], v}` onto the heap.
>
> The key invariant: when a vertex is extracted from the heap, its distance is **finalized** — no shorter path can exist because all future extractions will have larger or equal distances (greedy argument: we always process the minimum first).
>
> **Limitation**: Dijkstra does **not work with negative edge weights**. A negative edge could create a shorter path to an already-finalized vertex, violating the invariant. Use **Bellman-Ford** for negative weights (O(VE)) or **SPFA** as a practical optimization.

---

**[Mid]** What is the difference between memoization and tabulation in dynamic programming?

> Both avoid recomputing overlapping subproblems, but differ in direction:
>
> **Memoization (top-down)**: write the natural recursion, add a cache. Only computes subproblems that are actually needed. Easier to implement when the state space is sparse or the recurrence is complex.
>
> ```cpp
> // Only computes fib(n), fib(n-1), ... — no wasted work
> unordered_map<int,long long> memo;
> long long fib(int n) {
>     if (n<=1) return n;
>     if (memo.count(n)) return memo[n];
>     return memo[n] = fib(n-1) + fib(n-2);
> }
> ```
>
> **Tabulation (bottom-up)**: fill a table iteratively from base cases up. No recursion overhead, better cache access pattern (sequential memory), easier to optimize space.
>
> ```cpp
> // Computes all fib(0) to fib(n) — may compute unnecessary states
> long long fib(int n) {
>     vector<long long> dp(n+1);
>     dp[0]=0; dp[1]=1;
>     for(int i=2;i<=n;++i) dp[i]=dp[i-1]+dp[i-2];
>     return dp[n];
> }
> ```
>
> **Choose memoization** when: the recurrence is natural to write recursively, the state space is sparse (not all states visited), or transitions are complex.
> **Choose tabulation** when: all states are visited, you want to optimize space by discarding old rows, or recursion depth would cause stack overflow.

---

**[Senior]** How would you find the k-th largest element in an unsorted array efficiently?

> **Option 1 — `nth_element` / QuickSelect: O(n) average, O(n²) worst**
>
> QuickSelect partitions the array like QuickSort but only recurses into the half containing the k-th element:
>
> ```cpp
> int kth_largest(std::vector<int>& arr, int k) {
>     int target = arr.size() - k;  // k-th largest = (n-k)-th smallest
>     int lo = 0, hi = arr.size() - 1;
>     while (lo < hi) {
>         int p = partition(arr, lo, hi);  // Lomuto or Hoare
>         if      (p < target) lo = p + 1;
>         else if (p > target) hi = p - 1;
>         else                 return arr[p];
>     }
>     return arr[lo];
> }
> // std::nth_element(arr.begin(), arr.begin()+(n-k), arr.end()); — same idea
> ```
>
> **Option 2 — Min-heap of size k: O(n log k)**
>
> ```cpp
> int kth_largest(const std::vector<int>& arr, int k) {
>     std::priority_queue<int, std::vector<int>, std::greater<int>> pq; // min-heap
>     for (int x : arr) {
>         pq.push(x);
>         if ((int)pq.size() > k) pq.pop();  // keep only k largest
>     }
>     return pq.top();  // smallest of the k largest = k-th largest
> }
> ```
>
> **When to use which**:
> - QuickSelect if array fits in memory and average O(n) is acceptable
> - Min-heap if processing a **stream** (don't have all elements upfront) or k ≪ n
> - Sort + index: O(n log n) — only when you'll also need other order statistics

---

**[Senior]** Explain how Union-Find achieves near O(1) operations. What are path compression and union by rank?

> Union-Find (Disjoint Set Union) represents each set as a tree. `find(x)` returns the root of x's tree (the representative). `union(x,y)` merges the two trees.
>
> Without optimizations, find is O(depth) — a chain-shaped tree gives O(n).
>
> **Path compression**: after `find(x)` traverses up to the root, it sets the parent of every visited node directly to the root. Future finds on those nodes are O(1).
>
> ```cpp
> int find(int x) {
>     if (parent[x] != x) parent[x] = find(parent[x]);  // compress path
>     return parent[x];
> }
> ```
>
> **Union by rank**: always attach the shorter tree under the taller one. Without this, union could create chains. Rank is an upper bound on tree height.
>
> ```cpp
> void unite(int x, int y) {
>     int px = find(x), py = find(y);
>     if (rank[px] < rank[py]) std::swap(px, py);
>     parent[py] = px;
>     if (rank[px] == rank[py]) ++rank[px];
> }
> ```
>
> **Combined complexity**: with both optimizations, any sequence of m operations on n elements runs in O(m × α(n)) total time, where α is the inverse Ackermann function — α(n) < 5 for any practical n. Effectively O(1) per operation amortized.
