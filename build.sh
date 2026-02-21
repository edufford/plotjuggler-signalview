#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"

# Auto-detect PlotJuggler install prefix if not set
if [ -z "$PJ_INSTALL_DIR" ]; then
  PJ_BIN="$(which plotjuggler 2>/dev/null || true)"
  if [ -n "$PJ_BIN" ]; then
    PJ_INSTALL_DIR="$(cd "$(dirname "$PJ_BIN")/.." && pwd)"
  else
    echo "Error: Cannot find plotjuggler on PATH."
    echo "Either add it to PATH or set PJ_INSTALL_DIR, e.g.:"
    echo "  export PJ_INSTALL_DIR=/path/to/plotjuggler-install"
    exit 1
  fi
fi

# If user pointed to the bin dir, go up one level
if [ -f "$PJ_INSTALL_DIR/plotjuggler" ] && [ ! -d "$PJ_INSTALL_DIR/include" ]; then
  PJ_INSTALL_DIR="$(cd "$PJ_INSTALL_DIR/.." && pwd)"
fi

# Validate
if [ ! -f "$PJ_INSTALL_DIR/include/PlotJuggler/plotdata.h" ]; then
  echo "Error: PJ_INSTALL_DIR=$PJ_INSTALL_DIR does not contain PlotJuggler headers."
  echo "Expected: \$PJ_INSTALL_DIR/include/PlotJuggler/plotdata.h"
  exit 1
fi

echo "Using PlotJuggler at: $PJ_INSTALL_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
cmake "$SCRIPT_DIR" -DPJ_INSTALL_DIR="$PJ_INSTALL_DIR" -DCMAKE_PREFIX_PATH="$PJ_INSTALL_DIR" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
make -j"$(nproc)"
