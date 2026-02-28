#pragma once

#include <QSplitter>
#include <QWidget>
#include <algorithm>
#include <cmath>
#include <set>
#include <vector>

#include "overlay_manager.h"
#include "plot_canvas.h"

// Shared band-position helpers used by all column widgets.
namespace AxisLayout {
inline double bandTopY(const SignalEntry& sig, int widget_height,
                       double scroll_offset = 0.0) {
  double plot_h =
      widget_height - PlotCanvas::MARGIN_TOP - PlotCanvas::MARGIN_BOTTOM;
  return PlotCanvas::MARGIN_TOP +
         plot_h * (sig.band_center_norm - sig.band_height_norm * 0.5 -
                   scroll_offset);
}
inline double bandBottomY(const SignalEntry& sig, int widget_height,
                          double scroll_offset = 0.0) {
  double plot_h =
      widget_height - PlotCanvas::MARGIN_TOP - PlotCanvas::MARGIN_BOTTOM;
  return PlotCanvas::MARGIN_TOP +
         plot_h * (sig.band_center_norm + sig.band_height_norm * 0.5 -
                   scroll_offset);
}
// Returns the pixel Y offset for each signal's text row, ensuring no
// overlap. Signals are processed top-to-bottom so the highest signal
// claims the top slot. At the same position, earlier-added signals
// (lower index) stay on top for stable, predictable stacking.
inline std::vector<double> textRowYOffsets(
    const std::vector<SignalEntry>& entries, int widget_height, int row_height,
    double scroll_offset = 0.0) {
  std::vector<double> y_offsets(entries.size(), 0.0);
  if (entries.empty()) {
    return y_offsets;
  }

  // Process signals from top to bottom; earlier-added (lower index) first at
  // same Y
  std::vector<size_t> order(entries.size());
  for (size_t i = 0; i < entries.size(); i++) {
    order[i] = i;
  }
  std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
    double ya = bandTopY(entries[a], widget_height, scroll_offset);
    double yb = bandTopY(entries[b], widget_height, scroll_offset);
    if (std::abs(ya - yb) < 1.0) {
      return a < b;
    }
    return ya < yb;
  });

  std::vector<double> placed;  // absolute Y of each placed row
  for (size_t idx : order) {
    double base_y = bandTopY(entries[idx], widget_height, scroll_offset);
    double row_y = base_y;
    // Push down until no overlap with any previously placed row.
    // Use 0.5px tolerance to avoid infinite loop from floating-point
    // precision: (py + row_height) - py can be slightly < row_height.
    bool collision = true;
    while (collision) {
      collision = false;
      for (double py : placed) {
        if (std::abs(row_y - py) < row_height - 0.5) {
          row_y = py + row_height;
          collision = true;
          break;
        }
      }
    }
    y_offsets[idx] = row_y - base_y;
    placed.push_back(row_y);
  }
  return y_offsets;
}
}  // namespace AxisLayout

// Base class for signal column widgets (name and value columns).
// Provides shared drag, rubber-band selection, and scroll handling.
class SignalColumnBase : public QWidget {
  Q_OBJECT

 public:
  explicit SignalColumnBase(QWidget* parent = nullptr);
  virtual void setSignalEntries(const std::vector<SignalEntry>& entries);
  void setDragIndex(int idx);
  void setSelection(const std::set<int>& sel);
  void setScrollOffset(double offset);
  void setTheme(Theme theme);

 signals:
  void bandOffsetChanged(int index, double new_center);
  void dragIndexChanged(int index);
  void editYRangeRequested(int index);
  void addSignalRequested(double band_center_norm);
  void clickSelect(int index, bool toggle);
  void boxSelect(double y_top, double y_bottom, bool add);
  void verticalScrollRequested(double delta);
  void contextMenuRequested(int index, QPoint global_pos);

 protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void mouseDoubleClickEvent(QMouseEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;

  virtual void paintContent(QPainter& painter,
                            const std::vector<double>& row_offsets) = 0;
  int hitTestSignal(const QPoint& pos) const;
  int effectiveDragIndex() const;

  static constexpr int TEXT_ROW_HEIGHT = 15;
  std::vector<SignalEntry> m_signals;
  std::set<int> m_selected;
  double m_scroll_offset = 0.0;
  Theme m_theme = Theme::Dark;

 private:
  int m_drag_index = -1;
  int m_external_drag_index = -1;
  bool m_drag_moved = false;
  int m_drag_start_global_y = 0;
  double m_drag_start_band_center = 0.0;
  bool m_rubber_band_active = false;
  bool m_rubber_band_ctrl = false;
  QPoint m_rubber_band_origin;
  QPoint m_rubber_band_current;
};

// Signal name column: colored dot + signal name, draggable to reposition.
class SignalNameColumn : public SignalColumnBase {
 public:
  explicit SignalNameColumn(QWidget* parent = nullptr);
  static constexpr int DEFAULT_WIDTH = 70;

 protected:
  void paintContent(QPainter& painter,
                    const std::vector<double>& row_offsets) override;
};

// Signal value column: cursor readout values, vertically aligned with names.
class SignalValueColumn : public SignalColumnBase {
 public:
  explicit SignalValueColumn(QWidget* parent = nullptr);
  void setSignalEntries(const std::vector<SignalEntry>& entries) override;
  void setCursorValues(const std::vector<double>& values,
                       const std::vector<bool>& valid);
  static constexpr int DEFAULT_WIDTH = 70;

 protected:
  void paintContent(QPainter& painter,
                    const std::vector<double>& row_offsets) override;

 private:
  std::vector<double> m_cursor_values;
  std::vector<bool> m_cursor_valid;
};

// Container: name column | value column in a splitter.
class YAxisLabelColumn : public QWidget {
  Q_OBJECT

 public:
  explicit YAxisLabelColumn(QWidget* parent = nullptr);
  void setSignalEntries(const std::vector<SignalEntry>& entries);
  void setCursorValues(const std::vector<double>& values,
                       const std::vector<bool>& valid);
  void setCanvasHeight(int h);
  void setDragIndex(int idx);
  void setSelection(const std::set<int>& sel);
  void setScrollOffset(double offset);
  void setTheme(Theme theme);

  static constexpr int DEFAULT_WIDTH =
      SignalNameColumn::DEFAULT_WIDTH + SignalValueColumn::DEFAULT_WIDTH + 3;

  QList<int> splitterSizes() const { return m_splitter->sizes(); }
  void setSplitterSizes(const QList<int>& sizes) {
    m_splitter->setSizes(sizes);
  }

 signals:
  void bandOffsetChanged(int index, double new_center);
  void editYRangeRequested(int index);
  void addSignalRequested(double band_center_norm);
  void clickSelect(int index, bool toggle);
  void boxSelect(double y_top, double y_bottom, bool add);
  void verticalScrollRequested(double delta);
  void contextMenuRequested(int index, QPoint global_pos);

 private:
  QSplitter* m_splitter;
  SignalNameColumn* m_name_col;
  SignalValueColumn* m_value_col;
};

// Y-axis bars with tick marks, edge grab handles, drag interaction.
class YAxisBarColumn : public QWidget {
  Q_OBJECT

 public:
  explicit YAxisBarColumn(QWidget* parent = nullptr);
  void setSignalEntries(const std::vector<SignalEntry>& entries);
  void setCanvasHeight(int h);
  void setSnapAmount(double snap);
  const std::set<int>& selection() const { return m_selected; }
  void setSelection(const std::set<int>& sel);
  void clearSelection();
  void selectAll();
  void setScrollOffset(double offset);
  double scrollOffset() const { return m_scroll_offset; }
  void setTheme(Theme theme);

  static constexpr int DEFAULT_WIDTH = 60;

 signals:
  void yRangeChanged(int index, double y_min, double y_max);
  void bandOffsetChanged(int index, double new_center);
  void bandResized(int index, double new_center, double new_height);
  void barXChanged(int index, double new_bar_x);
  void contextMenuRequested(int index, QPoint global_pos);
  void dragIndexChanged(int index);
  void selectionChanged();
  void editYRangeRequested(int clicked_index);
  void addSignalRequested(double band_center_norm);
  void verticalScrollRequested(double delta);

 protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void mouseDoubleClickEvent(QMouseEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;

 private:
  enum HitZone { NONE, BODY, TOP_EDGE, BOTTOM_EDGE };
  struct HitResult {
    int index = -1;
    HitZone zone = NONE;
  };
  HitResult hitTest(const QPoint& pos) const;
  double axisX(double bar_x_norm) const;

  std::vector<SignalEntry> m_signals;
  std::set<int> m_selected;
  double m_scroll_offset = 0.0;
  Theme m_theme = Theme::Dark;
  HitResult m_drag_hit;
  bool m_drag_moved = false;
  double m_snap_amount = 0.01;
  int m_drag_start_global_x = 0;
  int m_drag_start_global_y = 0;
  double m_drag_start_bar_x = 1.0;
  double m_drag_start_band_center = 0.0;
  double m_drag_start_band_height = 0.0;
  // Rubber band selection
  bool m_rubber_band_active = false;
  bool m_rubber_band_ctrl = false;
  QPoint m_rubber_band_origin;
  QPoint m_rubber_band_current;

  static constexpr int EDGE_GRAB_PIXELS = 8;
  static constexpr int AXIS_PAD_LEFT = 4;
  static constexpr int AXIS_PAD_RIGHT = 10;
};

// Top-level container: label column | bar column in a splitter.
class YAxisPanel : public QWidget {
  Q_OBJECT

 public:
  explicit YAxisPanel(QWidget* parent = nullptr);
  void setSignalEntries(const std::vector<SignalEntry>& entries);
  void setTheme(Theme theme);
  void updateCursorValues(const std::shared_ptr<OverlayManager>& overlay_mgr,
                          double cursor_time);
  void setCanvasHeight(int h);
  void setSnapAmount(double snap);
  void setScrollOffset(double offset);
  const std::set<int>& selection() const;
  void clearSelection();
  void selectAll();

  // Splitter size accessors for layout save/restore
  QList<int> splitterSizes() const { return m_splitter->sizes(); }
  void setSplitterSizes(const QList<int>& sizes) {
    m_splitter->setSizes(sizes);
  }
  QList<int> labelSplitterSizes() const { return m_label_col->splitterSizes(); }
  void setLabelSplitterSizes(const QList<int>& sizes) {
    m_label_col->setSplitterSizes(sizes);
  }

 signals:
  void yRangeChanged(int index, double y_min, double y_max);
  void bandOffsetChanged(int index, double new_center);
  void bandResized(int index, double new_center, double new_height);
  void barXChanged(int index, double new_bar_x);
  void contextMenuRequested(int index, QPoint global_pos);
  void editYRangeRequested(int clicked_index);
  void addSignalRequested(double band_center_norm);
  void selectionChanged();
  void verticalScrollRequested(double delta);

 private:
  QSplitter* m_splitter;
  YAxisLabelColumn* m_label_col;
  YAxisBarColumn* m_bar_col;
  std::vector<SignalEntry> m_signals;
};
