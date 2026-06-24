# Distributed Systems

## 1. What Makes Distributed Systems Hard

Distributed systems are hard because independent machines communicate over unreliable networks.

Failures to assume:

- Node crashes.
- Network timeout.
- Packet loss.
- Message duplication.
- Message reordering.
- Clock skew.
- Partial failure.

Interview answer:

> A distributed system is not just multiple servers. It is a system where failure can happen independently in each node, link, clock, and dependency.

---

## 2. CAP Theorem

CAP says that during a network partition, a distributed system must choose between:

- Consistency: every read sees the latest valid write.
- Availability: every request receives a non-error response.
- Partition tolerance: system continues despite network split.

Since partitions can happen, real systems choose behavior under partition.

Examples:

- CP: prefer correctness, may reject requests.
- AP: prefer availability, may serve stale/conflicting data.

---

## 3. Replication

Replication stores copies of data across nodes.

Common models:

- Leader-follower.
- Multi-leader.
- Leaderless quorum.

Trade-offs:

- Leader-follower is simpler but leader is a bottleneck/failover point.
- Multi-leader improves locality but creates conflict resolution problems.
- Quorum systems balance read/write availability and consistency.

---

## 4. Consensus

Consensus lets nodes agree on a value despite failures.

Used for:

- Leader election.
- Metadata coordination.
- Distributed locks.
- Replicated logs.

Examples:

- Raft.
- Paxos.
- ZooKeeper / etcd style systems.

Important note:

> Consensus is expensive compared with local memory or single-node operations. Use it for critical coordination, not every request path.

---

## 5. Timeouts and Retries

Timeouts prevent waiting forever.

Retries handle transient failures.

Risks:

- Retry storm.
- Duplicate side effects.
- Increased load during outage.
- Longer tail latency.

Safer retry design:

- Set deadlines.
- Use exponential backoff.
- Add jitter.
- Make operations idempotent.
- Use circuit breakers for persistent failure.

---

## 6. Idempotency

An operation is idempotent if repeating it has the same effect as doing it once.

Examples:

- `PUT /user/123/status=active` can be idempotent.
- `POST /charge-card` is not safe unless protected by an idempotency key.

Production rule:

> If clients may retry, the server must handle duplicates safely.

---

## 7. Consistency Models

Common models:

- Strong consistency: read sees latest write.
- Eventual consistency: replicas converge eventually.
- Read-your-writes: user sees their own update.
- Monotonic reads: user does not go backward in observed state.

Interview angle:

> Strong consistency simplifies reasoning but can hurt latency and availability. Eventual consistency scales better but needs conflict handling and user experience design.

---

## Interview Questions

- What is partial failure?
- Explain CAP theorem with an example.
- Why are retries dangerous?
- What is idempotency?
- What is consensus used for?
- How can replication cause stale reads?
