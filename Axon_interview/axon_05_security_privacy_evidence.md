# Security, Privacy, and Evidence Integrity

Axon's JD explicitly says camera features should be built in a **secure, privacy-preserving way** ([JD](https://www.axon.com/careers/job/4958517003)). This file prepares the concepts most relevant to connected camera devices and digital evidence workflows.

---

## 1. Security Goals for Camera Devices

Protect:

- Captured audio/video.
- Metadata: timestamp, device serial, user assignment, event ID.
- Device identity and private keys.
- Firmware update path.
- Debug interfaces.
- Upload credentials.
- User/customer privacy.
- Chain of custody.

Threats:

- Device theft.
- Flash extraction.
- Debug port access.
- Malicious firmware update.
- Network interception.
- Replay/duplicate upload.
- Unauthorized deletion.
- Tampered metadata.
- Privacy leak through logs.

---

## 2. Evidence Integrity

Axon Evidence emphasizes digital fingerprints, file integrity, audit trails, encryption, and role-based permissions ([Axon Evidence](https://www.axon.com/products/axon-evidence)).

Device-level implications:

- Generate stable evidence ID at capture time.
- Record timestamp and device serial.
- Hash chunks/segments.
- Detect corruption before upload.
- Preserve local logs for recording start/stop/upload attempts.
- Do not allow unauthorized manual deletion.

Possible manifest:

```text
evidence_id
device_serial
firmware_version
recording_start_time
recording_end_time
chunk_count
chunk_hashes[]
metadata_hash
signature_or_mac
```

---

## 3. Chain of Custody

Chain of custody means you can explain:

- Who/what captured the evidence.
- When it was captured.
- Where it was stored.
- Who accessed or modified metadata.
- Whether file contents changed.
- When it was uploaded, viewed, exported, or deleted.

Axon's resource describes permanent activity logs/audit trails and body camera accountability behaviors such as timestamps, camera serial numbers, encrypted footage, auto-upload, and logs for camera start/stop activity ([Axon body-worn camera resource](https://www.axon.com/resources/how-axon-evidence-and-body-worn-camera-systems-work)).

Embedded angle:

> The device should create trustworthy source-of-truth metadata before the cloud ever sees the file.

---

## 4. Privacy-Preserving Device Software

Privacy is not only encryption. It includes minimizing unnecessary exposure.

Practices:

- Do not log raw video/audio.
- Do not log tokens, private keys, passwords, or full user identifiers.
- Limit debug command access.
- Protect local evidence storage.
- Use least privilege for services.
- Do not upload unnecessary data.
- Make retention/deletion behavior explicit and auditable.
- Separate diagnostic telemetry from evidence content.

Bad log:

```text
upload failed token=eyJ... user=john.smith@example.com file=/evidence/full_video.mp4
```

Better log:

```text
upload failed evidence_id=7f3a... reason=tls_timeout retry=3
```

---

## 5. Secure Boot and OTA

Minimum secure update requirements:

- Firmware image is signed.
- Bootloader verifies signature before boot.
- Version metadata prevents rollback.
- A/B update prevents bricking.
- Failed update rolls back.
- Private signing key is never on device.

Do not rely on CRC alone:

- CRC detects accidental corruption.
- Signature detects unauthorized modification.

Interview phrase:

> TLS protects transport, but OTA still needs local image signature verification because the device must reject tampered images regardless of how they arrived.

---

## 6. Device Identity and Key Management

Good design:

- Device-unique credential.
- Private key generated/stored in secure element, TPM, TrustZone, or protected storage.
- Backend associates public identity/cert with device.
- Rotate/revoke credentials if device is compromised.
- Development and production keys are separated.

Avoid:

- One shared key for all devices.
- Hardcoded cloud API token in firmware.
- Storing private update signing key on device.
- Printing keys in debug logs.

---

## 7. Network Security

Prepare to discuss:

- TLS certificate validation.
- Certificate pinning trade-offs.
- Clock issues during certificate validation.
- Mutual TLS for device identity.
- Replay protection with nonces/timestamps/counters.
- Idempotency keys for retry.
- Backoff and jitter to avoid retry storms.

Common bug:

> Disabling certificate verification to "fix" a field connectivity issue. This converts a network bug into a security vulnerability.

---

## 8. Bluetooth and Local Interfaces

Bluetooth/BLE can be used for provisioning, control, or short-range workflows.

Security rules:

- Treat all BLE writes as untrusted.
- Require pairing/bonding appropriate to risk.
- Authenticate privileged commands.
- Rate-limit command attempts.
- Avoid factory/debug commands in production builds.
- Validate lengths and enum values.
- Do not expose secrets over BLE.

UART/SWD/JTAG:

- Disable or lock in production.
- If RMA unlock exists, make it authenticated and usually data-destructive.

---

## 9. Testing Security Properties

Test cases:

- Invalid firmware signature rejected.
- Old firmware rollback rejected.
- Power loss during OTA recovers.
- Corrupted video chunk detected.
- Upload retry does not create duplicate evidence.
- TLS certificate failure blocks upload.
- BLE malformed command rejected.
- Debug shell disabled in production.
- Logs do not contain secrets.
- Disk full does not corrupt existing evidence.

Tools:

- Static analysis.
- Fuzz protocol parsers.
- ASan/UBSan for host parser tests.
- Network MITM test environment.
- Hardware-in-loop power interruption tests.

---

## 10. Interview Q&A

**Q: What does privacy-preserving camera firmware mean?**

> It means the firmware collects and exposes only what is needed, protects evidence at rest and in transit, avoids leaking sensitive content in logs, enforces access control on local/debug interfaces, and preserves auditable behavior for capture/upload/deletion workflows.

**Q: How do you ensure uploaded video was not tampered with?**

> Create hashes or a Merkle tree over recorded chunks, store signed/hashed metadata, verify chunks before upload, and have the server validate checksums on receipt. Preserve audit logs for recording and upload events.

**Q: Why is chain of custody relevant to embedded firmware?**

> The firmware is the source of evidence. If timestamp, device identity, recording boundaries, or local storage integrity are weak, the cloud cannot fully repair that later. The device must create trustworthy metadata and protect the recording from capture through upload.
