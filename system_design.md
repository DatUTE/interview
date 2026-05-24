# System Design

## Interview Framework (Use This Every Time)

```
1. CLARIFY requirements (5 min)
   - Functional:  what does the system DO?
   - Non-functional: scale, latency, availability, consistency, durability
   - Out of scope: what are we NOT building?

2. ESTIMATE scale (3 min)
   - DAU / QPS / storage / bandwidth
   - Read-heavy vs write-heavy ratio

3. HIGH-LEVEL design (10 min)
   - Draw boxes: clients → LB → services → storage
   - Identify main APIs

4. DEEP DIVE (20 min)
   - Pick 2–3 hardest components, go deep
   - Data model, algorithms, bottlenecks

5. WRAP UP (5 min)
   - Failure modes, monitoring, future scaling
```

---

## Numbers Every Engineer Should Know

### Latency Reference (approximate)
```
L1 cache reference          0.5 ns
L2 cache reference          7   ns
RAM access                  100 ns
Read 1 MB from RAM          250 μs
SSD random read             100 μs
SSD sequential read 1 MB    1   ms
HDD random read             10  ms
Round trip in same datacenter  500 μs
Round trip: US → Europe     150 ms
```

### Back-of-Envelope Formula
```
QPS (queries/sec) = DAU × avg_requests_per_day / 86,400

Storage per year = QPS × bytes_per_record × 86,400 × 365

Bandwidth = QPS × avg_response_size

Peak QPS ≈ 2–3× average QPS

1 million DAU, 10 req/day → ~116 QPS average → ~350 QPS peak

Twitter-scale: 300M DAU, 100 req/day → ~350K QPS
```

### Quick Size Reference
```
1 ASCII char = 1 byte
UUID         = 36 bytes (string) / 16 bytes (binary)
Timestamp    = 8 bytes
URL          = ~100 bytes
Tweet        = ~300 bytes
Image (web)  = ~300 KB
Video 1 min  = ~10 MB (compressed)

1 KB  = 10³ bytes
1 MB  = 10⁶ bytes
1 GB  = 10⁹ bytes
1 TB  = 10¹² bytes
1 PB  = 10¹⁵ bytes
```

---

## Core Concepts

### 1. Scalability

```
Vertical scaling (scale up):
  + Simple, no distributed complexity
  - Hard limit (biggest machine), single point of failure
  Use: databases (early stage), GPU workloads

Horizontal scaling (scale out):
  + Theoretically unlimited, fault tolerant
  - Distributed complexity (coordination, consistency)
  Use: stateless services, caches, message queues
```

### 2. CAP Theorem

A distributed system can guarantee at most **2 of 3**:

```
        Consistency
        /         \
       /           \
      /   Pick 2    \
     /               \
Availability ─────── Partition Tolerance
```

- **CP** (Consistency + Partition Tolerance): Strong consistency, may be unavailable during partition. Examples: HBase, Zookeeper, etcd
- **AP** (Availability + Partition Tolerance): Always available, may return stale data. Examples: Cassandra, DynamoDB (default), CouchDB
- **CA** (Consistency + Availability): Only works without partitions — impossible in distributed systems. Examples: single-node PostgreSQL

**Real world**: partitions always happen → choose between C and A.

### 3. ACID vs BASE

| Property | ACID (SQL) | BASE (NoSQL) |
|---|---|---|
| Atomicity | All-or-nothing transactions | Best-effort |
| Consistency | DB invariants always valid | Eventually consistent |
| Isolation | Concurrent txns don't interfere | Weaker isolation |
| Durability | Committed = persisted | May buffer writes |
| Use case | Financial, inventory, orders | Analytics, social feeds, logs |

### 4. Consistency Models (Weakest → Strongest)

```
Eventual consistency
    Data converges eventually (seconds to minutes)
    OK: user profiles, social feeds, DNS

Monotonic read consistency
    A user never reads older data after newer

Read-your-writes (read-your-own-writes)
    After write, you always read your own write
    Important: profile update, settings change

Session consistency
    Read-your-writes within a session

Causal consistency
    Causally related ops seen in order

Strong consistency (linearizability)
    All reads see the most recent write
    Required: bank balance, inventory count, distributed locks
```

### 5. Availability Nines

```
99%     = 3.65 days downtime/year    (2 nines)
99.9%   = 8.76 hours/year            (3 nines) — typical web app
99.99%  = 52.56 minutes/year         (4 nines) — financial systems
99.999% = 5.26 minutes/year          (5 nines) — carrier-grade telecom
```

**Serial availability**: A → B → C: `0.99 × 0.99 × 0.99 = 0.97` (97%)  
**Parallel availability**: A ∥ B: `1 - (0.01 × 0.01) = 99.99%`  
→ Redundancy dramatically improves availability.

---

## Building Blocks

### Load Balancer

```
Clients → [Load Balancer] → [Server 1]
                          → [Server 2]
                          → [Server 3]

Algorithms:
  Round Robin       — equal distribution, stateless servers
  Least Connections — variable request duration
  IP Hash           — sticky sessions (session affinity)
  Weighted          — heterogeneous server capacity
  Health Check      — remove unhealthy servers automatically

L4 LB: TCP/UDP level (IP + port) — fast, low overhead
L7 LB: HTTP level (URL, headers, body) — intelligent routing, SSL termination

Tools: nginx, HAProxy, AWS ALB/NLB, Envoy
```

### Cache

```
Cache-aside (Lazy loading — most common):
  read:  cache hit → return
         cache miss → read DB → write cache → return
  write: write DB only (cache invalidated or TTL expires)
  Pro: only cache what's needed, resilient to cache failure
  Con: cache miss → 3 trips, potential stale data

Write-through:
  write: write cache + DB synchronously
  Pro: no stale data
  Con: write latency, cache fills with unread data

Write-behind (Write-back):
  write: write cache, async flush to DB
  Pro: low write latency
  Con: data loss if cache dies before flush

Read-through:
  Like cache-aside but cache itself loads from DB
  Used by: ORM-level caches

Eviction Policies: LRU, LFU, FIFO, Random
Cache thundering herd: many miss simultaneously → mutex/semaphore or probabilistic early expiry
```

**Cache layer examples:**
- **Client-side**: browser cache, HTTP Cache-Control headers
- **CDN**: edge caches (Cloudflare, CloudFront) — static assets, geo distribution
- **Application**: in-process (Guava cache, Caffeine)
- **Distributed**: Redis, Memcached — shared across app servers

### Database Selection Guide

```
Relational (PostgreSQL, MySQL):
  ✓ Complex queries, JOINs, transactions
  ✓ Strong consistency, ACID
  ✗ Vertical scaling limit, schema changes painful
  Use: user accounts, orders, financial transactions

Document (MongoDB, CouchDB):
  ✓ Flexible schema, nested objects
  ✓ Horizontal scaling (sharding built-in)
  ✗ No joins, eventual consistency trade-offs
  Use: product catalog, CMS, user profiles

Wide-column (Cassandra, HBase):
  ✓ Massive write throughput, linear horizontal scaling
  ✓ Tunable consistency
  ✗ Limited query patterns, no ad-hoc queries
  Use: IoT time series, activity logs, messaging

Key-value (Redis, DynamoDB):
  ✓ O(1) reads/writes, extremely fast
  ✗ Simple queries only
  Use: sessions, leaderboards, rate limiting, distributed locks

Graph (Neo4j, Amazon Neptune):
  ✓ Relationship-heavy queries
  Use: social graph, recommendation engine, fraud detection

Search (Elasticsearch, Solr):
  ✓ Full-text search, faceting, ranking
  Use: search box, log analysis (ELK stack)

Time-series (InfluxDB, TimescaleDB):
  ✓ Optimized for append + time-range queries
  Use: metrics, monitoring, financial ticks
```

### Message Queue

```
Async decoupling:

Producer → [Queue] → Consumer
             │
             ├── Buffer spikes (producer faster than consumer)
             ├── Retry on consumer failure
             ├── Fan-out (multiple consumers)
             └── Decouple services (no direct RPC)

Kafka:
  - Distributed log, append-only, persistent
  - Consumer groups (parallel processing per partition)
  - Replay from offset (reprocess history)
  - Very high throughput (millions/sec)
  Use: event streaming, audit log, CDC (Change Data Capture)

RabbitMQ:
  - Message broker with routing (exchanges, queues, bindings)
  - Supports AMQP, task queues, pub/sub, RPC
  - Messages deleted after ACK (not a log)
  Use: task queues, job dispatch, RPC

SQS (AWS):
  - Fully managed, at-least-once delivery
  - FIFO queues available (exactly-once, ordered)
  Use: serverless workloads, AWS ecosystem

Redis Pub/Sub:
  - In-memory, no persistence, fire-and-forget
  Use: real-time notifications, live scores (not durable)
```

### Content Delivery Network (CDN)

```
Origin Server (datacenter) ──── CDN Edge (100+ PoPs worldwide)
                                       │
                              Cache static assets close to users:
                              images, CSS, JS, videos, fonts

Pull CDN: origin fetches on first miss, cached at edge (TTL)
Push CDN: pre-upload assets to all edge nodes (large, infrequently changing)

Cache-Control: max-age=31536000, immutable  ← forever cached (versioned files)
Cache-Control: no-cache                     ← must revalidate with origin
```

---

## Scaling Techniques

### Database Scaling

#### Replication
```
Primary-Replica (Master-Slave):
  Writes → Primary → replicated async to Replicas
  Reads  → Replicas (horizontal read scaling)
  Failover: replica promoted to primary (with possible data loss on async)

Multi-Primary (Multi-Master):
  All nodes accept writes → conflict resolution needed
  Use: geo-distributed writes

Sync replication: strong consistency, higher write latency
Async replication: lower write latency, possible data loss on primary failure
Semi-sync: at least 1 replica ACKs before primary ACKs to client
```

#### Sharding (Horizontal Partitioning)
```
Split data across multiple DB instances:

By range:    users 1–1M → shard1, 1M–2M → shard2
  + Simple range queries
  - Hot spots (new users always go to last shard)

By hash:     hash(user_id) % N → shard
  + Even distribution
  - Range queries hit all shards, resharding is painful

By directory: lookup table maps key → shard
  + Flexible
  - Lookup table is a bottleneck / SPOF

Consistent Hashing:
  Ring of virtual nodes → add/remove server = minimal remapping
  Used by: Cassandra, DynamoDB, Redis Cluster, Memcached (twemproxy)
```

#### Consistent Hashing Deep Dive
```
Virtual ring (0 to 2^32-1):
  ┌─────────────────────────────────────────┐
  │   ·  S1  ·  ·  S2  ·  S1  ·  S3  ·     │ ← servers placed at multiple
  │            ↑ key K lands here → S2      │   points (virtual nodes)
  └─────────────────────────────────────────┘

Adding a server: only its clockwise-neighbor's keys move → ~K/N remapping
Removing a server: its keys move to clockwise neighbor only

Virtual nodes (vnodes):
  Each physical server → 100-200 positions on ring
  Ensures even distribution even with heterogeneous servers
```

#### Denormalization & Indexes
```
Denormalization: store redundant data to avoid expensive JOINs
  Trade-off: write overhead + consistency complexity for read speed

Indexes: B-tree (default), Hash, GIN (full-text), BRIN (time series)
  Covering index: include all query columns → index-only scan
  Composite index: (a, b) satisfies queries on (a) or (a, b) but not (b) alone
```

---

## Consistency Patterns

### Two-Phase Commit (2PC)
```
Coordinator        Participant 1    Participant 2
    │                   │                │
    │──── PREPARE ──────▶│               │
    │──── PREPARE ───────┼───────────────▶│
    │                   │                │
    │◀─── READY ─────────│               │
    │◀─── READY ─────────┼───────────────│
    │                   │                │
    │──── COMMIT ───────▶│               │
    │──── COMMIT ────────┼───────────────▶│

Problem: coordinator crash between PREPARE and COMMIT → participants blocked
Used in: distributed SQL (Spanner, CockroachDB), XA transactions
```

### Saga Pattern (Distributed Transactions without 2PC)
```
Choreography: each service publishes event → next service reacts
Orchestration: central orchestrator tells each service what to do

Order saga:
  1. Create order (PENDING)
  2. Reserve inventory    → fail → compensate: cancel order
  3. Charge payment       → fail → compensate: release inventory
  4. Ship order
  5. Confirm order (COMPLETE)

Each step has a compensating transaction (not a rollback — forward recovery)
```

---

## Classic System Designs

### URL Shortener (bit.ly)

**Requirements**: shorten URL, redirect, ~100M URLs/day write, 10B redirects/day read

**Estimation**:
```
Write: 100M / 86400 ≈ 1200 QPS
Read:  10B  / 86400 ≈ 115K QPS  (read-heavy: 100:1)
Storage: 100M × 500 bytes ≈ 50 GB/day → 18 TB/year
```

**Key design decisions**:
```
Short code generation:
  Option 1: hash(long_url) → take first 7 chars (collision risk)
  Option 2: base62(auto-increment ID) → no collision, predictable
  Option 3: base62(UUID) → no central counter needed

Redirect type:
  301 Permanent: browser caches → fewer server hits, but no analytics
  302 Temporary: always hits server → accurate click tracking

Storage: (id, long_url, created_at, user_id, expiry)
  Primary key: short_code
  Read path: Redis cache → DB on miss (99% cache hit ratio)

Scale:
  DB: single table, read replicas for 115K RPS reads
  Cache: Redis for hot URLs (80/20 rule: 20% URLs = 80% traffic)
  API servers: stateless, horizontal scale behind LB
```

---

### Rate Limiter

**Algorithms**:

```
Token Bucket:
  Bucket holds max N tokens. Refill R tokens/sec.
  Request consumes 1 token. If empty → 429.
  Allows bursts up to bucket size.

Leaky Bucket:
  Requests enter a FIFO queue, drained at fixed rate.
  Smooths traffic. Bursty requests delayed, not dropped.

Fixed Window Counter:
  Count requests per minute. Reset at minute boundary.
  Flaw: burst at window edge: 100 at :59 + 100 at :00 = 200 in 2 seconds.

Sliding Window Log:
  Store timestamp of each request. Remove old timestamps.
  Count in last N seconds. Accurate but memory heavy.

Sliding Window Counter (hybrid):
  current_count = prev_window_count × overlap_ratio + curr_window_count
  Memory efficient + accurate.
```

**Distributed rate limiter (Redis)**:
```
-- Lua script (atomic in Redis)
local key = KEYS[1]
local limit = tonumber(ARGV[1])
local window = tonumber(ARGV[2])
local now = tonumber(ARGV[3])

redis.call('ZREMRANGEBYSCORE', key, 0, now - window)
local count = redis.call('ZCARD', key)
if count < limit then
    redis.call('ZADD', key, now, now)
    redis.call('EXPIRE', key, window)
    return 1  -- allowed
end
return 0  -- rate limited
```

**Placement**:
- API Gateway level (before routing)
- Middleware in service
- Client-side (reduce unnecessary requests)

---

### Chat System (WhatsApp / Slack)

**Requirements**: 1-1 messaging, group chat, online presence, message history

**Key choices**:
```
Protocol: WebSocket (full-duplex, persistent connection)
  vs HTTP long-polling: higher latency, server push awkward
  vs SSE: server→client only

Message delivery model:
  Push model: server pushes to recipient's WebSocket connection
  Fan-out on write: write to all recipients' inboxes
  Fan-out on read: store once, each user queries their messages

Small groups (≤ 100): fan-out on write (pre-computed inboxes)
Large groups (1000+): fan-out on read (single message copy + membership list)
```

**Architecture**:
```
Client ──WebSocket──▶ Chat Server (stateful: maps user → connection)
                           │
                    Message Queue (Kafka)
                           │
              ┌────────────┴────────────┐
         Message DB                Presence Service
       (Cassandra:                  (Redis: user → {online, last_seen})
        user_id, msg_id,
        timestamp, content)

Message ID: snowflake ID (timestamp + server_id + sequence)
           → unique, roughly time-ordered, no central coordinator

Delivery:
  1. Sender → Chat Server → Kafka topic
  2. Consumer writes to Cassandra
  3. If recipient online → push via WebSocket connection
  4. If offline → store in inbox → push on reconnect

Read receipts: ACK message type on same WebSocket
```

---

### News Feed (Twitter / Facebook Timeline)

**Requirements**: post tweets, follow users, see feed (last 500 tweets from followees)

**Fan-out strategies**:

```
Fan-out on write (push model):
  When user posts → write to all followers' feed caches immediately
  Pro: fast read (O(1) read from pre-built feed)
  Con: write amplification (celebrity with 100M followers = 100M writes)

Fan-out on read (pull model):
  When user requests feed → query all followees' tweets, merge, sort
  Pro: no write amplification
  Con: slow read, complex merge

Hybrid (Twitter approach):
  Normal users   → fan-out on write (pre-built feed in Redis)
  Celebrities    → fan-out on read  (queried at read time)
  Feed = Redis fan-out feed + celebrity tweets merged at read time
```

**Feed storage**:
```
Redis sorted set:  ZADD user:{id}:feed <timestamp> <tweet_id>
                   ZREVRANGE user:{id}:feed 0 499  ← latest 500
                   Trim to 500 entries on each write

Tweet data: stored separately in tweet DB (MySQL/Cassandra)
            feed only stores tweet_ids (saves memory)
```

---

### Key-Value Store (Redis / DynamoDB-like)

**Data structures**: hash map (in-memory) + WAL (Write-Ahead Log) + snapshots

```
Operations: get(key), put(key, val, ttl?), delete(key)

Single node:
  Hash table in memory → O(1) read/write
  Persistence: RDB snapshot (periodic) + AOF (append-only log)
  Expiry: lazy (on access) + active (periodic scan)

Distributed:
  Data partitioning: consistent hashing → N nodes
  Replication: each key stored on R nodes (coordinator + replicas)
  Consistency: quorum reads/writes
    W (write quorum) + R (read quorum) > N → strong consistency
    W=1, R=1, N=3 → available, eventually consistent
    W=3, R=1, N=3 → slow writes, fast reads

Conflict resolution: vector clocks or last-write-wins (timestamp)
Gossip protocol: nodes exchange state to detect failures
```

---

### Web Crawler

**Requirements**: crawl 1 billion pages, refresh every month, politeness

```
Architecture:
  Seed URLs → URL Frontier (priority queue) → Fetcher → Parser
                    ↑                                      │
                    └──────── Extracted URLs ──────────────┘
                                    │
                             Visited (BloomFilter)
                             Content (DB)

Components:
  URL Frontier: prioritized queue (BFS for breadth, PageRank for quality)
  DNS Resolver: cache DNS (TTL respected) to reduce lookups
  Fetcher: respect robots.txt, rate limit per domain, politeness delay
  Parser: extract links, content fingerprint (SimHash for near-duplicate)
  Deduplication: BloomFilter for URL seen check (false positive acceptable)
                 SimHash for content near-duplicate detection

Scale:
  1B pages × 500KB avg = 500 TB storage
  1B / (30×86400) ≈ 400 pages/sec → 400 parallel fetchers
  Distributed: consistent hash on domain → same crawler handles domain (politeness)
```

---

### Notification System

**Push channels**: APNs (iOS), FCM (Android), SMS (Twilio), Email (SendGrid)

```
Event sources → Notification Service → Channel Workers → 3rd party APIs
                        │
                 Notification DB (store + status tracking)
                 Retry Queue (failed sends → retry with backoff)

Rate limiting: don't send > N notifications/user/day (prevent spam)
User preferences: opt-out per channel, quiet hours
Deduplication: idempotency key per notification (avoid duplicate delivery)

Flow:
  1. Event triggers notification (order shipped, mention, etc.)
  2. Notification service fetches user device tokens, preferences
  3. Route to appropriate channel worker (push/email/SMS)
  4. Worker calls 3rd party API with retry
  5. Update delivery status (sent/failed/read)

Analytics: open rate, delivery rate, click-through
```

---

### Distributed ID Generator (Snowflake)

```
64-bit ID layout:
┌──────────────────────────────┬──────────────┬─────────────────┐
│   41 bits timestamp (ms)     │  10 bits     │  12 bits seq    │
│   since custom epoch         │  machine ID  │  per machine    │
└──────────────────────────────┴──────────────┴─────────────────┘

41 bits ms: ~69 years from epoch
10 bits machine: 1024 machines
12 bits seq: 4096 IDs per ms per machine
= 4096 × 1024 = ~4M IDs/ms = 4B IDs/sec globally

Properties: roughly time-sortable, no central coordinator, unique

Alternatives:
  UUID v4: 128-bit random, no coordination, not sortable, 36-char string
  UUID v7: 128-bit time-ordered, better for DB indexes
  ULID: 128-bit, base32, sortable, URL-safe
  DB auto-increment: simple, central bottleneck, not distributed
```

---

## Reliability Patterns

### Circuit Breaker
```
States: CLOSED → OPEN → HALF-OPEN → CLOSED
                   ↑
           failure threshold
           exceeded

CLOSED:     requests pass through, count failures
OPEN:       all requests fail-fast (no calls to downstream)
HALF-OPEN:  allow 1 probe request
            success → CLOSED, failure → OPEN

Protects: cascading failures when dependency is down
Libraries: Hystrix (Java), resilience4j, Polly (.NET)
```

### Retry with Exponential Backoff + Jitter
```cpp
int delay_ms = base_delay * (2 ^ attempt) + rand(0, base_delay);
// Jitter: prevents thundering herd on simultaneous retries
// Exponential: 100ms, 200ms, 400ms, 800ms, cap at 30s
// Max retries: 3–5 for user-facing, more for background jobs
// Only retry idempotent operations!
```

### Bulkhead Pattern
```
Isolate resources per client/service:
  Thread pool A → Service A (max 20 threads)
  Thread pool B → Service B (max 20 threads)
If Service A is slow and exhausts its pool, Service B is unaffected.
Named after ship bulkheads that prevent flooding entire vessel.
```

---

## Data Consistency Techniques

### Event Sourcing
```
Store sequence of events (facts) rather than current state:
  OrderCreated{...}
  ItemAdded{item_id, qty}
  PaymentProcessed{amount}
  OrderShipped{tracking}

Current state = replay of all events
Audit log for free
Time travel: reconstruct state at any point
Event stream → projections/read models (CQRS)
```

### CQRS (Command Query Responsibility Segregation)
```
Write side: Command → validates → updates write DB → emits event
Read side:  Event  → updates read model (denormalized, optimized for query)

Write DB: normalized, consistent (PostgreSQL)
Read DB:  denormalized, optimized per query (Redis, Elasticsearch, MongoDB)

Pro: independent scaling of reads and writes
Con: eventual consistency between write and read models
Use: high read/write ratio, complex queries, event sourcing
```

---

## Interview Q&As

**Q [Mid]: How would you scale a database that's becoming a read bottleneck?**

First, add a **read replica** (primary-replica replication) — route all reads to replicas while writes go to primary. This handles most read scaling. Next, add a **cache layer** (Redis/Memcached) in front of the DB using cache-aside pattern — cache hot data with appropriate TTL. For persistent read scaling, consider **database sharding** (horizontal partitioning) using range or hash sharding. Finally, evaluate if a **read-optimized store** (Elasticsearch for search, Redis for leaderboards) can serve specific query patterns more efficiently. Each step adds complexity — apply them in order based on actual bottleneck.

---

**Q [Mid]: What is the difference between SQL and NoSQL databases? When would you choose each?**

SQL databases (PostgreSQL, MySQL) provide ACID transactions, enforce schemas, support complex JOINs, and scale vertically. Choose SQL when data has strong relational structure, consistency is critical (financial, inventory), or query patterns are complex and unpredictable. NoSQL databases trade ACID for horizontal scalability and flexible schemas. Choose NoSQL when: the data model maps naturally (documents → MongoDB, time-series → InfluxDB, wide-column append → Cassandra), write throughput exceeds what SQL can handle vertically, or you need to shard across hundreds of nodes. In practice, most systems use both — SQL for transactional core, NoSQL/cache for high-volume auxiliary data.

---

**Q [Senior]: Design Twitter's timeline — how do you handle celebrity accounts with millions of followers?**

Use a **hybrid fan-out** approach. For regular users (< 10K followers), pre-compute timelines using fan-out on write: when they post, push their tweet ID to each follower's feed cache (Redis sorted set keyed by timestamp). This makes reads O(1). For celebrities (> threshold), skip fan-out — their tweets stay only in their own tweet store. At read time, a user's timeline = their pre-computed Redis feed + on-the-fly merge of all celebrity timelines they follow (typically few, so manageable). The merge is done in the application layer and cached per request. This avoids the 100M-write problem of fan-out for a single Katy Perry tweet while keeping most users on fast pre-computed feeds.

---

**Q [Senior]: How does consistent hashing help with horizontal scaling of a cache?**

With naive modulo hashing (`hash(key) % N`), adding/removing one server causes `~N/(N+1)` keys to remap — a cache wipe that floods the database. Consistent hashing maps both servers and keys onto a virtual ring. Each server occupies positions on the ring; a key maps to the nearest clockwise server. When a server is added or removed, only its immediate neighbors' keys remap — roughly `K/N` total remapping where K=keys and N=servers. Adding virtual nodes per physical server (100–200) ensures even key distribution even when server capacities differ. Redis Cluster and Cassandra both use variants of this approach.

---

**Q [Senior]: You have a write-heavy system logging 1 million events/second. Design the storage pipeline.**

Use a **write-optimized pipeline**: (1) **Kafka** as the write buffer — producers write to Kafka topics with async batching, zero-copy, and sequential disk writes. Kafka handles millions of writes/sec with durability. (2) **Stream consumers** (Kafka Streams / Flink) aggregate, transform, and route events — batch inserts into storage every 1–5 seconds. (3) **Storage layer**: for raw logs use **Cassandra** (append-only, wide-column, LSM-tree compaction handles high write volume); for analytics use **ClickHouse** or **Redshift** (columnar, high-throughput batch inserts). (4) **Tiered storage**: recent hot data in SSDs, older data in cheaper S3-backed object storage with lifecycle policies. Key insight: never write 1M events/sec directly to a relational DB — the transaction overhead and B-tree update cost won't survive. Buffer writes, batch, and use append-optimized stores.
