# Operating Systems

## 1. What an OS Does

An operating system manages hardware resources and provides abstractions to applications.

Core responsibilities:

- Process and thread scheduling.
- Virtual memory.
- File systems.
- Device IO.
- Networking.
- Security and permissions.

Interview answer:

> The OS is the layer that turns hardware into safe abstractions: processes instead of raw CPU execution, virtual memory instead of physical RAM, files instead of disk blocks, and sockets instead of network device queues.

---

## 2. Process vs Thread

| Concept | Process | Thread |
|---------|---------|--------|
| Address space | Own virtual address space | Shares process address space |
| Isolation | Stronger | Weaker |
| Creation cost | Higher | Lower |
| Communication | IPC required | Shared memory possible |
| Failure impact | Usually isolated | Can crash whole process |

Rule of thumb:

- Use processes for isolation.
- Use threads for shared-memory concurrency.

---

## 3. Context Switch

A context switch happens when the CPU stops running one execution context and runs another.

The OS may save/restore:

- CPU registers.
- Program counter.
- Stack pointer.
- Memory mapping state.
- Scheduler accounting.

Cost:

- Direct CPU overhead.
- Cache/TLB disruption.
- Higher tail latency under load.

---

## 4. Virtual Memory

Virtual memory gives each process its own address space.

Benefits:

- Isolation.
- Simpler programming model.
- Demand paging.
- Memory-mapped files.

Key terms:

- Page: fixed-size memory block, commonly 4 KB.
- Page table: maps virtual pages to physical frames.
- TLB: CPU cache for address translations.
- Page fault: occurs when a virtual page is not currently mapped/resident.

Production angle:

> A service can have enough total RAM but still suffer latency spikes from page faults, TLB misses, or poor memory locality.

---

## 5. File Descriptors

On Unix-like systems, many resources are represented by file descriptors:

- Files.
- Sockets.
- Pipes.
- Devices.

Common syscalls:

- `open`
- `read`
- `write`
- `close`
- `select` / `poll` / `epoll`

Interview note:

> A socket is often handled like a file descriptor, but its behavior depends on network protocol, buffering, blocking mode, and peer state.

---

## 6. Scheduling

The scheduler decides which thread/process runs next.

Important ideas:

- Time slice.
- Priority.
- Run queue.
- Preemption.
- CPU affinity.

Common failure modes:

- Starvation.
- Priority inversion.
- Lock convoy.
- Oversubscription.

---

## 7. Blocking vs Non-Blocking IO

Blocking IO:

- Thread waits until operation completes.
- Simpler code.
- Can waste threads under high concurrency.

Non-blocking IO:

- Operation returns immediately if not ready.
- Requires event loop or readiness API.
- Scales better for many connections.

Examples:

- `select`, `poll`, `epoll`, `kqueue`, IOCP.

---

## Interview Questions

- What is the difference between process and thread?
- What happens during a context switch?
- What is a page fault?
- Why can too many threads hurt performance?
- What is the difference between blocking and non-blocking IO?
- Why does virtual memory help isolation?
