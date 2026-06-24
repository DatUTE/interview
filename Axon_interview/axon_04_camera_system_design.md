# Camera System Design for Axon-Style Devices

This file prepares you to discuss connected camera device architecture without claiming private Axon internals.

---

## 1. High-Level Architecture

```text
Camera Sensor / Microphone
    |
Capture Driver / HAL
    |
Encoder Pipeline
    |
Local Storage + Metadata + Integrity Hash
    |
Connectivity Manager
    |
Uploader / Sync Engine
    |
Cloud Evidence Platform
    |
Mobile / Desktop / Backend Workflows
```

Design goals:

- Do not lose recorded evidence.
- Preserve integrity and metadata.
- Continue working under poor network conditions.
- Keep battery and thermal behavior under control.
- Support secure update and device provisioning.
- Provide enough logs/telemetry for field troubleshooting.

---

## 2. Core Subsystems

| Subsystem | Responsibilities | Failure Modes |
|-----------|------------------|---------------|
| Capture | Sensor/audio capture, timestamps, buffers | Dropped frames, clock drift, driver crash |
| Encoding | Compress audio/video | CPU overload, thermal throttling |
| Storage | Write evidence locally | Disk full, corruption, power loss |
| Metadata | Device ID, timestamp, user/case/event info | Wrong assignment, missing metadata |
| Integrity | Hash/Merkle tree, file fingerprint | Tampering, partial upload mismatch |
| Connectivity | Wi-Fi/Bluetooth/network state | Disconnect, DNS failure, captive network |
| Upload | Retry/resume/idempotent sync | Duplicate upload, stuck queue |
| Update | OTA, rollback, versioning | Bricked device, downgrade attack |
| Observability | Logs, metrics, crash reports | Sensitive logs, insufficient debug info |

---

## 3. Evidence Capture Pipeline

Important design pattern: bounded pipeline with backpressure.

```text
Capture thread
  -> bounded frame queue
Encoder thread
  -> encoded chunk queue
Storage writer
  -> durable local file
Uploader
  -> cloud when network is available
```

Trade-offs:

- Bigger buffers reduce drops but increase memory and latency.
- Smaller buffers expose backpressure earlier but may drop frames.
- Recording must not depend on network availability.
- Upload should be decoupled from capture.
- Critical metadata should be written early and atomically.

For interview:

> I would treat local durable storage as the source of truth until upload is confirmed. The network uploader is a consumer of stored evidence, not part of the capture critical path.

---

## 4. Local Storage Design

Concerns:

- Power loss during write.
- Disk full.
- File corruption.
- Wear leveling.
- Metadata consistency.
- Secure deletion/retention requirements.

Safer write pattern:

```text
write to temp file
fsync data
write metadata/checkpoint
fsync metadata
rename temp -> committed file
fsync parent directory
```

For large video:

- Write in chunks/segments.
- Maintain per-segment hash.
- Store upload checkpoint.
- Resume upload from last confirmed segment.
- Keep manifest signed/hashed.

---

## 5. Upload and Sync Engine

Requirements:

- Retry with exponential backoff and jitter.
- Resume partial uploads.
- Avoid duplicates with idempotency keys.
- Verify cloud acknowledgement before deleting local copy.
- Handle auth refresh.
- Handle network transitions: offline -> Wi-Fi -> upload.

State machine:

```text
Recorded
  -> ReadyToUpload
  -> Uploading
  -> UploadedPendingVerify
  -> Confirmed
  -> RetentionEligible
```

Idempotency:

```text
evidence_id = hash(device_id, recording_start_time, monotonic_counter)
chunk_id    = hash(evidence_id, chunk_index)
```

If upload is retried, backend can recognize duplicate chunks.

---

## 6. Connectivity Design

Wi-Fi:

- Reconnect automatically.
- Distinguish no signal vs auth failure vs DNS/TLS failure.
- Avoid infinite tight retry loop.
- Preserve battery.

Bluetooth:

- Often used for pairing/provisioning/control.
- Authenticate privileged commands.
- Avoid exposing factory/debug commands in production.
- Treat BLE writes as untrusted input.

DNS/TLS:

- Cache carefully but respect TTL where appropriate.
- Validate certificates.
- Handle clock-not-set problem for TLS validation.

---

## 7. Performance Constraints

Camera device constraints:

- CPU budget.
- Memory budget.
- Storage bandwidth.
- Battery.
- Thermal envelope.
- Boot time.
- Network bandwidth.

Common performance bugs:

- Unbounded queues during poor network.
- Excessive dynamic allocation in capture path.
- Logging too much in hot path.
- Synchronous upload blocking capture.
- Copying video buffers too many times.
- No backpressure from storage writer.

Optimization ideas:

- Pre-allocate buffers.
- Use ring buffers.
- Use zero-copy or move-only buffer ownership.
- Batch metadata writes where safe.
- Separate hot and cold paths.
- Profile before optimizing.

---

## 8. Observability and Field Debugging

Device logs should answer:

- Did recording start/stop?
- Why did upload fail?
- Was storage full?
- Did network disconnect?
- Did system reboot? Why?
- Was the device thermally throttled?
- What firmware version and config were active?

Privacy-preserving log rules:

- Do not log raw video/audio.
- Do not log tokens, keys, credentials.
- Redact user identifiers where possible.
- Use event IDs and hashes instead of sensitive payloads.

Useful metrics:

- Recording start/stop count.
- Dropped frames.
- Encoder queue depth.
- Storage free space.
- Upload throughput.
- Retry count.
- Battery/temperature.
- Crash/reboot reason.

---

## 9. Design Prompt: Body Camera Offline Upload

Prompt:

> Design firmware for a body camera that records video during a full shift, stores evidence locally, and uploads automatically when docked or connected to Wi-Fi.

Answer outline:

1. Capture pipeline writes to local durable storage.
2. Metadata includes device ID, timestamp, serial/user assignment, version.
3. Per-segment hash verifies integrity.
4. Upload service scans committed files and uploads chunks.
5. Use idempotency keys for retry/resume.
6. Delete only after cloud confirms complete evidence.
7. Encrypt at rest and in transit.
8. Maintain audit/security logs without leaking sensitive data.
9. OTA and config changes are signed/versioned.
10. Test power loss, Wi-Fi drop, disk full, corrupted chunk, clock drift.

---

## 10. Design Prompt: Live Streaming

Prompt:

> Add live streaming to a camera device while preserving local evidence recording.

Key answer:

- Local recording remains priority.
- Live stream is best-effort if bandwidth/thermal permits.
- Use separate queues for local record and live stream.
- Apply adaptive bitrate.
- Drop live frames before dropping local recording.
- Monitor battery/temperature.
- Secure stream with TLS/SRTP/WebRTC-like design depending product stack.
- Ensure user/privacy policy controls when live streaming is allowed.

Trade-off:

> The product may value local evidence completeness over live stream smoothness. State that priority explicitly.
