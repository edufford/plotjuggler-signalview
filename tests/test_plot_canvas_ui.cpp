#include <gtest/gtest.h>

#include <QApplication>
#include <QSignalSpy>
#include <QTest>

#include "plot_canvas.h"

static constexpr int W = 400;
static constexpr int H = 300;

// Pixel X for a given time value in a 400-wide widget with view range [0, 10].
static int timeToPixelX(double t) {
  double plot_w = W - PlotCanvas::MARGIN_LEFT - PlotCanvas::MARGIN_RIGHT;
  return PlotCanvas::MARGIN_LEFT + (int)(t / 10.0 * plot_w);
}

class PlotCanvasUITest : public ::testing::Test {
 protected:
  void SetUp() override {
    m_canvas = new PlotCanvas;
    m_canvas->resize(W, H);
    m_canvas->setViewRange(0.0, 10.0);
    m_canvas->show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(m_canvas));
  }

  void TearDown() override { delete m_canvas; }

  PlotCanvas* m_canvas = nullptr;
};

// Double-clicking anywhere in the canvas jumps the cursor to that time.
TEST_F(PlotCanvasUITest, DoubleClickSetsCursorTime) {
  QSignalSpy spy(m_canvas, &PlotCanvas::cursorMoved);
  int click_x = timeToPixelX(5.0);
  int click_y = H / 2;
  QTest::mouseDClick(m_canvas, Qt::LeftButton, Qt::NoModifier,
                     QPoint(click_x, click_y));
  ASSERT_GE(spy.count(), 1);
  double t = spy.last().at(0).toDouble();
  EXPECT_NEAR(t, 5.0, 0.5);
}

// A single left-click far from the cursor does nothing (no cursorMoved signal).
TEST_F(PlotCanvasUITest, SingleClickFarFromCursorDoesNothing) {
  // Cursor starts at 0.0; clicking at 5.0 is well outside the grab margin.
  QSignalSpy spy(m_canvas, &PlotCanvas::cursorMoved);
  QTest::mouseClick(m_canvas, Qt::LeftButton, Qt::NoModifier,
                    QPoint(timeToPixelX(5.0), H / 2));
  EXPECT_EQ(spy.count(), 0);
}

// Dragging after pressing within the grab margin updates cursor time.
TEST_F(PlotCanvasUITest, CursorDragUpdatesTime) {
  // Position cursor at 3.0 so the press lands within the grab margin.
  m_canvas->setCursorTime(3.0);

  QSignalSpy spy(m_canvas, &PlotCanvas::cursorMoved);
  int y = H / 2;
  QPoint start(timeToPixelX(3.0), y);
  QPoint end(timeToPixelX(7.0), y);

  QTest::mousePress(m_canvas, Qt::LeftButton, Qt::NoModifier, start);
  // Use explicit event: QTest::mouseMove relies on QCursor::setPos which is
  // nondeterministic under xvfb.
  QMouseEvent move(QEvent::MouseMove, end, m_canvas->mapToGlobal(end),
                   Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
  QApplication::sendEvent(m_canvas, &move);
  QTest::mouseRelease(m_canvas, Qt::LeftButton, Qt::NoModifier, end);

  ASSERT_GE(spy.count(), 1);
  double t_last = spy.last().at(0).toDouble();
  EXPECT_NEAR(t_last, 7.0, 0.5);
}

// Drag is relative: the cursor starts from its pre-drag position, not the
// click position.
TEST_F(PlotCanvasUITest, CursorDragIsRelative) {
  // Position cursor at 3.0. Press 2px to the right of cursor and drag 4s
  // worth of pixels to the right. Cursor should end up at ~7.0.
  m_canvas->setCursorTime(3.0);

  double plot_w = W - PlotCanvas::MARGIN_LEFT - PlotCanvas::MARGIN_RIGHT;
  int px_per_second = (int)(plot_w / 10.0);
  int cursor_px = timeToPixelX(3.0);
  int press_px = cursor_px + 2;  // 2px right of cursor, within grab margin
  int release_px = press_px + 4 * px_per_second;

  QSignalSpy spy(m_canvas, &PlotCanvas::cursorMoved);
  int y = H / 2;
  QTest::mousePress(m_canvas, Qt::LeftButton, Qt::NoModifier,
                    QPoint(press_px, y));
  QMouseEvent move(QEvent::MouseMove, QPoint(release_px, y),
                   m_canvas->mapToGlobal(QPoint(release_px, y)), Qt::LeftButton,
                   Qt::LeftButton, Qt::NoModifier);
  QApplication::sendEvent(m_canvas, &move);
  QTest::mouseRelease(m_canvas, Qt::LeftButton, Qt::NoModifier,
                      QPoint(release_px, y));

  ASSERT_GE(spy.count(), 1);
  double t = spy.last().at(0).toDouble();
  EXPECT_NEAR(t, 7.0, 0.5);
}

// Right arrow key moves the cursor right by one pixel's worth of time.
TEST_F(PlotCanvasUITest, ArrowKeyMovesRight) {
  m_canvas->setCursorTime(5.0);
  m_canvas->setFocus();

  QSignalSpy spy(m_canvas, &PlotCanvas::cursorMoved);
  QTest::keyClick(m_canvas, Qt::Key_Right);

  ASSERT_EQ(spy.count(), 1);
  double t = spy.first().at(0).toDouble();
  double plot_w = W - PlotCanvas::MARGIN_LEFT - PlotCanvas::MARGIN_RIGHT;
  double expected_dt = 10.0 / plot_w;
  EXPECT_NEAR(t, 5.0 + expected_dt, 1e-6);
}

// Left arrow key moves the cursor left by one pixel's worth of time.
TEST_F(PlotCanvasUITest, ArrowKeyMovesLeft) {
  m_canvas->setCursorTime(5.0);
  m_canvas->setFocus();

  QSignalSpy spy(m_canvas, &PlotCanvas::cursorMoved);
  QTest::keyClick(m_canvas, Qt::Key_Left);

  ASSERT_EQ(spy.count(), 1);
  double t = spy.first().at(0).toDouble();
  double plot_w = W - PlotCanvas::MARGIN_LEFT - PlotCanvas::MARGIN_RIGHT;
  double expected_dt = 10.0 / plot_w;
  EXPECT_NEAR(t, 5.0 - expected_dt, 1e-6);
}

// Arrow keys clamp to view range and do not overshoot.
TEST_F(PlotCanvasUITest, ArrowKeyClampedAtViewBounds) {
  // At min bound: left arrow should not move below 0.
  m_canvas->setCursorTime(0.0);
  m_canvas->setFocus();
  QSignalSpy spy_left(m_canvas, &PlotCanvas::cursorMoved);
  QTest::keyClick(m_canvas, Qt::Key_Left);
  ASSERT_EQ(spy_left.count(), 1);
  EXPECT_DOUBLE_EQ(spy_left.first().at(0).toDouble(), 0.0);

  // At max bound: right arrow should not move above 10.
  m_canvas->setCursorTime(10.0);
  QSignalSpy spy_right(m_canvas, &PlotCanvas::cursorMoved);
  QTest::keyClick(m_canvas, Qt::Key_Right);
  ASSERT_EQ(spy_right.count(), 1);
  EXPECT_DOUBLE_EQ(spy_right.first().at(0).toDouble(), 10.0);
}

// Right-click drag pans the view range horizontally.
TEST_F(PlotCanvasUITest, RightClickPanShiftsViewRange) {
  QSignalSpy spy(m_canvas, &PlotCanvas::viewRangeChanged);
  int y = H / 2;
  QPoint start(200, y);
  QPoint end(250, y);

  QTest::mousePress(m_canvas, Qt::RightButton, Qt::NoModifier, start);
  QMouseEvent move(QEvent::MouseMove, end, m_canvas->mapToGlobal(end),
                   Qt::RightButton, Qt::RightButton, Qt::NoModifier);
  QApplication::sendEvent(m_canvas, &move);
  QTest::mouseRelease(m_canvas, Qt::RightButton, Qt::NoModifier, end);

  ASSERT_GE(spy.count(), 1);
  double new_min = spy.last().at(0).toDouble();
  double new_max = spy.last().at(1).toDouble();
  // Dragging right pans the view left (earlier times)
  EXPECT_LT(new_min, 0.0);
  EXPECT_LT(new_max, 10.0);
}

// In zoom mode, a left-drag rubber band narrows the view range.
TEST_F(PlotCanvasUITest, ZoomRubberBandSetsViewRange) {
  m_canvas->setZoomMode(true);
  QSignalSpy spy(m_canvas, &PlotCanvas::viewRangeChanged);
  int y = H / 2;
  QPoint start(timeToPixelX(2.0), y);
  QPoint end(timeToPixelX(8.0), y);

  QTest::mousePress(m_canvas, Qt::LeftButton, Qt::NoModifier, start);
  QMouseEvent move(QEvent::MouseMove, end, m_canvas->mapToGlobal(end),
                   Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
  QApplication::sendEvent(m_canvas, &move);
  QTest::mouseRelease(m_canvas, Qt::LeftButton, Qt::NoModifier, end);

  ASSERT_GE(spy.count(), 1);
  double new_min = spy.last().at(0).toDouble();
  double new_max = spy.last().at(1).toDouble();
  EXPECT_NEAR(new_min, 2.0, 0.5);
  EXPECT_NEAR(new_max, 8.0, 0.5);
}

// In normal mode, a wheel event emits verticalScrollRequested.
TEST_F(PlotCanvasUITest, WheelInNormalModeEmitsVerticalScroll) {
  QSignalSpy spy(m_canvas, &PlotCanvas::verticalScrollRequested);
  QPoint center(W / 2, H / 2);
  QWheelEvent event(center, m_canvas->mapToGlobal(center), QPoint(0, 0),
                    QPoint(0, 120), Qt::NoButton, Qt::NoModifier,
                    Qt::NoScrollPhase, false);
  QApplication::sendEvent(m_canvas, &event);
  ASSERT_EQ(spy.count(), 1);
  double delta = spy.first().at(0).toDouble();
  EXPECT_LT(delta, 0.0);  // scroll up = negative delta
}

// In zoom mode, a wheel event changes the view range (zoom).
TEST_F(PlotCanvasUITest, WheelInZoomModeChangesViewRange) {
  m_canvas->setZoomMode(true);
  QSignalSpy spy(m_canvas, &PlotCanvas::viewRangeChanged);
  QPoint center(W / 2, H / 2);
  QWheelEvent event(center, m_canvas->mapToGlobal(center), QPoint(0, 0),
                    QPoint(0, 120), Qt::NoButton, Qt::NoModifier,
                    Qt::NoScrollPhase, false);
  QApplication::sendEvent(m_canvas, &event);
  ASSERT_EQ(spy.count(), 1);
  double new_min = spy.first().at(0).toDouble();
  double new_max = spy.first().at(1).toDouble();
  // Zoom in: range should be narrower than [0, 10]
  EXPECT_GT(new_min, 0.0);
  EXPECT_LT(new_max, 10.0);
}

// ---- Zoom stack tests -------------------------------------------------------

// After a rubber-band zoom, zoomStackChanged(true) is emitted and the stack
// holds the previous range.
TEST_F(PlotCanvasUITest, RubberBandZoomPushesToStack) {
  m_canvas->setZoomMode(true);
  QSignalSpy spy(m_canvas, &PlotCanvas::zoomStackChanged);
  int y = H / 2;
  QPoint start(timeToPixelX(2.0), y);
  QPoint end(timeToPixelX(8.0), y);

  QTest::mousePress(m_canvas, Qt::LeftButton, Qt::NoModifier, start);
  QMouseEvent move(QEvent::MouseMove, end, m_canvas->mapToGlobal(end),
                   Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
  QApplication::sendEvent(m_canvas, &move);
  QTest::mouseRelease(m_canvas, Qt::LeftButton, Qt::NoModifier, end);

  ASSERT_EQ(spy.count(), 1);
  EXPECT_TRUE(spy.first().at(0).toBool());
}

// prevZoom() after a rubber-band zoom restores the original range and emits
// zoomStackChanged(false) when the stack is exhausted.
TEST_F(PlotCanvasUITest, PrevZoomAfterRubberBandRestoresRange) {
  m_canvas->setZoomMode(true);
  int y = H / 2;
  QPoint start(timeToPixelX(2.0), y);
  QPoint end(timeToPixelX(8.0), y);

  QTest::mousePress(m_canvas, Qt::LeftButton, Qt::NoModifier, start);
  QMouseEvent move(QEvent::MouseMove, end, m_canvas->mapToGlobal(end),
                   Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
  QApplication::sendEvent(m_canvas, &move);
  QTest::mouseRelease(m_canvas, Qt::LeftButton, Qt::NoModifier, end);

  QSignalSpy range_spy(m_canvas, &PlotCanvas::viewRangeChanged);
  QSignalSpy stack_spy(m_canvas, &PlotCanvas::zoomStackChanged);
  m_canvas->prevZoom();

  ASSERT_EQ(range_spy.count(), 1);
  EXPECT_NEAR(range_spy.first().at(0).toDouble(), 0.0, 1e-9);
  EXPECT_NEAR(range_spy.first().at(1).toDouble(), 10.0, 1e-9);
  ASSERT_EQ(stack_spy.count(), 1);
  EXPECT_FALSE(stack_spy.first().at(0).toBool());
}

// After a scroll-wheel zoom, zoomStackChanged(true) is emitted.
TEST_F(PlotCanvasUITest, WheelZoomPushesToStack) {
  m_canvas->setZoomMode(true);
  QSignalSpy spy(m_canvas, &PlotCanvas::zoomStackChanged);
  QPoint center(W / 2, H / 2);
  QWheelEvent event(center, m_canvas->mapToGlobal(center), QPoint(0, 0),
                    QPoint(0, 120), Qt::NoButton, Qt::NoModifier,
                    Qt::NoScrollPhase, false);
  QApplication::sendEvent(m_canvas, &event);

  ASSERT_EQ(spy.count(), 1);
  EXPECT_TRUE(spy.first().at(0).toBool());
}

// prevZoom() after a scroll-wheel zoom restores the original range.
TEST_F(PlotCanvasUITest, PrevZoomAfterWheelZoomRestoresRange) {
  m_canvas->setZoomMode(true);
  QPoint center(W / 2, H / 2);
  QWheelEvent event(center, m_canvas->mapToGlobal(center), QPoint(0, 0),
                    QPoint(0, 120), Qt::NoButton, Qt::NoModifier,
                    Qt::NoScrollPhase, false);
  QApplication::sendEvent(m_canvas, &event);

  QSignalSpy range_spy(m_canvas, &PlotCanvas::viewRangeChanged);
  m_canvas->prevZoom();

  ASSERT_EQ(range_spy.count(), 1);
  EXPECT_NEAR(range_spy.first().at(0).toDouble(), 0.0, 1e-9);
  EXPECT_NEAR(range_spy.first().at(1).toDouble(), 10.0, 1e-9);
}

// prevZoom() on an empty stack is a no-op (no signals emitted).
TEST_F(PlotCanvasUITest, PrevZoomOnEmptyStackIsNoop) {
  QSignalSpy range_spy(m_canvas, &PlotCanvas::viewRangeChanged);
  QSignalSpy stack_spy(m_canvas, &PlotCanvas::zoomStackChanged);
  m_canvas->prevZoom();
  EXPECT_EQ(range_spy.count(), 0);
  EXPECT_EQ(stack_spy.count(), 0);
}

// Multiple zooms build a stack; prevZoom() unwinds them one at a time.
TEST_F(PlotCanvasUITest, PrevZoomUnwindsMultipleZooms) {
  m_canvas->setZoomMode(true);

  // First zoom: [0,10] -> ~[2,8]
  {
    int y = H / 2;
    QPoint start(timeToPixelX(2.0), y);
    QPoint end(timeToPixelX(8.0), y);
    QTest::mousePress(m_canvas, Qt::LeftButton, Qt::NoModifier, start);
    QMouseEvent move(QEvent::MouseMove, end, m_canvas->mapToGlobal(end),
                     Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(m_canvas, &move);
    QTest::mouseRelease(m_canvas, Qt::LeftButton, Qt::NoModifier, end);
  }

  double mid_min = m_canvas->viewMinTime();
  double mid_max = m_canvas->viewMaxTime();

  // Second zoom: further in
  {
    int y = H / 2;
    QPoint start(timeToPixelX(3.0), y);
    QPoint end(timeToPixelX(7.0), y);
    QTest::mousePress(m_canvas, Qt::LeftButton, Qt::NoModifier, start);
    QMouseEvent move(QEvent::MouseMove, end, m_canvas->mapToGlobal(end),
                     Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(m_canvas, &move);
    QTest::mouseRelease(m_canvas, Qt::LeftButton, Qt::NoModifier, end);
  }

  // First prevZoom: should return to mid range
  m_canvas->prevZoom();
  EXPECT_NEAR(m_canvas->viewMinTime(), mid_min, 1e-9);
  EXPECT_NEAR(m_canvas->viewMaxTime(), mid_max, 1e-9);

  // Second prevZoom: should return to original [0, 10]
  m_canvas->prevZoom();
  EXPECT_NEAR(m_canvas->viewMinTime(), 0.0, 1e-9);
  EXPECT_NEAR(m_canvas->viewMaxTime(), 10.0, 1e-9);
}

// resetZoom() does not push to the stack when the view range is unchanged
// (e.g. no data loaded so autoFitTimeRange is a no-op).
TEST_F(PlotCanvasUITest, ResetZoomDoesNotPushWhenViewUnchanged) {
  QSignalSpy spy(m_canvas, &PlotCanvas::zoomStackChanged);
  m_canvas->resetZoom();
  EXPECT_EQ(spy.count(), 0);
}

// Right-click drag pushes the pre-pan range onto the zoom stack on first move.
TEST_F(PlotCanvasUITest, RightClickPanPushesToStack) {
  QSignalSpy range_spy(m_canvas, &PlotCanvas::viewRangeChanged);
  QSignalSpy stack_spy(m_canvas, &PlotCanvas::zoomStackChanged);
  int y = H / 2;
  QPoint start(200, y);
  QPoint end(250, y);

  QTest::mousePress(m_canvas, Qt::RightButton, Qt::NoModifier, start);
  // No push yet — pan hasn't actually moved.
  EXPECT_EQ(stack_spy.count(), 0);

  QMouseEvent move(QEvent::MouseMove, end, m_canvas->mapToGlobal(end),
                   Qt::RightButton, Qt::RightButton, Qt::NoModifier);
  QApplication::sendEvent(m_canvas, &move);
  // Push happens on first move.
  ASSERT_EQ(stack_spy.count(), 1);
  EXPECT_TRUE(stack_spy.first().at(0).toBool());

  QTest::mouseRelease(m_canvas, Qt::RightButton, Qt::NoModifier, end);

  ASSERT_GE(range_spy.count(), 1);
  double new_min = range_spy.last().at(0).toDouble();
  double new_max = range_spy.last().at(1).toDouble();
  EXPECT_LT(new_min, 0.0);
  EXPECT_LT(new_max, 10.0);
}

// Right-click without dragging does NOT push to the zoom stack.
TEST_F(PlotCanvasUITest, RightClickWithoutDragDoesNotPushStack) {
  QSignalSpy stack_spy(m_canvas, &PlotCanvas::zoomStackChanged);
  QTest::mouseClick(m_canvas, Qt::RightButton, Qt::NoModifier,
                    QPoint(200, H / 2));
  EXPECT_EQ(stack_spy.count(), 0);
}

// prevZoom() after a right-click pan restores the pre-pan range.
TEST_F(PlotCanvasUITest, PrevZoomAfterPanRestoresRange) {
  int y = H / 2;
  QPoint start(200, y);
  QPoint end(250, y);

  QTest::mousePress(m_canvas, Qt::RightButton, Qt::NoModifier, start);
  QMouseEvent move(QEvent::MouseMove, end, m_canvas->mapToGlobal(end),
                   Qt::RightButton, Qt::RightButton, Qt::NoModifier);
  QApplication::sendEvent(m_canvas, &move);
  QTest::mouseRelease(m_canvas, Qt::RightButton, Qt::NoModifier, end);

  QSignalSpy range_spy(m_canvas, &PlotCanvas::viewRangeChanged);
  m_canvas->prevZoom();

  ASSERT_EQ(range_spy.count(), 1);
  EXPECT_NEAR(range_spy.first().at(0).toDouble(), 0.0, 1e-9);
  EXPECT_NEAR(range_spy.first().at(1).toDouble(), 10.0, 1e-9);
}

// Right-click still pans even when zoom mode is active.
TEST_F(PlotCanvasUITest, RightClickPansInZoomMode) {
  m_canvas->setZoomMode(true);
  QSignalSpy spy(m_canvas, &PlotCanvas::viewRangeChanged);
  int y = H / 2;
  QPoint start(200, y);
  QPoint end(250, y);

  QTest::mousePress(m_canvas, Qt::RightButton, Qt::NoModifier, start);
  QMouseEvent move(QEvent::MouseMove, end, m_canvas->mapToGlobal(end),
                   Qt::RightButton, Qt::RightButton, Qt::NoModifier);
  QApplication::sendEvent(m_canvas, &move);
  QTest::mouseRelease(m_canvas, Qt::RightButton, Qt::NoModifier, end);

  ASSERT_GE(spy.count(), 1);
  double new_min = spy.last().at(0).toDouble();
  double new_max = spy.last().at(1).toDouble();
  EXPECT_LT(new_min, 0.0);
  EXPECT_LT(new_max, 10.0);
}

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
