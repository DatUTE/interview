# Toolchains And Automation Overview

Embedded DevOps work often involves build systems, toolchains, and automation scripts.

The goal is to make developer workflows and CI workflows repeatable.

## Toolchain

A toolchain is a set of tools used to build software.

For C/C++ embedded development, it often includes:

- Compiler
- Linker
- Assembler
- Debugger
- Standard library
- Sysroot
- Build tools

Example:

```text
arm-none-eabi-gcc
aarch64-poky-linux-g++
gcc/g++ for native Linux builds
```

## Cross-Compilation

Cross-compilation means building software on one architecture for another architecture.

Example:

```text
Build host: x86_64 Ubuntu
Target: ARM Linux board
```

Important pieces:

- Target compiler
- Target sysroot
- Target headers
- Target libraries
- Correct ABI and CPU flags

## Build Systems

Common build systems:

- Make
- CMake
- Ninja
- Bazel
- Yocto/BitBake

In embedded projects, CMake may build applications, while Yocto builds the full Linux image.

## Automation Scripts

Automation scripts reduce manual steps.

Common scripts:

- `build.sh`
- `test.sh`
- `package.sh`
- `flash.sh`
- `deploy.sh`
- `collect_logs.py`

Shell is good for connecting command-line tools. Python is better for more complex logic, parsing, reports, and test orchestration.

## Common Automation Flow

```text
prepare environment
  -> check tool versions
  -> build
  -> run tests
  -> package artifacts
  -> deploy or flash target
  -> collect logs
  -> generate report
```

## Interview Notes

Good answers should mention:

- Scripts should fail fast and return meaningful exit codes.
- CI logs should be readable.
- Tool versions should be pinned.
- Cross-compilation issues often come from wrong sysroot or ABI mismatch.
- Build automation should produce traceable artifacts.
