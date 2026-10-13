# Yocto Overview

Yocto is a build system framework for creating custom embedded Linux distributions.

It is not a Linux distribution by itself. It gives you tools and metadata to build your own distribution for a target board or ECU.

## Why Yocto Is Used

Yocto is useful when a product needs:

- Custom Linux image
- Specific kernel configuration
- Board support package integration
- Cross-compilation
- Controlled package versions
- Reproducible release images
- Long-term maintainable embedded Linux builds

## Core Concepts

| Concept | Meaning |
|---|---|
| Poky | Reference Yocto distribution |
| BitBake | Task executor and build engine |
| Recipe | Build instructions for one component |
| Layer | Collection of recipes/configuration |
| Image | Final root filesystem and bootable output |
| Machine | Target hardware configuration |
| Distro | Distribution-level policy/configuration |
| BSP | Board Support Package for specific hardware |

## Recipe

A recipe usually describes:

- Where to get source code
- Which version to use
- How to configure/build/install
- Dependencies
- Files to package

Recipe file extension:

```text
.bb
```

Append file extension:

```text
.bbappend
```

## Layer

A layer groups related metadata.

Examples:

```text
meta-company
meta-board-vendor
meta-application
meta-openembedded
```

Layer priority and ordering matter when multiple layers modify the same package or configuration.

## BSP

BSP means Board Support Package.

It usually includes:

- Bootloader support
- Linux kernel patches/config
- Device tree files
- Drivers
- Machine configuration
- Hardware-specific firmware

## Cross-Compilation

Yocto builds software for a target architecture that is often different from the host machine.

Example:

```text
Host machine: x86_64 Linux PC
Target machine: ARM-based ECU/board
```

Yocto manages the cross-toolchain, sysroot, dependencies, and package output.

## Common Build Outputs

Yocto can produce:

- Kernel image
- Device tree blobs
- Root filesystem
- SDK/toolchain
- Bootloader image
- Full flashable image

## Common Commands

```bash
bitbake core-image-minimal
bitbake <recipe-name>
bitbake -c clean <recipe-name>
bitbake -c cleansstate <recipe-name>
bitbake -e <recipe-name>
```

## Build Directory Structure

After sourcing the Yocto environment, you usually work inside a build directory.

Common files and folders:

```text
build/
  conf/
    bblayers.conf
    local.conf
  downloads/
  sstate-cache/
  tmp/
```

Important parts:

- `bblayers.conf` lists the active layers.
- `local.conf` stores local build configuration such as machine, distro, package format, and parallelism.
- `downloads` stores source archives and Git checkouts.
- `sstate-cache` stores reusable shared-state build output.
- `tmp` contains work directories, sysroots, packages, deploy output, and logs.

## Important Configuration Files

### `bblayers.conf`

This file tells BitBake which layers are included in the build.

Example idea:

```text
BBLAYERS += "${TOPDIR}/../meta-company"
BBLAYERS += "${TOPDIR}/../meta-board"
BBLAYERS += "${TOPDIR}/../meta-openembedded/meta-oe"
```

If a recipe cannot be found, one common reason is that the layer containing it is not listed here.

### `local.conf`

This file controls build settings for the current build directory.

Common settings:

```text
MACHINE = "my-board"
DISTRO = "poky"
PACKAGE_CLASSES = "package_ipk"
BB_NUMBER_THREADS = "8"
PARALLEL_MAKE = "-j8"
```

In production CI, important config should not live only in a developer's local file. It should be version-controlled in a distro layer, template, or CI script.

## Recipe Anatomy

A simple recipe usually contains:

```bitbake
SUMMARY = "Example application"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "git://example.com/app.git;branch=main;protocol=https"
SRCREV = "abcdef1234567890"

S = "${WORKDIR}/git"

inherit cmake

DEPENDS += "openssl"
RDEPENDS:${PN} += "bash"
```

Common variables:

- `SUMMARY`: short package description.
- `LICENSE`: declared license.
- `LIC_FILES_CHKSUM`: license file checksum for compliance tracking.
- `SRC_URI`: source location and extra files.
- `SRCREV`: exact Git revision for reproducibility.
- `S`: source directory used by build tasks.
- `DEPENDS`: build-time dependencies.
- `RDEPENDS:${PN}`: runtime dependencies.

## BitBake Task Flow

BitBake recipes are executed as tasks.

Common tasks:

```text
do_fetch
  -> do_unpack
  -> do_patch
  -> do_configure
  -> do_compile
  -> do_install
  -> do_package
  -> do_rootfs
  -> do_image
```

Meaning:

- `do_fetch`: download source.
- `do_unpack`: unpack source into work directory.
- `do_patch`: apply patches from `SRC_URI`.
- `do_configure`: run configuration step such as CMake or Autotools.
- `do_compile`: build the component.
- `do_install`: install files into `${D}` staging directory.
- `do_package`: split installed files into packages.
- `do_rootfs`: assemble root filesystem.
- `do_image`: generate final image formats.

You can run a specific task:

```bash
bitbake -c compile <recipe-name>
bitbake -c install <recipe-name>
bitbake -c package <recipe-name>
```

## `.bbappend`

A `.bbappend` modifies or extends an existing recipe from another layer.

Common use cases:

- Add patches
- Change configuration flags
- Install extra files
- Add machine-specific behavior
- Override source revision

Example:

```bitbake
FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"

SRC_URI += "file://fix-startup.patch"

EXTRA_OECMAKE += "-DENABLE_FEATURE_X=ON"
```

The append file name must match the recipe name and version pattern.

Example:

```text
foo_1.0.bb
foo_1.0.bbappend
foo_%.bbappend
```

## Adding Software To An Image

To include a package in an image:

```bitbake
IMAGE_INSTALL:append = " my-application"
```

For production, this is often placed in a custom image recipe:

```bitbake
require recipes-core/images/core-image-minimal.bb

SUMMARY = "Company production image"

IMAGE_INSTALL:append = " my-application my-service"
```

Avoid manually copying files into the root filesystem after the image is built. It breaks reproducibility and traceability.

## Package vs Image

A recipe usually builds a package. An image recipe selects packages and creates a root filesystem.

Think of it like:

```text
recipe -> package
image recipe -> root filesystem containing selected packages
```

## SDK

Yocto can generate an SDK for application developers.

Command:

```bash
bitbake <image-name> -c populate_sdk
```

The SDK usually provides:

- Cross compiler
- Target sysroot
- Headers and libraries
- Environment setup script

Typical usage:

```bash
source /opt/poky/<version>/environment-setup-<target>
cmake -S . -B build
cmake --build build
```

## Reproducibility

For a reproducible Yocto build, control these inputs:

- Layer revisions
- Recipe `SRCREV`
- Machine and distro configuration
- Build container or host dependencies
- Downloads cache
- Sstate cache policy
- Patches and configuration fragments

Avoid floating branches like `SRCREV = "${AUTOREV}"` in release builds because the output can change without a source change in your repo.

## Debugging Build Failures

Useful commands:

```bash
bitbake-layers show-layers
bitbake-layers show-recipes
bitbake-layers show-appends
bitbake -e <recipe-name> | less
bitbake -c devshell <recipe-name>
bitbake -c clean <recipe-name>
bitbake -c cleansstate <recipe-name>
```

Useful locations:

```text
tmp/work/<machine-or-arch>/<recipe>/<version>/temp/log.do_compile
tmp/work/<machine-or-arch>/<recipe>/<version>/image/
tmp/deploy/images/<machine>/
```

Common failure causes:

- Missing layer in `bblayers.conf`
- Wrong `SRC_URI` or inaccessible Git repository
- License checksum mismatch
- Patch no longer applies
- Missing build dependency in `DEPENDS`
- File installed but not packaged
- Host contamination from using host tools unexpectedly
- Machine-specific config not selected

## Yocto And Kernel Changes

Kernel customization may involve:

- Kernel recipe or `.bbappend`
- Device tree changes
- Defconfig or config fragments
- Kernel patches
- Machine configuration

Common idea:

```text
meta-board
  -> machine config
  -> kernel recipe append
  -> device tree patch
  -> config fragment
```

For interview discussion, connect kernel/BSP work to real hardware enablement: boot, devices, drivers, device tree, and board-specific configuration.

## Yocto In CI/CD

Yocto builds can be slow, so CI often uses:

- Shared download cache
- Shared sstate cache
- Docker build environment
- Nightly image builds
- Artifact publishing
- Release tags tied to image output

## Interview Notes

Good answers should mention:

- Yocto creates custom embedded Linux images.
- BitBake executes tasks from recipes.
- Layers organize metadata.
- BSP connects Yocto to target hardware.
- Cross-compilation is built into the workflow.
- Reproducibility depends on fixed revisions, controlled layers, and stable build environment.
