# gRPC C++ Hello Example

Minimal gRPC C++ example with:

| File | Purpose |
|------|---------|
| [greeter.proto](greeter.proto) | Service contract and messages |
| [greeter_server.cpp](greeter_server.cpp) | Sync gRPC server implementation |
| [greeter_client.cpp](greeter_client.cpp) | Sync gRPC client for unary + server-streaming calls |
| [CMakeLists.txt](CMakeLists.txt) | Generates protobuf/gRPC code and builds both binaries |

## RPC Shapes

```proto
rpc SayHello(HelloRequest) returns (HelloReply);
rpc WatchGreetings(HelloRequest) returns (stream HelloReply);
```

- `SayHello`: unary request/response.
- `WatchGreetings`: server-streaming example; the server writes three replies for one request.

## Install Dependencies

### Windows with vcpkg

```powershell
vcpkg install grpc protobuf
```

Configure with your vcpkg toolchain file:

```powershell
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="<path-to-vcpkg>\scripts\buildsystems\vcpkg.cmake"
cmake --build build --config Release
```

### Linux / macOS

Install `grpc`, `protobuf`, `protoc`, and the gRPC C++ plugin with your package manager or vcpkg/conan, then:

```bash
cmake -S . -B build
cmake --build build
```

## Run

Start the server:

```powershell
.\build\Release\greeter_server.exe
```

In another terminal, run the client:

```powershell
.\build\Release\greeter_client.exe localhost:50051 Duy
```

Expected output:

```text
Unary reply: Hello, Duy! (request_id=1001)
Stream reply: Hello #1, Duy! (request_id=1002)
Stream reply: Hello #2, Duy! (request_id=1002)
Stream reply: Hello #3, Duy! (request_id=1002)
```

## Interview Notes

- gRPC C++ generated code gives the client a `Greeter::Stub` and the server a `Greeter::Service` base class.
- `ClientContext` / `ServerContext` carry metadata, deadline, cancellation, and auth-related state.
- Deadlines should be set by clients so failed or slow services do not block forever.
- Production deployments should use TLS credentials instead of `Insecure*Credentials`.
- For high concurrency, compare sync API vs async API / callback API based on latency, throughput, and code complexity.
