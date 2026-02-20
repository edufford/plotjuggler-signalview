#include "signal_view_widget.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QSplitter>
#include <QToolBar>
#include <QInputDialog>
#include <QStringList>
#include <QDialog>
#include <QDialogButtonBox>
#include <QTableWidget>
#include <QLineEdit>
#include <QDoubleValidator>
#include <QIntValidator>
#include <QHeaderView>
#include <QShortcut>
#include <algorithm>
#include <cmath>

static constexpr const char* kPluginVersion = "0.7.0";

const std::vector<QColor>& SignalViewWidget::signalColors()
{
  static const std::vector<QColor> colors = {
      QColor(0, 180, 255),    // Cyan-blue
      QColor(255, 100, 50),   // Orange-red
      QColor(50, 220, 100),   // Green
      QColor(255, 220, 50),   // Yellow
      QColor(200, 80, 255),   // Purple
      QColor(255, 100, 180),  // Pink
      QColor(100, 255, 220),  // Teal
      QColor(255, 160, 50),   // Amber
  };
  return colors;
}

SignalViewWidget::SignalViewWidget(PJ::PlotDataMapRef* data, QWidget* parent)
    : QWidget(parent), _data(data)
{
  setWindowTitle("Signal View");
  resize(900, 500);

  auto* main_layout = new QVBoxLayout(this);
  main_layout->setContentsMargins(2, 2, 2, 2);

  // Toolbar
  auto* toolbar = new QToolBar(this);
  toolbar->setIconSize(QSize(16, 16));

  auto* btn_add = new QPushButton("Add Signal", this);
  auto* btn_remove = new QPushButton("Remove Signal", this);
  auto* btn_group = new QPushButton("Group", this);
  auto* btn_reset = new QPushButton("Reset Zoom", this);
  auto* btn_reset_cursor = new QPushButton("Reset Cursor", this);
  auto* btn_close = new QPushButton("Close", this);

  auto* snap_label = new QLabel("Snap:", this);
  snap_label->setStyleSheet("color: #ccc; font-size: 9px; padding: 0 2px;");
  _snap_combo = new QComboBox(this);
  _snap_combo->addItem("Off",    0.0);
  _snap_combo->addItem("0.002",  0.002);
  _snap_combo->addItem("0.005",  0.005);
  _snap_combo->addItem("0.01",   0.01);
  _snap_combo->addItem("0.02",   0.02);
  _snap_combo->addItem("0.05",   0.05);
  _snap_combo->addItem("0.10",   0.10);
  _snap_combo->addItem("0.20",   0.20);
  _snap_combo->setCurrentIndex(3);  // default 0.01

  auto* version_label = new QLabel(QString("Signal View v%1").arg(kPluginVersion), this);
  version_label->setStyleSheet("color: #888; font-size: 9px; padding: 0 6px;");

  toolbar->addWidget(version_label);
  toolbar->addSeparator();
  toolbar->addWidget(btn_add);
  toolbar->addWidget(btn_remove);
  toolbar->addWidget(btn_group);
  toolbar->addSeparator();
  toolbar->addWidget(btn_reset);
  toolbar->addWidget(btn_reset_cursor);
  toolbar->addSeparator();
  toolbar->addWidget(snap_label);
  toolbar->addWidget(_snap_combo);
  toolbar->addSeparator();
  toolbar->addWidget(btn_close);

  main_layout->addWidget(toolbar);

  // Main content: Y-axis panel | Plot canvas in a splitter
  _y_axis_panel = new YAxisPanel(nullptr);
  _canvas = new PlotCanvas(nullptr);
  _canvas->setDataSource(_data);

  _main_splitter = new QSplitter(Qt::Horizontal, this);
  _main_splitter->setChildrenCollapsible(false);
  _main_splitter->addWidget(_y_axis_panel);
  _main_splitter->addWidget(_canvas);
  _main_splitter->setStretchFactor(0, 0);  // panel: don't stretch
  _main_splitter->setStretchFactor(1, 1);  // canvas: stretch
  _main_splitter->setSizes({ 173, 700 });
  _main_splitter->setHandleWidth(4);

  main_layout->addWidget(_main_splitter, 1);

  // Connections
  connect(btn_add, &QPushButton::clicked, this, &SignalViewWidget::onAddSignal);
  connect(btn_remove, &QPushButton::clicked, this, &SignalViewWidget::onRemoveSignal);
  connect(btn_group, &QPushButton::clicked, this, &SignalViewWidget::onGroupSignals);
  connect(btn_reset, &QPushButton::clicked, this, &SignalViewWidget::onResetZoom);
  connect(btn_reset_cursor, &QPushButton::clicked, this, [this]() {
    _canvas->setCursorTime(_canvas->viewMinTime());
    onCursorMoved(_canvas->viewMinTime());
  });
  connect(btn_close, &QPushButton::clicked, this, &SignalViewWidget::closeRequested);
  connect(_snap_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &SignalViewWidget::onSnapComboChanged);
  connect(_canvas, &PlotCanvas::cursorMoved, this, &SignalViewWidget::onCursorMoved);
  connect(_y_axis_panel, &YAxisPanel::yRangeChanged, this, &SignalViewWidget::onYRangeChanged);
  connect(_y_axis_panel, &YAxisPanel::bandOffsetChanged, this, &SignalViewWidget::onBandOffsetChanged);
  connect(_y_axis_panel, &YAxisPanel::bandResized, this, &SignalViewWidget::onBandResized);
  connect(_y_axis_panel, &YAxisPanel::barXChanged, this, &SignalViewWidget::onBarXChanged);
  connect(_y_axis_panel, &YAxisPanel::removeSignalRequested, this, &SignalViewWidget::onRemoveSignalByIndex);
  connect(_y_axis_panel, &YAxisPanel::editYRangeRequested, this, &SignalViewWidget::onEditYRange);
  connect(_canvas, &PlotCanvas::canvasResized, this, &SignalViewWidget::onCanvasResized);

  auto* delete_shortcut = new QShortcut(Qt::Key_Delete, this);
  delete_shortcut->setContext(Qt::WindowShortcut);
  connect(delete_shortcut, &QShortcut::activated, this, &SignalViewWidget::onDeleteSelected);
}

void SignalViewWidget::onAddSignal()
{
  if (!_data)
    return;

  QStringList available;
  for (const auto& pair : _data->numeric)
  {
    // Skip signals already added
    bool already_added = false;
    for (const auto& sig : _signals)
    {
      if (sig.name == pair.first)
      {
        already_added = true;
        break;
      }
    }
    if (!already_added)
    {
      available.append(QString::fromStdString(pair.first));
    }
  }

  if (available.isEmpty())
  {
    return;
  }

  available.sort();

  bool ok;
  QString selected = QInputDialog::getItem(
      this, "Add Signal", "Select a signal:", available, 0, false, &ok);

  if (!ok || selected.isEmpty())
    return;

  SignalEntry entry;
  entry.name = selected.toStdString();
  entry.color = signalColors()[_signals.size() % signalColors().size()];

  // Auto-detect Y range from data
  auto it = _data->numeric.find(entry.name);
  if (it != _data->numeric.end() && it->second.size() > 0)
  {
    auto range = it->second.rangeY();
    if (range)
    {
      double margin = (range->max - range->min) * 0.1;
      if (margin < 1e-9)
        margin = 1.0;
      entry.y_min = range->min - margin;
      entry.y_max = range->max + margin;
    }
  }

  _signals.push_back(entry);
  autoAssignBands();
  refreshViews();
}

void SignalViewWidget::onRemoveSignal()
{
  if (_signals.empty())
    return;

  QStringList names;
  for (const auto& sig : _signals)
  {
    names.append(QString::fromStdString(sig.name));
  }

  bool ok;
  QString selected = QInputDialog::getItem(
      this, "Remove Signal", "Select a signal to remove:", names, 0, false, &ok);

  if (!ok || selected.isEmpty())
    return;

  std::string name = selected.toStdString();
  _signals.erase(
      std::remove_if(_signals.begin(), _signals.end(),
                     [&](const SignalEntry& s) { return s.name == name; }),
      _signals.end());

  autoAssignBands();
  refreshViews();
}

void SignalViewWidget::onResetZoom()
{
  _canvas->resetZoom();
}

void SignalViewWidget::onCursorMoved(double time)
{
  _y_axis_panel->updateCursorValues(_data, time);
}

void SignalViewWidget::onYRangeChanged(int index, double y_min, double y_max)
{
  if (index < 0 || index >= (int)_signals.size())
    return;

  const auto& sel = _y_axis_panel->selection();
  if (sel.count(index) && sel.size() > 1)
  {
    // Multi-zoom: apply the same scale factor to all selected signals
    double old_range = _signals[index].y_max - _signals[index].y_min;
    double new_range = y_max - y_min;
    if (old_range > 1e-12)
    {
      double factor = new_range / old_range;
      for (int i : sel)
      {
        double center = (_signals[i].y_min + _signals[i].y_max) * 0.5;
        double half = (_signals[i].y_max - _signals[i].y_min) * 0.5 * factor;
        _signals[i].y_min = center - half;
        _signals[i].y_max = center + half;
      }
    }
  }
  else
  {
    _signals[index].y_min = y_min;
    _signals[index].y_max = y_max;
  }
  refreshViews();
}

void SignalViewWidget::onBandOffsetChanged(int index, double new_center)
{
  if (index < 0 || index >= (int)_signals.size())
    return;

  std::set<int> sel = _y_axis_panel->selection();
  if (sel.count(index) && sel.size() > 1)
  {
    // Multi-drag: move all selected signals together, preserving relative positions.
    // Capture original band_centers when a new drag starts.
    bool need_init = (_multi_drag_index != index || _multi_drag_origins.empty());
    if (!need_init)
    {
      for (int i : sel)
        if (_multi_drag_origins.find(i) == _multi_drag_origins.end())
        { need_init = true; break; }
    }
    if (need_init)
    {
      _multi_drag_index = index;
      _multi_drag_origins.clear();
      _multi_drag_bar_x_origins.clear();
      for (int i : sel)
      {
        _multi_drag_origins[i] = _signals[i].band_center;
        _multi_drag_bar_x_origins[i] = _signals[i].bar_x;
      }
    }

    // Compute snapped delta from the dragged signal
    double half_h = _signals[index].band_height * 0.5;
    double new_top = snapValue(new_center - half_h);
    double snapped_center = new_top + half_h;
    double delta = snapped_center - _multi_drag_origins[index];

    // Clamp delta so no selected signal leaves [0, 1]
    for (auto& [i, origin] : _multi_drag_origins)
    {
      delta = std::max(delta, -origin);
      delta = std::min(delta, 1.0 - origin);
    }
    for (auto& [i, origin] : _multi_drag_origins)
      _signals[i].band_center = origin + delta;
  }
  else
  {
    _multi_drag_origins.clear();
    _multi_drag_bar_x_origins.clear();
    _multi_drag_index = -1;

    double half_h = _signals[index].band_height * 0.5;
    double new_top = snapValue(new_center - half_h);
    _signals[index].band_center = new_top + half_h;
  }

  _canvas->setSignalEntries(_signals);
  _y_axis_panel->setSignalEntries(_signals);
}

void SignalViewWidget::onBandResized(int index, double new_center, double new_height)
{
  if (index < 0 || index >= (int)_signals.size())
    return;

  double new_top = new_center - new_height * 0.5;
  double new_bottom = new_center + new_height * 0.5;

  // Only snap the edge that's actually moving, leave the fixed edge alone
  double cur_top = _signals[index].band_center - _signals[index].band_height * 0.5;
  double cur_bottom = _signals[index].band_center + _signals[index].band_height * 0.5;

  bool top_moving = std::abs(new_top - cur_top) > 1e-6;
  bool bottom_moving = std::abs(new_bottom - cur_bottom) > 1e-6;

  if (top_moving)
    new_top = snapValue(new_top);
  if (bottom_moving)
    new_bottom = snapValue(new_bottom);

  if (new_bottom - new_top < 0.02)
    return;

  const auto& sel = _y_axis_panel->selection();
  if (sel.count(index) && sel.size() > 1)
  {
    // Multi-resize: apply the same height delta to all selected signals.
    // The moving edge shifts by delta; the fixed edge stays put.
    double height_delta = (new_bottom - new_top) - _signals[index].band_height;

    for (int i : sel)
    {
      double i_top = _signals[i].band_center - _signals[i].band_height * 0.5;
      double i_bottom = _signals[i].band_center + _signals[i].band_height * 0.5;

      if (top_moving)
        i_top -= height_delta;  // top edge moves up when growing
      else
        i_bottom += height_delta;  // bottom edge moves down when growing

      if (i_bottom - i_top < 0.02)
        continue;
      _signals[i].band_center = (i_top + i_bottom) * 0.5;
      _signals[i].band_height = i_bottom - i_top;
    }
  }
  else
  {
    _signals[index].band_center = (new_top + new_bottom) * 0.5;
    _signals[index].band_height = new_bottom - new_top;
  }

  _canvas->setSignalEntries(_signals);
  _y_axis_panel->setSignalEntries(_signals);
}

void SignalViewWidget::onBarXChanged(int index, double new_bar_x)
{
  if (index < 0 || index >= (int)_signals.size())
    return;

  const auto& sel = _y_axis_panel->selection();
  if (sel.count(index) && sel.size() > 1 && !_multi_drag_bar_x_origins.empty())
  {
    // Multi-drag: apply the same horizontal delta to all selected signals
    double snapped = snapValue(new_bar_x);
    double delta = snapped - _multi_drag_bar_x_origins[index];

    // Clamp delta so no selected signal leaves [0, 1]
    for (auto& [i, origin] : _multi_drag_bar_x_origins)
    {
      delta = std::max(delta, -origin);
      delta = std::min(delta, 1.0 - origin);
    }
    for (auto& [i, origin] : _multi_drag_bar_x_origins)
      _signals[i].bar_x = origin + delta;
  }
  else
  {
    _signals[index].bar_x = snapValue(new_bar_x);
  }
  _canvas->setSignalEntries(_signals);
  _y_axis_panel->setSignalEntries(_signals);
}

void SignalViewWidget::onCanvasResized()
{
  _y_axis_panel->setCanvasHeight(_canvas->height());
}

void SignalViewWidget::onSnapComboChanged(int combo_index)
{
  _snap_amount = _snap_combo->itemData(combo_index).toDouble();
  _y_axis_panel->setSnapAmount(_snap_amount);
}

double SignalViewWidget::snapValue(double val) const
{
  if (_snap_amount <= 0.0)
    return val;
  return std::round(val / _snap_amount) * _snap_amount;
}

void SignalViewWidget::setSnapAmount(double amount)
{
  _snap_amount = amount;
  _y_axis_panel->setSnapAmount(amount);
  // Find matching combo index
  for (int i = 0; i < _snap_combo->count(); i++)
  {
    if (std::abs(_snap_combo->itemData(i).toDouble() - amount) < 1e-9)
    {
      _snap_combo->setCurrentIndex(i);
      return;
    }
  }
  _snap_combo->setCurrentIndex(0);  // fallback to Off
}

void SignalViewWidget::setSnapIndex(int index)
{
  if (index >= 0 && index < _snap_combo->count())
    _snap_combo->setCurrentIndex(index);
}

void SignalViewWidget::onRemoveSignalByIndex(int index)
{
  if (index < 0 || index >= (int)_signals.size())
    return;
  _signals.erase(_signals.begin() + index);
  autoAssignBands();
  refreshViews();
}

void SignalViewWidget::refreshViews()
{
  _canvas->setSignalEntries(_signals);
  _y_axis_panel->setSignalEntries(_signals);
  _y_axis_panel->updateCursorValues(_data, _canvas->cursorTime());
}

void SignalViewWidget::onEditYRange(int clicked_index)
{
  if (clicked_index < 0 || clicked_index >= (int)_signals.size())
    return;

  // Build the set of signal indices to show: selection + clicked index
  std::set<int> sel = _y_axis_panel->selection();
  sel.insert(clicked_index);

  // Map from table row to signal index
  std::vector<int> row_to_idx(sel.begin(), sel.end());

  QDialog dlg(this);
  dlg.setWindowTitle("Edit Y Range");
  auto* layout = new QVBoxLayout(&dlg);

  auto* table = new QTableWidget((int)row_to_idx.size(), 4, &dlg);
  table->setHorizontalHeaderLabels({"Signal", "Y Min", "Y Max", "Divisions"});
  table->horizontalHeader()->setStretchLastSection(true);
  table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
  table->setSelectionBehavior(QAbstractItemView::SelectRows);
  table->setSelectionMode(QAbstractItemView::ExtendedSelection);
  table->verticalHeader()->setVisible(false);

  // Custom selection model for columns 1/2 (line edit cells):
  // - If the clicked row is already selected, preserve the full selection.
  // - If the clicked row is NOT selected, clear and select just that row.
  class ColumnGuardSelectionModel : public QItemSelectionModel
  {
  public:
    using QItemSelectionModel::QItemSelectionModel;
    void select(const QModelIndex& index, QItemSelectionModel::SelectionFlags command) override
    {
      if (index.isValid() && index.column() >= 1 && index.column() <= 3)
      {
        if (isRowSelected(index.row(), index.parent()))
          return;  // row already selected — preserve multi-selection
        QItemSelectionModel::select(index,
            QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        return;
      }
      QItemSelectionModel::select(index, command);
    }
    void select(const QItemSelection& selection, QItemSelectionModel::SelectionFlags command) override
    {
      QItemSelectionModel::select(selection, command);
    }
    void setCurrentIndex(const QModelIndex& index, QItemSelectionModel::SelectionFlags command) override
    {
      if (index.isValid() && index.column() >= 1 && index.column() <= 3)
      {
        if (isRowSelected(index.row(), index.parent()))
        {
          QItemSelectionModel::setCurrentIndex(index, QItemSelectionModel::NoUpdate);
          return;
        }
        QItemSelectionModel::setCurrentIndex(index,
            QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        return;
      }
      QItemSelectionModel::setCurrentIndex(index, command);
    }
  };
  table->setSelectionModel(new ColumnGuardSelectionModel(table->model(), table));

  // Store line edit pointers for reading results
  std::vector<QLineEdit*> min_edits, max_edits, div_edits;

  for (int row = 0; row < (int)row_to_idx.size(); row++)
  {
    int sig_idx = row_to_idx[row];
    const auto& sig = _signals[sig_idx];

    // Signal name (read-only)
    auto* name_item = new QTableWidgetItem(QString::fromStdString(sig.name));
    name_item->setFlags(name_item->flags() & ~Qt::ItemIsEditable);
    name_item->setForeground(sig.color);
    table->setItem(row, 0, name_item);

    // Y Min line edit
    auto* min_edit = new QLineEdit(&dlg);
    min_edit->setValidator(new QDoubleValidator(&dlg));
    min_edit->setText(QString::number(sig.y_min, 'g', 6));
    table->setCellWidget(row, 1, min_edit);
    min_edits.push_back(min_edit);

    // Y Max line edit
    auto* max_edit = new QLineEdit(&dlg);
    max_edit->setValidator(new QDoubleValidator(&dlg));
    max_edit->setText(QString::number(sig.y_max, 'g', 6));
    table->setCellWidget(row, 2, max_edit);
    max_edits.push_back(max_edit);

    // Divisions line edit (0 = auto)
    auto* div_edit = new QLineEdit(&dlg);
    div_edit->setValidator(new QIntValidator(0, SignalEntry::kMaxDivisions, &dlg));
    div_edit->setText(QString::number(sig.divisions));
    table->setCellWidget(row, 3, div_edit);
    div_edits.push_back(div_edit);
  }

  // Select all rows initially
  table->selectAll();

  // When a line edit value changes, apply to all selected rows in the same column
  auto editForCol = [&](int row, int col) -> QLineEdit* {
    if (col == 1) return min_edits[row];
    if (col == 2) return max_edits[row];
    return div_edits[row];
  };
  auto propagate = [&](int source_row, int col) {
    QString text = editForCol(source_row, col)->text();
    auto selected_rows = table->selectionModel()->selectedRows();
    for (const auto& mi : selected_rows)
    {
      int r = mi.row();
      if (r == source_row)
        continue;
      QLineEdit* target = editForCol(r, col);
      target->blockSignals(true);
      target->setText(text);
      target->blockSignals(false);
    }
  };

  for (int row = 0; row < (int)row_to_idx.size(); row++)
  {
    connect(min_edits[row], &QLineEdit::textEdited,
            &dlg, [&propagate, row]() { propagate(row, 1); });
    connect(max_edits[row], &QLineEdit::textEdited,
            &dlg, [&propagate, row]() { propagate(row, 2); });
    connect(div_edits[row], &QLineEdit::textEdited,
            &dlg, [&propagate, row]() { propagate(row, 3); });
  }

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
  connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

  layout->addWidget(table);
  layout->addWidget(buttons);
  dlg.resize(480, 50 + 30 * (int)row_to_idx.size() + 60);

  if (dlg.exec() != QDialog::Accepted)
    return;

  // Apply changes
  for (int row = 0; row < (int)row_to_idx.size(); row++)
  {
    int sig_idx = row_to_idx[row];
    bool ok_min = false, ok_max = false;
    double new_min = min_edits[row]->text().toDouble(&ok_min);
    double new_max = max_edits[row]->text().toDouble(&ok_max);
    if (ok_min && ok_max && new_max > new_min)
    {
      _signals[sig_idx].y_min = new_min;
      _signals[sig_idx].y_max = new_max;
    }
    bool ok_div = false;
    int new_div = div_edits[row]->text().toInt(&ok_div);
    if (ok_div && new_div >= 0)
      _signals[sig_idx].divisions = new_div;
  }
  refreshViews();
}

void SignalViewWidget::onDeleteSelected()
{
  const auto& sel = _y_axis_panel->selection();
  if (sel.empty())
    return;

  // Remove from highest index to lowest so indices stay valid
  std::vector<int> indices(sel.begin(), sel.end());
  std::sort(indices.rbegin(), indices.rend());
  for (int i : indices)
  {
    if (i >= 0 && i < (int)_signals.size())
      _signals.erase(_signals.begin() + i);
  }

  _y_axis_panel->clearSelection();
  refreshViews();
}

void SignalViewWidget::onGroupSignals()
{
  const auto& sel = _y_axis_panel->selection();
  if (sel.size() < 2)
    return;

  // Find the topmost selected signal (lowest band_top = band_center - band_height/2)
  int topmost = -1;
  double topmost_top = 2.0;  // above any valid position
  for (int i : sel)
  {
    double top = _signals[i].band_center - _signals[i].band_height * 0.5;
    if (top < topmost_top)
    {
      topmost_top = top;
      topmost = i;
    }
  }

  // Copy the topmost signal's band position, height, and Y range to all selected
  for (int i : sel)
  {
    if (i == topmost)
      continue;
    _signals[i].band_center = _signals[topmost].band_center;
    _signals[i].band_height = _signals[topmost].band_height;
    _signals[i].y_min = _signals[topmost].y_min;
    _signals[i].y_max = _signals[topmost].y_max;
    _signals[i].bar_x = _signals[topmost].bar_x;
    _signals[i].divisions = _signals[topmost].divisions;
  }
  refreshViews();
}

void SignalViewWidget::autoAssignBands()
{
  int n = (int)_signals.size();
  if (n == 0)
    return;

  if (n == 1)
  {
    double top = snapValue(0.0);
    double bottom = snapValue(1.0);
    if (bottom - top < 0.02)
    {
      top = 0.0;
      bottom = 1.0;
    }
    _signals[0].band_center = (top + bottom) * 0.5;
    _signals[0].band_height = bottom - top;
  }
  else
  {
    double band_h = 1.0 / n;
    for (int i = 0; i < n; i++)
    {
      double top = snapValue(i * band_h);
      double bottom = snapValue((i + 1) * band_h);
      if (bottom - top < 0.02)
      {
        top = i * band_h;
        bottom = (i + 1) * band_h;
      }
      _signals[i].band_center = (top + bottom) * 0.5;
      _signals[i].band_height = bottom - top;
    }
  }
}
