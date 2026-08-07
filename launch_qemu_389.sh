#!/bin/bash
set -euo pipefail

MACHINE_TYPE="s32k389"
KERNEL_PATH="${1:-}"
DEBUG_PARAMS="${2:-}"
DEBUG_ARGS=()

if [ -z "$DEBUG_PARAMS" ]; then
    DEBUG_PARAMS="guest_errors"
fi

if [ "$DEBUG_PARAMS" = "debug" ] || [ "$DEBUG_PARAMS" = "gdb" ]; then
    DEBUG_ARGS=(-S -gdb tcp::1234)
    DEBUG_PARAMS="guest_errors"
    echo "Starting QEMU in GDB-stopped mode"
else
    echo "Starting QEMU normally"
fi

SCRIPT_DIR="$( cd -- "$( dirname -- "${BASH_SOURCE[0]:-$0}" )" &> /dev/null && pwd )"
QEMU_BIN="$SCRIPT_DIR/build/qemu-system-arm"

if [ -z "$KERNEL_PATH" ]; then
    echo "Usage: $0 <kernel-path> [debug-params]"
    exit 1
fi

if [ ! -x "$QEMU_BIN" ]; then
    echo "QEMU binary not found: $QEMU_BIN"
    exit 1
fi

CAN_ARGS=()
if command -v ip >/dev/null 2>&1; then
    if ! ip link show vcan0 >/dev/null 2>&1; then
        if ! ip link add dev vcan0 type vcan; then
            echo "Unable to create vcan0; using an internal QEMU CAN bus"
        else
            echo "Created SocketCAN interface vcan0"
        fi
    fi

    if ip link show vcan0 >/dev/null 2>&1; then
        ip link set up vcan0 >/dev/null 2>&1 || true

        CAN_ARGS=(
            -object can-bus,id=canbus0
            -object can-host-socketcan,id=socketcan0,if=vcan0,canbus=canbus0
            -machine canbus0=canbus0
        )

        echo "Using SocketCAN interface vcan0 for QEMU CAN traffic"
    fi
else
    echo "ip command not available; using an internal QEMU CAN bus"
fi

# Launch selected QEMU
exec "$QEMU_BIN" \
  -M "$MACHINE_TYPE" \
  -kernel "$KERNEL_PATH" \
  -nographic \
  -serial mon:stdio \
  "${DEBUG_ARGS[@]}" \
  "${CAN_ARGS[@]}" \
  -d "$DEBUG_PARAMS"
