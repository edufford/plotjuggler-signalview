#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
PJ_INSTALL_DIR="${PJ_INSTALL_DIR:-/home/ed/work/plot/plotjuggler-install}"

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
cmake "$SCRIPT_DIR" -DPJ_INSTALL_DIR="$PJ_INSTALL_DIR" -DCMAKE_BUILD_TYPE=Release
make -j"$(nproc)"
