# CS Fundamentals Index

## What This Track Covers

Computer Science fundamentals are the shared language behind algorithms, operating systems, networking, databases, distributed systems, architecture, and security.

For interviews, the goal is not to memorize definitions. The goal is to explain trade-offs:

- Why one data structure fits a workload better than another.
- Why latency spikes even when CPU usage looks normal.
- Why retries can make a distributed outage worse.
- Why an index speeds reads but slows writes.
- Why cache locality matters for C++ performance.

---

## Topic Map

| Area | Core Questions |
|------|----------------|
| Algorithms | What is the complexity, invariant, and failure case? |
| Data structures | What operations are fast, slow, cache-friendly, or memory-heavy? |
| Operating systems | What does the kernel do for CPU, memory, files, and IO? |
| Networking | How does data move from process to process across machines? |
| Databases | How do storage engines provide lookup, durability, and transactions? |
| Distributed systems | What happens when nodes, networks, and clocks fail? |
| Architecture | How do CPU/cache/memory affect real code performance? |
| Security | What can an attacker do, and what trust boundary is crossed? |

---

## Study Roadmap

### Phase 1: Coding Interview Core

Focus:

- Big-O.
- Arrays, strings, hash tables.
- Stacks, queues, heaps.
- Trees and graphs.
- Binary search.
- Recursion and dynamic programming.

Outcome:

> Given a problem, identify the state, invariant, complexity, and edge cases.

### Phase 2: Systems Core

Focus:

- Process vs thread.
- Virtual memory.
- File descriptors.
- Socket API.
- TCP vs UDP.
- CPU cache and memory locality.

Outcome:

> Explain how an application call becomes OS/kernel/network/storage work.

### Phase 3: Production Systems

Focus:

- Database indexes and transactions.
- Replication and consistency.
- Retries, timeouts, idempotency.
- CAP theorem.
- Security basics.

Outcome:

> Design systems that handle failure instead of assuming the happy path.

---

## Interview Answer Template

Use this shape for most CS questions:

1. Define the concept in one sentence.
2. Explain the mechanism.
3. Give the trade-off.
4. Mention a real-world example.
5. Add one failure mode or edge case.

Example:

> A hash table gives average O(1) lookup by mapping keys to buckets. Collisions are handled by chaining or probing. It is fast for exact lookup, but it uses extra memory and can degrade with poor hash distribution. In production, it powers maps, caches, and indexes. A key risk is unbounded growth or adversarial keys causing collision-heavy behavior.
