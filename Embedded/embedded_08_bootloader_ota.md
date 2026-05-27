# Embedded Systems 08 — Bootloader & Firmware Update

---

## 8.1 Bootloader Fundamentals

A bootloader is the first code that runs after reset. Its job:
1. Minimal hardware initialization (clock, RAM if needed)
2. Check if firmware update is pending
3. Verify application integrity (CRC, signature)
4. Jump to application

**Jumping to Application**

```c
typedef void (*AppEntry)(void);

void jump_to_app(uint32_t app_start_addr) {
    // 1. Verify app magic / CRC first
    // 2. Disable all interrupts and peripherals
    __disable_irq();

    // 3. Set VTOR to app's vector table
    SCB->VTOR = app_start_addr;

    // 4. Set SP to app's initial stack pointer (first word of vector table)
    __set_MSP(*(volatile uint32_t*)app_start_addr);

    // 5. Get app reset handler (second word of vector table)
    AppEntry app = (AppEntry)(*(volatile uint32_t*)(app_start_addr + 4));

    // 6. Jump
    app();
}
```

**Flash Layout Example**

```
0x08000000 ├─────────────────────┤  Bootloader (32KB)
           │    Bootloader code  │
0x08008000 ├─────────────────────┤  Application (448KB)
           │    Vector table     │  ← VTOR set here
           │    Application code │
0x0807F000 ├─────────────────────┤  Config / NVS (4KB)
           │    Config data      │  CRC, version, update flags
0x08080000 └─────────────────────┘
```

**Boot Sequence from Power-On to `main()`**

1. **POR**: voltage crosses threshold → release reset
2. **ROM bootloader** (if present): clock init, load/verify bootloader from flash
3. **Bootloader / startup_stm32xx.s**:
   - Stack pointer loaded from vector table word 0
   - `Reset_Handler` invoked from vector table word 1
   - Copy `.data` section from Flash LMA → SRAM VMA
   - Zero `.bss` section
   - Call `SystemInit()`: configure PLL, clocks, Flash wait states
   - Call C++ static constructors (`__libc_init_array`)
4. **`main()`** begins

---

## 8.2 OTA (Over-The-Air) Update

**A/B Partition Scheme**

```
Flash:
 Slot A (active)   │ Slot B (new firmware)
 ──────────────────┼──────────────────────
 Running app       │ Download new firmware here
                   │ Verify integrity
                   │ Reboot → bootloader selects B
                   │ Run B, verify it's healthy
                   │ Mark B as active
```

Advantages:
- Atomic update: either A or B runs, never partial
- Rollback: if B fails to boot N times, revert to A

**Integrity Verification**

```c
// CRC32 verification
#include <stdint.h>

uint32_t crc32(const uint8_t *data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    while (len--) {
        crc ^= *data++;
        for (int i = 0; i < 8; i++)
            crc = (crc >> 1) ^ (crc & 1 ? 0xEDB88320 : 0);
    }
    return ~crc;
}

bool verify_firmware(uint32_t addr, uint32_t size, uint32_t expected_crc) {
    return crc32((uint8_t*)addr, size) == expected_crc;
}
```

**Secure Boot**

Chain of trust:
1. ROM bootloader (immutable, in hardware) — verifies stage-1 bootloader signature
2. Stage-1 bootloader (in protected flash) — verifies application signature
3. Application runs only if signature verified with stored public key

Uses asymmetric crypto (RSA/ECC): private key signs firmware on build server, public key stored in device (ideally in OTP/eFuse — can't be changed).

```
eFuse / OTP
┌─────────────────┐
│ Hash of RoT key │  (burned at manufacturing, immutable)
└────────┬────────┘
         │ verifies
         ▼
  ROM Bootloader
  (verifies bootloader image signature using stored public key hash)
         │
         ▼
  Bootloader
  (verifies application signature)
         │
         ▼
  Application
```

---

## Interview Questions

### Easy

**Q: What are the main responsibilities of a bootloader?**

> (1) Minimal hardware initialization (clock setup, RAM init if needed). (2) Check if a firmware update is available (flag in flash, UART/USB trigger, etc.). (3) Verify the application image integrity (CRC32, hash, or cryptographic signature). (4) Jump to the application if valid. The bootloader must be small, reliable, and rarely updated — it is the last line of defense before a bricked device.

**Q: Why must you set the VTOR (Vector Table Offset Register) before jumping to an application?**

> After reset, the CPU fetches interrupts from the vector table at address 0 (or the default VTOR). The application has its own vector table at its start address. If VTOR is not updated, any interrupt that fires after the jump will use the bootloader's vector table, calling bootloader ISRs with the application's stack — instant hard fault. Setting `SCB->VTOR = app_start_addr` redirects all exceptions and interrupts to the application's handlers.

**Q: What is the A/B partition scheme in OTA updates?**

> Two equally-sized flash slots: A (currently running) and B (update target). The new firmware is downloaded into slot B while the device continues running from A. After download completes and integrity is verified, the bootloader is instructed to boot B on the next reset. If B boots and runs successfully, it is marked permanent. If B fails to boot N times, the bootloader reverts to A. This ensures the device always has a known-good fallback, making updates atomic and recoverable.

---

### Mid

**Q: Walk through the exact steps of jumping from bootloader to application on Cortex-M. Why must interrupts be disabled?**

> 1. Verify the application (CRC/signature check). 2. `__disable_irq()` — interrupts must be off to prevent any IRQ firing between VTOR update and the jump; if an IRQ fires using the old VTOR, it calls a bootloader ISR that no longer exists in the new context. 3. `SCB->VTOR = app_addr` — set new vector table. 4. `__set_MSP(*(uint32_t*)app_addr)` — load the application's initial stack pointer (word 0 of its vector table). 5. Read the application reset handler address (word 1 of vector table). 6. Call the reset handler (function pointer call). Interrupts are re-enabled naturally when the application calls `__enable_irq()` after its own initialization.

**Q: Your OTA download completes, integrity passes, but after reset the bootloader still boots the old slot. What could be wrong?**

> (1) **Update flag not written to flash** — the flag that tells the bootloader to try the new slot might not have been committed (pending in cache/write buffer). Add a flush/verify step after writing. (2) **Flag in a write-protected region** — the flag sector may be accidentally protected. (3) **Bootloader reads the wrong address** — off-by-one in flash layout, or CRC check on the new slot is recomputing incorrectly because the expected CRC was not stored correctly. (4) **Bootloader version mismatch** — old bootloader may not understand the new flag format. Debug by reading back the flash content after writing the update flag, before resetting.

**Q: What is the difference between CRC verification and cryptographic signature verification?**

> **CRC**: detects accidental corruption (bit flips, incomplete writes). Fast, cheap (hardware CRC unit on most MCUs), but provides no security — an attacker can trivially compute a valid CRC for malicious firmware. **Cryptographic signature** (RSA/ECDSA): the firmware image is signed with the manufacturer's private key. The device verifies with the stored public key. Prevents installation of unauthorized firmware. Slower (requires crypto accelerator or software ECC), and requires key management. Use CRC for detecting flash corruption; use signatures for secure boot against malicious updates.

---

### Hard

**Q: Implement rollback: if new firmware fails to boot successfully 3 times, revert to the old slot. What data structures and logic does the bootloader need?**

> Store a **boot descriptor struct** in a dedicated flash sector (separate from both slots to avoid being erased on update):
> ```c
> typedef struct {
>     uint32_t active_slot;      // 0 = A, 1 = B
>     uint32_t pending_slot;     // slot to try next
>     uint32_t boot_try_count;   // incremented before each boot attempt
>     uint32_t boot_confirmed;   // set by app after successful startup
>     uint32_t crc;              // CRC of this struct
> } BootDescriptor;
> ```
> Bootloader logic: if `pending_slot` differs from `active_slot`: increment `boot_try_count`, write back, boot `pending_slot`. Application on successful startup calls `confirm_boot()` which sets `boot_confirmed=1`, copies `pending_slot → active_slot`, resets `boot_try_count`. If `boot_try_count ≥ 3` and `boot_confirmed == 0`: revert to `active_slot`. Protect the descriptor with a CRC to detect corruption.

**Q: Describe a complete secure boot chain for Cortex-M33 from eFuse to application, including key types and manufacturing provisioning.**

> Hardware: M33 with TrustZone. During manufacturing: burn a SHA-256 hash of the ROTPK (Root of Trust Public Key) into OTP/eFuse. ROTPK private key is stored in an HSM, never on the device. Chain: (1) **ROM bootloader** (in SoC ROM, immutable) reads the Stage-1 bootloader from flash, computes its hash, compares against the ROTPK-signed hash — verifies using ROTPK stored in OTP. (2) **Stage-1 bootloader** (in secure flash) verifies the application image using its own embedded public key (a sub-key signed by ROTPK). (3) Application runs in Non-Secure world after TF-M sets up TrustZone. Each stage uses ECC P-256 signatures (~72-byte signature, 32-byte key). Manufacturing: provision each device individually using a secure provisioning station that connects to the HSM to sign device-specific keys.

**Q: A config sector is rewritten on every boot. Flash endurance is 100K cycles. Firmware updates are monthly. How long before wear-out and how do you redesign?**

> Wear-out: 100,000 cycles. If the device boots once per day: 100,000 / 365 = **273 years** for the firmware update sector. But the config sector rewritten on every boot: if the device boots 10x/day: 100,000 / 3,650 = **27 years** — manageable, but marginal for a 20-year product. Redesign strategies: (1) **Wear leveling**: rotate config writes across N pages and track the current page index. Multiplies endurance by N. (2) **Write on change only**: compare new config with existing before erasing — only erase/write if something actually changed. (3) **Incremental writes**: use a log/journal approach — append small delta records and compact periodically. (4) **External EEPROM**: use a dedicated EEPROM (1M+ cycles) for frequently-changing config; keep flash for firmware only.
