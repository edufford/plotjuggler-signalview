#include "signal_view_widget.h"

#include <QColorDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleValidator>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QShortcut>
#include <QSplitter>
#include <QStringList>
#include <QTableWidget>
#include <QToolBar>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <numeric>

static constexpr const char* PLUGIN_VERSION = "1.0.0";

// Shared style option definitions used by onEditYRange and onStyleLayer.
struct LineStyleOption {
  QString label;
  Qt::PenStyle style;
};
static const std::vector<LineStyleOption>& lineStyleOptions() {
  static const std::vector<LineStyleOption> opts = {
      {"Solid", Qt::SolidLine},
      {"Dash", Qt::DashLine},
      {"Dot", Qt::DotLine},
      {"Dash-Dot", Qt::DashDotLine},
      {"Dash-Dot-Dot", Qt::DashDotDotLine},
  };
  return opts;
}

struct MarkerStyleOption {
  QString label;
  MarkerStyle style;
};
static const std::vector<MarkerStyleOption>& markerStyleOptions() {
  static const std::vector<MarkerStyleOption> opts = {
      {"None", MarkerStyle::None},
      {"Filled Circle", MarkerStyle::FilledCircle},
      {"Open Circle", MarkerStyle::OpenCircle},
      {"Filled Square", MarkerStyle::FilledSquare},
      {"Open Square", MarkerStyle::OpenSquare},
      {"Filled Triangle", MarkerStyle::FilledTriangle},
      {"Open Triangle", MarkerStyle::OpenTriangle},
  };
  return opts;
}

const std::vector<QColor>& SignalViewWidget::signalColors() {
  static const std::vector<QColor> colors = {
      QColor(0, 180, 255),    // Cyan-blue
      QColor(255, 100, 50),   // Orange-red
      QColor(50, 220, 100),   // Green
      QColor(255, 220, 50),   // Yellow
      QColor(200, 80, 255),   // Purple
      QColor(255, 100, 180),  // Pink
      QColor(100, 255, 220),  // Teal
      QColor(255, 160, 50),   // Amber
  };
  return colors;
}

// Mirrors HSL lightness around L=0.5 (L_new = 1.0 − L).  Applying this
// twice returns the original color exactly, making it a perfect round-trip
// transform for toggling between dark and light themes.  Pure white (L=1)
// maps to pure black (L=0) and vice versa.
static QColor mirrorLightness(const QColor& c) {
  qreal h, s, l, a;
  c.getHslF(&h, &s, &l, &a);
  return QColor::fromHslF(h, s, 1.0 - l, a);
}

SignalViewWidget::SignalViewWidget(PJ::PlotDataMapRef* data, QWidget* parent)
    : QWidget(parent), m_data(data) {
  m_overlay_mgr = std::make_shared<OverlayManager>();
  m_overlay_mgr->setBaseData(m_data);

  setWindowTitle("Signal View");
  resize(900, 500);

  auto* main_layout = new QVBoxLayout(this);
  main_layout->setContentsMargins(2, 2, 2, 2);

  // Toolbar
  auto* toolbar = new QToolBar(this);
  toolbar->setIconSize(QSize(16, 16));

  auto* btn_add = new QPushButton("Add Signal", this);
  auto* btn_remove = new QPushButton("Remove Signal", this);
  auto* btn_group = new QPushButton("Group", this);
  auto* btn_autoscale = new QPushButton("Auto Scale", this);
  auto* btn_reset = new QPushButton("Reset Zoom", this);
  auto* btn_reset_cursor = new QPushButton("Reset Cursor", this);
  auto* btn_close = new QPushButton("Close", this);

  auto* margin_label = new QLabel("Margin:", this);
  margin_label->setStyleSheet("color: #000; font-size: 9px; padding: 0 2px;");
  m_autoscale_margin_spin = new QSpinBox(this);
  m_autoscale_margin_spin->setRange(0, 100);
  m_autoscale_margin_spin->setValue(0);
  m_autoscale_margin_spin->setSuffix("%");
  m_autoscale_margin_spin->setFixedWidth(60);
  m_autoscale_margin_spin->setToolTip(
      "Margin added above and below signal extents when auto-scaling");

  auto* snap_label = new QLabel("Snap:", this);
  snap_label->setStyleSheet("color: #000; font-size: 9px; padding: 0 2px;");
  m_snap_combo = new QComboBox(this);
  m_snap_combo->addItem("Off", 0.0);
  m_snap_combo->addItem("0.002", 0.002);
  m_snap_combo->addItem("0.005", 0.005);
  m_snap_combo->addItem("0.01", 0.01);
  m_snap_combo->addItem("0.02", 0.02);
  m_snap_combo->addItem("0.05", 0.05);
  m_snap_combo->addItem("0.10", 0.10);
  m_snap_combo->addItem("0.20", 0.20);
  m_snap_combo->setCurrentIndex(3);  // default 0.01

  auto* version_label =
      new QLabel(QString("Signal View v%1").arg(PLUGIN_VERSION), this);
  version_label->setStyleSheet("color: #000; font-size: 9px; padding: 0 6px;");

  toolbar->addWidget(version_label);
  toolbar->addSeparator();
  toolbar->addWidget(btn_add);
  toolbar->addWidget(btn_remove);
  toolbar->addWidget(btn_group);
  toolbar->addWidget(btn_autoscale);
  toolbar->addWidget(margin_label);
  toolbar->addWidget(m_autoscale_margin_spin);
  toolbar->addSeparator();
  toolbar->addWidget(btn_reset);
  toolbar->addWidget(btn_reset_cursor);
  toolbar->addSeparator();

  // Streaming mode toggle + buffer time
  m_btn_stream = new QPushButton("Stream", this);
  m_btn_stream->setCheckable(true);
  m_btn_stream->setToolTip(
      "Toggle streaming mode: auto-scroll time axis to follow live data");
  m_btn_stream->setStyleSheet(
      "QPushButton:checked { background: #ffdd00; color: #000; }");
  toolbar->addWidget(m_btn_stream);

  auto* buf_label = new QLabel("Buf:", this);
  buf_label->setStyleSheet("color: #000; font-size: 9px; padding: 0 2px;");
  toolbar->addWidget(buf_label);
  m_stream_buffer_spin = new QDoubleSpinBox(this);
  m_stream_buffer_spin->setRange(1.0, 3600.0);
  m_stream_buffer_spin->setValue(30.0);
  m_stream_buffer_spin->setSuffix(" s");
  m_stream_buffer_spin->setDecimals(1);
  m_stream_buffer_spin->setFixedWidth(80);
  m_stream_buffer_spin->setToolTip("Stream buffer time window (seconds)");
  toolbar->addWidget(m_stream_buffer_spin);
  toolbar->addSeparator();

  toolbar->addWidget(snap_label);
  toolbar->addWidget(m_snap_combo);
  toolbar->addSeparator();
  const QString toggle_style =
      "QPushButton:checked { background: #ffdd00; color: #000; }";
  auto* btn_zoom = new QPushButton("H. Zoom", this);
  btn_zoom->setCheckable(true);
  btn_zoom->setToolTip("Toggle horizontal (time) zoom on scroll wheel");
  btn_zoom->setStyleSheet(toggle_style);
  toolbar->addWidget(btn_zoom);
  auto* btn_prev_zoom = new QPushButton("Prev Zoom", this);
  btn_prev_zoom->setToolTip("Return to previous zoom range");
  btn_prev_zoom->setEnabled(false);
  toolbar->addWidget(btn_prev_zoom);
  toolbar->addSeparator();
  auto* btn_time_shift = new QPushButton("Time Shift", this);
  btn_time_shift->setCheckable(true);
  btn_time_shift->setToolTip("Drag to shift data layer time offsets");
  btn_time_shift->setStyleSheet(toggle_style);
  toolbar->addWidget(btn_time_shift);

  auto* shift_layer_label = new QLabel("Layer:", this);
  shift_layer_label->setStyleSheet(
      "color: #000; font-size: 9px; padding: 0 2px;");
  toolbar->addWidget(shift_layer_label);
  m_shift_layer_combo = new QComboBox(this);
  m_shift_layer_combo->setToolTip(
      "Default layer to shift when no signals are selected");
  toolbar->addWidget(m_shift_layer_combo);
  toolbar->addSeparator();

  auto* btn_overlay = new QPushButton("Overlay", this);
  btn_overlay->setToolTip("Load an overlay data file (CSV) for comparison");
  toolbar->addWidget(btn_overlay);
  toolbar->addSeparator();

  auto* btn_theme = new QPushButton("Dark/Light", this);
  btn_theme->setToolTip("Toggle Dark/Light theme");
  toolbar->addWidget(btn_theme);
  toolbar->addSeparator();
  toolbar->addWidget(btn_close);

  main_layout->addWidget(toolbar);

  // Main content: Y-axis panel | Plot canvas in a splitter
  // nullptr parent: ownership transferred to m_main_splitter via addWidget().
  m_y_axis_panel = new YAxisPanel(nullptr);
  m_canvas = new PlotCanvas(nullptr);
  m_canvas->setDataSource(m_overlay_mgr);

  m_main_splitter = new QSplitter(Qt::Horizontal, this);
  m_main_splitter->setChildrenCollapsible(false);
  m_main_splitter->addWidget(m_y_axis_panel);
  m_main_splitter->addWidget(m_canvas);
  m_main_splitter->setStretchFactor(0, 0);  // panel: don't stretch
  m_main_splitter->setStretchFactor(1, 1);  // canvas: stretch
  m_main_splitter->setSizes({400, 700});
  m_main_splitter->setHandleWidth(4);

  m_scrollbar = new QScrollBar(Qt::Vertical, this);
  m_scrollbar->setRange(0, 0);
  m_scrollbar->setPageStep(1000);
  m_scrollbar->setSingleStep(50);  // matches 0.05 wheel scroll delta

  auto* content_layout = new QHBoxLayout();
  content_layout->setSpacing(0);
  content_layout->setContentsMargins(0, 0, 0, 0);
  content_layout->addWidget(m_main_splitter, 1);
  content_layout->addWidget(m_scrollbar);
  main_layout->addLayout(content_layout, 1);

  // Data Sets Panel (footer)
  m_data_sets_panel = new DataSetsPanel(this);
  main_layout->addWidget(m_data_sets_panel);
  m_data_sets_panel->refresh(m_overlay_mgr);
  updateShiftLayerCombo();

  // Connections
  connect(btn_add, &QPushButton::clicked, this, [this]() { onAddSignal(); });
  connect(btn_remove, &QPushButton::clicked, this,
          &SignalViewWidget::onRemoveSignal);
  connect(btn_group, &QPushButton::clicked, this,
          &SignalViewWidget::onGroupSignals);
  connect(btn_autoscale, &QPushButton::clicked, this,
          &SignalViewWidget::onAutoScale);
  connect(btn_reset, &QPushButton::clicked, this,
          &SignalViewWidget::onResetZoom);
  connect(btn_reset_cursor, &QPushButton::clicked, this, [this]() {
    m_canvas->setCursorTime(m_canvas->viewMinTime());
    onCursorMoved(m_canvas->viewMinTime());
  });
  connect(btn_close, &QPushButton::clicked, this,
          &SignalViewWidget::closeRequested);
  connect(m_snap_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &SignalViewWidget::onSnapComboChanged);
  connect(m_canvas, &PlotCanvas::cursorMoved, this,
          &SignalViewWidget::onCursorMoved);
  connect(m_y_axis_panel, &YAxisPanel::yRangeChanged, this,
          &SignalViewWidget::onYRangeChanged);
  connect(m_y_axis_panel, &YAxisPanel::bandOffsetChanged, this,
          &SignalViewWidget::onBandOffsetChanged);
  connect(m_y_axis_panel, &YAxisPanel::bandResized, this,
          &SignalViewWidget::onBandResized);
  connect(m_y_axis_panel, &YAxisPanel::barXChanged, this,
          &SignalViewWidget::onBarXChanged);
  connect(m_y_axis_panel, &YAxisPanel::zOrderChanged, this,
          &SignalViewWidget::onZOrderChanged);
  connect(m_y_axis_panel, &YAxisPanel::contextMenuRequested, this,
          &SignalViewWidget::onSignalContextMenu);
  connect(m_y_axis_panel, &YAxisPanel::editYRangeRequested, this,
          &SignalViewWidget::onEditYRange);
  connect(m_y_axis_panel, &YAxisPanel::addSignalRequested, this,
          &SignalViewWidget::onAddSignal);
  connect(m_canvas, &PlotCanvas::canvasResized, this,
          &SignalViewWidget::onCanvasResized);

  // Overlay button
  connect(btn_overlay, &QPushButton::clicked, this,
          &SignalViewWidget::onLoadOverlay);

  // Data Sets Panel signals
  connect(m_data_sets_panel, &DataSetsPanel::loadOverlayRequested, this,
          &SignalViewWidget::onLoadOverlay);
  connect(m_data_sets_panel, &DataSetsPanel::removeOverlayRequested, this,
          &SignalViewWidget::onRemoveOverlay);
  connect(m_data_sets_panel, &DataSetsPanel::clearOverlaysRequested, this,
          &SignalViewWidget::onClearOverlays);
  connect(m_data_sets_panel, &DataSetsPanel::styleLayerRequested, this,
          &SignalViewWidget::onStyleLayer);
  connect(m_data_sets_panel, &DataSetsPanel::layerRenamed, this,
          &SignalViewWidget::onLayerRenamed);

  // Prev Zoom button
  connect(btn_prev_zoom, &QPushButton::clicked, m_canvas,
          &PlotCanvas::prevZoom);
  connect(m_canvas, &PlotCanvas::zoomStackChanged, btn_prev_zoom,
          &QPushButton::setEnabled);

  // Zoom mode toggle
  connect(btn_zoom, &QPushButton::toggled, this,
          [this, btn_time_shift](bool checked) {
            m_canvas->setZoomMode(checked);
            if (checked) {
              btn_time_shift->setChecked(false);
            }
          });

  // Time shift mode toggle
  connect(btn_time_shift, &QPushButton::toggled, this,
          [this, btn_zoom](bool checked) {
            m_canvas->setTimeShiftMode(checked);
            if (checked) {
              btn_zoom->setChecked(false);
            }
          });

  // Dark/Light theme toggle: apply HSL lightness mirror (L → 0.9 − L) to
  // every signal color.  Applying it twice restores the original color, so
  // toggling back and forth is lossless for all colors.
  connect(btn_theme, &QPushButton::clicked, this, [this]() {
    for (auto& sig : m_signals) {
      sig.color = mirrorLightness(sig.color);
    }
    m_theme = (m_theme == Theme::Dark) ? Theme::Light : Theme::Dark;
    m_canvas->setTheme(m_theme);
    m_y_axis_panel->setTheme(m_theme);
    refreshViews();
  });

  // Shift layer combo
  connect(m_shift_layer_combo,
          QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [this](int idx) {
            int layer = m_shift_layer_combo->itemData(idx).toInt();
            m_canvas->setDefaultShiftLayer(layer);
          });

  // Update selected layers when Y-axis panel selection changes
  connect(m_y_axis_panel, &YAxisPanel::selectionChanged, this,
          [this]() { updateSelectedLayers(); });

  // When time shift is dragged, update DataSetsPanel offsets and cursor readout
  connect(m_canvas, &PlotCanvas::timeShiftChanged, this, [this]() {
    m_data_sets_panel->updateOffsets(m_overlay_mgr);
    m_y_axis_panel->updateCursorValues(m_overlay_mgr, m_canvas->cursorTime());
  });

  // Vertical scroll from all sources
  connect(m_canvas, &PlotCanvas::verticalScrollRequested, this,
          &SignalViewWidget::onVerticalScroll);
  connect(m_y_axis_panel, &YAxisPanel::verticalScrollRequested, this,
          &SignalViewWidget::onVerticalScroll);

  // Streaming mode
  connect(m_btn_stream, &QPushButton::toggled, this, [this](bool checked) {
    m_canvas->setStreamBufferSeconds(m_stream_buffer_spin->value());
    m_canvas->setStreamingMode(checked);
  });
  connect(m_stream_buffer_spin,
          QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          [this](double val) { m_canvas->setStreamBufferSeconds(val); });
  // Sync button state when streaming is cancelled by user interaction
  connect(m_canvas, &PlotCanvas::streamingModeChanged, m_btn_stream,
          &QPushButton::setChecked);

  // Scrollbar
  connect(m_scrollbar, &QScrollBar::valueChanged, this, [this](int value) {
    m_scroll_offset = value / 1000.0;
    m_canvas->setScrollOffset(m_scroll_offset);
    m_y_axis_panel->setScrollOffset(m_scroll_offset);
  });

  auto* delete_shortcut = new QShortcut(Qt::Key_Delete, this);
  delete_shortcut->setContext(Qt::WindowShortcut);
  connect(delete_shortcut, &QShortcut::activated, this,
          &SignalViewWidget::onDeleteSelected);

  auto* select_all_shortcut = new QShortcut(QKeySequence::SelectAll, this);
  select_all_shortcut->setContext(Qt::WindowShortcut);
  connect(select_all_shortcut, &QShortcut::activated, this,
          [this]() { m_y_axis_panel->selectAll(); });

  updateScrollBar();
}

void SignalViewWidget::onAddSignal(double band_center_norm) {
  if (!m_overlay_mgr) {
    return;
  }

  auto allm_signals = m_overlay_mgr->allAvailableSignals();
  QStringList available;
  for (const auto& name : allm_signals) {
    available.append(QString::fromStdString(name));
  }

  if (available.isEmpty()) {
    return;
  }

  available.sort();

  // Custom dialog with filter + multi-select list
  QDialog dlg(this);
  dlg.setWindowTitle("Add Signals");
  dlg.resize(400, 500);
  auto* layout = new QVBoxLayout(&dlg);

  auto* filter_edit = new QLineEdit(&dlg);
  filter_edit->setPlaceholderText("Type to filter...");
  layout->addWidget(filter_edit);

  auto* list = new QListWidget(&dlg);
  list->setSelectionMode(QAbstractItemView::ExtendedSelection);
  for (const auto& name : available) {
    list->addItem(name);
  }
  layout->addWidget(list);

  auto* buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
  layout->addWidget(buttons);
  connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

  // Filter: show/hide items as user types
  connect(filter_edit, &QLineEdit::textChanged, [&](const QString& text) {
    for (int i = 0; i < list->count(); i++) {
      auto* item = list->item(i);
      item->setHidden(!item->text().contains(text, Qt::CaseInsensitive));
    }
  });

  if (dlg.exec() != QDialog::Accepted) {
    return;
  }

  auto selected_items = list->selectedItems();
  if (selected_items.isEmpty()) {
    return;
  }

  // Treat click position as top edge of first band, not center
  double pos = (band_center_norm >= 0.0)
                   ? band_center_norm + DEFAULT_BAND_HEIGHT * 0.5
                   : DEFAULT_BAND_HEIGHT * 0.5;
  for (auto* item : selected_items) {
    SignalEntry entry;
    entry.name = item->text().toStdString();
    entry.color = signalColors()[m_signals.size() % signalColors().size()];
    if (m_theme == Theme::Light) {
      entry.color = mirrorLightness(entry.color);
    }

    // Auto-detect Y range from data
    auto resolved = m_overlay_mgr->resolveSignal(entry.name);
    if (resolved && resolved->series->size() > 0) {
      auto range = resolved->series->rangeY();
      if (range) {
        double margin = (range->max - range->min) * 0.1;
        if (margin < 1e-9) {
          margin = 1.0;
        }
        entry.y_min = range->min - margin;
        entry.y_max = range->max + margin;
      }
    }

    entry.band_height_norm = DEFAULT_BAND_HEIGHT;
    entry.band_center_norm = snapValue(pos);
    pos += DEFAULT_BAND_HEIGHT;

    m_signals.push_back(entry);
  }
  // Assign distinct z_orders so stacked signals have a defined visual order
  // from the start. Higher index (painted on top) gets the higher z_order.
  normalizeZOrders();
  refreshViews();
}

void SignalViewWidget::onRemoveSignal() {
  if (m_signals.empty()) {
    return;
  }

  QStringList names;
  for (const auto& sig : m_signals) {
    names.append(QString::fromStdString(sig.name));
  }

  bool ok;
  QString selected =
      QInputDialog::getItem(this, "Remove Signal",
                            "Select a signal to remove:", names, 0, false, &ok);

  if (!ok || selected.isEmpty()) {
    return;
  }

  std::string name = selected.toStdString();
  m_signals.erase(
      std::remove_if(m_signals.begin(), m_signals.end(),
                     [&](const SignalEntry& s) { return s.name == name; }),
      m_signals.end());

  refreshViews();
}

void SignalViewWidget::onResetZoom() { m_canvas->resetZoom(); }

void SignalViewWidget::onCursorMoved(double time) {
  m_y_axis_panel->updateCursorValues(m_overlay_mgr, time);
}

void SignalViewWidget::onYRangeChanged(int index, double y_min, double y_max) {
  if (index < 0 || index >= static_cast<int>(m_signals.size())) {
    return;
  }

  const auto& sel = m_y_axis_panel->selection();
  if (sel.count(index) && sel.size() > 1) {
    // Multi-zoom: apply the same scale factor to all selected signals
    double old_range = m_signals[index].y_max - m_signals[index].y_min;
    double new_range = y_max - y_min;
    if (old_range > 1e-12) {
      double factor = new_range / old_range;
      for (int i : sel) {
        double center = (m_signals[i].y_min + m_signals[i].y_max) * 0.5;
        double half = (m_signals[i].y_max - m_signals[i].y_min) * 0.5 * factor;
        m_signals[i].y_min = center - half;
        m_signals[i].y_max = center + half;
      }
    }
  } else {
    m_signals[index].y_min = y_min;
    m_signals[index].y_max = y_max;
  }
  refreshViews();
}

void SignalViewWidget::onBandOffsetChanged(int index, double new_center) {
  if (index < 0 || index >= static_cast<int>(m_signals.size())) {
    return;
  }

  std::set<int> sel = m_y_axis_panel->selection();
  if (sel.count(index) && sel.size() > 1) {
    // Multi-drag: move all selected signals together, preserving relative
    // positions. Capture original band_centers when a new drag starts.
    bool need_init =
        (m_multi_drag_index != index || m_multi_drag_origins.empty());
    if (!need_init) {
      for (int i : sel) {
        if (m_multi_drag_origins.find(i) == m_multi_drag_origins.end()) {
          need_init = true;
          break;
        }
      }
    }
    if (need_init) {
      m_multi_drag_index = index;
      m_multi_drag_origins.clear();
      m_multi_drag_bar_x_origins.clear();
      for (int i : sel) {
        m_multi_drag_origins[i] = m_signals[i].band_center_norm;
        m_multi_drag_bar_x_origins[i] = m_signals[i].bar_x_norm;
      }
    }

    // Compute snapped delta from the dragged signal
    double half_h = m_signals[index].band_height_norm * 0.5;
    double new_top = snapValue(new_center - half_h);
    double snapped_center = new_top + half_h;
    double delta = snapped_center - m_multi_drag_origins[index];

    // Clamp delta so no selected signal's top edge goes above position 0
    for (auto& [i, origin] : m_multi_drag_origins) {
      double half = m_signals[i].band_height_norm * 0.5;
      delta = std::max(delta, half - origin);
    }
    for (auto& [i, origin] : m_multi_drag_origins) {
      m_signals[i].band_center_norm = origin + delta;
    }
  } else {
    m_multi_drag_origins.clear();
    m_multi_drag_bar_x_origins.clear();
    m_multi_drag_index = -1;

    double half_h = m_signals[index].band_height_norm * 0.5;
    double new_top = std::max(0.0, snapValue(new_center - half_h));
    m_signals[index].band_center_norm = new_top + half_h;
  }

  m_canvas->setSignalEntries(m_signals);
  m_y_axis_panel->setSignalEntries(m_signals);
  updateScrollBar();
}

void SignalViewWidget::onBandResized(int index, double new_center,
                                     double new_height) {
  if (index < 0 || index >= static_cast<int>(m_signals.size())) {
    return;
  }

  double new_top = new_center - new_height * 0.5;
  double new_bottom = new_center + new_height * 0.5;

  // Only snap the edge that's actually moving, leave the fixed edge alone
  double cur_top = m_signals[index].band_center_norm -
                   m_signals[index].band_height_norm * 0.5;
  double cur_bottom = m_signals[index].band_center_norm +
                      m_signals[index].band_height_norm * 0.5;

  bool top_moving = std::abs(new_top - cur_top) > 1e-6;
  bool bottom_moving = std::abs(new_bottom - cur_bottom) > 1e-6;

  if (top_moving) {
    new_top = std::max(0.0, snapValue(new_top));
  }
  if (bottom_moving) {
    new_bottom = snapValue(new_bottom);
  }

  if (new_bottom - new_top < 0.02) {
    return;
  }

  const auto& sel = m_y_axis_panel->selection();
  if (sel.count(index) && sel.size() > 1) {
    // Multi-resize: apply the same height delta to all selected signals.
    // The moving edge shifts by delta; the fixed edge stays put.
    double height_delta =
        (new_bottom - new_top) - m_signals[index].band_height_norm;

    for (int i : sel) {
      double i_top =
          m_signals[i].band_center_norm - m_signals[i].band_height_norm * 0.5;
      double i_bottom =
          m_signals[i].band_center_norm + m_signals[i].band_height_norm * 0.5;

      if (top_moving) {
        i_top -= height_delta;  // top edge moves up when growing
      } else {
        i_bottom += height_delta;  // bottom edge moves down when growing
      }

      if (i_bottom - i_top < 0.02) {
        continue;
      }
      m_signals[i].band_center_norm = (i_top + i_bottom) * 0.5;
      m_signals[i].band_height_norm = i_bottom - i_top;
    }
  } else {
    m_signals[index].band_center_norm = (new_top + new_bottom) * 0.5;
    m_signals[index].band_height_norm = new_bottom - new_top;
  }

  m_canvas->setSignalEntries(m_signals);
  m_y_axis_panel->setSignalEntries(m_signals);
  updateScrollBar();
}

void SignalViewWidget::onBarXChanged(int index, double new_bar_x) {
  if (index < 0 || index >= static_cast<int>(m_signals.size())) {
    return;
  }

  const auto& sel = m_y_axis_panel->selection();
  if (sel.count(index) && sel.size() > 1 &&
      !m_multi_drag_bar_x_origins.empty()) {
    // Multi-drag: apply the same horizontal delta to all selected signals
    double snapped = snapValue(new_bar_x);
    double delta = snapped - m_multi_drag_bar_x_origins[index];

    // Clamp delta so no selected signal leaves [0, 1]
    for (auto& [i, origin] : m_multi_drag_bar_x_origins) {
      delta = std::max(delta, -origin);
      delta = std::min(delta, 1.0 - origin);
    }
    for (auto& [i, origin] : m_multi_drag_bar_x_origins) {
      m_signals[i].bar_x_norm = origin + delta;
    }
  } else {
    m_signals[index].bar_x_norm = snapValue(new_bar_x);
  }
  m_canvas->setSignalEntries(m_signals);
  m_y_axis_panel->setSignalEntries(m_signals);
}

void SignalViewWidget::normalizeZOrders() {
  // Assign each signal a unique z_order in [0, n-1] that preserves the
  // current relative ranking. Ties are broken by index order (higher index
  // gets higher z_order), which matches the stable-sort paint order so the
  // visually topmost bar in a fresh stack is also the one with the highest
  // z_order.
  std::vector<int> idx(m_signals.size());
  std::iota(idx.begin(), idx.end(), 0);
  std::stable_sort(idx.begin(), idx.end(), [&](int a, int b) {
    return m_signals[a].z_order < m_signals[b].z_order;
  });
  for (int rank = 0; rank < static_cast<int>(idx.size()); ++rank) {
    m_signals[idx[rank]].z_order = rank;
  }
}

void SignalViewWidget::onZOrderChanged(int index, int new_z_order) {
  if (index < 0 || index >= static_cast<int>(m_signals.size())) {
    return;
  }
  m_signals[index].z_order = new_z_order;

  // Renormalize to [0, n-1] so values never grow unboundedly.
  normalizeZOrders();

  m_canvas->setSignalEntries(m_signals);
  m_y_axis_panel->setSignalEntries(m_signals);
}

void SignalViewWidget::onCanvasResized() {
  m_y_axis_panel->setCanvasHeight(m_canvas->height());
}

void SignalViewWidget::onSnapComboChanged(int combo_index) {
  m_snap_amount = m_snap_combo->itemData(combo_index).toDouble();
  m_y_axis_panel->setSnapAmount(m_snap_amount);
}

double SignalViewWidget::snapValue(double val) const {
  if (m_snap_amount <= 0.0) {
    return val;
  }
  return std::round(val / m_snap_amount) * m_snap_amount;
}

void SignalViewWidget::setSnapAmount(double amount) {
  m_snap_amount = amount;
  m_y_axis_panel->setSnapAmount(amount);
  // Find matching combo index
  for (int i = 0; i < m_snap_combo->count(); i++) {
    if (std::abs(m_snap_combo->itemData(i).toDouble() - amount) < 1e-9) {
      m_snap_combo->setCurrentIndex(i);
      return;
    }
  }
  m_snap_combo->setCurrentIndex(0);  // fallback to Off
}

void SignalViewWidget::setSnapIndex(int index) {
  if (index >= 0 && index < m_snap_combo->count()) {
    m_snap_combo->setCurrentIndex(index);
  }
}

void SignalViewWidget::applyTheme(Theme t) {
  m_theme = t;
  m_canvas->setTheme(t);
  m_y_axis_panel->setTheme(t);
}

bool SignalViewWidget::streamingMode() const {
  return m_canvas->streamingMode();
}

double SignalViewWidget::streamBufferSeconds() const {
  return m_stream_buffer_spin->value();
}

void SignalViewWidget::setStreamingMode(bool enabled) {
  m_btn_stream->setChecked(enabled);
}

void SignalViewWidget::setStreamBufferSeconds(double secs) {
  m_stream_buffer_spin->setValue(secs);
  m_canvas->setStreamBufferSeconds(secs);
}

void SignalViewWidget::refreshOverlayUI() {
  m_data_sets_panel->refresh(m_overlay_mgr);
  updateShiftLayerCombo();
  refreshViews();
}

void SignalViewWidget::onRemoveSignalByIndex(int index) {
  if (index < 0 || index >= static_cast<int>(m_signals.size())) {
    return;
  }
  m_signals.erase(m_signals.begin() + index);
  refreshViews();
}

void SignalViewWidget::onSignalContextMenu(int index, QPoint global_pos) {
  if (index < 0 || index >= static_cast<int>(m_signals.size())) {
    return;
  }

  // If the right-clicked signal is part of the current selection, all actions
  // apply to the full selection. Otherwise they apply only to that one signal.
  std::set<int> sel = m_y_axis_panel->selection();
  const bool in_selection = sel.count(index) > 0;
  if (!in_selection) {
    sel = {index};
  }

  QMenu menu(this);
  menu.addAction("Auto Scale", [this, sel]() { autoScaleIndices(sel); });
  menu.addAction("Change Style...", [this, index]() { onEditYRange(index); });
  auto* group_action = menu.addAction("Group", [this]() { onGroupSignals(); });
  group_action->setEnabled(in_selection && sel.size() >= 2);
  menu.addSeparator();
  menu.addAction("Remove", [this, index, in_selection]() {
    if (in_selection) {
      onDeleteSelected();
    } else {
      onRemoveSignalByIndex(index);
    }
  });
  menu.exec(global_pos);
}

void SignalViewWidget::refreshViews() {
  m_canvas->setSignalEntries(m_signals);
  m_y_axis_panel->setSignalEntries(m_signals);
  m_y_axis_panel->updateCursorValues(m_overlay_mgr, m_canvas->cursorTime());
  updateScrollBar();
}

void SignalViewWidget::updateScrollBar() {
  // Find the maximum bottom extent of all signals in normalized coordinates
  double max_bottom = 1.0;
  for (const auto& sig : m_signals) {
    double bottom = sig.band_center_norm + sig.band_height_norm * 0.5;
    if (bottom > max_bottom) {
      max_bottom = bottom;
    }
  }
  // Ensure at least 2 screens worth of scrollable space
  double max_extent = std::max(max_bottom, 2.0);
  // Scrollable range: from 0 to (max_extent - 1.0), scaled by 1000
  int range = std::max(0, static_cast<int>((max_extent - 1.0) * 1000));
  m_scrollbar->blockSignals(true);
  m_scrollbar->setRange(0, range);
  m_scrollbar->setValue(static_cast<int>(m_scroll_offset * 1000));
  m_scrollbar->blockSignals(false);
}

// Per-row widget pointers for the Edit Y Range dialog table.
struct EditRowWidgets {
  QLineEdit* width_edit;
  QLineEdit* min_edit;
  QLineEdit* max_edit;
  QLineEdit* div_edit;
  QPushButton* color_btn;
  QComboBox* style_combo;
  QComboBox* marker_combo;
  QColor color;
};

static void setColorBtnStyle(QPushButton* btn, const QColor& c) {
  btn->setStyleSheet(QString("background-color: %1; border: 1px solid #888; "
                             "min-width: 28px; max-width: 28px;")
                         .arg(c.name()));
}

// Custom selection model: clicking a widget cell in an already-selected row
// preserves the multi-row selection instead of clearing it.
class ColumnGuardSelectionModel : public QItemSelectionModel {
 public:
  using QItemSelectionModel::QItemSelectionModel;
  void select(const QModelIndex& index,
              QItemSelectionModel::SelectionFlags command) override {
    if (index.isValid() && index.column() >= 1 && index.column() <= 7) {
      if (isRowSelected(index.row(), index.parent())) {
        return;  // row already selected — preserve multi-selection
      }
      QItemSelectionModel::select(index, QItemSelectionModel::ClearAndSelect |
                                             QItemSelectionModel::Rows);
      return;
    }
    QItemSelectionModel::select(index, command);
  }
  void select(const QItemSelection& selection,
              QItemSelectionModel::SelectionFlags command) override {
    QItemSelectionModel::select(selection, command);
  }
  void setCurrentIndex(const QModelIndex& index,
                       QItemSelectionModel::SelectionFlags command) override {
    if (index.isValid() && index.column() >= 1 && index.column() <= 7) {
      if (isRowSelected(index.row(), index.parent())) {
        QItemSelectionModel::setCurrentIndex(index,
                                             QItemSelectionModel::NoUpdate);
        return;
      }
      QItemSelectionModel::setCurrentIndex(
          index,
          QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
      return;
    }
    QItemSelectionModel::setCurrentIndex(index, command);
  }
};

// Populate one row of the Edit Y Range table from a SignalEntry.
static EditRowWidgets populateEditRow(QTableWidget* table, int row,
                                      const SignalEntry& sig, QWidget* parent) {
  EditRowWidgets w;

  // Signal name (read-only)
  auto* name_item = new QTableWidgetItem(QString::fromStdString(sig.name));
  name_item->setFlags(name_item->flags() & ~Qt::ItemIsEditable);
  name_item->setForeground(sig.color);
  table->setItem(row, 0, name_item);

  // Color button
  w.color = sig.color;
  w.color_btn = new QPushButton(parent);
  setColorBtnStyle(w.color_btn, sig.color);
  table->setCellWidget(row, 1, w.color_btn);

  // Line style combo
  w.style_combo = new QComboBox(parent);
  int current_style_idx = 0;
  const auto& ls = lineStyleOptions();
  for (int s = 0; s < static_cast<int>(ls.size()); s++) {
    w.style_combo->addItem(ls[s].label, static_cast<int>(ls[s].style));
    if (ls[s].style == sig.line_style) {
      current_style_idx = s;
    }
  }
  w.style_combo->setCurrentIndex(current_style_idx);
  table->setCellWidget(row, 2, w.style_combo);

  // Marker style combo
  w.marker_combo = new QComboBox(parent);
  int current_marker_idx = 0;
  const auto& ms = markerStyleOptions();
  for (int m = 0; m < static_cast<int>(ms.size()); m++) {
    w.marker_combo->addItem(ms[m].label, static_cast<int>(ms[m].style));
    if (ms[m].style == sig.marker_style) {
      current_marker_idx = m;
    }
  }
  w.marker_combo->setCurrentIndex(current_marker_idx);
  table->setCellWidget(row, 3, w.marker_combo);

  // Line width
  w.width_edit = new QLineEdit(parent);
  w.width_edit->setValidator(new QDoubleValidator(0.1, 10.0, 1, parent));
  w.width_edit->setText(QString::number(sig.line_width, 'f', 1));
  table->setCellWidget(row, 4, w.width_edit);

  // Y Min / Y Max
  w.min_edit = new QLineEdit(parent);
  w.min_edit->setValidator(new QDoubleValidator(parent));
  w.min_edit->setText(QString::number(sig.y_min, 'g', 6));
  table->setCellWidget(row, 5, w.min_edit);

  w.max_edit = new QLineEdit(parent);
  w.max_edit->setValidator(new QDoubleValidator(parent));
  w.max_edit->setText(QString::number(sig.y_max, 'g', 6));
  table->setCellWidget(row, 6, w.max_edit);

  // Divisions (0 = auto)
  w.div_edit = new QLineEdit(parent);
  w.div_edit->setValidator(
      new QIntValidator(0, SignalEntry::MAX_DIVISIONS, parent));
  w.div_edit->setText(QString::number(sig.divisions));
  table->setCellWidget(row, 7, w.div_edit);

  return w;
}

// Wire cross-row propagation: editing a value in one selected row copies it
// to all other selected rows in the same column.
static void wireEditTablePropagation(QTableWidget* table,
                                     std::vector<EditRowWidgets>& rows,
                                     QDialog* dlg) {
  auto editForCol = [&](int row, int col) -> QLineEdit* {
    if (col == 4) {
      return rows[row].width_edit;
    }
    if (col == 5) {
      return rows[row].min_edit;
    }
    if (col == 6) {
      return rows[row].max_edit;
    }
    return rows[row].div_edit;
  };
  auto propagateText = [=, &rows](int source_row, int col) {
    QString text = editForCol(source_row, col)->text();
    auto selected = table->selectionModel()->selectedRows();
    for (const auto& mi : selected) {
      int r = mi.row();
      if (r == source_row) {
        continue;
      }
      QLineEdit* target = editForCol(r, col);
      target->blockSignals(true);
      target->setText(text);
      target->blockSignals(false);
    }
  };
  auto isRowSelected = [table](int row) {
    for (const auto& mi : table->selectionModel()->selectedRows()) {
      if (mi.row() == row) {
        return true;
      }
    }
    return false;
  };

  for (int row = 0; row < static_cast<int>(rows.size()); row++) {
    // Text edits: width, min, max, divisions
    QObject::connect(rows[row].width_edit, &QLineEdit::textEdited, dlg,
                     [propagateText, row]() { propagateText(row, 4); });
    QObject::connect(rows[row].min_edit, &QLineEdit::textEdited, dlg,
                     [propagateText, row]() { propagateText(row, 5); });
    QObject::connect(rows[row].max_edit, &QLineEdit::textEdited, dlg,
                     [propagateText, row]() { propagateText(row, 6); });
    QObject::connect(rows[row].div_edit, &QLineEdit::textEdited, dlg,
                     [propagateText, row]() { propagateText(row, 7); });

    // Line style combo
    QObject::connect(rows[row].style_combo,
                     QOverload<int>::of(&QComboBox::currentIndexChanged), dlg,
                     [&rows, table, isRowSelected, row](int idx) {
                       if (!isRowSelected(row)) {
                         return;
                       }
                       auto selected = table->selectionModel()->selectedRows();
                       if (selected.size() <= 1) {
                         return;
                       }
                       for (const auto& mi : selected) {
                         int r = mi.row();
                         if (r == row) {
                           continue;
                         }
                         rows[r].style_combo->blockSignals(true);
                         rows[r].style_combo->setCurrentIndex(idx);
                         rows[r].style_combo->blockSignals(false);
                       }
                     });

    // Marker style combo
    QObject::connect(rows[row].marker_combo,
                     QOverload<int>::of(&QComboBox::currentIndexChanged), dlg,
                     [&rows, table, isRowSelected, row](int idx) {
                       if (!isRowSelected(row)) {
                         return;
                       }
                       auto selected = table->selectionModel()->selectedRows();
                       if (selected.size() <= 1) {
                         return;
                       }
                       for (const auto& mi : selected) {
                         int r = mi.row();
                         if (r == row) {
                           continue;
                         }
                         rows[r].marker_combo->blockSignals(true);
                         rows[r].marker_combo->setCurrentIndex(idx);
                         rows[r].marker_combo->blockSignals(false);
                       }
                     });

    // Color button
    QObject::connect(rows[row].color_btn, &QPushButton::clicked, dlg,
                     [&rows, table, dlg, row]() {
                       QColor chosen = QColorDialog::getColor(
                           rows[row].color, dlg, "Signal Color");
                       if (!chosen.isValid()) {
                         return;
                       }
                       auto selected = table->selectionModel()->selectedRows();
                       bool row_sel = false;
                       for (const auto& mi : selected) {
                         if (mi.row() == row) {
                           row_sel = true;
                           break;
                         }
                       }
                       if (row_sel && selected.size() > 1) {
                         for (const auto& mi : selected) {
                           int r = mi.row();
                           rows[r].color = chosen;
                           setColorBtnStyle(rows[r].color_btn, chosen);
                           table->item(r, 0)->setForeground(chosen);
                         }
                       } else {
                         rows[row].color = chosen;
                         setColorBtnStyle(rows[row].color_btn, chosen);
                         table->item(row, 0)->setForeground(chosen);
                       }
                     });
  }
}

// Apply accepted dialog values back to signal entries.
static void applyEditTableResults(const std::vector<EditRowWidgets>& rows,
                                  const std::vector<int>& row_to_idx,
                                  std::vector<SignalEntry>& entries) {
  for (int row = 0; row < static_cast<int>(row_to_idx.size()); row++) {
    int sig_idx = row_to_idx[row];
    const auto& w = rows[row];
    entries[sig_idx].color = w.color;
    entries[sig_idx].line_style =
        static_cast<Qt::PenStyle>(w.style_combo->currentData().toInt());
    entries[sig_idx].marker_style =
        static_cast<MarkerStyle>(w.marker_combo->currentData().toInt());
    bool ok_w = false;
    double new_w = w.width_edit->text().toDouble(&ok_w);
    if (ok_w && new_w >= 0.1 && new_w <= 10.0) {
      entries[sig_idx].line_width = new_w;
    }
    bool ok_min = false, ok_max = false;
    double new_min = w.min_edit->text().toDouble(&ok_min);
    double new_max = w.max_edit->text().toDouble(&ok_max);
    if (ok_min && ok_max && new_max > new_min) {
      entries[sig_idx].y_min = new_min;
      entries[sig_idx].y_max = new_max;
    }
    bool ok_div = false;
    int new_div = w.div_edit->text().toInt(&ok_div);
    if (ok_div && new_div >= 0) {
      entries[sig_idx].divisions = new_div;
    }
  }
}

void SignalViewWidget::onEditYRange(int clicked_index) {
  if (clicked_index < 0 ||
      clicked_index >= static_cast<int>(m_signals.size())) {
    return;
  }

  // Build the set of signal indices to show: selection + clicked index
  std::set<int> sel = m_y_axis_panel->selection();
  sel.insert(clicked_index);
  std::vector<int> row_to_idx(sel.begin(), sel.end());

  QDialog dlg(this);
  dlg.setWindowTitle("Edit Y Range");
  auto* layout = new QVBoxLayout(&dlg);

  auto* table = new QTableWidget(static_cast<int>(row_to_idx.size()), 8, &dlg);
  table->setHorizontalHeaderLabels({"Signal", "Color", "Line Style", "Marker",
                                    "Line Width", "Y Min", "Y Max",
                                    "Divisions"});
  table->horizontalHeader()->setStretchLastSection(true);
  table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
  for (int col = 1; col <= 4; col++) {
    table->horizontalHeader()->setSectionResizeMode(
        col, QHeaderView::ResizeToContents);
  }
  table->setSelectionBehavior(QAbstractItemView::SelectRows);
  table->setSelectionMode(QAbstractItemView::ExtendedSelection);
  table->verticalHeader()->setVisible(false);
  table->setSelectionModel(
      new ColumnGuardSelectionModel(table->model(), table));

  // Populate rows
  std::vector<EditRowWidgets> rows;
  rows.reserve(row_to_idx.size());
  for (int row = 0; row < static_cast<int>(row_to_idx.size()); row++) {
    rows.push_back(
        populateEditRow(table, row, m_signals[row_to_idx[row]], &dlg));
  }
  table->selectAll();

  // Wire cross-row propagation
  wireEditTablePropagation(table, rows, &dlg);

  auto* buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
  connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
  layout->addWidget(table);
  layout->addWidget(buttons);
  dlg.resize(920, 50 + 30 * static_cast<int>(row_to_idx.size()) + 60);

  if (dlg.exec() != QDialog::Accepted) {
    return;
  }

  applyEditTableResults(rows, row_to_idx, m_signals);
  refreshViews();
}

void SignalViewWidget::onDeleteSelected() {
  const auto& sel = m_y_axis_panel->selection();
  if (sel.empty()) {
    return;
  }

  // Remove from highest index to lowest so indices stay valid
  std::vector<int> indices(sel.begin(), sel.end());
  std::sort(indices.rbegin(), indices.rend());
  for (int i : indices) {
    if (i >= 0 && i < static_cast<int>(m_signals.size())) {
      m_signals.erase(m_signals.begin() + i);
    }
  }

  m_y_axis_panel->clearSelection();
  refreshViews();
}

void SignalViewWidget::onGroupSignals() {
  const auto& sel = m_y_axis_panel->selection();
  if (sel.size() < 2) {
    return;
  }

  // Find the topmost selected signal (lowest band_top = band_center_norm -
  // band_height_norm/2)
  int topmost = -1;
  double topmost_top = 2.0;  // above any valid position
  for (int i : sel) {
    double top =
        m_signals[i].band_center_norm - m_signals[i].band_height_norm * 0.5;
    if (top < topmost_top) {
      topmost_top = top;
      topmost = i;
    }
  }

  // Copy the topmost signal's band position, height, and Y range to all
  // selected
  for (int i : sel) {
    if (i == topmost) {
      continue;
    }
    m_signals[i].band_center_norm = m_signals[topmost].band_center_norm;
    m_signals[i].band_height_norm = m_signals[topmost].band_height_norm;
    m_signals[i].y_min = m_signals[topmost].y_min;
    m_signals[i].y_max = m_signals[topmost].y_max;
    m_signals[i].bar_x_norm = m_signals[topmost].bar_x_norm;
    m_signals[i].divisions = m_signals[topmost].divisions;
  }
  // After stacking, ensure signals have distinct z_orders so the one painted
  // on top (highest index) also has the highest z_order.
  normalizeZOrders();
  refreshViews();
}

void SignalViewWidget::autoScaleIndices(const std::set<int>& indices) {
  if (!m_overlay_mgr || m_signals.empty() || indices.empty()) {
    return;
  }

  // Group indices by band geometry (center, height, bar_x). Signals with
  // identical band geometry are considered grouped and share a combined Y
  // range.
  struct BandKey {
    double center, height, bar_x;
    bool operator<(const BandKey& o) const {
      if (center != o.center) {
        return center < o.center;
      }
      if (height != o.height) {
        return height < o.height;
      }
      return bar_x < o.bar_x;
    }
  };

  std::map<BandKey, std::vector<int>> groups;
  for (int i : indices) {
    if (i < 0 || i >= static_cast<int>(m_signals.size())) {
      continue;
    }
    const auto& sig = m_signals[i];
    groups[{sig.band_center_norm, sig.band_height_norm, sig.bar_x_norm}]
        .push_back(i);
  }

  bool changed = false;

  for (auto& [key, group] : groups) {
    // Compute combined Y extents over all signals in this group.
    double y_lo = std::numeric_limits<double>::max();
    double y_hi = std::numeric_limits<double>::lowest();
    bool found_any = false;

    for (int i : group) {
      auto resolved = m_overlay_mgr->resolveSignal(m_signals[i].name);
      if (!resolved || resolved->series->size() == 0) {
        continue;
      }

      const auto& series = *resolved->series;
      double t_offset = resolved->time_offset;
      double t_min = m_canvas->viewMinTime() - t_offset;
      double t_max = m_canvas->viewMaxTime() - t_offset;

      // Find first point at or after t_min (in local time)
      auto lb = std::lower_bound(
          series.begin(), series.end(), PJ::PlotData::Point(t_min, 0.0),
          [](const auto& a, const auto& b) { return a.x < b.x; });

      // Include the last point before t_min for step-wise hold value
      if (lb != series.begin()) {
        --lb;
      }

      for (auto pt_it = lb; pt_it != series.end() && pt_it->x <= t_max;
           ++pt_it) {
        y_lo = std::min(y_lo, pt_it->y);
        y_hi = std::max(y_hi, pt_it->y);
        found_any = true;
      }
    }

    if (!found_any) {
      continue;
    }

    double range = y_hi - y_lo;
    double margin = range * (m_autoscale_margin_spin->value() / 100.0);
    if (range < 1e-9) {
      margin = 1.0;  // fallback for constant/all-constant groups
    }

    for (int i : group) {
      m_signals[i].y_min = y_lo - margin;
      m_signals[i].y_max = y_hi + margin;
      changed = true;
    }
  }

  if (changed) {
    refreshViews();
  }
}

void SignalViewWidget::onAutoScale() {
  if (!m_overlay_mgr || m_signals.empty()) {
    return;
  }

  // Auto-scale selected signals, or all signals if none selected
  const auto& sel = m_y_axis_panel->selection();
  if (!sel.empty()) {
    autoScaleIndices(sel);
  } else {
    std::set<int> all;
    for (int i = 0; i < static_cast<int>(m_signals.size()); i++) {
      all.insert(i);
    }
    autoScaleIndices(all);
  }
}

void SignalViewWidget::onVerticalScroll(double delta) {
  m_scroll_offset = std::max(0.0, m_scroll_offset + delta);
  m_canvas->setScrollOffset(m_scroll_offset);
  m_y_axis_panel->setScrollOffset(m_scroll_offset);
  m_scrollbar->blockSignals(true);
  m_scrollbar->setValue(static_cast<int>(m_scroll_offset * 1000));
  m_scrollbar->blockSignals(false);
}

// --- Overlay ---

void SignalViewWidget::migrateSignalNames(bool add_prefix) {
  if (add_prefix) {
    // Add #1/ prefix to all existing unprefixed signal names
    for (auto& sig : m_signals) {
      auto parsed = OverlayManager::parsePrefixedName(sig.name);
      if (parsed.layer == 0) {
        sig.name = OverlayManager::makePrefixedName(1, sig.name);
      }
    }
  } else {
    // Remove prefixes — only when going back to single-layer
    for (auto& sig : m_signals) {
      auto parsed = OverlayManager::parsePrefixedName(sig.name);
      if (parsed.layer > 0) {
        sig.name = parsed.raw_name;
      }
    }
  }
}

void SignalViewWidget::onLoadOverlay() {
  QString file_path = QFileDialog::getOpenFileName(
      this, "Load Overlay Data", QString(),
      "CSV Files (*.csv *.tsv *.txt);;All Files (*)");

  if (file_path.isEmpty()) {
    return;
  }

  // If this is the first overlay and we have existing signals, prefix them with
  // #1/
  bool first_overlay = !m_overlay_mgr->hasOverlays();
  if (first_overlay && !m_signals.empty()) {
    migrateSignalNames(true);
  }

  int new_layer = m_overlay_mgr->loadOverlayFile(file_path.toStdString());
  if (new_layer < 0) {
    // Loading failed — undo prefix migration if it was the first attempt
    if (first_overlay && !m_signals.empty()) {
      migrateSignalNames(false);
    }
    QMessageBox::warning(this, "Overlay Error",
                         "Failed to load overlay file:\n" + file_path);
    return;
  }

  // Auto-match: for each displayed signal's raw name, check if overlay has a
  // match
  std::vector<std::string> raw_names;
  for (const auto& sig : m_signals) {
    raw_names.push_back(OverlayManager::rawName(sig.name));
  }

  auto matches = m_overlay_mgr->findMatchingSignals(new_layer, raw_names);

  // Add matched overlay signals with same position but different appearance
  for (const auto& match_name : matches) {
    auto parsed = OverlayManager::parsePrefixedName(match_name);

    // Find the base signal to copy band position from
    SignalEntry new_entry;
    new_entry.name = match_name;
    new_entry.color = signalColors()[m_signals.size() % signalColors().size()];
    if (m_theme == Theme::Light) {
      new_entry.color = mirrorLightness(new_entry.color);
    }
    new_entry.line_style = Qt::DashLine;  // overlay signals get dashed lines

    for (const auto& sig : m_signals) {
      if (OverlayManager::rawName(sig.name) == parsed.raw_name) {
        new_entry.band_center_norm = sig.band_center_norm;
        new_entry.band_height_norm = sig.band_height_norm;
        new_entry.y_min = sig.y_min;
        new_entry.y_max = sig.y_max;
        new_entry.bar_x_norm = sig.bar_x_norm;
        new_entry.divisions = sig.divisions;
        new_entry.line_width = sig.line_width;
        new_entry.marker_style = sig.marker_style;
        break;
      }
    }

    m_signals.push_back(new_entry);
  }

  m_data_sets_panel->refresh(m_overlay_mgr);
  updateShiftLayerCombo();
  refreshViews();
}

void SignalViewWidget::afterOverlayChange() {
  // If all overlays are gone, strip the "#N/" prefixes from signal names.
  if (!m_overlay_mgr->hasOverlays()) {
    migrateSignalNames(false);
  }
  // Rebuild the data-sets footer to reflect the new layer list.
  m_data_sets_panel->refresh(m_overlay_mgr);
  // Keep the time-shift layer combo in sync.
  updateShiftLayerCombo();
  // Redraw all plots.
  refreshViews();
}

void SignalViewWidget::onRemoveOverlay(int layer_index) {
  // Drop signal entries that belong to this layer.
  auto it = std::remove_if(
      m_signals.begin(), m_signals.end(),
      [layer_index](const SignalEntry& sig) {
        return OverlayManager::parsePrefixedName(sig.name).layer == layer_index;
      });
  m_signals.erase(it, m_signals.end());
  // Remove the layer's data from the manager.
  m_overlay_mgr->removeOverlay(layer_index);
  afterOverlayChange();
}

void SignalViewWidget::onClearOverlays() {
  // Drop all signal entries that belong to any overlay layer (index > 1).
  auto it = std::remove_if(
      m_signals.begin(), m_signals.end(), [](const SignalEntry& sig) {
        return OverlayManager::parsePrefixedName(sig.name).layer > 1;
      });
  m_signals.erase(it, m_signals.end());
  // Remove all overlay layers from the manager.
  m_overlay_mgr->clearOverlays();
  afterOverlayChange();
}

void SignalViewWidget::onStyleLayer(int layer_index) {
  // Open a dialog to set color, line style, line width for all signals in this
  // layer
  QDialog dlg(this);
  dlg.setWindowTitle(QString("Style Layer #%1").arg(layer_index));
  auto* layout = new QVBoxLayout(&dlg);

  // Color
  QColor current_color;
  for (const auto& sig : m_signals) {
    auto parsed = OverlayManager::parsePrefixedName(sig.name);
    if (parsed.layer == layer_index ||
        (parsed.layer == 0 && layer_index == 1)) {
      current_color = sig.color;
      break;
    }
  }

  auto* color_layout = new QHBoxLayout();
  color_layout->addWidget(new QLabel("Color:", &dlg));
  auto* color_btn = new QPushButton(&dlg);
  QColor chosen_color =
      current_color.isValid() ? current_color : QColor(200, 200, 200);
  color_btn->setStyleSheet(
      QString("background-color: %1; border: 1px solid #888; min-width: 60px;")
          .arg(chosen_color.name()));
  connect(color_btn, &QPushButton::clicked, &dlg, [&]() {
    QColor c = QColorDialog::getColor(chosen_color, &dlg, "Layer Color");
    if (c.isValid()) {
      chosen_color = c;
      color_btn->setStyleSheet(
          QString(
              "background-color: %1; border: 1px solid #888; min-width: 60px;")
              .arg(c.name()));
    }
  });
  color_layout->addWidget(color_btn);
  layout->addLayout(color_layout);

  // Line style
  auto* style_layout = new QHBoxLayout();
  style_layout->addWidget(new QLabel("Line Style:", &dlg));
  auto* style_combo = new QComboBox(&dlg);
  for (const auto& opt : lineStyleOptions()) {
    style_combo->addItem(opt.label, static_cast<int>(opt.style));
  }
  style_layout->addWidget(style_combo);
  layout->addLayout(style_layout);

  // Line width
  auto* width_layout = new QHBoxLayout();
  width_layout->addWidget(new QLabel("Line Width:", &dlg));
  auto* width_edit = new QLineEdit(&dlg);
  width_edit->setValidator(new QDoubleValidator(0.1, 10.0, 1, &dlg));
  width_edit->setText("1.5");
  width_layout->addWidget(width_edit);
  layout->addLayout(width_layout);

  auto* buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
  connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
  layout->addWidget(buttons);

  if (dlg.exec() != QDialog::Accepted) {
    return;
  }

  // Apply to all signals from this layer
  Qt::PenStyle new_style =
      static_cast<Qt::PenStyle>(style_combo->currentData().toInt());
  bool ok_w;
  double new_width = width_edit->text().toDouble(&ok_w);
  if (!ok_w || new_width < 0.1) {
    new_width = 1.5;
  }

  for (auto& sig : m_signals) {
    auto parsed = OverlayManager::parsePrefixedName(sig.name);
    int sig_layer = (parsed.layer == 0) ? 1 : parsed.layer;
    if (sig_layer == layer_index) {
      sig.color = chosen_color;
      sig.line_style = new_style;
      sig.line_width = new_width;
    }
  }

  refreshViews();
}

void SignalViewWidget::onLayerRenamed(int layer_index, const QString& name) {
  auto* layer = m_overlay_mgr->layerByIndex(layer_index);
  if (layer) {
    layer->display_name = name.toStdString();
  }
}

void SignalViewWidget::updateSelectedLayers() {
  std::set<int> layers;
  const auto& sel = m_y_axis_panel->selection();
  for (int idx : sel) {
    if (idx >= 0 && idx < static_cast<int>(m_signals.size())) {
      auto parsed = OverlayManager::parsePrefixedName(m_signals[idx].name);
      int layer = (parsed.layer == 0) ? 1 : parsed.layer;
      layers.insert(layer);
    }
  }
  m_canvas->setSelectedLayers(layers);
}

void SignalViewWidget::updateShiftLayerCombo() {
  m_shift_layer_combo->blockSignals(true);
  m_shift_layer_combo->clear();
  for (const auto& layer : m_overlay_mgr->layers()) {
    m_shift_layer_combo->addItem(QString("#%1").arg(layer.index), layer.index);
  }

  // Default to the highest layer index
  if (m_shift_layer_combo->count() > 0) {
    m_shift_layer_combo->setCurrentIndex(m_shift_layer_combo->count() - 1);
    int layer = m_shift_layer_combo->currentData().toInt();
    m_canvas->setDefaultShiftLayer(layer);
  }
  m_shift_layer_combo->blockSignals(false);
}
