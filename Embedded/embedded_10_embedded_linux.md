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

## 10.2 Linux Device Drivers

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

## 10.3 PREEMPT_RT Linux

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

**Q: What is a device tree and why is it used in embedded Linux?**

> A device tree (DT) is a data structure that describes hardware to the kernel — base addresses, IRQ numbers, clock sources, pin assignments — in a hardware-independent text format (DTS), compiled to a binary blob (DTB). It decouples hardware description from kernel code: the same kernel binary can run on different boards by loading a different DTB. Before DT, board-specific code was hardcoded in the kernel (arch/arm/mach-*), making the kernel difficult to maintain across hundreds of boards.

**Q: What is the difference between a loadable kernel module (`insmod`) and a built-in driver (`y` in menuconfig)?**

> **Module** (`m`): compiled as a `.ko` file, loaded at runtime with `insmod`/`modprobe`, unloadable with `rmmod`. Allows adding/removing drivers without rebooting. Larger initramfs or rootfs, slightly slower probe. **Built-in** (`y`): compiled directly into the `vmlinuz` image, always present, probed during boot. Smaller rootfs, no load latency, but cannot be removed. Use modules for optional hardware (USB devices, development). Use built-in for critical boot devices (eMMC, RTC, Ethernet for NFS root).

**Q: What does `devm_request_irq` do differently from `request_irq`?**

> `devm_request_irq` is a **managed resource** wrapper. It registers the IRQ handler AND registers a cleanup action — when the device is removed (driver `remove()` is called, or `probe()` fails partway through), the kernel automatically calls `free_irq` for you. With plain `request_irq`, you must manually call `free_irq` in `remove()` and in every error path in `probe()`. `devm_` wrappers (`devm_ioremap`, `devm_kmalloc`, etc.) are preferred in modern drivers to avoid resource leaks.

---

### Mid

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
