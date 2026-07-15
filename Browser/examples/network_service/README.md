# Network Service Example

This example is a small C++ learning project that models the shape of a browser
Network Service.

It is not Chromium code. It is a simplified interview-friendly implementation
showing how a browser-like network layer can:

- build an HTTP request,
- use TCP/IP as the transport,
- use TLS for encryption,
- send and receive the request asynchronously,
- dispatch work through a singleton thread pool,
- centralize request/response logic in a `NetworkManager`.

## Architecture

```text
main.cpp
  |
  v
HttpRequestBuilder
  |  method(), host(), port(), target(), header(), body(), build()
  v
NetworkManager
  |  sendAsync()
  |  handleResponse()
  |
  v
ThreadPool::instance()
  |
  v
worker thread
  |
  v
TlsHttpClient
  |
  +--> DNS lookup: getaddrinfo()
  +--> TCP connect: socket() + connect()
  +--> TLS handshake: OpenSSL SSL_connect()
  +--> HTTP request: SSL_write()
  +--> HTTP response: SSL_read()
```

Browser analogy:

```text
Renderer Process
  |
  | Mojo URLLoaderFactory request
  v
Network Service
  |
  | HTTP + TLS
  v
TCP/IP
  |
  v
Remote server
```

In this example, `NetworkManager` plays the role of the high-level network
service API, while `TlsHttpClient` plays the role of the lower-level loader that
owns TCP/TLS details.

## Files

| File | Role |
| --- | --- |
| `main.cpp` | Demo entry point |
| `network_manager.h/.cpp` | Schedules async send and handles response |
| `tls_http_client.h/.cpp` | TCP connect, TLS handshake, HTTP/1.1 send/receive |
| `thread_pool.h/.cpp` | Singleton worker pool |
| `http_types.h` | Simple `HttpRequest` / `HttpResponse` data models and `HttpRequestBuilder` |
| `CMakeLists.txt` | Build file |

## Build

Install dependencies on Ubuntu:

```bash
sudo apt update
sudo apt install -y build-essential cmake libssl-dev
```

Build:

```bash
cmake -S . -B build
cmake --build build -j
```

## Run

Default request:

```bash
./build/network_service_demo
```

Request a specific host and path:

```bash
./build/network_service_demo GET example.com /
./build/network_service_demo GET www.example.com / 443
./build/network_service_demo POST httpbin.org /post 443 '{"hello":"world"}'
```

Expected output shape:

```text
HTTP status: 200 OK
Content-Type: text/html
Body bytes: ...
Body preview:
...
```

## Interview Talking Points

### Why Builder pattern?

`HttpRequestBuilder` avoids hard-coding a single GET request shape. The caller can
compose a request step by step:

```cpp
HttpRequest request = HttpRequestBuilder()
    .method("POST")
    .host("api.example.com")
    .port("443")
    .target("/v1/events")
    .header("Accept", "application/json")
    .header("Content-Type", "application/json")
    .body(R"({"event":"page_view"})")
    .build();
```

This is useful for browser-style network code because resource loads are not all
the same. A page may trigger GET, POST, custom headers, different paths, or
different ports.

### Why use a thread pool?

Networking can block on DNS, TCP connect, TLS handshake, server response, or slow
remote peers. A thread pool keeps the caller thread free and allows multiple
requests to run concurrently.

In Chromium, the architecture is more advanced: async IO, task runners, Mojo
data pipes, Network Service, URLLoader, cache, cookies, redirects and security
policy. But the separation of roles is similar:

- API layer builds request.
- Worker/service layer performs network work.
- Response handling is centralized.
- Callers do not directly own raw sockets.

### Why TLS?

TLS encrypts HTTP bytes between client and server:

```text
HTTP request
  |
  v
TLS encryption
  |
  v
TCP/IP transport
```

The server still receives normal HTTP after decrypting TLS on its side.

### Why `NetworkManager`?

`NetworkManager` is the facade. It hides lower-level details from callers:

- how to format HTTP,
- how to connect TCP,
- how to configure TLS,
- which thread executes the request,
- how to handle the response.

This mirrors browser architecture where renderer code asks for a resource
through an API instead of directly opening sockets.

### Why singleton thread pool?

The singleton keeps one shared worker pool for the process. It avoids creating a
new set of threads per request and makes it clear that request execution is a
shared resource.

In production code, dependency injection is often better than a singleton for
testability. Here the singleton is intentional because the example demonstrates
the pattern explicitly.

