#include "signal_view_widget.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QSplitter>
#include <QToolBar>
#include <QInputDialog>
#include <QStringList>
#include <algorithm>

static constexpr const char* kPluginVersion = "0.4.0";

const std::vector<QColor>& SignalViewWidget::signalColors()
{
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

SignalViewWidget::SignalViewWidget(PJ::PlotDataMapRef* data, QWidget* parent)
    : QWidget(parent), _data(data)
{
  setWindowTitle("Signal View");
  resize(900, 500);

  auto* main_layout = new QVBoxLayout(this);
  main_layout->setContentsMargins(2, 2, 2, 2);

  // Toolbar
  auto* toolbar = new QToolBar(this);
  toolbar->setIconSize(QSize(16, 16));

  auto* btn_add = new QPushButton("Add Signal", this);
  auto* btn_remove = new QPushButton("Remove Signal", this);
  auto* btn_reset = new QPushButton("Reset Zoom", this);

  auto* version_label = new QLabel(QString("Signal View v%1").arg(kPluginVersion), this);
  version_label->setStyleSheet("color: #888; font-size: 9px; padding: 0 6px;");

  toolbar->addWidget(version_label);
  toolbar->addSeparator();
  toolbar->addWidget(btn_add);
  toolbar->addWidget(btn_remove);
  toolbar->addSeparator();
  toolbar->addWidget(btn_reset);

  main_layout->addWidget(toolbar);

  // Main content: Y-axis panel | Plot canvas in a splitter
  _y_axis_panel = new YAxisPanel(nullptr);
  _canvas = new PlotCanvas(nullptr);
  _canvas->setDataSource(_data);

  auto* splitter = new QSplitter(Qt::Horizontal, this);
  splitter->setChildrenCollapsible(false);
  splitter->addWidget(_y_axis_panel);
  splitter->addWidget(_canvas);
  splitter->setStretchFactor(0, 0);  // panel: don't stretch
  splitter->setStretchFactor(1, 1);  // canvas: stretch
  splitter->setSizes({ 173, 700 });
  splitter->setHandleWidth(4);

  main_layout->addWidget(splitter, 1);

  // Connections
  connect(btn_add, &QPushButton::clicked, this, &SignalViewWidget::onAddSignal);
  connect(btn_remove, &QPushButton::clicked, this, &SignalViewWidget::onRemoveSignal);
  connect(btn_reset, &QPushButton::clicked, this, &SignalViewWidget::onResetZoom);
  connect(_canvas, &PlotCanvas::cursorMoved, this, &SignalViewWidget::onCursorMoved);
  connect(_y_axis_panel, &YAxisPanel::yRangeChanged, this, &SignalViewWidget::onYRangeChanged);
  connect(_y_axis_panel, &YAxisPanel::bandOffsetChanged, this, &SignalViewWidget::onBandOffsetChanged);
  connect(_y_axis_panel, &YAxisPanel::bandResized, this, &SignalViewWidget::onBandResized);
  connect(_y_axis_panel, &YAxisPanel::removeSignalRequested, this, &SignalViewWidget::onRemoveSignalByIndex);
  connect(_canvas, &PlotCanvas::canvasResized, this, &SignalViewWidget::onCanvasResized);
}

void SignalViewWidget::onAddSignal()
{
  if (!_data)
    return;

  QStringList available;
  for (const auto& pair : _data->numeric)
  {
    // Skip signals already added
    bool already_added = false;
    for (const auto& sig : _signals)
    {
      if (sig.name == pair.first)
      {
        already_added = true;
        break;
      }
    }
    if (!already_added)
    {
      available.append(QString::fromStdString(pair.first));
    }
  }

  if (available.isEmpty())
  {
    return;
  }

  available.sort();

  bool ok;
  QString selected = QInputDialog::getItem(
      this, "Add Signal", "Select a signal:", available, 0, false, &ok);

  if (!ok || selected.isEmpty())
    return;

  SignalEntry entry;
  entry.name = selected.toStdString();
  entry.color = signalColors()[_signals.size() % signalColors().size()];

  // Auto-detect Y range from data
  auto it = _data->numeric.find(entry.name);
  if (it != _data->numeric.end() && it->second.size() > 0)
  {
    auto range = it->second.rangeY();
    if (range)
    {
      double margin = (range->max - range->min) * 0.1;
      if (margin < 1e-9)
        margin = 1.0;
      entry.y_min = range->min - margin;
      entry.y_max = range->max + margin;
    }
  }

  _signals.push_back(entry);
  autoAssignBands();
  refreshViews();
}

void SignalViewWidget::onRemoveSignal()
{
  if (_signals.empty())
    return;

  QStringList names;
  for (const auto& sig : _signals)
  {
    names.append(QString::fromStdString(sig.name));
  }

  bool ok;
  QString selected = QInputDialog::getItem(
      this, "Remove Signal", "Select a signal to remove:", names, 0, false, &ok);

  if (!ok || selected.isEmpty())
    return;

  std::string name = selected.toStdString();
  _signals.erase(
      std::remove_if(_signals.begin(), _signals.end(),
                     [&](const SignalEntry& s) { return s.name == name; }),
      _signals.end());

  autoAssignBands();
  refreshViews();
}

void SignalViewWidget::onResetZoom()
{
  _canvas->resetZoom();
}

void SignalViewWidget::onCursorMoved(double time)
{
  _y_axis_panel->updateCursorValues(_data, time);
}

void SignalViewWidget::onYRangeChanged(int index, double y_min, double y_max)
{
  if (index < 0 || index >= (int)_signals.size())
    return;
  _signals[index].y_min = y_min;
  _signals[index].y_max = y_max;
  refreshViews();
}

void SignalViewWidget::onBandOffsetChanged(int index, double new_center)
{
  if (index < 0 || index >= (int)_signals.size())
    return;
  _signals[index].band_center = new_center;
  _canvas->setSignalEntries(_signals);
  _y_axis_panel->setSignalEntries(_signals);
}

void SignalViewWidget::onBandResized(int index, double new_center, double new_height)
{
  if (index < 0 || index >= (int)_signals.size())
    return;
  _signals[index].band_center = new_center;
  _signals[index].band_height = new_height;
  _canvas->setSignalEntries(_signals);
  _y_axis_panel->setSignalEntries(_signals);
}

void SignalViewWidget::onCanvasResized()
{
  _y_axis_panel->setCanvasHeight(_canvas->height());
}

void SignalViewWidget::onRemoveSignalByIndex(int index)
{
  if (index < 0 || index >= (int)_signals.size())
    return;
  _signals.erase(_signals.begin() + index);
  autoAssignBands();
  refreshViews();
}

void SignalViewWidget::refreshViews()
{
  _canvas->setSignalEntries(_signals);
  _y_axis_panel->setSignalEntries(_signals);
  _y_axis_panel->updateCursorValues(_data, _canvas->cursorTime());
}

void SignalViewWidget::autoAssignBands()
{
  int n = (int)_signals.size();
  if (n == 0)
    return;

  if (n == 1)
  {
    _signals[0].band_center = 0.5;
    _signals[0].band_height = 0.9;
  }
  else
  {
    double band_h = 1.0 / n;
    for (int i = 0; i < n; i++)
    {
      _signals[i].band_center = (i + 0.5) * band_h;
      _signals[i].band_height = band_h * 0.9;
    }
  }
}
