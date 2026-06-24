# Axon Company Context

## Mission

Axon's mission is **Protect Life**. The JD also frames the role around building secure, privacy-preserving camera systems for public safety customers and first responders. Axon's homepage describes the company as building a public safety operating system and states a mission to reduce gun-related deaths between police and the public by 50% by 2033 through technology, training, and trust ([Axon homepage](https://www.axon.com/)).

Interview positioning:

> "I understand Axon is not just a device company. The embedded camera, cloud evidence platform, mobile/desktop apps, and backend services form a safety-critical and privacy-sensitive ecosystem. Device software must be reliable, secure, auditable, and respectful of customer workflows."

---

## Product Ecosystem

Axon's product catalog includes:

- **Cameras and sensors**: Axon Body 4, Axon Body 3, Axon Body 2, Axon Body Workforce, Axon Fleet 3, Axon Signal.
- **Digital evidence management**: Axon Evidence, Axon Justice, Axon Capture, redaction and transcription tools.
- **Real-time operations**: Axon Respond, Fusus.
- **TASER energy weapons**: TASER 10, TASER 7, TASER X2, TASER X26P.
- **AI and productivity products**: transcription, report drafting, translation, policy assistance.

Source: [Axon product catalog](https://www.axon.com/products)

What this means for embedded interviews:

- Camera firmware is part of a larger connected ecosystem.
- Device features may touch cloud upload, evidence metadata, user assignment, live streaming, Bluetooth/Wi-Fi, battery, storage, and security.
- Engineering trade-offs must consider officer/customer workflow, legal evidence integrity, privacy, and operational reliability.

---

## Axon Evidence Context

Axon Evidence is described as a secure, cloud-based evidence management system for capturing, reviewing, disclosing, and auditing digital evidence. Axon emphasizes:

- File integrity via unique digital fingerprints.
- Chain of custody via detailed audit trails.
- Encryption in transit and at rest.
- Role-based permissions.
- Compliance and security controls.

Source: [Axon Evidence](https://www.axon.com/products/axon-evidence)

Embedded connection:

> A camera device is the source of evidence. Firmware must preserve integrity from capture through upload: correct timestamps, metadata, encryption, crash-safe local storage, reliable upload, and no unauthorized deletion.

---

## Body-Worn Camera Accountability Concepts

Axon's resource on body-worn cameras and Axon Evidence highlights these ideas:

- Video frames include timestamp and camera serial number.
- Camera start/stop activity is logged.
- Footage is encrypted at rest and in transit.
- Auto-upload behavior protects against loss.
- Audit trails preserve chain of custody.
- Agencies control retention and permissions, while Axon provides secure tools and safeguards.
- Security/compliance includes integrity checks, SHA-2 checksums, ISO/IEC 27001, SOC 2, CSA STAR, and FedRAMP High authorization for SaaS solutions.

Source: [How Axon Evidence and body-worn camera systems work](https://www.axon.com/resources/how-axon-evidence-and-body-worn-camera-systems-work)

Interview implications:

- Know how to discuss **evidence integrity**.
- Know how to discuss **auditability**.
- Know how to discuss **privacy-preserving device behavior**.
- Know how to design for **offline capture and later upload**.
- Know how to reason about **retention, deletion, and immutable logs**.

---

## Role-Specific Company Signals

From the JD, Axon wants someone who can:

- Develop features for camera products from inception through release.
- Build secure, privacy-preserving device software.
- Coordinate with desktop, mobile, and backend teams.
- Make system design trade-offs in a "bullet-proof" system.
- Troubleshoot technical issues.
- Resolve ambiguous/conflicting requirements.
- Mentor and raise engineering standards.

Source: [Axon Embedded Software Engineer I JD](https://www.axon.com/careers/job/4958517003)

How to speak to this:

- Emphasize reliability and ownership.
- Explain trade-offs, not just APIs.
- Show comfort crossing device/cloud/mobile boundaries.
- Mention testing strategy, observability, and failure recovery.
- Treat privacy/security as product requirements, not afterthoughts.

---

## Questions to Ask Axon

- What camera product or subsystem would this role focus on first?
- Is the device stack closer to embedded Linux, RTOS, or both?
- What are the biggest reliability problems today: battery, storage, upload, thermal, boot time, connectivity?
- How are device logs collected and correlated with cloud/backend logs?
- What testing infrastructure exists: hardware-in-loop, camera farms, CI, automated field scenario tests?
- How does the team handle secure boot, OTA, rollback, and device provisioning?
- How does the team balance privacy requirements with debugging and observability?
