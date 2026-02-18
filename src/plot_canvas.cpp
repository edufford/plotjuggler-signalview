#include "plot_canvas.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QWheelEvent>
#include <cmath>
#include <algorithm>
#include <limits>

PlotCanvas::PlotCanvas(QWidget* parent)
    : QWidget(parent)
{
  setMinimumSize(200, 150);
  setMouseTracking(true);
  setFocusPolicy(Qt::StrongFocus);
  setAutoFillBackground(true);

  QPalette pal = palette();
  pal.setColor(QPalette::Window, QColor(30, 30, 30));
  setPalette(pal);
}

void PlotCanvas::setDataSource(PJ::PlotDataMapRef* data)
{
  _data = data;
}

void PlotCanvas::setSignalEntries(const std::vector<SignalEntry>& entries)
{
  _signals = entries;
  if (_auto_fit)
  {
    autoFitTimeRange();
  }
  update();
}

void PlotCanvas::setCursorTime(double t)
{
  _cursor_time = t;
  update();
}

void PlotCanvas::resetZoom()
{
  _auto_fit = true;
  autoFitTimeRange();
  update();
  emit viewRangeChanged(_view_t_min, _view_t_max);
}

// --- Coordinate transforms ---

double PlotCanvas::timeToPixelX(double t) const
{
  double plot_w = width() - kMarginLeft - kMarginRight;
  if (_view_t_max <= _view_t_min || plot_w <= 0)
    return kMarginLeft;
  return kMarginLeft + (t - _view_t_min) / (_view_t_max - _view_t_min) * plot_w;
}

double PlotCanvas::pixelXToTime(double px) const
{
  double plot_w = width() - kMarginLeft - kMarginRight;
  if (plot_w <= 0)
    return _view_t_min;
  return _view_t_min + (px - kMarginLeft) / plot_w * (_view_t_max - _view_t_min);
}

double PlotCanvas::valueToPixelY(double value, const SignalEntry& sig) const
{
  double plot_h = height() - kMarginTop - kMarginBottom;
  if (plot_h <= 0 || sig.y_max <= sig.y_min)
    return kMarginTop + plot_h * 0.5;

  // The signal's band occupies a portion of the canvas
  double band_top = kMarginTop + plot_h * (sig.band_center - sig.band_height * 0.5);
  double band_bottom = kMarginTop + plot_h * (sig.band_center + sig.band_height * 0.5);
  double band_h = band_bottom - band_top;

  // Map value within [y_min, y_max] to pixel within [band_bottom, band_top] (Y inverted)
  double normalized = (value - sig.y_min) / (sig.y_max - sig.y_min);
  return band_bottom - normalized * band_h;
}

void PlotCanvas::autoFitTimeRange()
{
  if (!_data || _signals.empty())
    return;

  double t_min = std::numeric_limits<double>::max();
  double t_max = std::numeric_limits<double>::lowest();

  for (const auto& sig : _signals)
  {
    auto it = _data->numeric.find(sig.name);
    if (it == _data->numeric.end() || it->second.size() == 0)
      continue;

    auto range = it->second.rangeX();
    if (range)
    {
      t_min = std::min(t_min, range->min);
      t_max = std::max(t_max, range->max);
    }
  }

  if (t_min < t_max)
  {
    double margin = (t_max - t_min) * 0.02;
    _view_t_min = t_min - margin;
    _view_t_max = t_max + margin;
  }
}

// --- Drawing ---

void PlotCanvas::drawGrid(QPainter& painter)
{
  double plot_w = width() - kMarginLeft - kMarginRight;
  double plot_h = height() - kMarginTop - kMarginBottom;
  if (plot_w <= 0 || plot_h <= 0)
    return;

  painter.setPen(QPen(QColor(60, 60, 60), 1, Qt::DotLine));

  // Horizontal grid lines
  int n_hlines = 5;
  for (int i = 0; i <= n_hlines; i++)
  {
    double y = kMarginTop + plot_h * i / n_hlines;
    painter.drawLine(QPointF(kMarginLeft, y), QPointF(width() - kMarginRight, y));
  }

  // Vertical grid lines — compute nice tick spacing
  double range = _view_t_max - _view_t_min;
  if (range <= 0)
    return;

  double raw_step = range / 8.0;
  double magnitude = std::pow(10.0, std::floor(std::log10(raw_step)));
  double residual = raw_step / magnitude;
  double nice_step;
  if (residual <= 1.5)
    nice_step = 1.0 * magnitude;
  else if (residual <= 3.5)
    nice_step = 2.0 * magnitude;
  else if (residual <= 7.5)
    nice_step = 5.0 * magnitude;
  else
    nice_step = 10.0 * magnitude;

  double t_start = std::ceil(_view_t_min / nice_step) * nice_step;
  for (double t = t_start; t <= _view_t_max; t += nice_step)
  {
    double x = timeToPixelX(t);
    painter.drawLine(QPointF(x, kMarginTop), QPointF(x, kMarginTop + plot_h));
  }
}

void PlotCanvas::drawTimeAxis(QPainter& painter)
{
  double plot_h = height() - kMarginTop - kMarginBottom;
  double plot_w = width() - kMarginLeft - kMarginRight;
  if (plot_w <= 0)
    return;

  double axis_y = kMarginTop + plot_h;

  // Axis line
  painter.setPen(QPen(QColor(180, 180, 180), 1));
  painter.drawLine(QPointF(kMarginLeft, axis_y), QPointF(width() - kMarginRight, axis_y));

  // Tick marks and labels
  double range = _view_t_max - _view_t_min;
  if (range <= 0)
    return;

  double raw_step = range / 8.0;
  double magnitude = std::pow(10.0, std::floor(std::log10(raw_step)));
  double residual = raw_step / magnitude;
  double nice_step;
  if (residual <= 1.5)
    nice_step = 1.0 * magnitude;
  else if (residual <= 3.5)
    nice_step = 2.0 * magnitude;
  else if (residual <= 7.5)
    nice_step = 5.0 * magnitude;
  else
    nice_step = 10.0 * magnitude;

  int decimals = std::max(0, (int)std::ceil(-std::log10(nice_step)));

  painter.setFont(QFont("monospace", 8));
  painter.setPen(QColor(180, 180, 180));

  double t_start = std::ceil(_view_t_min / nice_step) * nice_step;
  for (double t = t_start; t <= _view_t_max; t += nice_step)
  {
    double x = timeToPixelX(t);
    painter.drawLine(QPointF(x, axis_y), QPointF(x, axis_y + 5));

    QString label = QString::number(t, 'f', decimals);
    QRectF text_rect(x - 40, axis_y + 6, 80, 20);
    painter.drawText(text_rect, Qt::AlignHCenter | Qt::AlignTop, label);
  }

  // Axis label
  QRectF label_rect(kMarginLeft, height() - 18, plot_w, 18);
  painter.drawText(label_rect, Qt::AlignCenter, "Time (s)");
}

void PlotCanvas::drawSignals(QPainter& painter)
{
  if (!_data)
    return;

  painter.setRenderHint(QPainter::Antialiasing, true);

  for (const auto& sig : _signals)
  {
    auto it = _data->numeric.find(sig.name);
    if (it == _data->numeric.end())
      continue;

    const PJ::PlotData& series = it->second;
    if (series.size() < 2)
      continue;

    painter.setPen(QPen(sig.color, 1.5));

    QPainterPath path;
    bool first = true;

    for (size_t i = 0; i < series.size(); i++)
    {
      const auto& pt = series[i];
      if (pt.x < _view_t_min || pt.x > _view_t_max)
      {
        // Still draw the boundary points to avoid clipping artifacts
        if (pt.x > _view_t_max && !first)
        {
          double px = timeToPixelX(pt.x);
          double py = valueToPixelY(pt.y, sig);
          path.lineTo(px, py);
          break;
        }
        // Skip until we're near the view, but keep the last pre-view point
        if (i + 1 < series.size() && series[i + 1].x >= _view_t_min)
        {
          double px = timeToPixelX(pt.x);
          double py = valueToPixelY(pt.y, sig);
          path.moveTo(px, py);
          first = false;
        }
        continue;
      }

      double px = timeToPixelX(pt.x);
      double py = valueToPixelY(pt.y, sig);

      if (first)
      {
        path.moveTo(px, py);
        first = false;
      }
      else
      {
        path.lineTo(px, py);
      }
    }

    painter.drawPath(path);
  }

  painter.setRenderHint(QPainter::Antialiasing, false);
}

void PlotCanvas::drawCursor(QPainter& painter)
{
  double x = timeToPixelX(_cursor_time);
  double plot_h = height() - kMarginTop - kMarginBottom;
  if (x < kMarginLeft || x > width() - kMarginRight)
    return;

  painter.setPen(QPen(QColor(255, 255, 100, 200), 1, Qt::DashLine));
  painter.drawLine(QPointF(x, kMarginTop), QPointF(x, kMarginTop + plot_h));

  // Draw a small triangle handle at the top
  QPainterPath handle;
  handle.moveTo(x - 5, kMarginTop);
  handle.lineTo(x + 5, kMarginTop);
  handle.lineTo(x, kMarginTop + 8);
  handle.closeSubpath();
  painter.setBrush(QColor(255, 255, 100, 200));
  painter.setPen(Qt::NoPen);
  painter.drawPath(handle);
}

void PlotCanvas::paintEvent(QPaintEvent* /*event*/)
{
  QPainter painter(this);

  // Background
  painter.fillRect(rect(), QColor(30, 30, 30));

  // Plot area border
  double plot_w = width() - kMarginLeft - kMarginRight;
  double plot_h = height() - kMarginTop - kMarginBottom;
  painter.setPen(QPen(QColor(80, 80, 80), 1));
  painter.drawRect(QRectF(kMarginLeft, kMarginTop, plot_w, plot_h));

  // Clip to plot area for signals
  painter.save();
  painter.setClipRect(QRectF(kMarginLeft, kMarginTop, plot_w, plot_h));
  drawGrid(painter);
  drawSignals(painter);
  drawCursor(painter);
  painter.restore();

  drawTimeAxis(painter);
}

// --- Mouse interaction ---

void PlotCanvas::mousePressEvent(QMouseEvent* event)
{
  if (event->button() == Qt::LeftButton)
  {
    // Check if clicking near the cursor
    double cursor_x = timeToPixelX(_cursor_time);
    if (std::abs(event->pos().x() - cursor_x) < 10 ||
        event->pos().y() < kMarginTop + 10)
    {
      _cursor_dragging = true;
      _cursor_time = pixelXToTime(event->pos().x());
      _cursor_time = std::clamp(_cursor_time, _view_t_min, _view_t_max);
      emit cursorMoved(_cursor_time);
      update();
      return;
    }
    // Otherwise start cursor drag from click position
    _cursor_dragging = true;
    _cursor_time = pixelXToTime(event->pos().x());
    _cursor_time = std::clamp(_cursor_time, _view_t_min, _view_t_max);
    emit cursorMoved(_cursor_time);
    update();
  }
  else if (event->button() == Qt::MiddleButton)
  {
    _panning = true;
    _pan_start = event->pos();
    _pan_t_min_start = _view_t_min;
    _pan_t_max_start = _view_t_max;
    setCursor(Qt::ClosedHandCursor);
  }
}

void PlotCanvas::mouseMoveEvent(QMouseEvent* event)
{
  if (_cursor_dragging)
  {
    _cursor_time = pixelXToTime(event->pos().x());
    _cursor_time = std::clamp(_cursor_time, _view_t_min, _view_t_max);
    emit cursorMoved(_cursor_time);
    update();
  }
  else if (_panning)
  {
    double dx_pixels = event->pos().x() - _pan_start.x();
    double plot_w = width() - kMarginLeft - kMarginRight;
    if (plot_w <= 0)
      return;
    double dt = -dx_pixels / plot_w * (_pan_t_max_start - _pan_t_min_start);
    _view_t_min = _pan_t_min_start + dt;
    _view_t_max = _pan_t_max_start + dt;
    _auto_fit = false;
    emit viewRangeChanged(_view_t_min, _view_t_max);
    update();
  }
  else
  {
    // Show appropriate cursor hint
    double cursor_x = timeToPixelX(_cursor_time);
    if (std::abs(event->pos().x() - cursor_x) < 10)
      setCursor(Qt::SizeHorCursor);
    else
      setCursor(Qt::ArrowCursor);
  }
}

void PlotCanvas::mouseReleaseEvent(QMouseEvent* event)
{
  if (event->button() == Qt::LeftButton)
  {
    _cursor_dragging = false;
  }
  else if (event->button() == Qt::MiddleButton)
  {
    _panning = false;
    setCursor(Qt::ArrowCursor);
  }
}

void PlotCanvas::wheelEvent(QWheelEvent* event)
{
  double plot_w = width() - kMarginLeft - kMarginRight;
  if (plot_w <= 0)
    return;

  // Zoom centered on mouse position
  double mouse_t = pixelXToTime(event->position().x());
  double factor = (event->angleDelta().y() > 0) ? 0.8 : 1.25;

  double new_min = mouse_t + (_view_t_min - mouse_t) * factor;
  double new_max = mouse_t + (_view_t_max - mouse_t) * factor;

  if (new_max - new_min > 1e-9)
  {
    _view_t_min = new_min;
    _view_t_max = new_max;
    _auto_fit = false;
    emit viewRangeChanged(_view_t_min, _view_t_max);
    update();
  }
}

void PlotCanvas::resizeEvent(QResizeEvent* event)
{
  QWidget::resizeEvent(event);
  update();
}
