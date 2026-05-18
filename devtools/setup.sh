#!/usr/bin/env bash

# devtools/setup.sh -- one-time setup for QEMU-based kernel module testing.
# Downloads kernel source and busybox, builds both, creates an initramfs.
# Each phase is idempotent: skipped when output exists AND config has not changed.
#
# When a prebuilt tarball is available on GitHub Releases, the kernel build
# phase is replaced by a download (~30s instead of ~15 min).
# Set LKMPG_NO_PREBUILT=1 to force building from source.
#
# Must run on Linux (native or via Docker/lima on macOS).

set -euo pipefail

DEVTOOLS_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$DEVTOOLS_DIR/.." && pwd)"
. "$DEVTOOLS_DIR/config.defaults"
[ -f "$DEVTOOLS_DIR/config.local" ] && . "$DEVTOOLS_DIR/config.local"

NPROC=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

# GitHub repository hosting prebuilt releases
GITHUB_REPO="${GITHUB_REPO:-sysprog21/lkmpg}"

die() { echo "ERROR: $*" >&2; exit 1; }

host_arch() {
    case "$(uname -m)" in
        x86_64|amd64) echo "x86_64" ;;
        aarch64|arm64) echo "arm64" ;;
        *)
            uname -m
            ;;
    esac
}

# Compute a short hash of the given files to detect config changes.
stamp_hash() {
    cat "$@" 2>/dev/null | sha256sum | cut -c1-16
}

stamp_value() {
    local config_hash="$1"
    echo "${config_hash}:$(host_arch)"
}

# Check whether a stamp file matches the current config hash.
stamp_fresh() {
    local stamp_file="$1" expected="$2"
    [ -f "$stamp_file" ] && [ "$(cat "$stamp_file")" = "$expected" ]
}

# Compute the release tag for the current prebuilt inputs.
prebuilt_tag() {
    local h
    h=$(cat \
        "$DEVTOOLS_DIR/kernel.config" \
        "$DEVTOOLS_DIR/config.defaults" \
        "$DEVTOOLS_DIR/initramfs/init" \
        "$DEVTOOLS_DIR/setup.sh" \
        "$DEVTOOLS_DIR/pack-prebuilt.sh" \
        2>/dev/null | sha256sum | cut -c1-8)
    echo "devtools-v${KERNEL_VERSION}-$(host_arch)-${h}"
}

check_host() {
    if [ "$(uname)" = "Darwin" ]; then
        die "Kernel and busybox must be built on Linux.
Run this script inside lima or Docker:
  lima $0
  docker run --rm -v $PROJECT_ROOT:/src -w /src/devtools gcc:latest bash setup.sh"
    fi

    # Build tools are only required when building from source.
    # Prebuilt downloads need just curl + tar.
}

# --- Prebuilt download ---
# Try to fetch a prebuilt kernel-build tarball from GitHub Releases.
# Returns 0 if the prebuilt was successfully unpacked, 1 otherwise.
try_prebuilt() {
    if [ "${LKMPG_NO_PREBUILT:-0}" = "1" ]; then
        echo "     Prebuilt download disabled (LKMPG_NO_PREBUILT=1)"
        return 1
    fi

    local tag
    tag=$(prebuilt_tag)
    local url="https://github.com/${GITHUB_REPO}/releases/download/${tag}/lkmpg-prebuilt-${tag}.tar.xz"
    local tarball="$CACHE_DIR/lkmpg-prebuilt-${tag}.tar.xz"

    echo "     Checking for prebuilt: $tag"

    # Quick HEAD check before downloading the full tarball
    if ! curl -fsSL --head "$url" >/dev/null 2>&1; then
        echo "     No matching prebuilt release found for host arch $(host_arch) (will build from source)"
        return 1
    fi

    echo "     Downloading prebuilt tarball ..."
    if ! curl -fL --progress-bar -o "$tarball" "$url"; then
        echo "     Download failed (will build from source)"
        rm -f "$tarball"
        return 1
    fi

    echo "     Unpacking prebuilt ..."
    mkdir -p "$KERNEL_BUILD"

    # Unpack the entire prebuilt tarball into CACHE_DIR.
    # pack-prebuilt.sh creates the archive with `tar -C $STAGE .`, so members
    # are stored as ./kernel-build/..., ./initramfs.cpio.gz, etc.
    if ! tar -xJf "$tarball" -C "$CACHE_DIR"; then
        echo "     Extraction failed (will build from source)"
        rm -f "$tarball"
        return 1
    fi

    # Verify essential kbuild artifacts were actually extracted.
    # bzImage alone is not enough -- module builds also need Module.symvers
    # and host tools like scripts/mod/modpost.
    if [ ! -f "$KERNEL_BUILD/arch/x86/boot/bzImage" ] || \
       [ ! -f "$KERNEL_BUILD/Module.symvers" ]; then
        echo "     Prebuilt tarball is missing required files (will build from source)"
        return 1
    fi

    # The prebuilt build directory needs a "source" symlink pointing to the
    # kernel source tree (kbuild follows it during out-of-tree module builds).
    # The Makefile in the tarball already references source/Makefile (rewritten
    # by pack-prebuilt.sh), so this symlink resolves it to the user's source.
    ln -sfn "$KERNEL_SRC" "$KERNEL_BUILD/source"

    echo "     Prebuilt unpacked successfully"
    return 0
}

# --- Phase 1: Download and extract kernel source ---
# Always needed: kbuild references the source tree via the "source" symlink
# even when using prebuilt build output.
download_kernel() {
    if [ -d "$KERNEL_SRC" ]; then
        echo "[1/3] Kernel source already at $KERNEL_SRC (skipping download)"
        return
    fi

    echo "[1/3] Downloading kernel $KERNEL_VERSION ..."
    mkdir -p "$CACHE_DIR"

    # Try kernel.org first (xz, ~130MB), fall back to GitHub mirror (gzip, ~220MB).
    # GitHub uses gregkh/linux (stable tree) which has point-release tags.
    local tarball_xz="$CACHE_DIR/linux-${KERNEL_VERSION}.tar.xz"
    local tarball_gz="$CACHE_DIR/linux-${KERNEL_VERSION}.tar.gz"

    if [ -f "$tarball_xz" ]; then
        echo "     Using cached tarball: $tarball_xz"
        tar -xf "$tarball_xz" -C "$CACHE_DIR"
    elif [ -f "$tarball_gz" ]; then
        echo "     Using cached tarball: $tarball_gz"
        tar -xzf "$tarball_gz" -C "$CACHE_DIR"
    elif curl -fL --progress-bar -o "$tarball_xz" "$KERNEL_URL_PRIMARY" 2>/dev/null; then
        echo "     Extracting (kernel.org) ..."
        tar -xf "$tarball_xz" -C "$CACHE_DIR"
    elif curl -fL --progress-bar -o "$tarball_gz" "$KERNEL_URL_GITHUB" 2>/dev/null; then
        echo "     Extracting (GitHub mirror) ..."
        tar -xzf "$tarball_gz" -C "$CACHE_DIR"
        # GitHub archives extract to linux-v$VERSION; rename to match kernel.org layout
        if [ -d "$CACHE_DIR/linux-v${KERNEL_VERSION}" ] && [ ! -d "$KERNEL_SRC" ]; then
            mv "$CACHE_DIR/linux-v${KERNEL_VERSION}" "$KERNEL_SRC"
        fi
    else
        die "Failed to download kernel source from both kernel.org and GitHub."
    fi

    [ -d "$KERNEL_SRC" ] || die "Kernel source directory $KERNEL_SRC not found after extraction."
    echo "     Done: $KERNEL_SRC"
}

# --- Phase 2: Build the kernel (or use prebuilt) ---
build_kernel() {
    local bzimage="$KERNEL_BUILD/arch/x86/boot/bzImage"
    local stamp="$KERNEL_BUILD/.config-stamp"
    local expected_hash expected
    expected_hash=$(stamp_hash "$DEVTOOLS_DIR/kernel.config" "$DEVTOOLS_DIR/config.defaults")
    expected=$(stamp_value "$expected_hash")

    if [ -f "$bzimage" ] && stamp_fresh "$stamp" "$expected"; then
        echo "[2/3] Kernel already built (config unchanged, skipping)"
        # Ensure source symlink is current
        ln -sfn "$KERNEL_SRC" "$KERNEL_BUILD/source"
        return
    fi

    echo "[2/3] Preparing kernel $KERNEL_VERSION ..."

    # Try prebuilt download first.
    # try_prebuilt() already validates bzImage + Module.symvers exist.
    if try_prebuilt && [ -f "$bzimage" ]; then
        echo "$expected" > "$stamp"
        echo "     Done (prebuilt): $bzimage"
        return
    fi

    # Fall back to building from source -- check build tools
    for cmd in make gcc bc flex bison; do
        command -v "$cmd" >/dev/null 2>&1 || die "$cmd not found. Install build-essential."
    done

    if [ -f "$bzimage" ]; then
        echo "     Kernel config changed, rebuilding ..."
    else
        echo "     Building from source ..."
    fi

    mkdir -p "$KERNEL_BUILD"

    # Start from defconfig, then merge our minimal fragment.
    # merge_config.sh runs bare `make` internally, so it must execute
    # from the kernel source directory (it has no -C equivalent).
    make -C "$KERNEL_SRC" O="$KERNEL_BUILD" defconfig
    (cd "$KERNEL_SRC" && \
        scripts/kconfig/merge_config.sh \
            -O "$KERNEL_BUILD" \
            "$KERNEL_BUILD/.config" \
            "$DEVTOOLS_DIR/kernel.config")

    # Build kernel image and prepare for out-of-tree module builds.
    # `make bzImage` generates vmlinux.symvers (vmlinux exported symbols)
    # via modpost during the vmlinux link. `modules_prepare` sets up the
    # generated headers and host tools needed by kbuild.
    # Copy vmlinux.symvers to Module.symvers so that out-of-tree module
    # builds can resolve vmlinux symbols (printk, register_chrdev, etc.)
    # without building all in-tree modules.
    #
    # Pin -std=gnu11: GCC 15+ defaults to C23 where bool/false/true are
    # keywords, conflicting with the kernel's own typedefs.  We pass it
    # through three variables because several x86 sub-Makefiles (EFI stub,
    # decompressor, real-mode setup) rebuild KBUILD_CFLAGS with := which
    # discards the top-level -std=gnu11.  KCPPFLAGS survives because those
    # sub-Makefiles don't override KBUILD_CPPFLAGS.
    #
    # -Wno-error demotes all -Werror to warnings.  Needed because CONFIG_WERROR
    # (default y) enables -Werror, and newer GCC warns on kernel patterns that
    # are intentional (non-NUL-terminated ACPI/PNP ID strings, etc.).  KCFLAGS
    # and KCPPFLAGS are appended after KBUILD_CFLAGS, so our -Wno-error
    # overrides the kernel's -Werror.
    local stdflag="-std=gnu11 -Wno-error"
    make -C "$KERNEL_SRC" O="$KERNEL_BUILD" -j"$NPROC" \
        KCFLAGS="$stdflag" KCPPFLAGS="$stdflag" HOSTCFLAGS="$stdflag" bzImage
    make -C "$KERNEL_SRC" O="$KERNEL_BUILD" -j"$NPROC" \
        KCFLAGS="$stdflag" KCPPFLAGS="$stdflag" HOSTCFLAGS="$stdflag" modules_prepare
    cp "$KERNEL_BUILD/vmlinux.symvers" "$KERNEL_BUILD/Module.symvers"

    echo "$expected" > "$stamp"
    echo "     Done: $bzimage"
}

# --- Phase 3: Build busybox and create initramfs ---
build_busybox() {
    local busybox_bin="$BUSYBOX_SRC/busybox"
    if [ -f "$busybox_bin" ]; then
        echo "     Busybox already built (skipping)"
        return
    fi

    local tarball="$CACHE_DIR/busybox-${BUSYBOX_VERSION}.tar.bz2"
    if [ ! -d "$BUSYBOX_SRC" ]; then
        echo "     Downloading busybox $BUSYBOX_VERSION ..."
        if [ ! -f "$tarball" ]; then
            curl -L --progress-bar -o "$tarball" "$BUSYBOX_URL"
        fi
        tar -xf "$tarball" -C "$CACHE_DIR"
    fi

    echo "     Building busybox (static) ..."
    make -C "$BUSYBOX_SRC" defconfig
    # Static linking for initramfs; disable tc (traffic control) to avoid
    # build failures against newer kernel headers that removed CBQ structs.
    sed -i -e 's/# CONFIG_STATIC is not set/CONFIG_STATIC=y/' \
           -e 's/CONFIG_TC=y/# CONFIG_TC is not set/' \
        "$BUSYBOX_SRC/.config"
    make -C "$BUSYBOX_SRC" -j"$NPROC"
}

build_initramfs() {
    local stamp="$CACHE_DIR/.initramfs-stamp"
    local expected
    expected=$(stamp_hash "$DEVTOOLS_DIR/initramfs/init")

    if [ -f "$INITRAMFS_CPIO" ] && stamp_fresh "$stamp" "$expected"; then
        echo "[3/3] Initramfs already built (init script unchanged, skipping)"
        return
    fi

    if [ -f "$INITRAMFS_CPIO" ]; then
        echo "[3/3] Init script changed, rebuilding initramfs ..."
    else
        echo "[3/3] Creating initramfs ..."
    fi

    build_busybox

    # Create directory structure
    rm -rf "$INITRAMFS_DIR"
    mkdir -p "$INITRAMFS_DIR"
    make -C "$BUSYBOX_SRC" CONFIG_PREFIX="$INITRAMFS_DIR" install

    mkdir -p "$INITRAMFS_DIR"/{proc,sys,dev,tmp,mnt/modules,etc}

    # Install our init script
    cp "$DEVTOOLS_DIR/initramfs/init" "$INITRAMFS_DIR/init"
    chmod +x "$INITRAMFS_DIR/init"

    # Pack the initramfs
    (cd "$INITRAMFS_DIR" && find . | cpio -o -H newc 2>/dev/null | gzip > "$INITRAMFS_CPIO")

    echo "$expected" > "$stamp"
    echo "     Done: $INITRAMFS_CPIO ($(du -h "$INITRAMFS_CPIO" | cut -f1))"
}

check_host
download_kernel
build_kernel
build_initramfs

echo ""
echo "Setup complete. Next steps:"
echo "  devtools/build-modules.sh    # build kernel modules"
echo "  devtools/boot.sh             # boot QEMU (interactive)"
