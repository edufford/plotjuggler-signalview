#include "y_axis_panel.h"

#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

#include "overlay_manager.h"

// ============================================================================
// SignalColumnBase (shared drag, rubber-band, and scroll handling)
// ============================================================================

SignalColumnBase::SignalColumnBase(QWidget* parent) : QWidget(parent) {
  setMinimumWidth(40);
  setMouseTracking(true);
}

void SignalColumnBase::setSignalEntries(
    const std::vector<SignalEntry>& entries) {
  m_signals = entries;
  update();
}

void SignalColumnBase::setDragIndex(int idx) {
  m_external_drag_index = idx;
  update();
}

void SignalColumnBase::setSelection(const std::set<int>& sel) {
  m_selected = sel;
  update();
}

void SignalColumnBase::setScrollOffset(double offset) {
  m_scroll_offset = offset;
  update();
}

int SignalColumnBase::effectiveDragIndex() const {
  // Only apply drag demotion after movement starts, not on initial press.
  if (m_drag_index >= 0 && m_drag_moved) {
    // During active multi-drag, use stored index to preserve previous stacking.
    if (m_selected.size() > 1 && m_selected.count(m_drag_index))
      return m_external_drag_index;
    return m_drag_index;
  }
  return m_external_drag_index;
}

int SignalColumnBase::hitTestSignal(const QPoint& pos) const {
  auto offsets = AxisLayout::textRowYOffsets(m_signals, height(),
                                             TEXT_ROW_HEIGHT, m_scroll_offset);
  for (int i = 0; i < (int)m_signals.size(); i++) {
    double top = AxisLayout::bandTopY(m_signals[i], height(), m_scroll_offset);
    double row_y = top + offsets[i];
    if (pos.y() >= row_y && pos.y() <= row_y + TEXT_ROW_HEIGHT) return i;
  }
  return -1;
}

void SignalColumnBase::paintEvent(QPaintEvent* /*event*/) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.fillRect(rect(), QColor(32, 32, 38));

  auto offsets = AxisLayout::textRowYOffsets(m_signals, height(),
                                             TEXT_ROW_HEIGHT, m_scroll_offset);

  for (int i = 0; i < (int)m_signals.size(); i++) {
    double top = AxisLayout::bandTopY(m_signals[i], height(), m_scroll_offset);
    double row_y = top + offsets[i];
    if (m_selected.count(i))
      painter.fillRect(QRectF(0, row_y, width(), TEXT_ROW_HEIGHT),
                       QColor(255, 255, 255, 20));
  }

  paintContent(painter, offsets);

  if (m_rubber_band_active) {
    QRect rb = QRect(m_rubber_band_origin, m_rubber_band_current).normalized();
    painter.fillRect(rb, QColor(100, 150, 255, 30));
    painter.setPen(QPen(QColor(100, 150, 255, 120), 1));
    painter.drawRect(rb);
  }
}

void SignalColumnBase::mousePressEvent(QMouseEvent* event) {
  if (event->button() == Qt::LeftButton) {
    m_drag_index = hitTestSignal(event->pos());
    if (m_drag_index >= 0) {
      // Select on press (so drag also selects, matching bar column)
      if (event->modifiers() & Qt::ControlModifier)
        emit clickSelect(m_drag_index, true);
      else if (!m_selected.count(m_drag_index))
        emit clickSelect(m_drag_index, false);

      m_drag_start_global_y = event->globalPos().y();
      m_drag_start_band_center = m_signals[m_drag_index].band_center_norm;
      m_drag_moved = false;
      setCursor(Qt::ClosedHandCursor);
    } else {
      // Empty space: start rubber band
      m_rubber_band_active = true;
      m_rubber_band_ctrl = event->modifiers() & Qt::ControlModifier;
      m_rubber_band_origin = event->pos();
      m_rubber_band_current = event->pos();
      setCursor(Qt::CrossCursor);
    }
  }
}

void SignalColumnBase::mouseMoveEvent(QMouseEvent* event) {
  if (m_rubber_band_active) {
    m_rubber_band_current = event->pos();
    update();
    return;
  }

  if (m_drag_index < 0) {
    int hit = hitTestSignal(event->pos());
    setCursor(hit >= 0 ? Qt::OpenHandCursor : Qt::ArrowCursor);
    return;
  }

  if (!m_drag_moved) {
    m_drag_moved = true;
    // Only update cross-column stacking for single-signal drags.
    // Multi-drag preserves existing stacking order.
    if (m_selected.size() <= 1 || !m_selected.count(m_drag_index))
      emit dragIndexChanged(m_drag_index);
  }

  int dy = event->globalPos().y() - m_drag_start_global_y;
  double plot_h = height() - PlotCanvas::MARGIN_TOP - PlotCanvas::MARGIN_BOTTOM;
  if (plot_h <= 0) return;

  double delta = dy / plot_h;
  double new_center = m_drag_start_band_center + delta;
  emit bandOffsetChanged(m_drag_index, new_center);
}

void SignalColumnBase::mouseReleaseEvent(QMouseEvent* /*event*/) {
  if (m_rubber_band_active) {
    m_rubber_band_active = false;
    QRect rb = QRect(m_rubber_band_origin, m_rubber_band_current).normalized();
    if (rb.height() > 3)
      emit boxSelect(rb.top(), rb.bottom(), m_rubber_band_ctrl);
    else
      emit clickSelect(-1, false);  // click on empty = clear
    update();
    setCursor(Qt::ArrowCursor);
    return;
  }

  if (m_drag_index >= 0 && m_drag_moved) {
    if (m_selected.size() <= 1 || !m_selected.count(m_drag_index))
      m_external_drag_index = m_drag_index;
  }
  m_drag_index = -1;
  setCursor(Qt::ArrowCursor);
}

void SignalColumnBase::mouseDoubleClickEvent(QMouseEvent* event) {
  int idx = hitTestSignal(event->pos());
  if (idx >= 0) {
    emit editYRangeRequested(idx);
  } else {
    double plot_h =
        height() - PlotCanvas::MARGIN_TOP - PlotCanvas::MARGIN_BOTTOM;
    double band_center_norm =
        (plot_h > 0) ? (event->pos().y() - PlotCanvas::MARGIN_TOP) / plot_h +
                           m_scroll_offset
                     : 0.5;
    emit addSignalRequested(band_center_norm);
  }
}

void SignalColumnBase::wheelEvent(QWheelEvent* event) {
  double delta = (event->angleDelta().y() > 0) ? -0.05 : 0.05;
  emit verticalScrollRequested(delta);
}

// ============================================================================
// SignalNameColumn
// ============================================================================

SignalNameColumn::SignalNameColumn(QWidget* parent)
    : SignalColumnBase(parent) {}

void SignalNameColumn::paintContent(QPainter& painter,
                                    const std::vector<double>& offsets) {
  for (int i = 0; i < (int)m_signals.size(); i++) {
    const auto& sig = m_signals[i];
    double top = AxisLayout::bandTopY(sig, height(), m_scroll_offset);
    double row_y = top + offsets[i];

    painter.setPen(sig.color);
    QFont name_font("sans-serif", 10, QFont::Bold);
    painter.setFont(name_font);
    QString name = QString::fromStdString(sig.name);
    QFontMetrics fm(name_font);
    QString elided = fm.elidedText(name, Qt::ElideMiddle, width() - 8);
    painter.drawText(QRectF(4, row_y, width() - 8, TEXT_ROW_HEIGHT),
                     Qt::AlignLeft | Qt::AlignVCenter, elided);
  }
}

// ============================================================================
// SignalValueColumn
// ============================================================================

SignalValueColumn::SignalValueColumn(QWidget* parent)
    : SignalColumnBase(parent) {}

void SignalValueColumn::setSignalEntries(
    const std::vector<SignalEntry>& entries) {
  SignalColumnBase::setSignalEntries(entries);
  m_cursor_values.resize(entries.size(), 0.0);
  m_cursor_valid.resize(entries.size(), false);
}

void SignalValueColumn::setCursorValues(const std::vector<double>& values,
                                        const std::vector<bool>& valid) {
  m_cursor_values = values;
  m_cursor_valid = valid;
  update();
}

void SignalValueColumn::paintContent(QPainter& painter,
                                     const std::vector<double>& offsets) {
  for (int i = 0; i < (int)m_signals.size(); i++) {
    const auto& sig = m_signals[i];
    double top = AxisLayout::bandTopY(sig, height(), m_scroll_offset);
    double row_y = top + offsets[i];

    if (i < (int)m_cursor_valid.size()) {
      painter.setPen(sig.color);
      QFont readout_font("monospace", 10, QFont::Bold);
      painter.setFont(readout_font);

      QString value_str = m_cursor_valid[i]
                              ? QString::number(m_cursor_values[i], 'g', 6)
                              : QStringLiteral("---");

      painter.drawText(QRectF(4, row_y, width() - 8, TEXT_ROW_HEIGHT),
                       Qt::AlignLeft | Qt::AlignVCenter, value_str);
    }
  }
}

// ============================================================================
// YAxisLabelColumn (container: name | value in a splitter)
// ============================================================================

YAxisLabelColumn::YAxisLabelColumn(QWidget* parent) : QWidget(parent) {
  m_name_col = new SignalNameColumn(nullptr);
  m_value_col = new SignalValueColumn(nullptr);

  m_splitter = new QSplitter(Qt::Horizontal, this);
  m_splitter->setChildrenCollapsible(false);
  m_splitter->addWidget(m_name_col);
  m_splitter->addWidget(m_value_col);
  m_splitter->setSizes(
      {SignalNameColumn::DEFAULT_WIDTH, SignalValueColumn::DEFAULT_WIDTH});
  m_splitter->setHandleWidth(2);
  m_splitter->setStyleSheet("QSplitter::handle { background: #444; }");

  auto* layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);
  layout->addWidget(m_splitter);

  setMinimumWidth(60);

  connect(m_name_col, &SignalNameColumn::bandOffsetChanged, this,
          &YAxisLabelColumn::bandOffsetChanged);
  connect(m_value_col, &SignalValueColumn::bandOffsetChanged, this,
          &YAxisLabelColumn::bandOffsetChanged);

  // Cross-column drag index: when one column starts dragging, tell the other
  connect(m_name_col, &SignalNameColumn::dragIndexChanged, m_value_col,
          &SignalValueColumn::setDragIndex);
  connect(m_value_col, &SignalValueColumn::dragIndexChanged, m_name_col,
          &SignalNameColumn::setDragIndex);

  // Forward editYRangeRequested from sub-columns
  connect(m_name_col, &SignalNameColumn::editYRangeRequested, this,
          &YAxisLabelColumn::editYRangeRequested);
  connect(m_value_col, &SignalValueColumn::editYRangeRequested, this,
          &YAxisLabelColumn::editYRangeRequested);

  // Forward addSignalRequested from sub-columns
  connect(m_name_col, &SignalNameColumn::addSignalRequested, this,
          &YAxisLabelColumn::addSignalRequested);
  connect(m_value_col, &SignalValueColumn::addSignalRequested, this,
          &YAxisLabelColumn::addSignalRequested);

  // Forward selection signals from sub-columns
  connect(m_name_col, &SignalNameColumn::clickSelect, this,
          &YAxisLabelColumn::clickSelect);
  connect(m_value_col, &SignalValueColumn::clickSelect, this,
          &YAxisLabelColumn::clickSelect);
  connect(m_name_col, &SignalNameColumn::boxSelect, this,
          &YAxisLabelColumn::boxSelect);
  connect(m_value_col, &SignalValueColumn::boxSelect, this,
          &YAxisLabelColumn::boxSelect);

  // Forward vertical scroll requests
  connect(m_name_col, &SignalNameColumn::verticalScrollRequested, this,
          &YAxisLabelColumn::verticalScrollRequested);
  connect(m_value_col, &SignalValueColumn::verticalScrollRequested, this,
          &YAxisLabelColumn::verticalScrollRequested);
}

void YAxisLabelColumn::setSignalEntries(
    const std::vector<SignalEntry>& entries) {
  m_name_col->setSignalEntries(entries);
  m_value_col->setSignalEntries(entries);
}

void YAxisLabelColumn::setCursorValues(const std::vector<double>& values,
                                       const std::vector<bool>& valid) {
  m_value_col->setCursorValues(values, valid);
}

void YAxisLabelColumn::setDragIndex(int idx) {
  m_name_col->setDragIndex(idx);
  m_value_col->setDragIndex(idx);
}

void YAxisLabelColumn::setSelection(const std::set<int>& sel) {
  m_name_col->setSelection(sel);
  m_value_col->setSelection(sel);
}

void YAxisLabelColumn::setScrollOffset(double offset) {
  m_name_col->setScrollOffset(offset);
  m_value_col->setScrollOffset(offset);
}

void YAxisLabelColumn::setCanvasHeight(int /*h*/) {
  m_name_col->update();
  m_value_col->update();
}

// ============================================================================
// YAxisBarColumn
// ============================================================================

YAxisBarColumn::YAxisBarColumn(QWidget* parent) : QWidget(parent) {
  setMinimumWidth(40);
  setMouseTracking(true);
}

void YAxisBarColumn::setSignalEntries(const std::vector<SignalEntry>& entries) {
  m_signals = entries;
  // Prune selected indices that are now out of range
  for (auto it = m_selected.begin(); it != m_selected.end();) {
    if (*it >= (int)entries.size())
      it = m_selected.erase(it);
    else
      ++it;
  }
  update();
}

void YAxisBarColumn::setSelection(const std::set<int>& sel) {
  m_selected = sel;
  emit selectionChanged();
  update();
}

void YAxisBarColumn::clearSelection() {
  m_selected.clear();
  emit selectionChanged();
  update();
}

void YAxisBarColumn::selectAll() {
  m_selected.clear();
  for (int i = 0; i < (int)m_signals.size(); i++) m_selected.insert(i);
  emit selectionChanged();
  update();
}

void YAxisBarColumn::setCanvasHeight(int /*h*/) { update(); }

void YAxisBarColumn::setScrollOffset(double offset) {
  m_scroll_offset = offset;
  update();
}

void YAxisBarColumn::setSnapAmount(double snap) { m_snap_amount = snap; }

double YAxisBarColumn::axisX(double bar_x_norm) const {
  return AXIS_PAD_LEFT +
         bar_x_norm * (width() - AXIS_PAD_LEFT - AXIS_PAD_RIGHT);
}

YAxisBarColumn::HitResult YAxisBarColumn::hitTest(const QPoint& pos) const {
  for (int i = 0; i < (int)m_signals.size(); i++) {
    double ax = axisX(m_signals[i].bar_x_norm);

    // Check horizontal proximity to this bar's axis
    if (pos.x() < ax - 30 || pos.x() > ax + 10) continue;

    double top = AxisLayout::bandTopY(m_signals[i], height(), m_scroll_offset);
    double bottom =
        AxisLayout::bandBottomY(m_signals[i], height(), m_scroll_offset);

    if (pos.y() < top - EDGE_GRAB_PIXELS || pos.y() > bottom + EDGE_GRAB_PIXELS)
      continue;

    if (std::abs(pos.y() - top) <= EDGE_GRAB_PIXELS) return {i, TOP_EDGE};
    if (std::abs(pos.y() - bottom) <= EDGE_GRAB_PIXELS) return {i, BOTTOM_EDGE};
    if (pos.y() >= top && pos.y() <= bottom) return {i, BODY};
  }
  return {-1, NONE};
}

void YAxisBarColumn::paintEvent(QPaintEvent* /*event*/) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.fillRect(rect(), QColor(30, 30, 30));

  for (int i = 0; i < (int)m_signals.size(); i++) {
    const auto& sig = m_signals[i];
    double top = AxisLayout::bandTopY(sig, height(), m_scroll_offset);
    double bottom = AxisLayout::bandBottomY(sig, height(), m_scroll_offset);
    double band_pixel_h = bottom - top;

    if (band_pixel_h < 4) continue;

    double ax = axisX(sig.bar_x_norm);

    // Color stripe (right of axis)
    painter.fillRect(QRectF(ax + 2, top, 4, band_pixel_h), sig.color);

    // Axis line
    painter.setPen(QPen(sig.color.lighter(130), 1.5));
    painter.drawLine(QPointF(ax, top), QPointF(ax, bottom));

    // Edge grab handles
    painter.setPen(QPen(sig.color, 2));
    painter.drawLine(QPointF(ax - 10, top), QPointF(ax + 6, top));
    painter.drawLine(QPointF(ax - 10, bottom), QPointF(ax + 6, bottom));

    // Tick marks and value labels
    int n_ticks = sig.tickCount(band_pixel_h);
    painter.setFont(QFont("monospace", 7));
    painter.setPen(QColor(170, 170, 170));

    for (int t = 0; t <= n_ticks; t++) {
      double frac = (double)t / n_ticks;
      double y = bottom - frac * band_pixel_h;
      double val = sig.y_min + frac * (sig.y_max - sig.y_min);

      painter.drawLine(QPointF(ax - 3, y), QPointF(ax + 3, y));

      QString label = QString::number(val, 'g', 4);
      QRectF text_rect(ax - 48, y - 8, 40, 16);
      painter.drawText(text_rect, Qt::AlignRight | Qt::AlignVCenter, label);
    }
  }

  // Rubber band overlay
  if (m_rubber_band_active) {
    QRect rb = QRect(m_rubber_band_origin, m_rubber_band_current).normalized();
    painter.fillRect(rb, QColor(100, 150, 255, 30));
    painter.setPen(QPen(QColor(100, 150, 255, 120), 1));
    painter.drawRect(rb);
  }
}

// --- Mouse interaction ---

void YAxisBarColumn::mousePressEvent(QMouseEvent* event) {
  if (event->button() == Qt::LeftButton) {
    m_drag_hit = hitTest(event->pos());
    if (m_drag_hit.index < 0) {
      // Empty space: start rubber band
      m_rubber_band_active = true;
      m_rubber_band_ctrl = event->modifiers() & Qt::ControlModifier;
      m_rubber_band_origin = event->pos();
      m_rubber_band_current = event->pos();
      setCursor(Qt::CrossCursor);
      return;
    }

    // Update selection on body clicks
    if (m_drag_hit.zone == BODY) {
      if (event->modifiers() & Qt::ControlModifier) {
        // Ctrl+click: toggle
        if (m_selected.count(m_drag_hit.index))
          m_selected.erase(m_drag_hit.index);
        else
          m_selected.insert(m_drag_hit.index);
        emit selectionChanged();
        update();
      } else if (!m_selected.count(m_drag_hit.index)) {
        // Not already selected: clear and select this one
        m_selected.clear();
        m_selected.insert(m_drag_hit.index);
        emit selectionChanged();
        update();
      }
      // If already selected without Ctrl: don't change (preserves
      // multi-selection for double-click and drag)
    }

    m_drag_start_global_x = event->globalPos().x();
    m_drag_start_global_y = event->globalPos().y();
    m_drag_start_bar_x = m_signals[m_drag_hit.index].bar_x_norm;
    m_drag_start_band_center = m_signals[m_drag_hit.index].band_center_norm;
    m_drag_start_band_height = m_signals[m_drag_hit.index].band_height_norm;

    m_drag_moved = false;

    if (m_drag_hit.zone == BODY)
      setCursor(Qt::SizeAllCursor);
    else
      setCursor(Qt::SizeVerCursor);
  } else if (event->button() == Qt::RightButton) {
    auto hit = hitTest(event->pos());
    if (hit.index >= 0) emit removeSignalRequested(hit.index);
  }
}

void YAxisBarColumn::mouseMoveEvent(QMouseEvent* event) {
  if (m_rubber_band_active) {
    m_rubber_band_current = event->pos();
    update();
    return;
  }

  if (m_drag_hit.index < 0) {
    auto hit = hitTest(event->pos());
    if (hit.zone == TOP_EDGE || hit.zone == BOTTOM_EDGE)
      setCursor(Qt::SizeVerCursor);
    else if (hit.zone == BODY)
      setCursor(Qt::SizeAllCursor);
    else
      setCursor(Qt::ArrowCursor);
    return;
  }

  int dy = event->globalPos().y() - m_drag_start_global_y;
  double plot_h = height() - PlotCanvas::MARGIN_TOP - PlotCanvas::MARGIN_BOTTOM;

  // Only update stacking priority when vertical movement exceeds one snap step
  if (!m_drag_moved && plot_h > 0) {
    double snap_px = m_snap_amount > 0 ? m_snap_amount * plot_h : 1.0;
    if (std::abs(dy) > snap_px) {
      m_drag_moved = true;
      if (m_selected.size() <= 1 || !m_selected.count(m_drag_hit.index))
        emit dragIndexChanged(m_drag_hit.index);
    }
  }
  if (plot_h <= 0) return;

  int idx = m_drag_hit.index;

  if (m_drag_hit.zone == BODY) {
    // Vertical movement
    double delta_y = dy / plot_h;
    double new_center = m_drag_start_band_center + delta_y;
    emit bandOffsetChanged(idx, new_center);

    // Horizontal movement
    int dx = event->globalPos().x() - m_drag_start_global_x;
    double bar_range = width() - AXIS_PAD_LEFT - AXIS_PAD_RIGHT;
    if (bar_range > 0) {
      double new_bar_x =
          std::clamp(m_drag_start_bar_x + dx / bar_range, 0.0, 1.0);
      emit barXChanged(idx, new_bar_x);
    }
  } else if (m_drag_hit.zone == TOP_EDGE) {
    double delta_norm = dy / plot_h;
    double old_top = m_drag_start_band_center - m_drag_start_band_height * 0.5;
    double old_bottom =
        m_drag_start_band_center + m_drag_start_band_height * 0.5;
    double new_top = std::min(old_top + delta_norm, old_bottom - 0.02);
    double new_height = old_bottom - new_top;
    double new_center = new_top + new_height * 0.5;
    emit bandResized(idx, new_center, new_height);
  } else if (m_drag_hit.zone == BOTTOM_EDGE) {
    double delta_norm = dy / plot_h;
    double old_top = m_drag_start_band_center - m_drag_start_band_height * 0.5;
    double old_bottom =
        m_drag_start_band_center + m_drag_start_band_height * 0.5;
    double new_bottom = std::max(old_bottom + delta_norm, old_top + 0.02);
    double new_height = new_bottom - old_top;
    double new_center = old_top + new_height * 0.5;
    emit bandResized(idx, new_center, new_height);
  }
}

void YAxisBarColumn::mouseReleaseEvent(QMouseEvent* /*event*/) {
  if (m_rubber_band_active) {
    m_rubber_band_active = false;
    QRect rb = QRect(m_rubber_band_origin, m_rubber_band_current).normalized();
    if (rb.height() > 3) {
      // Select signals whose bands overlap with the rubber band in both axes
      std::set<int> new_sel;
      if (m_rubber_band_ctrl) new_sel = m_selected;  // Ctrl: add to existing
      for (int i = 0; i < (int)m_signals.size(); i++) {
        double top =
            AxisLayout::bandTopY(m_signals[i], height(), m_scroll_offset);
        double bottom =
            AxisLayout::bandBottomY(m_signals[i], height(), m_scroll_offset);
        double ax = axisX(m_signals[i].bar_x_norm);
        double bar_left = ax - 30;
        double bar_right = ax + 10;
        if (bottom >= rb.top() && top <= rb.bottom() &&
            bar_right >= rb.left() && bar_left <= rb.right())
          new_sel.insert(i);
      }
      m_selected = new_sel;
    } else {
      // Tiny rubber band = click on empty: clear selection
      if (!m_rubber_band_ctrl) m_selected.clear();
    }
    emit selectionChanged();
    update();
    setCursor(Qt::ArrowCursor);
    return;
  }

  m_drag_hit = {-1, NONE};
  setCursor(Qt::ArrowCursor);
}

void YAxisBarColumn::mouseDoubleClickEvent(QMouseEvent* event) {
  auto hit = hitTest(event->pos());
  if (hit.index >= 0) {
    emit editYRangeRequested(hit.index);
  } else {
    double plot_h =
        height() - PlotCanvas::MARGIN_TOP - PlotCanvas::MARGIN_BOTTOM;
    double band_center_norm =
        (plot_h > 0) ? (event->pos().y() - PlotCanvas::MARGIN_TOP) / plot_h +
                           m_scroll_offset
                     : 0.5;
    emit addSignalRequested(band_center_norm);
  }
}

void YAxisBarColumn::wheelEvent(QWheelEvent* event) {
  double delta = (event->angleDelta().y() > 0) ? -0.05 : 0.05;
  emit verticalScrollRequested(delta);
}

// ============================================================================
// YAxisPanel (container)
// ============================================================================

YAxisPanel::YAxisPanel(QWidget* parent) : QWidget(parent) {
  m_label_col = new YAxisLabelColumn(nullptr);
  m_bar_col = new YAxisBarColumn(nullptr);

  m_splitter = new QSplitter(Qt::Horizontal, this);
  m_splitter->setChildrenCollapsible(false);
  m_splitter->addWidget(m_label_col);
  m_splitter->addWidget(m_bar_col);
  m_splitter->setSizes(
      {YAxisLabelColumn::DEFAULT_WIDTH, YAxisBarColumn::DEFAULT_WIDTH});
  m_splitter->setHandleWidth(3);
  m_splitter->setStyleSheet("QSplitter::handle { background: #555; }");

  auto* layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);
  layout->addWidget(m_splitter);

  setMinimumWidth(120);
  setMaximumWidth(400);
  resize(YAxisLabelColumn::DEFAULT_WIDTH + YAxisBarColumn::DEFAULT_WIDTH + 3,
         height());

  // Forward signals from both columns
  connect(m_label_col, &YAxisLabelColumn::bandOffsetChanged, this,
          &YAxisPanel::bandOffsetChanged);
  connect(m_bar_col, &YAxisBarColumn::yRangeChanged, this,
          &YAxisPanel::yRangeChanged);
  connect(m_bar_col, &YAxisBarColumn::bandOffsetChanged, this,
          &YAxisPanel::bandOffsetChanged);
  connect(m_bar_col, &YAxisBarColumn::bandResized, this,
          &YAxisPanel::bandResized);
  connect(m_bar_col, &YAxisBarColumn::barXChanged, this,
          &YAxisPanel::barXChanged);
  connect(m_bar_col, &YAxisBarColumn::removeSignalRequested, this,
          &YAxisPanel::removeSignalRequested);

  // Forward editYRangeRequested from both bar and label columns
  connect(m_bar_col, &YAxisBarColumn::editYRangeRequested, this,
          &YAxisPanel::editYRangeRequested);
  connect(m_label_col, &YAxisLabelColumn::editYRangeRequested, this,
          &YAxisPanel::editYRangeRequested);

  // Forward addSignalRequested from both bar and label columns
  connect(m_bar_col, &YAxisBarColumn::addSignalRequested, this,
          &YAxisPanel::addSignalRequested);
  connect(m_label_col, &YAxisLabelColumn::addSignalRequested, this,
          &YAxisPanel::addSignalRequested);

  // Forward vertical scroll requests
  connect(m_bar_col, &YAxisBarColumn::verticalScrollRequested, this,
          &YAxisPanel::verticalScrollRequested);
  connect(m_label_col, &YAxisLabelColumn::verticalScrollRequested, this,
          &YAxisPanel::verticalScrollRequested);

  // Propagate bar column drag index to label columns (name + value)
  connect(m_bar_col, &YAxisBarColumn::dragIndexChanged, m_label_col,
          &YAxisLabelColumn::setDragIndex);

  // Propagate selection from bar column to label columns and parent
  connect(m_bar_col, &YAxisBarColumn::selectionChanged, this, [this]() {
    m_label_col->setSelection(m_bar_col->selection());
    emit selectionChanged();
  });

  // Handle click-select from label columns
  connect(m_label_col, &YAxisLabelColumn::clickSelect, this,
          [this](int index, bool toggle) {
            std::set<int> sel = m_bar_col->selection();
            if (index < 0) {
              if (!toggle) sel.clear();
            } else if (toggle) {
              if (sel.count(index))
                sel.erase(index);
              else
                sel.insert(index);
            } else if (!sel.count(index)) {
              sel.clear();
              sel.insert(index);
            }
            // If already selected without toggle: don't change (preserves for
            // double-click)
            m_bar_col->setSelection(sel);
          });

  // Handle box-select from label columns — use text row positions (not full
  // band)
  connect(m_label_col, &YAxisLabelColumn::boxSelect, this,
          [this](double y_top, double y_bottom, bool add) {
            std::set<int> sel;
            if (add) sel = m_bar_col->selection();
            int h = m_bar_col->height();
            constexpr int row_h =
                15;  // matches SignalNameColumn::TEXT_ROW_HEIGHT
            auto offsets = AxisLayout::textRowYOffsets(
                m_signals, h, row_h, m_bar_col->scrollOffset());
            for (int i = 0; i < (int)m_signals.size(); i++) {
              double row_top = AxisLayout::bandTopY(m_signals[i], h,
                                                    m_bar_col->scrollOffset()) +
                               offsets[i];
              double row_bottom = row_top + row_h;
              if (row_bottom >= y_top && row_top <= y_bottom) sel.insert(i);
            }
            m_bar_col->setSelection(sel);
          });
}

void YAxisPanel::setSignalEntries(const std::vector<SignalEntry>& entries) {
  m_signals = entries;
  m_label_col->setSignalEntries(entries);
  m_bar_col->setSignalEntries(entries);
}

void YAxisPanel::updateCursorValues(
    const std::shared_ptr<OverlayManager>& overlay_mgr, double cursor_time) {
  if (!overlay_mgr) return;

  std::vector<double> values(m_signals.size(), 0.0);
  std::vector<bool> valid(m_signals.size(), false);

  for (size_t i = 0; i < m_signals.size(); i++) {
    auto resolved = overlay_mgr->resolveSignal(m_signals[i].name);
    if (!resolved || resolved->series->size() == 0) continue;

    // Step-wise lookup: find the last data point at or before cursor_time,
    // accounting for the layer's time offset.
    const auto& series = *resolved->series;
    double local_time = cursor_time - resolved->time_offset;
    auto lb = std::lower_bound(
        series.begin(), series.end(), PJ::PlotData::Point(local_time, 0.0),
        [](const auto& a, const auto& b) { return a.x < b.x; });

    // lower_bound gives first element with x >= local_time
    if (lb != series.end() && lb->x == local_time) {
      values[i] = lb->y;
      valid[i] = true;
    } else if (lb != series.begin()) {
      --lb;  // step back to last point before local_time
      values[i] = lb->y;
      valid[i] = true;
    }
  }

  m_label_col->setCursorValues(values, valid);
}

void YAxisPanel::setCanvasHeight(int h) {
  m_label_col->setCanvasHeight(h);
  m_bar_col->setCanvasHeight(h);
}

void YAxisPanel::setScrollOffset(double offset) {
  m_label_col->setScrollOffset(offset);
  m_bar_col->setScrollOffset(offset);
}

void YAxisPanel::setSnapAmount(double snap) { m_bar_col->setSnapAmount(snap); }

const std::set<int>& YAxisPanel::selection() const {
  return m_bar_col->selection();
}

void YAxisPanel::clearSelection() { m_bar_col->clearSelection(); }

void YAxisPanel::selectAll() { m_bar_col->selectAll(); }
