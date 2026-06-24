# Embedded Systems 03 — Communication Protocols

---

## 3.1 UART

Asynchronous serial. No shared clock — both sides agree on baud rate beforehand.

**Frame Format**

```
Idle  Start  D0  D1  D2  D3  D4  D5  D6  D7  Parity  Stop
 1     0     lsb                            msb  opt    1/2
```

- **Start bit**: always 0 (falling edge — synchronizes receiver)
- **Stop bit(s)**: always 1. 1, 1.5, or 2 stop bits
- **Parity**: none / odd / even (weak error detection only)

**Baud Rate Tolerance**

Receiver samples at midpoint of each bit. Total timing error budget ≈ ±2.5% of a bit period across the full frame. Crystal oscillators give <0.05% error; internal RC oscillators may give 1–3% — marginal.

**Flow Control**

- **Hardware (RTS/CTS)**: RTS (Ready to Send) deasserted when RX buffer almost full. CTS checked before TX.
- **Software (XON/XOFF)**: send special bytes 0x11 (XON) / 0x13 (XOFF) in-band. Cannot be used when binary data contains those values.

**RS-485 vs UART**

UART is single-ended (absolute voltage). RS-485 is differential (voltage difference between A and B lines):
- Immune to common-mode noise
- Up to 1200m cable length at low baud
- Multi-drop: up to 32 nodes on one bus
- Half-duplex (typically): driver enable (DE) pin controls bus direction

---

## 3.2 SPI (Serial Peripheral Interface)

Synchronous, full-duplex, master-slave. 4 wires: SCLK, MOSI, MISO, CS (one per slave).

**CPOL and CPHA**

| Mode | CPOL | CPHA | Clock idle | Sample edge     |
|------|------|------|------------|-----------------|
| 0    | 0    | 0    | Low        | Rising edge     |
| 1    | 0    | 1    | Low        | Falling edge    |
| 2    | 1    | 0    | High       | Falling edge    |
| 3    | 1    | 1    | High       | Rising edge     |

```
Mode 0 (CPOL=0, CPHA=0):
SCLK:  _┌─┐_┌─┐_┌─┐_┌─┐_
MOSI:  ──X─────X─────X─────X──   (data changes on falling, sampled on rising)
MISO:  ──X─────X─────X─────X──
CS:    ─────────────────────────
       └─ asserted (low) ──────┘
```

**Multi-Slave Wiring**

Each slave has its own CS line. Only one CS active at a time. MISO lines are tristated when CS deasserted — requires open-drain MISO or dedicated select.

**Maximum Throughput**

No protocol overhead — just clock and data. Limited by:
- SPI peripheral clock divider (e.g., PCLK/2 = 36 MHz on STM32F1)
- PCB trace capacitance and signal integrity
- Slave's maximum clock speed

---

## 3.3 I2C (Inter-Integrated Circuit)

Synchronous, half-duplex, multi-master capable. 2 wires: SCL (clock), SDA (data). Both open-drain with pull-ups.

**Transaction Structure**

```
S  ADDR[7:1] R/W  A   DATA[7:0]   A   ...   DATA[7:0]   N   P
│  slave address  │ACK│  byte 1  │ACK│      │ last byte │NAK│
Start                                                        Stop
```

- **S**: START (SDA falls while SCL high)
- **A/N**: ACK (slave pulls SDA low) / NACK (SDA stays high)
- **P**: STOP (SDA rises while SCL high)
- **RS**: Repeated START (no STOP between read/write phases)

**10-bit Addressing**

7-bit: 128 addresses, some reserved → 112 usable.  
10-bit: 1024 addresses. Two-byte address sequence, first byte starts with `11110b`.

**Clock Stretching**

Slave holds SCL low to pause master. Used when slave needs time to prepare data. Not all masters support it — check datasheet.

**I2C Speed Modes**

| Mode          | Speed   |
|---------------|---------|
| Standard      | 100 kHz |
| Fast          | 400 kHz |
| Fast-Plus     | 1 MHz   |
| High-speed    | 3.4 MHz |
| Ultra-fast    | 5 MHz   |

**Pull-up Resistor Sizing**

Too large → slow rise time (fails at high speed). Too small → too much current.

`R_pull = (VCC - VOL_min) / I_sink_max`

Typical: 4.7kΩ for 100kHz, 2.2kΩ for 400kHz, 1kΩ for 1MHz.

---

## 3.4 CAN Bus

Multi-master, differential bus (CAN_H, CAN_L). Designed for automotive/industrial noise environments where many ECUs need deterministic message priority without a central host.

**Physical Layer**

CAN uses differential signaling:

| Bus state  | Logic | CAN_H            | CAN_L            | Differential |
|------------|-------|------------------|------------------|--------------|
| Recessive  | 1     | ~2.5 V           | ~2.5 V           | ~0 V         |
| Dominant   | 0     | ~3.5 V           | ~1.5 V           | ~2 V         |

- **Dominant wins**: if any node drives dominant, the bus reads dominant.
- **Termination**: 120 ohm resistor at each physical end of the main bus, not at every node.
- **Topology**: linear bus with short stubs. Long star wiring causes reflections and arbitration errors.
- **Common mode tolerance**: differential receivers tolerate noise that appears on both wires.
- **Typical classic CAN bit rates**: 125 kbps, 250 kbps, 500 kbps, 1 Mbps. Higher speed means shorter bus length.

Approximate classic CAN length examples:

| Bit rate | Typical max bus length |
|----------|------------------------|
| 1 Mbps   | ~40 m                  |
| 500 kbps | ~100 m                 |
| 250 kbps | ~250 m                 |
| 125 kbps | ~500 m                 |

**Frame Types**

| Frame    | Purpose                                   |
|----------|-------------------------------------------|
| Data     | Transmit up to 8 bytes with 11/29-bit ID  |
| Remote   | Request data from another node            |
| Error    | Broadcast when error detected             |
| Overload | Request delay between frames              |

**Identifiers and Message Priority**

CAN is message-oriented, not address-oriented. The ID describes the meaning and priority of the message, not the destination node.

- **Standard frame**: 11-bit identifier.
- **Extended frame**: 29-bit identifier.
- **Lower numerical ID = higher priority** because arbitration compares bits from MSB to LSB and dominant 0 beats recessive 1.
- Hardware acceptance filters let a node receive only IDs it cares about, reducing CPU load.

Example:

| ID      | Meaning                     | Priority |
|---------|-----------------------------|----------|
| 0x080   | Brake/torque safety message | High     |
| 0x180   | Motor status                | Medium   |
| 0x500   | Diagnostic data             | Low      |

**Arbitration (CSMA/CD+AMP)**

All nodes transmit simultaneously. Dominant bit (0) wins over recessive (1). Node that loses arbitration backs off and retries. Lowest ID wins — higher priority.

```
Node A transmits ID: 0x100 = 0001 0000 0000b
Node B transmits ID: 0x0F0 = 0000 1111 0000b
                              ↑ B wins (dominant 0 vs A's recessive)
```

Arbitration is non-destructive: the winning frame continues without being corrupted. This is why CAN can provide deterministic priority under load.

**Classic CAN Data Frame Fields**

```
SOF  Identifier  RTR/IDE/r0  DLC  Data  CRC  ACK  EOF
 0   11/29 bits  control     0-8  byte  15b  slot 7 recessive bits
```

- **SOF**: Start of frame, dominant bit.
- **Identifier**: message priority and meaning.
- **RTR**: Remote Transmission Request. Dominant for data frame, recessive for remote frame.
- **IDE**: distinguishes standard vs extended ID.
- **DLC**: Data Length Code, 0 to 8 bytes for classic CAN.
- **CRC**: detects frame corruption.
- **ACK slot**: transmitter sends recessive; any receiver that got the frame correctly drives dominant.
- **EOF**: End of frame, fixed recessive bits.

**Bit Stuffing**

After 5 consecutive bits of the same polarity, the transmitter inserts one opposite-polarity stuff bit. Receivers remove it automatically. This guarantees enough edges for clock synchronization. A violation creates a stuff error.

**Bit Timing**

Each bit is divided into time quanta:

```
Sync Segment | Propagation Segment | Phase Segment 1 | Phase Segment 2
             ^ resynchronization adjusts sample point here
```

- **Sample point** is often configured around 75-87.5% of the bit time.
- **Propagation segment** covers cable delay, transceiver delay, and optocoupler delay if used.
- **SJW (Synchronization Jump Width)** limits how much the controller may resynchronize on an edge.
- All nodes on a bus must use compatible nominal bit timing, not just the same nominal bit rate.

**Error Handling**

CAN has sophisticated error confinement:
- **Bit error**: sent bit ≠ read-back bit
- **Stuff error**: 6 consecutive same-polarity bits (violates stuffing rule)
- **CRC error**: CRC mismatch
- **ACK error**: no ACK received
- **Form error**: fixed-format field violated

Each node has TX Error Counter (TEC) and RX Error Counter (REC). At TEC/REC > 127: **Error Passive** mode. At TEC > 255: **Bus Off** (node disconnects until 128×11 recessive bits detected).

Error states:

| State          | Counter condition              | Behavior |
|----------------|--------------------------------|----------|
| Error Active   | TEC < 128 and REC < 128        | Can transmit active error flags |
| Error Passive  | TEC >= 128 or REC >= 128       | Still communicates, but uses passive error flags |
| Bus Off        | TEC > 255                      | Stops transmitting until recovered by software/controller |

Common real-world causes:

- Missing or extra termination.
- Too-long stubs or star topology.
- Bit timing mismatch between nodes.
- Ground offset or weak transceiver supply.
- No other node present to ACK frames.
- CAN_H/CAN_L swapped.

**Bus Load**

CAN is deterministic, but high bus load increases latency for low-priority messages. Keep average bus utilization well below 100%; many automotive designs target <50-70% depending on safety margin.

Rough estimate:

`bus_load = total_frame_bits_per_second / bus_bit_rate`

A classic CAN frame with 8 data bytes often consumes roughly 110-130 bits on the wire after overhead and bit stuffing, so 1000 frames/s at 500 kbps is about 22-26% bus load.

**CAN FD (Flexible Data-rate)**

CAN FD keeps classic CAN arbitration but improves payload size and throughput.

- **Payload up to 64 bytes** instead of 8 bytes.
- **Dual bit rate**: arbitration phase uses the normal CAN bit rate; data phase can switch faster.
- **BRS (Bit Rate Switch)** bit enables the faster data phase.
- **FDF/EDL bit** marks the frame as CAN FD.
- **ESI (Error State Indicator)** reports whether the transmitter is error active or error passive.
- **Improved CRC**: 17-bit CRC for smaller CAN FD payloads, 21-bit CRC for larger payloads.
- **No remote frames** in CAN FD.

CAN FD frame concept:

```
Arbitration phase        Data phase at higher speed           Arbitration phase
SOF + ID + control  -->  payload + CRC at data bit rate  -->  ACK + EOF
```

Typical setup:

| Phase       | Example bit rate | Purpose |
|-------------|------------------|---------|
| Arbitration | 500 kbps         | All nodes arbitrate reliably across full bus length |
| Data        | 2 Mbps or 5 Mbps | Winner sends larger payload faster |

**Classic CAN vs CAN FD**

| Feature       | Classic CAN          | CAN FD |
|---------------|----------------------|--------|
| Max payload   | 8 bytes              | 64 bytes |
| Bit rate      | One bit rate         | Arbitration + optional faster data phase |
| CRC           | 15-bit               | 17-bit or 21-bit |
| Remote frame  | Supported            | Not supported |
| Compatibility | Classic controllers  | Requires CAN FD-capable controllers |

Important compatibility note: a classic CAN controller that sees a CAN FD frame may treat it as an error. Mixed networks must ensure FD frames are only used where all active nodes tolerate CAN FD, or isolate classic-only nodes.

**When to Use CAN FD**

Use CAN FD when:

- Firmware updates or diagnostics need larger packets.
- Sensor payloads exceed 8 bytes.
- You need lower bus load without changing the arbitration behavior.
- The hardware transceivers, controllers, and tools all support CAN FD.

Classic CAN is still enough when messages are small, timing is well understood, and compatibility with legacy ECUs/tools matters more than throughput.

---

## 3.5 USB (Universal Serial Bus)

**Architecture**

Host-centric: one host, multiple devices/hubs. Host initiates all transactions.

**Transfer Types**

| Type        | Guaranteed? | Use case                    |
|-------------|-------------|-----------------------------|
| Control     | Yes         | Enumeration, configuration  |
| Bulk        | No          | Mass storage, printers      |
| Interrupt   | Latency-bounded | HID (keyboard, mouse)  |
| Isochronous | Bandwidth   | Audio, video (no retry)     |

**Enumeration Sequence**

1. Device attached → host detects D+ or D- pull-up
2. Host issues RESET (SE0 for ≥10ms)
3. Host reads Device Descriptor (8 bytes minimum, address 0)
4. Host assigns address via SET_ADDRESS
5. Host reads full descriptors (configuration, interface, endpoint)
6. Host loads driver, SET_CONFIGURATION

**USB Speed Tiers**

| Speed        | Bit Rate  | Standard |
|--------------|-----------|----------|
| Low Speed    | 1.5 Mbps  | USB 1.0  |
| Full Speed   | 12 Mbps   | USB 1.1  |
| High Speed   | 480 Mbps  | USB 2.0  |
| SuperSpeed   | 5 Gbps    | USB 3.0  |
| SuperSpeed+  | 10/20 Gbps| USB 3.1/3.2|

---

## Interview Questions

### Easy

**Q: What are the four SPI signals and what does each carry?**

> **SCLK**: clock generated by master, synchronizes data. **MOSI** (Master Out Slave In): data from master to slave. **MISO** (Master In Slave Out): data from slave to master. **CS/NSS** (Chip Select, active-low): selects which slave is active. SPI is full-duplex: MOSI and MISO transfer simultaneously every clock edge.

**Q: In I2C, who generates the ACK? What does a NACK mean?**

> The **receiver** generates the ACK (pulls SDA low during the 9th clock pulse). When the master writes: each slave ACKs its own address + data bytes. When the master reads: the master ACKs each received byte except the last (sends NACK before STOP to tell the slave to stop transmitting). NACK means: address not recognized, slave busy, data not expected, or last byte in a read.

**Q: What is baud rate and how does it relate to bits per second?**

> Baud rate = number of symbol changes per second. For UART with one bit per symbol (binary signaling), baud rate = bit rate. At 115200 baud, 10 bits per frame (1 start + 8 data + 1 stop) → actual throughput = 115200 / 10 = 11,520 bytes/sec. For protocols with multi-level signaling (e.g., PAM4), baud rate < bit rate.

---

### Mid

**Q: Why does I2C use open-drain signals with pull-up resistors instead of push-pull like SPI?**

> Open-drain enables: (1) **Wired-AND arbitration** — any device can pull SDA low without fighting another driver (both pulling low = still low). (2) **Multi-master** — masters detect when another master is transmitting and back off. (3) **Clock stretching** — slaves can hold SCL low to pause the bus. With push-pull, two devices driving opposite values would short-circuit. The pull-up resistor determines the bus rise time and limits maximum speed.

**Q: Two CAN nodes simultaneously transmit IDs 0x200 and 0x1FF. Which wins and why?**

> **0x1FF wins**. CAN arbitration compares identifier bits from MSB to LSB while every transmitter monitors the bus. 0x200 = `010 0000 0000b`, 0x1FF = `001 1111 1111b`. The first differing bit is the second bit: 0x200 sends recessive 1, while 0x1FF sends dominant 0. The node sending recessive but reading dominant loses arbitration and retries later. Lower numerical ID = higher priority.

**Q: Why does CAN need termination, and where should the termination resistors be placed?**

> CAN is a high-speed differential bus, so the cable behaves like a transmission line. Without termination, signal edges reflect from cable ends and can create false bits or arbitration errors. Use **120 ohm termination at each physical end of the main bus**. Do not place 120 ohm resistors at every node; that overloads the drivers. Keep stubs short because long branches also create reflections.

**Q: What is the difference between a CAN node address and a CAN identifier?**

> CAN does not use node addresses like many master-slave protocols. The **identifier describes the message**, such as wheel speed, motor torque request, or diagnostic response. Any node may transmit a message ID, and any interested node may receive it through acceptance filters. The ID also defines priority: lower numerical ID wins arbitration.

**Q: What happens if only one CAN node is connected to the bus and it tries to transmit?**

> The transmitter sends the frame, but no other node drives the ACK slot dominant. The transmitter detects an **ACK error**, retries, and its transmit error counter increases. This is a common bench-test issue: even if the wiring is correct, a single-node CAN bus cannot successfully acknowledge its own frames in normal mode.

**Q: Explain SPI CPOL and CPHA. A sensor datasheet says "data captured on rising edge, clock idles low." Which SPI mode?**

> CPOL=0: clock idles low. CPOL=1: clock idles high. CPHA=0: data captured on first (leading) edge. CPHA=1: data captured on second (trailing) edge. "Idles low" → CPOL=0. "Captured on rising edge" with idle-low means rising is the first edge → CPHA=0. Answer: **Mode 0 (CPOL=0, CPHA=0)**.

---

### Hard

**Q: Your I2C bus hangs with SCL stuck low. What are the causes and how do you recover without a full system reset?**

> Causes: (1) Slave was mid-transaction during a reset and is holding SDA low waiting for more clocks. (2) Master crashed mid-transaction, leaving SCL driven low. (3) Bus lockup from electrical fault. Recovery without full reset: send **9 manual clock pulses on SCL** (bit-bang) to allow the slave to complete any in-progress byte and release SDA. Then generate a STOP condition (SDA low → high while SCL high). In code: disable the I2C peripheral, configure SCL/SDA as GPIO, clock 9 times, send STOP, re-enable the I2C peripheral. Most STM32 HAL implementations have `HAL_I2C_MspDeInit` + re-init for this.

**Q: Design a multi-drop RS-485 network for 32 nodes at 500 kbps over 100m cable. What termination, biasing, and timing considerations apply?**

> **Termination**: 120 Ω resistors at both cable ends (match characteristic impedance of twisted pair ~120 Ω) to prevent reflections. At 500 kbps, bit period = 2 µs; a 100m cable has ~1 µs one-way propagation — reflections must be damped within the bit period. **Bias resistors**: 560–1 kΩ pull-up on A and pull-down on B from a master node to define the idle (recessive) state when no transmitter is driving. Without bias, an unterminated bus floats and picks up noise. **Timing**: Driver enable (DE) must deassert at least one bit-time after the last stop bit to avoid truncating the last bit. Add propagation delay margin: 100m × 5 ns/m × 2 (round-trip) = 1 µs — within the 2 µs bit period, but tight. **Failsafe**: if all drivers are tri-state simultaneously, bias resistors must maintain a valid logic level so receivers don't see noise as valid data.

**Q: Your SPI slave requires a minimum 50 ns CS-to-SCLK setup time, but the MCU asserts CS and immediately starts the clock. At 36 MHz, each cycle is ~28 ns. How do you guarantee the timing?**

> Options in order of preference: (1) **Insert a software NOP delay** after asserting CS: at 72 MHz system clock, 4 NOPs ≈ 56 ns ≥ 50 ns. Use `__NOP()` intrinsic — not subject to compiler reordering if `volatile`. (2) **Use GPIO-controlled CS and add a DMA/timer-synchronized delay** — most robust but complex. (3) **Configure SPI NSS pulse management** if the MCU's SPI peripheral supports `NSS pulse` mode (inserts 1 cycle gap between CS and CLK). (4) **Reduce SPI clock to 18 MHz** — halve speed so each cycle is 56 ns, and the CS-to-CLK gap is naturally larger. Document the constraint clearly in the driver code with a comment citing the datasheet page.

**Q: How is CAN FD faster than classic CAN if arbitration still happens at the normal CAN bit rate?**

> CAN FD keeps arbitration at the nominal bit rate so all nodes can safely resolve priority across the full bus. After one node wins arbitration, the frame can switch to a faster **data phase** using the BRS bit. This improves throughput because the payload and CRC dominate large frames. Example: arbitration may run at 500 kbps while the data field runs at 2 Mbps or 5 Mbps.

**Q: Can classic CAN and CAN FD nodes share the same bus?**

> Only with care. Classic CAN controllers generally do not understand CAN FD frames and may flag them as errors. A mixed network is safe only if classic nodes are isolated from FD traffic, are configured as FD-tolerant where supported, or the network uses only classic CAN frames. If active classic-only nodes are on the same bus, do not transmit CAN FD frames.
