#include "y_axis_panel.h"

#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QHBoxLayout>
#include <QInputDialog>
#include <cmath>
#include <algorithm>

// ============================================================================
// SignalNameColumn
// ============================================================================

SignalNameColumn::SignalNameColumn(QWidget* parent)
    : QWidget(parent)
{
  setMinimumWidth(40);
  setMouseTracking(true);
}

void SignalNameColumn::setSignalEntries(const std::vector<SignalEntry>& entries)
{
  _signals = entries;
  update();
}

void SignalNameColumn::paintEvent(QPaintEvent* /*event*/)
{
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.fillRect(rect(), QColor(32, 32, 38));

  for (int i = 0; i < (int)_signals.size(); i++)
  {
    const auto& sig = _signals[i];
    double top = AxisLayout::bandTopY(sig, height());
    double bottom = AxisLayout::bandBottomY(sig, height());
    double band_h = bottom - top;

    if (band_h < 4)
      continue;

    // Band background
    painter.fillRect(QRectF(0, top, width(), band_h), QColor(38, 36, 34));

    // Color dot
    painter.setBrush(sig.color);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QPointF(10, top + 10), 4, 4);

    // Signal name
    painter.setPen(sig.color);
    QFont name_font("sans-serif", 8, QFont::Bold);
    painter.setFont(name_font);
    QString name = QString::fromStdString(sig.name);
    QFontMetrics fm(name_font);
    QString elided = fm.elidedText(name, Qt::ElideMiddle, width() - 22);
    painter.drawText(QRectF(20, top + 2, width() - 24, 16),
                     Qt::AlignLeft | Qt::AlignVCenter, elided);
  }
}

int SignalNameColumn::hitTestSignal(const QPoint& pos) const
{
  for (int i = 0; i < (int)_signals.size(); i++)
  {
    double top = AxisLayout::bandTopY(_signals[i], height());
    double bottom = AxisLayout::bandBottomY(_signals[i], height());
    if (pos.y() >= top && pos.y() <= bottom)
      return i;
  }
  return -1;
}

void SignalNameColumn::mousePressEvent(QMouseEvent* event)
{
  if (event->button() == Qt::LeftButton)
  {
    _drag_index = hitTestSignal(event->pos());
    if (_drag_index >= 0)
    {
      _drag_start_global_y = event->globalPos().y();
      _drag_start_band_center = _signals[_drag_index].band_center;
      setCursor(Qt::ClosedHandCursor);
    }
  }
}

void SignalNameColumn::mouseMoveEvent(QMouseEvent* event)
{
  if (_drag_index < 0)
  {
    int hit = hitTestSignal(event->pos());
    setCursor(hit >= 0 ? Qt::OpenHandCursor : Qt::ArrowCursor);
    return;
  }

  int dy = event->globalPos().y() - _drag_start_global_y;
  double plot_h = height() - PlotCanvas::kMarginTop - PlotCanvas::kMarginBottom;
  if (plot_h <= 0)
    return;

  double delta = dy / plot_h;
  double new_center = std::clamp(_drag_start_band_center + delta, 0.0, 1.0);
  emit bandOffsetChanged(_drag_index, new_center);
}

void SignalNameColumn::mouseReleaseEvent(QMouseEvent* /*event*/)
{
  _drag_index = -1;
  setCursor(Qt::ArrowCursor);
}

// ============================================================================
// SignalValueColumn
// ============================================================================

SignalValueColumn::SignalValueColumn(QWidget* parent)
    : QWidget(parent)
{
  setMinimumWidth(40);
}

void SignalValueColumn::setSignalEntries(const std::vector<SignalEntry>& entries)
{
  _signals = entries;
  _cursor_values.resize(entries.size(), 0.0);
  _cursor_valid.resize(entries.size(), false);
  update();
}

void SignalValueColumn::setCursorValues(const std::vector<double>& values,
                                        const std::vector<bool>& valid)
{
  _cursor_values = values;
  _cursor_valid = valid;
  update();
}

void SignalValueColumn::paintEvent(QPaintEvent* /*event*/)
{
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.fillRect(rect(), QColor(32, 32, 38));

  for (int i = 0; i < (int)_signals.size(); i++)
  {
    const auto& sig = _signals[i];
    double top = AxisLayout::bandTopY(sig, height());
    double bottom = AxisLayout::bandBottomY(sig, height());
    double band_h = bottom - top;

    if (band_h < 4)
      continue;

    // Band background
    painter.fillRect(QRectF(0, top, width(), band_h), QColor(38, 36, 34));

    // Cursor readout value
    if (i < (int)_cursor_valid.size())
    {
      painter.setPen(sig.color);
      QFont readout_font("monospace", 9, QFont::Bold);
      painter.setFont(readout_font);

      QString value_str = _cursor_valid[i]
                              ? QString::number(_cursor_values[i], 'g', 6)
                              : QStringLiteral("---");

      painter.drawText(QRectF(4, top + 2, width() - 8, 16),
                       Qt::AlignLeft | Qt::AlignVCenter, value_str);
    }
  }
}

// ============================================================================
// YAxisLabelColumn (container: name | value in a splitter)
// ============================================================================

YAxisLabelColumn::YAxisLabelColumn(QWidget* parent)
    : QWidget(parent)
{
  _name_col = new SignalNameColumn(nullptr);
  _value_col = new SignalValueColumn(nullptr);

  _splitter = new QSplitter(Qt::Horizontal, this);
  _splitter->setChildrenCollapsible(false);
  _splitter->addWidget(_name_col);
  _splitter->addWidget(_value_col);
  _splitter->setSizes({ SignalNameColumn::kDefaultWidth, SignalValueColumn::kDefaultWidth });
  _splitter->setHandleWidth(2);
  _splitter->setStyleSheet(
      "QSplitter::handle { background: #444; }"
  );

  auto* layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);
  layout->addWidget(_splitter);

  setMinimumWidth(60);

  connect(_name_col, &SignalNameColumn::bandOffsetChanged,
          this, &YAxisLabelColumn::bandOffsetChanged);
}

void YAxisLabelColumn::setSignalEntries(const std::vector<SignalEntry>& entries)
{
  _name_col->setSignalEntries(entries);
  _value_col->setSignalEntries(entries);
}

void YAxisLabelColumn::setCursorValues(const std::vector<double>& values,
                                       const std::vector<bool>& valid)
{
  _value_col->setCursorValues(values, valid);
}

void YAxisLabelColumn::setCanvasHeight(int /*h*/)
{
  _name_col->update();
  _value_col->update();
}

// ============================================================================
// YAxisBarColumn
// ============================================================================

YAxisBarColumn::YAxisBarColumn(QWidget* parent)
    : QWidget(parent)
{
  setMinimumWidth(40);
  setMouseTracking(true);
}

void YAxisBarColumn::setSignalEntries(const std::vector<SignalEntry>& entries)
{
  _signals = entries;
  update();
}

void YAxisBarColumn::setCanvasHeight(int /*h*/)
{
  update();
}

YAxisBarColumn::HitResult YAxisBarColumn::hitTest(const QPoint& pos) const
{
  for (int i = 0; i < (int)_signals.size(); i++)
  {
    double top = AxisLayout::bandTopY(_signals[i], height());
    double bottom = AxisLayout::bandBottomY(_signals[i], height());

    if (pos.y() < top - kEdgeGrabPixels || pos.y() > bottom + kEdgeGrabPixels)
      continue;

    if (std::abs(pos.y() - top) <= kEdgeGrabPixels)
      return { i, TOP_EDGE };
    if (std::abs(pos.y() - bottom) <= kEdgeGrabPixels)
      return { i, BOTTOM_EDGE };
    if (pos.y() >= top && pos.y() <= bottom)
      return { i, BODY };
  }
  return { -1, NONE };
}

void YAxisBarColumn::paintEvent(QPaintEvent* /*event*/)
{
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.fillRect(rect(), QColor(30, 30, 30));

  for (int i = 0; i < (int)_signals.size(); i++)
  {
    const auto& sig = _signals[i];
    double top = AxisLayout::bandTopY(sig, height());
    double bottom = AxisLayout::bandBottomY(sig, height());
    double band_h = bottom - top;

    if (band_h < 4)
      continue;

    // Band background
    painter.fillRect(QRectF(0, top, width(), band_h), QColor(34, 36, 40));

    // Color stripe on right edge (adjacent to canvas)
    painter.fillRect(QRectF(width() - 4, top, 4, band_h), sig.color);

    // Axis line
    int axis_x = width() - 10;
    painter.setPen(QPen(sig.color.lighter(130), 1.5));
    painter.drawLine(QPointF(axis_x, top), QPointF(axis_x, bottom));

    // Edge grab handles
    painter.setPen(QPen(sig.color, 2));
    painter.drawLine(QPointF(axis_x - 10, top), QPointF(axis_x + 4, top));
    painter.drawLine(QPointF(axis_x - 10, bottom), QPointF(axis_x + 4, bottom));

    // Tick marks and value labels
    int n_ticks = std::max(2, (int)(band_h / 35));
    painter.setFont(QFont("monospace", 7));
    painter.setPen(QColor(170, 170, 170));

    for (int t = 0; t <= n_ticks; t++)
    {
      double frac = (double)t / n_ticks;
      double y = bottom - frac * band_h;
      double val = sig.y_min + frac * (sig.y_max - sig.y_min);

      painter.drawLine(QPointF(axis_x - 3, y), QPointF(axis_x + 3, y));

      QString label = QString::number(val, 'g', 4);
      QRectF text_rect(1, y - 8, axis_x - 8, 16);
      painter.drawText(text_rect, Qt::AlignRight | Qt::AlignVCenter, label);
    }
  }
}

// --- Mouse interaction ---

void YAxisBarColumn::mousePressEvent(QMouseEvent* event)
{
  if (event->button() == Qt::LeftButton)
  {
    _drag_hit = hitTest(event->pos());
    if (_drag_hit.index < 0)
      return;

    _drag_start_global_y = event->globalPos().y();
    _drag_start_band_center = _signals[_drag_hit.index].band_center;
    _drag_start_band_height = _signals[_drag_hit.index].band_height;

    if (_drag_hit.zone == BODY)
      setCursor(Qt::ClosedHandCursor);
    else
      setCursor(Qt::SizeVerCursor);
  }
  else if (event->button() == Qt::RightButton)
  {
    auto hit = hitTest(event->pos());
    if (hit.index >= 0)
      emit removeSignalRequested(hit.index);
  }
}

void YAxisBarColumn::mouseMoveEvent(QMouseEvent* event)
{
  if (_drag_hit.index < 0)
  {
    auto hit = hitTest(event->pos());
    if (hit.zone == TOP_EDGE || hit.zone == BOTTOM_EDGE)
      setCursor(Qt::SizeVerCursor);
    else if (hit.zone == BODY)
      setCursor(Qt::OpenHandCursor);
    else
      setCursor(Qt::ArrowCursor);
    return;
  }

  int dy = event->globalPos().y() - _drag_start_global_y;
  double plot_h = height() - PlotCanvas::kMarginTop - PlotCanvas::kMarginBottom;
  if (plot_h <= 0)
    return;

  int idx = _drag_hit.index;

  if (_drag_hit.zone == BODY)
  {
    double delta = dy / plot_h;
    double new_center = std::clamp(_drag_start_band_center + delta, 0.0, 1.0);
    emit bandOffsetChanged(idx, new_center);
  }
  else if (_drag_hit.zone == TOP_EDGE)
  {
    double delta_norm = dy / plot_h;
    double old_top = _drag_start_band_center - _drag_start_band_height * 0.5;
    double old_bottom = _drag_start_band_center + _drag_start_band_height * 0.5;
    double new_top = std::clamp(old_top + delta_norm, 0.0, old_bottom - 0.02);
    double new_height = old_bottom - new_top;
    double new_center = new_top + new_height * 0.5;
    emit bandResized(idx, new_center, new_height);
  }
  else if (_drag_hit.zone == BOTTOM_EDGE)
  {
    double delta_norm = dy / plot_h;
    double old_top = _drag_start_band_center - _drag_start_band_height * 0.5;
    double old_bottom = _drag_start_band_center + _drag_start_band_height * 0.5;
    double new_bottom = std::clamp(old_bottom + delta_norm, old_top + 0.02, 1.0);
    double new_height = new_bottom - old_top;
    double new_center = old_top + new_height * 0.5;
    emit bandResized(idx, new_center, new_height);
  }
}

void YAxisBarColumn::mouseReleaseEvent(QMouseEvent* /*event*/)
{
  _drag_hit = { -1, NONE };
  setCursor(Qt::ArrowCursor);
}

void YAxisBarColumn::mouseDoubleClickEvent(QMouseEvent* event)
{
  auto hit = hitTest(event->pos());
  if (hit.index < 0)
    return;

  const auto& sig = _signals[hit.index];
  bool ok;
  double new_min = QInputDialog::getDouble(
      this, "Y Min",
      QString("Min for %1:").arg(QString::fromStdString(sig.name)),
      sig.y_min, -1e15, 1e15, 6, &ok);
  if (!ok)
    return;

  double new_max = QInputDialog::getDouble(
      this, "Y Max",
      QString("Max for %1:").arg(QString::fromStdString(sig.name)),
      sig.y_max, -1e15, 1e15, 6, &ok);
  if (!ok)
    return;

  if (new_max > new_min)
    emit yRangeChanged(hit.index, new_min, new_max);
}

void YAxisBarColumn::wheelEvent(QWheelEvent* event)
{
  auto hit = hitTest(event->position().toPoint());
  if (hit.index < 0)
    return;

  const auto& sig = _signals[hit.index];
  double factor = (event->angleDelta().y() > 0) ? 0.8 : 1.25;
  double center = (sig.y_min + sig.y_max) * 0.5;
  double half_range = (sig.y_max - sig.y_min) * 0.5 * factor;
  if (half_range > 1e-12)
    emit yRangeChanged(hit.index, center - half_range, center + half_range);
}

// ============================================================================
// YAxisPanel (container)
// ============================================================================

YAxisPanel::YAxisPanel(QWidget* parent)
    : QWidget(parent)
{
  _label_col = new YAxisLabelColumn(nullptr);
  _bar_col = new YAxisBarColumn(nullptr);

  _splitter = new QSplitter(Qt::Horizontal, this);
  _splitter->setChildrenCollapsible(false);
  _splitter->addWidget(_label_col);
  _splitter->addWidget(_bar_col);
  _splitter->setSizes({ YAxisLabelColumn::kDefaultWidth, YAxisBarColumn::kDefaultWidth });
  _splitter->setHandleWidth(3);
  _splitter->setStyleSheet(
      "QSplitter::handle { background: #555; }"
  );

  auto* layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);
  layout->addWidget(_splitter);

  setMinimumWidth(120);
  setMaximumWidth(400);
  resize(YAxisLabelColumn::kDefaultWidth + YAxisBarColumn::kDefaultWidth + 3, height());

  // Forward signals from both columns
  connect(_label_col, &YAxisLabelColumn::bandOffsetChanged, this, &YAxisPanel::bandOffsetChanged);
  connect(_bar_col, &YAxisBarColumn::yRangeChanged, this, &YAxisPanel::yRangeChanged);
  connect(_bar_col, &YAxisBarColumn::bandOffsetChanged, this, &YAxisPanel::bandOffsetChanged);
  connect(_bar_col, &YAxisBarColumn::bandResized, this, &YAxisPanel::bandResized);
  connect(_bar_col, &YAxisBarColumn::removeSignalRequested, this, &YAxisPanel::removeSignalRequested);
}

void YAxisPanel::setSignalEntries(const std::vector<SignalEntry>& entries)
{
  _signals = entries;
  _label_col->setSignalEntries(entries);
  _bar_col->setSignalEntries(entries);
}

void YAxisPanel::updateCursorValues(PJ::PlotDataMapRef* data, double cursor_time)
{
  if (!data)
    return;

  std::vector<double> values(_signals.size(), 0.0);
  std::vector<bool> valid(_signals.size(), false);

  for (size_t i = 0; i < _signals.size(); i++)
  {
    auto it = data->numeric.find(_signals[i].name);
    if (it == data->numeric.end() || it->second.size() == 0)
      continue;

    auto val = it->second.getYfromX(cursor_time);
    if (val.has_value())
    {
      values[i] = val.value();
      valid[i] = true;
    }
  }

  _label_col->setCursorValues(values, valid);
}

void YAxisPanel::setCanvasHeight(int h)
{
  _label_col->setCanvasHeight(h);
  _bar_col->setCanvasHeight(h);
}
