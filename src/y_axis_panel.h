#pragma once

#include <QWidget>
#include <vector>
#include "plot_canvas.h"

// Single custom-painted widget that draws per-signal Y-axis bars aligned with the
// canvas plot area. Each axis bar spans the same vertical pixel range as the
// signal's band on the PlotCanvas. Bars can be dragged up/down to reposition,
// and top/bottom edges can be dragged to resize the bar (keeping min/max fixed).
class YAxisPanel : public QWidget
{
  Q_OBJECT

public:
  explicit YAxisPanel(QWidget* parent = nullptr);

  void setSignalEntries(const std::vector<SignalEntry>& entries);
  void updateCursorValues(PJ::PlotDataMapRef* data, double cursor_time);

  // Must match the canvas height for pixel alignment
  void setCanvasHeight(int h);

signals:
  void yRangeChanged(int index, double y_min, double y_max);
  void bandOffsetChanged(int index, double new_center);
  void bandResized(int index, double new_center, double new_height);
  void removeSignalRequested(int index);

protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void mouseDoubleClickEvent(QMouseEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;

private:
  // Pixel Y coordinates of a signal's band, matching PlotCanvas layout
  double bandTopY(const SignalEntry& sig) const;
  double bandBottomY(const SignalEntry& sig) const;

  // Hit-testing: which signal and which zone (top-edge, bottom-edge, body)
  enum HitZone { NONE, BODY, TOP_EDGE, BOTTOM_EDGE };
  struct HitResult { int index = -1; HitZone zone = NONE; };
  HitResult hitTest(const QPoint& pos) const;

  std::vector<SignalEntry> _signals;
  std::vector<double> _cursor_values;
  std::vector<bool> _cursor_valid;

  int _canvas_height = 500;

  // Drag state
  HitResult _drag_hit;
  int _drag_start_global_y = 0;
  double _drag_start_band_center = 0.0;
  double _drag_start_band_height = 0.0;
  double _drag_start_y_min = 0.0;
  double _drag_start_y_max = 0.0;

  static constexpr int kPanelWidth = 150;
  static constexpr int kEdgeGrabPixels = 8;  // grab zone for top/bottom edge
};
