#pragma once

#include <QWidget>
#include <QComboBox>
#include <QSplitter>
#include <vector>
#include <map>
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
  QSplitter* mainSplitter() { return _main_splitter; }

  const std::vector<SignalEntry>& signalEntries() const { return _signals; }

  // Serialization helpers
  std::vector<SignalEntry>& signalEntriesMutable() { return _signals; }
  double cursorTime() const { return _canvas->cursorTime(); }
  double snapAmount() const { return _snap_amount; }
  void setSnapAmount(double amount);
  void setSnapIndex(int index);

signals:
  void closeRequested();

private slots:
  void onAddSignal(double band_center = -1.0);
  void onRemoveSignal();
  void onResetZoom();
  void onCursorMoved(double time);
  void onYRangeChanged(int index, double y_min, double y_max);
  void onBandOffsetChanged(int index, double new_center);
  void onBandResized(int index, double new_center, double new_height);
  void onBarXChanged(int index, double new_bar_x);
  void onRemoveSignalByIndex(int index);
  void onCanvasResized();
  void onSnapComboChanged(int combo_index);
  void onEditYRange(int clicked_index);
  void onGroupSignals();
  void onDeleteSelected();
  void onVerticalScroll(double delta);

private:
  double snapValue(double val) const;
  void refreshViews();
  void autoAssignBands();

  PJ::PlotDataMapRef* _data;
  std::vector<SignalEntry> _signals;
  double _snap_amount = 0.01;
  double _scroll_offset = 0.0;

  PlotCanvas* _canvas;
  YAxisPanel* _y_axis_panel;
  QSplitter* _main_splitter;
  QComboBox* _snap_combo;

  // Multi-drag state: original positions captured at drag start
  int _multi_drag_index = -1;
  std::map<int, double> _multi_drag_origins;      // band_center origins
  std::map<int, double> _multi_drag_bar_x_origins; // bar_x origins

  static constexpr double kDefaultBandHeight = 0.20;

  // Predefined signal colors
  static const std::vector<QColor>& signalColors();
};
