# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

PlotJuggler Signal View is a C++17 Qt5 ToolboxPlugin for [PlotJuggler](https://github.com/facontidavide/PlotJuggler) that provides an oscilloscope-style time series viewer with multi-layer overlay support.

## Build & Test Commands

```bash
# Build (auto-detects PlotJuggler on PATH, or set PJ_INSTALL_DIR)
./build.sh

# Run tests (builds in Debug mode with tests enabled)
./test.sh

# Run with JUnit XML output (used by CI)
JUNIT_OUTPUT=results.xml ./test.sh

# Launch PlotJuggler with the plugin loaded
./run.sh
./run.sh -d data.csv        # with data file
./run.sh -l layout.xml      # with saved layout
```

Tests use Google Test + Qt Test. UI tests simulate mouse events with explicit `QMouseEvent` + `QApplication::sendEvent` (not `QTest::mouseMove`, which is unreliable under xvfb). Run a single test with `cd build && ctest -R TestName`.

## Architecture

```
SignalViewPlugin          Plugin entry point (PJ::ToolboxPlugin), XML state save/restore
  └─ SignalViewWidget     Main orchestrator: toolbar, signal/overlay management, dialogs
       ├─ PlotCanvas      Time series rendering, cursor, zoom/pan, downsampling
       ├─ YAxisPanel       Y-axis UI coordinator
       │   ├─ YAxisLabelColumn
       │   │   ├─ SignalNameColumn   ─┐
       │   │   └─ SignalValueColumn  ─┤─ both extend SignalColumnBase
       │   └─ YAxisBarColumn         Draggable axis bars with tick marks
       ├─ DataSetsPanel    Footer: overlay layer list with context menus
       └─ OverlayManager   Multi-layer data, CSV parsing, signal resolution
```

**SignalEntry** (defined in `plot_canvas.h`) is the core data structure passed between all widgets — it holds signal name, color, Y range, band position, line/marker style, and layer index.

**Signal naming**: Base layer signals are unprefixed or `#1/name`. Overlay layers use `#2/name`, `#3/name`, etc. `OverlayManager::resolveSignal()` maps these to PlotJuggler `PlotData` series.

## Coding Conventions

- Google C++ formatting (enforced via `clang-format -style=Google`)
- Qt naming conventions:
  - Member variables prefixed with `m_` (e.g., `m_signals`, `m_overlay_mgr`)
  - Constants use `UPPER_CASE` (e.g., `MARGIN_LEFT`, `DEFAULT_WIDTH`)
  - Functions use `camelCase` (e.g., `refreshViews`, `onEditYRange`)
  - Classes use `PascalCase` (e.g., `PlotCanvas`, `SignalEntry`)
- Headers use `#pragma once`
- Qt: `Q_OBJECT` macro, signals/slots, `CMAKE_AUTOMOC` enabled

## Key Behavioral Reference

`signalview_features.txt` is a comprehensive feature behavior reference (~490 lines) covering toolbar actions, mouse interactions, signal band mechanics, overlay system, and keyboard shortcuts. Consult this when modifying UI behavior to ensure consistency.
