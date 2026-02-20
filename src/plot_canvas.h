#pragma once

#include <QWidget>
#include <QColor>
#include <vector>
#include <string>
#include <set>
#include <map>
#include "PlotJuggler/plotdata.h"

class QLineEdit;
class OverlayManager;

enum class MarkerStyle : int
{
  None = 0,
  FilledCircle,
  OpenCircle,
  FilledSquare,
  OpenSquare,
  FilledTriangle,
  OpenTriangle,
};

struct SignalEntry
{
  std::string name;
  QColor color;
  double y_min = 0.0;
  double y_max = 1.0;
  // Vertical position of this signal's band in normalized [0,1] space.
  // 0 = top of canvas, 1 = bottom. Each signal is assigned a band.
  double band_center = 0.5;
  double band_height = 1.0;  // fraction of canvas height
  double bar_x = 1.0;        // horizontal position of Y-axis bar [0=left, 1=right]
  int divisions = 8;          // Y-axis tick divisions (0 = auto)
  Qt::PenStyle line_style = Qt::SolidLine;
  double line_width = 1.5;
  MarkerStyle marker_style = MarkerStyle::None;

  static constexpr int kMaxDivisions = 32;
  static constexpr int kPixelsPerAutoTick = 32;
};

class PlotCanvas : public QWidget
{
  Q_OBJECT

public:
  explicit PlotCanvas(QWidget* parent = nullptr);

  void setDataSource(OverlayManager* mgr);
  void setSignalEntries(const std::vector<SignalEntry>& entries);

  double cursorTime() const { return _cursor_time; }
  void setCursorTime(double t);

  void resetZoom();

  double viewMinTime() const { return _view_t_min; }
  double viewMaxTime() const { return _view_t_max; }
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
  void timeShiftChanged();  // emitted when a layer's time offset is modified by drag

protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;

private:
  // Map time value to pixel X
  double timeToPixelX(double t) const;
  // Map pixel X to time value
  double pixelXToTime(double px) const;
  // Map signal value to pixel Y using the signal's own axis
  double valueToPixelY(double value, const SignalEntry& sig) const;

  void autoFitTimeRange();
  void drawTimeAxis(class QPainter& painter);
  void drawSignals(class QPainter& painter);
  void drawCursor(class QPainter& painter);
  void drawGrid(class QPainter& painter);

  OverlayManager* _overlay_mgr = nullptr;
  std::vector<SignalEntry> _signals;

  // View range (time axis)
  double _view_t_min = 0.0;
  double _view_t_max = 10.0;
  bool _auto_fit = true;

  // Vertical scroll
  double _scroll_offset = 0.0;
  bool _zoom_mode = false;

  // Cursor
  double _cursor_time = 0.0;
  bool _cursor_dragging = false;
  bool _cursor_needs_data = false;  // set when signals change, cleared when data found

  // Pan state
  bool _panning = false;
  QPoint _pan_start;
  double _pan_t_min_start = 0.0;
  double _pan_t_max_start = 0.0;

  // Zoom-select (rubber band) state
  bool _zoom_selecting = false;
  double _zoom_select_start_x = 0.0;   // pixel X of press
  double _zoom_select_current_x = 0.0; // pixel X of current drag

  // Time shift state
  bool _time_shift_mode = false;
  bool _time_shift_dragging = false;
  QPoint _time_shift_start;
  std::set<int> _selected_layers;
  int _default_shift_layer = 1;
  std::map<int, double> _time_shift_start_offsets;  // original offsets at drag start

  // Time range edit fields
  QLineEdit* _time_start_edit;
  QLineEdit* _time_end_edit;
  void repositionTimeEdits();
  void updateTimeEditTexts();

public:
  // Layout constants — public so YAxisPanel can align with the plot area
  static constexpr int kMarginLeft = 10;
  static constexpr int kMarginRight = 20;
  static constexpr int kMarginTop = 10;
  static constexpr int kMarginBottom = 40;
};
