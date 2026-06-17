# Low-Latency System Design & Initialization

> **Related:** [realtime_media_voip_webrtc.md](realtime_media_voip_webrtc.md) (jitter buffer, RTP) · [networking.md](networking.md) (WebSocket, TCP tuning)

## What Is Low Latency?

```
Latency        = time from stimulus to response
Throughput     = operations per second
Jitter         = variance in latency (worst enemy in real-time systems)

Targets by domain:
  HFT (High-Frequency Trading)  < 1 µs  (nanosecond matters)
  Market data / order routing   1–10 µs
  Online gaming (server tick)   < 1 ms
  Real-time audio/video         < 10 ms
  Interactive web               < 100 ms

Percentiles matter more than average:
  p50  = median latency
  p99  = 99th percentile (1 in 100 requests)
  p999 = 99.9th percentile — "tail latency"
  p9999= 99.99th — "worst case" most SLAs care about
```

---

## The Low-Latency Stack

```
Application Code
    │  ← lock-free, no alloc, no syscall on hot path
    ▼
C++ Runtime
    │  ← pre-allocate, disable exceptions on hot path
    ▼
OS / Kernel
    │  ← CPU pinning, real-time scheduling, IRQ isolation
    ▼
Hardware / BIOS
    │  ← disable C-states, P-states, turbo, hyperthreading
    ▼
Network
       ← kernel bypass (DPDK), busy poll, UDP multicast
```

Every layer adds latency. Low-latency engineering = **eliminating overhead at every layer**.

---

## 1. Hardware & BIOS Configuration

### CPU Power Management — the #1 source of jitter

```bash
# C-states: CPU sleep states — deeper = more latency on wake-up
# C0 = running, C1 = halt, C2 = stop-clock, C6 = power-gate (deep sleep)
# Disable deep C-states (force C0 or C1 only)

# Method 1: BIOS — set "CPU C-States" to Disabled or C1 only
# Method 2: kernel parameter
GRUB_CMDLINE_LINUX="... intel_idle.max_cstate=1 processor.max_cstate=1"

# Verify current C-states
turbostat --quiet --show Busy,Avg_MHz,C1,C2,C3,C6

# P-states: CPU frequency scaling
# Disable frequency scaling — fix CPU at max frequency
echo performance > /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor
# or
cpupower frequency-set -g performance

# Turbo Boost: can cause momentary stall when boosting
# For consistent latency: disable turbo
echo 1 > /sys/devices/system/cpu/intel_pstate/no_turbo

# Hyperthreading: two logical cores share one physical core
# Sibling shares L1/L2 cache → cache eviction, pipeline pressure
# For latency: disable HT in BIOS (keeps only physical cores)
# Check HT siblings: cat /sys/devices/system/cpu/cpu0/topology/thread_siblings_list
```

### NUMA (Non-Uniform Memory Access)

```
[Socket 0]  ←── fast ───  [DRAM Bank 0]
[Socket 1]  ←── slow ───  [DRAM Bank 0]  (cross-NUMA = +100ns latency)

Rule: keep your process on the same NUMA node as its memory and NIC
```

---

## 2. OS & Kernel Configuration

### CPU Isolation

```bash
# Reserve CPUs exclusively for your process (no OS tasks)
# GRUB kernel parameter:
GRUB_CMDLINE_LINUX="isolcpus=2,3,4,5 nohz_full=2,3,4,5 rcu_nocbs=2,3,4,5"

# isolcpus    → kernel scheduler won't place tasks on these CPUs
# nohz_full   → disable scheduler timer tick on these CPUs (reduces jitter ~1µs)
# rcu_nocbs   → move RCU callbacks off these CPUs

# After boot, pin your process:
taskset -c 2,3 ./my_app
# Or at runtime:
sched_setaffinity(pid, sizeof(cpu_set_t), &cpuset);  // syscall
```

### Real-Time Scheduling

```cpp
#include <sched.h>

// SCHED_FIFO: highest priority, runs until it yields or blocks
// Priority 1–99 (99 = highest)
struct sched_param sp{};
sp.sched_priority = 90;  // high but leave room for IRQ handlers (99)
sched_setscheduler(0, SCHED_FIFO, &sp);

// Or via chrt at launch:
// chrt -f 90 ./my_app
// chrt -f -p 90 <pid>

// Yield explicitly in spin loops to avoid starving lower-priority threads:
sched_yield();

// SCHED_RR: round-robin within same priority (time-sliced)
// For latency: prefer SCHED_FIFO
```

### Interrupt Affinity (IRQ Isolation)

```bash
# Stop irqbalance from moving IRQs to your isolated CPUs
systemctl stop irqbalance
systemctl disable irqbalance

# Pin NIC interrupts to a non-isolated CPU (or dedicated CPU)
cat /proc/interrupts          # find your NIC interrupt number
echo 1 > /proc/irq/<N>/smp_affinity_list   # pin IRQ N to CPU 1

# Or use set_irq_affinity script from NIC vendor
```

### Memory: Disable Swap & Lock Pages

```bash
# Disable swap — page faults on cold memory = milliseconds of jitter
swapoff -a
# Remove swap from /etc/fstab permanently

# Lock process memory into RAM (no paging)
ulimit -l unlimited
```

```cpp
#include <sys/mman.h>
// Lock all current AND future pages
mlockall(MCL_CURRENT | MCL_FUTURE);
// Unlock when done
munlockall();
```

### Transparent Huge Pages (THP)

```bash
# THP: kernel automatically merges 4K pages into 2MB pages
# Problem: the merging process causes unpredictable latency spikes (khugepaged)
# For low latency: disable THP
echo never > /sys/kernel/mm/transparent_hugepage/enabled
echo never > /sys/kernel/mm/transparent_hugepage/defrag

# Use explicit huge pages instead (see Section 4)
```

### Kernel Tuning (sysctl)

```bash
# /etc/sysctl.conf additions for low-latency

# Network receive buffer (prevent packet drops)
net.core.rmem_max = 134217728
net.core.rmem_default = 134217728
net.ipv4.udp_mem = 102400 873800 134217728

# Disable network timestamps (save ~100ns per packet)
net.core.tstamp_allow_data = 0

# Reduce timer interrupt frequency (less jitter)
# CONFIG_HZ=100 in kernel (vs default 250 or 1000)
# Set at kernel compile time or via isolcpus + nohz_full

# Virtual memory
vm.swappiness = 0           # never swap (already have swapoff)
vm.dirty_ratio = 5          # flush dirty pages sooner (reduce write spikes)
vm.dirty_background_ratio = 2
```

---

## 3. Memory Management

### Pre-Allocate Everything

```cpp
// Rule: NEVER allocate/free on the hot path
// Allocate all memory at startup, return to pool when done

// Pool allocator — fixed-size blocks, O(1) alloc/free
template <typename T, std::size_t N>
class ObjectPool {
public:
    ObjectPool() {
        // Pre-allocate N objects
        storage_.resize(N);
        for (auto& obj : storage_) free_list_.push(&obj);
    }

    T* acquire() noexcept {
        if (free_list_.empty()) return nullptr;
        T* obj = free_list_.top();
        free_list_.pop();
        return new(obj) T{};         // placement new — no heap alloc
    }

    void release(T* obj) noexcept {
        obj->~T();
        free_list_.push(obj);
    }

private:
    std::vector<std::aligned_storage_t<sizeof(T), alignof(T)>> storage_;
    std::stack<T*> free_list_;
};

// Usage
ObjectPool<Order, 10000> order_pool;
// Startup: pool fills with 10000 Orders in contiguous memory
// Hot path: acquire() / release() — no malloc/free
```

### Huge Pages (2MB vs 4KB)

```cpp
// 2MB huge pages reduce TLB misses (TLB miss = ~100 ns extra)
// 4KB pages: 1GB buffer needs 262,144 TLB entries → many misses
// 2MB pages: 1GB buffer needs only 512 TLB entries → few misses

// Reserve huge pages at boot:
// GRUB: hugepages=1024    (1024 × 2MB = 2GB of huge pages)
// echo 1024 > /proc/sys/vm/nr_hugepages

// Allocate with mmap using MAP_HUGETLB
void* alloc_hugepages(std::size_t size) {
    void* p = mmap(nullptr, size,
                   PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB,
                   -1, 0);
    if (p == MAP_FAILED) {
        // Fall back to regular pages
        p = mmap(nullptr, size, PROT_READ|PROT_WRITE,
                 MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    }
    return p;
}

// Or use libhugetlbfs:
// void* p = get_huge_pages(size, GHP_DEFAULT);
```

### Pre-fault Memory (Eliminate Page Faults on Hot Path)

```cpp
// Problem: mmap is lazy — pages not actually allocated until first access
// First access → page fault → kernel maps physical page → ~1ms jitter

// Solution: pre-fault all pages before entering hot path
void prefault_memory(void* addr, std::size_t size) {
    volatile char* p = static_cast<volatile char*>(addr);
    const std::size_t page_size = 4096;
    for (std::size_t i = 0; i < size; i += page_size)
        p[i] = 0;    // touch every page → faults happen now, not later
}

// Or: madvise with MADV_POPULATE_WRITE (Linux 5.14+)
madvise(addr, size, MADV_POPULATE_WRITE);

// Or: use MAP_POPULATE flag in mmap
void* p = mmap(nullptr, size, PROT_READ|PROT_WRITE,
               MAP_PRIVATE|MAP_ANONYMOUS|MAP_POPULATE, -1, 0);
// MAP_POPULATE: prefaults synchronously during mmap call
```

### CPU Cache & Sequential Containers (`vector`, `array`)

> Chi tiết STL & so sánh container: [stl_containers.md](stl_containers.md) § Cache Locality

Trong hệ **low-latency**, thường **duyệt** container nhiều hơn **insert/xóa** giữa chừng. **Bộ nhớ liên tục** (sequential) giúp CPU cache và hardware prefetcher — lý do `std::vector` / `std::array` thường thắng `std::list` dù Big-O insert khác nhau.

#### CPU cache hierarchy (tóm tắt)

```
CPU core
  L1d  ~32 KB, ~4 cycles   — data cache
  L1i  ~32 KB              — instruction cache
  L2   ~256 KB–1 MB, ~12 cycles
  L3   shared, MB-scale, ~40+ cycles
  RAM  ~100 ns (hundreds of cycles)

Cache line (typical): 64 bytes — đơn vị load/store từ RAM → cache
```

| Khái niệm | Ý nghĩa |
|-----------|---------|
| **Spatial locality** | Truy cập địa chỉ A → CPU load cả **cache line** chứa A và hàng xóm |
| **Temporal locality** | Truy cập lại cùng vùng sớm → hit L1/L2 |
| **Cache miss** | Dữ liệu không trong cache → stall chờ RAM |
| **Prefetcher** | CPU đoán **truy cập tuần tự** và load trước cache line tiếp theo |

#### Sequential traversal — `vector` / `array`

```cpp
// Elements contiguous in memory
std::vector<int> v = /* ... */;
for (int x : v) { sum += x; }           // ideal: stride = sizeof(int)

std::array<double, 1024> a{};
for (std::size_t i = 0; i < a.size(); ++i)
    process(a[i]);                        // same cache line holds nhiều double
```

```
vector.data() →  [e0][e1][e2][e3]...[e15] | [e16]...
                  └──── 64 B cache line ────┘
                  sequential ++i → prefetcher loads next line early
```

**Vì sao nhanh trên hot path:**

1. Một cache miss → lấy **nhiều phần tử** cùng lúc (line 64B = 16× `int` hoặc 8× `double`).  
2. Hardware **stride prefetch** nhận pattern `for (i=0..n-1)`.  
3. Compiler có thể **vectorize** (AVX: 8 float/cycle).  
4. **TLB:** ít page hơn so với nhiều allocation rời (`list`).

#### `std::list` — phá sequential access

```
list:  [node0]→[node1]→[node2]   (mỗi node heap riêng, pointer chase)
         ↓       ↓       ↓
       cache miss mỗi bước — prefetcher khó đoán địa chỉ
```

| Pattern | `vector`/`array` | `std::list` |
|---------|------------------|-------------|
| `for (x : c)` full scan | Rất nhanh | Chậm (pointer chase) |
| Insert giữa chừng O(1) | O(n) shift | O(1) |
| `find` + erase scan | Thường **vẫn nhanh hơn list** vì scan sequential | O(1) erase sau khi tìm iterator |

**Quy tắc low-latency:** default container = **`vector`**; `reserve()` trước hot loop; tránh `push_back` trong inner loop nếu gây realloc.

```cpp
std::vector<Event> events;
events.reserve(expected_count);   // một lần — không realloc trên hot path
for (...) events.emplace_back(...);

// Hot loop — chỉ read
for (const auto& e : events)
    dispatch(e);
```

#### `std::array` vs `std::vector`

| | `std::array<T,N>` | `std::vector<T>` |
|---|-------------------|------------------|
| Storage | Stack hoặc embedded trong object | Heap (thường) |
| Size | Compile-time `N` | Runtime |
| Cache | Contiguous — giống vector | Contiguous |
| Low-latency | Fixed buffers, ring index, stack hot structs | Dynamic collections, `reserve` |

```cpp
// Stack — zero alloc, predictable address (watch stack limit)
std::array<Order, 256> ring;

// vector backing store for SPSC — still contiguous
std::array<T, 4096> buf_;  // in SPSCQueue — cache-friendly ring
```

#### AoS vs SoA khi duyệt một field

```cpp
// AoS — duyệt chỉ .price: load cả struct → lãng phí cache line
struct Tick { double price, volume; uint64_t ts; };
std::vector<Tick> ticks;

// SoA — duyệt price: mỗi cache line toàn price → tối đa throughput
struct TickSoA {
    std::vector<double> price;
    std::vector<double> volume;
    std::vector<uint64_t> ts;
};
```

Dùng **SoA** khi hot loop chỉ đụng 1–2 field; **AoS** khi luôn đọc cả object.

#### Software prefetch (khi stride không đủ cho hardware)

```cpp
#include <xmmintrin.h>  // _mm_prefetch

void process(const std::vector<LargeObject>& v) {
    for (std::size_t i = 0; i < v.size(); ++i) {
        if (i + 8 < v.size())
            _mm_prefetch(reinterpret_cast<const char*>(&v[i + 8]), _MM_HINT_T0);
        hot_work(v[i]);
    }
}
```

Chỉ dùng khi `perf` chứng minh cache miss cao — prefetch sai làm **chậm hơn**.

#### `deque` — giữa vector và list

Contiguous **từng chunk**, không một block duy nhất — sequential scan vẫn ổn nhưng kém `vector` một chút khi cross chunk boundary.

#### Checklist low-latency iteration

- [ ] Hot path: `vector`/`array`, `reserve`, không realloc trong loop  
- [ ] Tránh `list`/`map` node traversal trên path tần số cao  
- [ ] Struct lớn: cân SoA vs AoS  
- [ ] `alignas(64)` cho atomics/counters (false sharing) — khác nhưng liên quan cache line  
- [ ] Đo: `perf stat -e cache-misses,cache-references` hoặc `perf mem record`

### Cache-Friendly Data Layout

```cpp
// AoS (Array of Structs) — bad for cache when only one field needed
struct Particle { float x, y, z, mass, charge; };
Particle particles[1000];
// Iterating only x: loads y, z, mass, charge too → cache waste

// SoA (Struct of Arrays) — vectorizer-friendly, cache-efficient
struct Particles {
    float x[1000], y[1000], z[1000];
    float mass[1000], charge[1000];
};
// Iterating only x: entire cache line is x values → perfect cache utilization
// Also: SIMD can process 8 floats (256-bit AVX) at once

// False sharing: two threads writing adjacent cache-line data
struct alignas(64) PaddedAtomic {   // 64 = typical cache line size
    std::atomic<int64_t> value{0};
    char padding[64 - sizeof(std::atomic<int64_t>)];
};
PaddedAtomic counters[NUM_THREADS];
// Each counter on its own cache line → no false sharing
```

---

## 4. Lock-Free Programming

### SPSC Ring Buffer (Single Producer Single Consumer)

```cpp
// Zero-lock, zero-syscall message passing between two threads
// The backbone of most HFT / real-time systems

template <typename T, std::size_t N>
class SPSCQueue {
    static_assert((N & (N-1)) == 0, "N must be power of 2");
public:
    // Producer thread only
    bool push(T item) noexcept {
        const auto tail = tail_.load(std::memory_order_relaxed);
        const auto next = (tail + 1) & (N - 1);
        if (next == head_.load(std::memory_order_acquire))
            return false;   // full
        buf_[tail] = std::move(item);
        tail_.store(next, std::memory_order_release);
        return true;
    }

    // Consumer thread only
    bool pop(T& item) noexcept {
        const auto head = head_.load(std::memory_order_relaxed);
        if (head == tail_.load(std::memory_order_acquire))
            return false;   // empty
        item = std::move(buf_[head]);
        head_.store((head + 1) & (N - 1), std::memory_order_release);
        return true;
    }

private:
    alignas(64) std::atomic<std::size_t> head_{0};
    alignas(64) std::atomic<std::size_t> tail_{0};
    std::array<T, N> buf_{};
};
// No mutex, no condvar, no syscall — pure atomic store/load
```

### Spin-Wait vs Sleep

```cpp
// Sleep (bad for latency, good for CPU)
while (!queue.pop(msg)) {
    std::this_thread::sleep_for(std::chrono::microseconds(1)); // ~1µs wakeup jitter
}

// Spin-wait with pause (good for latency, burns CPU)
while (!queue.pop(msg)) {
    _mm_pause();  // x86: PAUSE instruction — reduces power, prevents pipeline abuse
}

// Hybrid: spin N iterations, then yield/sleep
int spin_count = 0;
while (!queue.pop(msg)) {
    if (++spin_count < 1000) {
        _mm_pause();
    } else if (spin_count < 10000) {
        sched_yield();
    } else {
        std::this_thread::sleep_for(std::chrono::microseconds(10));
        spin_count = 0;
    }
}
```

---

## 5. Kernel Bypass Networking

### Why Bypass the Kernel?

```
Normal path:  NIC → DMA → kernel ring buffer → copy → socket buffer → read()
                                                         ↑
                                              syscall + memory copy
                                              adds ~5–50 µs

Bypass path:  NIC → DMA → userspace ring buffer → direct read
              No syscall, no copy, no kernel involvement
              Achieves < 1 µs latency
```

### DPDK (Data Plane Development Kit)

```cpp
// DPDK: polls NIC hardware queue directly from userspace
// Used by: Cisco, Nokia, trading firms

// Pseudo-code structure:
rte_eal_init(argc, argv);            // init DPDK environment
struct rte_mempool* pool = rte_pktmbuf_pool_create(...);
struct rte_eth_dev_info dev_info;
rte_eth_dev_info_get(port_id, &dev_info);
rte_eth_dev_configure(port_id, 1, 1, &port_conf);
rte_eth_rx_queue_setup(port_id, 0, RX_RING_SIZE, ...);

// Hot path — runs on isolated core, 100% CPU
while (running) {
    struct rte_mbuf* pkts[BURST_SIZE];
    uint16_t nb = rte_eth_rx_burst(port_id, 0, pkts, BURST_SIZE);
    for (int i = 0; i < nb; i++) {
        process_packet(pkts[i]);
        rte_pktmbuf_free(pkts[i]);
    }
}
// No sleep, no yield — dedicated polling core
```

### Busy Polling (Lightweight Kernel Bypass)

```cpp
// Without full DPDK: reduce kernel latency using SO_BUSY_POLL
int busy_us = 50;   // poll for 50µs before sleeping
setsockopt(fd, SOL_SOCKET, SO_BUSY_POLL, &busy_us, sizeof(busy_us));

// Kernel will busy-poll the NIC for 50µs instead of going to sleep immediately
// Reduces latency at cost of CPU usage
// Also: enable globally: echo 50 > /proc/sys/net/core/busy_read
```

### UDP Multicast (Market Data)

```cpp
// Multicast: one sender → many receivers without duplicate unicast traffic
// Used for: exchange market data feeds

// Receiver setup:
int fd = socket(AF_INET, SOCK_DGRAM, 0);

// Bind to multicast port
sockaddr_in addr{};
addr.sin_family = AF_INET;
addr.sin_port = htons(MCAST_PORT);
addr.sin_addr.s_addr = htonl(INADDR_ANY);
bind(fd, (sockaddr*)&addr, sizeof(addr));

// Join multicast group
struct ip_mreq mreq{};
inet_pton(AF_INET, "239.1.1.1", &mreq.imr_multiaddr);
mreq.imr_interface.s_addr = htonl(INADDR_ANY);
setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));

// Disable loopback (we don't want our own messages)
int loop = 0;
setsockopt(fd, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop));

// Receive
char buf[1500];
recvfrom(fd, buf, sizeof(buf), 0, nullptr, nullptr);
```

---

## 6. Code-Level Optimizations

### Avoid Syscalls on the Hot Path

```cpp
// Every syscall = user→kernel mode switch ≈ 100–1000 ns
// Common hidden syscalls to eliminate:

// BAD: clock_gettime() is a syscall on older kernels
auto t1 = std::chrono::steady_clock::now();  // may be syscall

// GOOD: vDSO (virtual Dynamic Shared Object)
// Linux maps clock_gettime into userspace — no syscall!
// Verify: strace ./app 2>&1 | grep clock  (should show nothing if vDSO)
// Modern glibc uses vDSO automatically for CLOCK_MONOTONIC

// BEST for benchmarking: RDTSC (CPU cycle counter — no syscall ever)
inline uint64_t rdtsc() {
    uint32_t lo, hi;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}
// Convert to ns: ns = cycles * 1e9 / cpu_freq_hz
// Get freq: cat /proc/cpuinfo | grep "cpu MHz"

// RDTSCP: serializing version (waits for prior instructions to complete)
inline uint64_t rdtscp(uint32_t& aux) {
    uint32_t lo, hi;
    __asm__ __volatile__("rdtscp" : "=a"(lo), "=d"(hi), "=c"(aux));
    return ((uint64_t)hi << 32) | lo;
}
```

### Branch Prediction

```cpp
// Compiler hints (C++20)
if (unlikely_error) [[unlikely]] {
    handle_error();
}

// GCC/Clang built-ins (pre-C++20)
#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

if (unlikely(error_code)) {
    handle_error();
}

// Profile-guided optimization (PGO) — better than manual hints
// 1. Compile with -fprofile-generate
// 2. Run with realistic workload
// 3. Recompile with -fprofile-use
// Compiler uses actual branch frequencies from profiling data
```

### Prefetching

```cpp
// Tell CPU to load cache lines before you need them
// Hides memory latency by doing fetch in background

void process_array(int* data, int n) {
    const int PREFETCH_DISTANCE = 16;   // tune based on memory latency / compute ratio
    for (int i = 0; i < n; ++i) {
        // Prefetch element we'll need PREFETCH_DISTANCE iterations ahead
        if (i + PREFETCH_DISTANCE < n)
            __builtin_prefetch(&data[i + PREFETCH_DISTANCE], 0, 1);
            //                                                ^  ^ 
            //                                         read   locality(0-3)
        process(data[i]);
    }
}

// Locality hints:
// 0 = no temporal locality (evict from cache after use)
// 1 = L3 prefetch
// 2 = L2+L3 prefetch
// 3 = L1+L2+L3 prefetch (highest locality)
```

### SIMD (Single Instruction Multiple Data)

```cpp
#include <immintrin.h>

// Process 8 floats at once (AVX 256-bit)
void add_arrays(float* a, float* b, float* c, int n) {
    int i = 0;
    for (; i <= n - 8; i += 8) {
        __m256 va = _mm256_loadu_ps(&a[i]);
        __m256 vb = _mm256_loadu_ps(&b[i]);
        __m256 vc = _mm256_add_ps(va, vb);
        _mm256_storeu_ps(&c[i], vc);
    }
    for (; i < n; ++i) c[i] = a[i] + b[i];  // scalar tail
}
// 8× speedup vs scalar (with aligned loads: use _mm256_load_ps for more speed)

// Compiler auto-vectorization (no intrinsics needed):
// g++ -O3 -march=native -ffast-math
// Check: objdump -d app | grep ymm  (ymm = 256-bit AVX register)
```

### Eliminate Virtual Dispatch on Hot Path

```cpp
// Virtual call: load vptr → load vtable → indirect call → branch mispredict
// Cost: ~5–10 ns on modern CPUs when vtable not in cache

// Alternative 1: CRTP (compile-time polymorphism)
template <typename Derived>
class Strategy {
public:
    void execute() { static_cast<Derived*>(this)->execute_impl(); }
};
class FastStrategy : public Strategy<FastStrategy> {
public:
    void execute_impl() { /* inlined, no vtable */ }
};

// Alternative 2: std::variant + std::visit (known set of types)
using Handler = std::variant<OrderHandler, CancelHandler, QuoteHandler>;
void dispatch(Handler& h, const Message& msg) {
    std::visit([&](auto& handler) { handler.process(msg); }, h);
    // Compiler generates jump table — no vtable, but still indirect
}

// Alternative 3: function pointer array (manual vtable)
using ProcessFn = void(*)(const Message&);
ProcessFn dispatch_table[MSG_TYPE_COUNT] = {
    handle_order, handle_cancel, handle_quote
};
dispatch_table[msg.type](msg);  // one indirect call, predictable
```

### Compiler Flags for Low Latency

```makefile
CXXFLAGS = -O3                  # full optimization
           -march=native        # use all CPU features (AVX2, BMI2, etc.)
           -mtune=native        # tune instruction scheduling for this CPU
           -ffast-math          # allow fp reordering (careful with NaN/Inf)
           -funroll-loops       # unroll small loops
           -fomit-frame-pointer # free up one register (harder to debug)
           -flto                # link-time optimization (cross-TU inlining)
           -DNDEBUG             # disable assert()
           -fno-exceptions      # no exception tables (if you don't use them)
           -fno-rtti            # no RTTI (if you don't use dynamic_cast/typeid)
```

---

## 7. Startup Initialization Sequence

A proper low-latency system initializes everything before entering the hot path:

```cpp
class LowLatencySystem {
public:
    void initialize() {
        // === Phase 1: OS-level setup ===
        pin_to_cpu(WORKER_CPU);
        set_realtime_priority(90);
        mlockall(MCL_CURRENT | MCL_FUTURE);
        disable_swap_prefault();

        // === Phase 2: Memory pre-allocation ===
        message_pool_.reserve(MAX_MESSAGES);
        order_pool_.preallocate(MAX_ORDERS);
        ring_buffer_.initialize();
        prefault_all_memory();

        // === Phase 3: Network setup ===
        setup_network_interface();
        join_multicast_groups();
        set_socket_options();
        warmup_socket();        // send/receive dummy packets to warm kernel path

        // === Phase 4: Cache warming ===
        warmup_caches();        // touch all data structures once
        load_static_data();     // reference data into cache
        jit_compile_hot_paths(); // if using JIT

        // === Phase 5: Calibration ===
        calibrate_rdtsc();      // measure TSC frequency
        measure_baseline_latency();
        log_system_config();

        // === Phase 6: Sync & Go ===
        synchronize_clocks();   // PTP/GPS for timestamping
        ready_.store(true, std::memory_order_release);
    }

    void run_hot_loop() {
        // This is the critical path — nothing new allocated or freed
        while (running_.load(std::memory_order_relaxed)) {
            Message msg;
            if (ring_buffer_.pop(msg)) [[likely]] {
                process(msg);       // deterministic processing
            } else {
                _mm_pause();        // spin with pause
            }
        }
    }

private:
    void pin_to_cpu(int cpu) {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(cpu, &cpuset);
        sched_setaffinity(0, sizeof(cpuset), &cpuset);
    }

    void set_realtime_priority(int prio) {
        sched_param sp{prio};
        sched_setscheduler(0, SCHED_FIFO, &sp);
    }

    void warmup_caches() {
        // Read-touch all buffers to pull into L1/L2/L3
        volatile char sink = 0;
        auto* ptr = reinterpret_cast<volatile char*>(ring_buffer_.data());
        for (std::size_t i = 0; i < ring_buffer_.capacity(); i += 64)
            sink ^= ptr[i];
    }

    void calibrate_rdtsc() {
        // Measure TSC ticks per nanosecond
        auto t0 = std::chrono::steady_clock::now();
        uint64_t r0 = rdtsc();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        uint64_t r1 = rdtsc();
        auto t1 = std::chrono::steady_clock::now();
        auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1-t0).count();
        tsc_per_ns_ = static_cast<double>(r1 - r0) / ns;
    }

    SPSCQueue<Message, 65536> ring_buffer_;
    ObjectPool<Order, 10000>  order_pool_;
    std::atomic<bool> ready_{false};
    std::atomic<bool> running_{true};
    double tsc_per_ns_{1.0};
};
```

---

## 8. Measurement & Profiling

### Latency Histogram (HDR Histogram)

```cpp
// Never report average latency only — tail matters
// HDR Histogram: O(1) record, memory-efficient, wide range

// Simple percentile accumulator
class LatencyStats {
    std::array<std::atomic<uint64_t>, 1000000> buckets_{};  // 1ns buckets up to 1ms
public:
    void record(uint64_t ns) {
        if (ns < buckets_.size())
            buckets_[ns].fetch_add(1, std::memory_order_relaxed);
    }
    void report() {
        uint64_t total = 0;
        for (auto& b : buckets_) total += b.load();
        uint64_t p50_target = total * 50 / 100;
        uint64_t p99_target = total * 99 / 100;
        uint64_t p999_target = total * 999 / 1000;
        uint64_t cum = 0;
        for (std::size_t i = 0; i < buckets_.size(); ++i) {
            cum += buckets_[i].load();
            if (cum >= p50_target)  { printf("p50:  %zu ns\n", i); p50_target  = UINT64_MAX; }
            if (cum >= p99_target)  { printf("p99:  %zu ns\n", i); p99_target  = UINT64_MAX; }
            if (cum >= p999_target) { printf("p999: %zu ns\n", i); p999_target = UINT64_MAX; break; }
        }
    }
};

// Measure a section:
uint64_t t0 = rdtscp(aux);
// ... hot path ...
uint64_t t1 = rdtscp(aux);
stats.record((t1 - t0) / tsc_per_ns);
```

### Perf & Profiling Commands

```bash
# CPU cycles, cache misses, branch mispredicts
perf stat -e cycles,cache-misses,branch-misses,instructions ./app

# Flame graph (find hot functions)
perf record -F 99 -g ./app
perf script | stackcollapse-perf.pl | flamegraph.pl > flame.svg

# Cache behavior
perf stat -e L1-dcache-misses,L2-dcache-misses,LLC-load-misses ./app

# System-wide latency
perf trace ./app 2>&1 | head -100    # show all syscalls with timing

# NUMA stats
numastat -p <pid>                    # per-NUMA-node memory usage
numactl --show                       # current NUMA policy

# Interrupt counts per CPU
watch -n1 cat /proc/interrupts       # see if IRQs landing on your CPUs

# Context switches (should be near zero on isolated core)
pidstat -w -p <pid> 1
```

---

## 9. Common Latency Killers Checklist

| Source | Detection | Fix |
|---|---|---|
| Memory allocation on hot path | `perf` shows `malloc` | Pre-allocate pools |
| Page fault | `perf stat -e page-faults` | `mlockall`, MAP_POPULATE |
| Mutex contention | `perf` shows `futex` | Lock-free, SPSC, per-thread |
| Cache miss (L3) | `perf stat -e LLC-load-misses` | SoA layout, smaller structs |
| False sharing | `perf c2c` | Align to 64-byte cache line |
| Virtual dispatch | `perf` shows lots of `call *` | CRTP, variant, fn pointer |
| Syscall overhead | `strace -T ./app` | Batch, vDSO, io_uring |
| IRQ on worker CPU | `cat /proc/interrupts` | irqbalance off, affinity |
| CPU frequency scaling | `turbostat` shows freq changes | `performance` governor |
| C-state wake latency | `cpupower monitor` | Disable C2+ states |
| Swap / page reclaim | `vmstat -w 1` shows `si/so` | `swapoff -a` |
| THP compaction | `grep thp /proc/vmstat` | Disable THP |
| Hyperthreading cache fights | `perf stat -e` SMI count | Disable HT in BIOS |
| Kernel timer tick jitter | `nohz_full` not set | Add `nohz_full=<cpus>` |
| GC (in JVM/GC langs) | N/A (you're in C++) | Use C++ |

---

## 10. Quick-Start Initialization Script

```bash
#!/bin/bash
# run as root before launching low-latency process

set -euo pipefail

ISOLATED_CPUS="4,5,6,7"    # adjust for your system
NIC_IRQ=32                  # find with: grep eth0 /proc/interrupts
HOUSEKEEPING_CPU=0

echo "=== Low-Latency System Init ==="

# 1. CPU frequency governor
for cpu in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
    echo performance > "$cpu"
done
echo "[OK] CPU governor: performance"

# 2. Disable THP
echo never > /sys/kernel/mm/transparent_hugepage/enabled
echo never > /sys/kernel/mm/transparent_hugepage/defrag
echo "[OK] THP disabled"

# 3. Stop irqbalance
systemctl stop irqbalance 2>/dev/null || true
echo "[OK] irqbalance stopped"

# 4. Pin NIC IRQ to housekeeping CPU
echo $HOUSEKEEPING_CPU > /proc/irq/$NIC_IRQ/smp_affinity_list
echo "[OK] NIC IRQ pinned to CPU $HOUSEKEEPING_CPU"

# 5. Disable swap
swapoff -a
echo "[OK] Swap disabled"

# 6. Reserve huge pages
echo 512 > /proc/sys/vm/nr_hugepages   # 512 × 2MB = 1GB
echo "[OK] Huge pages reserved"

# 7. Tune network
sysctl -w net.core.rmem_max=134217728 >/dev/null
sysctl -w net.core.rmem_default=134217728 >/dev/null
echo "[OK] Network buffers tuned"

echo "=== Init complete. Launch with: ==="
echo "  chrt -f 90 taskset -c $ISOLATED_CPUS ./your_app"
```

---

## Interview Q&As

**Q [Mid]: What is the difference between latency and throughput, and why can optimizing one hurt the other?**

Latency is the time for one operation end-to-end; throughput is how many operations complete per second. They trade off via batching: batching requests increases throughput (amortize fixed overhead) but increases latency (a request waits for the batch to fill). Example: flushing a socket immediately gives low latency but sends many small packets (high overhead). Nagle's algorithm buffers data to coalesce packets — higher throughput, higher latency. Disable Nagle (`TCP_NODELAY`) for latency-critical paths. Another trade-off: spin-waiting burns a CPU core (reduces throughput available to other work) but gives sub-microsecond latency vs sleep-based waiting.

---

**Q [Mid]: Why do we pin threads to CPU cores in a low-latency system?**

Unpinned threads can be migrated by the OS scheduler to a different core at any time. Migration costs: (1) all L1/L2 cache content is lost — the new core's cache is cold, causing cache misses for the next thousands of accesses; (2) if crossing NUMA boundaries, memory accesses now go to the remote NUMA node (+100 ns per access); (3) migration itself takes time. Pinning (`sched_setaffinity`) prevents migration, keeps the cache warm, and ensures data stays on the same NUMA node. Combined with `isolcpus`, the kernel never schedules other tasks on those cores, eliminating scheduler-induced jitter.

---

**Q [Senior]: How does a SPSC ring buffer achieve lock-free operation safely?**

The SPSC (Single Producer Single Consumer) ring buffer relies on the invariant that only one thread writes `tail` (producer) and only one thread writes `head` (consumer). The producer reads `head` to check if there's space — it only needs to see a sufficiently recent value (acquire). The consumer reads `tail` to check if there's data — same. Since only one thread modifies each index, no CAS or mutex is needed. The critical detail is memory ordering: the producer stores data, then does a `release` store to `tail`. The consumer does an `acquire` load of `tail` — this establishes a happens-before: the consumer is guaranteed to see the data written before the release. Without `release`/`acquire`, the CPU or compiler could reorder the data write after the index update, causing the consumer to read garbage.

---

**Q [Senior]: Walk through all the sources of latency jitter on a Linux system and how to eliminate each.**

Starting from hardware: **C-state transitions** (CPU sleeping in C2/C6 takes µs to wake — fix: disable deep C-states in BIOS/kernel). **CPU frequency scaling** (turbo spikes and P-state transitions cause frequency variance — fix: `performance` governor, disable turbo). At OS level: **scheduler interruptions** (OS places other tasks on your core — fix: `isolcpus`, SCHED_FIFO). **Timer tick** (kernel fires 250–1000 Hz scheduling tick even on idle core — fix: `nohz_full`). **IRQ delivery** (NIC interrupts land on your core — fix: irqbalance off, pin IRQs). **Page faults** (first memory access pages in from disk — fix: `mlockall`, MAP_POPULATE). **THP compaction** (khugepaged migrates pages in background — fix: disable THP). At application level: **heap allocation** (malloc has locks — fix: pre-allocated pools). **False sharing** (two threads on adjacent cache-line data — fix: `alignas(64)`). **Virtual dispatch** (indirect call misses in branch predictor — fix: CRTP or function pointer tables).

---

**Q [Mid]: Tại sao duyệt `std::vector` nhanh hơn `std::list` trong hệ low-latency dù list có O(1) insert?**

> Hot path thường là **scan toàn bộ** (sum, filter, dispatch) chứ không insert giữa chừng liên tục. `vector`/`array` lưu phần tử **liên tục** — mỗi cache line 64B chứa nhiều phần tử, hardware prefetcher load trước line tiếp theo khi `for (i++)`. `list` mỗi node rải rác heap — mỗi bước `next` là pointer chase, cache miss liên tiếp, không SIMD. Insert O(1) của list không cứu được scan O(n) chậm hơn vector scan trong thực tế. Low-latency: `vector` + `reserve`, erase từ cuối hoặc batch rebuild nếu cần.

---

**Q [Senior]: Giải thích spatial locality khi iterate `std::array<double, 1000>`.**

> Phần tử nằm liên tiếp (stack hoặc trong object). Khi đọc `a[i]`, CPU load cache line 64B — thường chứa 8 `double`. Các lần đọc `a[i+1]..a[i+7]` hit L1. Prefetcher thấy stride 8 byte → fetch line kế tiếp trước khi cần. Throughput gần băng thông memory; latency mỗi access amortized thấp. Đổi sang `list<double>` cùng 1000 phần tử: mỗi `*` theo pointer mới → miss rate cao, tail latency tăng — quan trọng cho p99.

---

**Q [Senior]: Why is `mlockall(MCL_CURRENT | MCL_FUTURE)` important, and what does it actually do?**

`mlockall` instructs the kernel to lock all pages of the process — both currently mapped (`MCL_CURRENT`) and any future allocations (`MCL_FUTURE`) — into physical RAM, preventing them from being paged out to swap. Without it: even if you disable swap, the kernel may still reclaim pages from a process it considers inactive (rmap-based reclamation under memory pressure). The latency cost of a page fault is dramatic — a minor fault (page exists in page cache) takes ~1 µs, a major fault (must read from disk) takes ~10 ms. On a real-time core, either is catastrophic. `MCL_FUTURE` is critical for dynamic allocations after the call — if you pre-allocate your pool after calling `mlockall`, those pages are also locked. You also need `ulimit -l unlimited` (or sufficient RLIMIT_MEMLOCK) for this to work as a non-root user.
