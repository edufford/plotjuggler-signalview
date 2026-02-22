# Signal View plugin for PlotJuggler

![CI (Linux)](https://github.com/edufford/plotjuggler-signalview/actions/workflows/ci.yml/badge.svg?branch=dev_ai&job=Build+and+Test+%28Linux%29)
![CI (Windows)](https://github.com/edufford/plotjuggler-signalview/actions/workflows/ci.yml/badge.svg?branch=dev_ai&job=Build+and+Test+%28Windows%29)

An oscilloscope-style signal viewer plugin for [PlotJuggler](https://github.com/facontidavide/PlotJuggler). Displays time series data as step-wise (staircase) traces with individually positionable Y-axis bars, multi-file overlay comparison, and time shifting.

![Signal View screenshot](images/plotjuggler-signalviewer-sample.jpg)

## Features

- **Step-wise signal traces** with per-pixel downsampling for smooth rendering at any zoom level
- **Three-column Y-axis panel**: signal names, cursor readout values, and draggable Y-axis bars with tick marks
- **Multi-file data overlay**: load CSV files on top of PlotJuggler data for side-by-side comparison
- **Time shifting**: drag to shift any data layer's time offset for alignment
- **Flexible signal bands**: independently position, resize, and group signal display regions
- **Selection and batch editing**: select signals with click, Ctrl+click, rubber band, or Ctrl+A; edit properties in bulk via the Y-range table dialog
- **Customizable appearance**: per-signal color, line style, line width, marker style, and Y-axis divisions
- **Layout persistence**: full save/restore of signal configuration, overlays, and view state via PlotJuggler layout files

## Requirements

- PlotJuggler 3.x (built and installed from source)
- Qt 5
- CMake 3.16+
- fmt library (bundled with PlotJuggler)

**Linux (Ubuntu/Debian):**

```bash
sudo apt install qtbase5-dev cmake
```

**Windows:**

- Visual Studio 2026 with the v142 (VS 2019) C++ toolchain installed
- Qt 5.15 via [aqtinstall](https://github.com/miurahr/aqtinstall): `pip install aqtinstall && aqt install-qt windows desktop 5.15.2 win64_msvc2019_64 -O C:\Qt`
- PlotJuggler must be [built from source](https://github.com/facontidavide/PlotJuggler) and installed via `cmake --install` to produce the install tree

## Building

### Linux

If `plotjuggler` is on your PATH, the build script auto-detects the install prefix:

```bash
./build.sh
```

Otherwise, set `PJ_INSTALL_DIR` manually:

```bash
PJ_INSTALL_DIR=/path/to/plotjuggler-install ./build.sh
```

### Windows

If both `qmake` and `plotjuggler` are on your PATH, the build script auto-detects both:

```bat
build.bat
```

Otherwise, set the directories manually:

```bat
set QT_DIR=C:\Qt\5.15.2\msvc2019_64
set PJ_INSTALL_DIR=C:\path\to\plotjuggler-install
build.bat
```

## Running

### Linux

```bash
./run.sh                        # launch PlotJuggler with the plugin
./run.sh -d data.csv            # load a data file
./run.sh -l layout.xml          # restore a saved layout
```

### Windows

```bat
run.bat                         # launch PlotJuggler with the plugin
run.bat -d data.csv             # load a data file
run.bat -l layout.xml           # restore a saved layout
```

Any additional arguments are passed through to PlotJuggler.

Then go to **Tools > Signal View** to open the plugin.

## Usage

1. Click **Add Signal** to add signals to the view
2. Double-click a signal name, value, or bar to edit properties (color, line style, Y range, etc.)
3. Click **Overlay** to load a CSV file for comparison
4. Toggle **Time Shift** and drag on the canvas to align data layers
5. Toggle **H. Zoom** and drag to zoom into a time range

## Testing

Run the unit tests with:

```bash
./test.sh
```

This builds with Google Test and runs all test suites via CTest. Tests cover name parsing, overlay manager logic, axis layout calculations, CSV parsing, and UI widget interactions.

To output JUnit XML results (used by CI for test reporting):

```bash
JUNIT_OUTPUT=test-results.xml ./test.sh
```

## Coding Conventions

- Google C++ formatting (`clang-format -style=Google`)
- Qt naming conventions: `m_` member prefix, `UPPER_CASE` constants, `camelCase` functions, `PascalCase` classes

## Project Structure

```
src/
  signal_view_plugin.h/cpp   Plugin entry point (PJ::ToolboxPlugin)
  signal_view_widget.h/cpp   Main widget: toolbar, layout, signal management
  plot_canvas.h/cpp          Time series rendering and mouse interaction
  y_axis_panel.h/cpp         Signal name, value, and Y-axis bar columns
  overlay_manager.h/cpp      Multi-layer data management and CSV loading
  data_sets_panel.h/cpp      Footer panel listing loaded data layers
```
