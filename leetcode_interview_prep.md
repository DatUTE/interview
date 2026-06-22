# LeetCode — Danh sách ôn phỏng vấn (Easy → Medium)

~**10 bài/concept**, sắp xếp từ dễ đến khó trong concept. Ưu tiên pattern hay gặp phỏng vấn **Senior C++ / backend / systems** (kèm coding test).

**Cách ôn:** làm **1→10** trong mỗi concept; nếu kẹt >20 phút, đọc hint, code lại từ đầu sau 24h.

**Liên quan:** [C++ App/algorithms.md](C++%20App/algorithms.md) · [C++ App/stl_containers.md](C++%20App/stl_containers.md)

---

## Lộ trình 4 tuần (gợi ý)

| Tuần | Concepts |
|------|----------|
| 1 | Array/Hash, Two Pointers, String, Stack, Queue |
| 2 | Sliding Window, Binary Search, Linked List |
| 3 | Tree (BFS/DFS), Heap, Intervals |
| 4 | DP (1D/2D), Backtracking, Graph cơ bản |

---

## 1. Array & Hash Map

**Pattern:** `unordered_map` / `unordered_set` — O(1) lookup, đếm frequency, complement.

| # | Bài | LC | Level | Ghi chú |
|---|-----|-----|-------|---------|
| 1 | Two Sum | [1](https://leetcode.com/problems/two-sum/) | Easy | Hash map `target - x` |
| 2 | Contains Duplicate | [217](https://leetcode.com/problems/contains-duplicate/) | Easy | Set |
| 3 | Valid Anagram | [242](https://leetcode.com/problems/valid-anagram/) | Easy | Frequency array [26] |
| 4 | Majority Element | [169](https://leetcode.com/problems/majority-element/) | Easy | Boyer-Moore hoặc hash |
| 5 | Intersection of Two Arrays II | [350](https://leetcode.com/problems/intersection-of-two-arrays-ii/) | Easy | Frequency map |
| 6 | Group Anagrams | [49](https://leetcode.com/problems/group-anagrams/) | Medium | Key = sorted string hoặc count |
| 7 | Top K Frequent Elements | [347](https://leetcode.com/problems/top-k-frequent-elements/) | Medium | Hash + heap / bucket |
| 8 | Product of Array Except Self | [238](https://leetcode.com/problems/product-of-array-except-self/) | Medium | Prefix/suffix, O(1) extra |
| 9 | Longest Consecutive Sequence | [128](https://leetcode.com/problems/longest-consecutive-sequence/) | Medium | Set + expand from start |
| 10 | Subarray Sum Equals K | [560](https://leetcode.com/problems/subarray-sum-equals-k/) | Medium | Prefix sum + hash |

---

## 2. Two Pointers

**Pattern:** left/right hoặc slow/fast trên array/string đã sort hoặc có structure.

| # | Bài | LC | Level | Ghi chú |
|---|-----|-----|-------|---------|
| 1 | Merge Sorted Array | [88](https://leetcode.com/problems/merge-sorted-array/) | Easy | Fill từ cuối |
| 2 | Remove Duplicates from Sorted Array | [26](https://leetcode.com/problems/remove-duplicates-from-sorted-array/) | Easy | Slow/fast |
| 3 | Move Zeroes | [283](https://leetcode.com/problems/move-zeroes/) | Easy | Write pointer |
| 4 | Valid Palindrome | [125](https://leetcode.com/problems/valid-palindrome/) | Easy | Two pointers in/out |
| 5 | Two Sum II - Input Array Is Sorted | [167](https://leetcode.com/problems/two-sum-ii-input-array-is-sorted/) | Medium | L/R converge |
| 6 | 3Sum | [15](https://leetcode.com/problems/3sum/) | Medium | Sort + fix i + two sum |
| 7 | Container With Most Water | [11](https://leetcode.com/problems/container-with-most-water/) | Medium | Greedy L/R |
| 8 | Sort Colors | [75](https://leetcode.com/problems/sort-colors/) | Medium | Dutch national flag |
| 9 | Trapping Rain Water | [42](https://leetcode.com/problems/trapping-rain-water/) | Hard* | L/R max — stretch goal |
| 10 | 4Sum | [18](https://leetcode.com/problems/4sum/) | Medium | Extension 3Sum |

\* #9 Hard — làm sau khi thuần #6–8; pattern two pointers rất hay hỏi.

---

## 3. String

**Pattern:** frequency, two pointers, parsing, `string_view` trong C++.

| # | Bài | LC | Level | Ghi chú |
|---|-----|-----|-------|---------|
| 1 | Reverse String | [344](https://leetcode.com/problems/reverse-string/) | Easy | Two pointers |
| 2 | Valid Palindrome II | [680](https://leetcode.com/problems/valid-palindrome-ii/) | Easy | Skip one char |
| 3 | First Unique Character | [387](https://leetcode.com/problems/first-unique-character-in-a-string/) | Easy | Count [26] |
| 4 | Longest Common Prefix | [14](https://leetcode.com/problems/longest-common-prefix/) | Easy | Vertical scan |
| 5 | Implement strStr() | [28](https://leetcode.com/problems/find-the-index-of-the-first-occurrence-in-a-string/) | Easy | KMP optional |
| 6 | Longest Substring Without Repeating | [3](https://leetcode.com/problems/longest-substring-without-repeating-characters/) | Medium | Sliding window + set |
| 7 | Group Anagrams | [49](https://leetcode.com/problems/group-anagrams/) | Medium | (trùng hash) |
| 8 | String to Integer (atoi) | [8](https://leetcode.com/problems/string-to-integer-atoi/) | Medium | Edge cases, overflow |
| 9 | Longest Palindromic Substring | [5](https://leetcode.com/problems/longest-palindromic-substring/) | Medium | Expand center |
| 10 | Decode String | [394](https://leetcode.com/problems/decode-string/) | Medium | Stack + build string |

---

## 4. Stack

**Pattern:** LIFO — matching brackets, monotonic stack, simulate recursion.

| # | Bài | LC | Level | Ghi chú |
|---|-----|-----|-------|---------|
| 1 | Valid Parentheses | [20](https://leetcode.com/problems/valid-parentheses/) | Easy | Classic stack |
| 2 | Min Stack | [155](https://leetcode.com/problems/min-stack/) | Medium | Auxiliary stack |
| 3 | Implement Queue using Stacks | [232](https://leetcode.com/problems/implement-queue-using-stacks/) | Easy | 2 stacks |
| 4 | Next Greater Element I | [496](https://leetcode.com/problems/next-greater-element-i/) | Easy | Monotonic stack intro |
| 5 | Daily Temperatures | [739](https://leetcode.com/problems/daily-temperatures/) | Medium | Monotonic stack |
| 6 | Evaluate Reverse Polish Notation | [150](https://leetcode.com/problems/evaluate-reverse-polish-notation/) | Medium | Stack eval |
| 7 | Simplify Path | [71](https://leetcode.com/problems/simplify-path/) | Medium | Stack path parts |
| 8 | Asteroid Collision | [735](https://leetcode.com/problems/asteroid-collision/) | Medium | Stack simulate |
| 9 | Remove K Digits | [402](https://leetcode.com/problems/remove-k-digits/) | Medium | Monotonic increasing |
| 10 | Largest Rectangle in Histogram | [84](https://leetcode.com/problems/largest-rectangle-in-histogram/) | Hard* | Monotonic stack — PV senior |

---

## 5. Queue & BFS (level-order)

**Pattern:** `std::queue` — BFS tree/graph, sliding window max (deque).

| # | Bài | LC | Level | Ghi chú |
|---|-----|-----|-------|---------|
| 1 | Implement Stack using Queues | [225](https://leetcode.com/problems/implement-stack-using-queues/) | Easy | 1 hoặc 2 queues |
| 2 | Number of Recent Calls | [933](https://leetcode.com/problems/number-of-recent-calls/) | Easy | Queue time window |
| 3 | Binary Tree Level Order Traversal | [102](https://leetcode.com/problems/binary-tree-level-order-traversal/) | Medium | BFS + queue |
| 4 | Symmetric Tree | [101](https://leetcode.com/problems/symmetric-tree/) | Easy | BFS/DFS pair |
| 5 | Rotting Oranges | [994](https://leetcode.com/problems/rotting-oranges/) | Medium | Multi-source BFS |
| 6 | Perfect Squares | [279](https://leetcode.com/problems/perfect-squares/) | Medium | BFS shortest path |
| 7 | Open the Lock | [752](https://leetcode.com/problems/open-the-lock/) | Medium | BFS state space |
| 8 | Sliding Window Maximum | [239](https://leetcode.com/problems/sliding-window-maximum/) | Hard* | **Deque** monotonic |
| 9 | Course Schedule | [207](https://leetcode.com/problems/course-schedule/) | Medium | BFS Kahn / DFS cycle |
| 10 | Word Ladder | [127](https://leetcode.com/problems/word-ladder/) | Hard* | BFS shortest transform |

---

## 6. Sliding Window

**Pattern:** cửa sổ cố định hoặc co giãn; hash đếm trong window.

| # | Bài | LC | Level | Ghi chú |
|---|-----|-----|-------|---------|
| 1 | Maximum Average Subarray I | [643](https://leetcode.com/problems/maximum-average-subarray-i/) | Easy | Fixed size k |
| 2 | Minimum Size Subarray Sum | [209](https://leetcode.com/problems/minimum-size-subarray-sum/) | Medium | Variable window |
| 3 | Longest Substring Without Repeating | [3](https://leetcode.com/problems/longest-substring-without-repeating-characters/) | Medium | Shrink khi duplicate |
| 4 | Fruits Into Baskets | [904](https://leetcode.com/problems/fruit-into-baskets/) | Medium | At most 2 types |
| 5 | Permutation in String | [567](https://leetcode.com/problems/permutation-in-string/) | Medium | Fixed window + count |
| 6 | Find All Anagrams in a String | [438](https://leetcode.com/problems/find-all-anagrams-in-a-string/) | Medium | Same pattern |
| 7 | Longest Repeating Character Replacement | [424](https://leetcode.com/problems/longest-repeating-character-replacement/) | Medium | max_freq trick |
| 8 | Subarray Product Less Than K | [713](https://leetcode.com/problems/subarray-product-less-than-k/) | Medium | Variable window |
| 9 | Minimum Window Substring | [76](https://leetcode.com/problems/minimum-window-substring/) | Hard* | Classic hard medium |
| 10 | Sliding Window Maximum | [239](https://leetcode.com/problems/sliding-window-maximum/) | Hard* | Deque |

---

## 7. Binary Search

**Pattern:** sorted array, answer space binary search, lower/upper bound.

| # | Bài | LC | Level | Ghi chú |
|---|-----|-----|-------|---------|
| 1 | Binary Search | [704](https://leetcode.com/problems/binary-search/) | Easy | Template cơ bản |
| 2 | First Bad Version | [278](https://leetcode.com/problems/first-bad-version/) | Easy | Find first true |
| 3 | Search Insert Position | [35](https://leetcode.com/problems/search-insert-position/) | Easy | lower_bound |
| 4 | Sqrt(x) | [69](https://leetcode.com/problems/sqrtx/) | Easy | BS on answer |
| 5 | Guess Number Higher or Lower | [374](https://leetcode.com/problems/guess-number-higher-or-lower/) | Easy | |
| 6 | Search in Rotated Sorted Array | [33](https://leetcode.com/problems/search-in-rotated-sorted-array/) | Medium | Pivot half sorted |
| 7 | Find Minimum in Rotated Sorted Array | [153](https://leetcode.com/problems/find-minimum-in-rotated-sorted-array/) | Medium | Compare mid vs right |
| 8 | Koko Eating Bananas | [875](https://leetcode.com/problems/koko-eating-bananas/) | Medium | BS on speed |
| 9 | Find Peak Element | [162](https://leetcode.com/problems/find-peak-element/) | Medium | BS local max |
| 10 | Median of Two Sorted Arrays | [4](https://leetcode.com/problems/median-of-two-sorted-arrays/) | Hard* | Partition BS — stretch |

**C++ template:**

```cpp
// lower_bound style — first index where ok
int lo = 0, hi = n;
while (lo < hi) {
    int mid = lo + (hi - lo) / 2;
    if (check(mid)) hi = mid;
    else lo = mid + 1;
}
```

---

## 8. Linked List

**Pattern:** dummy head, fast/slow, reverse, merge.

| # | Bài | LC | Level | Ghi chú |
|---|-----|-----|-------|---------|
| 1 | Reverse Linked List | [206](https://leetcode.com/problems/reverse-linked-list/) | Easy | Iterative 3 ptr |
| 2 | Merge Two Sorted Lists | [21](https://leetcode.com/problems/merge-two-sorted-lists/) | Easy | Dummy node |
| 3 | Linked List Cycle | [141](https://leetcode.com/problems/linked-list-cycle/) | Easy | Floyd slow/fast |
| 4 | Middle of the Linked List | [876](https://leetcode.com/problems/middle-of-the-linked-list/) | Easy | Fast 2x slow |
| 5 | Remove Nth Node From End | [19](https://leetcode.com/problems/remove-nth-node-from-end-of-list/) | Medium | Two pointers gap n |
| 6 | Add Two Numbers | [2](https://leetcode.com/problems/add-two-numbers/) | Medium | Digit carry |
| 7 | Reorder List | [143](https://leetcode.com/problems/reorder-list/) | Medium | Find mid + reverse |
| 8 | Copy List with Random Pointer | [138](https://leetcode.com/problems/copy-list-with-random-pointer/) | Medium | Hash old→new |
| 9 | LRU Cache | [146](https://leetcode.com/problems/lru-cache/) | Medium | Hash + list — **PV hot** |
| 10 | Merge k Sorted Lists | [23](https://leetcode.com/problems/merge-k-sorted-lists/) | Hard* | Heap / divide |

---

## 9. Tree — DFS & BFS

**Pattern:** recursion, `max(left,right)+1`, path sum, level order.

| # | Bài | LC | Level | Ghi chú |
|---|-----|-----|-------|---------|
| 1 | Maximum Depth of Binary Tree | [104](https://leetcode.com/problems/maximum-depth-of-binary-tree/) | Easy | DFS |
| 2 | Same Tree | [100](https://leetcode.com/problems/same-tree/) | Easy | |
| 3 | Invert Binary Tree | [226](https://leetcode.com/problems/invert-binary-tree/) | Easy | |
| 4 | Diameter of Binary Tree | [543](https://leetcode.com/problems/diameter-of-binary-tree/) | Easy | DFS height |
| 5 | Subtree of Another Tree | [572](https://leetcode.com/problems/subtree-of-another-tree/) | Easy | |
| 6 | Lowest Common Ancestor of BST | [235](https://leetcode.com/problems/lowest-common-ancestor-of-a-binary-search-tree/) | Medium | BST property |
| 7 | Binary Tree Right Side View | [199](https://leetcode.com/problems/binary-tree-right-side-view/) | Medium | BFS last / DFS depth |
| 8 | Validate Binary Search Tree | [98](https://leetcode.com/problems/validate-binary-search-tree/) | Medium | Range min/max |
| 9 | Kth Smallest Element in BST | [230](https://leetcode.com/problems/kth-smallest-element-in-a-bst/) | Medium | Inorder |
| 10 | Serialize and Deserialize Binary Tree | [297](https://leetcode.com/problems/serialize-and-deserialize-binary-tree/) | Hard* | BFS/DFS encode |

---

## 10. Heap / Priority Queue

**Pattern:** `priority_queue` — top K, merge streams, scheduling.

| # | Bài | LC | Level | Ghi chú |
|---|-----|-----|-------|---------|
| 1 | Last Stone Weight | [1046](https://leetcode.com/problems/last-stone-weight/) | Easy | Max heap |
| 2 | Kth Largest Element in Array | [215](https://leetcode.com/problems/kth-largest-element-in-an-array/) | Medium | Min heap size k |
| 3 | Top K Frequent Elements | [347](https://leetcode.com/problems/top-k-frequent-elements/) | Medium | |
| 4 | Merge k Sorted Lists | [23](https://leetcode.com/problems/merge-k-sorted-lists/) | Hard* | Min heap |
| 5 | Find K Pairs with Smallest Sums | [373](https://leetcode.com/problems/find-k-pairs-with-smallest-sums/) | Medium | Heap |
| 6 | Task Scheduler | [621](https://leetcode.com/problems/task-scheduler/) | Medium | Greedy + heap |
| 7 | Reorganize String | [767](https://leetcode.com/problems/reorganize-string/) | Medium | Max heap |
| 8 | Ugly Number II | [264](https://leetcode.com/problems/ugly-number-ii/) | Medium | Min heap / DP |
| 9 | Find Median from Data Stream | [295](https://leetcode.com/problems/find-median-from-data-stream/) | Hard* | Two heaps — classic |
| 10 | IPO | [502](https://leetcode.com/problems/ipo/) | Hard* | Greedy + heaps |

---

## 11. Intervals

**Pattern:** sort by start, merge, sweep line.

| # | Bài | LC | Level | Ghi chú |
|---|-----|-----|-------|---------|
| 1 | Meeting Rooms | [252](https://leetcode.com/problems/meeting-rooms/) | Easy | Sort + compare |
| 2 | Can Attend Meetings | [252](https://leetcode.com/problems/meeting-rooms/) | Easy | |
| 3 | Merge Intervals | [56](https://leetcode.com/problems/merge-intervals/) | Medium | **Must know** |
| 4 | Insert Interval | [57](https://leetcode.com/problems/insert-interval/) | Medium | Merge one |
| 5 | Non-overlapping Intervals | [435](https://leetcode.com/problems/non-overlapping-intervals/) | Medium | Greedy by end |
| 6 | Minimum Number of Arrows | [452](https://leetcode.com/problems/minimum-number-of-arrows-to-burst-balloons/) | Medium | Same greedy |
| 7 | Meeting Rooms II | [253](https://leetcode.com/problems/meeting-rooms-ii/) | Medium | Heap / sweep |
| 8 | Employee Free Time | [759](https://leetcode.com/problems/employee-free-time/) | Hard* | Merge lists |
| 9 | Interval List Intersections | [986](https://leetcode.com/problems/interval-list-intersections/) | Medium | Two pointers |
| 10 | My Calendar I | [729](https://leetcode.com/problems/my-calendar-i/) | Medium | Tree or list |

---

## 12. Dynamic Programming (1D / 2D)

**Pattern:** define state, transition, base case; space optimize.

| # | Bài | LC | Level | Ghi chú |
|---|-----|-----|-------|---------|
| 1 | Climbing Stairs | [70](https://leetcode.com/problems/climbing-stairs/) | Easy | Fibonacci |
| 2 | Min Cost Climbing Stairs | [746](https://leetcode.com/problems/min-cost-climbing-stairs/) | Easy | |
| 3 | House Robber | [198](https://leetcode.com/problems/house-robber/) | Medium | dp[i-1], dp[i-2] |
| 4 | Coin Change | [322](https://leetcode.com/problems/coin-change/) | Medium | Unbounded knapsack |
| 5 | Longest Increasing Subsequence | [300](https://leetcode.com/problems/longest-increasing-subsequence/) | Medium | DP O(n²) or BS O(n log n) |
| 6 | Word Break | [139](https://leetcode.com/problems/word-break/) | Medium | dp[i] reachable |
| 7 | Unique Paths | [62](https://leetcode.com/problems/unique-paths/) | Medium | Grid DP |
| 8 | Longest Common Subsequence | [1143](https://leetcode.com/problems/longest-common-subsequence/) | Medium | 2D DP |
| 9 | Edit Distance | [72](https://leetcode.com/problems/edit-distance/) | Medium | Classic 2D |
| 10 | Decode Ways | [91](https://leetcode.com/problems/decode-ways/) | Medium | String DP |

---

## 13. Backtracking

**Pattern:** choose → explore → unchoose; pruning.

| # | Bài | LC | Level | Ghi chú |
|---|-----|-----|-------|---------|
| 1 | Subsets | [78](https://leetcode.com/problems/subsets/) | Medium | Include/exclude |
| 2 | Subsets II | [90](https://leetcode.com/problems/subsets-ii/) | Medium | Sort + skip duplicate |
| 3 | Permutations | [46](https://leetcode.com/problems/permutations/) | Medium | Swap / used[] |
| 4 | Permutations II | [47](https://leetcode.com/problems/permutations-ii/) | Medium | Duplicate handling |
| 5 | Combination Sum | [39](https://leetcode.com/problems/combination-sum/) | Medium | Reuse allowed |
| 6 | Combination Sum II | [40](https://leetcode.com/problems/combination-sum-ii/) | Medium | No reuse, dedup |
| 7 | Letter Combinations of Phone Number | [17](https://leetcode.com/problems/letter-combinations-of-a-phone-number/) | Medium | |
| 8 | Generate Parentheses | [22](https://leetcode.com/problems/generate-parentheses/) | Medium | open/close count |
| 9 | Palindrome Partitioning | [131](https://leetcode.com/problems/palindrome-partitioning/) | Medium | Cut + check |
| 10 | N-Queens | [51](https://leetcode.com/problems/n-queens/) | Hard* | Classic backtrack |

---

## 14. Graph (BFS / DFS / Union-Find)

**Pattern:** adjacency list, visited, topo sort, connected components.

| # | Bài | LC | Level | Ghi chú |
|---|-----|-----|-------|---------|
| 1 | Find if Path Exists | [1971](https://leetcode.com/problems/find-if-path-exists-in-graph/) | Easy | DFS/BFS |
| 2 | Number of Islands | [200](https://leetcode.com/problems/number-of-islands/) | Medium | Grid DFS/BFS |
| 3 | Flood Fill | [733](https://leetcode.com/problems/flood-fill/) | Easy | DFS |
| 4 | Clone Graph | [133](https://leetcode.com/problems/clone-graph/) | Medium | Hash map clone |
| 5 | Pacific Atlantic Water Flow | [417](https://leetcode.com/problems/pacific-atlantic-water-flow/) | Medium | Multi-source DFS |
| 6 | Course Schedule | [207](https://leetcode.com/problems/course-schedule/) | Medium | Cycle detection |
| 7 | Course Schedule II | [210](https://leetcode.com/problems/course-schedule-ii/) | Medium | Topo order |
| 8 | Number of Connected Components | [323](https://leetcode.com/problems/number-of-connected-components-in-an-undirected-graph/) | Medium | Union-Find |
| 9 | Redundant Connection | [684](https://leetcode.com/problems/redundant-connection/) | Medium | Union-Find cycle |
| 10 | Word Search | [79](https://leetcode.com/problems/word-search/) | Medium | Grid backtrack |

---

## 15. Trie

**Pattern:** prefix tree — autocomplete, word search.

| # | Bài | LC | Level | Ghi chú |
|---|-----|-----|-------|---------|
| 1 | Implement Trie | [208](https://leetcode.com/problems/implement-trie-prefix-tree/) | Medium | **Must implement** |
| 2 | Design Add and Search Words | [211](https://leetcode.com/problems/design-add-and-search-word-data-structure/) | Medium | Trie + DFS '.' |
| 3 | Longest Word in Dictionary | [720](https://leetcode.com/problems/longest-word-in-dictionary/) | Medium | Build trie |
| 4 | Replace Words | [648](https://leetcode.com/problems/replace-words/) | Medium | Prefix replace |
| 5 | Map Sum Pairs | [677](https://leetcode.com/problems/map-sum-pairs/) | Medium | Trie + value |
| 6 | Word Search II | [212](https://leetcode.com/problems/word-search-ii/) | Hard* | Trie + grid DFS |
| 7 | Prefix and Suffix Search | [745](https://leetcode.com/problems/prefix-and-suffix-search/) | Hard* | Design |
| 8 | Search Suggestions System | [1268](https://leetcode.com/problems/search-suggestions-system/) | Medium | Trie / sort |
| 9 | Maximum XOR of Two Numbers | [421](https://leetcode.com/problems/maximum-xor-of-two-numbers-in-an-array/) | Medium | Bit trie |
| 10 | Index Pairs of a String | [1065](https://leetcode.com/problems/index-pairs-of-a-string/) | Easy | Trie keywords |

---

## 16. Bit Manipulation

**Pattern:** XOR, masks, `n & (n-1)`.

| # | Bài | LC | Level | Ghi chú |
|---|-----|-----|-------|---------|
| 1 | Number of 1 Bits | [191](https://leetcode.com/problems/number-of-1-bits/) | Easy | Hamming weight |
| 2 | Reverse Bits | [190](https://leetcode.com/problems/reverse-bits/) | Easy | |
| 3 | Power of Two | [231](https://leetcode.com/problems/power-of-two/) | Easy | `n & (n-1)` |
| 4 | Missing Number | [268](https://leetcode.com/problems/missing-number/) | Easy | XOR all |
| 5 | Single Number | [136](https://leetcode.com/problems/single-number/) | Easy | XOR |
| 6 | Counting Bits | [338](https://leetcode.com/problems/counting-bits/) | Easy | DP + bit |
| 7 | Single Number II | [137](https://leetcode.com/problems/single-number-ii/) | Medium | Bit count mod 3 |
| 8 | Sum of Two Integers | [371](https://leetcode.com/problems/sum-of-two-integers/) | Medium | XOR + carry |
| 9 | Subsets (bitmask) | [78](https://leetcode.com/problems/subsets/) | Medium | 1<<n iteration |
| 10 | Maximum XOR of Two Numbers | [421](https://leetcode.com/problems/maximum-xor-of-two-numbers-in-an-array/) | Medium | Trie bits |

---

## Must-do trước phỏng vấn (top 20 rút gọn)

Nếu thiếu thời gian, ưu tiên:

1. Two Sum · 2. Valid Parentheses · 3. Merge Two Sorted Lists · 4. Maximum Depth  
5. Climbing Stairs · 6. House Robber · 7. Binary Search · 8. Search Rotated Array  
9. LRU Cache · 10. Merge Intervals · 11. Longest Substring Without Repeating  
12. 3Sum · 13. Number of Islands · 14. Course Schedule · 15. Subsets  
16. Permutations · 17. Word Break · 18. Kth Largest · 19. Implement Trie  
20. Min Window Substring hoặc Trapping Rain Water

---

## Tips phỏng vấn C++ (khi code LeetCode)

```cpp
// Prefer
std::vector<int> v;
v.reserve(n);
std::unordered_map<int,int> mp;
std::string_view sv(s);  // read-only slice, no copy

// State complexity — nói ra
// Time: O(n log n), Space: O(n)

// Edge cases: empty, n=1, duplicate, overflow
```

- Nói **approach trước**, complexity, rồi code.  
- Dùng `const&` cho input lớn; tránh copy string không cần.  
- Senior: đề xuất follow-up (optimize space, thread-safe, production API).

---

## Ghi chú cá nhân

| Concept | Đã làm | Cần ôn lại |
|---------|--------|------------|
| | | |
| | | |

*Cập nhật khi làm xong từng concept.*
