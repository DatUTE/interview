# Embedded Systems 02 — Peripheral Fundamentals

---

## 2.1 GPIO (General Purpose I/O)

**Output Modes**

- **Push-pull**: driver actively drives high (VCC) and low (GND). Most common. Cannot be wired-OR.
- **Open-drain**: only pulls low; external pull-up resistor drives high. Used for I2C, wired-OR logic.

```
Push-pull:          Open-drain:
VCC                 VCC
 |                   |
[P-FET] ← 1        [R] ← pull-up (e.g. 4.7kΩ)
 |                   |
PIN ─────           PIN ─────
 |                   |
[N-FET] ← 0        [N-FET] ← 0
 |                   |
GND                 GND
```

**Pull-up / Pull-down Resistors**

Float state on an undriven input causes undefined behavior. Internal pull resistors (~40kΩ typical) prevent this.

```c
// STM32 HAL example
GPIO_InitTypeDef cfg = {
    .Pin   = GPIO_PIN_0,
    .Mode  = GPIO_MODE_INPUT,
    .Pull  = GPIO_PULLUP,   // or GPIO_PULLDOWN, GPIO_NOPULL
};
HAL_GPIO_Init(GPIOA, &cfg);
```

**Slew Rate**

Controls how fast the output transitions. Slow slew rate = less EMI, more signal integrity at low frequencies. Fast slew rate = needed for high-frequency signals (SPI CLK, USB).

**Alternate Function Multiplexing**

Physical pins are shared between GPIO and peripheral functions (UART TX, SPI MOSI, etc.). Configured via AF register:

```c
// PA9 = USART1_TX on STM32
GPIO_InitTypeDef cfg = {
    .Pin       = GPIO_PIN_9,
    .Mode      = GPIO_MODE_AF_PP,
    .Alternate = GPIO_AF7_USART1,
};
```

---

## 2.2 Interrupts & Exceptions

**ARM Cortex-M Exception Model**

Exceptions are numbered. Interrupts (IRQs) are a subset:

| Number | Exception       | Priority          |
|--------|-----------------|-------------------|
| 1      | Reset           | -3 (highest fixed)|
| 2      | NMI             | -2 (fixed)        |
| 3      | HardFault       | -1 (fixed)        |
| 4      | MemManage       | Configurable      |
| 5      | BusFault        | Configurable      |
| 6      | UsageFault      | Configurable      |
| 11     | SVCall          | Configurable      |
| 14     | PendSV          | Configurable      |
| 15     | SysTick         | Configurable      |
| 16+    | IRQ0, IRQ1, ... | Configurable      |

**NVIC (Nested Vectored Interrupt Controller)**

- **Priority**: lower numerical value = higher priority
- **Preemption priority**: can this IRQ interrupt a running ISR?
- **Subpriority**: tiebreaker between same preemption level
- Priority grouping controls how bits are split (PRIGROUP field in AIRCR)

```c
// Set UART1 IRQ: preemption priority 2, subpriority 1
HAL_NVIC_SetPriority(USART1_IRQn, 2, 1);
HAL_NVIC_EnableIRQ(USART1_IRQn);
```

**ISR Best Practices**

```c
// BAD: blocking inside ISR
void USART1_IRQHandler(void) {
    while (!tx_buffer_empty()) { }  // NEVER — blocks everything
    HAL_Delay(10);                  // NEVER — uses SysTick
}

// GOOD: set flag, process in main loop
volatile bool uart_rx_ready = false;
volatile uint8_t uart_rx_byte;

void USART1_IRQHandler(void) {
    if (USART1->SR & USART_SR_RXNE) {
        uart_rx_byte  = (uint8_t)USART1->DR;
        uart_rx_ready = true;
    }
}

// main loop
while (1) {
    if (uart_rx_ready) {
        uart_rx_ready = false;
        process(uart_rx_byte);
    }
}
```

**Interrupt Latency**

Time from interrupt assertion to first instruction of ISR. On Cortex-M:
- Minimum: 12 cycles (register stacking: xPSR, PC, LR, R12, R3, R2, R1, R0)
- Plus any pipeline flush, priority arbitration

**Edge vs Level Triggered**

- **Edge**: fires on rising or falling transition — can miss events if not serviced fast
- **Level**: fires as long as signal is asserted — re-enters ISR until cleared (must clear source in ISR)

---

## 2.3 Timers & Counters

**Timer Modes**

```
                    Up-counting (edge-aligned PWM):
CNT:  0──────────ARR──0──────────ARR
CCR:       ┌─────┘                   ← compare match → output toggles
Output: ____████████_________________

                    Center-aligned PWM:
CNT:  0────ARR────0────ARR
CCR:     ┌──┘└──┐
Output: __████████__████████__
```

- **ARR (Auto-Reload Register)**: period = (ARR+1) / timer_clk
- **CCR (Capture/Compare Register)**: duty = CCR / (ARR+1)
- **PSC (Prescaler)**: divides input clock before counter

**Frequency Calculation**

```c
// Timer clock = 72 MHz, desired PWM = 50 Hz, 1000-step resolution
// ARR = 1000 - 1 = 999
// PSC = 72MHz / (50Hz * 1000) - 1 = 1439
TIM3->PSC = 1439;
TIM3->ARR = 999;
TIM3->CCR1 = 500;  // 50% duty cycle
```

**Input Capture**

Captures CNT value when an edge is detected on input pin. Used for measuring frequency or pulse width.

```c
// Measure period between two rising edges
uint32_t t1, t2, period_ticks;

void TIM2_IRQHandler(void) {
    static uint8_t edge_count = 0;
    if (TIM2->SR & TIM_SR_CC1IF) {
        if (edge_count++ == 0) t1 = TIM2->CCR1;
        else { t2 = TIM2->CCR1; period_ticks = t2 - t1; edge_count = 0; }
    }
}
```

---

## 2.4 ADC / DAC

ADC converts an analog voltage into a digital number. DAC converts a digital number into an analog voltage.

```text
Sensor voltage ──> ADC ──> MCU software
MCU software  ──> DAC ──> analog voltage/current output
```

**ADC Applications**

ADC is used when the MCU needs to measure a real-world analog signal.

Common examples:

- **Sensor measurement**: temperature sensor, pressure sensor, light sensor, gas sensor, potentiometer.
- **Battery monitoring**: read battery voltage through a resistor divider.
- **Current sensing**: measure shunt resistor or current-sense amplifier output for motor/load protection.
- **Audio input**: sample microphone or line-in signal.
- **Motor control feedback**: read phase current, DC bus voltage, throttle position, or analog Hall sensor.
- **Touch/keypad input**: detect multiple buttons using one ADC channel and different resistor values.
- **Diagnostics**: monitor supply rails, board temperature, analog fault pins, or external sensor health.

Example: battery voltage divider.

```text
Battery ── R1 ─┬── ADC_IN
               |
              R2
               |
              GND

Vadc = Vbattery * R2 / (R1 + R2)
```

If `R1 = 100k`, `R2 = 33k`, and ADC reads `1.0V`:

```text
Vbattery = Vadc * (R1 + R2) / R2
         = 1.0 * 133k / 33k
         ≈ 4.03V
```

**DAC Applications**

DAC is used when the MCU needs to generate an analog output.

Common examples:

- **Audio output**: generate simple tones, voice prompts, or waveform playback.
- **Analog reference generation**: provide adjustable reference voltage for comparator, sensor front-end, or external circuit.
- **Waveform generation**: sine, triangle, ramp, or test signal generation.
- **Control voltage output**: drive analog-controlled power supplies, amplifiers, or industrial modules.
- **Sensor simulation**: emulate analog sensor output during testing.
- **Calibration output**: generate known voltage levels for production test or self-test.
- **Bias/offset control**: tune analog front-end offset or threshold.

Example: DAC code to voltage.

```c
float dac_code_to_voltage(uint16_t code, float vref, uint8_t bits) {
    return (float)code * vref / ((1 << bits) - 1);
}
// 12-bit DAC, code=2048, vref=3.3V -> about 1.65V
```

In many MCUs, DAC output cannot drive heavy loads directly. It may need an op-amp buffer if the external circuit needs low output impedance or higher current.

**ADC vs DAC Quick Comparison**

| Peripheral | Direction | Typical Question | Example |
|------------|-----------|------------------|---------|
| ADC | Analog → Digital | "What voltage/current/temperature is this?" | Read battery voltage |
| DAC | Digital → Analog | "What voltage/waveform should I output?" | Generate 1.65V reference |

Practical rule:

- Use **ADC** for measurement and feedback.
- Use **DAC** for analog generation, calibration, references, and simulation.

**ADC Key Concepts**

- **Resolution**: number of bits (8/10/12/16). 12-bit → 4096 levels
- **LSB voltage**: VREF / 2^N (e.g., 3.3V / 4096 ≈ 0.8 mV for 12-bit)
- **Sampling rate**: must satisfy Nyquist: fs ≥ 2 × fsignal
- **Acquisition time**: time to charge internal sampling capacitor — depends on source impedance

**Conversion Formula**

```c
float adc_to_voltage(uint16_t raw, float vref, uint8_t bits) {
    return (float)raw * vref / ((1 << bits) - 1);
}
// 12-bit, raw=2048, vref=3.3V → 1.65V
```

**ADC Architectures**

| Type               | Speed     | Resolution | Use case              |
|--------------------|-----------|------------|-----------------------|
| Flash (parallel)   | Fastest   | Low (8-bit)| Oscilloscopes         |
| SAR (successive approx) | Medium | 12-16 bit | MCU ADCs, general    |
| Delta-Sigma (Σ-Δ)  | Slow      | 16-24 bit  | Audio, precision meas.|
| Dual-slope         | Very slow | High       | DMMs                  |

**Anti-Aliasing Filter**

Low-pass RC filter before ADC input. Cutoff = fs/2. Without it, high-frequency noise aliases into the signal band.

```
                    R (e.g., 1kΩ)
Signal ───┤├────────────────── ADC_IN
                    │
                   [C] (e.g., 100nF)
                    │
                   GND
fc = 1/(2πRC) = 1/(2π × 1k × 100n) ≈ 1.6 kHz
```

**Oversampling**

Each doubling of sample count adds 0.5 bits of effective resolution. 4× oversampling + decimation → +1 bit. Used to achieve >12-bit effective resolution from a 12-bit ADC.

---

## 2.5 DMA (Direct Memory Access)

DMA moves data between memory and peripherals without CPU intervention.

**Why DMA?**

Without DMA: CPU must execute a load+store for every byte/word — wastes cycles on trivially predictable transfers.
With DMA: CPU programs the DMA controller, starts transfer, does useful work or sleeps, gets notified (IRQ) on completion.

**DMA Transfer Modes**

| Mode                    | Description                              |
|-------------------------|------------------------------------------|
| Memory-to-Memory        | Bulk copy (e.g., memcpy acceleration)    |
| Peripheral-to-Memory    | ADC → buffer, UART RX → buffer           |
| Memory-to-Peripheral    | buffer → SPI TX, buffer → DAC           |
| Circular                | Continuous ADC sampling to ping-pong buf |

**Ping-Pong Buffer with DMA**

```c
#define HALF_BUF  128
uint16_t adc_buf[HALF_BUF * 2];  // DMA fills continuously

// HAL callback fires at half-transfer and full-transfer
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc) {
    process_samples(&adc_buf[0], HALF_BUF);       // first half ready
}
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc) {
    process_samples(&adc_buf[HALF_BUF], HALF_BUF); // second half ready
}
```

**Cache Coherency with DMA**

On MCUs with data cache (Cortex-M7), DMA writes to RAM but CPU cache may have stale copy:

```c
// Before DMA reads from buffer: clean cache (write dirty lines to RAM)
SCB_CleanDCache_by_Addr((uint32_t*)tx_buf, sizeof(tx_buf));

// After DMA writes to buffer: invalidate cache (force CPU re-read from RAM)
SCB_InvalidateDCache_by_Addr((uint32_t*)rx_buf, sizeof(rx_buf));
```

Rule: DMA buffers must be in non-cached memory region OR you must explicitly manage cache.

---

## Interview Questions

### Easy

**Q: What is the difference between push-pull and open-drain GPIO output modes?**

> Push-pull: the driver actively drives both high (via P-FET) and low (via N-FET) — fast edges, strong drive. Cannot be wired-OR: two outputs driving opposite states short-circuit. Open-drain: only pulls low; relies on an external pull-up resistor for the high state. Can be wired-OR (I2C). Drive strength is limited by the pull-up resistor value.

**Q: Why must ISRs be short? Name three things you must never do in an ISR.**

> ISRs block all interrupts at the same or lower priority. A long ISR increases latency for every other interrupt and can cause missed events. Never: (1) call `HAL_Delay()` or any blocking delay — it relies on SysTick which may be blocked; (2) call `malloc`/`free` — not reentrant without locking; (3) use FreeRTOS blocking APIs (`xQueueReceive` with timeout, `xSemaphoreTake`) — use the `FromISR` variants instead.

**Q: What is PWM and how is duty cycle calculated from ARR and CCR values?**

> PWM (Pulse Width Modulation) is a square wave with variable on-time used to control power (motor speed, LED brightness, servo position). Duty cycle = CCR / (ARR + 1). Example: ARR=999, CCR=250 → 25% duty. The frequency is set by ARR and the prescaler: f_PWM = f_timer / ((PSC+1) × (ARR+1)).

---

### Mid

**Q: You connect a button to a GPIO input and enable the internal pull-up, but readings are still noisy/random. What are the likely causes?**

> (1) **Contact bounce** — mechanical buttons bounce 5–20ms on press/release; need software debounce (stable for N consecutive reads) or RC debounce filter. (2) **Long wires or traces acting as antenna** — picking up EMI; add a 100 nF capacitor from the pin to GND close to the MCU. (3) **Pull-up too weak for cable capacitance** — internal pull-up is ~40 kΩ; RC time constant with a long cable can be too slow to track fast transitions. Use an external lower-value pull-up.

**Q: What is interrupt latency on Cortex-M? What adds to the minimum of 12 cycles?**

> Minimum: 12 clock cycles for hardware to stack {xPSR, PC, LR, R12, R0–R3} and branch to the ISR vector. Additional latency: (1) **Pipeline flush** if a branch was in-flight — up to 2 cycles on M3/M4; (2) **Priority arbitration** if multiple IRQs fire simultaneously; (3) **Flash wait states** if the vector table or ISR is in flash with wait states; (4) **Interrupt disabled window** (`taskENTER_CRITICAL`) if the CPU was inside a critical section; (5) **D-cache miss** on Cortex-M7 if the ISR code was evicted.

**Q: Your 12-bit ADC reads a 50 Hz mains-noise signal on a sensor channel. How do you reduce it?**

> Nyquist says the ADC sample rate must be > 2× the highest frequency you care about. For 50 Hz noise: (1) Add an **anti-aliasing RC low-pass filter** before the ADC pin (cutoff ~10 Hz for a slow sensor) — this attenuates 50 Hz before sampling. (2) **Oversample at 200 sps and average** — 4× oversampling adds 1 bit resolution and averages out noise. (3) **Sample synchronously with the 50 Hz cycle** and use a notch filter at 50 Hz in software.

---

### Hard

**Q: You need to generate a 38 kHz IR carrier with exactly 33% duty cycle. Timer clock = 72 MHz. Calculate PSC, ARR, and CCR.**

> f_PWM = f_timer / ((PSC+1)(ARR+1)) = 38,000 Hz → (PSC+1)(ARR+1) = 72,000,000 / 38,000 ≈ 1894.7
> Choose PSC=0 (no prescale) → ARR = 1894 - 1 = **1893**.
> Actual f = 72,000,000 / 1894 = 38,015 Hz (error < 0.04% — acceptable for IR).
> 33% duty: CCR = round(0.333 × 1894) = **631**.
> Verify: 631/1894 = 33.3% ✓.

**Q: You use DMA in circular mode to receive UART data into a 256-byte ring buffer. Every few hundred transfers, the first byte of a new half-buffer is corrupted. What is happening?**

> Classic **DMA-cache coherency race on Cortex-M7**: The DMA writes the new half-buffer to RAM. The CPU's ISR callback reads `rx_buf[0]` — but the cache still holds the old value from the previous half. Fix: call `SCB_InvalidateDCache_by_Addr(rx_buf_half, 128)` at the beginning of each half-complete callback, before reading any bytes. Alternatively, place the DMA buffer in a non-cacheable MPU region.

**Q: Describe exactly what happens cycle-by-cycle when a higher-priority IRQ arrives while a lower-priority ISR is running on Cortex-M.**

> 1. Hardware detects the higher-priority pending IRQ while executing the lower-priority ISR.
> 2. Hardware **preempts** the current ISR — pushes a new exception frame {xPSR, PC, LR, R12, R0–R3} onto the current stack (MSP or PSP).
> 3. LR is set to `EXC_RETURN` value indicating nested exception.
> 4. CPU fetches the new ISR from the vector table and begins execution (12-cycle latency from detection to first instruction).
> 5. Higher-priority ISR runs to completion and executes `BX LR`.
> 6. Hardware **tail-chains** if another exception is pending (no unstacking + restacking), or **unstacks** the lower-priority ISR's frame and resumes it.
> Key: the lower-priority ISR stack frame is on top of the lower ISR's ongoing work — this is why nesting must be bounded to avoid stack overflow.
