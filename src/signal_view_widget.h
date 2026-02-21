#pragma once

#include <QComboBox>
#include <QScrollBar>
#include <QSplitter>
#include <QWidget>
#include <map>
#include <vector>

#include "PlotJuggler/plotdata.h"
#include "data_sets_panel.h"
#include "overlay_manager.h"
#include "plot_canvas.h"
#include "y_axis_panel.h"

class SignalViewWidget : public QWidget {
  Q_OBJECT

 public:
  explicit SignalViewWidget(PJ::PlotDataMapRef* data,
                            QWidget* parent = nullptr);

  PlotCanvas* canvas() { return m_canvas; }
  YAxisPanel* yAxisPanel() { return m_y_axis_panel; }
  QSplitter* mainSplitter() { return m_main_splitter; }
  OverlayManager* overlayManager() { return m_overlay_mgr; }

  const std::vector<SignalEntry>& signalEntries() const { return m_signals; }

  // Serialization helpers
  std::vector<SignalEntry>& signalEntriesMutable() { return m_signals; }
  double cursorTime() const { return m_canvas->cursorTime(); }
  double snapAmount() const { return m_snap_amount; }
  void setSnapAmount(double amount);
  void setSnapIndex(int index);
  void refreshOverlayUI();  // call after programmatic overlay changes (e.g. XML
                            // restore)

 signals:
  void closeRequested();

 private slots:
  void onAddSignal(double band_center_norm = -1.0);
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
  void onAutoScale();
  void onDeleteSelected();
  void onVerticalScroll(double delta);
  void onLoadOverlay();
  void onRemoveOverlay(int layer_index);
  void onStyleLayer(int layer_index);
  void onLayerRenamed(int layer_index, const QString& name);

 private:
  double snapValue(double val) const;
  void refreshViews();
  void autoAssignBands();
  void updateScrollBar();
  void migrateSignalNames(bool add_prefix);
  void updateSelectedLayers();
  void updateShiftLayerCombo();

  PJ::PlotDataMapRef* m_data;
  OverlayManager* m_overlay_mgr;
  std::vector<SignalEntry> m_signals;
  double m_snap_amount = 0.01;
  double m_scroll_offset = 0.0;

  PlotCanvas* m_canvas;
  YAxisPanel* m_y_axis_panel;
  QSplitter* m_main_splitter;
  QComboBox* m_snap_combo;
  QComboBox* m_shift_layer_combo;
  QScrollBar* m_scrollbar;
  DataSetsPanel* m_data_sets_panel;

  // Multi-drag state: original positions captured at drag start
  int m_multi_drag_index = -1;
  std::map<int, double> m_multi_drag_origins;        // band_center_norm origins
  std::map<int, double> m_multi_drag_bar_x_origins;  // bar_x_norm origins

  static constexpr double DEFAULT_BAND_HEIGHT = 0.20;

  // Predefined signal colors
  static const std::vector<QColor>& signalColors();
};
