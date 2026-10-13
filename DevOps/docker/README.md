# Docker Overview

Docker packages tools, dependencies, and environment settings into a container image.

In embedded development, Docker is often used to make C/C++ build environments reproducible across developer machines and CI runners.

## Why Docker Matters

Without Docker, two developers may have different:

- Compiler versions
- Python versions
- CMake versions
- Cross-compilation toolchains
- System libraries
- Yocto dependencies

With Docker, the team can define one build image and reuse it in local development and CI.

## Key Concepts

| Concept | Meaning |
|---|---|
| Image | Read-only template containing tools and dependencies |
| Container | Running instance of an image |
| Dockerfile | Script used to build an image |
| Volume | Host directory mounted into container |
| Registry | Storage for Docker images |
| Tag | Version label for an image |

## Simple Dockerfile

```dockerfile
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    python3 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace
```

## Build And Run

```bash
docker build -t embedded-build:dev .
docker run --rm -it -v "$PWD:/workspace" embedded-build:dev bash
```

## Image vs Container

An image is the template. A container is a running instance of that template.

Example:

```bash
docker build -t cpp-build:1.0 .
docker run --rm cpp-build:1.0 g++ --version
```

The image can be reused many times. Each `docker run` creates a new container.

## Dockerfile Layering

Each Dockerfile instruction creates a layer. Docker can reuse unchanged layers during rebuilds.

Good pattern:

```dockerfile
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    python3 \
    git \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace
```

Why combine package installation in one `RUN`?

- Fewer layers.
- Easier cache behavior.
- Avoids stale `apt` metadata.

Bad pattern:

```dockerfile
RUN apt-get update
RUN apt-get install -y build-essential
```

This can create cache problems because the update and install steps are split.

## Tags And Versioning

Avoid relying only on `latest` for CI.

Better:

```text
embedded-build:gcc-12-ubuntu-22.04
yocto-build:kirkstone-v1
static-analysis:clang-18
```

For release builds, record the Docker image tag or digest in release metadata.

Example digest format:

```text
registry.example.com/team/embedded-build@sha256:<digest>
```

Tags are convenient. Digests are more exact.

## Volumes

Volumes or bind mounts let a container access files from the host.

Common development command:

```bash
docker run --rm -it \
  -v "$PWD:/workspace" \
  -w /workspace \
  embedded-build:dev \
  bash
```

Meaning:

- `-v "$PWD:/workspace"` mounts current host directory into the container.
- `-w /workspace` sets working directory.
- `--rm` removes the container after it exits.

For Yocto, large directories are often mounted as volumes:

```bash
docker run --rm -it \
  -v "$PWD:/workspace" \
  -v "$HOME/yocto-cache/downloads:/workspace/build/downloads" \
  -v "$HOME/yocto-cache/sstate-cache:/workspace/build/sstate-cache" \
  yocto-build:dev \
  bash
```

## Environment Variables

Environment variables can configure a container at runtime.

```bash
docker run --rm \
  -e BUILD_TYPE=Release \
  -e TARGET_MACHINE=my-board \
  embedded-build:dev \
  ./build.sh
```

In CI, environment variables often provide:

- Build type
- Target machine
- Version string
- Registry path
- Cache path
- Credentials or tokens

Do not bake secrets into Docker images.

## Networking

By default, containers use bridge networking.

Common cases:

- Download source dependencies from the internet.
- Access internal package registries.
- Connect to license servers.
- Run local test services.

Useful flags:

```bash
docker run --network host ...
docker run -p 8080:8080 ...
```

`--network host` can be useful in Linux CI or hardware labs, but it reduces isolation.

## Accessing Hardware Devices

Some embedded tasks need USB, serial, or flashing tools.

Example serial access:

```bash
docker run --rm -it \
  --device=/dev/ttyUSB0 \
  embedded-tools:dev \
  minicom -D /dev/ttyUSB0
```

Example broader device access:

```bash
docker run --rm -it \
  --privileged \
  embedded-tools:dev \
  bash
```

Use `--privileged` carefully. It gives the container much more access to the host.

For CI, a dedicated shell runner is sometimes safer and simpler for hardware flashing than a privileged Docker runner.

## Multi-Stage Builds

Multi-stage builds keep the final image smaller by separating build tools from runtime files.

Example:

```dockerfile
FROM gcc:13 AS builder
WORKDIR /src
COPY . .
RUN g++ -std=c++17 main.cc -o app

FROM ubuntu:22.04
COPY --from=builder /src/app /usr/local/bin/app
CMD ["app"]
```

In embedded development, multi-stage builds are useful for tools or test binaries, but full Yocto builds usually use a dedicated build image instead.

## Docker Compose

Docker Compose describes multi-container setups.

Example use cases:

- App plus database for integration tests
- Test service plus mock server
- Local CI-like environment

Simple example:

```yaml
services:
  app-test:
    image: embedded-build:dev
    volumes:
      - .:/workspace
    working_dir: /workspace
    command: ./test.sh
```

For embedded C/C++, Compose is less central than in backend development, but it can be useful for integration tests and tool orchestration.

## Docker In CI

In GitLab CI, a job can run inside a Docker image:

```yaml
build:
  image: embedded-build:dev
  script:
    - cmake -S . -B build
    - cmake --build build
```

This reduces "works on my machine" problems.

## Building Docker Images In CI

Many teams build and publish their own CI images.

Typical flow:

```text
Dockerfile change
  -> build image
  -> run smoke test
  -> tag image
  -> push to registry
  -> use image in project pipelines
```

Example:

```yaml
build_ci_image:
  image: docker:latest
  services:
    - docker:dind
  script:
    - docker build -t registry.example.com/team/embedded-build:$CI_COMMIT_SHA .
    - docker push registry.example.com/team/embedded-build:$CI_COMMIT_SHA
```

Some GitLab runners use Docker-in-Docker. Others mount the host Docker socket. Both have security trade-offs.

## Embedded Use Cases

Docker is useful for:

- C/C++ native build environment
- Cross-compilation environment
- Yocto dependency environment
- Static analysis tools
- Unit test environment
- Packaging and release tools

## Limitations

Docker does not replace hardware.

Some embedded tasks still need host access or special permissions:

- USB flashing tools
- Serial console access
- JTAG/SWD debugging
- Hardware-in-the-loop tests
- Kernel module development

For these cases, CI may use privileged containers, shell runners, or dedicated hardware runners.

## Docker Best Practices

- Pin base image versions.
- Keep Dockerfiles readable and minimal.
- Install only required tools.
- Remove package manager cache after install.
- Use non-root users when practical.
- Do not store secrets in images.
- Version CI images.
- Use `.dockerignore` to avoid copying unnecessary files.
- Record image tags or digests in release metadata.

Example `.dockerignore`:

```text
.git
build/
tmp/
downloads/
sstate-cache/
*.log
```

## Common Docker Problems

- Build works locally but CI uses a different image tag.
- Container cannot access USB or serial devices.
- File permission mismatch between host and container.
- Large build context because `.dockerignore` is missing.
- CI uses `latest` and unexpectedly changes tool versions.
- Yocto build is slow because downloads and sstate are not cached.
- Secrets are accidentally copied into image layers.

## Interview Notes

Good answers should mention:

- Docker improves reproducibility.
- Docker images should be versioned.
- CI should use the same image as local development when possible.
- For embedded hardware access, Docker may need device mounts or dedicated runners.
- Large Yocto builds inside Docker require careful volume/cache setup.
