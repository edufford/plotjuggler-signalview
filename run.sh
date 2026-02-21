#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"

if [ ! -f "$BUILD_DIR/libSignalViewPlugin.so" ]; then
  echo "Error: Plugin not built. Run ./build.sh first."
  exit 1
fi

# Find plotjuggler binary
if [ -n "$PJ_INSTALL_DIR" ]; then
  # If user pointed to the bin dir, go up one level
  if [ -f "$PJ_INSTALL_DIR/plotjuggler" ] && [ ! -d "$PJ_INSTALL_DIR/bin" ]; then
    PJ_BIN="$PJ_INSTALL_DIR/plotjuggler"
  else
    PJ_BIN="$PJ_INSTALL_DIR/bin/plotjuggler"
  fi
else
  PJ_BIN="$(which plotjuggler 2>/dev/null || true)"
fi

if [ -z "$PJ_BIN" ] || [ ! -f "$PJ_BIN" ]; then
  echo "Error: Cannot find plotjuggler."
  echo "Either add it to PATH or set PJ_INSTALL_DIR, e.g.:"
  echo "  export PJ_INSTALL_DIR=/path/to/plotjuggler-install"
  exit 1
fi

"$PJ_BIN" --plugin_folders "$BUILD_DIR" -n "$@"
