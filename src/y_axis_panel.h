#pragma once

#include <QWidget>
#include <QScrollArea>
#include <vector>
#include "plot_canvas.h"

class YAxisBar : public QWidget
{
  Q_OBJECT

public:
  explicit YAxisBar(int index, const SignalEntry& signal, QWidget* parent = nullptr);

  void setSignalEntry(const SignalEntry& sig);
  void setCursorValue(double value, bool valid);

  int signalIndex() const { return _index; }

signals:
  void yRangeChanged(int index, double y_min, double y_max);
  void bandOffsetChanged(int index, double new_center);
  void removeRequested(int index);

protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void mouseDoubleClickEvent(QMouseEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;

private:
  int _index;
  SignalEntry _signal;
  double _cursor_value = 0.0;
  bool _cursor_valid = false;

  // Dragging state for repositioning
  bool _dragging = false;
  int _drag_start_y = 0;
  double _drag_start_center = 0.0;

  static constexpr int kBarWidth = 160;
  static constexpr int kBarHeight = 120;
};

class YAxisPanel : public QWidget
{
  Q_OBJECT

public:
  explicit YAxisPanel(QWidget* parent = nullptr);

  void setSignalEntries(const std::vector<SignalEntry>& entries);
  void updateCursorValues(PJ::PlotDataMapRef* data, double cursor_time);

signals:
  void yRangeChanged(int index, double y_min, double y_max);
  void bandOffsetChanged(int index, double new_center);
  void removeSignalRequested(int index);

private:
  void rebuildBars();

  std::vector<SignalEntry> _signals;
  std::vector<YAxisBar*> _bars;
  QScrollArea* _scroll_area;
  QWidget* _scroll_content;
};
