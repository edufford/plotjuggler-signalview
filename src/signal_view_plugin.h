#pragma once

#include <QtPlugin>
#include "PlotJuggler/toolbox_base.h"

class SignalViewWidget;

class SignalViewPlugin : public PJ::ToolboxPlugin
{
  Q_OBJECT
  Q_PLUGIN_METADATA(IID "facontidavide.PlotJuggler3.Toolbox")
  Q_INTERFACES(PJ::ToolboxPlugin)

public:
  SignalViewPlugin();
  ~SignalViewPlugin() override;

  const char* name() const override { return "Signal View"; }

  void init(PJ::PlotDataMapRef& src_data, PJ::TransformsMap& transform_map) override;

  std::pair<QWidget*, WidgetType> providedWidget() const override;

public slots:
  bool onShowWidget() override;

  bool xmlSaveState(QDomDocument& doc, QDomElement& parent_element) const override;
  bool xmlLoadState(const QDomElement& parent_element) override;

private:
  SignalViewWidget* _widget = nullptr;
  PJ::PlotDataMapRef* _plot_data = nullptr;
  PJ::TransformsMap* _transforms = nullptr;
};
