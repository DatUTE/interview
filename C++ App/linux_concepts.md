# Linux Concepts for C++ Engineers

---

## 1. Linux Architecture Overview

```
┌─────────────────────────────────────────────────────┐
│                   User Space                        │
│  Applications  Shell  Libraries (glibc, libstdc++)  │
│                        │                            │
│              System Call Interface                   │
│     (open, read, write, fork, mmap, socket, ...)    │
├─────────────────────────────────────────────────────┤
│                  Kernel Space                        │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌────────┐ │
│  │ Process  │ │  Memory  │ │   File   │ │Network │ │
│  │ Scheduler│ │  Manager │ │  System  │ │ Stack  │ │
│  └──────────┘ └──────────┘ └──────────┘ └────────┘ │
│                   Device Drivers                     │
├─────────────────────────────────────────────────────┤
│                   Hardware                           │
│          CPU   RAM   Disk   NIC   GPU                │
└─────────────────────────────────────────────────────┘
```

### Kernel Space vs User Space

| | **User Space** | **Kernel Space** |
|---|---|---|
| **Privilege level** | Ring 3 (unprivileged) | Ring 0 (full privilege) |
| **Memory access** | Own virtual address space | All physical memory |
| **Hardware access** | Via system calls only | Direct |
| **Fault isolation** | Crash kills only the process | Kernel panic (system crash) |
| **Context switch cost** | Thread switch: ~1µs | Syscall: ~100ns |

### System Call Mechanism

```cpp
// User code calls a C library wrapper (glibc)
int fd = open("file.txt", O_RDONLY);

// glibc wrapper internally does (simplified):
// 1. Set syscall number in rax (e.g., __NR_open = 2)
// 2. Set arguments in rdi, rsi, rdx, r10, r8, r9
// 3. Execute 'syscall' instruction → CPU switches to Ring 0
// 4. Kernel handles the request
// 5. CPU returns to Ring 3, return value in rax

// Inspect syscalls a process makes:
// strace ./program
// strace -e trace=open,read,write ./program
```

---

## 2. Processes

### Process Lifecycle

```
fork()         exec()          exit()
  │              │               │
  ▼              ▼               ▼
Created  →  Running  →  Waiting  →  Zombie
                  ↑         │
                  └─────────┘  (unblocked)

States:
R — Running or runnable (on run queue)
S — Sleeping (interruptible — waiting for I/O, signal)
D — Sleeping (uninterruptible — waiting for disk I/O) ← cannot be killed!
Z — Zombie (exited, waiting for parent to reap via wait())
T — Stopped (SIGSTOP or debugger)
```

### `fork()` — Creating a Process

`fork()` creates an **exact copy** of the calling process. The child gets a copy of the parent's address space (via **copy-on-write** — pages shared until one writes).

```cpp
#include <unistd.h>
#include <sys/wait.h>
#include <iostream>

int main() {
    std::cout << "Parent PID: " << getpid() << "\n";

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork failed");
        return 1;
    } else if (pid == 0) {
        // Child process — fork() returns 0
        std::cout << "Child PID: "  << getpid()
                  << " Parent PID: " << getppid() << "\n";
        return 0;
    } else {
        // Parent process — fork() returns child's PID
        std::cout << "Parent: child PID = " << pid << "\n";

        int status;
        waitpid(pid, &status, 0);   // wait for child to exit

        if (WIFEXITED(status))
            std::cout << "Child exited with: " << WEXITSTATUS(status) << "\n";
    }
    return 0;
}
```

### Copy-on-Write (COW)

After `fork()`, parent and child **share the same physical pages**. The kernel marks them read-only. On first write by either process, the kernel copies that page — only the written page is duplicated.

```
After fork():
Parent VA → [Page A shared, read-only]  ← same physical page
Child  VA  → [Page A shared, read-only]

Parent writes to Page A:
Parent VA → [Page A copy] ← new physical page (parent's private copy)
Child  VA  → [Page A original]
```

This makes `fork()` fast even for large processes — no actual copying until needed.

### `exec()` — Replacing the Process Image

```cpp
// exec replaces the current process image with a new program
// The PID stays the same; everything else is replaced
pid_t pid = fork();
if (pid == 0) {
    // Child: replace itself with 'ls -la'
    execl("/bin/ls", "ls", "-la", nullptr);
    // execl only returns on error
    perror("exec failed");
    exit(1);
}
```

`exec` family:
```cpp
execl("/bin/ls", "ls", "-la", nullptr);          // args as varargs
execlp("ls", "ls", "-la", nullptr);               // searches PATH
execle("/bin/ls", "ls", nullptr, envp);           // custom environment
execv("/bin/ls", argv);                           // args as array
execvp("ls", argv);                               // searches PATH
execvpe("ls", argv, envp);                        // PATH + custom env
```

### Zombie and Orphan Processes

**Zombie**: child has exited but parent hasn't called `wait()`. The child's entry in the process table persists (holds exit status). Too many zombies exhaust the process table.

```cpp
// Fix: always wait() for children
// Option 1: waitpid() in parent
waitpid(child_pid, &status, 0);

// Option 2: signal handler for SIGCHLD
signal(SIGCHLD, [](int){ while(waitpid(-1, nullptr, WNOHANG) > 0); });

// Option 3: double-fork — grandchild is adopted by init
pid_t pid = fork();
if (pid == 0) {
    if (fork() > 0) exit(0);  // middle child exits immediately
    // grandchild: parent is now init (PID 1), which auto-reaps
    do_work();
    exit(0);
}
waitpid(pid, nullptr, 0);   // reap middle child immediately
```

**Orphan**: parent exits before child. The child is **re-parented to init (PID 1)** which calls `wait()` automatically — not harmful.

### Process Groups and Sessions

```
Session (SID)
└── Process Group (PGID)
    ├── Process
    └── Process Group
        └── Process

# Shell example:
ls -la | grep .cpp | wc -l
# ls, grep, wc are in the same process group
# Ctrl+C sends SIGINT to the entire process group
```

```cpp
getpid();    // process ID
getppid();   // parent PID
getpgid(0);  // process group ID (0 = self)
getsid(0);   // session ID

setpgid(0, 0);    // create new process group
setsid();         // create new session (used by daemons)
```

### Process Priority and Scheduling

```bash
# View process priority
ps -el | head       # PRI column = kernel priority, NI = nice value

# Set nice value (-20 = highest priority, 19 = lowest)
nice -n 10 ./program           # start with nice=10
renice -n -5 -p 1234           # change running process 1234 to nice=-5

# Real-time scheduling (requires root)
chrt -f 50 ./program           # SCHED_FIFO, priority 50
chrt -r 50 ./program           # SCHED_RR (round-robin)
```

---

## 3. Threads (pthreads)

### Thread vs Process

| | **Thread** | **Process** |
|---|---|---|
| **Address space** | Shared with siblings | Private |
| **Memory** | Share heap, globals, fd table | Separate (COW after fork) |
| **Creation cost** | ~10µs | ~100µs |
| **Context switch** | ~1µs (same address space) | ~10µs (TLB flush) |
| **Communication** | Shared memory (fast) | IPC (pipe, socket, shm) |
| **Fault isolation** | None (crash kills all threads) | Strong (crash kills one process) |
| **Syscall** | `clone(CLONE_VM|CLONE_FS|...)` | `fork()` (`clone` with fewer flags) |

### POSIX Threads

```cpp
#include <pthread.h>

void* worker(void* arg) {
    int id = *(int*)arg;
    printf("Thread %d running\n", id);
    return nullptr;
}

int main() {
    pthread_t threads[4];
    int ids[4] = {0, 1, 2, 3};

    for (int i = 0; i < 4; ++i)
        pthread_create(&threads[i], nullptr, worker, &ids[i]);

    for (int i = 0; i < 4; ++i)
        pthread_join(threads[i], nullptr);   // wait + reap resources
}
// Compile: g++ -pthread program.cpp
```

### Thread-Local Storage (TLS)

```cpp
// Each thread gets its own copy — not shared
thread_local int tls_counter = 0;  // C++11

// POSIX equivalent
pthread_key_t key;
pthread_key_create(&key, destructor_fn);
pthread_setspecific(key, ptr);      // set value for current thread
void* val = pthread_getspecific(key); // get value for current thread
```

---

## 4. File System

### Everything is a File

Linux abstracts almost everything as a file: regular files, directories, devices (`/dev/sda`), sockets, pipes, and special files like `/proc` and `/sys`.

### Inodes

An **inode** stores file metadata — permissions, owner, timestamps, size, and pointers to data blocks. It does **not** store the file name (the directory entry maps name → inode number).

```bash
ls -i file.txt          # show inode number
stat file.txt           # full inode info: size, blocks, permissions, timestamps
df -i                   # inode usage per filesystem
```

```
Directory entry:     "file.txt" → inode 123456
                                       │
Inode 123456:                          ▼
┌─────────────────────────────────────────────┐
│ mode: 0644  uid: 1000  gid: 1000            │
│ size: 4096  links: 1                        │
│ atime: ...  mtime: ...  ctime: ...          │
│ blocks: [block_ptr_1, block_ptr_2, ...]     │
└─────────────────────────────────────────────┘
```

### Hard Links vs Soft Links

```bash
# Hard link — another directory entry pointing to the SAME inode
ln file.txt hardlink.txt
ls -li file.txt hardlink.txt   # same inode number, link count = 2
# Deleting original doesn't delete data — inode survives until link count = 0

# Soft link (symlink) — a file whose content is a path string
ln -s file.txt softlink.txt
ls -li softlink.txt            # different inode, shows "softlink.txt -> file.txt"
# Deleting original → dangling symlink (softlink.txt points to nonexistent file)
```

| | **Hard Link** | **Soft Link** |
|---|---|---|
| Inode | Same as original | New inode |
| Works across filesystems | No | Yes |
| Works for directories | No (prevents cycles) | Yes |
| Survives original deletion | Yes | No (dangling) |
| `ls -l` | Normal file | `l` type, shows `→ target` |

### File Descriptors

Every open file is represented by an integer **file descriptor (fd)** in the process's fd table.

```
Process fd table:
fd 0 → stdin  (keyboard)
fd 1 → stdout (terminal)
fd 2 → stderr (terminal)
fd 3 → file.txt (opened with open())
fd 4 → socket
...

Each entry points to a kernel file description (struct file):
- position (offset)
- flags (O_RDONLY, O_NONBLOCK, ...)
- pointer to inode
```

```cpp
#include <fcntl.h>
#include <unistd.h>

// Open
int fd = open("file.txt", O_RDONLY);
int fd = open("file.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
int fd = open("file.txt", O_RDWR | O_APPEND);

// Read / Write
char buf[4096];
ssize_t n = read(fd, buf, sizeof(buf));   // returns bytes read, 0=EOF, -1=error
ssize_t w = write(fd, buf, n);

// Seek
off_t pos = lseek(fd, 0, SEEK_SET);   // seek to beginning
lseek(fd, 0, SEEK_END);               // seek to end (get file size)
lseek(fd, -10, SEEK_CUR);             // seek 10 bytes backward from current

// Close
close(fd);

// Duplicate fd
int fd2 = dup(fd);       // fd2 is a new fd pointing to same file description
int fd2 = dup2(fd, 5);   // make fd 5 point to same file as fd
```

### File Permissions

```
-rwxr-xr--  1 user group  4096 Jan 1 00:00 file
│└──────────── owner: rwx (7)
│   └──────── group: r-x (5)
│      └───── others: r-- (4)
└────────────── file type: - regular, d directory, l symlink, c char dev, b block dev

# Numeric: r=4, w=2, x=1
chmod 755 file    # rwxr-xr-x — owner all, group/others read+exec
chmod 644 file    # rw-r--r-- — owner read+write, others read only
chmod +x file     # add execute for all
chmod o-w file    # remove write for others

# Special bits
chmod u+s file    # setuid — runs as file owner, not caller
chmod g+s dir     # setgid — new files inherit group
chmod +t dir      # sticky — only owner can delete files (e.g., /tmp)

# Ownership
chown user:group file
chown -R user:group directory/
```

### `/proc` Virtual Filesystem

`/proc` is a **pseudo-filesystem** — no disk data, all generated on-the-fly by the kernel. Essential for process inspection.

```bash
/proc/PID/
├── cmdline       # command line arguments (null-separated)
├── status        # human-readable process status
├── maps          # virtual memory map (address ranges, permissions, mappings)
├── fd/           # symbolic links to open file descriptors
├── stat          # process statistics (CPU time, state, etc.)
├── mem           # process memory (readable with ptrace)
├── environ       # environment variables
└── exe           # symlink to the executable

# Useful reads:
cat /proc/1234/status         # process state, memory usage, threads
cat /proc/1234/maps           # virtual memory layout
ls -la /proc/1234/fd          # open file descriptors
cat /proc/meminfo             # system memory info
cat /proc/cpuinfo             # CPU info
cat /proc/loadavg             # load average
cat /proc/net/tcp             # TCP connections
```

---

## 5. I/O Models

### Blocking vs Non-Blocking I/O

```cpp
// Blocking (default) — read() blocks until data available
int fd = open("file.txt", O_RDONLY);
char buf[1024];
ssize_t n = read(fd, buf, sizeof(buf));  // blocks if no data

// Non-blocking — read() returns immediately with EAGAIN if no data
int fd = open("device", O_RDONLY | O_NONBLOCK);
ssize_t n = read(fd, buf, sizeof(buf));
if (n < 0 && errno == EAGAIN) {
    // no data available right now — try again later
}

// Set non-blocking on existing fd
int flags = fcntl(fd, F_GETFL, 0);
fcntl(fd, F_SETFL, flags | O_NONBLOCK);
```

### `select` — I/O Multiplexing (Legacy)

Wait for any of multiple fds to become ready. O(n) scan, max 1024 fds.

```cpp
#include <sys/select.h>

fd_set read_fds;
FD_ZERO(&read_fds);
FD_SET(fd1, &read_fds);
FD_SET(fd2, &read_fds);

struct timeval timeout = {5, 0};   // 5 seconds

int ready = select(max_fd + 1, &read_fds, nullptr, nullptr, &timeout);
if (ready > 0) {
    if (FD_ISSET(fd1, &read_fds)) { /* fd1 is readable */ }
    if (FD_ISSET(fd2, &read_fds)) { /* fd2 is readable */ }
} else if (ready == 0) {
    // timeout
}
```

### `epoll` — Scalable I/O Multiplexing (Linux)

O(1) per event — designed for thousands of connections. The kernel maintains the interest list; only reports ready fds.

```cpp
#include <sys/epoll.h>

// Create epoll instance
int epfd = epoll_create1(0);

// Register fd of interest
struct epoll_event ev;
ev.events  = EPOLLIN | EPOLLET;   // readable + edge-triggered
ev.data.fd = client_fd;
epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &ev);

// Event loop
std::array<epoll_event, 64> events;
while (true) {
    int n = epoll_wait(epfd, events.data(), events.size(), -1); // -1 = block forever
    for (int i = 0; i < n; ++i) {
        if (events[i].events & EPOLLIN) {
            int fd = events[i].data.fd;
            handle_read(fd);
        }
        if (events[i].events & (EPOLLERR | EPOLLHUP)) {
            close(events[i].data.fd);
            epoll_ctl(epfd, EPOLL_CTL_DEL, events[i].data.fd, nullptr);
        }
    }
}
close(epfd);
```

### Level-Triggered vs Edge-Triggered

| | **Level-Triggered (LT)** | **Edge-Triggered (ET)** |
|---|---|---|
| **When notified** | As long as data is available | Only when state changes (new data arrives) |
| **Unread data** | epoll_wait returns again next call | Notified once — must read until EAGAIN |
| **Risk** | Lower (simpler to use) | Missing data if you don't drain fully |
| **Performance** | Slightly more wakeups | Fewer wakeups, better for high throughput |
| **Default** | Level-triggered | `EPOLLET` flag for edge-triggered |

```cpp
// Edge-triggered: must read ALL available data in one go
void handle_read_ET(int fd) {
    while (true) {
        char buf[4096];
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n > 0) {
            process(buf, n);
        } else if (n == 0) {
            close(fd);        // EOF — peer disconnected
            break;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;        // all data read — stop
            perror("read error");
            break;
        }
    }
}
```

### `select` vs `poll` vs `epoll`

| | `select` | `poll` | `epoll` |
|---|---|---|---|
| **Max fds** | 1024 (FD_SETSIZE) | Unlimited | Unlimited |
| **Scan cost** | O(max_fd) | O(n fds) | O(events) |
| **API** | Bit mask | Array of pollfd | Kernel-managed list |
| **Level/Edge** | Level only | Level only | Both |
| **Cross-platform** | POSIX | POSIX | Linux only |
| **Use today** | Legacy | Portable | Preferred on Linux |

---

## 6. Linux Kernel Scheduler

### Completely Fair Scheduler (CFS)

The default Linux scheduler since kernel 2.6.23. Aims to give each runnable process a **fair share of CPU time**.

**Key concept — virtual runtime (`vruntime`)**: each process tracks how much CPU time it has consumed, weighted by priority. CFS always runs the process with the **smallest vruntime** next.

```
Runqueue (Red-Black Tree, keyed by vruntime):

      vruntime=10
         /    \
  vrt=5       vrt=15
   /  \
vrt=2  vrt=8   ← leftmost node = next to run
```

```bash
# View scheduler info per process
cat /proc/PID/sched

# View scheduler statistics
cat /proc/schedstat

# Scheduling policy of a process
chrt -p PID          # shows policy and priority
```

### Context Switching

A context switch saves the CPU state (registers, stack pointer, program counter) of the outgoing process and restores the incoming process's state.

```
Context switch cost breakdown:
- Save/restore registers:         ~100ns
- TLB flush (if process switch):  ~200-500ns
- Cache warm-up cost:             ~1-100µs (depends on working set)

Thread switch within same process: no TLB flush → faster (~1-3µs)
Process switch: TLB flush required → slower (~5-10µs + cache effects)
```

```bash
# View context switches
vmstat 1              # cs column = context switches per second
pidstat -w -p PID 1   # per-process context switch rate
cat /proc/PID/status  # voluntary_ctxt_switches, nonvoluntary_ctxt_switches
```

### CPU Affinity

Pin a process/thread to specific CPUs to avoid cache thrashing from migration.

```cpp
#include <sched.h>

cpu_set_t cpuset;
CPU_ZERO(&cpuset);
CPU_SET(0, &cpuset);   // allow only CPU 0
CPU_SET(1, &cpuset);   // allow CPU 0 and 1

sched_setaffinity(0, sizeof(cpuset), &cpuset);   // 0 = current process
pthread_setaffinity_np(thread, sizeof(cpuset), &cpuset);
```

```bash
taskset -c 0,1 ./program     # run program on CPUs 0 and 1
taskset -cp 0,1 PID          # set affinity of running process
```

---

## 7. Memory Management

### Virtual Memory Layout (64-bit Linux)

```
0xFFFFFFFFFFFFFFFF
│   Kernel Space (top 128TB)        ← not accessible from user code
0xFFFF800000000000
│   [non-canonical gap]
0x00007FFFFFFFFFFF
│   Stack (grows down ↓)
│   ...
│   mmap region (shared libs, mmap())
│   ...
│   Heap (grows up ↑)
│   BSS segment (uninitialized globals)
│   Data segment (initialized globals)
│   Text segment (code)
0x0000000000400000 (typical ELF load address)
│   [unmapped — catching null dereferences]
0x0000000000000000
```

### `mmap` — Memory Mapping

```cpp
#include <sys/mman.h>

// Anonymous mapping (not backed by a file) — like a heap allocation but page-aligned
void* mem = mmap(nullptr,          // let kernel choose address
                 4096,             // size (must be page-aligned)
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS,
                 -1, 0);           // no file

// File-backed mapping — file content accessible as memory
int fd = open("data.bin", O_RDONLY);
struct stat st; fstat(fd, &st);
void* data = mmap(nullptr, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
close(fd);   // fd can be closed after mmap — mapping remains

// Unmap when done
munmap(mem, 4096);
munmap(data, st.st_size);

// Lock pages in RAM (prevent swapping) — requires CAP_IPC_LOCK
mlock(mem, 4096);
mlockall(MCL_CURRENT | MCL_FUTURE);

// Advise kernel about access patterns
madvise(data, size, MADV_SEQUENTIAL);   // prefetch sequentially
madvise(data, size, MADV_RANDOM);       // don't prefetch
madvise(data, size, MADV_WILLNEED);     // prefetch now
madvise(data, size, MADV_DONTNEED);     // kernel can reclaim pages
```

### Memory Limits and OOM Killer

```bash
# View process memory usage
cat /proc/PID/status | grep -E "VmRSS|VmSize|VmPeak"
# VmSize  = virtual memory size (all mapped regions)
# VmRSS   = resident set size (physical RAM used)
# VmPeak  = peak virtual memory

# System memory
free -h
cat /proc/meminfo

# Set memory limits (per process)
ulimit -v 1048576   # limit virtual memory to 1GB
ulimit -m 524288    # limit RSS to 512MB

# OOM killer: when RAM is exhausted, kernel kills a process
# OOM score: higher = more likely to be killed
cat /proc/PID/oom_score
echo -1000 > /proc/PID/oom_score_adj   # protect from OOM killer (-1000 = never kill)
echo 1000  > /proc/PID/oom_score_adj   # most likely to be killed
```

---

## 8. Signals

### Signal Overview

```bash
kill -l                    # list all signals
kill -SIGTERM PID          # graceful shutdown request
kill -SIGKILL PID          # force kill (cannot be caught)
kill -SIGSTOP PID          # pause process
kill -SIGCONT PID          # resume process
kill -SIGUSR1 PID          # user-defined signal 1
kill -0 PID                # check if process exists (no signal sent)

# Send to process group
kill -SIGTERM -PGID        # negative PID = process group
```

### Signal Disposition

```cpp
#include <csignal>
#include <sys/signalfd.h>

// 1. Default — kernel's default action (terminate, core dump, ignore, stop)
signal(SIGTERM, SIG_DFL);

// 2. Ignore
signal(SIGPIPE, SIG_IGN);   // ignore SIGPIPE — important for network servers

// 3. Custom handler (limited — only async-signal-safe functions)
std::atomic<bool> running{true};
signal(SIGTERM, [](int){ running = false; });

// 4. sigaction — preferred (more control than signal())
struct sigaction sa{};
sa.sa_handler = my_handler;
sigemptyset(&sa.sa_mask);
sa.sa_flags = SA_RESTART;   // restart interrupted syscalls
sigaction(SIGTERM, &sa, nullptr);

// 5. Signal mask — block signals in current thread
sigset_t mask;
sigemptyset(&mask);
sigaddset(&mask, SIGTERM);
pthread_sigmask(SIG_BLOCK, &mask, nullptr);   // block SIGTERM

// 6. signalfd — receive signals as file descriptor events (epoll-friendly)
sigset_t mask;
sigemptyset(&mask);
sigaddset(&mask, SIGTERM);
sigaddset(&mask, SIGINT);
sigprocmask(SIG_BLOCK, &mask, nullptr);   // block so handler doesn't fire
int sfd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
// Now read from sfd to receive signal info — works with epoll!
```

---

## 9. Pipes and Redirection

### Shell Redirection Internals

```bash
./program > output.txt     # stdout (fd 1) redirected to file
./program 2> error.txt     # stderr (fd 2) redirected to file
./program > out.txt 2>&1   # both stdout and stderr to same file
./program < input.txt      # stdin (fd 0) from file
./program >> out.txt       # append stdout to file

# Pipe: stdout of ls → stdin of grep
ls -la | grep ".cpp"
# Kernel creates a pipe; ls writes to write-end, grep reads from read-end
```

```cpp
// Implementing a pipe between two processes
int pipefd[2];
pipe(pipefd);   // pipefd[0] = read end, pipefd[1] = write end

if (fork() == 0) {
    // Child: replace stdin with pipe read end
    close(pipefd[1]);               // close write end
    dup2(pipefd[0], STDIN_FILENO);  // fd 0 = pipe read end
    close(pipefd[0]);
    execlp("grep", "grep", ".cpp", nullptr);
} else {
    // Parent: replace stdout with pipe write end
    close(pipefd[0]);               // close read end
    dup2(pipefd[1], STDOUT_FILENO); // fd 1 = pipe write end
    close(pipefd[1]);
    execlp("ls", "ls", "-la", nullptr);
}
```

---

## 10. Essential Commands

### Process Management

```bash
ps aux                     # all processes (user, pid, cpu, mem, command)
ps -ef                     # all processes (full format)
ps -eLf                    # all threads
top / htop                 # live process view
pgrep nginx                # find PID by name
pkill nginx                # kill by name
kill PID                   # send SIGTERM
kill -9 PID                # send SIGKILL
killall nginx              # kill all processes named nginx

jobs                       # list background jobs in current shell
fg %1                      # bring job 1 to foreground
bg %1                      # send job 1 to background
nohup ./program &          # run immune to hangup (survives terminal close)
disown %1                  # detach job from shell
```

### File Operations

```bash
ls -la                     # long listing with hidden files
find /path -name "*.cpp"   # find files by name
find /path -newer ref.txt  # files newer than ref.txt
find /path -size +100M     # files larger than 100MB
locate filename            # fast name lookup (uses index)

stat file.txt              # inode info
file binary                # determine file type
strings binary             # extract printable strings
hexdump -C file.bin        # hex + ASCII dump

cp -a src/ dst/            # archive copy (preserves permissions, timestamps)
rsync -av src/ dst/        # efficient sync (skips unchanged files)
ln -s target link          # create symlink
readlink -f link           # resolve symlink to absolute path
```

### Disk and Filesystem

```bash
df -h                      # disk usage by filesystem
du -sh directory/          # disk usage of directory
du -sh * | sort -h         # sorted disk usage of current dir contents
lsblk                      # block device tree
mount                      # show mounted filesystems
fdisk -l                   # partition table

# Check filesystem
fsck /dev/sda1             # check and repair (unmounted)
tune2fs -l /dev/sda1       # ext4 filesystem info
```

### Network

```bash
ip addr                    # network interfaces and addresses
ip route                   # routing table
ss -tulpn                  # listening TCP/UDP sockets with process names
netstat -tulpn             # older alternative to ss
ping host                  # ICMP reachability
traceroute host            # path to host
curl -v http://host/path   # HTTP request with verbose headers
wget URL                   # download file
tcpdump -i eth0 port 8080  # capture packets on port 8080
wireshark                  # GUI packet capture/analysis
```

### Text Processing

```bash
grep -r "pattern" dir/     # recursive search
grep -n "pattern" file     # with line numbers
grep -v "pattern" file     # invert match
awk '{print $1, $3}' file  # print columns 1 and 3
sed 's/old/new/g' file     # replace all occurrences
sort file                  # sort lines
sort -k2 -n file           # sort by column 2, numerically
uniq -c                    # count consecutive duplicates
wc -l file                 # count lines
cut -d: -f1 /etc/passwd    # cut column 1 with ':' delimiter
head -n 20 file            # first 20 lines
tail -n 20 file            # last 20 lines
tail -f log.txt            # follow log in real-time
```

---

## 11. Performance Analysis Tools

### `strace` — System Call Tracer

```bash
strace ./program                    # trace all syscalls
strace -e trace=open,read ./prog    # trace specific syscalls
strace -c ./program                 # summary: count + time per syscall
strace -f ./program                 # follow forks (trace child processes)
strace -p PID                       # attach to running process
strace -o output.txt ./program      # write to file

# Example output:
# open("config.json", O_RDONLY)    = 3
# read(3, "{\"key\":\"val\"}", 4096) = 17
# close(3)                          = 0
```

### `perf` — Performance Profiler

```bash
# CPU profiling — where is time spent?
perf stat ./program                 # summary: cache misses, branch mispredicts, IPC
perf record -g ./program            # record call graph
perf report                         # interactive annotated report

# Specific events
perf stat -e cache-misses,cache-references,instructions,cycles ./program

# Flame graph (via Brendan Gregg's scripts)
perf record -F 99 -g ./program
perf script | ./stackcollapse-perf.pl | ./flamegraph.pl > flame.svg
```

### `valgrind` — Memory Error Detector

```bash
valgrind --leak-check=full --show-leak-kinds=all ./program
valgrind --tool=callgrind ./program    # call graph profiler
callgrind_annotate callgrind.out.*     # annotated source
valgrind --tool=cachegrind ./program   # cache profiler
valgrind --tool=helgrind ./program     # thread error detector (data races)
```

### `lsof` — List Open Files

```bash
lsof -p PID                         # all open fds for process
lsof -i :8080                       # processes using port 8080
lsof /path/to/file                  # processes with this file open
lsof -u username                    # all fds for user
```

### `/proc` Inspection

```bash
# Memory map
cat /proc/PID/maps
# 7f8a00000000-7f8a00021000 rw-p 00000000 00:00 0  [heap]
# 7fff00000000-7fff00021000 rw-p 00000000 00:00 0  [stack]

# Open file descriptors
ls -la /proc/PID/fd

# CPU and memory stats
cat /proc/PID/stat          # raw stats (used by ps/top)
cat /proc/PID/statm         # memory stats in pages
cat /proc/PID/status        # human-readable combined stats

# System-wide
cat /proc/loadavg           # 1, 5, 15 min load averages + running/total threads
cat /proc/interrupts        # interrupt counts per CPU
cat /proc/softirqs          # software interrupt counts
```

---

## 12. Daemons and Services

### Creating a Daemon (Classic)

```cpp
// Daemonize: detach from terminal, run in background
void daemonize() {
    // 1. Fork — parent exits, child continues
    if (fork() > 0) exit(0);

    // 2. New session — detach from controlling terminal
    setsid();

    // 3. Fork again — guarantees we're not session leader
    //    (can't reacquire a controlling terminal)
    if (fork() > 0) exit(0);

    // 4. Change working directory to /
    chdir("/");

    // 5. Reset file creation mask
    umask(0);

    // 6. Redirect standard fds to /dev/null
    int devnull = open("/dev/null", O_RDWR);
    dup2(devnull, STDIN_FILENO);
    dup2(devnull, STDOUT_FILENO);
    dup2(devnull, STDERR_FILENO);
    close(devnull);

    // Now running as a proper daemon
    // Write PID file for management
    int pid_fd = open("/var/run/myapp.pid", O_WRONLY|O_CREAT|O_TRUNC, 0644);
    dprintf(pid_fd, "%d\n", getpid());
    close(pid_fd);
}
```

### systemd Service Unit

```ini
# /etc/systemd/system/myapp.service
[Unit]
Description=My C++ Application
After=network.target

[Service]
Type=simple
User=myapp
ExecStart=/usr/bin/myapp --config /etc/myapp/config.json
Restart=on-failure
RestartSec=5s
LimitNOFILE=65536        # max open files
LimitNPROC=4096          # max processes
MemoryLimit=512M         # memory limit (cgroup)
CPUQuota=200%            # use up to 2 CPU cores

# Hardening
PrivateTmp=true
NoNewPrivileges=true
ProtectSystem=strict
ProtectHome=true

[Install]
WantedBy=multi-user.target
```

```bash
systemctl start myapp
systemctl stop myapp
systemctl restart myapp
systemctl status myapp
systemctl enable myapp    # start on boot
journalctl -u myapp -f    # follow logs
```

---

## 13. Shell Scripting Essentials

```bash
#!/bin/bash
set -euo pipefail          # exit on error, unset vars, pipe failures

# Variables
NAME="world"
echo "Hello, $NAME"
echo "PID: $$"             # current shell PID
echo "Last exit: $?"       # last command exit code
echo "Args: $@"            # all arguments
echo "Arg count: $#"

# Conditionals
if [ -f "/etc/passwd" ]; then
    echo "file exists"
fi

if [[ "$VAR" == "value" ]]; then   # [[ ]] supports regex, no word splitting
    echo "match"
fi

# Loops
for i in {1..5}; do echo $i; done
for file in *.cpp; do echo "$file"; done
while read line; do echo "$line"; done < file.txt

# Functions
my_func() {
    local var="local to function"
    echo "arg1=$1, arg2=$2"
    return 0
}
my_func "hello" "world"

# Command substitution
files=$(ls *.cpp)
count=$(ls *.cpp | wc -l)

# Arrays
arr=(one two three)
echo "${arr[0]}"           # "one"
echo "${arr[@]}"           # all elements
echo "${#arr[@]}"          # length

# Process substitution
diff <(ls dir1/) <(ls dir2/)

# Heredoc
cat <<'EOF'
Literal text with $no expansion
EOF
```

---

## Mock Interview Questions

---

**[Mid]** What happens when you type `ls` in a shell and press Enter?

> 1. The shell **reads and parses** the input `ls` — recognizes it as a command with no arguments.
> 2. Shell calls **`fork()`** — creates a child process that is an exact copy of the shell.
> 3. In the child, shell calls **`execvp("ls", ["ls", nullptr])`** — replaces the child's process image with `/bin/ls` (found via `PATH` lookup). The child's PID is unchanged; its code, stack, and heap are replaced.
> 4. The kernel loads the `ls` ELF binary: maps `.text`, `.data`, `.bss` into the new address space; sets up the stack with `argc`, `argv`, `envp`; runs the dynamic linker to resolve shared library symbols.
> 5. `ls` runs — calls `opendir(".")`, reads directory entries via `getdents64`, `stat`s each entry, writes formatted output to `stdout` (fd 1) via `write()`.
> 6. `ls` calls `exit(0)` — kernel releases resources, converts child to a **zombie** briefly.
> 7. The parent shell receives **`SIGCHLD`**, calls **`waitpid()`** to reap the zombie, retrieves the exit status.
> 8. Shell prints its prompt and waits for the next command.

---

**[Mid]** What is the difference between a process and a thread at the kernel level?

> At the kernel level, both processes and threads are **tasks** — represented by `struct task_struct`. The difference is which resources they **share** with their creator.
>
> `fork()` calls `clone()` with minimal flags — creates a new `task_struct` with a **new address space, file descriptor table, and signal handlers** (all copied).
>
> `pthread_create()` calls `clone()` with `CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND` — creates a new `task_struct` that **shares** the address space, file descriptors, and signal handlers with the parent task.
>
> The scheduler sees threads and processes identically — both are schedulable units on the run queue. The practical difference is:
> - **Memory**: threads share virtual address space (no TLB flush on switch between threads of the same process). Process switch requires TLB flush.
> - **Communication**: threads communicate via shared memory directly. Processes need IPC.
> - **Fault isolation**: a crashing thread kills the entire process. A crashing process doesn't affect others.
> - **Creation cost**: thread `clone()` is ~10µs; process `fork()` is ~100µs (more state to copy/set up).

---

**[Mid]** What is a zombie process? How do you prevent them?

> A **zombie** is a process that has exited but whose `task_struct` (specifically the exit status) has not yet been collected by the parent via `wait()`. The zombie holds no resources — no memory, no CPU — but occupies a slot in the process table. Too many zombies (thousands) can exhaust the process table and prevent new processes from being created.
>
> **Prevention strategies**:
>
> 1. **Call `waitpid()` in the parent** after each child:
> ```cpp
> waitpid(child_pid, &status, 0);   // blocking wait
> waitpid(-1, &status, WNOHANG);    // non-blocking: reap any child that has exited
> ```
>
> 2. **`SIGCHLD` handler**: install a handler that calls `waitpid(-1, nullptr, WNOHANG)` in a loop to reap all available children:
> ```cpp
> signal(SIGCHLD, [](int){ while(waitpid(-1, nullptr, WNOHANG) > 0); });
> ```
>
> 3. **`signal(SIGCHLD, SIG_IGN)`**: explicitly ignoring SIGCHLD tells the kernel to auto-reap children without creating zombies. Note: this means you can't retrieve exit status.
>
> 4. **Double-fork**: the intermediate process exits immediately, making the grandchild an orphan adopted by `init`, which always reaps its children.

---

**[Senior]** Explain `epoll` and how it differs from `select`. Why is `epoll` O(1) per event?

> **`select`** takes three **bit masks** of fd sets (read, write, except). On each call, the kernel **scans all fds up to `nfds`** to check readiness, then the application scans the returned masks — O(nfds) per call. Limited to 1024 fds (FD_SETSIZE).
>
> **`epoll`** uses a three-call API:
> - `epoll_create1()` — creates a kernel-managed event object backed by a red-black tree of registered fds
> - `epoll_ctl(ADD/MOD/DEL)` — registers/modifies/removes interest in an fd — O(log n)
> - `epoll_wait()` — **blocks until events are ready**, returns only the ready fds in an array — O(events), not O(registered fds)
>
> **Why O(1)**:
> When a registered fd becomes ready (data arrives on a socket, a timer fires, etc.), the kernel's interrupt handler adds that fd's event to a **ready list** (a simple linked list). `epoll_wait` just copies this list to userspace. No scanning needed — the work is done at interrupt time, amortized across all events.
>
> ```
> select:    scan ALL fds on every call  → O(n) per call regardless of activity
> epoll:     ready list populated by IRQ → O(events) per call, regardless of total fds
> ```
>
> For 10,000 connected clients where 10 are active: `select` scans 10,000 fds every call. `epoll` returns exactly 10 events. At scale, this difference is the gap between serving 1,000 vs 100,000 connections per server.

---

**[Senior]** A production server process is consuming unexpectedly high memory. Walk through how you'd diagnose it.

> **Step 1 — Quantify**
> ```bash
> cat /proc/PID/status | grep -E "VmRSS|VmSize|VmPeak|VmSwap"
> # VmRSS = physical RAM in use
> # VmSize = virtual address space (includes mapped but not yet faulted pages)
> ```
>
> **Step 2 — Classify: leak vs bloat vs fragmentation**
> ```bash
> # Watch RSS over time
> watch -n 5 'cat /proc/PID/status | grep VmRSS'
> # Steadily growing → likely leak
> # Stable but high → one-time bloat or fragmentation
> ```
>
> **Step 3 — Inspect memory map**
> ```bash
> cat /proc/PID/maps    # or /proc/PID/smaps for detailed RSS per region
> # Look for: anonymous regions growing (heap), unexpected large mmap regions
> cat /proc/PID/smaps | grep -A 15 "heap"
> ```
>
> **Step 4 — Run with leak detectors (in staging)**
> ```bash
> # AddressSanitizer — catches leaks at exit
> g++ -fsanitize=address -g ./program
> ASAN_OPTIONS=detect_leaks=1 ./program
>
> # Valgrind — full leak report
> valgrind --leak-check=full --track-origins=yes ./program
>
> # Heap profiler (gperftools/tcmalloc)
> LD_PRELOAD=/usr/lib/libprofiler.so HEAPPROFILE=/tmp/heap ./program
> pprof --pdf ./program /tmp/heap.0001.heap > heap.pdf
> ```
>
> **Step 5 — Check allocator fragmentation**
> High RSS without a leak may be **heap fragmentation**: many small allocations and frees leaving holes. The allocator holds pages it cannot return to the OS. Solutions: use `jemalloc` or `tcmalloc` (better fragmentation handling), object pools, or `malloc_trim()` to force return of free pages.
>
> **Step 6 — Check for fd/mmap leaks**
> ```bash
> ls /proc/PID/fd | wc -l    # count open fds — should be stable
> cat /proc/PID/maps | wc -l  # count memory regions — growing mmap regions = mmap leak
> ```
