#include "data_sets_panel.h"

#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QMenu>

#include "overlay_manager.h"

DataSetsPanel::DataSetsPanel(QWidget* parent) : QWidget(parent) {
  auto* outer = new QVBoxLayout(this);
  outer->setContentsMargins(4, 2, 4, 2);
  outer->setSpacing(0);

  _rows_layout = new QVBoxLayout();
  _rows_layout->setContentsMargins(0, 0, 0, 0);
  _rows_layout->setSpacing(1);
  outer->addLayout(_rows_layout);
  outer->addStretch();

  setStyleSheet(
      "DataSetsPanel { background: #1e1e26; border-top: 1px solid #444; }");

  setMaximumHeight(100);
}

void DataSetsPanel::refresh(const OverlayManager* mgr) {
  // Clear existing rows
  for (auto& row : _rows) {
    delete row.prefix_label->parentWidget();
  }
  _rows.clear();

  if (!mgr) return;

  for (const auto& layer : mgr->layers()) {
    auto* row_widget = new QWidget(this);
    auto* hl = new QHBoxLayout(row_widget);
    hl->setContentsMargins(2, 0, 2, 0);
    hl->setSpacing(6);

    RowWidgets rw;
    rw.layer_index = layer.index;

    // Prefix: "#1", "#2", etc.
    rw.prefix_label = new QLabel(QString("#%1").arg(layer.index), row_widget);
    rw.prefix_label->setStyleSheet(
        "color: #000; font: bold 9pt monospace; min-width: 24px;");
    hl->addWidget(rw.prefix_label);

    // Display name (filename or "PJ Data")
    rw.name_label =
        new QLabel(QString::fromStdString(layer.display_name), row_widget);
    rw.name_label->setStyleSheet("color: #000; font: 9pt monospace;");
    rw.name_label->setSizePolicy(QSizePolicy::Expanding,
                                 QSizePolicy::Preferred);
    hl->addWidget(rw.name_label);

    // Time offset
    QString offset_str =
        QString("offset: %1s").arg(layer.time_offset, 0, 'f', 3);
    if (layer.time_offset > 0)
      offset_str = QString("offset: +%1s").arg(layer.time_offset, 0, 'f', 3);
    rw.offset_label = new QLabel(offset_str, row_widget);
    rw.offset_label->setStyleSheet("color: #000; font: 9pt monospace;");
    hl->addWidget(rw.offset_label);

    row_widget->setProperty("layer_index", layer.index);
    _rows_layout->addWidget(row_widget);
    _rows.push_back(rw);
  }
}

void DataSetsPanel::updateOffsets(const OverlayManager* mgr) {
  if (!mgr) return;

  for (auto& row : _rows) {
    double offset = mgr->timeOffset(row.layer_index);
    QString offset_str = QString("offset: %1%2s")
                             .arg(offset >= 0 ? "+" : "")
                             .arg(offset, 0, 'f', 3);
    row.offset_label->setText(offset_str);
  }
}

void DataSetsPanel::contextMenuEvent(QContextMenuEvent* event) {
  // Find which row was right-clicked
  int clicked_layer = -1;
  for (const auto& row : _rows) {
    QWidget* row_widget = row.prefix_label->parentWidget();
    QRect geom = row_widget->geometry();
    // Translate to panel coordinates
    if (geom.contains(event->pos())) {
      clicked_layer = row.layer_index;
      break;
    }
  }

  if (clicked_layer < 0) return;

  QMenu menu(this);

  auto* style_action = menu.addAction("Style All Signals...");
  connect(style_action, &QAction::triggered, this,
          [this, clicked_layer]() { emit styleLayerRequested(clicked_layer); });

  auto* rename_action = menu.addAction("Rename...");
  connect(rename_action, &QAction::triggered, this, [this, clicked_layer]() {
    // Find current name
    for (const auto& row : _rows) {
      if (row.layer_index == clicked_layer) {
        bool ok;
        QString new_name = QInputDialog::getText(
            this, "Rename Layer", "Display name:", QLineEdit::Normal,
            row.name_label->text(), &ok);
        if (ok && !new_name.isEmpty()) {
          row.name_label->setText(new_name);
          emit layerRenamed(clicked_layer, new_name);
        }
        break;
      }
    }
  });

  // Only allow removing overlay layers (not the PJ base layer)
  if (clicked_layer > 1 || (clicked_layer == 1 && _rows.size() > 1)) {
    auto* remove_action = menu.addAction("Remove Overlay");
    connect(remove_action, &QAction::triggered, this, [this, clicked_layer]() {
      emit removeOverlayRequested(clicked_layer);
    });
  }

  menu.addSeparator();
  auto* load_action = menu.addAction("Load Overlay...");
  connect(load_action, &QAction::triggered, this,
          [this]() { emit loadOverlayRequested(); });

  menu.exec(event->globalPos());
}
