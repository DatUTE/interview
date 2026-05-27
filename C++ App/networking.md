# Networking Concepts

## OSI vs TCP/IP Model

```
OSI (7 layers)              TCP/IP (4 layers)       Examples
┌─────────────────┐         ┌──────────────────┐
│  7. Application │         │                  │     HTTP, FTP, DNS, SMTP
│  6. Presentation│ ───────▶│   Application    │     TLS, MIME encoding
│  5. Session     │         │                  │     RPC sessions
└─────────────────┘         └──────────────────┘
┌─────────────────┐         ┌──────────────────┐
│  4. Transport   │ ───────▶│   Transport      │     TCP, UDP, QUIC
└─────────────────┘         └──────────────────┘
┌─────────────────┐         ┌──────────────────┐
│  3. Network     │ ───────▶│   Internet       │     IP, ICMP, BGP
└─────────────────┘         └──────────────────┘
┌─────────────────┐         ┌──────────────────┐
│  2. Data Link   │ ───────▶│   Network Access │     Ethernet, WiFi, ARP
│  1. Physical    │         │                  │     Cables, signals
└─────────────────┘         └──────────────────┘
```

---

## 1. TCP vs UDP

| Property | TCP | UDP |
|---|---|---|
| Connection | Connection-oriented (3-way handshake) | Connectionless |
| Reliability | Guaranteed delivery + ordering | Best-effort, no guarantee |
| Error checking | Checksum + retransmission | Checksum only (optional drop) |
| Flow control | Yes (receive window) | No |
| Congestion control | Yes (CWND, slow start, AIMD) | No |
| Header size | 20–60 bytes | 8 bytes |
| Latency | Higher (ACKs, retransmit) | Lower |
| Throughput | Lower (overhead) | Higher |
| Use cases | HTTP, FTP, SSH, databases | DNS, DHCP, video stream, gaming |

---

## 2. TCP Three-Way Handshake & Teardown

### Connection Setup

```
Client                          Server
  │                               │
  │──────── SYN (seq=x) ─────────▶│  Client: SYN_SENT
  │                               │  Server: SYN_RCVD
  │◀── SYN-ACK (seq=y, ack=x+1) ──│
  │                               │
  │──────── ACK (ack=y+1) ────────▶│  Both: ESTABLISHED
  │                               │
  │════════ Data transfer ════════ │
```

**SYN flood attack**: attacker sends many SYN without completing. Server allocates state per half-open connection. **Mitigation**: SYN cookies — server encodes state into the ISN (initial sequence number) so no per-connection state until ACK.

### Connection Teardown (4-way)

```
Client (active close)           Server
  │                               │
  │──────── FIN ─────────────────▶│  Client: FIN_WAIT_1
  │◀──────── ACK ─────────────────│  Client: FIN_WAIT_2
  │                               │  (server may still send data)
  │◀──────── FIN ─────────────────│  Server: LAST_ACK
  │──────── ACK ─────────────────▶│
  │                               │  Client: TIME_WAIT (2×MSL ≈ 60s)
  │                               │  Server: CLOSED
```

**TIME_WAIT** purpose:
1. Ensure final ACK was received (if ACK lost, server retransmits FIN)
2. Prevent stale packets from previous connection being misread by new connection on same port

`SO_REUSEADDR` allows binding to port in TIME_WAIT (for server restart).

---

## 3. TCP Flow Control & Congestion Control

### Flow Control — Receive Window

Prevents sender from overwhelming receiver's buffer.

```
Receiver advertises rwnd in each ACK header (16-bit field → up to 65535 bytes, or scaled with window scaling option)
Sender may send: min(cwnd, rwnd) bytes without ACK
```

### Congestion Control

```
Slow Start:
cwnd = 1 MSS
each ACK → cwnd += 1 MSS   (exponential growth)
cwnd reaches ssthresh → enter Congestion Avoidance

Congestion Avoidance (AIMD — Additive Increase Multiplicative Decrease):
each RTT → cwnd += 1 MSS   (linear growth)
packet loss (timeout) → ssthresh = cwnd/2, cwnd = 1 (back to slow start)
packet loss (3 dup ACK) → ssthresh = cwnd/2, cwnd = ssthresh (fast retransmit/recovery)

TCP Cubic (Linux default): uses cubic function of time since last loss event
TCP BBR (Google): model-based — measures bandwidth & RTT directly
```

---

## 4. Socket Programming

### Server Setup

```cpp
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

int create_tcp_server(uint16_t port) {
    // 1. Create socket
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); return -1; }

    // 2. Set socket options
    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    // SO_REUSEADDR: allow bind to TIME_WAIT port
    // SO_REUSEPORT: multiple sockets on same port (load balancing)

    // 3. Bind to address
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;   // 0.0.0.0
    addr.sin_port = htons(port);         // host-to-network byte order

    if (bind(fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind"); close(fd); return -1;
    }

    // 4. Listen (backlog = max pending connections in SYN queue)
    if (listen(fd, SOMAXCONN) < 0) {  // SOMAXCONN ≈ 128 typically
        perror("listen"); close(fd); return -1;
    }

    return fd;
}

void accept_loop(int server_fd) {
    while (true) {
        sockaddr_in client_addr{};
        socklen_t addrlen = sizeof(client_addr);

        // 5. Accept — blocks until new connection
        int conn_fd = accept(server_fd, (sockaddr*)&client_addr, &addrlen);
        if (conn_fd < 0) { perror("accept"); continue; }

        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip));
        printf("Connected: %s:%d\n", ip, ntohs(client_addr.sin_port));

        // Handle in thread or epoll (see below)
        handle_connection(conn_fd);
        close(conn_fd);
    }
}
```

### Client Connection

```cpp
int connect_tcp(const char* host, uint16_t port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);
    inet_pton(AF_INET, host, &addr.sin_addr);   // "192.168.1.1" → binary

    if (connect(fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect"); close(fd); return -1;
    }
    return fd;
}
```

### Send / Receive

```cpp
// Reliable send (handles partial writes)
ssize_t send_all(int fd, const void* buf, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(fd, (const char*)buf + sent, len - sent, MSG_NOSIGNAL);
        if (n < 0) return -1;  // error
        if (n == 0) return sent;  // connection closed
        sent += n;
    }
    return sent;
}

// Reliable recv (read exactly n bytes)
ssize_t recv_all(int fd, void* buf, size_t len) {
    size_t got = 0;
    while (got < len) {
        ssize_t n = recv(fd, (char*)buf + got, len - got, 0);
        if (n < 0) return -1;
        if (n == 0) return got;  // peer closed
        got += n;
    }
    return got;
}

// MSG_NOSIGNAL: suppress SIGPIPE on broken pipe (use SO_NOSIGPIPE on macOS)
```

### UDP Socket

```cpp
int fd = socket(AF_INET, SOCK_DGRAM, 0);  // SOCK_DGRAM for UDP

// Sender (no connect needed)
sendto(fd, buf, len, 0, (sockaddr*)&dest, sizeof(dest));

// Receiver
sockaddr_in src;
socklen_t src_len = sizeof(src);
recvfrom(fd, buf, sizeof(buf), 0, (sockaddr*)&src, &src_len);

// OR connect() to remember default destination
connect(fd, (sockaddr*)&dest, sizeof(dest));  // not a real connection
send(fd, buf, len, 0);   // now works like sendto with remembered dest
```

---

## 5. Non-blocking Sockets & `epoll`

```cpp
// Make socket non-blocking
int flags = fcntl(fd, F_GETFL, 0);
fcntl(fd, F_SETFL, flags | O_NONBLOCK);

// OR at creation (Linux 2.6.27+)
int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);

// epoll-based event loop (handles 10k+ connections efficiently)
struct EpollServer {
    int epfd_;
    int server_fd_;
    static const int MAX_EVENTS = 64;

    void run(uint16_t port) {
        server_fd_ = create_tcp_server(port);
        // Make non-blocking
        fcntl(server_fd_, F_SETFL, O_NONBLOCK);

        epfd_ = epoll_create1(EPOLL_CLOEXEC);

        // Add server socket for incoming connections
        epoll_event ev{};
        ev.events  = EPOLLIN;
        ev.data.fd = server_fd_;
        epoll_ctl(epfd_, EPOLL_CTL_ADD, server_fd_, &ev);

        epoll_event events[MAX_EVENTS];
        while (true) {
            int n = epoll_wait(epfd_, events, MAX_EVENTS, -1); // -1 = block forever
            for (int i = 0; i < n; ++i) {
                int fd = events[i].data.fd;
                if (fd == server_fd_) {
                    accept_new_connection();
                } else {
                    handle_client_data(fd, events[i].events);
                }
            }
        }
    }

    void accept_new_connection() {
        while (true) {
            int conn = accept4(server_fd_, nullptr, nullptr, SOCK_NONBLOCK);
            if (conn < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) break; // all accepted
                perror("accept"); break;
            }
            // Register with epoll, edge-triggered
            epoll_event ev{};
            ev.events  = EPOLLIN | EPOLLET;  // edge-triggered
            ev.data.fd = conn;
            epoll_ctl(epfd_, EPOLL_CTL_ADD, conn, &ev);
        }
    }

    void handle_client_data(int fd, uint32_t events) {
        if (events & EPOLLIN) {
            // Edge-triggered: must drain completely
            char buf[4096];
            while (true) {
                ssize_t n = read(fd, buf, sizeof(buf));
                if (n < 0) {
                    if (errno == EAGAIN) break;  // drained
                    close_connection(fd); return;
                }
                if (n == 0) { close_connection(fd); return; }
                // process buf[0..n)
            }
        }
        if (events & (EPOLLERR | EPOLLHUP)) close_connection(fd);
    }

    void close_connection(int fd) {
        epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, nullptr);
        close(fd);
    }
};
```

---

## 6. HTTP/1.1 vs HTTP/2 vs HTTP/3

| Feature | HTTP/1.1 | HTTP/2 | HTTP/3 |
|---|---|---|---|
| Transport | TCP | TCP | QUIC (UDP-based) |
| Multiplexing | No (pipelining broken) | Yes (streams) | Yes (streams) |
| Head-of-line blocking | Yes (per connection) | Yes (TCP level) | No (per stream) |
| Header compression | No | HPACK | QPACK |
| Server push | No | Yes | Yes |
| Encryption | Optional | Effectively required | Always (TLS 1.3) |
| Connection setup | 1 RTT + handshake | 1 RTT + handshake | 0–1 RTT |
| Adoption | Universal | ~60% | Growing (Chrome, cloudflare) |

### HTTP/1.1 Keep-Alive and Pipelining

```
Without Keep-Alive:
GET /page → TCP handshake → request → response → TCP close → (repeat)

With Keep-Alive (persistent connection):
GET /page → handshake → req1 → resp1 → req2 → resp2 → ... → close

Pipelining (requests without waiting for response):
→ req1 → req2 → req3    then    ← resp1 ← resp2 ← resp3
Problem: responses must come in order → HOL blocking if resp1 is slow
```

### HTTP/2 Multiplexing

```
Single TCP connection, multiple logical streams (numbered):
Stream 1: [HEADERS][DATA][DATA] ──▶
Stream 3:          [HEADERS][DATA] ──▶
Stream 5:                    [HEADERS] ──▶
                                         ◀── [DATA stream 3]
                                         ◀── [HEADERS stream 5]
                                         ◀── [DATA stream 1]

All interleaved in frames. No ordering requirement between streams.
But: a lost TCP segment blocks ALL streams (TCP-level HOL blocking)
```

### HTTP/3 / QUIC

- QUIC runs over UDP, implements its own reliability per stream
- Lost packet blocks only its stream, not others
- 0-RTT resumption: client sends data immediately on reconnect using cached session ticket
- Built-in TLS 1.3

---

## 7. TLS Handshake

### TLS 1.2 (Legacy — 2 RTTs)

```
Client                                    Server
  │                                          │
  │── ClientHello (TLS version, ciphers, ───▶│
  │   client_random, extensions)             │
  │                                          │
  │◀── ServerHello (chosen cipher,  ─────────│
  │    server_random, session_id)            │
  │◀── Certificate (server public key)       │
  │◀── ServerHelloDone ──────────────────────│
  │                                          │
  │── ClientKeyExchange ─────────────────────▶│
  │   (encrypted pre-master secret with      │
  │    server's public key)                  │
  │── ChangeCipherSpec ──────────────────────▶│
  │── Finished (HMAC of handshake) ──────────▶│
  │                                          │
  │◀── ChangeCipherSpec ─────────────────────│
  │◀── Finished ─────────────────────────────│
  │                                          │
  │══════════ Encrypted Application Data ════│  (2 RTTs before data)
```

### TLS 1.3 (1 RTT + 0-RTT resumption)

```
Client                                    Server
  │                                          │
  │── ClientHello + key_share ──────────────▶│
  │   (DH public key in first message)       │
  │                                          │
  │◀── ServerHello + key_share ──────────────│
  │◀── EncryptedExtensions ──────────────────│
  │◀── Certificate ──────────────────────────│
  │◀── CertificateVerify (signed) ───────────│
  │◀── Finished ─────────────────────────────│
  │                                          │
  │── Finished ─────────────────────────────▶│
  │══════════ Encrypted Data ════════════════│  (1 RTT)

0-RTT (Session Resumption):
Client sends early data in first flight using PSK from previous session
Server can accept early data (replay risk — only for idempotent requests)
```

### Key Concepts

| Concept | Explanation |
|---|---|
| Certificate | Server identity proof signed by CA |
| CA chain | Root CA → Intermediate CA → Server cert |
| OCSP stapling | Server bundles cert revocation status |
| Perfect Forward Secrecy | Ephemeral DH keys → past sessions safe if long-term key leaked |
| ALPN extension | Client/server negotiate application protocol (h2, http/1.1) |
| SNI extension | Client sends hostname so server picks correct cert |
| mTLS | Both client and server authenticate with certs |

---

## 8. DNS Resolution

```
Browser requests www.example.com:
  1. Check local cache (browser, OS)
  2. Ask recursive resolver (ISP or 8.8.8.8)
  3. Resolver asks Root nameserver → "try .com TLD"
  4. Resolver asks .com TLD → "try example.com NS"
  5. Resolver asks example.com authoritative NS → "93.184.216.34"
  6. Resolver caches + returns to client (TTL-respecting)

Record types:
  A      → IPv4 address
  AAAA   → IPv6 address
  CNAME  → alias to another domain
  MX     → mail exchange
  TXT    → arbitrary text (SPF, DKIM, verification)
  NS     → nameserver for domain
  PTR    → reverse DNS (IP → hostname)
  SRV    → service location (used by SIP, XMPP)
```

---

## 9. Load Balancing Strategies

| Strategy | Description | Use Case |
|---|---|---|
| Round Robin | Requests rotate across servers | Stateless, equal capacity |
| Least Connections | Route to server with fewest active | Variable request duration |
| IP Hash | Hash client IP → consistent server | Session affinity (sticky sessions) |
| Weighted Round Robin | Proportional distribution | Mixed server capacity |
| Random | Random selection | Simple, scales well |
| Consistent Hashing | Minimal remapping on server add/remove | Distributed cache (Memcached, Redis) |
| L4 Load Balancing | Based on TCP/UDP (IP+port) | Fast, no HTTP parsing (nginx stream) |
| L7 Load Balancing | Based on HTTP content (URL, headers, cookies) | Smart routing, A/B testing |

---

## 10. Important Socket Options

```cpp
// Enable address reuse (server restart without TIME_WAIT wait)
setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

// Multiple sockets on same port (multi-process/thread servers, Linux 3.9+)
setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

// TCP keepalive (detect dead connections)
setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt));
int idle = 60, interval = 10, count = 5;
setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE,  &idle,     sizeof(idle));
setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(interval));
setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT,   &count,    sizeof(count));

// Disable Nagle's algorithm (send small packets immediately — reduce latency)
int nodelay = 1;
setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));
// Nagle: buffers small writes until ACK arrives or buffer full
// Use for: interactive apps (SSH, gaming). Don't use for bulk transfer.

// Receive/send buffer sizes
int rcvbuf = 256*1024;
setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));

// Linger — wait for data to be sent on close
struct linger lg = {.l_onoff = 1, .l_linger = 5};
setsockopt(fd, SOL_SOCKET, SO_LINGER, &lg, sizeof(lg));

// Timeout
struct timeval tv = {.tv_sec = 5, .tv_usec = 0};
setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
```

---

## 11. Protocols at a Glance

### WebSocket

```
1. HTTP Upgrade handshake (GET + Upgrade: websocket header)
2. Server responds 101 Switching Protocols
3. Full-duplex frames over same TCP connection

Frame format: opcode (text/binary/ping/pong/close) + payload length + masking key + data
Use case: real-time bidirectional (chat, live dashboard, gaming)
```

### gRPC

```
Built on HTTP/2 + Protocol Buffers
- Service definition in .proto → generates client/server stubs
- Streaming: unary, server-stream, client-stream, bidirectional
- Binary encoding → smaller, faster than JSON/REST
- Built-in deadline/cancellation propagation
Use case: microservice RPC, mobile backends
```

### MQTT

```
Pub/sub over TCP (or WebSocket)
Lightweight: 2-byte fixed header
QoS levels: 0 (at-most-once), 1 (at-least-once), 2 (exactly-once)
Use case: IoT devices, embedded systems
```

---

## 12. Network Diagnostics

```bash
# Connection state
ss -tunap               # all TCP/UDP sockets with processes
netstat -anp            # similar, older tool
ss -s                   # socket statistics summary

# Port scan
nmap -p 80,443 host     # check open ports

# DNS lookup
dig example.com          # full DNS resolution
dig @8.8.8.8 example.com # use specific resolver
nslookup example.com

# HTTP testing
curl -v https://example.com                  # verbose headers
curl -w "%{time_connect} %{time_total}\n" url # timing

# Packet capture
tcpdump -i eth0 port 80 -w capture.pcap
wireshark capture.pcap

# Latency / routing
ping -c 5 host
traceroute host          # hops to destination (UDP probes)
mtr host                 # combined ping + traceroute (live)

# TCP connection timing
strace -e connect,sendto,recvfrom curl http://example.com

# Linux network stats
cat /proc/net/tcp        # all TCP connections raw
cat /proc/net/dev        # interface stats (bytes, errors, drops)
ethtool eth0             # interface capabilities
```

---

## Interview Q&As

**Q [Mid]: Explain the TCP three-way handshake. Why three steps and not two?**

Two steps would be sufficient for the client to know the server is alive, but insufficient for the **server to know the client received its SYN-ACK**. The server needs to know its side of the connection is usable before sending data. With three steps: (1) client sends SYN, (2) server sends SYN-ACK (proves server is alive and reachable), (3) client sends ACK (proves to server that its SYN-ACK arrived and the client's return path works). This establishes that both directions of communication are working and both sides have agreed on initial sequence numbers.

---

**Q [Mid]: What is the difference between `select`, `poll`, and `epoll`?**

All monitor multiple file descriptors for I/O readiness. `select` has a fixed `FD_SETSIZE` limit (1024 fds), passes all fds on every call (O(n) kernel scan), and must rebuild the set each call. `poll` removes the fd limit and uses a cleaner API but still does O(n) kernel scan per call. `epoll` is O(1) for wait — instead of scanning all fds, the kernel uses an IRQ-driven ready list and only returns ready fds. `epoll_ctl` adds/removes fds incrementally (no full set on each call). Edge-triggered mode reduces syscalls further by only notifying on state transitions. `epoll` is the right choice for high-connection-count servers (10k+).

---

**Q [Senior]: How does HTTP/2 multiplexing work and why does HTTP/3 exist?**

HTTP/2 multiplexes multiple logical streams over a single TCP connection — each HTTP request/response is a stream identified by a stream ID. Frames from different streams are interleaved, so a slow response doesn't block others (no application-level HOL blocking). However, TCP provides ordered byte delivery — if a single packet is lost, TCP blocks all streams until it's retransmitted (TCP-level HOL blocking). HTTP/3 replaces TCP with QUIC (UDP-based), which implements its own per-stream reliability. A lost packet only blocks its own stream. Additionally, QUIC integrates TLS 1.3 and supports 0-RTT connection resumption, reducing latency for reconnecting clients.

---

**Q [Senior]: Design a high-performance TCP server that handles 100,000 concurrent connections.**

Use the reactor pattern with `epoll` in edge-triggered mode on a single thread (or thread pool): one listening socket + one epoll instance. Accept all pending connections in a loop (`accept4` with `SOCK_NONBLOCK`). Register each fd with `EPOLLIN | EPOLLET`. On readiness, drain the fd completely until `EAGAIN`. For CPU-bound work, dispatch to a thread pool (never block the event loop). For multi-core: use `SO_REUSEPORT` to run N event-loop threads (one per core), each with its own epoll — the kernel load-balances `accept` across them. Set `TCP_NODELAY` for low latency, tune `SO_RCVBUF`/`SO_SNDBUF`, increase `ulimit -n` (file descriptor limit), and tune kernel params: `net.core.somaxconn`, `net.ipv4.tcp_max_syn_backlog`, `net.ipv4.tcp_tw_reuse`.

---

**Q [Senior]: What is TLS Perfect Forward Secrecy and why does it matter?**

With traditional RSA key exchange, the client encrypts a pre-master secret with the server's long-term RSA public key. If an attacker records encrypted traffic today and later obtains the server's private key (breach, legal order, side channel), they can decrypt all past sessions. PFS uses ephemeral Diffie-Hellman (ECDHE): a new DH key pair is generated for each session, the session key is derived, and the ephemeral keys are discarded after the handshake. Even if the server's long-term private key is compromised, past session keys cannot be reconstructed — each session's key material existed only transiently. TLS 1.3 mandates PFS by only supporting ECDHE key exchange (RSA key exchange removed entirely).
