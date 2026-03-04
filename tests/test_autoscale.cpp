#include <gtest/gtest.h>

#include <QApplication>
#include <QPushButton>
#include <QSpinBox>
#include <QTest>

#include "signal_view_widget.h"
#include "y_axis_panel.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static QPushButton* findButton(QWidget* root, const QString& text) {
  for (auto* btn : root->findChildren<QPushButton*>()) {
    if (btn->text() == text) return btn;
  }
  return nullptr;
}

// Build a PlotDataMapRef containing a varying signal ("speed", y in [2,8])
// and a constant signal ("flat", y = 5) over t = [0, 10].
static PJ::PlotDataMapRef makeTestData() {
  PJ::PlotDataMapRef data;

  auto& speed = data.addNumeric("speed")->second;
  for (int i = 0; i <= 10; ++i) {
    speed.pushBack({static_cast<double>(i), (i % 2 == 0) ? 2.0 : 8.0});
  }

  auto& flat = data.addNumeric("flat")->second;
  for (int i = 0; i <= 10; ++i) {
    flat.pushBack({static_cast<double>(i), 5.0});
  }

  return data;
}

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------

class AutoScaleTest : public ::testing::Test {
 protected:
  void SetUp() override {
    m_data = makeTestData();
    m_widget = new SignalViewWidget(&m_data);
    m_widget->resize(400, 300);
    m_widget->show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(m_widget));

    // View covers all data.
    m_widget->canvas()->setViewRange(0.0, 10.0);

    // Inject signal entries with sentinel y values so we can detect changes.
    // Give each signal a distinct band position so they are NOT grouped by
    // default; grouped behaviour is tested separately.
    auto& sigs = m_widget->signalEntriesMutable();
    SignalEntry speed_entry;
    speed_entry.name = "speed";
    speed_entry.y_min = -999.0;
    speed_entry.y_max = 999.0;
    speed_entry.band_center_norm = 0.25;
    speed_entry.band_height_norm = 0.30;
    sigs.push_back(speed_entry);

    SignalEntry flat_entry;
    flat_entry.name = "flat";
    flat_entry.y_min = -999.0;
    flat_entry.y_max = 999.0;
    flat_entry.band_center_norm = 0.75;
    flat_entry.band_height_norm = 0.30;
    sigs.push_back(flat_entry);

    m_spin = m_widget->findChild<QSpinBox*>();
    ASSERT_NE(m_spin, nullptr);
    m_btn = findButton(m_widget, "Auto Scale");
    ASSERT_NE(m_btn, nullptr);
  }

  void TearDown() override { delete m_widget; }

  PJ::PlotDataMapRef m_data;
  SignalViewWidget* m_widget = nullptr;
  QSpinBox* m_spin = nullptr;
  QPushButton* m_btn = nullptr;
};

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

// 0% margin → y_min/y_max exactly equal the data extents.
TEST_F(AutoScaleTest, ZeroMargin_ExactFit) {
  m_spin->setValue(0);
  QTest::mouseClick(m_btn, Qt::LeftButton);

  // speed: y in [2, 8], range = 6, margin = 0%  → exact fit.
  const auto& speed = m_widget->signalEntries()[0];
  EXPECT_DOUBLE_EQ(speed.y_min, 2.0);
  EXPECT_DOUBLE_EQ(speed.y_max, 8.0);
}

// 25% margin → each side gets 25% of the data range as padding.
TEST_F(AutoScaleTest, TwentyFivePercentMargin) {
  m_spin->setValue(25);
  QTest::mouseClick(m_btn, Qt::LeftButton);

  // speed: range = 6, margin = 1.5.
  const auto& speed = m_widget->signalEntries()[0];
  EXPECT_NEAR(speed.y_min, 2.0 - 1.5, 1e-9);
  EXPECT_NEAR(speed.y_max, 8.0 + 1.5, 1e-9);
}

// 100% margin → each side gets the full data range as padding.
TEST_F(AutoScaleTest, OneHundredPercentMargin) {
  m_spin->setValue(100);
  QTest::mouseClick(m_btn, Qt::LeftButton);

  // speed: range = 6, margin = 6.
  const auto& speed = m_widget->signalEntries()[0];
  EXPECT_NEAR(speed.y_min, 2.0 - 6.0, 1e-9);
  EXPECT_NEAR(speed.y_max, 8.0 + 6.0, 1e-9);
}

// A constant signal gets a fixed fallback margin of ±1.0 regardless of the
// margin % setting.
TEST_F(AutoScaleTest, ConstantSignal_FallbackMargin) {
  m_spin->setValue(0);
  QTest::mouseClick(m_btn, Qt::LeftButton);

  // flat: all y = 5.0, range ≈ 0 → fallback margin = 1.0.
  const auto& flat = m_widget->signalEntries()[1];
  EXPECT_NEAR(flat.y_min, 4.0, 1e-9);
  EXPECT_NEAR(flat.y_max, 6.0, 1e-9);
}

// When only some signals are selected, Auto Scale only applies to them.
TEST_F(AutoScaleTest, WithSelection_OnlyScalesSelected) {
  m_spin->setValue(0);
  // Select only the first signal (speed, index 0).
  m_widget->yAxisPanel()->setSelection({0});
  QTest::mouseClick(m_btn, Qt::LeftButton);

  // speed should be scaled to its data extents.
  const auto& speed = m_widget->signalEntries()[0];
  EXPECT_DOUBLE_EQ(speed.y_min, 2.0);
  EXPECT_DOUBLE_EQ(speed.y_max, 8.0);

  // flat should be untouched (still has sentinel values).
  const auto& flat = m_widget->signalEntries()[1];
  EXPECT_DOUBLE_EQ(flat.y_min, -999.0);
  EXPECT_DOUBLE_EQ(flat.y_max, 999.0);
}

// When the view is entirely before all data (no step-wise hold point either),
// the signal entries are left unchanged.
TEST_F(AutoScaleTest, NoDataInView_EntryUnchanged) {
  // Data starts at t=0; a view ending before that has no points at all.
  m_widget->canvas()->setViewRange(-200.0, -100.0);
  m_spin->setValue(0);
  QTest::mouseClick(m_btn, Qt::LeftButton);

  const auto& speed = m_widget->signalEntries()[0];
  EXPECT_DOUBLE_EQ(speed.y_min, -999.0);
  EXPECT_DOUBLE_EQ(speed.y_max, 999.0);
}

// Clicking Group with 2 selected signals copies topmost signal's band
// properties to all others in the selection.
TEST_F(AutoScaleTest, GroupSignals_CopiesTopmost) {
  // Give the two signals distinct band positions.
  // speed (index 0): top at 0.25 - 0.15 = 0.10 → topmost.
  // flat  (index 1): top at 0.75 - 0.15 = 0.60.
  auto& sigs = m_widget->signalEntriesMutable();
  sigs[0].band_center_norm = 0.25;
  sigs[0].band_height_norm = 0.30;
  sigs[1].band_center_norm = 0.75;
  sigs[1].band_height_norm = 0.30;

  m_widget->yAxisPanel()->setSelection({0, 1});

  auto* btn_group = findButton(m_widget, "Group");
  ASSERT_NE(btn_group, nullptr);
  QTest::mouseClick(btn_group, Qt::LeftButton);

  // flat should now share speed's band position and height.
  const auto& s0 = m_widget->signalEntries()[0];
  const auto& s1 = m_widget->signalEntries()[1];
  EXPECT_DOUBLE_EQ(s1.band_center_norm, s0.band_center_norm);
  EXPECT_DOUBLE_EQ(s1.band_height_norm, s0.band_height_norm);
}

// Grouped signals (same band geometry) are auto-scaled to their combined data
// extents so both signals share a common Y range.
TEST_F(AutoScaleTest, GroupedSignals_CombinedExtents) {
  m_spin->setValue(0);
  // Place both signals in the same band — this is what the Group button does.
  auto& sigs = m_widget->signalEntriesMutable();
  sigs[0].band_center_norm = 0.5;
  sigs[0].band_height_norm = 0.5;
  sigs[0].bar_x_norm = 1.0;
  sigs[1].band_center_norm = 0.5;
  sigs[1].band_height_norm = 0.5;
  sigs[1].bar_x_norm = 1.0;

  QTest::mouseClick(m_btn, Qt::LeftButton);

  // speed: y in [2, 8]; flat: y = 5. Combined extents: [2, 8].
  // Both signals must share that range.
  const auto& speed = m_widget->signalEntries()[0];
  const auto& flat = m_widget->signalEntries()[1];
  EXPECT_DOUBLE_EQ(speed.y_min, 2.0);
  EXPECT_DOUBLE_EQ(speed.y_max, 8.0);
  EXPECT_DOUBLE_EQ(flat.y_min, 2.0);
  EXPECT_DOUBLE_EQ(flat.y_max, 8.0);
}

// Grouped signals with a margin: combined extents are computed first, then the
// margin is applied once to the combined range — not per signal.
TEST_F(AutoScaleTest, GroupedSignals_SharedMargin) {
  m_spin->setValue(25);
  auto& sigs = m_widget->signalEntriesMutable();
  sigs[0].band_center_norm = 0.5;
  sigs[0].band_height_norm = 0.5;
  sigs[0].bar_x_norm = 1.0;
  sigs[1].band_center_norm = 0.5;
  sigs[1].band_height_norm = 0.5;
  sigs[1].bar_x_norm = 1.0;

  QTest::mouseClick(m_btn, Qt::LeftButton);

  // Combined: y_lo=2, y_hi=8, range=6. Margin = 6 * 0.25 = 1.5.
  const auto& speed = m_widget->signalEntries()[0];
  const auto& flat = m_widget->signalEntries()[1];
  EXPECT_NEAR(speed.y_min, 0.5, 1e-9);
  EXPECT_NEAR(speed.y_max, 9.5, 1e-9);
  EXPECT_NEAR(flat.y_min, 0.5, 1e-9);
  EXPECT_NEAR(flat.y_max, 9.5, 1e-9);
}

// After a bar-body click, SignalViewWidget::onZOrderChanged must renormalize
// all z_order values to [0, n-1] so they never grow unboundedly.
TEST_F(AutoScaleTest, ZOrder_NormalizedAfterClick) {
  // Seed with large non-contiguous values to confirm they get compacted.
  auto& sigs = m_widget->signalEntriesMutable();
  sigs[0].z_order = 100;
  sigs[1].z_order = 200;
  // Sync the panel so the bar column's hitTest sees the signals and
  // onZOrderChanged's normalization applies to the seeded values.
  m_widget->yAxisPanel()->setSignalEntries(m_widget->signalEntries());

  // Click the speed bar (index 0) via the YAxisBarColumn widget.
  auto* bar_col = m_widget->yAxisPanel()->findChild<YAxisBarColumn*>();
  ASSERT_NE(bar_col, nullptr);

  // Compute a click position inside the speed band (band_center_norm = 0.25).
  int bar_h = bar_col->height();
  double top = AxisLayout::bandTopY(sigs[0], bar_h);
  double bot = AxisLayout::bandBottomY(sigs[0], bar_h);
  int cy = static_cast<int>((top + bot) / 2.0);
  // bar_x_norm = 1.0 → pixel x mirrors test_yaxis_bar_ui constants.
  constexpr int AXIS_PAD_LEFT = 4;
  constexpr int AXIS_PAD_RIGHT = 10;
  int ax = AXIS_PAD_LEFT +
           static_cast<int>(
               1.0 * (bar_col->width() - AXIS_PAD_LEFT - AXIS_PAD_RIGHT));
  QTest::mouseClick(bar_col, Qt::LeftButton, Qt::NoModifier, QPoint(ax, cy));

  // All z_orders must now be in [0, n-1].
  const auto& result = m_widget->signalEntries();
  for (const auto& s : result) {
    EXPECT_GE(s.z_order, 0);
    EXPECT_LT(s.z_order, static_cast<int>(result.size()));
  }
  // The clicked signal (index 0) must have the highest z_order.
  EXPECT_GT(result[0].z_order, result[1].z_order);
}

// ---------------------------------------------------------------------------

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
