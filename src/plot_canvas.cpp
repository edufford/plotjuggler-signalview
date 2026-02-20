#include "plot_canvas.h"
#include "overlay_manager.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QLineEdit>
#include <QDoubleValidator>
#include <cmath>
#include <algorithm>
#include <limits>

namespace {
QCursor makeZoomCursor()
{
  constexpr int sz = 32;
  QPixmap pix(sz, sz);
  pix.fill(Qt::transparent);

  QPainter p(&pix);
  p.setRenderHint(QPainter::Antialiasing);

  // Arrow pointer at top-left (tip at 0,0)
  QPainterPath arrow;
  arrow.moveTo(0, 0);
  arrow.lineTo(0, 18);
  arrow.lineTo(5, 13);
  arrow.lineTo(10, 13);
  arrow.closeSubpath();
  p.setPen(QPen(Qt::black, 1.0));
  p.setBrush(Qt::white);
  p.drawPath(arrow);

  // Magnifying glass lens close to arrow
  constexpr double cx = 17, cy = 17, r = 7;
  p.setPen(QPen(Qt::white, 1.6));
  p.setBrush(QColor(0, 0, 0, 140));
  p.drawEllipse(QPointF(cx, cy), r, r);

  // Short handle from lens edge toward bottom-right
  p.setPen(QPen(Qt::white, 1.8));
  double hx = cx + r * 0.707;
  double hy = cy + r * 0.707;
  p.drawLine(QPointF(hx, hy), QPointF(hx + 3, hy + 3));

  // Horizontal double arrow inside the lens
  p.setPen(QPen(Qt::white, 1.2));
  double ay = cy;
  double al = cx - 4, ar = cx + 4;
  p.drawLine(QPointF(al, ay), QPointF(ar, ay));
  // Left arrowhead
  p.drawLine(QPointF(al, ay), QPointF(al + 2.2, ay - 1.8));
  p.drawLine(QPointF(al, ay), QPointF(al + 2.2, ay + 1.8));
  // Right arrowhead
  p.drawLine(QPointF(ar, ay), QPointF(ar - 2.2, ay - 1.8));
  p.drawLine(QPointF(ar, ay), QPointF(ar - 2.2, ay + 1.8));

  p.end();
  return QCursor(pix, 0, 0);  // hotspot at arrow tip
}
QCursor makeTimeShiftCursor()
{
  constexpr int sz = 24;
  QPixmap pix(sz, sz);
  pix.fill(Qt::transparent);

  QPainter p(&pix);
  p.setRenderHint(QPainter::Antialiasing);

  // Sine wave in the upper portion
  QPainterPath wave;
  constexpr double wave_cx = 12, wave_cy = 8;
  constexpr double wave_w = 9, wave_h = 5;
  constexpr int steps = 20;
  for (int i = 0; i <= steps; i++)
  {
    double t = (double)i / steps;
    double x = wave_cx - wave_w + 2 * wave_w * t;
    double y = wave_cy - wave_h * std::sin(t * 2 * M_PI);
    if (i == 0)
      wave.moveTo(x, y);
    else
      wave.lineTo(x, y);
  }
  p.setPen(QPen(Qt::white, 1.5));
  p.drawPath(wave);

  // Horizontal double arrow below the sine wave
  constexpr double ay = 18;
  constexpr double al = 3, ar = 21;
  p.setPen(QPen(Qt::white, 1.4));
  p.drawLine(QPointF(al, ay), QPointF(ar, ay));
  // Left arrowhead
  p.drawLine(QPointF(al, ay), QPointF(al + 3, ay - 2.5));
  p.drawLine(QPointF(al, ay), QPointF(al + 3, ay + 2.5));
  // Right arrowhead
  p.drawLine(QPointF(ar, ay), QPointF(ar - 3, ay - 2.5));
  p.drawLine(QPointF(ar, ay), QPointF(ar - 3, ay + 2.5));

  p.end();
  return QCursor(pix, 12, 12);
}
}  // namespace

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

  // Time range edit fields at the bottom corners of the plot area
  auto setupTimeEdit = [this](QLineEdit* edit) {
    edit->setValidator(new QDoubleValidator(edit));
    edit->setFixedHeight(16);
    edit->setFrame(false);
    edit->setStyleSheet(
        "QLineEdit {"
        "  background: rgba(30, 30, 30, 220);"
        "  color: #b4b4b4;"
        "  border: 1px solid #555;"
        "  font: bold 8pt monospace;"
        "  padding: 0px 2px;"
        "}");
  };

  _time_start_edit = new QLineEdit(this);
  _time_end_edit = new QLineEdit(this);
  setupTimeEdit(_time_start_edit);
  setupTimeEdit(_time_end_edit);
  _time_end_edit->setAlignment(Qt::AlignRight);

  connect(_time_start_edit, &QLineEdit::editingFinished, this, [this]() {
    bool ok;
    double val = _time_start_edit->text().toDouble(&ok);
    if (ok && val < _view_t_max) {
      _view_t_min = val;
      _auto_fit = false;
      emit viewRangeChanged(_view_t_min, _view_t_max);
      update();
    }
    updateTimeEditTexts();
  });

  connect(_time_end_edit, &QLineEdit::editingFinished, this, [this]() {
    bool ok;
    double val = _time_end_edit->text().toDouble(&ok);
    if (ok && val > _view_t_min) {
      _view_t_max = val;
      _auto_fit = false;
      emit viewRangeChanged(_view_t_min, _view_t_max);
      update();
    }
    updateTimeEditTexts();
  });

  updateTimeEditTexts();
}

void PlotCanvas::setDataSource(OverlayManager* mgr)
{
  _overlay_mgr = mgr;
}

void PlotCanvas::setSignalEntries(const std::vector<SignalEntry>& entries)
{
  _signals = entries;
  _cursor_needs_data = true;
  if (_auto_fit)
  {
    autoFitTimeRange();
    // Snap cursor into data range if it's currently outside
    if (_cursor_time < _view_t_min || _cursor_time > _view_t_max)
      _cursor_time = _view_t_min;
  }
  update();
  emit cursorMoved(_cursor_time);
}

void PlotCanvas::setCursorTime(double t)
{
  _cursor_time = t;
  update();
}

void PlotCanvas::setViewRange(double t_min, double t_max)
{
  _view_t_min = t_min;
  _view_t_max = t_max;
  _auto_fit = false;
  updateTimeEditTexts();
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

  // The signal's band occupies a portion of the canvas, shifted by scroll offset
  double band_top = kMarginTop + plot_h * (sig.band_center - sig.band_height * 0.5 - _scroll_offset);
  double band_bottom = kMarginTop + plot_h * (sig.band_center + sig.band_height * 0.5 - _scroll_offset);
  double band_h = band_bottom - band_top;

  // Map value within [y_min, y_max] to pixel within [band_bottom, band_top] (Y inverted)
  double normalized = (value - sig.y_min) / (sig.y_max - sig.y_min);
  return band_bottom - normalized * band_h;
}

void PlotCanvas::setScrollOffset(double offset)
{
  _scroll_offset = offset;
  update();
}

void PlotCanvas::setZoomMode(bool enabled)
{
  _zoom_mode = enabled;
  updateIdleCursor();
}

void PlotCanvas::setTimeShiftMode(bool enabled)
{
  _time_shift_mode = enabled;
  updateIdleCursor();
}

void PlotCanvas::updateIdleCursor()
{
  if (_time_shift_mode)
  {
    static QCursor shift_cursor = makeTimeShiftCursor();
    setCursor(shift_cursor);
  }
  else if (_zoom_mode)
  {
    static QCursor zoom_cursor = makeZoomCursor();
    setCursor(zoom_cursor);
  }
  else
    setCursor(Qt::ArrowCursor);
}

void PlotCanvas::setSelectedLayers(const std::set<int>& layers)
{
  _selected_layers = layers;
}

void PlotCanvas::setDefaultShiftLayer(int layer_index)
{
  _default_shift_layer = layer_index;
}

void PlotCanvas::repositionTimeEdits()
{
  const int field_w = 80;
  const int field_h = 16;
  int y = height() - field_h - 1;
  _time_start_edit->setGeometry(kMarginLeft, y, field_w, field_h);
  _time_end_edit->setGeometry(width() - kMarginRight - field_w, y, field_w, field_h);
}

void PlotCanvas::updateTimeEditTexts()
{
  if (!_time_start_edit->hasFocus())
    _time_start_edit->setText(QString::number(_view_t_min, 'g', 6));
  if (!_time_end_edit->hasFocus())
    _time_end_edit->setText(QString::number(_view_t_max, 'g', 6));
}

void PlotCanvas::autoFitTimeRange()
{
  if (!_overlay_mgr || _signals.empty())
    return;

  double t_min = std::numeric_limits<double>::max();
  double t_max = std::numeric_limits<double>::lowest();

  for (const auto& sig : _signals)
  {
    auto resolved = _overlay_mgr->resolveSignal(sig.name);
    if (!resolved || resolved->series->size() == 0)
      continue;

    auto range = resolved->series->rangeX();
    if (range)
    {
      t_min = std::min(t_min, range->min + resolved->time_offset);
      t_max = std::max(t_max, range->max + resolved->time_offset);
    }
  }

  if (t_min < t_max)
  {
    _view_t_min = t_min;
    _view_t_max = t_max;
  }
  updateTimeEditTexts();
}

// --- Drawing ---

void PlotCanvas::drawGrid(QPainter& painter)
{
  double plot_w = width() - kMarginLeft - kMarginRight;
  double plot_h = height() - kMarginTop - kMarginBottom;
  if (plot_w <= 0 || plot_h <= 0)
    return;

  // Per-signal horizontal grid lines aligned to each signal's Y-axis divisions
  for (const auto& sig : _signals)
  {
    double band_top = kMarginTop + plot_h * (sig.band_center - sig.band_height * 0.5 - _scroll_offset);
    double band_bottom = kMarginTop + plot_h * (sig.band_center + sig.band_height * 0.5 - _scroll_offset);
    double band_h = band_bottom - band_top;

    if (band_h < 4)
      continue;

    // Same tick count as the Y-axis bar column
    int n_ticks = (sig.divisions > 0) ? sig.divisions : std::clamp((int)(band_h / SignalEntry::kPixelsPerAutoTick), 2, SignalEntry::kMaxDivisions);

    painter.setPen(QPen(QColor(60, 60, 60), 1, Qt::DotLine));

    for (int t = 0; t <= n_ticks; t++)
    {
      double frac = (double)t / n_ticks;
      double y = band_bottom - frac * band_h;
      painter.drawLine(QPointF(kMarginLeft, y), QPointF(width() - kMarginRight, y));
    }
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

  // Axis label — centered between the time edit fields
  int label_left = kMarginLeft + 84;
  int label_right = width() - kMarginRight - 84;
  if (label_right > label_left)
  {
    QRectF label_rect(label_left, height() - 18, label_right - label_left, 18);
    painter.drawText(label_rect, Qt::AlignCenter, "Time (s)");
  }
}

void PlotCanvas::drawSignals(QPainter& painter)
{
  if (!_overlay_mgr)
    return;

  painter.setRenderHint(QPainter::Antialiasing, true);

  const double plot_w = width() - kMarginLeft - kMarginRight;

  for (const auto& sig : _signals)
  {
    auto resolved = _overlay_mgr->resolveSignal(sig.name);
    if (!resolved)
      continue;

    const PJ::PlotData& series = *resolved->series;
    double t_offset = resolved->time_offset;
    if (series.size() < 2)
      continue;

    painter.setPen(QPen(sig.color, sig.line_width, sig.line_style));

    // Binary search for the first point at or after view_t_min (in local time)
    double local_t_min = _view_t_min - t_offset;
    double local_t_max = _view_t_max - t_offset;
    auto lb = std::lower_bound(
        series.begin(), series.end(),
        PJ::PlotData::Point(local_t_min, 0.0),
        [](const auto& a, const auto& b) { return a.x < b.x; });

    // Include one point before the view for the entry hold value
    size_t start_idx = (lb != series.begin()) ? (lb - series.begin() - 1) : 0;

    QPainterPath path;
    bool first = true;

    // Determine if downsampling is needed: compare visible point count to pixel width
    auto ub = std::lower_bound(
        series.begin(), series.end(),
        PJ::PlotData::Point(local_t_max, 0.0),
        [](const auto& a, const auto& b) { return a.x < b.x; });
    size_t end_idx = std::min((size_t)(ub - series.begin() + 1), series.size());
    size_t visible_count = (end_idx > start_idx) ? (end_idx - start_idx) : 0;
    bool downsample = (plot_w > 0 && visible_count > (size_t)(plot_w * 2));

    if (downsample)
    {
      // Per-pixel-column min/max downsampling for step-wise signals.
      // For each pixel column, we track the first value (entry hold), min, max,
      // and last value. We emit: hold line to column, then min→max or max→min
      // vertical extent, then the exit value. This preserves every visible
      // vertical extent identically to full-resolution rendering.

      int prev_px_col = -1;
      double col_first_y = 0, col_min_y = 0, col_max_y = 0, col_last_y = 0;
      int col_min_idx = 0, col_max_idx = 0;  // track order of min/max

      for (size_t i = start_idx; i < series.size(); i++)
      {
        const auto& pt = series[i];
        double t = pt.x + t_offset;

        if (t > _view_t_max)
        {
          // Flush current column
          if (prev_px_col >= 0 && !first)
          {
            double cpx = (double)prev_px_col;
            // Hold line from previous position
            path.lineTo(cpx, path.currentPosition().y());
            path.lineTo(cpx, col_first_y);
            // Draw min/max extent in order of occurrence
            if (col_min_idx < col_max_idx)
            {
              path.lineTo(cpx, col_min_y);
              path.lineTo(cpx, col_max_y);
            }
            else
            {
              path.lineTo(cpx, col_max_y);
              path.lineTo(cpx, col_min_y);
            }
            path.lineTo(cpx, col_last_y);
          }
          // Draw boundary point for clean exit
          if (!first)
          {
            double px = timeToPixelX(t);
            double py = valueToPixelY(pt.y, sig);
            path.lineTo(px, path.currentPosition().y());
            path.lineTo(px, py);
          }
          break;
        }

        double px = timeToPixelX(t);
        double py = valueToPixelY(pt.y, sig);
        int px_col = (int)std::round(px);

        if (first)
        {
          path.moveTo(px, py);
          first = false;
          prev_px_col = px_col;
          col_first_y = col_min_y = col_max_y = col_last_y = py;
          col_min_idx = col_max_idx = 0;
          continue;
        }

        if (px_col == prev_px_col)
        {
          // Same pixel column — accumulate min/max
          int idx = (int)(i - start_idx);
          if (py < col_min_y) { col_min_y = py; col_min_idx = idx; }
          if (py > col_max_y) { col_max_y = py; col_max_idx = idx; }
          col_last_y = py;
        }
        else
        {
          // Flush previous pixel column
          double cpx = (double)prev_px_col;
          path.lineTo(cpx, path.currentPosition().y());
          path.lineTo(cpx, col_first_y);
          if (col_min_idx < col_max_idx)
          {
            path.lineTo(cpx, col_min_y);
            path.lineTo(cpx, col_max_y);
          }
          else
          {
            path.lineTo(cpx, col_max_y);
            path.lineTo(cpx, col_min_y);
          }
          path.lineTo(cpx, col_last_y);

          // Start new column
          prev_px_col = px_col;
          col_first_y = col_min_y = col_max_y = col_last_y = py;
          col_min_idx = col_max_idx = (int)(i - start_idx);
        }
      }

      // Flush last column if loop ended without exceeding view
      if (prev_px_col >= 0 && !first)
      {
        double cpx = (double)prev_px_col;
        path.lineTo(cpx, path.currentPosition().y());
        path.lineTo(cpx, col_first_y);
        if (col_min_idx < col_max_idx)
        {
          path.lineTo(cpx, col_min_y);
          path.lineTo(cpx, col_max_y);
        }
        else
        {
          path.lineTo(cpx, col_max_y);
          path.lineTo(cpx, col_min_y);
        }
        path.lineTo(cpx, col_last_y);
      }
    }
    else
    {
      // Full-resolution step-wise rendering
      for (size_t i = start_idx; i < series.size(); i++)
      {
        const auto& pt = series[i];
        double t = pt.x + t_offset;

        if (t > _view_t_max && !first)
        {
          double px = timeToPixelX(t);
          double py = valueToPixelY(pt.y, sig);
          path.lineTo(px, path.currentPosition().y());
          path.lineTo(px, py);
          break;
        }

        double px = timeToPixelX(t);
        double py = valueToPixelY(pt.y, sig);

        if (first)
        {
          path.moveTo(px, py);
          first = false;
        }
        else
        {
          path.lineTo(px, path.currentPosition().y());
          path.lineTo(px, py);
        }
      }
    }

    painter.drawPath(path);

    // Draw markers at data points (skip when downsampled — markers are sub-pixel)
    if (sig.marker_style != MarkerStyle::None && !downsample)
    {
      const double r = sig.line_width + 1.5;  // marker radius scales with line width
      bool filled = (sig.marker_style == MarkerStyle::FilledCircle ||
                     sig.marker_style == MarkerStyle::FilledSquare ||
                     sig.marker_style == MarkerStyle::FilledTriangle);
      if (filled)
      {
        painter.setPen(Qt::NoPen);
        painter.setBrush(sig.color);
      }
      else
      {
        painter.setPen(QPen(sig.color, sig.line_width));
        painter.setBrush(Qt::NoBrush);
      }

      for (size_t i = start_idx; i < series.size(); i++)
      {
        const auto& pt = series[i];
        double t = pt.x + t_offset;
        if (t < _view_t_min)
          continue;
        if (t > _view_t_max)
          break;

        double px = timeToPixelX(t);
        double py = valueToPixelY(pt.y, sig);

        switch (sig.marker_style)
        {
          case MarkerStyle::FilledCircle:
          case MarkerStyle::OpenCircle:
            painter.drawEllipse(QPointF(px, py), r, r);
            break;
          case MarkerStyle::FilledSquare:
          case MarkerStyle::OpenSquare:
            painter.drawRect(QRectF(px - r, py - r, r * 2, r * 2));
            break;
          case MarkerStyle::FilledTriangle:
          case MarkerStyle::OpenTriangle:
          {
            QPainterPath tri;
            tri.moveTo(px, py - r);
            tri.lineTo(px - r, py + r);
            tri.lineTo(px + r, py + r);
            tri.closeSubpath();
            painter.drawPath(tri);
            break;
          }
          default:
            break;
        }
      }
      painter.setBrush(Qt::NoBrush);
    }
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

  // Time label next to the triangle
  painter.setPen(QColor(255, 255, 100, 200));
  painter.setFont(QFont("monospace", 8));
  QString time_str = QString::number(_cursor_time, 'g', 6);
  QRectF text_rect(x + 7, kMarginTop - 2, 80, 14);
  // Flip to the left side if too close to the right edge
  if (x + 7 + 80 > width() - kMarginRight)
    text_rect = QRectF(x - 87, kMarginTop - 2, 80, 14);
  painter.drawText(text_rect, (text_rect.left() < x ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter, time_str);
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

  // Zoom rubber band overlay
  if (_zoom_selecting)
  {
    double x1 = std::max(_zoom_select_start_x, (double)kMarginLeft);
    double x2 = std::max(_zoom_select_current_x, (double)kMarginLeft);
    x1 = std::min(x1, (double)(width() - kMarginRight));
    x2 = std::min(x2, (double)(width() - kMarginRight));
    double left = std::min(x1, x2);
    double right = std::max(x1, x2);
    painter.fillRect(QRectF(left, kMarginTop, right - left, plot_h),
                     QColor(255, 255, 100, 40));
    painter.setPen(QPen(QColor(255, 255, 100, 160), 1));
    painter.drawLine(QPointF(left, kMarginTop), QPointF(left, kMarginTop + plot_h));
    painter.drawLine(QPointF(right, kMarginTop), QPointF(right, kMarginTop + plot_h));
  }

  painter.restore();

  drawTimeAxis(painter);

  // Detect when signal data first becomes available (e.g. after layout
  // restore where data loads after the plugin state is restored).
  if (_cursor_needs_data && _overlay_mgr && !_signals.empty())
  {
    for (const auto& sig : _signals)
    {
      auto resolved = _overlay_mgr->resolveSignal(sig.name);
      if (resolved && resolved->series->size() > 0)
      {
        _cursor_needs_data = false;
        if (_auto_fit)
        {
          autoFitTimeRange();
          if (_cursor_time < _view_t_min || _cursor_time > _view_t_max)
            _cursor_time = _view_t_min;
        }
        emit cursorMoved(_cursor_time);
        update();
        break;
      }
    }
  }
}

// --- Mouse interaction ---

void PlotCanvas::mousePressEvent(QMouseEvent* event)
{
  if (event->button() == Qt::LeftButton)
  {
    if (_time_shift_mode && _overlay_mgr)
    {
      // Start time shift drag
      _time_shift_dragging = true;
      _time_shift_start = event->pos();
      // Capture current offsets for target layers
      _time_shift_start_offsets.clear();
      std::set<int> targets = _selected_layers.empty()
          ? std::set<int>{_default_shift_layer}
          : _selected_layers;
      for (int layer_idx : targets)
        _time_shift_start_offsets[layer_idx] = _overlay_mgr->timeOffset(layer_idx);
      setCursor(Qt::SizeHorCursor);
      return;
    }
    if (_zoom_mode)
    {
      // Start rubber-band zoom selection
      _zoom_selecting = true;
      _zoom_select_start_x = event->pos().x();
      _zoom_select_current_x = event->pos().x();
      update();
      return;
    }
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
  else if (event->button() == Qt::RightButton)
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
  if (_time_shift_dragging && _overlay_mgr)
  {
    double dx_pixels = event->pos().x() - _time_shift_start.x();
    double plot_w = width() - kMarginLeft - kMarginRight;
    if (plot_w <= 0)
      return;
    double dt = dx_pixels / plot_w * (_view_t_max - _view_t_min);
    for (auto& [layer_idx, start_offset] : _time_shift_start_offsets)
      _overlay_mgr->setTimeOffset(layer_idx, start_offset + dt);
    emit timeShiftChanged();
    update();
    return;
  }
  if (_zoom_selecting)
  {
    _zoom_select_current_x = event->pos().x();
    update();
    return;
  }
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
    updateTimeEditTexts();
    emit viewRangeChanged(_view_t_min, _view_t_max);
    update();
  }
  else
  {
    // Show appropriate cursor hint
    if (_time_shift_mode || _zoom_mode)
    {
      updateIdleCursor();
    }
    else
    {
      double cursor_x = timeToPixelX(_cursor_time);
      if (std::abs(event->pos().x() - cursor_x) < 10)
        setCursor(Qt::SizeHorCursor);
      else
        setCursor(Qt::ArrowCursor);
    }
  }
}

void PlotCanvas::mouseReleaseEvent(QMouseEvent* event)
{
  if (event->button() == Qt::LeftButton)
  {
    if (_time_shift_dragging)
    {
      _time_shift_dragging = false;
      _time_shift_start_offsets.clear();
      updateIdleCursor();
      return;
    }
    if (_zoom_selecting)
    {
      _zoom_selecting = false;
      double t1 = pixelXToTime(_zoom_select_start_x);
      double t2 = pixelXToTime(event->pos().x());
      double new_min = std::min(t1, t2);
      double new_max = std::max(t1, t2);
      // Only zoom if the selection spans a meaningful range
      if (new_max - new_min > 1e-9)
      {
        _view_t_min = new_min;
        _view_t_max = new_max;
        _auto_fit = false;
        updateTimeEditTexts();
        emit viewRangeChanged(_view_t_min, _view_t_max);
      }
      update();
      return;
    }
    _cursor_dragging = false;
  }
  else if (event->button() == Qt::RightButton)
  {
    _panning = false;
    updateIdleCursor();
  }
}

void PlotCanvas::wheelEvent(QWheelEvent* event)
{
  if (!_zoom_mode)
  {
    // Default: vertical scroll
    double delta = (event->angleDelta().y() > 0) ? -0.05 : 0.05;
    emit verticalScrollRequested(delta);
    return;
  }

  // Zoom mode: time-axis zoom centered on mouse position
  double plot_w = width() - kMarginLeft - kMarginRight;
  if (plot_w <= 0)
    return;

  double mouse_t = pixelXToTime(event->position().x());
  double factor = (event->angleDelta().y() > 0) ? 0.8 : 1.25;

  double new_min = mouse_t + (_view_t_min - mouse_t) * factor;
  double new_max = mouse_t + (_view_t_max - mouse_t) * factor;

  if (new_max - new_min > 1e-9)
  {
    _view_t_min = new_min;
    _view_t_max = new_max;
    _auto_fit = false;
    updateTimeEditTexts();
    emit viewRangeChanged(_view_t_min, _view_t_max);
    update();
  }
}

void PlotCanvas::resizeEvent(QResizeEvent* event)
{
  QWidget::resizeEvent(event);
  repositionTimeEdits();
  emit canvasResized(height());
  update();
}
