#include <gtest/gtest.h>

#include <QApplication>
#include <QSignalSpy>
#include <QTest>

#include "y_axis_panel.h"

static constexpr int W = 60;
static constexpr int H = 500;
static constexpr int AXIS_PAD_LEFT = 4;
static constexpr int AXIS_PAD_RIGHT = 10;

// Helper: create a signal entry at the given band position.
static SignalEntry makeEntry(double center, double height,
                             double bar_x_norm = 1.0) {
  SignalEntry e;
  e.name = "test";
  e.color = QColor(Qt::red);
  e.band_center_norm = center;
  e.band_height_norm = height;
  e.bar_x_norm = bar_x_norm;
  return e;
}

// Pixel X of the Y-axis bar for a given bar_x_norm in a 60-wide widget.
static int barPixelX(double bar_x_norm) {
  return AXIS_PAD_LEFT +
         (int)(bar_x_norm * (W - AXIS_PAD_LEFT - AXIS_PAD_RIGHT));
}

// Pixel Y of the band center for a given entry in a 500-high widget.
static int bandCenterY(const SignalEntry& e) {
  double top = AxisLayout::bandTopY(e, H);
  double bot = AxisLayout::bandBottomY(e, H);
  return (int)((top + bot) / 2.0);
}

class YAxisBarUITest : public ::testing::Test {
 protected:
  void SetUp() override {
    m_bar = new YAxisBarColumn;
    m_bar->resize(W, H);
    m_bar->show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(m_bar));
  }

  void TearDown() override { delete m_bar; }

  YAxisBarColumn* m_bar = nullptr;
};

// Clicking on a bar body selects that signal.
TEST_F(YAxisBarUITest, ClickOnBarSelectsSignal) {
  auto entry = makeEntry(0.5, 1.0);
  m_bar->setSignalEntries({entry});
  QSignalSpy spy(m_bar, &YAxisBarColumn::selectionChanged);

  QPoint pos(barPixelX(1.0), bandCenterY(entry));
  QTest::mouseClick(m_bar, Qt::LeftButton, Qt::NoModifier, pos);

  ASSERT_GE(spy.count(), 1);
  EXPECT_EQ(m_bar->selection().count(0), 1u);
}

// Ctrl+clicking a selected bar deselects it.
TEST_F(YAxisBarUITest, CtrlClickTogglesSelection) {
  auto entry = makeEntry(0.5, 1.0);
  m_bar->setSignalEntries({entry});
  QPoint pos(barPixelX(1.0), bandCenterY(entry));

  // Select first
  QTest::mouseClick(m_bar, Qt::LeftButton, Qt::NoModifier, pos);
  ASSERT_EQ(m_bar->selection().count(0), 1u);

  // Ctrl+click to deselect
  QTest::mouseClick(m_bar, Qt::LeftButton, Qt::ControlModifier, pos);
  EXPECT_EQ(m_bar->selection().count(0), 0u);
}

// Right-clicking on a bar emits contextMenuRequested with the correct index.
TEST_F(YAxisBarUITest, RightClickEmitsContextMenuRequested) {
  auto entry = makeEntry(0.5, 1.0);
  m_bar->setSignalEntries({entry});
  QSignalSpy spy(m_bar, &YAxisBarColumn::contextMenuRequested);

  QPoint pos(barPixelX(1.0), bandCenterY(entry));
  QTest::mouseClick(m_bar, Qt::RightButton, Qt::NoModifier, pos);

  ASSERT_EQ(spy.count(), 1);
  EXPECT_EQ(spy.first().at(0).toInt(), 0);
}

// Dragging a bar body vertically emits bandOffsetChanged.
TEST_F(YAxisBarUITest, DragBarBodyEmitsBandOffsetChanged) {
  auto entry = makeEntry(0.5, 1.0);
  m_bar->setSignalEntries({entry});
  QSignalSpy spy(m_bar, &YAxisBarColumn::bandOffsetChanged);

  int ax = barPixelX(1.0);
  int cy = bandCenterY(entry);
  QPoint start(ax, cy);
  QPoint end(ax, cy + 50);

  QTest::mousePress(m_bar, Qt::LeftButton, Qt::NoModifier, start);
  // Use explicit event: QTest::mouseMove relies on QCursor::setPos which is
  // nondeterministic under xvfb.
  QMouseEvent move(QEvent::MouseMove, end, m_bar->mapToGlobal(end),
                   Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
  QApplication::sendEvent(m_bar, &move);
  QTest::mouseRelease(m_bar, Qt::LeftButton, Qt::NoModifier, end);

  ASSERT_GE(spy.count(), 1);
  double new_center = spy.last().at(1).toDouble();
  EXPECT_GT(new_center, 0.5);
}

// Dragging the top edge of a bar emits bandResized with a smaller height.
TEST_F(YAxisBarUITest, DragTopEdgeEmitsBandResized) {
  auto entry = makeEntry(0.5, 0.5);
  m_bar->setSignalEntries({entry});
  QSignalSpy spy(m_bar, &YAxisBarColumn::bandResized);

  int ax = barPixelX(1.0);
  int top_y = (int)AxisLayout::bandTopY(entry, H);
  QPoint start(ax, top_y);
  QPoint end(ax, top_y + 30);

  QTest::mousePress(m_bar, Qt::LeftButton, Qt::NoModifier, start);
  QMouseEvent move2(QEvent::MouseMove, end, m_bar->mapToGlobal(end),
                    Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
  QApplication::sendEvent(m_bar, &move2);
  QTest::mouseRelease(m_bar, Qt::LeftButton, Qt::NoModifier, end);

  ASSERT_GE(spy.count(), 1);
  double new_height = spy.last().at(2).toDouble();
  EXPECT_LT(new_height, 0.5);
}

// Clicking in empty space (no bar) clears the selection.
TEST_F(YAxisBarUITest, ClickEmptySpaceClearsSelection) {
  auto entry = makeEntry(0.5, 0.3, 1.0);
  m_bar->setSignalEntries({entry});

  // Select the signal first
  QPoint bar_pos(barPixelX(1.0), bandCenterY(entry));
  QTest::mouseClick(m_bar, Qt::LeftButton, Qt::NoModifier, bar_pos);
  ASSERT_EQ(m_bar->selection().size(), 1u);

  // Click far from any bar (top-left corner, well outside bar range)
  QSignalSpy spy(m_bar, &YAxisBarColumn::selectionChanged);
  QPoint empty(2, 2);
  QTest::mouseClick(m_bar, Qt::LeftButton, Qt::NoModifier, empty);

  ASSERT_GE(spy.count(), 1);
  EXPECT_TRUE(m_bar->selection().empty());
}

// Clicking a bar body emits zOrderChanged with the clicked index.
TEST_F(YAxisBarUITest, ClickBarBody_EmitsZOrderChanged) {
  auto entry = makeEntry(0.5, 1.0);
  m_bar->setSignalEntries({entry});
  QSignalSpy spy(m_bar, &YAxisBarColumn::zOrderChanged);

  QPoint pos(barPixelX(1.0), bandCenterY(entry));
  QTest::mouseClick(m_bar, Qt::LeftButton, Qt::NoModifier, pos);

  ASSERT_EQ(spy.count(), 1);
  EXPECT_EQ(spy.first().at(0).toInt(), 0);  // index 0
}

// Clicking stacked bars in sequence brings each one to the top z_order.
// Both signals occupy the same band; clicking the second should give it a
// higher z_order than the first, and clicking back should reverse that.
TEST_F(YAxisBarUITest, StackedBars_ZOrderFollowsLastClick) {
  // Two signals stacked at the same band position and bar_x.
  auto e0 = makeEntry(0.5, 1.0);
  auto e1 = makeEntry(0.5, 1.0);
  e0.z_order = 0;
  e1.z_order = 0;
  m_bar->setSignalEntries({e0, e1});

  QSignalSpy spy(m_bar, &YAxisBarColumn::zOrderChanged);

  QPoint pos(barPixelX(1.0), bandCenterY(e0));

  // First click — one of the two stacked signals gets a bump.
  QTest::mouseClick(m_bar, Qt::LeftButton, Qt::NoModifier, pos);
  ASSERT_EQ(spy.count(), 1);
  int first_idx = spy.first().at(0).toInt();
  int first_z = spy.first().at(1).toInt();
  EXPECT_GT(first_z, 0);

  // Propagate the z_order update back into the bar column (mirrors what
  // SignalViewWidget::onZOrderChanged does in the full application).
  std::vector<SignalEntry> updated = {e0, e1};
  updated[first_idx].z_order = first_z;
  m_bar->setSignalEntries(updated);

  // Second click — another bump must exceed first_z so the new bar is on top.
  QTest::mouseClick(m_bar, Qt::LeftButton, Qt::NoModifier, pos);
  ASSERT_GE(spy.count(), 2);
  int second_z = spy.last().at(1).toInt();
  EXPECT_GT(second_z, first_z);
}

// Clicking a stacked group selects the bar with the highest z_order (the one
// visually on top), not the first by index order.
TEST_F(YAxisBarUITest, StackedBars_ClickSelectsHighestZOrder) {
  auto e0 = makeEntry(0.5, 1.0);
  auto e1 = makeEntry(0.5, 1.0);
  e0.z_order = 0;  // lower index, lower z_order → painted underneath
  e1.z_order = 1;  // higher index, higher z_order → painted on top
  m_bar->setSignalEntries({e0, e1});

  QSignalSpy spy(m_bar, &YAxisBarColumn::zOrderChanged);
  QPoint pos(barPixelX(1.0), bandCenterY(e0));
  QTest::mouseClick(m_bar, Qt::LeftButton, Qt::NoModifier, pos);

  // hitTest must return index 1 (highest z_order), not index 0.
  ASSERT_EQ(spy.count(), 1);
  EXPECT_EQ(spy.first().at(0).toInt(), 1);
}

// Dragging the top edge of a stacked group targets the bar with the highest
// z_order, not the first by index order.
TEST_F(YAxisBarUITest, StackedBars_EdgeGrabSelectsHighestZOrder) {
  auto e0 = makeEntry(0.5, 0.5);
  auto e1 = makeEntry(0.5, 0.5);
  e0.z_order = 0;
  e1.z_order = 1;
  m_bar->setSignalEntries({e0, e1});

  QSignalSpy spy(m_bar, &YAxisBarColumn::bandResized);

  int ax = barPixelX(1.0);
  int top_y = static_cast<int>(AxisLayout::bandTopY(e0, H));
  QPoint start(ax, top_y);
  QPoint end(ax, top_y + 30);

  QTest::mousePress(m_bar, Qt::LeftButton, Qt::NoModifier, start);
  QMouseEvent move(QEvent::MouseMove, end, m_bar->mapToGlobal(end),
                   Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
  QApplication::sendEvent(m_bar, &move);
  QTest::mouseRelease(m_bar, Qt::LeftButton, Qt::NoModifier, end);

  // bandResized must target index 1 (highest z_order).
  ASSERT_GE(spy.count(), 1);
  EXPECT_EQ(spy.last().at(0).toInt(), 1);
}

// ---------------------------------------------------------------------------

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
