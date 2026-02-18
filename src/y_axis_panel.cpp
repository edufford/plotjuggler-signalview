#include "y_axis_panel.h"

#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QVBoxLayout>
#include <QInputDialog>
#include <cmath>

// --- YAxisBar ---

YAxisBar::YAxisBar(int index, const SignalEntry& signal, QWidget* parent)
    : QWidget(parent), _index(index), _signal(signal)
{
  setFixedSize(kBarWidth, kBarHeight);
  setMouseTracking(true);
}

void YAxisBar::setSignalEntry(const SignalEntry& sig)
{
  _signal = sig;
  update();
}

void YAxisBar::setCursorValue(double value, bool valid)
{
  _cursor_value = value;
  _cursor_valid = valid;
  update();
}

void YAxisBar::paintEvent(QPaintEvent* /*event*/)
{
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  // Background
  painter.fillRect(rect(), QColor(40, 40, 40));

  // Color stripe on the left
  painter.fillRect(0, 0, 6, height(), _signal.color);

  // Border
  painter.setPen(QPen(QColor(80, 80, 80), 1));
  painter.drawRect(rect().adjusted(0, 0, -1, -1));

  // Signal name
  painter.setPen(_signal.color);
  QFont name_font("sans-serif", 9, QFont::Bold);
  painter.setFont(name_font);
  QString name = QString::fromStdString(_signal.name);
  // Elide if too long
  QFontMetrics fm(name_font);
  QString elided = fm.elidedText(name, Qt::ElideMiddle, kBarWidth - 16);
  painter.drawText(QRect(10, 4, kBarWidth - 14, 18), Qt::AlignLeft | Qt::AlignVCenter, elided);

  // Y-axis range visualization
  int axis_x = 20;
  int axis_top = 26;
  int axis_bottom = height() - 28;
  int axis_h = axis_bottom - axis_top;

  // Axis line
  painter.setPen(QPen(_signal.color.lighter(120), 1));
  painter.drawLine(axis_x, axis_top, axis_x, axis_bottom);

  // Tick marks
  int n_ticks = 4;
  painter.setFont(QFont("monospace", 7));
  painter.setPen(QColor(170, 170, 170));
  for (int i = 0; i <= n_ticks; i++)
  {
    double frac = (double)i / n_ticks;
    int y = axis_bottom - (int)(frac * axis_h);
    double val = _signal.y_min + frac * (_signal.y_max - _signal.y_min);

    painter.drawLine(axis_x - 3, y, axis_x + 3, y);

    QString label = QString::number(val, 'g', 4);
    QRect text_rect(axis_x + 6, y - 8, kBarWidth - axis_x - 10, 16);
    painter.drawText(text_rect, Qt::AlignLeft | Qt::AlignVCenter, label);
  }

  // Cursor readout
  painter.setPen(QColor(255, 255, 100));
  QFont readout_font("monospace", 10, QFont::Bold);
  painter.setFont(readout_font);

  QString value_str;
  if (_cursor_valid)
  {
    value_str = QString::number(_cursor_value, 'g', 6);
  }
  else
  {
    value_str = "---";
  }

  QRect readout_rect(10, height() - 24, kBarWidth - 14, 20);
  painter.drawText(readout_rect, Qt::AlignLeft | Qt::AlignVCenter, value_str);
}

void YAxisBar::mousePressEvent(QMouseEvent* event)
{
  if (event->button() == Qt::LeftButton)
  {
    _dragging = true;
    _drag_start_y = event->globalPos().y();
    _drag_start_center = _signal.band_center;
    setCursor(Qt::ClosedHandCursor);
  }
  else if (event->button() == Qt::RightButton)
  {
    emit removeRequested(_index);
  }
}

void YAxisBar::mouseMoveEvent(QMouseEvent* event)
{
  if (_dragging)
  {
    int dy = event->globalPos().y() - _drag_start_y;
    // Map pixel delta to band_center delta (assuming parent canvas ~600px)
    double delta = dy / 400.0;
    double new_center = std::clamp(_drag_start_center + delta, 0.0, 1.0);
    emit bandOffsetChanged(_index, new_center);
  }
}

void YAxisBar::mouseReleaseEvent(QMouseEvent* event)
{
  if (event->button() == Qt::LeftButton)
  {
    _dragging = false;
    setCursor(Qt::ArrowCursor);
  }
}

void YAxisBar::mouseDoubleClickEvent(QMouseEvent* /*event*/)
{
  bool ok;
  double new_min = QInputDialog::getDouble(
      this, "Y Min", QString("Min for %1:").arg(QString::fromStdString(_signal.name)),
      _signal.y_min, -1e15, 1e15, 6, &ok);
  if (!ok)
    return;

  double new_max = QInputDialog::getDouble(
      this, "Y Max", QString("Max for %1:").arg(QString::fromStdString(_signal.name)),
      _signal.y_max, -1e15, 1e15, 6, &ok);
  if (!ok)
    return;

  if (new_max > new_min)
  {
    emit yRangeChanged(_index, new_min, new_max);
  }
}

void YAxisBar::wheelEvent(QWheelEvent* event)
{
  // Scroll wheel on a Y-axis bar zooms that signal's Y range
  double factor = (event->angleDelta().y() > 0) ? 0.8 : 1.25;
  double center = (_signal.y_min + _signal.y_max) * 0.5;
  double half_range = (_signal.y_max - _signal.y_min) * 0.5 * factor;
  if (half_range > 1e-12)
  {
    emit yRangeChanged(_index, center - half_range, center + half_range);
  }
}

// --- YAxisPanel ---

YAxisPanel::YAxisPanel(QWidget* parent)
    : QWidget(parent)
{
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);

  _scroll_area = new QScrollArea(this);
  _scroll_area->setWidgetResizable(true);
  _scroll_area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  _scroll_area->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

  _scroll_content = new QWidget();
  _scroll_content->setLayout(new QVBoxLayout());
  _scroll_content->layout()->setContentsMargins(2, 2, 2, 2);
  _scroll_content->layout()->setSpacing(4);
  static_cast<QVBoxLayout*>(_scroll_content->layout())->addStretch();

  _scroll_area->setWidget(_scroll_content);
  layout->addWidget(_scroll_area);

  setFixedWidth(170);

  QPalette pal = palette();
  pal.setColor(QPalette::Window, QColor(35, 35, 35));
  setPalette(pal);
  setAutoFillBackground(true);
}

void YAxisPanel::setSignalEntries(const std::vector<SignalEntry>& entries)
{
  _signals = entries;
  rebuildBars();
}

void YAxisPanel::rebuildBars()
{
  // Clear existing bars
  for (auto* bar : _bars)
  {
    bar->deleteLater();
  }
  _bars.clear();

  auto* content_layout = static_cast<QVBoxLayout*>(_scroll_content->layout());

  // Remove stretch item
  QLayoutItem* stretch = content_layout->takeAt(content_layout->count() - 1);
  delete stretch;

  for (int i = 0; i < (int)_signals.size(); i++)
  {
    auto* bar = new YAxisBar(i, _signals[i], _scroll_content);
    content_layout->addWidget(bar);
    _bars.push_back(bar);

    connect(bar, &YAxisBar::yRangeChanged, this, &YAxisPanel::yRangeChanged);
    connect(bar, &YAxisBar::bandOffsetChanged, this, &YAxisPanel::bandOffsetChanged);
    connect(bar, &YAxisBar::removeRequested, this, &YAxisPanel::removeSignalRequested);
  }

  content_layout->addStretch();
}

void YAxisPanel::updateCursorValues(PJ::PlotDataMapRef* data, double cursor_time)
{
  if (!data)
    return;

  for (int i = 0; i < (int)_bars.size() && i < (int)_signals.size(); i++)
  {
    auto it = data->numeric.find(_signals[i].name);
    if (it == data->numeric.end() || it->second.size() == 0)
    {
      _bars[i]->setCursorValue(0.0, false);
      continue;
    }

    auto val = it->second.getYfromX(cursor_time);
    if (val.has_value())
    {
      _bars[i]->setCursorValue(val.value(), true);
    }
    else
    {
      _bars[i]->setCursorValue(0.0, false);
    }
  }
}
