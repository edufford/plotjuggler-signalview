#pragma once

#include <QWidget>
#include <vector>
#include "plot_canvas.h"
#include "y_axis_panel.h"
#include "PlotJuggler/plotdata.h"

class SignalViewWidget : public QWidget
{
  Q_OBJECT

public:
  explicit SignalViewWidget(PJ::PlotDataMapRef* data, QWidget* parent = nullptr);

  PlotCanvas* canvas() { return _canvas; }
  YAxisPanel* yAxisPanel() { return _y_axis_panel; }

  const std::vector<SignalEntry>& signalEntries() const { return _signals; }

  // Serialization helpers
  std::vector<SignalEntry>& signalEntriesMutable() { return _signals; }
  double cursorTime() const { return _canvas->cursorTime(); }

signals:
  void closeRequested();

private slots:
  void onAddSignal();
  void onRemoveSignal();
  void onResetZoom();
  void onCursorMoved(double time);
  void onYRangeChanged(int index, double y_min, double y_max);
  void onBandOffsetChanged(int index, double new_center);
  void onBandResized(int index, double new_center, double new_height);
  void onBarXChanged(int index, double new_bar_x);
  void onRemoveSignalByIndex(int index);
  void onCanvasResized();

private:
  void refreshViews();
  void autoAssignBands();

  PJ::PlotDataMapRef* _data;
  std::vector<SignalEntry> _signals;

  PlotCanvas* _canvas;
  YAxisPanel* _y_axis_panel;

  // Predefined signal colors
  static const std::vector<QColor>& signalColors();
};
