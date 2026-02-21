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

// Left-clicking on the canvas sets the cursor time near the clicked position.
TEST_F(PlotCanvasUITest, LeftClickSetsCursorTime) {
  QSignalSpy spy(m_canvas, &PlotCanvas::cursorMoved);
  int click_x = timeToPixelX(5.0);
  int click_y = H / 2;
  QTest::mouseClick(m_canvas, Qt::LeftButton, Qt::NoModifier,
                    QPoint(click_x, click_y));
  ASSERT_GE(spy.count(), 1);
  double t = spy.last().at(0).toDouble();
  EXPECT_NEAR(t, 5.0, 0.5);
}

// Dragging horizontally after a left press updates cursor time continuously.
TEST_F(PlotCanvasUITest, CursorDragUpdatesTime) {
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

  ASSERT_GE(spy.count(), 2);
  double t_first = spy.first().at(0).toDouble();
  double t_last = spy.last().at(0).toDouble();
  EXPECT_NEAR(t_first, 3.0, 0.5);
  EXPECT_NEAR(t_last, 7.0, 0.5);
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

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
