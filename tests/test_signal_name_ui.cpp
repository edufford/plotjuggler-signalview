#include <gtest/gtest.h>

#include <QApplication>
#include <QSignalSpy>
#include <QTest>

#include "y_axis_panel.h"

static constexpr int kW = 70;
static constexpr int kH = 500;
static constexpr int kTextRowHeight = 15;

// Helper: create a signal entry at the given band position.
static SignalEntry makeEntry(const std::string& name, QColor color,
                             double center, double height) {
  SignalEntry e;
  e.name = name;
  e.color = color;
  e.band_center = center;
  e.band_height = height;
  return e;
}

// Pixel Y of the signal's text row top for a given entry.
static int signalRowY(const std::vector<SignalEntry>& entries, int index) {
  auto offsets = AxisLayout::textRowYOffsets(entries, kH, kTextRowHeight);
  double top = AxisLayout::bandTopY(entries[index], kH);
  return (int)(top + offsets[index]) + kTextRowHeight / 2;
}

class SignalNameUITest : public ::testing::Test {
 protected:
  void SetUp() override {
    col_ = new SignalNameColumn;
    col_->resize(kW, kH);
    entries_ = {makeEntry("sig_a", Qt::red, 0.3, 0.2),
                makeEntry("sig_b", Qt::blue, 0.7, 0.2)};
    col_->setSignalEntries(entries_);
    col_->show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(col_));
  }

  void TearDown() override { delete col_; }

  SignalNameColumn* col_ = nullptr;
  std::vector<SignalEntry> entries_;
};

// Clicking on a signal name row emits clickSelect with that index.
TEST_F(SignalNameUITest, ClickOnSignalSelectsIt) {
  QSignalSpy spy(col_, &SignalNameColumn::clickSelect);
  QPoint pos(kW / 2, signalRowY(entries_, 0));
  QTest::mouseClick(col_, Qt::LeftButton, Qt::NoModifier, pos);

  ASSERT_GE(spy.count(), 1);
  EXPECT_EQ(spy.first().at(0).toInt(), 0);
  EXPECT_FALSE(spy.first().at(1).toBool());
}

// Ctrl+clicking a signal emits clickSelect with toggle=true.
TEST_F(SignalNameUITest, CtrlClickTogglesSelection) {
  QSignalSpy spy(col_, &SignalNameColumn::clickSelect);
  QPoint pos(kW / 2, signalRowY(entries_, 0));
  QTest::mouseClick(col_, Qt::LeftButton, Qt::ControlModifier, pos);

  ASSERT_GE(spy.count(), 1);
  EXPECT_EQ(spy.first().at(0).toInt(), 0);
  EXPECT_TRUE(spy.first().at(1).toBool());
}

// Double-clicking on a signal name emits editYRangeRequested.
TEST_F(SignalNameUITest, DoubleClickEmitsEditRequest) {
  QSignalSpy spy(col_, &SignalNameColumn::editYRangeRequested);
  QPoint pos(kW / 2, signalRowY(entries_, 0));
  QTest::mouseDClick(col_, Qt::LeftButton, Qt::NoModifier, pos);

  ASSERT_GE(spy.count(), 1);
  EXPECT_EQ(spy.first().at(0).toInt(), 0);
}

// Double-clicking on empty space emits addSignalRequested.
TEST_F(SignalNameUITest, DoubleClickEmptyEmitsAddRequest) {
  QSignalSpy spy(col_, &SignalNameColumn::addSignalRequested);
  // Click at the very bottom, well below any signal row
  QPoint pos(kW / 2, kH - 5);
  QTest::mouseDClick(col_, Qt::LeftButton, Qt::NoModifier, pos);

  ASSERT_GE(spy.count(), 1);
}

// Dragging a signal name vertically emits bandOffsetChanged.
TEST_F(SignalNameUITest, DragSignalEmitsBandOffsetChanged) {
  QSignalSpy spy(col_, &SignalNameColumn::bandOffsetChanged);
  int row_y = signalRowY(entries_, 0);
  QPoint start(kW / 2, row_y);
  QPoint end(kW / 2, row_y + 40);

  QTest::mousePress(col_, Qt::LeftButton, Qt::NoModifier, start);
  // Send move event explicitly: QTest::mouseMove uses QCursor::setPos which
  // depends on the window manager delivering the event with correct globalPos.
  QMouseEvent move_event(QEvent::MouseMove, end, col_->mapToGlobal(end),
                         Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
  QApplication::sendEvent(col_, &move_event);
  QTest::mouseRelease(col_, Qt::LeftButton, Qt::NoModifier, end);

  ASSERT_GE(spy.count(), 1);
  double new_center = spy.last().at(1).toDouble();
  EXPECT_GT(new_center, 0.3);
}

// A wheel event emits verticalScrollRequested.
TEST_F(SignalNameUITest, WheelEmitsVerticalScroll) {
  QSignalSpy spy(col_, &SignalNameColumn::verticalScrollRequested);
  QPoint center(kW / 2, kH / 2);
  QWheelEvent event(center, col_->mapToGlobal(center), QPoint(0, 0),
                    QPoint(0, 120), Qt::NoButton, Qt::NoModifier,
                    Qt::NoScrollPhase, false);
  QApplication::sendEvent(col_, &event);

  ASSERT_EQ(spy.count(), 1);
  double delta = spy.first().at(0).toDouble();
  EXPECT_LT(delta, 0.0);  // scroll up = negative delta
}

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
