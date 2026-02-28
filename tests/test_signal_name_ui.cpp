#include <gtest/gtest.h>

#include <QApplication>
#include <QSignalSpy>
#include <QTest>

#include "y_axis_panel.h"

static constexpr int W = 70;
static constexpr int H = 500;
static constexpr int TEXT_ROW_HEIGHT = 15;

// Helper: create a signal entry at the given band position.
static SignalEntry makeEntry(const std::string& name, QColor color,
                             double center, double height) {
  SignalEntry e;
  e.name = name;
  e.color = color;
  e.band_center_norm = center;
  e.band_height_norm = height;
  return e;
}

// Pixel Y of the signal's text row top for a given entry.
static int signalRowY(const std::vector<SignalEntry>& entries, int index) {
  auto offsets = AxisLayout::textRowYOffsets(entries, H, TEXT_ROW_HEIGHT);
  double top = AxisLayout::bandTopY(entries[index], H);
  return (int)(top + offsets[index]) + TEXT_ROW_HEIGHT / 2;
}

class SignalNameUITest : public ::testing::Test {
 protected:
  void SetUp() override {
    m_col = new SignalNameColumn;
    m_col->resize(W, H);
    m_entries = {makeEntry("sig_a", Qt::red, 0.3, 0.2),
                 makeEntry("sig_b", Qt::blue, 0.7, 0.2)};
    m_col->setSignalEntries(m_entries);
    m_col->show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(m_col));
  }

  void TearDown() override { delete m_col; }

  SignalNameColumn* m_col = nullptr;
  std::vector<SignalEntry> m_entries;
};

// Clicking on a signal name row emits clickSelect with that index.
TEST_F(SignalNameUITest, ClickOnSignalSelectsIt) {
  QSignalSpy spy(m_col, &SignalNameColumn::clickSelect);
  QPoint pos(W / 2, signalRowY(m_entries, 0));
  QTest::mouseClick(m_col, Qt::LeftButton, Qt::NoModifier, pos);

  ASSERT_GE(spy.count(), 1);
  EXPECT_EQ(spy.first().at(0).toInt(), 0);
  EXPECT_FALSE(spy.first().at(1).toBool());
}

// Ctrl+clicking a signal emits clickSelect with toggle=true.
TEST_F(SignalNameUITest, CtrlClickTogglesSelection) {
  QSignalSpy spy(m_col, &SignalNameColumn::clickSelect);
  QPoint pos(W / 2, signalRowY(m_entries, 0));
  QTest::mouseClick(m_col, Qt::LeftButton, Qt::ControlModifier, pos);

  ASSERT_GE(spy.count(), 1);
  EXPECT_EQ(spy.first().at(0).toInt(), 0);
  EXPECT_TRUE(spy.first().at(1).toBool());
}

// Double-clicking on a signal name emits editYRangeRequested.
TEST_F(SignalNameUITest, DoubleClickEmitsEditRequest) {
  QSignalSpy spy(m_col, &SignalNameColumn::editYRangeRequested);
  QPoint pos(W / 2, signalRowY(m_entries, 0));
  QTest::mouseDClick(m_col, Qt::LeftButton, Qt::NoModifier, pos);

  ASSERT_GE(spy.count(), 1);
  EXPECT_EQ(spy.first().at(0).toInt(), 0);
}

// Double-clicking on empty space emits addSignalRequested.
TEST_F(SignalNameUITest, DoubleClickEmptyEmitsAddRequest) {
  QSignalSpy spy(m_col, &SignalNameColumn::addSignalRequested);
  // Click at the very bottom, well below any signal row
  QPoint pos(W / 2, H - 5);
  QTest::mouseDClick(m_col, Qt::LeftButton, Qt::NoModifier, pos);

  ASSERT_GE(spy.count(), 1);
}

// Right-clicking on a signal name emits contextMenuRequested with that index.
TEST_F(SignalNameUITest, RightClickEmitsContextMenuRequested) {
  QSignalSpy spy(m_col, &SignalNameColumn::contextMenuRequested);
  QPoint pos(W / 2, signalRowY(m_entries, 0));
  QTest::mouseClick(m_col, Qt::RightButton, Qt::NoModifier, pos);

  ASSERT_EQ(spy.count(), 1);
  EXPECT_EQ(spy.first().at(0).toInt(), 0);
}

// Dragging a signal name vertically emits bandOffsetChanged.
TEST_F(SignalNameUITest, DragSignalEmitsBandOffsetChanged) {
  QSignalSpy spy(m_col, &SignalNameColumn::bandOffsetChanged);
  int row_y = signalRowY(m_entries, 0);
  QPoint start(W / 2, row_y);
  QPoint end(W / 2, row_y + 40);

  QTest::mousePress(m_col, Qt::LeftButton, Qt::NoModifier, start);
  // Send move event explicitly: QTest::mouseMove uses QCursor::setPos which
  // depends on the window manager delivering the event with correct globalPos.
  QMouseEvent move_event(QEvent::MouseMove, end, m_col->mapToGlobal(end),
                         Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
  QApplication::sendEvent(m_col, &move_event);
  QTest::mouseRelease(m_col, Qt::LeftButton, Qt::NoModifier, end);

  ASSERT_GE(spy.count(), 1);
  double new_center = spy.last().at(1).toDouble();
  EXPECT_GT(new_center, 0.3);
}

// A wheel event emits verticalScrollRequested.
TEST_F(SignalNameUITest, WheelEmitsVerticalScroll) {
  QSignalSpy spy(m_col, &SignalNameColumn::verticalScrollRequested);
  QPoint center(W / 2, H / 2);
  QWheelEvent event(center, m_col->mapToGlobal(center), QPoint(0, 0),
                    QPoint(0, 120), Qt::NoButton, Qt::NoModifier,
                    Qt::NoScrollPhase, false);
  QApplication::sendEvent(m_col, &event);

  ASSERT_EQ(spy.count(), 1);
  double delta = spy.first().at(0).toDouble();
  EXPECT_LT(delta, 0.0);  // scroll up = negative delta
}

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
