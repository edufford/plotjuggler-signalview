#include "signal_view_plugin.h"
#include "signal_view_widget.h"
#include <QStackedWidget>

SignalViewPlugin::SignalViewPlugin() = default;

SignalViewPlugin::~SignalViewPlugin()
{
  if (_widget)
  {
    delete _widget;
    _widget = nullptr;
  }
}

void SignalViewPlugin::init(PJ::PlotDataMapRef& src_data, PJ::TransformsMap& transform_map)
{
  _plot_data = &src_data;
  _transforms = &transform_map;

  _widget = new SignalViewWidget(_plot_data);
  connect(_widget, &QWidget::destroyed, this, [this]() { _widget = nullptr; });
  connect(_widget, &SignalViewWidget::closeRequested, this, &SignalViewPlugin::closed);
}

std::pair<QWidget*, PJ::ToolboxPlugin::WidgetType> SignalViewPlugin::providedWidget() const
{
  return { _widget, PJ::ToolboxPlugin::FLOATING };
}

bool SignalViewPlugin::onShowWidget()
{
  if (_widget)
  {
    _widget->show();
    _widget->raise();
    _widget->activateWindow();
    return true;
  }
  return false;
}

bool SignalViewPlugin::xmlSaveState(QDomDocument& doc, QDomElement& parent_element) const
{
  if (!_widget)
    return false;

  QDomElement widget_elem = doc.createElement("widget");
  widget_elem.setAttribute("visible", _widget->isVisible() ? "1" : "0");
  parent_element.appendChild(widget_elem);

  const auto& sig_entries = _widget->signalEntries();
  for (const auto& sig : sig_entries)
  {
    QDomElement sig_elem = doc.createElement("signal");
    sig_elem.setAttribute("name", QString::fromStdString(sig.name));
    sig_elem.setAttribute("color", sig.color.name());
    sig_elem.setAttribute("y_min", sig.y_min);
    sig_elem.setAttribute("y_max", sig.y_max);
    sig_elem.setAttribute("band_center", sig.band_center);
    sig_elem.setAttribute("band_height", sig.band_height);
    parent_element.appendChild(sig_elem);
  }

  QDomElement cursor_elem = doc.createElement("cursor");
  cursor_elem.setAttribute("time", _widget->cursorTime());
  parent_element.appendChild(cursor_elem);

  QDomElement view_elem = doc.createElement("view");
  view_elem.setAttribute("t_min", _widget->canvas()->viewMinTime());
  view_elem.setAttribute("t_max", _widget->canvas()->viewMaxTime());
  parent_element.appendChild(view_elem);

  return true;
}

bool SignalViewPlugin::xmlLoadState(const QDomElement& parent_element)
{
  if (!_widget)
    return false;

  auto& sig_entries = _widget->signalEntriesMutable();
  sig_entries.clear();

  QDomElement sig_elem = parent_element.firstChildElement("signal");
  while (!sig_elem.isNull())
  {
    SignalEntry entry;
    entry.name = sig_elem.attribute("name").toStdString();
    entry.color = QColor(sig_elem.attribute("color", "#00b4ff"));
    entry.y_min = sig_elem.attribute("y_min", "0").toDouble();
    entry.y_max = sig_elem.attribute("y_max", "1").toDouble();
    entry.band_center = sig_elem.attribute("band_center", "0.5").toDouble();
    entry.band_height = sig_elem.attribute("band_height", "1.0").toDouble();

    // Don't check data existence here — data is loaded after plugins
    sig_entries.push_back(entry);

    sig_elem = sig_elem.nextSiblingElement("signal");
  }

  _widget->canvas()->setSignalEntries(sig_entries);
  _widget->yAxisPanel()->setSignalEntries(sig_entries);

  QDomElement cursor_elem = parent_element.firstChildElement("cursor");
  if (!cursor_elem.isNull())
  {
    double cursor_time = cursor_elem.attribute("time", "0").toDouble();
    _widget->canvas()->setCursorTime(cursor_time);
  }

  QDomElement view_elem = parent_element.firstChildElement("view");
  if (!view_elem.isNull())
  {
    double t_min = view_elem.attribute("t_min", "0").toDouble();
    double t_max = view_elem.attribute("t_max", "10").toDouble();
    _widget->canvas()->setViewRange(t_min, t_max);
  }

  if (_plot_data)
  {
    _widget->yAxisPanel()->updateCursorValues(_plot_data, _widget->cursorTime());
  }

  // Show the widget if it was visible when the layout was saved.
  // PlotJuggler places toolbox widgets inside a QStackedWidget,
  // so we need to switch the stack to our widget's index.
  QDomElement widget_elem = parent_element.firstChildElement("widget");
  if (!widget_elem.isNull() && widget_elem.attribute("visible", "0") == "1")
  {
    if (auto* stack = qobject_cast<QStackedWidget*>(_widget->parentWidget()))
    {
      int idx = stack->indexOf(_widget);
      if (idx >= 0)
        stack->setCurrentIndex(idx);
    }
    onShowWidget();
  }

  return true;
}
