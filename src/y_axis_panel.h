#pragma once

#include <QWidget>
#include <QSplitter>
#include <vector>
#include "plot_canvas.h"

// Left column: signal name + cursor readout value, vertically aligned with canvas bands.
class YAxisLabelColumn : public QWidget
{
  Q_OBJECT

public:
  explicit YAxisLabelColumn(QWidget* parent = nullptr);

  void setSignalEntries(const std::vector<SignalEntry>& entries);
  void setCursorValues(const std::vector<double>& values, const std::vector<bool>& valid);
  void setCanvasHeight(int h);

signals:
  void bandOffsetChanged(int index, double new_center);

protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;

private:
  double bandTopY(const SignalEntry& sig) const;
  double bandBottomY(const SignalEntry& sig) const;
  int hitTestSignal(const QPoint& pos) const;

  std::vector<SignalEntry> _signals;
  std::vector<double> _cursor_values;
  std::vector<bool> _cursor_valid;

  // Drag state
  int _drag_index = -1;
  int _drag_start_global_y = 0;
  double _drag_start_band_center = 0.0;

public:
  static constexpr int kDefaultWidth = 110;
};

// Right column: Y-axis bars with tick marks, edge grab handles, drag interaction.
class YAxisBarColumn : public QWidget
{
  Q_OBJECT

public:
  explicit YAxisBarColumn(QWidget* parent = nullptr);

  void setSignalEntries(const std::vector<SignalEntry>& entries);
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
  double bandTopY(const SignalEntry& sig) const;
  double bandBottomY(const SignalEntry& sig) const;

  enum HitZone { NONE, BODY, TOP_EDGE, BOTTOM_EDGE };
  struct HitResult { int index = -1; HitZone zone = NONE; };
  HitResult hitTest(const QPoint& pos) const;

  std::vector<SignalEntry> _signals;

  // Drag state
  HitResult _drag_hit;
  int _drag_start_global_y = 0;
  double _drag_start_band_center = 0.0;
  double _drag_start_band_height = 0.0;

public:
  static constexpr int kDefaultWidth = 60;

private:
  static constexpr int kEdgeGrabPixels = 8;
};

// Container that places label column and axis bar column side by side.
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
  void removeSignalRequested(int index);

private:
  QSplitter* _splitter;
  YAxisLabelColumn* _label_col;
  YAxisBarColumn* _bar_col;
  std::vector<SignalEntry> _signals;
};
