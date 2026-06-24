# Computer Networking

## 1. Network Stack

Practical view:

```text
Application
    |
Socket API
    |
TCP / UDP
    |
IP
    |
Link Layer
    |
Physical Network
```

Meaning:

- Application calls OS socket APIs.
- TCP/UDP provide transport behavior.
- IP routes packets between machines.
- Link layer moves frames on Ethernet/Wi-Fi.

---

## 2. TCP vs UDP

| Feature | TCP | UDP |
|---------|-----|-----|
| Connection | Connection-oriented | Connectionless |
| Reliability | Retransmission, ordering | Best effort |
| Ordering | Guaranteed byte order | Not guaranteed |
| Flow control | Yes | No |
| Congestion control | Yes | No |
| Latency | Higher under loss | Lower, but loss visible |
| Typical use | HTTP, DB, SSH | DNS, VoIP, games, telemetry |

Interview answer:

> TCP gives reliable ordered byte streams. UDP gives datagrams with less overhead but no delivery guarantee, so the application must decide what reliability means.

---

## 3. Socket Lifecycle

TCP server:

```text
socket()
bind()
listen()
accept()
read()/write()
close()
```

TCP client:

```text
socket()
connect()
read()/write()
close()
```

UDP:

```text
socket()
bind() optional
sendto()/recvfrom()
close()
```

---

## 4. DNS

DNS maps names to addresses.

Common records:

- `A`: IPv4 address.
- `AAAA`: IPv6 address.
- `CNAME`: alias.
- `MX`: mail server.
- `TXT`: metadata and verification.

Production notes:

- DNS has caching via TTL.
- DNS failures can look like application outages.
- Short TTL improves failover but increases query load.

---

## 5. HTTP and TLS

HTTP:

- Request/response protocol.
- Runs commonly over TCP.
- HTTP/2 multiplexes streams over one TCP connection.
- HTTP/3 runs over QUIC/UDP.

TLS:

- Provides encryption, integrity, and server authentication.
- Adds handshake cost.
- Requires certificate validation.

---

## 6. Latency

Latency components:

- DNS lookup.
- TCP handshake.
- TLS handshake.
- Server queueing.
- Application processing.
- Network propagation.
- Retransmissions.

Useful terms:

- RTT: round-trip time.
- Throughput: data per second.
- Tail latency: p95/p99 slow requests.
- Head-of-line blocking: later data waits behind earlier lost data.

---

## Interview Questions

- What happens when a client connects to a TCP server?
- Why does TCP have head-of-line blocking?
- When would you use UDP?
- What is DNS TTL?
- What does TLS protect?
- Why is p99 latency more important than average latency in production?
