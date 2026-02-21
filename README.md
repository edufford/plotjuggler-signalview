# PlotJuggler Signal View

An oscilloscope-style signal viewer plugin for [PlotJuggler](https://github.com/facontidavide/PlotJuggler). Displays time series data as step-wise (staircase) traces with individually positionable Y-axis bars, multi-file overlay comparison, and time shifting.

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

- PlotJuggler 3.x (built and installed)
- Qt 5
- CMake 3.16+
- fmt library

## Building

If `plotjuggler` is on your PATH, the build script auto-detects the install prefix:

```bash
./build.sh
```

Otherwise, set `PJ_INSTALL_DIR` manually:

```bash
PJ_INSTALL_DIR=/path/to/plotjuggler-install ./build.sh
```

## Running

The `run.sh` script finds `plotjuggler` and launches it with the plugin loaded from the build directory. It uses the same detection as `build.sh` — `PJ_INSTALL_DIR` if set, otherwise PATH lookup.

```bash
./run.sh                        # launch PlotJuggler with the plugin
./run.sh -d data.csv            # load a data file
./run.sh -l layout.xml          # restore a saved layout
```

Any additional arguments are passed through to PlotJuggler.

Then go to **Tools > Signal View** to open the plugin.

## Usage

1. Click **Add Signal** to add signals to the view
2. Double-click a signal name, value, or bar to edit properties (color, line style, Y range, etc.)
3. Click **Overlay** to load a CSV file for comparison
4. Toggle **Time Shift** and drag on the canvas to align data layers
5. Toggle **H. Zoom** and drag to zoom into a time range

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
