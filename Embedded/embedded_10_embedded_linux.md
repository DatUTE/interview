# Embedded Systems 10 — Embedded Linux

---

## 10.1 Boot Sequence

```
Power-on
  │
  ▼
ROM Bootloader (SoC-internal, immutable)
  │  Loads U-Boot SPL from eMMC/NOR/NAND
  ▼
SPL (Secondary Program Loader)
  │  Initializes DRAM, loads full U-Boot
  ▼
U-Boot
  │  Initializes peripherals, loads kernel + DTB
  │  Command: bootm / bootz / booti
  ▼
Linux Kernel
  │  Reads Device Tree (DTB) for hardware description
  │  Mounts root filesystem
  │  Runs /sbin/init (or systemd)
  ▼
Userspace (init → services → shell/app)
```

**Device Tree**

Hardware description file (DTS → DTB). Decouples hardware description from kernel code.

```dts
// Example: UART node in device tree
uart0: serial@40010000 {
    compatible = "arm,pl011";
    reg = <0x40010000 0x1000>;  // base addr, size
    interrupts = <0 5 4>;       // GIC interrupt
    clocks = <&apb_clk>;
    status = "okay";
    pinctrl-names = "default";
    pinctrl-0 = <&uart0_pins>;
};
```

---

## 10.2 Linux Kernel — What It Manages

After boot, the kernel is the core layer between hardware and userspace. It provides abstractions and policies so applications do not touch hardware directly.

```text
Userspace (apps, shell, systemd, your C/C++ program)
        │  syscalls: open, read, write, fork, mmap, socket, ...
        ▼
Linux Kernel
  ├── Process / Thread management
  ├── Virtual Memory (MMU, paging)
  ├── Virtual File System (VFS) + concrete filesystems
  ├── Scheduler (which runnable task runs next)
  ├── Network stack (sockets, TCP/UDP/IP)
  └── Device drivers, IRQ, DMA, timers, ...
        ▼
Hardware (CPU, RAM, flash, Ethernet, UART, GPIO, ...)
```

### Process

A **process** is a running program instance with its own resources:

- Virtual address space
- File descriptors (open files, sockets, pipes)
- Process ID (PID), parent PID (PPID)
- User/group IDs, credentials
- Signal disposition

Key syscalls:

| Syscall   | Purpose |
|-----------|---------|
| `fork()`  | Create a child process (copy-on-write address space) |
| `execve()`| Replace current process image with a new program |
| `exit()`  | Terminate process |
| `wait()`  | Parent waits for child exit status |

On embedded Linux, typical processes include **PID 1** (`systemd` or `BusyBox init`), network daemons, sensor/control apps, and shell scripts started at boot.

```bash
ps aux          # list processes
cat /proc/<pid>/status   # process details
cat /proc/<pid>/maps     # virtual memory map
```

### Thread

A **thread** is a unit of execution **inside** a process. Threads in the same process **share**:

- Virtual address space
- File descriptors
- Signal handlers (with caveats)

Each thread has its own:

- Stack
- Register context
- Thread ID (TID)

```text
Process
  ├── Thread 1 (main)
  ├── Thread 2 (sensor reader)
  └── Thread 3 (network sender)
       └── share heap, globals, open files
```

Userspace threads are usually created with **pthread** (`pthread_create`). The kernel sees them as tasks sharing one address space.

Kernel threads (**kthreads**) are kernel-side threads — e.g., workqueue workers, `ksoftirqd`, driver threads. They run in kernel space, not userspace.

**Process vs thread (interview one-liner):**

- **Process**: separate memory space; heavier to create.
- **Thread**: shared memory space; lighter for parallel work inside one app.

### Virtual Memory

The kernel gives each process its **own virtual address space**. The **MMU** maps virtual addresses to physical RAM (or swap, if configured — uncommon on small embedded targets).

```text
Virtual address (process view)  --MMU-->  Physical RAM / device memory
0x00000000 - 0xFFFFFFFF              page tables, TLB
```

Important concepts:

| Concept | Meaning |
|---------|---------|
| Page | Fixed-size chunk of memory (often 4 KB on ARM) |
| Page table | Kernel data structure mapping virtual → physical |
| Page fault | Access to unmapped or not-yet-present page; kernel loads or allocates |
| `mmap()` | Map file or anonymous memory into address space |
| Heap | Grows via `brk()`/`sbrk()` or `mmap()` |

Typical process layout:

```text
high address
  stack        (grows down)
  ...
  mmap regions (shared libs, mmapped files)
  heap         (grows up)
  BSS / data / text (program code and globals)
low address
```

Embedded notes:

- RAM is limited — watch **RSS** (resident set size) and avoid memory leaks in long-running daemons.
- **OOM killer** terminates processes when the system runs out of memory.
- **DMA buffers** may need physically contiguous or cache-coherent memory — userspace `malloc` is not always suitable; drivers use `dma_alloc_coherent()` or `dma_map_single()`.
- Read-only rootfs (SquashFS) saves RAM; writable data goes to `/tmp`, overlay, or UBIFS partition.

### File System

The **VFS (Virtual File System)** provides a uniform interface (`open`, `read`, `write`, `ioctl`) over many backend filesystems.

```text
App: open("/etc/config", O_RDONLY)
  → VFS
    → ext4 / squashfs / ubifs / ...
      → block device driver (eMMC, NAND, NOR)
```

Common filesystems on embedded Linux:

| Filesystem | Typical use |
|------------|-------------|
| **SquashFS** | Compressed read-only rootfs |
| **ext4** | Read/write on eMMC/SD |
| **UBIFS** | Raw NAND with wear leveling |
| **JFFS2** | Older NOR/NAND |
| **tmpfs** | RAM-backed `/tmp`, `/run` |
| **procfs** | `/proc` — process and kernel info |
| **sysfs** | `/sys` — devices, drivers, tunables |
| **devtmpfs** | `/dev` — device nodes |

Key objects:

- **inode**: metadata for a file (size, permissions, timestamps).
- **dentry**: directory entry cache (name → inode).
- **mount point**: attaches a filesystem at a path (e.g., `/` or `/data`).

Embedded pattern: **read-only root** + **writable `/data` partition** for logs, config, and firmware updates.

### Scheduler

The **scheduler** decides which runnable task (process or thread) runs on which CPU core and for how long.

Common scheduling policies:

| Policy | Use |
|--------|-----|
| `SCHED_OTHER` (CFS) | Normal time-shared tasks — default for most apps |
| `SCHED_FIFO` | Real-time, fixed priority, runs until it blocks or is preempted by higher RT priority |
| `SCHED_RR` | Real-time with time quantum |
| `SCHED_IDLE` | Lowest priority, only when CPU is idle |

Core ideas:

- **Context switch**: save registers/stack of outgoing task, restore incoming task.
- **Preemption**: higher-priority or time-sliced task can interrupt lower-priority running task (depends on config and policy).
- **Runqueue**: per-CPU queue of runnable tasks.

Embedded relevance:

- Control loops or media pipelines may need **RT priorities** (`sched_setscheduler`, `chrt`).
- Pin critical threads to dedicated cores: `taskset`, kernel cmdline `isolcpus=`.
- Long non-preemptible kernel sections or IRQ storms cause **latency jitter** — see PREEMPT_RT (Section 10.4).

```bash
chrt -f 80 ./control_loop    # SCHED_FIFO priority 80
taskset -c 2 ./sensor_app     # pin to CPU 2
```

### Network Stack

The kernel **network stack** implements protocols and exposes the **socket API** to userspace.

```text
App: socket() → connect()/send()/recv()
  → TCP / UDP / IP
    → routing, netfilter (optional)
      → netdev (e.g., eth0)
        → Ethernet MAC/driver
```

Layers (simplified):

| Layer | Examples |
|-------|----------|
| Application | HTTP client, MQTT, custom TCP server |
| Socket API | `socket`, `bind`, `listen`, `accept`, `send`, `recv` |
| Transport | TCP, UDP |
| Network | IPv4, IPv6, ICMP, routing table |
| Link | Ethernet, Wi-Fi, CAN (via **SocketCAN**) |

Embedded notes:

- Static IP or DHCP via `systemd-networkd`, `udhcpc`, or `/etc/network/interfaces`.
- Firewall: `iptables` / `nftables` (if enabled in kernel config).
- **SocketCAN** exposes CAN as `can0` — apps use `socket(PF_CAN, ...)` instead of raw register access.
- Debug: `ip addr`, `ip route`, `ss -tulpn`, `tcpdump`, `ping`.

```bash
ip link show
ip addr add 192.168.1.10/24 dev eth0
ping 192.168.1.1
```

### How They Work Together (Example)

A sensor gateway app on embedded Linux:

1. **Process** starts via `systemd` unit at boot.
2. App creates **threads** — one reads I2C sensor, one sends TCP data.
3. **Virtual memory** holds code, heap buffers, and `mmap` of config file.
4. Config is read through the **VFS** from `/etc/sensor.conf` on UBIFS.
5. **Scheduler** runs RT reader thread at higher priority than logger thread.
6. **Network stack** sends JSON over TCP socket to a cloud server.

---

## 10.3 Linux Device Drivers

**Driver Model**

```c
// Minimal platform driver skeleton
#include <linux/module.h>
#include <linux/platform_device.h>

static int my_probe(struct platform_device *pdev) {
    struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    void __iomem *base = devm_ioremap_resource(&pdev->dev, res);
    if (IS_ERR(base)) return PTR_ERR(base);

    int irq = platform_get_irq(pdev, 0);
    int ret = devm_request_irq(&pdev->dev, irq, my_irq_handler,
                               IRQF_SHARED, "my_driver", pdev);
    return ret;
}

static int my_remove(struct platform_device *pdev) {
    return 0;  // devm_ resources freed automatically
}

static const struct of_device_id my_of_ids[] = {
    { .compatible = "myvendor,mydevice" },
    { }
};
MODULE_DEVICE_TABLE(of, my_of_ids);

static struct platform_driver my_driver = {
    .probe  = my_probe,
    .remove = my_remove,
    .driver = { .name = "my_driver", .of_match_table = my_of_ids },
};
module_platform_driver(my_driver);
MODULE_LICENSE("GPL");
```

**Character Device**

Exposes device as file in `/dev`. User reads/writes via `open`, `read`, `write`, `ioctl`.

```c
// file_operations structure
static struct file_operations my_fops = {
    .owner          = THIS_MODULE,
    .open           = my_open,
    .release        = my_release,
    .read           = my_read,
    .write          = my_write,
    .unlocked_ioctl = my_ioctl,
    .mmap           = my_mmap,
};
```

**Interrupt Handling: Top vs Bottom Half**

```
Top half (ISR):   Runs in interrupt context
  - Must be fast, cannot sleep
  - Acknowledge hardware interrupt
  - Schedule bottom half

Bottom half: Runs in process context (can sleep)
  - Tasklet:   Runs in softirq context, cannot sleep
  - Workqueue: Runs in kernel thread, CAN sleep, most flexible
  - Threaded IRQ: devm_request_threaded_irq() — runs in dedicated kthread
```

```c
// Threaded IRQ — most modern approach
static irqreturn_t my_hard_isr(int irq, void *dev) {
    // quick: read status, ack interrupt
    return IRQ_WAKE_THREAD;  // schedule thread
}

static irqreturn_t my_thread_isr(int irq, void *dev) {
    // slow processing, can sleep
    return IRQ_HANDLED;
}

devm_request_threaded_irq(&pdev->dev, irq,
    my_hard_isr, my_thread_isr,
    IRQF_SHARED, "my_driver", pdev);
```

**Kernel Memory Allocation**

| Function               | Context | Can sleep | Physically contiguous |
|------------------------|---------|-----------|----------------------|
| `kmalloc()`            | Any     | No (GFP_ATOMIC) / Yes (GFP_KERNEL) | Yes |
| `vmalloc()`            | Process | Yes       | No (virtually contiguous) |
| `kzalloc()`            | Any     | Depends   | Yes (zeroed)         |
| `dma_alloc_coherent()` | Any     | Yes       | Yes + DMA-safe       |
| `get_free_pages()`     | Any     | Depends   | Yes (page-aligned)   |

---

## 10.4 PREEMPT_RT Linux

Standard Linux is not hard real-time. `PREEMPT_RT` patch makes it nearly so:
- Converts spinlocks to sleeping locks (mutex)
- Makes interrupt handlers preemptible (threaded by default)
- Eliminates long non-preemptible sections

**Setting Process Priority in RT**

```c
#include <sched.h>
struct sched_param sp = { .sched_priority = 80 };  // 1=lowest, 99=highest
sched_setscheduler(0, SCHED_FIFO, &sp);  // SCHED_FIFO: no time-slicing

// Lock memory to prevent page faults (real-time requirement)
mlockall(MCL_CURRENT | MCL_FUTURE);
```

**CPU Isolation**

Dedicate CPU cores for RT tasks (via `isolcpus=2,3` kernel cmdline + `taskset`).

**Key Filesystem Interfaces**

| Path         | Purpose                                        |
|--------------|------------------------------------------------|
| `/dev`       | Device nodes (character, block)                |
| `/proc`      | Process info, kernel tunables (`/proc/sys/...`)|
| `/sys`       | Device attributes, driver parameters (sysfs)   |
| `/dev/mem`   | Direct physical memory access (dangerous)      |
| `/dev/uio*`  | Userspace I/O — simple driver without kernel module |

---

## Interview Questions

### Easy

**Q: What are the main things the Linux kernel manages?**

> The kernel manages **processes and threads** (lifecycle, scheduling, signals), **virtual memory** (MMU page tables, `mmap`, page faults), the **file system** (VFS over ext4/SquashFS/UBIFS/proc/sysfs), the **scheduler** (which task runs on which CPU, CFS vs RT policies), and the **network stack** (sockets, TCP/UDP/IP, netdev drivers). It also handles device drivers, interrupts, and timers. Userspace talks to all of these through **system calls**.

**Q: What is a device tree and why is it used in embedded Linux?**

> A device tree (DT) is a data structure that describes hardware to the kernel — base addresses, IRQ numbers, clock sources, pin assignments — in a hardware-independent text format (DTS), compiled to a binary blob (DTB). It decouples hardware description from kernel code: the same kernel binary can run on different boards by loading a different DTB. Before DT, board-specific code was hardcoded in the kernel (arch/arm/mach-*), making the kernel difficult to maintain across hundreds of boards.

**Q: What is the difference between a loadable kernel module (`insmod`) and a built-in driver (`y` in menuconfig)?**

> **Module** (`m`): compiled as a `.ko` file, loaded at runtime with `insmod`/`modprobe`, unloadable with `rmmod`. Allows adding/removing drivers without rebooting. Larger initramfs or rootfs, slightly slower probe. **Built-in** (`y`): compiled directly into the `vmlinuz` image, always present, probed during boot. Smaller rootfs, no load latency, but cannot be removed. Use modules for optional hardware (USB devices, development). Use built-in for critical boot devices (eMMC, RTC, Ethernet for NFS root).

**Q: What does `devm_request_irq` do differently from `request_irq`?**

> `devm_request_irq` is a **managed resource** wrapper. It registers the IRQ handler AND registers a cleanup action — when the device is removed (driver `remove()` is called, or `probe()` fails partway through), the kernel automatically calls `free_irq` for you. With plain `request_irq`, you must manually call `free_irq` in `remove()` and in every error path in `probe()`. `devm_` wrappers (`devm_ioremap`, `devm_kmalloc`, etc.) are preferred in modern drivers to avoid resource leaks.

---

### Mid

**Q: What is the difference between a process and a thread in Linux?**

> A **process** is an independent program instance with its own virtual address space, file descriptors, and PID. A **thread** is an execution unit inside a process — threads share the same address space and file descriptors but have separate stacks and TIDs. `fork()` creates a new process; `pthread_create()` creates a thread within the current process. For embedded apps, use threads when tasks share data and need concurrent I/O; use separate processes for isolation (crash in one does not kill the other).

**Q: Explain virtual memory on embedded Linux. Why does each process think it has its own memory?**

> The **MMU** maps each process's virtual addresses to physical RAM using **page tables**. This gives isolation (one process cannot read another's memory) and a uniform address layout (code, heap, stack, mmap regions). On access to an unmapped page, a **page fault** triggers the kernel to allocate or load the page. Embedded systems often have no swap — when RAM is exhausted, the **OOM killer** terminates a process. DMA and driver code must respect cache coherency and physical contiguity, which userspace `malloc` does not guarantee.

**Q: What is the difference between a tasklet, workqueue, and threaded IRQ in Linux? When would you use each?**

> **Tasklet**: runs in softirq context (interrupt context), cannot sleep, runs on the same CPU that scheduled it. Use for very short deferred work that doesn't need to sleep. Being deprecated in newer kernels. **Workqueue**: runs in a kernel thread (process context), CAN sleep, can be scheduled to any CPU. Use when deferred work needs to call sleeping APIs (`msleep`, `mutex_lock`, I2C transfers). **Threaded IRQ** (`request_threaded_irq`): the hard ISR runs in interrupt context (acks hardware, returns `IRQ_WAKE_THREAD`), the thread ISR runs in a dedicated `kthread` (can sleep). Best modern approach — keeps hard ISR minimal and moves processing to preemptible thread context.

**Q: What is the difference between `kmalloc` and `vmalloc`? Which should you use for a DMA buffer?**

> `kmalloc`: allocates physically contiguous memory (guaranteed). Uses the slab allocator. Cannot sleep if called with `GFP_ATOMIC`. Fast. Limited to `KMALLOC_MAX_SIZE` (typically 4MB). **`vmalloc`**: allocates virtually contiguous but physically scattered memory. Can allocate large buffers (hundreds of MB). Slower, requires TLB entries, not physically contiguous. For DMA: use `dma_alloc_coherent()` — it allocates physically contiguous, cache-coherent memory and returns both a virtual address (for CPU access) and a DMA address (for the device). `kmalloc` memory may work for DMA on some platforms but is not guaranteed to be cache-coherent.

**Q: Your driver's `probe()` returns `-ENODEV`. What does it mean and how do you debug it?**

> `-ENODEV` means "no such device" — the driver ran `probe()` but decided the hardware isn't present or isn't the right variant. Common causes: (1) **Device tree `compatible` mismatch** — the DT node's `compatible` string doesn't match the driver's `of_match_table`. Check with `dmesg | grep "of_device_id"`. (2) **Missing device tree node** — the node exists but `status = "disabled"` prevents probing. (3) **Register read failure** — driver reads a chip ID register but gets 0xFFFF (bus error or wrong address). (4) **Clock/power not enabled** — driver probes before its power domain is ready. Debug: add `pr_err` at the start of `probe()`, check `dmesg`, verify the DT node with `dtc -I fs /proc/device-tree`.

---

### Hard

**Q: Describe the full flow for a DMA transfer in an embedded Linux driver: channel request, setup, completion, and cache coherency.**

> 1. **Request channel**: `chan = dma_request_chan(dev, "tx")` — matches the `dma-names` property in DT. 2. **Allocate coherent buffer**: `buf = dma_alloc_coherent(dev, size, &dma_addr, GFP_KERNEL)` — physically contiguous, cache-coherent, returns CPU virtual address and DMA bus address. 3. **Prepare descriptor**: `desc = dmaengine_prep_slave_single(chan, dma_addr, size, DMA_MEM_TO_DEV, DMA_PREP_INTERRUPT)`. 4. **Set callback**: `desc->callback = tx_done; desc->callback_param = priv`. 5. **Submit and fire**: `cookie = dmaengine_submit(desc); dma_async_issue_pending(chan)`. 6. **Completion callback** runs in tasklet/softirq context — signal a `completion` or wake a task. 7. Cache coherency: with `dma_alloc_coherent`, the kernel marks the buffer as non-cacheable — no explicit cache maintenance needed. With `dma_map_single`, you must call `dma_sync_single_for_cpu` before reading the buffer after DMA fills it.

**Q: Your RT process on PREEMPT_RT occasionally misses deadlines. How do you systematically diagnose whether the cause is priority inversion, IRQ latency, or something else?**

> Step 1: **Measure latency** with `cyclictest -p 99 -t1 -n` — shows max scheduling latency. If > expected, the RT infrastructure is the problem. Step 2: **Trace with `ftrace`**: enable `function_graph` + `irqsoff` tracer — shows what's holding interrupts disabled and for how long. Step 3: **Check for priority inversion**: use `chrt`/`schedtool` to verify all threads in the call chain are at the correct priorities. Check if any non-RT thread holds a lock that the RT thread needs (`/proc/PID/wchan`). Step 4: **Check IRQ affinity**: if the RT process and a high-rate IRQ share the same CPU core, the IRQ steals time. Use `taskset` to pin the RT process to an isolated core (`isolcpus=3`). Step 5: **Lock memory**: if not already done, `mlockall(MCL_CURRENT | MCL_FUTURE)` — page faults during execution cause unbounded latency.

**Q: Describe the boot sequence from U-Boot's `booti` command to the first userspace process executing.**

> 1. **`booti`** loads the kernel Image (ARM64 uncompressed), DTB, and optional initramfs into RAM and calls `kernel_entry(0, machine_id, atags/dtb)`. 2. **Kernel decompresses** itself (if compressed), sets up early page tables, enables MMU. 3. **`start_kernel()`**: initializes memory (buddy allocator, slab), IRQ subsystem, timers, scheduler. 4. **Device tree parsing** (`of_unflatten_device_tree`): the DT blob is parsed into a live tree of `device_node` structs. 5. **`do_initcalls()`**: runs all `module_init`/`subsys_initcall` functions — platform bus probes, calls `probe()` for each driver whose `compatible` matches a DT node. 6. **Mount root filesystem**: if initramfs, extract and run `/init`. Otherwise, mount the rootfs from the kernel cmdline (`root=/dev/mmcblk0p2`). 7. **`/sbin/init`** (PID 1) starts — systemd or BusyBox init reads `/etc/inittab` or unit files and spawns services. First userspace PID=1 executes at this point.
