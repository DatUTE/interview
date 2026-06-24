# Computer Architecture

## 1. Why Architecture Matters

Computer architecture explains why two pieces of code with the same Big-O can have very different performance.

Important factors:

- CPU cache.
- Branch prediction.
- Memory bandwidth.
- Instruction pipeline.
- SIMD/vectorization.
- NUMA.

---

## 2. Memory Hierarchy

Typical hierarchy:

```text
CPU registers
L1 cache
L2 cache
L3 cache
RAM
SSD / disk
Network storage
```

General rule:

> The farther data is from the CPU, the slower it is to access.

Implication:

- Contiguous arrays are usually faster than pointer-heavy structures.
- Random memory access hurts cache locality.
- Large working sets can exceed cache capacity.

---

## 3. CPU Cache

Cache stores recently used memory near the CPU.

Key ideas:

- Cache line: common transfer unit, often 64 bytes.
- Spatial locality: nearby data is likely to be used.
- Temporal locality: recently used data is likely to be reused.
- Cache miss: CPU waits for lower memory level.

Production example:

> A `std::vector<int>` scan can beat a linked list scan because vector elements are contiguous and prefetch-friendly.

---

## 4. Branch Prediction

Modern CPUs guess branch direction to keep the pipeline full.

Predictable branches are fast.

Unpredictable branches cause pipeline flushes.

Example:

```text
if (value > threshold) ...
```

If the branch outcome is random, prediction is poor.

---

## 5. False Sharing

False sharing happens when two threads update different variables on the same cache line.

Even though variables are logically separate, the cache coherence protocol treats the whole cache line as shared.

Symptom:

- High CPU usage.
- Poor scaling with more threads.
- Performance improves when padding/alignment separates variables.

---

## 6. SIMD

SIMD means Single Instruction, Multiple Data.

It lets one instruction operate on multiple values.

Use cases:

- Image processing.
- Audio/video.
- Numeric loops.
- Parsing and scanning.

Compilers may auto-vectorize simple loops, but data layout and aliasing can prevent it.

---

## 7. NUMA

NUMA means Non-Uniform Memory Access.

On multi-socket systems:

- Local memory access is faster.
- Remote memory access is slower.

Production implication:

> Thread placement and memory allocation policy can affect latency-sensitive services.

---

## Interview Questions

- Why can an array be faster than a linked list?
- What is a cache line?
- What is false sharing?
- How does branch prediction affect performance?
- What is SIMD?
- What is NUMA and why does it matter?
