#pragma once

#include <QWidget>
#include <QSplitter>
#include <vector>
#include "plot_canvas.h"

// Shared band-position helpers used by all column widgets.
namespace AxisLayout
{
inline double bandTopY(const SignalEntry& sig, int widget_height)
{
  double plot_h = widget_height - PlotCanvas::kMarginTop - PlotCanvas::kMarginBottom;
  return PlotCanvas::kMarginTop + plot_h * (sig.band_center - sig.band_height * 0.5);
}
inline double bandBottomY(const SignalEntry& sig, int widget_height)
{
  double plot_h = widget_height - PlotCanvas::kMarginTop - PlotCanvas::kMarginBottom;
  return PlotCanvas::kMarginTop + plot_h * (sig.band_center + sig.band_height * 0.5);
}
}  // namespace AxisLayout

// Signal name column: colored dot + signal name, draggable to reposition.
class SignalNameColumn : public QWidget
{
  Q_OBJECT

public:
  explicit SignalNameColumn(QWidget* parent = nullptr);
  void setSignalEntries(const std::vector<SignalEntry>& entries);

  static constexpr int kDefaultWidth = 70;

signals:
  void bandOffsetChanged(int index, double new_center);

protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;

private:
  int hitTestSignal(const QPoint& pos) const;
  static constexpr int kTextRowHeight = 20;
  std::vector<SignalEntry> _signals;
  int _drag_index = -1;
  int _drag_start_global_y = 0;
  double _drag_start_band_center = 0.0;
};

// Signal value column: cursor readout values, vertically aligned with names.
class SignalValueColumn : public QWidget
{
  Q_OBJECT

public:
  explicit SignalValueColumn(QWidget* parent = nullptr);
  void setSignalEntries(const std::vector<SignalEntry>& entries);
  void setCursorValues(const std::vector<double>& values, const std::vector<bool>& valid);

  static constexpr int kDefaultWidth = 70;

signals:
  void bandOffsetChanged(int index, double new_center);

protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;

private:
  int hitTestSignal(const QPoint& pos) const;
  static constexpr int kTextRowHeight = 20;
  std::vector<SignalEntry> _signals;
  std::vector<double> _cursor_values;
  std::vector<bool> _cursor_valid;
  int _drag_index = -1;
  int _drag_start_global_y = 0;
  double _drag_start_band_center = 0.0;
};

// Container: name column | value column in a splitter.
class YAxisLabelColumn : public QWidget
{
  Q_OBJECT

public:
  explicit YAxisLabelColumn(QWidget* parent = nullptr);
  void setSignalEntries(const std::vector<SignalEntry>& entries);
  void setCursorValues(const std::vector<double>& values, const std::vector<bool>& valid);
  void setCanvasHeight(int h);

  static constexpr int kDefaultWidth = SignalNameColumn::kDefaultWidth +
                                       SignalValueColumn::kDefaultWidth + 3;

signals:
  void bandOffsetChanged(int index, double new_center);

private:
  QSplitter* _splitter;
  SignalNameColumn* _name_col;
  SignalValueColumn* _value_col;
};

// Y-axis bars with tick marks, edge grab handles, drag interaction.
class YAxisBarColumn : public QWidget
{
  Q_OBJECT

public:
  explicit YAxisBarColumn(QWidget* parent = nullptr);
  void setSignalEntries(const std::vector<SignalEntry>& entries);
  void setCanvasHeight(int h);

  static constexpr int kDefaultWidth = 60;

signals:
  void yRangeChanged(int index, double y_min, double y_max);
  void bandOffsetChanged(int index, double new_center);
  void bandResized(int index, double new_center, double new_height);
  void barXChanged(int index, double new_bar_x);
  void removeSignalRequested(int index);

protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void mouseDoubleClickEvent(QMouseEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;

private:
  enum HitZone { NONE, BODY, TOP_EDGE, BOTTOM_EDGE };
  struct HitResult { int index = -1; HitZone zone = NONE; };
  HitResult hitTest(const QPoint& pos) const;
  double axisX(double bar_x) const;

  std::vector<SignalEntry> _signals;
  HitResult _drag_hit;
  int _drag_start_global_x = 0;
  int _drag_start_global_y = 0;
  double _drag_start_bar_x = 1.0;
  double _drag_start_band_center = 0.0;
  double _drag_start_band_height = 0.0;

  static constexpr int kEdgeGrabPixels = 8;
  static constexpr int kAxisPadLeft = 4;
  static constexpr int kAxisPadRight = 10;
};

// Top-level container: label column | bar column in a splitter.
class YAxisPanel : public QWidget
{
  Q_OBJECT

public:
  explicit YAxisPanel(QWidget* parent = nullptr);
  void setSignalEntries(const std::vector<SignalEntry>& entries);
  void updateCursorValues(PJ::PlotDataMapRef* data, double cursor_time);
  void setCanvasHeight(int h);

signals:
  void yRangeChanged(int index, double y_min, double y_max);
  void bandOffsetChanged(int index, double new_center);
  void bandResized(int index, double new_center, double new_height);
  void barXChanged(int index, double new_bar_x);
  void removeSignalRequested(int index);

private:
  QSplitter* _splitter;
  YAxisLabelColumn* _label_col;
  YAxisBarColumn* _bar_col;
  std::vector<SignalEntry> _signals;
};
