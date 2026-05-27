# Embedded Systems 11 — Safety & Security

---

## 11.1 Functional Safety

**ISO 26262 (Automotive)**

Automotive Safety Integrity Level (ASIL):

| ASIL | Hazard probability | Examples                         |
|------|-------------------|----------------------------------|
| A    | Low               | Comfort features                 |
| B    | Medium-low        | Mirrors, windows                 |
| C    | Medium-high       | ABS, traction control            |
| D    | Highest           | Steering, braking, airbags       |
| QM   | Not safety-related| Infotainment                     |

Requirements scale with ASIL:
- Software development lifecycle (V-model)
- Requirements traceability
- Code coverage targets (MC/DC for ASIL D)
- Software fault tolerance, freedom from interference

**IEC 61508 (General)**

Safety Integrity Level (SIL 1–4). Similar concept to ASIL. Used for industrial machinery, process control.

**MISRA C:2012 (Key Rules)**

| Rule  | Category | Description                              |
|-------|----------|------------------------------------------|
| 10.1  | Required | Operands of essential Boolean expressions|
| 11.3  | Required | No cast between pointer and integer      |
| 12.1  | Advisory | Precedence: use explicit parentheses     |
| 14.4  | Required | if/while/for condition must be Boolean   |
| 15.5  | Advisory | Function return at end only              |
| 16.4  | Required | switch: every case terminated with break |
| 17.1  | Required | No `<stdarg.h>` (no variadic functions)  |
| 21.3  | Required | No `malloc`/`free`                       |

**Software Fault Tolerance Techniques**
- **Watchdog timer**: if firmware hangs, WDT resets the system
- **CRC on config data**: detect flash corruption
- **Redundant execution**: run critical computation twice, compare
- **Stack canary**: detect stack overflow before corruption propagates
- **Safe state**: on fault detection, switch to known-safe output state (e.g., motor off)

```c
// Watchdog refresh — must be called periodically
void main_loop(void) {
    while (1) {
        HAL_IWDG_Refresh(&hiwdg);  // if missed → reset in ~1 sec
        do_application_work();
    }
}
```

---

## 11.2 Security

**Secure Boot Chain**

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

**TrustZone (ARMv8-M)**

Hardware isolation between Secure World and Non-Secure World:
- Secure world: keys, crypto, secure boot, firmware update auth
- Non-Secure world: main application, RTOS
- Transition via `SG` instruction (secure gateway), `NSC` (non-secure callable) regions

```c
// Trusted Firmware-M (TF-M) — secure services API
psa_status_t status = psa_crypto_init();
psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;
psa_set_key_usage_flags(&attr, PSA_KEY_USAGE_SIGN_HASH);
psa_set_key_algorithm(&attr, PSA_ALG_ECDSA(PSA_ALG_SHA_256));
psa_generate_key(&attr, &key_id);  // key stored in secure world
```

**Common Vulnerabilities in Embedded**

| Vulnerability          | Description                              | Mitigation                           |
|------------------------|------------------------------------------|--------------------------------------|
| Debug left enabled     | JTAG/SWD accessible in production        | Disable/lock debug port in production |
| Unencrypted firmware   | Binary readable from flash               | Encrypt flash, enable RDP             |
| Hardcoded credentials  | Passwords in firmware binary             | Device-unique keys, provisioning      |
| Buffer overflow        | Unchecked input lengths                  | Bounds checking, stack canaries, MPU  |
| Timing side-channel    | Crypto operations leak timing info       | Constant-time algorithms              |
| Command injection      | Unvalidated UART/shell commands          | Input validation, allowlist           |

**Flash Read Protection (STM32 example)**

| RDP Level | Effect                                                    |
|-----------|-----------------------------------------------------------|
| 0         | No protection (debug fully accessible)                   |
| 1         | Debug disconnected, flash unreadable from debugger        |
| 2         | Permanent — cannot be reversed, JTAG permanently disabled |

---

## Interview Questions

### Easy

**Q: What is ASIL-D and what kind of system requires it?**

> ASIL-D (Automotive Safety Integrity Level D) is the highest safety level in ISO 26262. It applies to functions where failure could cause severe, life-threatening injury with high probability. Examples: electronic power steering, brake-by-wire, airbag deployment, active safety systems. ASIL-D requires the most stringent development processes: full MC/DC code coverage, extensive FMEA/FMEDA, hardware redundancy, freedom-from-interference proof, and formal sign-off at each V-model stage.

**Q: Name three MISRA C rules and the reason behind each.**

> (1) **Rule 21.3 (Required)**: No `malloc`/`free` — dynamic memory is non-deterministic and can fragment, causing unpredictable failures in safety-critical code. (2) **Rule 14.4 (Required)**: `if`/`while`/`for` condition must be essentially Boolean — prevents accidental use of integers as conditions (e.g., `if (ptr)` instead of `if (ptr != NULL)`), reducing type confusion bugs. (3) **Rule 11.3 (Required)**: No cast between a pointer and integer — such casts are implementation-defined and non-portable; accessing hardware registers must use the approved `volatile` pointer pattern.

**Q: What does a watchdog timer do and why is it critical in safety-critical systems?**

> A WDT is a hardware timer that resets the MCU if not "kicked" (refreshed) within a configured timeout period. In safety-critical systems: if firmware hangs (infinite loop, deadlock, exception handler stuck), the WDT brings the system back to a known state within a bounded time. This prevents a stuck system from remaining in an unsafe state (e.g., motor running indefinitely, valve stuck open). The key discipline: the WDT kick must be placed in a path that proves the system is functioning correctly — not in an ISR that runs regardless of main-loop health.

---

### Mid

**Q: What is the difference between ISO 26262 and IEC 61508? Which applies to a medical infusion pump?**

> **ISO 26262**: automotive-specific, derived from IEC 61508. Uses ASIL A–D levels. Covers E/E systems in passenger vehicles. **IEC 61508**: general functional safety standard for electrical/electronic/programmable systems across all industries. Uses SIL 1–4 levels. Medical devices fall under **IEC 62304** (medical device software) and **IEC 60601** (electrical medical equipment) which reference IEC 61508 concepts but with medical-specific requirements. An infusion pump would use IEC 62304 (software lifecycle) with safety levels based on risk analysis per ISO 14971.

**Q: What is the difference between RDP Level 1 and Level 2 on STM32? Why is Level 2 irreversible?**

> **RDP Level 1**: debug access is blocked (SWD/JTAG cannot read flash), but the MCU can still be unlocked by performing a mass-erase (which destroys all flash content). Useful for production builds — firmware is protected, but device can be re-flashed by the manufacturer. **RDP Level 2**: JTAG/SWD pins are permanently disabled in hardware and can never be re-enabled. No mass erase option. Irreversible because the protection bits are stored in OTP (One-Time Programmable) fuses that can only be programmed, never erased. Use Level 2 only for the highest-security production devices — a bricked device with Level 2 cannot be recovered.

**Q: What is a timing side-channel attack? Give an embedded example and the mitigation.**

> A timing side-channel leaks secret information through the time taken by an operation. Example: a string comparison `if (memcmp(received_password, stored_password, 8) == 0)` returns early on the first mismatch. An attacker measures response time for different inputs — longer time means more bytes matched. By trying all 256 values for byte 0 until response time increases, then all 256 for byte 1, etc., they recover an 8-byte password in 8 × 256 = 2048 guesses instead of 256^8. **Mitigation**: use constant-time comparison — compare all bytes regardless of early mismatch, accumulate XOR of differences, check at the end. `crypto_memneq()` in Linux kernel or CMSIS `memcmp_const_time()` implement this.

---

### Hard

**Q: Design an ASIL-C motor controller where the requirement is "motor must stop within 10ms of overcurrent detection." Describe the architecture.**

> Hardware: (1) **Hardware overcurrent comparator** directly disables the gate driver (< 1µs, no software path). (2) **Redundant current sensors** (two independent ADC channels) to detect sensor failure. Software: (3) **Safety monitor task** (highest RTOS priority, 1ms period): reads both current sensors, compares, trips a software stop flag. If sensors disagree → safe state. (4) **Watchdog**: SW1 (IWDG) kicked by monitor task — timeout 5ms. If task hangs, reset occurs. (5) **Safe state function**: shuts PWM, asserts brake signal, sets error flag — called from ISR or task. Verification: (6) Fault injection testing — remove sensor signal and verify stop within 10ms. (7) Formal FMEA for every failure mode. The hardware path ensures <1µs stop; the software path within 1ms provides redundancy and diagnostics.

**Q: Your device uses TrustZone on Cortex-M33. Describe the complete call mechanism when Non-Secure code needs to call a Secure world crypto function.**

> 1. **NS code calls** a function in the Non-Secure Callable (NSC) region — a special flash region that contains `SG` (Secure Gateway) instructions. 2. **`SG` instruction** switches the CPU to Secure state: sets the Security state bit in the CONTROL register, updates the security state in the exception frame. 3. CPU is now in **Secure world** — all S-world registers and memory are accessible, NS world registers are stacked. 4. **Veneer function** in the NSC region validates parameters (pointer ranges are checked against `cmse_check_address_range` to ensure NS code isn't passing pointers into S-world memory as a confused-deputy attack). 5. **Crypto operation** executes in S-world. 6. Return: `BXNS LR` instruction switches back to NS state, returns result via R0. The S-world must never trust pointers from NS-world without validation.

**Q: An attacker uses a timing oracle with 10ns resolution to recover your AES key byte-by-byte. Explain the attack and the complete firmware fix.**

> **Attack (Cache-Timing / AES Lookup Table attack)**: Software AES using lookup tables (T-tables) accesses different memory addresses based on the key and plaintext. Different addresses cause different cache-miss patterns, changing execution time by ~10–100ns per block. By measuring enough ciphertext/time pairs, the attacker can recover the key using statistical analysis (e.g., Kocher's timing attack). **Fix — hardware AES**: use the MCU's hardware AES accelerator (STM32 CRYP peripheral, etc.) — executes in constant time, no data-dependent memory accesses. **Fix — constant-time software**: use bitsliced AES or a lookup-table implementation with `volatile` indexed by a time-constant memory access pattern. Ensure the comparison of computed vs expected values uses `crypto_memneq`. Also add random delay injection and power noise to defeat more sophisticated power analysis attacks. Test with the `dudect` constant-time testing framework.
