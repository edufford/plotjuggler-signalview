#pragma once

#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>
#include <functional>
#include <vector>

class OverlayManager;

/// Compact footer panel showing loaded data layers with their
/// prefix numbers, display names, and time offsets.
class DataSetsPanel : public QWidget {
  Q_OBJECT

 public:
  explicit DataSetsPanel(QWidget* parent = nullptr);

  /// Rebuild the panel rows from the current OverlayManager state.
  void refresh(const OverlayManager* mgr);

  /// Update just the time offset labels (called during drag).
  void updateOffsets(const OverlayManager* mgr);

 signals:
  /// User requested to load a new overlay file.
  void loadOverlayRequested();

  /// User requested to remove an overlay layer.
  void removeOverlayRequested(int layer_index);

  /// User requested to style all signals from a layer.
  void styleLayerRequested(int layer_index);

  /// User renamed a layer's display name.
  void layerRenamed(int layer_index, const QString& new_name);

 private:
  void contextMenuEvent(QContextMenuEvent* event) override;

  struct RowWidgets {
    int layer_index = 0;
    QLabel* prefix_label = nullptr;
    QLabel* name_label = nullptr;
    QLabel* offset_label = nullptr;
  };

  QVBoxLayout* _rows_layout = nullptr;
  std::vector<RowWidgets> _rows;
};
