# Embedded Systems 07 — Power Management

---

## 7.1 Sleep Modes

ARM Cortex-M sleep modes (exact names vary by MCU vendor):

| Mode       | Clocks off        | Wake sources          | Wake latency |
|------------|-------------------|-----------------------|--------------|
| Sleep      | CPU only          | Any interrupt         | Very fast    |
| Stop/Deep  | CPU + peripherals | EXTI, RTC, WDT        | Moderate     |
| Standby    | Almost everything | WKUP pin, RTC, RST    | POR-like     |
| Shutdown   | Everything        | WKUP pin only         | POR-like     |

```c
// Enter sleep: CPU halted, peripherals still running
__WFI();   // Wait For Interrupt
__WFE();   // Wait For Event (can be woken by SEV instruction or external event)

// Enter Stop mode (STM32 example)
HAL_PWR_EnterSTOPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
// Execution resumes here after wakeup
SystemClock_Config();  // PLL may need re-initialization after Stop mode
```

**Tickless Mode in FreeRTOS**

Normally SysTick fires every tick (e.g., every 1ms), waking the CPU pointlessly. Tickless idle:
1. On entering idle task, compute next wakeup time
2. Configure RTC/LPTIM for that duration
3. Enter Stop mode
4. On wakeup, update tick count to compensate for slept ticks

Configured via `configUSE_TICKLESS_IDLE = 1` in FreeRTOSConfig.h.

---

## 7.2 Low-Power Design Patterns

**Event-Driven vs Polling**

```c
// Polling: CPU always active — terrible for battery
while (1) {
    if (button_pressed()) handle_button();
}

// Event-driven: CPU sleeps, woken by interrupt
void EXTI0_IRQHandler(void) {   // button wakes CPU
    button_event = true;
}
void main(void) {
    while (1) {
        if (button_event) { button_event = false; handle_button(); }
        __WFI();  // sleep until next interrupt
    }
}
```

**Clock Gating**

Disable peripheral clocks when not in use. Each clock domain draws quiescent current even with peripheral idle.

```c
// Disable UART when not transmitting
__HAL_RCC_USART2_CLK_DISABLE();
// Enable only when needed
__HAL_RCC_USART2_CLK_ENABLE();
```

**Peripheral Power Gating**

Some MCUs can fully power-down peripheral domains. Consult datasheet for power domains and isolation requirements.

**LDO vs DCDC**

| Type | Efficiency | Noise     | Cost  | Use when               |
|------|------------|-----------|-------|------------------------|
| LDO  | Low (η = VOUT/VIN) | Low | Low | VIN ≈ VOUT, low current |
| DCDC | High (>90%) | Higher   | Higher | VIN >> VOUT, high current |

**Low-Power Design Checklist**

1. **Event-driven** — never poll; use interrupts and `WFI/WFE`
2. **Minimize active time** — wake → work → sleep as fast as possible
3. **Tickless RTOS** — disable SysTick when idle
4. **Clock gating** — disable peripheral clocks when unused
5. **Reduce SYSCLK** — run at lowest frequency that meets timing requirements
6. **Optimize peripherals** — turn off ADC, DAC between conversions
7. **Use low-power peripherals** — LPTIM, LPUART operate in Stop mode
8. **Voltage scaling** — many MCUs support lower VDD at lower clock speeds
9. **Avoid floating inputs** — pull resistors on unused pins prevent floating (but add quiescent current)
10. **Profile current** — use power profiler (e.g., Nordic PPK2, Otii Arc) to find unexpected active periods

---

## Interview Questions

### Easy

**Q: What is the difference between Sleep mode and Stop mode on a Cortex-M MCU?**

> **Sleep**: only the CPU core is halted; all peripherals (UART, SPI, timers, DMA) continue running. Woken by any enabled interrupt. Very fast wakeup (~12 cycles). Current reduction: ~30–50% vs active. **Stop**: CPU and most peripherals clocks are gated off. Only low-power peripherals (RTC, LPTIM, EXTI, WDT) can wake the MCU. Wakeup latency is higher (PLL may need to restart). Current reduction: 95–99% vs active. Used for battery-powered deep sleep.

**Q: What does `__WFI()` do at the hardware level?**

> `WFI` (Wait For Interrupt) is a Cortex-M instruction that halts the CPU pipeline and gates the CPU clock until any enabled interrupt (or NMI/reset) occurs. The MCU enters at least Sleep mode. If `SLEEPDEEP` bit in `SCR` is set, it enters Stop/Standby mode instead. Unlike a spin loop, `WFI` actually stops the CPU from consuming power — no instructions execute until a wakeup event.

**Q: What is clock gating and why does it save power?**

> Clock gating disables the clock signal to a peripheral's logic when it is not in use. CMOS digital logic consumes power proportional to: switching frequency × capacitance × voltage². With the clock stopped, flip-flops stop toggling and dynamic power drops to near zero (only leakage current remains). On STM32: `__HAL_RCC_USART2_CLK_DISABLE()` disables the APB clock to USART2. Even if the peripheral is "idle," it still consumes power if its clock is running.

---

### Mid

**Q: Your MCU consumes 2 mA in Stop mode but the datasheet says 10 µA. What are the likely causes?**

> (1) **Peripheral clocks not disabled** — a peripheral left running (ADC, DAC, USB, comparator) keeps its clock domain active. (2) **GPIO floating inputs** — a floating input toggles randomly causing a pull-up/pull-down to sink current; or an input is driven by an external signal that toggles. (3) **Pull resistors** — internal or external pull-up/down on pins that change state draw steady-state current through the resistor. (4) **VDDIO pins at high voltage** — if an I/O pin is being driven by an external source to a non-rail voltage, the ESD protection diodes leak. (5) **PLL / HSE still running** — check `RCC->CR` to ensure HSE/PLL are off. Use a power profiler with µA resolution and toggle GPIO before/after disabling each subsystem to isolate.

**Q: What is tickless idle in FreeRTOS? What hardware does it rely on?**

> Normally, FreeRTOS's idle task does nothing and SysTick fires every 1ms, waking the CPU to check if any task is ready — 1000 unnecessary wakeups per second. Tickless idle: when the idle task runs and no task will wake for N ticks, the idle hook: (1) programs an LPTIM/RTC timer for N ticks, (2) enters Stop mode, (3) wakeup fires, (4) adjusts `xTickCount` to compensate for the slept time. Relies on: a low-power timer that runs in Stop mode (LPTIM, RTC, SysTick cannot be used since SysTick stops in Stop mode). Configure with `configUSE_TICKLESS_IDLE = 2` and implement `vPortSuppressTicksAndSleep`.

**Q: When would you choose an LDO over a DCDC converter, despite the LDO's lower efficiency?**

> LDO advantages: (1) **No switching noise** — critical for analog/RF circuits; DCDC switching frequency appears as spikes on the output. (2) **Simpler design** — no inductor, smaller footprint, fewer external components. (3) **Good efficiency when VIN ≈ VOUT** — η = VOUT/VIN, so at 3.0V in / 2.8V out, η = 93%, comparable to DCDC. (4) **Fast transient response** — better for loads with sudden current spikes. Use DCDC when VIN >> VOUT (e.g., 5V → 1.8V, η = 36% for LDO vs >90% for DCDC) or when average current is high (>100 mA) and battery life matters.

---

### Hard

**Q: An IoT sensor node must run 10 years on a 1000 mAh battery. Samples every 60 seconds, active 50ms at 10mA, rest in deep sleep at 5µA. Does it last 10 years?**

> Average current = (active_time × active_current + sleep_time × sleep_current) / period
> = (0.05s × 10mA + 59.95s × 0.005mA) / 60s
> = (0.5 mAh/3600 + 0.2998 mAh/3600) / ... let's compute properly:
> Per 60s cycle: charge = 0.05 × 10mA + 59.95 × 0.005mA = 0.5 mAs + 0.29975 mAs = 0.79975 mAs
> Average current = 0.79975 mAs / 60s = **0.01333 mA = 13.33 µA**
> Capacity at 1000 mAh / 0.01333 mA = **75,019 hours ≈ 8.57 years** — just short of 10 years.
> To reach 10 years: need average ≤ 11.4 µA. Reduce active current (use DCDC, lower clock speed) or reduce active time (faster ADC, DMA transfer instead of CPU polling).

**Q: After exiting Stop mode, the first SPI transaction is corrupted. What is happening and how do you fix it?**

> Likely causes: (1) **PLL not yet locked** — Stop mode disables the PLL. If `SystemClock_Config()` is called asynchronously and SPI starts before the PLL lock flag (`RCC_CR_PLLRDY`) is set, SPI runs at the wrong frequency. Fix: poll for PLL lock before enabling peripherals. (2) **SPI peripheral state** — some MCUs require re-initializing the SPI peripheral after Stop mode because its internal state machine reset. Fix: call `HAL_SPI_DeInit` / `HAL_SPI_Init` after wakeup. (3) **CS line glitch** — if CS is a GPIO and the GPIO clock was disabled, CS may float during the wakeup transition. Fix: re-enable GPIO clock and re-configure CS pin before the first transaction.

**Q: In a FreeRTOS tickless implementation using LPTIM, how do you keep the tick count accurate after wakeup?**

> In `vPortSuppressTicksAndSleep(xExpectedIdleTime)`: (1) Read the LPTIM counter before entering Stop mode. (2) Enter Stop. (3) On wakeup (LPTIM match or external IRQ), read the LPTIM counter again. (4) Compute `actualSleptTicks = (lptim_after - lptim_before) × configTICK_RATE_HZ / lptim_freq`. (5) Call `vTaskStepTick(actualSleptTicks)` to advance the FreeRTOS tick count by the actual number of ticks slept (may be less than `xExpectedIdleTime` if woken early by an external IRQ). Critical: if woken early, only count ticks up to the actual elapsed time, not the expected time — otherwise task delays expire early.
