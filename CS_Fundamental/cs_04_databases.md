# Databases

## 1. What a Database Provides

A database stores data and provides ways to query, update, persist, and recover it.

Core concerns:

- Data model.
- Query performance.
- Durability.
- Concurrency.
- Consistency.
- Replication and backup.

---

## 2. Indexes

An index is an auxiliary data structure that speeds lookup.

Common index structures:

- B-tree / B+ tree.
- Hash index.
- LSM-tree based indexes.

Trade-off:

- Faster reads.
- Slower writes.
- More storage.
- Extra maintenance during update/delete.

Interview answer:

> An index is like a sorted lookup structure beside the table. It avoids full scans, but every write must also update the index.

---

## 3. Transactions

A transaction groups operations into one logical unit.

ACID:

- Atomicity: all or nothing.
- Consistency: constraints preserved.
- Isolation: concurrent transactions do not corrupt each other.
- Durability: committed data survives crash.

Example:

```text
transfer $100 from A to B
1. debit A
2. credit B
3. commit both together
```

---

## 4. Isolation Levels

Isolation controls what one transaction can see from another.

Common anomalies:

- Dirty read: read uncommitted data.
- Non-repeatable read: same row changes between reads.
- Phantom read: matching row set changes between reads.

Common levels:

- Read committed.
- Repeatable read.
- Serializable.

Trade-off:

> Stronger isolation gives simpler correctness but can reduce concurrency.

---

## 5. Storage Engines

B-tree style:

- Good for range queries.
- Common in relational databases.
- Updates are in-place or page-oriented.

LSM-tree style:

- Write-optimized.
- Writes go to memory/table log first.
- Background compaction merges sorted files.

Use LSM when write throughput matters, but watch compaction and read amplification.

---

## 6. Replication

Replication copies data to multiple nodes.

Benefits:

- Availability.
- Read scaling.
- Disaster recovery.

Risks:

- Replication lag.
- Split brain.
- Conflict resolution.
- Stale reads.

---

## 7. SQL vs NoSQL

SQL:

- Relational model.
- Strong query language.
- Transactions and constraints.
- Good for structured data.

NoSQL:

- Key-value, document, column, graph.
- Often optimized for scale, flexibility, or specific access patterns.
- Consistency model varies.

Rule:

> Pick the database based on access patterns, consistency needs, operational maturity, and failure modes, not just popularity.

---

## Interview Questions

- What is an index and why can it slow writes?
- Explain ACID.
- What is the difference between read committed and serializable?
- Why does replication lag matter?
- When would you choose SQL over NoSQL?
- What is the difference between B-tree and LSM-tree storage?
