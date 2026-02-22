#pragma once

#include <QColor>
#include <QWidget>
#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "PlotJuggler/plotdata.h"
#include "overlay_manager.h"

class QLineEdit;

enum class MarkerStyle : int {
  None = 0,
  FilledCircle,
  OpenCircle,
  FilledSquare,
  OpenSquare,
  FilledTriangle,
  OpenTriangle,
};

struct SignalEntry {
  std::string name;
  QColor color;
  double y_min = 0.0;
  double y_max = 1.0;
  // Vertical position of this signal's band in normalized [0,1] space.
  // 0 = top of canvas, 1 = bottom. Each signal is assigned a band.
  double band_center_norm = 0.5;
  double band_height_norm = 1.0;  // fraction of canvas height
  double bar_x_norm =
      1.0;            // horizontal position of Y-axis bar [0=left, 1=right]
  int divisions = 8;  // Y-axis tick divisions (0 = auto)
  Qt::PenStyle line_style = Qt::SolidLine;
  double line_width = 1.5;
  MarkerStyle marker_style = MarkerStyle::None;

  static constexpr int MAX_DIVISIONS = 32;
  static constexpr int PIXELS_PER_AUTO_TICK = 32;

  // Returns the number of Y-axis tick divisions for this signal given the
  // band's pixel height. Uses sig.divisions if set, otherwise auto-scales.
  int tickCount(double band_pixel_h) const {
    return (divisions > 0)
               ? divisions
               : std::clamp((int)(band_pixel_h / PIXELS_PER_AUTO_TICK), 2,
                            MAX_DIVISIONS);
  }
};

class PlotCanvas : public QWidget {
  Q_OBJECT

 public:
  explicit PlotCanvas(QWidget* parent = nullptr);
  ~PlotCanvas() override;

  void setDataSource(std::shared_ptr<OverlayManager> mgr);
  void setSignalEntries(const std::vector<SignalEntry>& entries);

  double cursorTime() const { return m_cursor_time; }
  void setCursorTime(double t);

  void resetZoom();

  double viewMinTime() const { return m_view_t_min; }
  double viewMaxTime() const { return m_view_t_max; }
  void setViewRange(double t_min, double t_max);

  void setScrollOffset(double offset);
  void setZoomMode(bool enabled);
  void setTimeShiftMode(bool enabled);
  void setSelectedLayers(const std::set<int>& layers);
  void setDefaultShiftLayer(int layer_index);

 signals:
  void cursorMoved(double time);
  void viewRangeChanged(double t_min, double t_max);
  void canvasResized(int new_height);
  void verticalScrollRequested(double delta);
  void
  timeShiftChanged();  // emitted when a layer's time offset is modified by drag

 protected:
  void paintEvent(QPaintEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;
  void mouseDoubleClickEvent(QMouseEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;

 private:
  friend class PlotCanvasPathTest;  // for unit testing buildSignalPath

  // Map time value to pixel X
  double timeToPixelX(double t) const;
  // Map pixel X to time value
  double pixelXToTime(double px) const;
  // Map signal value to pixel Y using the signal's own axis
  double valueToPixelY(double value, const SignalEntry& sig) const;

  void autoFitTimeRange();
  void updateIdleCursor();
  void drawTimeAxis(class QPainter& painter);
  void drawSignals(class QPainter& painter);
  void drawCursor(class QPainter& painter);
  void drawGrid(class QPainter& painter);
  // Builds a step-wise QPainterPath for one signal over the current view.
  // Uses per-pixel min/max downsampling when downsample=true.
  class QPainterPath buildSignalPath(const SignalEntry& sig,
                                     const PJ::PlotData& series,
                                     double t_offset, size_t start_idx,
                                     bool downsample) const;

  // Shared with SignalViewWidget; set via setDataSource().
  std::shared_ptr<OverlayManager> m_overlay_mgr;
  std::vector<SignalEntry> m_signals;

  // View range (time axis)
  double m_view_t_min = 0.0;
  double m_view_t_max = 10.0;
  bool m_auto_fit = true;

  // Interaction mode (set externally by toolbar toggles)
  enum class InteractionMode { Normal, Zoom, TimeShift };
  InteractionMode m_mode = InteractionMode::Normal;

  // Active drag state (mutually exclusive; only one active at a time)
  enum class DragState { None, CursorDrag, Panning, ZoomSelect, TimeShiftDrag };
  DragState m_drag_state = DragState::None;

  // Vertical scroll
  double m_scroll_offset = 0.0;

  // Cursor
  double m_cursor_time = 0.0;
  bool m_cursor_needs_data =
      false;  // set when signals change, cleared when data found
  double m_cursor_drag_start_x = 0.0;
  double m_cursor_drag_start_time = 0.0;

  // Pan state
  QPoint m_pan_start;
  double m_pan_t_min_start = 0.0;
  double m_pan_t_max_start = 0.0;

  // Zoom-select (rubber band) state
  double m_zoom_select_start_x = 0.0;    // pixel X of press
  double m_zoom_select_current_x = 0.0;  // pixel X of current drag

  // Time shift state
  QPoint m_time_shift_start;
  std::set<int> m_selected_layers;
  int m_default_shift_layer = 1;
  std::map<int, double>
      m_time_shift_start_offsets;  // original offsets at drag start

  // Time range edit fields
  QLineEdit* m_time_start_edit;
  QLineEdit* m_time_end_edit;
  void repositionTimeEdits();
  void updateTimeEditTexts();

 public:
  // Layout constants — public so YAxisPanel can align with the plot area
  static constexpr int MARGIN_LEFT = 10;
  static constexpr int MARGIN_RIGHT = 20;
  static constexpr int MARGIN_TOP = 10;
  static constexpr int MARGIN_BOTTOM = 40;
  static constexpr int CURSOR_GRAB_PX = 5;
};
