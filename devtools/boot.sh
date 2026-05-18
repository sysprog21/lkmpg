#!/usr/bin/env bash

# devtools/boot.sh -- launch QEMU with the lkmpg kernel and initramfs.
# The project root is shared into the guest via 9p virtfs at /mnt/lkmpg/.
#
# Usage:
#   devtools/boot.sh              # interactive shell
#   devtools/boot.sh --gdb        # wait for GDB on localhost:1234
#   devtools/boot.sh --test CMD   # run CMD in guest, exit with its status
#   devtools/boot.sh -- EXTRA     # pass extra args to QEMU

set -euo pipefail

DEVTOOLS_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$DEVTOOLS_DIR/.." && pwd)"
. "$DEVTOOLS_DIR/config.defaults"
[ -f "$DEVTOOLS_DIR/config.local" ] && . "$DEVTOOLS_DIR/config.local"

die() { echo "ERROR: $*" >&2; exit 1; }

base64_nowrap() {
    base64 | tr -d '\n'
}

# Parse arguments before expensive checks so --help works without setup
GDB_MODE=0
TEST_CMD=""
EXTRA_QEMU_ARGS=()

while [ $# -gt 0 ]; do
    case "$1" in
        --gdb)      GDB_MODE=1; shift ;;
        --test)     shift; TEST_CMD="${1:?--test requires a command}"; shift ;;
        --)         shift; EXTRA_QEMU_ARGS=("$@"); break ;;
        -h|--help)
            echo "Usage: $0 [--gdb] [--test CMD] [-- QEMU_ARGS...]"
            exit 0 ;;
        *)          die "Unknown option: $1" ;;
    esac
done

BZIMAGE="$KERNEL_BUILD/arch/x86/boot/bzImage"
[ -f "$BZIMAGE" ] || die "Kernel not built. Run devtools/setup.sh first."
[ -f "$INITRAMFS_CPIO" ] || die "Initramfs not built. Run devtools/setup.sh first."
command -v "$QEMU_BIN" >/dev/null 2>&1 || die "$QEMU_BIN not found. Install QEMU."

# Kernel command line
KCMD="console=ttyS0 loglevel=7 nokaslr"
if [ -n "$TEST_CMD" ]; then
    # Base64-encode the command to survive kernel cmdline word-splitting.
    # Strip newlines portably so this works with both GNU and BSD base64.
    # The init script decodes it before execution.
    KCMD="$KCMD lkmpg.cmd64=$(printf '%s' "$TEST_CMD" | base64_nowrap)"
fi

# Build QEMU arguments
QEMU_ARGS=(
    -kernel "$BZIMAGE"
    -initrd "$INITRAMFS_CPIO"
    -nographic
    -m "$QEMU_MEM"
    -smp "$QEMU_SMP"
    -no-reboot
    -virtfs "local,id=lkmpg,path=$PROJECT_ROOT,security_model=none,mount_tag=lkmpg"
    -append "$KCMD"
)

# KVM acceleration (Linux only, when available)
if [ -w /dev/kvm ] 2>/dev/null; then
    QEMU_ARGS+=(-enable-kvm -cpu host)
fi

# GDB mode: start paused, waiting for debugger
if [ "$GDB_MODE" -eq 1 ]; then
    QEMU_ARGS+=(-gdb "tcp::$QEMU_GDB_PORT" -S)
    echo "QEMU waiting for GDB on localhost:$QEMU_GDB_PORT"
    if [ -f "$KERNEL_BUILD/vmlinux" ]; then
        echo "Connect with: gdb $KERNEL_BUILD/vmlinux -ex 'target remote :$QEMU_GDB_PORT'"
    else
        echo "WARNING: vmlinux not found (prebuilt kernels exclude it for size)."
        echo "Rebuild from source for full GDB support: LKMPG_NO_PREBUILT=1 devtools/setup.sh"
        echo "Connect with: gdb -ex 'target remote :$QEMU_GDB_PORT'"
    fi
fi

# Append any extra user-provided QEMU args
QEMU_ARGS+=("${EXTRA_QEMU_ARGS[@]+"${EXTRA_QEMU_ARGS[@]}"}")

# In test mode the guest never reads stdin.  Redirect from /dev/null so
# QEMU's -nographic serial setup never calls tcsetattr() on a real
# terminal, which would receive SIGTTOU if the caller piped stdout or
# wrapped boot.sh with timeout(1) (whose child runs in its own process
# group, not the terminal's foreground group).
if [ -n "$TEST_CMD" ]; then
    exec "$QEMU_BIN" "${QEMU_ARGS[@]}" < /dev/null
else
    exec "$QEMU_BIN" "${QEMU_ARGS[@]}"
fi
