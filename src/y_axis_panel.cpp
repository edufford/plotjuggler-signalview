#include "y_axis_panel.h"

#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QHBoxLayout>
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

void SignalNameColumn::setDragIndex(int idx)
{
  _external_drag_index = idx;
  update();
}

void SignalNameColumn::setSelection(const std::set<int>& sel)
{
  _selected = sel;
  update();
}

int SignalNameColumn::effectiveDragIndex() const
{
  // Only apply drag demotion after movement starts, not on initial press.
  if (_drag_index >= 0 && _drag_moved)
  {
    // During active multi-drag, use stored index to preserve previous stacking.
    if (_selected.size() > 1 && _selected.count(_drag_index))
      return _external_drag_index;
    return _drag_index;
  }
  return _external_drag_index;
}

void SignalNameColumn::paintEvent(QPaintEvent* /*event*/)
{
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.fillRect(rect(), QColor(32, 32, 38));

  auto offsets = AxisLayout::textRowYOffsets(_signals, height(), kTextRowHeight, effectiveDragIndex());

  for (int i = 0; i < (int)_signals.size(); i++)
  {
    const auto& sig = _signals[i];
    double top = AxisLayout::bandTopY(sig, height());
    double row_y = top + offsets[i];

    // Selection highlight
    if (_selected.count(i))
      painter.fillRect(QRectF(0, row_y, width(), kTextRowHeight), QColor(255, 255, 255, 20));

    // Signal name
    painter.setPen(sig.color);
    QFont name_font("sans-serif", 10, QFont::Bold);
    painter.setFont(name_font);
    QString name = QString::fromStdString(sig.name);
    QFontMetrics fm(name_font);
    QString elided = fm.elidedText(name, Qt::ElideMiddle, width() - 8);
    painter.drawText(QRectF(4, row_y, width() - 8, kTextRowHeight),
                     Qt::AlignLeft | Qt::AlignVCenter, elided);
  }

  // Rubber band overlay
  if (_rubber_band_active)
  {
    QRect rb = QRect(_rubber_band_origin, _rubber_band_current).normalized();
    painter.fillRect(rb, QColor(100, 150, 255, 30));
    painter.setPen(QPen(QColor(100, 150, 255, 120), 1));
    painter.drawRect(rb);
  }
}

int SignalNameColumn::hitTestSignal(const QPoint& pos) const
{
  auto offsets = AxisLayout::textRowYOffsets(_signals, height(), kTextRowHeight, effectiveDragIndex());
  for (int i = 0; i < (int)_signals.size(); i++)
  {
    double top = AxisLayout::bandTopY(_signals[i], height());
    double row_y = top + offsets[i];
    if (pos.y() >= row_y && pos.y() <= row_y + kTextRowHeight)
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
      // Select on press (so drag also selects, matching bar column)
      if (event->modifiers() & Qt::ControlModifier)
        emit clickSelect(_drag_index, true);
      else if (!_selected.count(_drag_index))
        emit clickSelect(_drag_index, false);

      _drag_start_global_y = event->globalPos().y();
      _drag_start_band_center = _signals[_drag_index].band_center;
      _drag_moved = false;
      setCursor(Qt::ClosedHandCursor);
    }
    else
    {
      // Empty space: start rubber band
      _rubber_band_active = true;
      _rubber_band_ctrl = event->modifiers() & Qt::ControlModifier;
      _rubber_band_origin = event->pos();
      _rubber_band_current = event->pos();
      setCursor(Qt::CrossCursor);
    }
  }
}

void SignalNameColumn::mouseMoveEvent(QMouseEvent* event)
{
  if (_rubber_band_active)
  {
    _rubber_band_current = event->pos();
    update();
    return;
  }

  if (_drag_index < 0)
  {
    int hit = hitTestSignal(event->pos());
    setCursor(hit >= 0 ? Qt::OpenHandCursor : Qt::ArrowCursor);
    return;
  }

  if (!_drag_moved)
  {
    _drag_moved = true;
    // Only update cross-column stacking for single-signal drags.
    // Multi-drag preserves existing stacking order.
    if (_selected.size() <= 1 || !_selected.count(_drag_index))
      emit dragIndexChanged(_drag_index);
  }

  int dy = event->globalPos().y() - _drag_start_global_y;
  double plot_h = height() - PlotCanvas::kMarginTop - PlotCanvas::kMarginBottom;
  if (plot_h <= 0)
    return;

  double delta = dy / plot_h;
  double new_center = std::clamp(_drag_start_band_center + delta, 0.0, 1.0);
  emit bandOffsetChanged(_drag_index, new_center);
}

void SignalNameColumn::mouseReleaseEvent(QMouseEvent* event)
{
  if (_rubber_band_active)
  {
    _rubber_band_active = false;
    QRect rb = QRect(_rubber_band_origin, _rubber_band_current).normalized();
    if (rb.height() > 3)
      emit boxSelect(rb.top(), rb.bottom(), _rubber_band_ctrl);
    else
      emit clickSelect(-1, false);  // click on empty = clear
    update();
    setCursor(Qt::ArrowCursor);
    return;
  }

  if (_drag_index >= 0 && _drag_moved)
  {
    if (_selected.size() <= 1 || !_selected.count(_drag_index))
      _external_drag_index = _drag_index;
  }
  _drag_index = -1;
  setCursor(Qt::ArrowCursor);
}

void SignalNameColumn::mouseDoubleClickEvent(QMouseEvent* event)
{
  int idx = hitTestSignal(event->pos());
  if (idx >= 0)
    emit editYRangeRequested(idx);
}

// ============================================================================
// SignalValueColumn
// ============================================================================

SignalValueColumn::SignalValueColumn(QWidget* parent)
    : QWidget(parent)
{
  setMinimumWidth(40);
  setMouseTracking(true);
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

void SignalValueColumn::setDragIndex(int idx)
{
  _external_drag_index = idx;
  update();
}

void SignalValueColumn::setSelection(const std::set<int>& sel)
{
  _selected = sel;
  update();
}

int SignalValueColumn::effectiveDragIndex() const
{
  if (_drag_index >= 0 && _drag_moved)
  {
    if (_selected.size() > 1 && _selected.count(_drag_index))
      return _external_drag_index;
    return _drag_index;
  }
  return _external_drag_index;
}

void SignalValueColumn::paintEvent(QPaintEvent* /*event*/)
{
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.fillRect(rect(), QColor(32, 32, 38));

  auto offsets = AxisLayout::textRowYOffsets(_signals, height(), kTextRowHeight, effectiveDragIndex());

  for (int i = 0; i < (int)_signals.size(); i++)
  {
    const auto& sig = _signals[i];
    double top = AxisLayout::bandTopY(sig, height());
    double row_y = top + offsets[i];

    // Selection highlight
    if (_selected.count(i))
      painter.fillRect(QRectF(0, row_y, width(), kTextRowHeight), QColor(255, 255, 255, 20));

    // Cursor readout value
    if (i < (int)_cursor_valid.size())
    {
      painter.setPen(sig.color);
      QFont readout_font("monospace", 10, QFont::Bold);
      painter.setFont(readout_font);

      QString value_str = _cursor_valid[i]
                              ? QString::number(_cursor_values[i], 'g', 6)
                              : QStringLiteral("---");

      painter.drawText(QRectF(4, row_y, width() - 8, kTextRowHeight),
                       Qt::AlignLeft | Qt::AlignVCenter, value_str);
    }
  }

  // Rubber band overlay
  if (_rubber_band_active)
  {
    QRect rb = QRect(_rubber_band_origin, _rubber_band_current).normalized();
    painter.fillRect(rb, QColor(100, 150, 255, 30));
    painter.setPen(QPen(QColor(100, 150, 255, 120), 1));
    painter.drawRect(rb);
  }
}

int SignalValueColumn::hitTestSignal(const QPoint& pos) const
{
  auto offsets = AxisLayout::textRowYOffsets(_signals, height(), kTextRowHeight, effectiveDragIndex());
  for (int i = 0; i < (int)_signals.size(); i++)
  {
    double top = AxisLayout::bandTopY(_signals[i], height());
    double row_y = top + offsets[i];
    if (pos.y() >= row_y && pos.y() <= row_y + kTextRowHeight)
      return i;
  }
  return -1;
}

void SignalValueColumn::mousePressEvent(QMouseEvent* event)
{
  if (event->button() == Qt::LeftButton)
  {
    _drag_index = hitTestSignal(event->pos());
    if (_drag_index >= 0)
    {
      if (event->modifiers() & Qt::ControlModifier)
        emit clickSelect(_drag_index, true);
      else if (!_selected.count(_drag_index))
        emit clickSelect(_drag_index, false);

      _drag_start_global_y = event->globalPos().y();
      _drag_start_band_center = _signals[_drag_index].band_center;
      _drag_moved = false;
      setCursor(Qt::ClosedHandCursor);
    }
    else
    {
      // Empty space: start rubber band
      _rubber_band_active = true;
      _rubber_band_ctrl = event->modifiers() & Qt::ControlModifier;
      _rubber_band_origin = event->pos();
      _rubber_band_current = event->pos();
      setCursor(Qt::CrossCursor);
    }
  }
}

void SignalValueColumn::mouseMoveEvent(QMouseEvent* event)
{
  if (_rubber_band_active)
  {
    _rubber_band_current = event->pos();
    update();
    return;
  }

  if (_drag_index < 0)
  {
    int hit = hitTestSignal(event->pos());
    setCursor(hit >= 0 ? Qt::OpenHandCursor : Qt::ArrowCursor);
    return;
  }

  if (!_drag_moved)
  {
    _drag_moved = true;
    if (_selected.size() <= 1 || !_selected.count(_drag_index))
      emit dragIndexChanged(_drag_index);
  }

  int dy = event->globalPos().y() - _drag_start_global_y;
  double plot_h = height() - PlotCanvas::kMarginTop - PlotCanvas::kMarginBottom;
  if (plot_h <= 0)
    return;

  double delta = dy / plot_h;
  double new_center = std::clamp(_drag_start_band_center + delta, 0.0, 1.0);
  emit bandOffsetChanged(_drag_index, new_center);
}

void SignalValueColumn::mouseReleaseEvent(QMouseEvent* event)
{
  if (_rubber_band_active)
  {
    _rubber_band_active = false;
    QRect rb = QRect(_rubber_band_origin, _rubber_band_current).normalized();
    if (rb.height() > 3)
      emit boxSelect(rb.top(), rb.bottom(), _rubber_band_ctrl);
    else
      emit clickSelect(-1, false);  // click on empty = clear
    update();
    setCursor(Qt::ArrowCursor);
    return;
  }

  if (_drag_index >= 0 && _drag_moved)
  {
    if (_selected.size() <= 1 || !_selected.count(_drag_index))
      _external_drag_index = _drag_index;
  }
  _drag_index = -1;
  setCursor(Qt::ArrowCursor);
}

void SignalValueColumn::mouseDoubleClickEvent(QMouseEvent* event)
{
  int idx = hitTestSignal(event->pos());
  if (idx >= 0)
    emit editYRangeRequested(idx);
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
  connect(_value_col, &SignalValueColumn::bandOffsetChanged,
          this, &YAxisLabelColumn::bandOffsetChanged);

  // Cross-column drag index: when one column starts dragging, tell the other
  connect(_name_col, &SignalNameColumn::dragIndexChanged,
          _value_col, &SignalValueColumn::setDragIndex);
  connect(_value_col, &SignalValueColumn::dragIndexChanged,
          _name_col, &SignalNameColumn::setDragIndex);

  // Forward editYRangeRequested from sub-columns
  connect(_name_col, &SignalNameColumn::editYRangeRequested,
          this, &YAxisLabelColumn::editYRangeRequested);
  connect(_value_col, &SignalValueColumn::editYRangeRequested,
          this, &YAxisLabelColumn::editYRangeRequested);

  // Forward selection signals from sub-columns
  connect(_name_col, &SignalNameColumn::clickSelect,
          this, &YAxisLabelColumn::clickSelect);
  connect(_value_col, &SignalValueColumn::clickSelect,
          this, &YAxisLabelColumn::clickSelect);
  connect(_name_col, &SignalNameColumn::boxSelect,
          this, &YAxisLabelColumn::boxSelect);
  connect(_value_col, &SignalValueColumn::boxSelect,
          this, &YAxisLabelColumn::boxSelect);
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

void YAxisLabelColumn::setDragIndex(int idx)
{
  _name_col->setDragIndex(idx);
  _value_col->setDragIndex(idx);
}

void YAxisLabelColumn::setSelection(const std::set<int>& sel)
{
  _name_col->setSelection(sel);
  _value_col->setSelection(sel);
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
  // Prune selected indices that are now out of range
  for (auto it = _selected.begin(); it != _selected.end(); )
  {
    if (*it >= (int)entries.size())
      it = _selected.erase(it);
    else
      ++it;
  }
  update();
}

void YAxisBarColumn::setSelection(const std::set<int>& sel)
{
  _selected = sel;
  emit selectionChanged();
  update();
}

void YAxisBarColumn::clearSelection()
{
  _selected.clear();
  emit selectionChanged();
  update();
}

void YAxisBarColumn::setCanvasHeight(int /*h*/)
{
  update();
}

void YAxisBarColumn::setSnapAmount(double snap)
{
  _snap_amount = snap;
}

double YAxisBarColumn::axisX(double bar_x) const
{
  return kAxisPadLeft + bar_x * (width() - kAxisPadLeft - kAxisPadRight);
}

YAxisBarColumn::HitResult YAxisBarColumn::hitTest(const QPoint& pos) const
{
  for (int i = 0; i < (int)_signals.size(); i++)
  {
    double ax = axisX(_signals[i].bar_x);

    // Check horizontal proximity to this bar's axis
    if (pos.x() < ax - 30 || pos.x() > ax + 10)
      continue;

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

    double ax = axisX(sig.bar_x);

    // Color stripe (right of axis)
    painter.fillRect(QRectF(ax + 2, top, 4, band_h), sig.color);

    // Axis line
    painter.setPen(QPen(sig.color.lighter(130), 1.5));
    painter.drawLine(QPointF(ax, top), QPointF(ax, bottom));

    // Edge grab handles
    painter.setPen(QPen(sig.color, 2));
    painter.drawLine(QPointF(ax - 10, top), QPointF(ax + 6, top));
    painter.drawLine(QPointF(ax - 10, bottom), QPointF(ax + 6, bottom));

    // Tick marks and value labels
    int n_ticks = std::max(2, (int)(band_h / 35));
    painter.setFont(QFont("monospace", 7));
    painter.setPen(QColor(170, 170, 170));

    for (int t = 0; t <= n_ticks; t++)
    {
      double frac = (double)t / n_ticks;
      double y = bottom - frac * band_h;
      double val = sig.y_min + frac * (sig.y_max - sig.y_min);

      painter.drawLine(QPointF(ax - 3, y), QPointF(ax + 3, y));

      QString label = QString::number(val, 'g', 4);
      QRectF text_rect(ax - 48, y - 8, 40, 16);
      painter.drawText(text_rect, Qt::AlignRight | Qt::AlignVCenter, label);
    }
  }

  // Rubber band overlay
  if (_rubber_band_active)
  {
    QRect rb = QRect(_rubber_band_origin, _rubber_band_current).normalized();
    painter.fillRect(rb, QColor(100, 150, 255, 30));
    painter.setPen(QPen(QColor(100, 150, 255, 120), 1));
    painter.drawRect(rb);
  }
}

// --- Mouse interaction ---

void YAxisBarColumn::mousePressEvent(QMouseEvent* event)
{
  if (event->button() == Qt::LeftButton)
  {
    _drag_hit = hitTest(event->pos());
    if (_drag_hit.index < 0)
    {
      // Empty space: start rubber band
      _rubber_band_active = true;
      _rubber_band_ctrl = event->modifiers() & Qt::ControlModifier;
      _rubber_band_origin = event->pos();
      _rubber_band_current = event->pos();
      setCursor(Qt::CrossCursor);
      return;
    }

    // Update selection on body clicks
    if (_drag_hit.zone == BODY)
    {
      if (event->modifiers() & Qt::ControlModifier)
      {
        // Ctrl+click: toggle
        if (_selected.count(_drag_hit.index))
          _selected.erase(_drag_hit.index);
        else
          _selected.insert(_drag_hit.index);
        emit selectionChanged();
        update();
      }
      else if (!_selected.count(_drag_hit.index))
      {
        // Not already selected: clear and select this one
        _selected.clear();
        _selected.insert(_drag_hit.index);
        emit selectionChanged();
        update();
      }
      // If already selected without Ctrl: don't change (preserves multi-selection
      // for double-click and drag)
    }

    _drag_start_global_x = event->globalPos().x();
    _drag_start_global_y = event->globalPos().y();
    _drag_start_bar_x = _signals[_drag_hit.index].bar_x;
    _drag_start_band_center = _signals[_drag_hit.index].band_center;
    _drag_start_band_height = _signals[_drag_hit.index].band_height;

    _drag_moved = false;

    if (_drag_hit.zone == BODY)
      setCursor(Qt::SizeAllCursor);
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
  if (_rubber_band_active)
  {
    _rubber_band_current = event->pos();
    update();
    return;
  }

  if (_drag_hit.index < 0)
  {
    auto hit = hitTest(event->pos());
    if (hit.zone == TOP_EDGE || hit.zone == BOTTOM_EDGE)
      setCursor(Qt::SizeVerCursor);
    else if (hit.zone == BODY)
      setCursor(Qt::SizeAllCursor);
    else
      setCursor(Qt::ArrowCursor);
    return;
  }

  int dy = event->globalPos().y() - _drag_start_global_y;
  double plot_h = height() - PlotCanvas::kMarginTop - PlotCanvas::kMarginBottom;

  // Only update stacking priority when vertical movement exceeds one snap step
  if (!_drag_moved && plot_h > 0)
  {
    double snap_px = _snap_amount > 0 ? _snap_amount * plot_h : 1.0;
    if (std::abs(dy) > snap_px)
    {
      _drag_moved = true;
      if (_selected.size() <= 1 || !_selected.count(_drag_hit.index))
        emit dragIndexChanged(_drag_hit.index);
    }
  }
  if (plot_h <= 0)
    return;

  int idx = _drag_hit.index;

  if (_drag_hit.zone == BODY)
  {
    // Vertical movement
    double delta_y = dy / plot_h;
    double new_center = std::clamp(_drag_start_band_center + delta_y, 0.0, 1.0);
    emit bandOffsetChanged(idx, new_center);

    // Horizontal movement
    int dx = event->globalPos().x() - _drag_start_global_x;
    double bar_range = width() - kAxisPadLeft - kAxisPadRight;
    if (bar_range > 0)
    {
      double new_bar_x = std::clamp(_drag_start_bar_x + dx / bar_range, 0.0, 1.0);
      emit barXChanged(idx, new_bar_x);
    }
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
  if (_rubber_band_active)
  {
    _rubber_band_active = false;
    QRect rb = QRect(_rubber_band_origin, _rubber_band_current).normalized();
    if (rb.height() > 3)
    {
      // Select signals whose bands overlap with the rubber band Y range
      std::set<int> new_sel;
      if (_rubber_band_ctrl)
        new_sel = _selected;  // Ctrl: add to existing
      for (int i = 0; i < (int)_signals.size(); i++)
      {
        double top = AxisLayout::bandTopY(_signals[i], height());
        double bottom = AxisLayout::bandBottomY(_signals[i], height());
        if (bottom >= rb.top() && top <= rb.bottom())
          new_sel.insert(i);
      }
      _selected = new_sel;
    }
    else
    {
      // Tiny rubber band = click on empty: clear selection
      if (!_rubber_band_ctrl)
        _selected.clear();
    }
    emit selectionChanged();
    update();
    setCursor(Qt::ArrowCursor);
    return;
  }

  _drag_hit = { -1, NONE };
  setCursor(Qt::ArrowCursor);
}

void YAxisBarColumn::mouseDoubleClickEvent(QMouseEvent* event)
{
  auto hit = hitTest(event->pos());
  if (hit.index >= 0)
    emit editYRangeRequested(hit.index);
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
  connect(_bar_col, &YAxisBarColumn::barXChanged, this, &YAxisPanel::barXChanged);
  connect(_bar_col, &YAxisBarColumn::removeSignalRequested, this, &YAxisPanel::removeSignalRequested);

  // Forward editYRangeRequested from both bar and label columns
  connect(_bar_col, &YAxisBarColumn::editYRangeRequested,
          this, &YAxisPanel::editYRangeRequested);
  connect(_label_col, &YAxisLabelColumn::editYRangeRequested,
          this, &YAxisPanel::editYRangeRequested);

  // Propagate bar column drag index to label columns (name + value)
  connect(_bar_col, &YAxisBarColumn::dragIndexChanged,
          _label_col, &YAxisLabelColumn::setDragIndex);

  // Propagate selection from bar column to label columns
  connect(_bar_col, &YAxisBarColumn::selectionChanged, this, [this]() {
    _label_col->setSelection(_bar_col->selection());
  });

  // Handle click-select from label columns
  connect(_label_col, &YAxisLabelColumn::clickSelect, this, [this](int index, bool toggle) {
    std::set<int> sel = _bar_col->selection();
    if (index < 0)
    {
      if (!toggle)
        sel.clear();
    }
    else if (toggle)
    {
      if (sel.count(index))
        sel.erase(index);
      else
        sel.insert(index);
    }
    else if (!sel.count(index))
    {
      sel.clear();
      sel.insert(index);
    }
    // If already selected without toggle: don't change (preserves for double-click)
    _bar_col->setSelection(sel);
  });

  // Handle box-select from label columns
  connect(_label_col, &YAxisLabelColumn::boxSelect, this,
          [this](double y_top, double y_bottom, bool add) {
    std::set<int> sel;
    if (add)
      sel = _bar_col->selection();
    int h = _bar_col->height();
    for (int i = 0; i < (int)_signals.size(); i++)
    {
      double top = AxisLayout::bandTopY(_signals[i], h);
      double bottom = AxisLayout::bandBottomY(_signals[i], h);
      if (bottom >= y_top && top <= y_bottom)
        sel.insert(i);
    }
    _bar_col->setSelection(sel);
  });
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

void YAxisPanel::setSnapAmount(double snap)
{
  _bar_col->setSnapAmount(snap);
}

const std::set<int>& YAxisPanel::selection() const
{
  return _bar_col->selection();
}

void YAxisPanel::clearSelection()
{
  _bar_col->clearSelection();
}
