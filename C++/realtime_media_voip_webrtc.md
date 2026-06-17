# Real-Time Media — VoIP, RTP, WebRTC, Streaming

**Nice-to-have** cho JD kiểu VinSmart Future (call, super-app realtime). Không thay thế nền tảng C++/networking — bổ sung domain media.

**Prerequisites:** [networking.md](networking.md) (TCP/UDP), [low_latency.md](low_latency.md) (jitter), [multithreading_and_concurrency.md](multithreading_and_concurrency.md).

---

## 1. End-to-end audio (VoIP)

```
Microphone → ADC → PCM (16-bit linear)
    → Encoder (Opus / G.711 / AAC)
    → RTP packetizer → UDP (+ optional RTCP)
    → Network
    → Jitter buffer → Decoder → PCM → Speaker
```

| Codec | Bitrate | Latency | Ghi chú |
|-------|---------|---------|---------|
| **Opus** | 6–510 kbps | Thấp, adaptive | WebRTC default, FEC built-in |
| **G.711** | 64 kbps | Rất thấp | PSTN bridge, không nén |
| **AAC** | Variable | Cao hơn Opus | Streaming VOD hơn là call |

**Frame size:** thường 10–20 ms audio/frame — trade-off latency vs overhead header.

---

## 2. RTP / RTCP

### RTP header (simplified)

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|V=2|P|X|  CC   |M|     PT      |       sequence number         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                           timestamp                           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|           synchronization source (SSRC) identifier          |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                         payload (codec data)                  |
```

- **Sequence:** phát hiện mất gói, reorder  
- **Timestamp:** clock rate codec-dependent (Opus often 48 kHz)  
- **SSRC:** identify one media stream  
- **PT (payload type):** map tới codec (dynamic 96–127 trong SDP)

### RTCP

- **SR/RR:** statistics — loss, jitter, RTT  
- **SDES:** metadata (CNAME)  
- Dùng cho **congestion control**, sync lip-sync (video+audio)

**UDP:** không reliable — app/WebRTC layer xử lý loss (NACK, FEC, PLC).

---

## 3. Jitter buffer & packet loss

**Jitter:** biến thiên delay giữa các gói (network queuing).

```
Arrival times:  |--20ms--|--45ms--|--18ms--|--60ms--|
Playout (fixed):|----20ms----|----20ms----|----20ms----|
Jitter buffer:  absorb variance → steady playout clock
```

| Strategy | Mô tả |
|----------|--------|
| **Adaptive buffer** | Tăng buffer khi jitter cao → tăng latency |
| **Fixed buffer** | Đơn giản, dễ tune embedded |
| **PLC** | Packet Loss Concealment — synthesize missing frame |
| **FEC** | Redundant packets (RFC 5109, Opus in-band) |
| **NACK + retransmit** | WebRTC — chỉ khi RTT đủ thấp |

**C++ concern:** jitter buffer chạy trên thread riêng; lock-free SPSC giữa network thread và decode thread — xem [low_latency.md](low_latency.md).

---

## 4. WebRTC architecture

```
┌─────────────┐     Signaling (WebSocket/HTTP)      ┌─────────────┐
│  Client A   │◀──── SDP offer/answer, ICE ────────▶│  Client B   │
└──────┬──────┘                                     └──────┬──────┘
       │                                                    │
       │    STUN: discover public reflexive address         │
       │    TURN: relay when P2P fails (symmetric NAT)      │
       │                                                    │
       └──────────▶ Media path: SRTP (UDP) ◀───────────────┘
                    (P2P or via SFU/MCU)
```

| Component | Vai trò |
|-----------|---------|
| **Signaling** | Trao đổi SDP, ICE candidates — **không** chở media |
| **ICE** | Chọn cặp địa chỉ tốt nhất (host, srflx, relay) |
| **DTLS** | Handshake, fingerprint trong SDP |
| **SRTP** | Encrypt RTP payload |
| **DataChannel** | SCTP over DTLS — chat/file game state |

### SDP (Session Description Protocol)

- Mô tả codecs, ports, ICE ufrag/pwd  
- Offer/Answer model — negotiator quyết định intersection codecs  

### NAT traversal

| Type | P2P direct? |
|------|-------------|
| Full cone | Thường OK |
| Symmetric NAT | Thường cần **TURN** |

---

## 5. SFU vs MCU vs Mesh

| Model | Mô tả | Scale |
|-------|--------|-------|
| **Mesh** | Mỗi client gửi N-1 streams | O(N²) bandwidth — chỉ nhóm nhỏ |
| **MCU** | Server mix audio/video → 1 stream/user | CPU nặng, latency mix |
| **SFU** | Server **forward** từng stream, client subscribe | Zoom/Meet pattern — C++ media server phổ biến |

**SFU C++ stacks (tham khảo):** Janus, mediasoup, LiveKit (Go SFU + C++ clients), custom trên **libwebrtc** hoặc **GStreamer**.

---

## 6. Video streaming (outline)

```
Capture → encode (H.264/AV1/VP9) → mux (optional) → RTP or RTMP/HLS/DASH
```

| Protocol | Transport | Latency |
|----------|-----------|---------|
| **WebRTC** | UDP/SRTP | Thấp (interactive) |
| **RTMP** | TCP | Trung bình (legacy ingest) |
| **HLS/DASH** | HTTP segments | Cao (10–30s+) — CDN VOD/live |

**ABR (Adaptive Bitrate):** client chọn layer theo bandwidth — logic ở player, không thuộc raw C++ socket.

---

## 7. C++ implementation landscape

| Library | Use case |
|---------|----------|
| **libwebrtc** | Full stack client (Google) — nặng, build phức tạp |
| **PJSIP** | VoIP/SIP endpoints, embedded-friendly |
| **GStreamer** | Pipeline media (plugins), server transcode |
| **FFmpeg** | Decode/encode, không signaling |
| **Boost.Beast** | WebSocket signaling server |
| **Custom** | RTP depacketize + Opus decode — full control, high effort |

**Threading model điển hình:**

```
Thread 1: epoll — UDP RTP in/out
Thread 2: jitter buffer + decode
Thread 3: encode + capture (or callback from OS)
UI thread: không block — post tasks to media thread
```

---

## 8. Performance checklist (media server)

- [ ] Zero-copy where possible (`recvmsg`, pinned buffers)  
- [ ] Per-core UDP sockets (`SO_REUSEPORT`)  
- [ ] Pre-allocated packet pools — no `malloc` on hot path  
- [ ] Separate control (signaling) và media plane  
- [ ] Measure **end-to-end mouth-to-ear** latency, not chỉ CPU  
- [ ] Rate limit signaling; validate SDP size (DoS)  

---

## 9. Interview Q&As

**[Mid]** Tại sao VoIP thường dùng UDP thay vì TCP?

> TCP retransmit và ordering gây **delay biến thiên** — gói cũ có thể đến muộn vô dụng cho realtime audio (tốt hơn bỏ qua và PLC). UDP cho phép kiểm soát loss/latency ở application layer (jitter buffer, codec FEC). TCP phù hợp signaling, không phù hợp strict low-latency media.

---

**[Mid]** Jitter buffer làm gì? Trade-off?

> Làm mịn thời điểm playout khi delay mạng dao động. Buffer lớn → ít underrun, **latency cao hơn**. Adaptive buffer tăng khi mạng xấu. Tune theo use case: call interactive ~50–150 ms target; streaming chấp nhận hơn.

---

**[Senior]** Giải thích ICE gathering và tại sao cần TURN.

> Client thu thập **candidates**: host (local IP), server reflexive (qua STUN), relay (TURN). ICE connectivity checks (STUN binding) chọn cặp hoạt động. Symmetric NAT map port khác nhau mỗi destination → P2P thường fail → media relay qua TURN (tốn băng thông server, nhưng reliable).

---

**[Senior]** Thiết kế SFU nhận 500 video publishers, 10k subscribers — bottleneck?

> Ingress: 500 × bitrate UDP → SFU forward selective (simulcast/SVC). CPU: không decode full nếu chỉ forward RTP. RAM: state per subscriber layer. Network: outbound fan-out — cần NIC bandwidth và possibly regional SFU. C++: thread per core + epoll, packet forwarding without copy, subscription map sharded by hash.

---

## 10. Cross-references

| Topic | File |
|-------|------|
| WebSocket signaling | [networking.md](networking.md) § WebSocket |
| TCP vs UDP | [networking.md](networking.md) § TCP vs UDP |
| Chat system design | [system_design.md](system_design.md) § Chat |
| Lock-free, latency | [low_latency.md](low_latency.md) |
| VSF prep | [../vsf_senior_cpp_vinsmart_future.md](../vsf_senior_cpp_vinsmart_future.md) |
