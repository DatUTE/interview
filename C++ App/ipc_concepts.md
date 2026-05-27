# Inter-Process Communication (IPC)

---

## What is IPC?

**Inter-Process Communication (IPC)** is a set of mechanisms that allow separate processes to exchange data, synchronize actions, and coordinate work.

Unlike threads (which share the same address space), **processes have isolated memory** — one process cannot directly read or write another's memory. IPC bridges that isolation.

### Why Processes Instead of Threads?

| Reason | Explanation |
|---|---|
| **Fault isolation** | A crash in one process doesn't take down others |
| **Security boundary** | OS enforces privilege separation between processes |
| **Language/runtime independence** | A C++ process can communicate with a Python process |
| **Separate lifecycle** | Processes can start, stop, restart independently |
| **Scalability** | Distribute work across machines (sockets) |

---

## IPC Mechanisms Overview

| Mechanism | Direction | Speed | Persistence | Cross-machine |
|---|---|---|---|---|
| **Pipe (anonymous)** | Unidirectional | Fast | No | No |
| **Named Pipe (FIFO)** | Unidirectional | Fast | No | No |
| **Message Queue** | Bidirectional | Medium | Yes (until deleted) | No |
| **Shared Memory** | Bidirectional | Fastest | No (volatile) | No |
| **Semaphore** | Sync only | Fast | No | No |
| **Signal** | Unidirectional | Fast | No | No |
| **Socket (Unix)** | Bidirectional | Fast | No | No |
| **Socket (TCP/UDP)** | Bidirectional | Medium | No | **Yes** |
| **Memory-mapped File** | Bidirectional | Fastest | Yes (file-backed) | No |

---

## 1. Pipes

### Theory

A pipe is a **unidirectional byte stream** between two processes. Data written to the write end can be read from the read end — first in, first out.

- **Anonymous pipe**: exists only between a parent and its child. Created with `pipe()`. No name in the filesystem. Destroyed when both ends are closed.
- **Named pipe (FIFO)**: has a filesystem path. Any process that knows the path can open it. Created with `mkfifo()`.

**Key properties**:
- Kernel-buffered (typically 64 KB on Linux)
- Write blocks when the buffer is full; read blocks when empty
- Reading from a pipe whose write end is closed returns EOF
- Writing to a pipe whose read end is closed sends `SIGPIPE`

### Anonymous Pipe — Example

```cpp
#include <unistd.h>
#include <cstring>
#include <iostream>

int main() {
    int fds[2];
    pipe(fds);  // fds[0] = read end, fds[1] = write end

    pid_t pid = fork();

    if (pid == 0) {
        // Child — reader
        close(fds[1]);                  // close write end
        char buf[128] = {};
        read(fds[0], buf, sizeof(buf));
        std::cout << "Child received: " << buf << "\n";
        close(fds[0]);
    } else {
        // Parent — writer
        close(fds[0]);                  // close read end
        const char* msg = "hello from parent";
        write(fds[1], msg, strlen(msg));
        close(fds[1]);
        wait(nullptr);
    }
    return 0;
}
```

### Named Pipe (FIFO) — Example

```cpp
// Writer process
#include <fcntl.h>
#include <unistd.h>

int main() {
    mkfifo("/tmp/myfifo", 0666);        // create FIFO if not exists
    int fd = open("/tmp/myfifo", O_WRONLY);
    write(fd, "ping", 4);
    close(fd);
}

// Reader process (separate program)
int main() {
    int fd = open("/tmp/myfifo", O_RDONLY);  // blocks until writer opens
    char buf[16] = {};
    read(fd, buf, sizeof(buf));
    printf("Got: %s\n", buf);
    close(fd);
}
```

### Use Cases

- **Shell pipelines**: `ls | grep .cpp | wc -l` — each `|` is an anonymous pipe
- **Parent spawning a child worker**: parent sends config, child sends results back via two pipes
- **Log streaming**: child process streams logs to parent's pipe for aggregation
- **Simple IPC where processes are related** (share a parent)

### Limitations

- Unidirectional — need two pipes for bidirectional communication
- Anonymous pipes: only between related processes (parent/child)
- No message boundaries — it's a byte stream, not packet-based

---

## 2. Message Queues

### Theory

A **message queue** is a kernel-managed linked list of messages. Processes can send and receive **discrete, typed messages** — unlike pipes, message boundaries are preserved.

- Each message has a **type** (integer) — receivers can selectively receive by type
- Queue persists until explicitly deleted or system reboots (POSIX MQ persists until `mq_unlink`)
- Two APIs: **System V** (`msgget`/`msgsnd`/`msgrcv`) and **POSIX** (`mq_open`/`mq_send`/`mq_receive`)
- POSIX message queues support **async notification** (`mq_notify`) — signal or thread when a message arrives

### POSIX Message Queue — Example

```cpp
// Sender
#include <mqueue.h>
#include <cstring>

int main() {
    mqd_t mq = mq_open("/my_queue",
                        O_CREAT | O_WRONLY,
                        0644,
                        nullptr);           // default attributes

    const char* msg = "task:process_image";
    mq_send(mq, msg, strlen(msg), 1);       // priority = 1
    mq_close(mq);
    return 0;
}

// Receiver
int main() {
    struct mq_attr attr { .mq_maxmsg = 10, .mq_msgsize = 256 };
    mqd_t mq = mq_open("/my_queue",
                        O_CREAT | O_RDONLY,
                        0644,
                        &attr);

    char buf[256] = {};
    unsigned int priority;
    mq_receive(mq, buf, sizeof(buf), &priority);
    printf("Received [pri=%u]: %s\n", priority, buf);

    mq_close(mq);
    mq_unlink("/my_queue");  // delete the queue
    return 0;
}
```

### Use Cases

- **Task dispatch**: a manager process sends work items to a pool of worker processes
- **Microservice messaging**: lightweight alternative to a full message broker for local services
- **Event notification with priority**: high-priority messages (errors, alerts) received before low-priority ones
- **Decoupled producer/consumer**: sender doesn't need the receiver to be running at the same moment

### Limitations

- Size limits per message and per queue (configurable but bounded)
- No built-in request-reply correlation (must implement yourself)
- Not suitable for large data transfers — use shared memory for bulk data

---

## 3. Shared Memory

### Theory

Shared memory is the **fastest IPC mechanism** — two or more processes map the same physical memory pages into their address spaces. Communication is just a memory read/write — no kernel involvement per access.

**Critical**: shared memory provides **no synchronization** — you must add a semaphore or mutex to prevent data races.

Two APIs:
- **POSIX**: `shm_open` + `mmap` (preferred, portable)
- **System V**: `shmget` + `shmat`

```
Process A address space        Process B address space
┌──────────────────┐           ┌──────────────────┐
│  Code / Stack    │           │  Code / Stack    │
│  Heap            │           │  Heap            │
│  [shared region] │←── same ──│  [shared region] │
└──────────────────┘  physical └──────────────────┘
                       memory
```

### POSIX Shared Memory — Example

```cpp
// Writer
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>

struct SharedData {
    int   counter;
    char  message[256];
};

int main() {
    int fd = shm_open("/my_shm", O_CREAT | O_RDWR, 0666);
    ftruncate(fd, sizeof(SharedData));

    SharedData* data = (SharedData*)mmap(
        nullptr, sizeof(SharedData),
        PROT_READ | PROT_WRITE,
        MAP_SHARED, fd, 0);

    data->counter = 42;
    strcpy(data->message, "hello from writer");

    munmap(data, sizeof(SharedData));
    close(fd);
    return 0;
}

// Reader
int main() {
    int fd = shm_open("/my_shm", O_RDONLY, 0666);

    SharedData* data = (SharedData*)mmap(
        nullptr, sizeof(SharedData),
        PROT_READ,
        MAP_SHARED, fd, 0);

    printf("counter=%d, msg=%s\n", data->counter, data->message);

    munmap(data, sizeof(SharedData));
    close(fd);
    shm_unlink("/my_shm");  // delete
    return 0;
}
```

### Shared Memory + Semaphore (Safe Access)

```cpp
#include <semaphore.h>

struct SharedRegion {
    sem_t mutex;      // embedded semaphore for synchronization
    int   data[1024];
};

// Initialize (writer, first process)
SharedRegion* shm = /* ... mmap ... */;
sem_init(&shm->mutex, 1, 1);  // 1 = shared between processes, initial value = 1

// Writer
sem_wait(&shm->mutex);
shm->data[0] = 99;
sem_post(&shm->mutex);

// Reader
sem_wait(&shm->mutex);
int val = shm->data[0];
sem_post(&shm->mutex);
```

### Use Cases

- **High-frequency data sharing**: sensor data, market tick data, video frames — where copy overhead of pipes is unacceptable
- **Shared cache**: multiple worker processes read a shared lookup table written by a master process
- **Zero-copy buffer passing**: one process fills a frame buffer, another renders it — no copy needed
- **Database buffer pool**: multiple DB processes share the same in-memory page cache

### Limitations

- **No synchronization built in** — must pair with semaphores/mutexes
- Pointers inside shared memory are meaningless (processes map at different virtual addresses) — use offsets instead
- If a process crashes while holding a lock on shared memory, other processes may deadlock

---

## 4. Semaphores

### Theory

A semaphore is a kernel-managed **integer counter** used purely for synchronization — not data transfer. Two operations:
- **`wait` (P / down)**: decrement; if count == 0, block until someone posts
- **`post` (V / up)**: increment; wake one blocked waiter

**Binary semaphore** (count 0 or 1): behaves like a mutex — mutual exclusion.
**Counting semaphore**: controls access to a pool of N resources.

Unlike `std::mutex`, POSIX semaphores work **across processes** (`sem_init` with `pshared=1`, or named semaphores via `sem_open`).

### Named Semaphore — Example

```cpp
#include <semaphore.h>
#include <fcntl.h>

// Process A — producer signals when data is ready
int main() {
    sem_t* sem = sem_open("/ready_signal", O_CREAT, 0666, 0); // initial = 0
    write_shared_data();
    sem_post(sem);   // signal consumer
    sem_close(sem);
}

// Process B — consumer waits for signal
int main() {
    sem_t* sem = sem_open("/ready_signal", 0);
    sem_wait(sem);   // blocks until producer posts
    read_shared_data();
    sem_close(sem);
    sem_unlink("/ready_signal");
}
```

### Counting Semaphore — Resource Pool

```cpp
// Limit concurrent access to a shared resource pool (e.g. 4 DB connections)
sem_t* pool_sem = sem_open("/db_pool", O_CREAT, 0666, 4);  // 4 slots

// Each worker:
sem_wait(pool_sem);        // acquire a slot (blocks if all 4 in use)
use_database_connection();
sem_post(pool_sem);        // release slot
```

### Use Cases

- **Synchronize shared memory access** between processes
- **Rate limiting**: cap number of concurrent worker processes accessing a resource
- **Event signaling**: one process signals another when an async event occurs
- **Barrier synchronization**: all N processes wait until all have reached a checkpoint

---

## 5. Signals

### Theory

Signals are **asynchronous notifications** sent to a process. They are the most primitive IPC form — no data payload (except `sigqueue` / real-time signals which carry a small value).

When a signal is delivered, the process's normal execution is interrupted and a **signal handler** runs.

Common signals:

| Signal | Default action | Meaning |
|---|---|---|
| `SIGTERM` | Terminate | Graceful shutdown request |
| `SIGKILL` | Terminate | Forced kill (cannot be caught) |
| `SIGINT` | Terminate | Ctrl+C from terminal |
| `SIGHUP` | Terminate | Terminal closed / reload config |
| `SIGUSR1/2` | Terminate | User-defined — use for custom IPC |
| `SIGCHLD` | Ignore | Child process changed state |
| `SIGPIPE` | Terminate | Write to broken pipe |
| `SIGALRM` | Terminate | Timer expired |

### Signal Handler — Example

```cpp
#include <csignal>
#include <atomic>
#include <iostream>

std::atomic<bool> running{true};

void handle_sigterm(int) {
    running = false;  // signal-safe: atomic store
}

int main() {
    std::signal(SIGTERM, handle_sigterm);
    std::signal(SIGINT,  handle_sigterm);

    while (running) {
        do_work();
    }

    cleanup();  // graceful shutdown
    return 0;
}
```

### Sending a Signal Between Processes

```cpp
#include <csignal>

// Send SIGUSR1 to process with known PID
kill(target_pid, SIGUSR1);

// Send signal to process group
killpg(process_group_id, SIGTERM);
```

### Signal Safety Rules

Signal handlers are **severely restricted** — you can only call async-signal-safe functions inside them:
- **Safe**: `write()`, `_exit()`, `sem_post()`, atomic operations
- **Unsafe**: `malloc()`, `printf()`, `std::cout`, `throw`, mutex operations

```cpp
// WRONG — malloc and printf are not signal-safe
void bad_handler(int) {
    printf("signal received\n");  // undefined behavior
    std::string s = "oops";       // allocates — UB
}

// CORRECT — set a flag, handle in main loop
std::atomic<int> signal_received{0};
void safe_handler(int sig) {
    signal_received.store(sig, std::memory_order_relaxed);
}
```

### Use Cases

- **Graceful shutdown**: `SIGTERM` handler sets a flag, main loop exits cleanly
- **Config reload**: `SIGHUP` triggers re-reading a config file (used by nginx, sshd, etc.)
- **Child reaping**: `SIGCHLD` notifies parent when a child exits
- **Watchdog**: a monitor process sends `SIGUSR1` as a heartbeat check
- **Timeout**: `SIGALRM` to interrupt a blocking operation after a deadline

### Limitations

- No data payload (only signal number, except `sigqueue`)
- Asynchronous and non-queued (multiple same signals may collapse to one — except real-time signals)
- Signal handler restrictions make complex logic impossible inline

---

## 6. Unix Domain Sockets

### Theory

Unix domain sockets (UDS) are like network sockets but **entirely within the kernel** — they use a filesystem path as an address instead of IP:port. Much faster than TCP (no network stack, no copying through TCP buffers).

Supports both:
- **`SOCK_STREAM`**: like TCP — reliable, ordered, connection-oriented byte stream
- **`SOCK_DGRAM`**: like UDP — message-based, connectionless (reliable locally)

Unique feature: **file descriptor passing** — you can send open file descriptors from one process to another through a UDS.

### Unix Domain Socket — Example

```cpp
// Server
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

const char* SOCKET_PATH = "/tmp/my.sock";

int main() {
    int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, SOCKET_PATH);

    unlink(SOCKET_PATH);  // remove stale socket file
    bind(server_fd, (sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 5);

    int client_fd = accept(server_fd, nullptr, nullptr);

    char buf[256] = {};
    read(client_fd, buf, sizeof(buf));
    printf("Received: %s\n", buf);

    const char* reply = "ACK";
    write(client_fd, reply, strlen(reply));

    close(client_fd);
    close(server_fd);
    unlink(SOCKET_PATH);
}

// Client
int main() {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, SOCKET_PATH);

    connect(fd, (sockaddr*)&addr, sizeof(addr));
    write(fd, "hello server", 12);

    char buf[64] = {};
    read(fd, buf, sizeof(buf));
    printf("Reply: %s\n", buf);
    close(fd);
}
```

### File Descriptor Passing (Advanced)

```cpp
// Send an open fd over UDS using ancillary data (cmsg)
void send_fd(int socket, int fd_to_send) {
    char buf[1] = {0};
    iovec iov{ buf, 1 };

    char cmsg_buf[CMSG_SPACE(sizeof(int))];
    msghdr msg{};
    msg.msg_iov    = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control    = cmsg_buf;
    msg.msg_controllen = sizeof(cmsg_buf);

    cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type  = SCM_RIGHTS;
    cmsg->cmsg_len   = CMSG_LEN(sizeof(int));
    memcpy(CMSG_DATA(cmsg), &fd_to_send, sizeof(int));

    sendmsg(socket, &msg, 0);
}
```

### Use Cases

- **Service communication on same host**: microservices talking locally (docker containers on same host)
- **Privilege separation**: a privileged helper process communicates with an unprivileged main process via UDS (e.g., sudo helper, gpg-agent)
- **Database clients**: PostgreSQL, MySQL use UDS for local connections (faster than TCP loopback)
- **File descriptor passing**: a process opens a privileged file/socket and passes the fd to a sandboxed process
- **D-Bus**: Linux desktop IPC system is built on UDS

---

## 7. TCP/UDP Sockets

### Theory

Network sockets allow IPC **across machines** over a network. The OS implements the full network stack (IP, TCP/UDP).

- **TCP (`SOCK_STREAM`)**: reliable, ordered, connection-oriented. Adds handshake overhead. Good for: commands, queries, structured data.
- **UDP (`SOCK_DGRAM`)**: unreliable, unordered, connectionless. Lower latency. Good for: real-time data, discovery, where occasional loss is acceptable.

### TCP Socket — Example

```cpp
// Server — listens for connections
#include <sys/socket.h>
#include <netinet/in.h>

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(8080);

    bind(server_fd, (sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 10);

    sockaddr_in client_addr{};
    socklen_t len = sizeof(client_addr);
    int client_fd = accept(server_fd, (sockaddr*)&client_addr, &len);

    char buf[1024] = {};
    ssize_t n = recv(client_fd, buf, sizeof(buf), 0);
    send(client_fd, buf, n, 0);  // echo back

    close(client_fd);
    close(server_fd);
}
```

### Use Cases

- **Distributed systems**: services on different machines communicate via TCP
- **Client-server architecture**: HTTP servers, gRPC, REST APIs
- **UDP for real-time**: game state sync, video streaming, DNS queries
- **Service discovery**: broadcast UDP to find peers on a LAN
- **Monitoring agents**: send metrics from multiple hosts to a central collector

---

## 8. Memory-Mapped Files

### Theory

`mmap()` maps a **file** directly into a process's address space. Multiple processes mapping the same file share the same physical pages — giving shared memory that is **file-backed** (persists across reboots).

Reads and writes go directly to the page cache — the OS handles flushing to disk lazily. Much faster than `read()`/`write()` for large files.

```
Process A             Process B
┌──────────┐          ┌──────────┐
│  mmap'd  │◄── same physical pages ──►│  mmap'd  │
│  region  │          │  region  │
└──────────┘          └──────────┘
      ↕  page cache ↕
┌──────────────────────┐
│      File on disk    │
└──────────────────────┘
```

### Example

```cpp
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

// Shared log buffer across processes
struct LogBuffer {
    std::atomic<uint32_t> write_pos;
    char data[1024 * 1024];  // 1 MB ring buffer
};

int main() {
    int fd = open("/tmp/shared_log.bin", O_CREAT | O_RDWR, 0666);
    ftruncate(fd, sizeof(LogBuffer));

    LogBuffer* log = (LogBuffer*)mmap(
        nullptr, sizeof(LogBuffer),
        PROT_READ | PROT_WRITE,
        MAP_SHARED, fd, 0);

    // Write to it — changes visible to all other processes mapping the same file
    uint32_t pos = log->write_pos.fetch_add(64, std::memory_order_relaxed);
    snprintf(log->data + pos, 64, "process %d: event at %ld\n", getpid(), time(nullptr));

    msync(log, sizeof(LogBuffer), MS_ASYNC);  // hint OS to flush (optional)
    munmap(log, sizeof(LogBuffer));
    close(fd);
}
```

### Use Cases

- **Persistent shared state**: configuration or state that survives process restarts
- **Large file processing**: multiple workers process different sections of a large file without copying it
- **Shared ring buffers**: high-performance logging, market data feeds, audio buffers
- **Database storage engines**: SQLite, LMDB map their entire database file for direct access
- **Zero-copy IPC**: producer writes to mmap'd buffer, consumer reads — no kernel copies

---

## Comparison & Decision Guide

### Choose Based on Need

```
Need cross-machine communication?
└── Yes → TCP/UDP Sockets
└── No  →
    Need data persistence across restarts?
    └── Yes → Memory-mapped File
    └── No  →
        Need maximum throughput / zero-copy?
        └── Yes → Shared Memory + Semaphore
        └── No  →
            Need message boundaries + priority?
            └── Yes → Message Queue
            └── No  →
                Related processes (parent/child)?
                └── Yes → Pipe (anonymous)
                └── No  →
                    Need bidirectional + auth/security?
                    └── Yes → Unix Domain Socket
                    └── No  → Named Pipe (FIFO) or Signal
```

### Performance Ranking (Same Machine)

```
Fastest ──────────────────────────────────────── Slowest
Shared Memory  mmap  Pipe  UDS  Message Queue  TCP loopback
  ~200ns       ~300ns ~1µs  ~2µs     ~5µs          ~10µs
(approximate, hardware-dependent)
```

### Full Comparison

| | Pipe | Named Pipe | Msg Queue | Shared Mem | Semaphore | Signal | Unix Socket | TCP Socket | mmap File |
|---|---|---|---|---|---|---|---|---|---|
| **Data transfer** | ✅ | ✅ | ✅ | ✅ | ❌ | ❌ | ✅ | ✅ | ✅ |
| **Message boundaries** | ❌ | ❌ | ✅ | Manual | — | ❌ | Stream/dgram | Stream/dgram | Manual |
| **Bidirectional** | ❌ | ❌ | ✅ | ✅ | — | ❌ | ✅ | ✅ | ✅ |
| **Unrelated processes** | ❌ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| **Cross-machine** | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ✅ | ❌ |
| **Persistence** | ❌ | ❌ | Partial | ❌ | ❌ | ❌ | ❌ | ❌ | ✅ |
| **Sync needed** | ❌ | ❌ | ❌ | ✅ | Built-in | ❌ | ❌ | ❌ | ✅ |
| **Kernel involvement per op** | Yes | Yes | Yes | **No** | Yes | Yes | Yes | Yes | **No** |

---

## Real-World System Examples

| System | IPC Used | Why |
|---|---|---|
| **Shell pipeline** (`ls \| grep`) | Anonymous pipe | Simple, fast, parent spawns children |
| **nginx worker processes** | Shared memory + signals | Workers share cache; `SIGHUP` for reload |
| **PostgreSQL client connection** | Unix domain socket (local) or TCP | UDS for local clients (faster), TCP for remote |
| **Chrome browser** | Named pipes / UDS | Renderer, GPU, network processes are isolated for security |
| **Redis** | TCP socket + Unix socket | TCP for remote, UDS for local clients |
| **LMDB / SQLite** | Memory-mapped file | Database file mmap'd directly into process space |
| **Docker daemon** | Unix domain socket | `/var/run/docker.sock` — CLI talks to daemon |
| **Pulseaudio** | Unix domain socket + shared memory | Control via UDS, audio data via shared memory |
| **ROS (Robot OS)** | TCP socket + shared memory | Topics between nodes on same/different machines |
| **Android Binder** | Custom kernel driver (mmap-based) | Zero-copy IPC optimized for mobile |

---

## Common Interview Questions

---

**[Mid]** What is the difference between a pipe and a message queue?

> A **pipe** is a byte stream — no message boundaries. If you write 10 bytes and 20 bytes, the reader may receive 30 bytes in one read or split across multiple reads. It is unidirectional, and anonymous pipes only work between related processes.
>
> A **message queue** preserves **message boundaries** — each `mq_send` is received as a discrete unit by `mq_receive`. It supports **priority ordering**, works between any processes by name, and messages persist in the queue even if no one is currently reading. It is naturally bidirectional (both sides can send/receive on the same queue).
>
> **Choose pipe** for streaming data between parent/child. **Choose message queue** when message atomicity, priority, or persistence matters.

---

**[Mid]** Why is shared memory the fastest IPC mechanism? What is its main danger?

> Shared memory is fastest because after the initial `mmap` setup, **no kernel calls are needed per access** — reads and writes go directly to physical memory pages shared between processes. Compare to pipes or sockets where data must be copied into kernel buffers and back out.
>
> The main danger is **no built-in synchronization**. Two processes writing simultaneously produce a data race — undefined behavior. You must add an external synchronization primitive (POSIX semaphore, or a `pthread_mutex` initialized with `PTHREAD_PROCESS_SHARED`) inside or alongside the shared region.
>
> A secondary danger: **if a process crashes while holding a lock** on the shared region, other processes may deadlock permanently. Robust mutexes (`PTHREAD_MUTEX_ROBUST`) can detect this and return `EOWNERDEAD` so the next owner can recover.

---

**[Mid]** What is a Unix domain socket and how does it differ from a TCP loopback socket (`127.0.0.1`)?

> Both allow bidirectional communication between processes on the same machine, but:
>
> | | Unix Domain Socket | TCP Loopback |
> |---|---|---|
> | Address | Filesystem path | IP:port |
> | Network stack | Bypassed entirely | Full TCP/IP stack |
> | Latency | ~2µs | ~10µs |
> | Overhead | Minimal | Handshake, checksums, flow control |
> | Authentication | File permissions (UID/GID) | IP-based only |
> | Fd passing | ✅ Supported | ❌ Not supported |
>
> For local IPC, **always prefer Unix domain sockets** — faster, more secure (filesystem permissions control access), and support file descriptor passing.

---

**[Senior]** A producer process writes sensor data at 100,000 samples/sec into a buffer shared with a consumer process. Design the IPC. What synchronization primitives would you use and why?

> **Mechanism**: shared memory (`shm_open` + `mmap`) — zero-copy, no kernel call per sample.
>
> **Layout**: a **lock-free ring buffer** in the shared region:
>
> ```cpp
> struct SensorRingBuffer {
>     static constexpr size_t CAPACITY = 1 << 20;  // power of 2
>     std::atomic<uint64_t> write_idx{0};           // producer writes here
>     std::atomic<uint64_t> read_idx{0};            // consumer reads here
>     float samples[CAPACITY];
> };
> ```
>
> **Synchronization**: `write_idx` and `read_idx` are `std::atomic<uint64_t>` with `release`/`acquire` ordering:
> - Producer: write sample → `fetch_add(write_idx, release)` — publishes new data
> - Consumer: `load(write_idx, acquire)` to check available data → read samples → update `read_idx`
>
> No mutex needed — single producer, single consumer lock-free ring buffer is wait-free.
>
> **Why not mutex?**: at 100K samples/sec, a mutex lock/unlock per sample (~1µs) would consume the entire budget. The lock-free ring buffer keeps per-sample overhead to nanoseconds.
>
> **Backpressure**: if the ring buffer fills (consumer too slow), producer either drops samples (acceptable for real-time sensors) or slows down (back-pressure, application-dependent).

---

**[Senior]** Explain how Chrome uses IPC for process isolation and why.

> Chrome uses a **multi-process architecture** for security and stability:
> - **Browser process**: trusted, handles UI and coordinates
> - **Renderer processes**: one per site origin (or tab) — runs untrusted JavaScript/HTML
> - **GPU process**: handles graphics API calls
> - **Network process**: handles all network I/O
> - **Utility processes**: spell check, file handling, etc.
>
> **Why isolation?**: a renderer process runs attacker-controlled code (JavaScript on a malicious website). If it's compromised, it should not be able to access the filesystem, other tabs, or the OS directly. Each renderer runs in a **sandbox** with almost no OS privileges.
>
> **IPC mechanism**: Chrome's **Mojo IPC** framework (built on named pipes / UDS + shared memory):
> - **Control messages** (small): sent over a pipe/UDS channel — capability requests, DOM events, navigation
> - **Large data** (images, video frames): passed via shared memory to avoid copying
> - **File descriptors**: privileged FDs (camera, microphone) are opened in the browser process and passed to the renderer via UDS fd-passing
>
> **Capability model**: the renderer can only do what the browser process explicitly allows via IPC — no direct syscalls to dangerous APIs. This is enforced by the OS sandbox (seccomp on Linux, sandbox on Windows/macOS).

---

**[Senior]** What is the difference between `SIGTERM` and `SIGKILL`? How do you implement graceful shutdown?

> - **`SIGTERM`**: can be caught, blocked, or ignored. The process has a chance to clean up — flush buffers, close connections, finish in-flight requests. The default action is termination, but you override it with a handler.
> - **`SIGKILL`**: cannot be caught, blocked, or ignored — the kernel kills the process immediately, no handler runs, no cleanup. Use only as a last resort.
>
> **Graceful shutdown pattern**:
>
> ```cpp
> std::atomic<bool> shutdown_requested{false};
>
> void sigterm_handler(int) {
>     shutdown_requested.store(true, std::memory_order_relaxed);
> }
>
> int main() {
>     signal(SIGTERM, sigterm_handler);
>     signal(SIGINT,  sigterm_handler);
>
>     Server server;
>     server.start();
>
>     while (!shutdown_requested) {
>         server.process_events();
>     }
>
>     // Graceful phase
>     server.stop_accepting_new_connections();
>     server.wait_for_in_flight_requests(std::chrono::seconds(30));
>     server.flush_and_close_all();
>
>     return 0;
> }
> ```
>
> **Production pattern** (used by systemd-managed services):
> 1. `systemctl stop service` sends `SIGTERM`
> 2. Service has `TimeoutStopSec` seconds (default 90s) to finish
> 3. If still running after timeout → `SIGKILL`
>
> Implement a **watchdog timeout** inside the handler to avoid hanging indefinitely if shutdown itself gets stuck.
