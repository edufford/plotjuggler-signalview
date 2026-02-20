#include "signal_view_plugin.h"
#include "signal_view_widget.h"
#include "overlay_manager.h"
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
    sig_elem.setAttribute("bar_x", sig.bar_x);
    sig_elem.setAttribute("divisions", sig.divisions);
    sig_elem.setAttribute("line_style", (int)sig.line_style);
    sig_elem.setAttribute("line_width", sig.line_width);
    sig_elem.setAttribute("marker_style", (int)sig.marker_style);
    parent_element.appendChild(sig_elem);
  }

  QDomElement cursor_elem = doc.createElement("cursor");
  cursor_elem.setAttribute("time", _widget->cursorTime());
  parent_element.appendChild(cursor_elem);

  QDomElement view_elem = doc.createElement("view");
  view_elem.setAttribute("t_min", _widget->canvas()->viewMinTime());
  view_elem.setAttribute("t_max", _widget->canvas()->viewMaxTime());
  parent_element.appendChild(view_elem);

  QDomElement settings_elem = doc.createElement("settings");
  settings_elem.setAttribute("snap", _widget->snapAmount());
  parent_element.appendChild(settings_elem);

  // Overlay layers (including base layer time offset)
  auto* overlay_mgr = _widget->overlayManager();
  if (overlay_mgr)
  {
    for (const auto& layer : overlay_mgr->layers())
    {
      QDomElement layer_elem = doc.createElement("overlay");
      layer_elem.setAttribute("layer", layer.index);
      layer_elem.setAttribute("display_name", QString::fromStdString(layer.display_name));
      layer_elem.setAttribute("time_offset", layer.time_offset);
      if (!layer.file_path.empty())
        layer_elem.setAttribute("file", QString::fromStdString(layer.file_path));
      parent_element.appendChild(layer_elem);
    }
  }

  // Splitter sizes: main (panel|canvas), panel (label|bar), label (name|value)
  QDomElement layout_elem = doc.createElement("layout");
  auto main_sizes = _widget->mainSplitter()->sizes();
  if (main_sizes.size() == 2)
  {
    layout_elem.setAttribute("panel_w", main_sizes[0]);
    layout_elem.setAttribute("canvas_w", main_sizes[1]);
  }
  auto panel_sizes = _widget->yAxisPanel()->splitterSizes();
  if (panel_sizes.size() == 2)
  {
    layout_elem.setAttribute("label_w", panel_sizes[0]);
    layout_elem.setAttribute("bar_w", panel_sizes[1]);
  }
  auto label_sizes = _widget->yAxisPanel()->labelSplitterSizes();
  if (label_sizes.size() == 2)
  {
    layout_elem.setAttribute("name_w", label_sizes[0]);
    layout_elem.setAttribute("value_w", label_sizes[1]);
  }
  parent_element.appendChild(layout_elem);

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
    entry.bar_x = sig_elem.attribute("bar_x", "1.0").toDouble();
    entry.divisions = sig_elem.attribute("divisions", "8").toInt();
    entry.line_style = (Qt::PenStyle)sig_elem.attribute("line_style",
        QString::number((int)Qt::SolidLine)).toInt();
    entry.line_width = sig_elem.attribute("line_width", "1.5").toDouble();
    entry.marker_style = (MarkerStyle)sig_elem.attribute("marker_style",
        QString::number((int)MarkerStyle::None)).toInt();

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

  QDomElement settings_elem = parent_element.firstChildElement("settings");
  if (!settings_elem.isNull())
  {
    double snap = settings_elem.attribute("snap", "0.01").toDouble();
    _widget->setSnapAmount(snap);
  }

  // Restore splitter sizes
  QDomElement layout_elem = parent_element.firstChildElement("layout");
  if (!layout_elem.isNull())
  {
    if (layout_elem.hasAttribute("panel_w") && layout_elem.hasAttribute("canvas_w"))
      _widget->mainSplitter()->setSizes(
          { layout_elem.attribute("panel_w").toInt(),
            layout_elem.attribute("canvas_w").toInt() });
    if (layout_elem.hasAttribute("label_w") && layout_elem.hasAttribute("bar_w"))
      _widget->yAxisPanel()->setSplitterSizes(
          { layout_elem.attribute("label_w").toInt(),
            layout_elem.attribute("bar_w").toInt() });
    if (layout_elem.hasAttribute("name_w") && layout_elem.hasAttribute("value_w"))
      _widget->yAxisPanel()->setLabelSplitterSizes(
          { layout_elem.attribute("name_w").toInt(),
            layout_elem.attribute("value_w").toInt() });
  }

  // Restore overlay layers
  auto* overlay_mgr = _widget->overlayManager();
  if (overlay_mgr)
  {
    QDomElement overlay_elem = parent_element.firstChildElement("overlay");
    while (!overlay_elem.isNull())
    {
      int layer_index = overlay_elem.attribute("layer", "0").toInt();
      double time_offset = overlay_elem.attribute("time_offset", "0").toDouble();
      QString display_name = overlay_elem.attribute("display_name");
      QString file_path = overlay_elem.attribute("file");

      if (!file_path.isEmpty())
      {
        // Load the overlay file
        int new_layer = overlay_mgr->loadOverlayFile(file_path.toStdString());
        if (new_layer > 0)
        {
          overlay_mgr->setTimeOffset(new_layer, time_offset);
          if (!display_name.isEmpty())
          {
            auto* layer = overlay_mgr->layerByIndex(new_layer);
            if (layer)
              layer->display_name = display_name.toStdString();
          }
        }
      }
      else
      {
        // Base layer — just restore time offset and display name
        overlay_mgr->setTimeOffset(layer_index, time_offset);
        if (!display_name.isEmpty())
        {
          auto* layer = overlay_mgr->layerByIndex(layer_index);
          if (layer)
            layer->display_name = display_name.toStdString();
        }
      }

      overlay_elem = overlay_elem.nextSiblingElement("overlay");
    }

    // Refresh UI after overlay restore
    _widget->refreshOverlayUI();
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
