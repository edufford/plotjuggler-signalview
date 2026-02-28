#include <gtest/gtest.h>

#include <QApplication>
#include <QPushButton>
#include <QSpinBox>
#include <QTest>

#include "signal_view_widget.h"

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
    auto& sigs = m_widget->signalEntriesMutable();
    SignalEntry speed_entry;
    speed_entry.name = "speed";
    speed_entry.y_min = -999.0;
    speed_entry.y_max = 999.0;
    sigs.push_back(speed_entry);

    SignalEntry flat_entry;
    flat_entry.name = "flat";
    flat_entry.y_min = -999.0;
    flat_entry.y_max = 999.0;
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

// ---------------------------------------------------------------------------

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
