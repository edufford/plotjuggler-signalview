#include "y_axis_panel.h"

#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QInputDialog>
#include <cmath>
#include <algorithm>

YAxisPanel::YAxisPanel(QWidget* parent)
    : QWidget(parent)
{
  setFixedWidth(kPanelWidth);
  setMouseTracking(true);
  setAutoFillBackground(true);

  QPalette pal = palette();
  pal.setColor(QPalette::Window, QColor(35, 35, 35));
  setPalette(pal);
}

void YAxisPanel::setSignalEntries(const std::vector<SignalEntry>& entries)
{
  _signals = entries;
  _cursor_values.resize(entries.size(), 0.0);
  _cursor_valid.resize(entries.size(), false);
  update();
}

void YAxisPanel::updateCursorValues(PJ::PlotDataMapRef* data, double cursor_time)
{
  if (!data)
    return;

  _cursor_values.resize(_signals.size(), 0.0);
  _cursor_valid.resize(_signals.size(), false);

  for (size_t i = 0; i < _signals.size(); i++)
  {
    auto it = data->numeric.find(_signals[i].name);
    if (it == data->numeric.end() || it->second.size() == 0)
    {
      _cursor_valid[i] = false;
      continue;
    }

    auto val = it->second.getYfromX(cursor_time);
    if (val.has_value())
    {
      _cursor_values[i] = val.value();
      _cursor_valid[i] = true;
    }
    else
    {
      _cursor_valid[i] = false;
    }
  }
  update();
}

void YAxisPanel::setCanvasHeight(int h)
{
  _canvas_height = h;
  setFixedHeight(h);
  update();
}

// --- Coordinate helpers (must match PlotCanvas::valueToPixelY layout) ---

double YAxisPanel::bandTopY(const SignalEntry& sig) const
{
  double plot_h = _canvas_height - PlotCanvas::kMarginTop - PlotCanvas::kMarginBottom;
  return PlotCanvas::kMarginTop + plot_h * (sig.band_center - sig.band_height * 0.5);
}

double YAxisPanel::bandBottomY(const SignalEntry& sig) const
{
  double plot_h = _canvas_height - PlotCanvas::kMarginTop - PlotCanvas::kMarginBottom;
  return PlotCanvas::kMarginTop + plot_h * (sig.band_center + sig.band_height * 0.5);
}

// --- Hit testing ---

YAxisPanel::HitResult YAxisPanel::hitTest(const QPoint& pos) const
{
  for (int i = 0; i < (int)_signals.size(); i++)
  {
    double top = bandTopY(_signals[i]);
    double bottom = bandBottomY(_signals[i]);

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

// --- Paint ---

void YAxisPanel::paintEvent(QPaintEvent* /*event*/)
{
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.fillRect(rect(), QColor(35, 35, 35));

  for (int i = 0; i < (int)_signals.size(); i++)
  {
    const auto& sig = _signals[i];
    double top = bandTopY(sig);
    double bottom = bandBottomY(sig);
    double band_h = bottom - top;

    if (band_h < 4)
      continue;

    // Subtle background for the band
    painter.fillRect(QRectF(0, top, kPanelWidth - 1, band_h), QColor(42, 42, 42));

    // Color stripe on the right edge (adjacent to canvas)
    painter.fillRect(QRectF(kPanelWidth - 5, top, 5, band_h), sig.color);

    // Axis line
    int axis_x = kPanelWidth - 12;
    painter.setPen(QPen(sig.color.lighter(130), 1.5));
    painter.drawLine(QPointF(axis_x, top), QPointF(axis_x, bottom));

    // Edge grab handles — small horizontal bars
    painter.setPen(QPen(sig.color, 2));
    painter.drawLine(QPointF(axis_x - 8, top), QPointF(axis_x + 4, top));
    painter.drawLine(QPointF(axis_x - 8, bottom), QPointF(axis_x + 4, bottom));

    // Tick marks and value labels
    int n_ticks = std::max(2, (int)(band_h / 30));
    painter.setFont(QFont("monospace", 7));
    painter.setPen(QColor(170, 170, 170));

    for (int t = 0; t <= n_ticks; t++)
    {
      double frac = (double)t / n_ticks;
      double y = bottom - frac * band_h;
      double val = sig.y_min + frac * (sig.y_max - sig.y_min);

      // Tick mark
      painter.drawLine(QPointF(axis_x - 3, y), QPointF(axis_x + 3, y));

      // Label
      QString label = QString::number(val, 'g', 4);
      QRectF text_rect(2, y - 8, axis_x - 8, 16);
      painter.drawText(text_rect, Qt::AlignRight | Qt::AlignVCenter, label);
    }

    // Signal name (at top of band)
    painter.setPen(sig.color);
    QFont name_font("sans-serif", 8, QFont::Bold);
    painter.setFont(name_font);
    QString name = QString::fromStdString(sig.name);
    QFontMetrics fm(name_font);
    QString elided = fm.elidedText(name, Qt::ElideMiddle, axis_x - 10);
    painter.drawText(QRectF(4, top + 2, axis_x - 10, 14),
                     Qt::AlignLeft | Qt::AlignTop, elided);

    // Cursor readout (centered in band)
    if (i < (int)_cursor_valid.size())
    {
      painter.setPen(QColor(255, 255, 100));
      QFont readout_font("monospace", 9, QFont::Bold);
      painter.setFont(readout_font);

      QString value_str = _cursor_valid[i]
                              ? QString::number(_cursor_values[i], 'g', 6)
                              : QStringLiteral("---");

      double center_y = (top + bottom) * 0.5;
      painter.drawText(QRectF(4, center_y - 8, axis_x - 10, 16),
                       Qt::AlignLeft | Qt::AlignVCenter, value_str);
    }
  }
}

// --- Mouse interaction ---

void YAxisPanel::mousePressEvent(QMouseEvent* event)
{
  if (event->button() == Qt::LeftButton)
  {
    _drag_hit = hitTest(event->pos());
    if (_drag_hit.index < 0)
      return;

    _drag_start_global_y = event->globalPos().y();
    _drag_start_band_center = _signals[_drag_hit.index].band_center;
    _drag_start_band_height = _signals[_drag_hit.index].band_height;
    _drag_start_y_min = _signals[_drag_hit.index].y_min;
    _drag_start_y_max = _signals[_drag_hit.index].y_max;

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

void YAxisPanel::mouseMoveEvent(QMouseEvent* event)
{
  if (_drag_hit.index < 0)
  {
    // Update cursor shape on hover
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
  double plot_h = _canvas_height - PlotCanvas::kMarginTop - PlotCanvas::kMarginBottom;
  if (plot_h <= 0)
    return;

  int idx = _drag_hit.index;

  if (_drag_hit.zone == BODY)
  {
    // Drag the whole band up/down — change band_center in normalized space
    double delta = dy / plot_h;
    double new_center = std::clamp(_drag_start_band_center + delta, 0.0, 1.0);
    emit bandOffsetChanged(idx, new_center);
  }
  else if (_drag_hit.zone == TOP_EDGE)
  {
    // Dragging the top edge resizes the bar upward/downward.
    // dy > 0 means mouse moved down → top edge moves down → bar shrinks.
    // Convert pixel delta to normalized-space delta.
    double delta_norm = dy / plot_h;
    // The top edge in normalized space is (center - height/2).
    // New top = old_top + delta_norm, bottom stays fixed.
    double old_top = _drag_start_band_center - _drag_start_band_height * 0.5;
    double old_bottom = _drag_start_band_center + _drag_start_band_height * 0.5;
    double new_top = std::clamp(old_top + delta_norm, 0.0, old_bottom - 0.02);
    double new_height = old_bottom - new_top;
    double new_center = new_top + new_height * 0.5;
    emit bandResized(idx, new_center, new_height);
  }
  else if (_drag_hit.zone == BOTTOM_EDGE)
  {
    // Dragging the bottom edge resizes the bar downward/upward.
    // dy > 0 means mouse moved down → bottom edge moves down → bar grows.
    double delta_norm = dy / plot_h;
    double old_top = _drag_start_band_center - _drag_start_band_height * 0.5;
    double old_bottom = _drag_start_band_center + _drag_start_band_height * 0.5;
    double new_bottom = std::clamp(old_bottom + delta_norm, old_top + 0.02, 1.0);
    double new_height = new_bottom - old_top;
    double new_center = old_top + new_height * 0.5;
    emit bandResized(idx, new_center, new_height);
  }
}

void YAxisPanel::mouseReleaseEvent(QMouseEvent* /*event*/)
{
  _drag_hit = { -1, NONE };
  setCursor(Qt::ArrowCursor);
}

void YAxisPanel::mouseDoubleClickEvent(QMouseEvent* event)
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

void YAxisPanel::wheelEvent(QWheelEvent* event)
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
