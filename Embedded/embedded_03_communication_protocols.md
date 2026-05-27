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

Multi-master, differential bus (CAN_H, CAN_L). Designed for automotive/industrial noise environments.

**Frame Types**

| Frame    | Purpose                                   |
|----------|-------------------------------------------|
| Data     | Transmit up to 8 bytes with 11/29-bit ID  |
| Remote   | Request data from another node            |
| Error    | Broadcast when error detected             |
| Overload | Request delay between frames              |

**Arbitration (CSMA/CD+AMP)**

All nodes transmit simultaneously. Dominant bit (0) wins over recessive (1). Node that loses arbitration backs off and retries. Lowest ID wins — higher priority.

```
Node A transmits ID: 0x100 = 0001 0000 0000b
Node B transmits ID: 0x0F0 = 0000 1111 0000b
                              ↑ B wins (dominant 0 vs A's recessive)
```

**Error Handling**

CAN has sophisticated error confinement:
- **Bit error**: sent bit ≠ read-back bit
- **Stuff error**: 6 consecutive same-polarity bits (violates stuffing rule)
- **CRC error**: CRC mismatch
- **ACK error**: no ACK received
- **Form error**: fixed-format field violated

Each node has TX Error Counter (TEC) and RX Error Counter (REC). At TEC/REC > 127: **Error Passive** mode. At TEC > 255: **Bus Off** (node disconnects until 128×11 recessive bits detected).

**CAN FD (Flexible Data-rate)**

- Data phase can switch to higher bit rate (up to 8 Mbps)
- Payload up to 64 bytes (vs 8 bytes classic CAN)
- Separate bit timing for arbitration and data phase

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

> **0x1FF wins**. CAN arbitration: both nodes transmit simultaneously, monitoring the bus. At each bit, if a node sends recessive (1) but reads dominant (0) on the bus, it has lost arbitration and stops. 0x200 = `0010 0000 0000b`, 0x1FF = `0001 1111 1111b`. At bit 9 (0-indexed from MSB): 0x200 sends 1 (recessive), 0x1FF sends 1. At bit 10: 0x200 sends 0 (dominant), 0x1FF sends 0. Actually compare bit by bit from MSB: first differing bit is bit 9 where 0x200 = `1` (recessive) and 0x1FF = `0` (dominant) — **0x1FF wins** because it has a dominant (0) where 0x200 has a recessive (1). Lower numerical ID = higher priority.

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
