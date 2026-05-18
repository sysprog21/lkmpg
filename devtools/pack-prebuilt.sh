#!/usr/bin/env bash

# devtools/pack-prebuilt.sh -- create a minimal tarball of kernel build output.
# Contains only what kbuild needs for out-of-tree module compilation plus
# the bzImage for QEMU boot and the initramfs.
#
# The kernel source tree is NOT included (downloaded separately from kernel.org).
# vmlinux is NOT included (too large with debug info; rebuild for GDB use).
#
# Usage: devtools/pack-prebuilt.sh [output.tar.xz]

set -euo pipefail

DEVTOOLS_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$DEVTOOLS_DIR/.." && pwd)"
. "$DEVTOOLS_DIR/config.defaults"
[ -f "$DEVTOOLS_DIR/config.local" ] && . "$DEVTOOLS_DIR/config.local"

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

[ -f "$KERNEL_BUILD/arch/x86/boot/bzImage" ] || die "Kernel not built. Run devtools/setup.sh first."
[ -f "$INITRAMFS_CPIO" ] || die "Initramfs not built. Run devtools/setup.sh first."

# Release tag is derived from all inputs that affect the packaged prebuilt
INPUT_HASH=$(cat \
    "$DEVTOOLS_DIR/kernel.config" \
    "$DEVTOOLS_DIR/config.defaults" \
    "$DEVTOOLS_DIR/initramfs/init" \
    "$DEVTOOLS_DIR/setup.sh" \
    "$DEVTOOLS_DIR/pack-prebuilt.sh" \
    2>/dev/null | sha256sum | cut -c1-8)
INIT_HASH=$(sha256sum "$DEVTOOLS_DIR/initramfs/init" 2>/dev/null | cut -c1-8)
HOST_ARCH=$(host_arch)
TAG="devtools-v${KERNEL_VERSION}-${HOST_ARCH}-${INPUT_HASH}"
OUTPUT="${1:-$CACHE_DIR/lkmpg-prebuilt-${TAG}.tar.xz}"

echo "Packing prebuilt artifacts ..."
echo "  Tag:    $TAG"
echo "  Host:   $HOST_ARCH"
echo "  Output: $OUTPUT"

# Create a staging directory with only the files kbuild needs
STAGE=$(mktemp -d)
trap 'rm -rf "$STAGE"' EXIT

STAGE_BUILD="$STAGE/kernel-build"
mkdir -p "$STAGE_BUILD"

# --- Kernel build output (for out-of-tree module compilation) ---

# Core kbuild files
cp "$KERNEL_BUILD/.config" "$STAGE_BUILD/"
cp "$KERNEL_BUILD/Module.symvers" "$STAGE_BUILD/"
[ -f "$KERNEL_BUILD/modules.order" ] && cp "$KERNEL_BUILD/modules.order" "$STAGE_BUILD/"

# Top-level Makefile (kbuild entry point when using O=).
# kbuild generates this with an absolute include path; rewrite it to use a
# relative "source" symlink so the prebuilt tarball is machine-independent.
cp "$KERNEL_BUILD/Makefile" "$STAGE_BUILD/"
sed -i "s|^include .*/linux-${KERNEL_VERSION}/Makefile|include source/Makefile|" \
    "$STAGE_BUILD/Makefile"

# Config stamp (for setup.sh staleness detection)
[ -f "$KERNEL_BUILD/.config-stamp" ] && cp "$KERNEL_BUILD/.config-stamp" "$STAGE_BUILD/"

# Generated headers (autoconf.h, uapi, etc.)
mkdir -p "$STAGE_BUILD/include"
cp -a "$KERNEL_BUILD/include/config" "$STAGE_BUILD/include/" 2>/dev/null || true
cp -a "$KERNEL_BUILD/include/generated" "$STAGE_BUILD/include/" 2>/dev/null || true

# Architecture-specific generated headers
mkdir -p "$STAGE_BUILD/arch/x86/include"
cp -a "$KERNEL_BUILD/arch/x86/include/generated" "$STAGE_BUILD/arch/x86/include/" 2>/dev/null || true

# Kbuild scripts (modpost, fixdep, etc.) -- compiled host tools
# Copy the scripts directory but strip intermediate build artifacts
rsync -a \
    --include='*/' \
    --include='*.sh' --include='*.pl' --include='*.py' \
    --include='Makefile*' --include='Kbuild*' --include='Kconfig*' \
    --include='mod/modpost' --include='basic/fixdep' \
    --include='genksyms/genksyms' --include='sign-file' \
    --include='recordmcount' --include='sorttable' \
    --include='module.lds' --include='modules-check.sh' \
    --include='gcc-plugins/*.so' \
    --exclude='*.o' --exclude='*.o.d' --exclude='.*.cmd' \
    --exclude='*.a' --exclude='.tmp_*' \
    "$KERNEL_BUILD/scripts/" "$STAGE_BUILD/scripts/"

# objtool (host tool required for out-of-tree module builds with CONFIG_OBJTOOL)
if [ -f "$KERNEL_BUILD/tools/objtool/objtool" ]; then
    mkdir -p "$STAGE_BUILD/tools/objtool"
    cp "$KERNEL_BUILD/tools/objtool/objtool" "$STAGE_BUILD/tools/objtool/"
fi

# bzImage for QEMU boot
mkdir -p "$STAGE_BUILD/arch/x86/boot"
cp "$KERNEL_BUILD/arch/x86/boot/bzImage" "$STAGE_BUILD/arch/x86/boot/"

# --- Initramfs ---
cp "$INITRAMFS_CPIO" "$STAGE/initramfs.cpio.gz"

# --- Initramfs stamp ---
[ -f "$CACHE_DIR/.initramfs-stamp" ] && cp "$CACHE_DIR/.initramfs-stamp" "$STAGE/"

# --- Metadata ---
cat > "$STAGE/manifest.txt" <<EOF
kernel_version=$KERNEL_VERSION
host_arch=$HOST_ARCH
input_hash=$INPUT_HASH
init_hash=$INIT_HASH
tag=$TAG
built=$(date -u +%Y-%m-%dT%H:%M:%SZ)
EOF

# Pack
echo "  Compressing ..."
tar -cJf "$OUTPUT" -C "$STAGE" .

SIZE=$(du -h "$OUTPUT" | cut -f1)
echo "  Done: $OUTPUT ($SIZE)"
echo ""
echo "Publish with:"
echo "  gh release create $TAG $OUTPUT --title '$TAG' --notes 'Prebuilt kernel $KERNEL_VERSION for lkmpg devtools'"
