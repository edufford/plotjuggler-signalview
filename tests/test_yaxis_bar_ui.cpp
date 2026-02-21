#include <gtest/gtest.h>

#include <QApplication>
#include <QSignalSpy>
#include <QTest>

#include "y_axis_panel.h"

static constexpr int kW = 60;
static constexpr int kH = 500;
static constexpr int kAxisPadLeft = 4;
static constexpr int kAxisPadRight = 10;

// Helper: create a signal entry at the given band position.
static SignalEntry makeEntry(double center, double height, double bar_x = 1.0) {
  SignalEntry e;
  e.name = "test";
  e.color = QColor(Qt::red);
  e.band_center = center;
  e.band_height = height;
  e.bar_x = bar_x;
  return e;
}

// Pixel X of the Y-axis bar for a given bar_x in a 60-wide widget.
static int barPixelX(double bar_x) {
  return kAxisPadLeft + (int)(bar_x * (kW - kAxisPadLeft - kAxisPadRight));
}

// Pixel Y of the band center for a given entry in a 500-high widget.
static int bandCenterY(const SignalEntry& e) {
  double top = AxisLayout::bandTopY(e, kH);
  double bot = AxisLayout::bandBottomY(e, kH);
  return (int)((top + bot) / 2.0);
}

class YAxisBarUITest : public ::testing::Test {
 protected:
  void SetUp() override {
    bar_ = new YAxisBarColumn;
    bar_->resize(kW, kH);
    bar_->show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(bar_));
  }

  void TearDown() override { delete bar_; }

  YAxisBarColumn* bar_ = nullptr;
};

// Clicking on a bar body selects that signal.
TEST_F(YAxisBarUITest, ClickOnBarSelectsSignal) {
  auto entry = makeEntry(0.5, 1.0);
  bar_->setSignalEntries({entry});
  QSignalSpy spy(bar_, &YAxisBarColumn::selectionChanged);

  QPoint pos(barPixelX(1.0), bandCenterY(entry));
  QTest::mouseClick(bar_, Qt::LeftButton, Qt::NoModifier, pos);

  ASSERT_GE(spy.count(), 1);
  EXPECT_EQ(bar_->selection().count(0), 1u);
}

// Ctrl+clicking a selected bar deselects it.
TEST_F(YAxisBarUITest, CtrlClickTogglesSelection) {
  auto entry = makeEntry(0.5, 1.0);
  bar_->setSignalEntries({entry});
  QPoint pos(barPixelX(1.0), bandCenterY(entry));

  // Select first
  QTest::mouseClick(bar_, Qt::LeftButton, Qt::NoModifier, pos);
  ASSERT_EQ(bar_->selection().count(0), 1u);

  // Ctrl+click to deselect
  QTest::mouseClick(bar_, Qt::LeftButton, Qt::ControlModifier, pos);
  EXPECT_EQ(bar_->selection().count(0), 0u);
}

// Right-clicking on a bar emits removeSignalRequested.
TEST_F(YAxisBarUITest, RightClickEmitsRemoveRequest) {
  auto entry = makeEntry(0.5, 1.0);
  bar_->setSignalEntries({entry});
  QSignalSpy spy(bar_, &YAxisBarColumn::removeSignalRequested);

  QPoint pos(barPixelX(1.0), bandCenterY(entry));
  QTest::mouseClick(bar_, Qt::RightButton, Qt::NoModifier, pos);

  ASSERT_EQ(spy.count(), 1);
  EXPECT_EQ(spy.first().at(0).toInt(), 0);
}

// Dragging a bar body vertically emits bandOffsetChanged.
TEST_F(YAxisBarUITest, DragBarBodyEmitsBandOffsetChanged) {
  auto entry = makeEntry(0.5, 1.0);
  bar_->setSignalEntries({entry});
  QSignalSpy spy(bar_, &YAxisBarColumn::bandOffsetChanged);

  int ax = barPixelX(1.0);
  int cy = bandCenterY(entry);
  QPoint start(ax, cy);
  QPoint end(ax, cy + 50);

  QTest::mousePress(bar_, Qt::LeftButton, Qt::NoModifier, start);
  QTest::mouseMove(bar_, end);
  QTest::mouseRelease(bar_, Qt::LeftButton, Qt::NoModifier, end);

  ASSERT_GE(spy.count(), 1);
  double new_center = spy.last().at(1).toDouble();
  EXPECT_GT(new_center, 0.5);
}

// Dragging the top edge of a bar emits bandResized with a smaller height.
TEST_F(YAxisBarUITest, DragTopEdgeEmitsBandResized) {
  auto entry = makeEntry(0.5, 0.5);
  bar_->setSignalEntries({entry});
  QSignalSpy spy(bar_, &YAxisBarColumn::bandResized);

  int ax = barPixelX(1.0);
  int top_y = (int)AxisLayout::bandTopY(entry, kH);
  QPoint start(ax, top_y);
  QPoint end(ax, top_y + 30);

  QTest::mousePress(bar_, Qt::LeftButton, Qt::NoModifier, start);
  QTest::mouseMove(bar_, end);
  QTest::mouseRelease(bar_, Qt::LeftButton, Qt::NoModifier, end);

  ASSERT_GE(spy.count(), 1);
  double new_height = spy.last().at(2).toDouble();
  EXPECT_LT(new_height, 0.5);
}

// Clicking in empty space (no bar) clears the selection.
TEST_F(YAxisBarUITest, ClickEmptySpaceClearsSelection) {
  auto entry = makeEntry(0.5, 0.3, 1.0);
  bar_->setSignalEntries({entry});

  // Select the signal first
  QPoint bar_pos(barPixelX(1.0), bandCenterY(entry));
  QTest::mouseClick(bar_, Qt::LeftButton, Qt::NoModifier, bar_pos);
  ASSERT_EQ(bar_->selection().size(), 1u);

  // Click far from any bar (top-left corner, well outside bar range)
  QSignalSpy spy(bar_, &YAxisBarColumn::selectionChanged);
  QPoint empty(2, 2);
  QTest::mouseClick(bar_, Qt::LeftButton, Qt::NoModifier, empty);

  ASSERT_GE(spy.count(), 1);
  EXPECT_TRUE(bar_->selection().empty());
}

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
