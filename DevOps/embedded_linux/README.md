# Embedded Linux DevOps Overview

Embedded Linux DevOps connects software build automation with target hardware deployment and debugging.

The goal is to make builds, flashing, testing, and release validation repeatable.

## Common Responsibilities

- Build C/C++ applications for target hardware
- Build Linux images or firmware packages
- Deploy binaries to target boards
- Flash images to target storage
- Debug boot or runtime failures
- Collect logs from target devices
- Integrate software with BSP, drivers, and middleware
- Maintain scripts for automation

## Deployment To Target Hardware

Deployment can mean different things:

- Copy application binary to target root filesystem
- Install a package
- Replace a shared library
- Flash a full image
- Update bootloader/kernel/rootfs
- Deploy through OTA mechanism

## Flashing

Flashing writes firmware or images to target storage.

Possible targets:

- eMMC
- NAND/NOR flash
- SD card
- SPI flash
- UFS

Common tools depend on board/vendor:

- `fastboot`
- `dfu-util`
- `uuu`
- `scp`
- vendor flashing scripts
- JTAG/SWD tools

## Debugging Embedded Linux

Common sources of evidence:

- Serial console logs
- Kernel logs with `dmesg`
- System logs with `journalctl`
- Process list with `ps`
- Memory map with `/proc/<pid>/maps`
- Network state with `ip`, `ss`, `tcpdump`
- Core dumps
- GDB remote debugging

## Build And Toolchain

Embedded Linux builds often use:

- Cross compiler
- Sysroot
- CMake toolchain file
- Yocto SDK
- Vendor SDK
- Docker container for build reproducibility

Example idea:

```text
source Yocto SDK environment
  -> configure CMake with target sysroot
  -> build binary
  -> package artifact
  -> deploy to board
```

## Automation Scripts

Scripting is important because many workflows combine multiple tools.

Common script tasks:

- Build project
- Package artifacts
- Flash target
- Reboot target
- Wait for device online
- Collect logs
- Run smoke tests
- Upload reports

Typical languages:

- Shell
- Python

## Interview Notes

Good answers should mention:

- Embedded deployment must consider target hardware state.
- Serial logs are often the first debug source when boot fails.
- Cross-compilation needs correct sysroot and target libraries.
- Docker can reproduce build tools, but hardware access may still need host setup.
- A good pipeline stores logs and artifacts so failures can be diagnosed later.
