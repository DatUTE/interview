# Database Management — SQL, NoSQL & C++ Core Systems

Khi nào dùng **SQL** vs **NoSQL**, cách **hệ C++ core** (gateway, real-time service) kết nối DB mà **không phá latency**, và thư viện phổ biến.

**Liên quan:** [system_design.md](system_design.md) (CAP, replication, sharding, cache) · [low_latency.md](low_latency.md) (hot path) · [networking.md](networking.md)

---

## 1. SQL là gì?

**SQL (Structured Query Language)** là ngôn ngữ truy vấn cho **relational database (RDBMS)** — dữ liệu trong **bảng** (rows/columns), quan hệ qua **foreign key**, ràng buộc **schema**.


| Khái niệm       | Ý nghĩa                                    |
| --------------- | ------------------------------------------ |
| **Table**       | Entity: `users`, `orders`                  |
| **Row**         | Một bản ghi                                |
| **Column**      | Thuộc tính có kiểu cố định                 |
| **Primary key** | Định danh duy nhất                         |
| **Foreign key** | Liên kết bảng (`order.user_id → users.id`) |
| **Index**       | B-tree / hash — tăng tốc read, cost write  |
| **Transaction** | Nhóm thao tác **ACID**                     |


**Engine phổ biến:** PostgreSQL, MySQL/MariaDB, SQLite, SQL Server, Oracle.

```sql
-- Ví dụ: đơn hàng + user — JOIN tự nhiên
SELECT o.id, u.email, o.total
FROM orders o
JOIN users u ON u.id = o.user_id
WHERE o.created_at > '2026-01-01'
  AND o.status = 'paid';
```

---

## 2. NoSQL là gì?

**NoSQL** = “Not Only SQL” — họ **không** gom về một mô hình bảng quan hệ duy nhất. Gồm nhiều **loại store** khác nhau:


| Loại            | Mô hình               | Ví dụ                 | Điển hình                         |
| --------------- | --------------------- | --------------------- | --------------------------------- |
| **Document**    | JSON/BSON document    | MongoDB, CouchDB      | Schema linh hoạt, nested          |
| **Key-value**   | `key → value`         | Redis, DynamoDB, etcd | Cực nhanh, lookup đơn giản        |
| **Wide-column** | Row + column families | Cassandra, HBase      | Write scale, time-series friendly |
| **Graph**       | Nodes + edges         | Neo4j                 | Quan hệ sâu, recommendation       |
| **Search**      | Inverted index        | Elasticsearch         | Full-text, log analytics          |
| **Time-series** | Timestamp + metrics   | InfluxDB, TimescaleDB | Monitoring, IoT                   |


**Lưu ý:** Redis vừa là cache vừa là DB in-memory; nhiều hệ dùng **SQL + Redis** cùng lúc — không phải “chỉ chọn một”.

---

## 3. SQL vs NoSQL — so sánh nhanh


| Tiêu chí         | SQL (RDBMS)                             | NoSQL (tùy loại)                           |
| ---------------- | --------------------------------------- | ------------------------------------------ |
| **Schema**       | Cố định, migration                      | Linh hoạt (document) hoặc schema-less (KV) |
| **Query**        | JOIN, aggregation phức tạp              | Thường theo key / pattern định sẵn         |
| **Consistency**  | ACID mạnh (một node)                    | Thường eventual (distributed)              |
| **Scale write**  | Vertical + read replicas; shard khó hơn | Horizontal shard sẵn (Cassandra, Dynamo)   |
| **Transactions** | Multi-row, multi-table                  | Hạn chế hoặc per-partition (Cassandra LWT) |
| **Use case**     | Billing, inventory, account             | Session, feed, metrics, cache              |


→ CAP, ACID vs BASE: [system_design.md](system_design.md) § CAP, § ACID vs BASE

---

## 4. Khi nào dùng SQL?

Chọn **PostgreSQL / MySQL** khi:

- Cần **transaction** đa bảng (chuyển tiền, trừ kho + tạo order)
- **Quan hệ** và **JOIN** là core (báo cáo, admin, compliance)
- **Consistency** mạnh: số dư, inventory không được âm
- Query **ad-hoc** (BI, analyst) trên cùng dataset
- Team quen schema migration, constraint, FK

**Ví dụ domain:**


| Domain                | Vì sao SQL                   |
| --------------------- | ---------------------------- |
| User account, billing | ACID, unique email, audit    |
| E-commerce order      | Order + line items + payment |
| Config có version     | Transactional update         |


---

## 5. Khi nào dùng NoSQL (từng loại)?

### 5.1 Key-value — Redis, DynamoDB

- **Session** user → connection mapping  
- **Rate limit**, leaderboard  
- **Cache** hot data (cache-aside)  
- **Pub/Sub** realtime (Redis — không durable như Kafka)  
- **Distributed lock** (Redis Redlock — dùng cẩn thận)

### 5.2 Document — MongoDB

- Catalog sản phẩm thuộc tính động  
- User profile nhiều nested field  
- Prototype nhanh, schema thay đổi thường xuyên

**Tránh:** báo cáo JOIN nặng nếu không denormalize.

### 5.3 Wide-column — Cassandra

- **Write-heavy**, partition key rõ (`user_id`, `day`)  
- Messaging inbox, activity log, IoT events  
- Chấp nhận **eventual consistency** + idempotent writes

### 5.4 Time-series — InfluxDB, TimescaleDB

- Metrics server (CPU, latency histogram)  
- Sensor telemetry

### 5.5 Search — Elasticsearch

- Full-text search, log aggregation (ELK)  
- Không thay thế system of record cho transactional data

### Decision flowchart (phỏng vấn)

```
Cần ACID multi-table transaction?
  YES → SQL (PostgreSQL)
  NO  → Access pattern chủ yếu?
          key lookup / cache / TTL     → Redis
          massive write + partition key → Cassandra
          flexible document            → MongoDB
          full-text search             → Elasticsearch
          time-range metrics           → TimescaleDB / Influx
```

---

## 6. C++ core có dùng DB được không? — Có, nhưng tách “hot path”

**C++ core** (gateway, media, matching engine) thường:

- **Hot path:** network I/O, parse protocol, routing — **không** gọi DB đồng bộ  
- **Warm path:** auth lookup cache hit, config snapshot in-memory  
- **Cold path:** persist message, audit, billing — **async** qua queue hoặc thread pool DB

```
                    ┌─────────────────────────────────┐
  Clients ──TCP/WS──▶│  C++ Core (epoll, low latency)│
                    │  Hot path: NO blocking DB       │
                    └───────────┬─────────────────────┘
                                │
              ┌─────────────────┼─────────────────┐
              ▼                 ▼                 ▼
        In-memory map     Redis (session)    Thread pool /
        (shard local)     cache / pubsub     async writers
              │                 │                 │
              │                 │                 ▼
              │                 │         PostgreSQL /
              │                 │         Cassandra /
              └─────────────────┴──────── Kafka → consumers → DB
```

### Nguyên tắc tích hợp DB trong C++


| Nguyên tắc                 | Lý do                                 |
| -------------------------- | ------------------------------------- |
| **Không block I/O thread** | DB RTT ms–100ms — phá real-time       |
| **Connection pool**        | Tạo connection đắt; reuse             |
| **Prepared statements**    | SQL injection + parse một lần         |
| **Batch writes**           | Giảm round-trip                       |
| **Cache trước DB**         | Redis cho read 80/20                  |
| **Idempotent writes**      | Retry an toàn (at-least-once queue)   |
| **Circuit breaker**        | DB chậm → degrade, không kéo sập core |


### Anti-patterns

- `SELECT` trong vòng lặp xử lý mỗi packet  
- Một global `mutex` bọc mọi query  
- ORM nặng trên thread xử lý 100k connections  
- Transaction dài giữ lock khi chờ network

---

## 7. Thư viện C++ phổ biến

### 7.1 SQL


| Library                   | DB                   | Ghi chú                           |
| ------------------------- | -------------------- | --------------------------------- |
| **libpqxx**               | PostgreSQL           | Native, modern C++, prepared stmt |
| **SOCI**                  | PG, MySQL, SQLite, … | Abstraction, backend plugins      |
| **SQLite** (amalgamation) | Embedded / edge      | Single file, zero server          |
| **ODBC**                  | Enterprise           | Driver chung, ít “modern C++”     |
| **mysql-connector-c++**   | MySQL                | Official                          |


```cpp
// libpqxx — prepared statement (conceptual)
pqxx::connection conn{"postgresql://user:pass@localhost/app"};
pqxx::work txn{conn};
auto r = txn.exec_params(
    "SELECT status FROM sessions WHERE id = $1",
    session_id);
txn.commit();
```

### 7.2 Redis (cache / KV)


| Library                              | Ghi chú             |
| ------------------------------------ | ------------------- |
| **hiredis**                          | C API, sync         |
| **redis-plus-plus**                  | C++ wrapper hiredis |
| **Boost.Redis** (nếu có trong stack) | Async-friendly      |


Thường chạy trên **thread pool riêng**, không trên epoll thread.

### 7.3 MongoDB

- **mongocxx** / legacy driver — document BSON  
- Dùng cho admin/config, ít khi trên per-packet path

### 7.4 Cassandra / Scylla

- **datastax/cpp-driver** — async driver, phù hợp write-heavy pipeline

### 7.5 Async pattern (không block core)

```cpp
// Pattern: post DB work to pool
db_pool.enqueue([session_id, payload] {
    store_message(session_id, payload);  // libpqxx / cassandra driver
});

// Hoặc: produce to Kafka, consumer service ghi DB
kafka_producer.send("messages", key, payload);
```

**C++20:** `std::jthread` + queue; hoặc **Asio** strand cho serial access connection.

---

## 8. Patterns thường gặp trong hệ real-time

### 8.1 Cache-aside (Redis + PostgreSQL)

```
Read:  Redis GET → miss → PG SELECT → SET Redis TTL
Write: PG INSERT/UPDATE → DEL Redis key (hoặc update)
```

C++ core: read session từ **local LRU** → Redis → PG (3 tần, chỉ miss mới đi sâu).

### 8.2 Write-behind / async persist

```
Core nhận message → ACK client → enqueue → worker batch INSERT
```

Trade-off: risk mất vài ms data nếu process crash trước flush — cần **WAL** hoặc replicate queue (Kafka).

### 8.3 CQRS (tách read/write model)

```
Write model: normalized SQL
Read model:  denormalized Redis / Elasticsearch / materialized view
```

Phù hợp feed, search — sync qua event stream.

### 8.4 SQLite trong embedded / edge C++

- Config, offline buffer, local queue trước khi sync cloud  
- Không thay PG cho multi-writer cluster

---

## 9. Schema & indexing (SQL) — senior cần biết

```sql
-- Composite index: (user_id, created_at) phục vụ
--   WHERE user_id = ? ORDER BY created_at DESC
CREATE INDEX idx_orders_user_time ON orders(user_id, created_at DESC);

-- Covering index: INCLUDE columns → index-only scan
CREATE INDEX idx_cover ON orders(user_id) INCLUDE (status, total);
```


| Vấn đề          | Triệu chứng          | Hướng xử lý                        |
| --------------- | -------------------- | ---------------------------------- |
| Missing index   | Seq scan, p99 DB cao | EXPLAIN ANALYZE                    |
| Over-indexing   | Write chậm           | Chỉ index query thật               |
| N+1 queries     | 1000 query/1 request | JOIN hoặc batch IN (...)           |
| Lock contention | UPDATE cùng row      | Shorter tx, queue, optimistic lock |


---

## 10. NoSQL trong C++ service — ví dụ mapping


| Nhu cầu core C++       | Store                    | API style                     |
| ---------------------- | ------------------------ | ----------------------------- |
| Online presence        | Redis HASH               | HSET/HGET                     |
| Undelivered msg buffer | Redis LIST / Stream      | LPUSH / XADD                  |
| Chat history durable   | Cassandra                | partition `(conv_id, bucket)` |
| User billing           | PostgreSQL               | ACID transaction              |
| Search message         | Elasticsearch            | async index từ Kafka          |
| Metrics latency        | TimescaleDB / Prometheus | batch insert                  |


---

## 11. Interview Q&As

**[Mid]** SQL và NoSQL khác nhau cơ bản thế nào?

> SQL dùng mô hình bảng quan hệ, schema cố định, ACID transaction, JOIN mạnh — phù hợp dữ liệu có cấu trúc và consistency nghiệp vụ. NoSQL là họ công nghệ gồm document, key-value, wide-column, graph… tối ưu cho scale ngang, schema linh hoạt hoặc access pattern đơn giản, thường đánh đổi consistency (eventual). Chọn theo access pattern và consistency requirement, không theo hype.

---

**[Mid]** Khi nào dùng Redis vs PostgreSQL?

> PostgreSQL là **system of record** — durable, ACID, query phức tạp. Redis là **in-memory** — latency µs–sub-ms, TTL, cấu trúc dữ liệu đơn giản; dùng cache, session, rate limit, pub/sub. Dữ liệu chỉ trong Redis có thể mất khi restart (trừ persistence RDB/AOF với trade-off). Production: Redis cache + PG source of truth.

---

**[Senior]** C++ gateway 50k WebSocket — bạn có gọi PostgreSQL trên mỗi message không?

> Không trên hot path. Connection state giữ in-memory (sharded map); auth/session load một lần lúc connect (Redis/PG). Message persist: enqueue tới worker pool hoặc Kafka consumer ghi Cassandra/PG batch. Metric queue depth để backpressure. Nếu cần read history: API riêng hoặc cache, không block fan-out realtime.

---

**[Senior]** Thiết kế lưu chat message: SQL hay Cassandra?

> **SQL** nếu volume vừa, cần transaction (edit/delete policy), query linh hoạt, team nhỏ. **Cassandra** nếu write cực cao, partition key rõ (`conversation_id`, time bucket), chấp nhận eventual, fan-out on write. Hybrid phổ biến: Cassandra hot store + Kafka → warehouse (SQL/BigQuery) cho analytics. C++ core chỉ cần driver async và idempotent write.

---

**[Senior]** Làm sao tránh SQL injection từ C++ service?

> Luôn **parameterized queries** (`$1`, `?`), không nối string user input. Dùng prepared statements. Principle of least privilege DB user (chỉ quyền cần). Review ORM/query builder generate SQL đúng.

---

## 12. Ôn tập & cross-references


| Chủ đề                            | File                                                                         |
| --------------------------------- | ---------------------------------------------------------------------------- |
| CAP, replication, sharding, cache | [system_design.md](system_design.md)                                         |
| Không block I/O thread            | [low_latency.md](low_latency.md), [networking.md](networking.md)             |
| IPC / shared state                | [ipc_concepts.md](ipc_concepts.md)                                           |
| VSF checklist                     | [../vsf_senior_cpp_vinsmart_future.md](../vsf_senior_cpp_vinsmart_future.md) |


**Đọc thứ tự:** `database_management.md` (chọn store + C++ integration) → `system_design.md` (scale cluster).