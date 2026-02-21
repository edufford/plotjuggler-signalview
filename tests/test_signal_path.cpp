#include <gtest/gtest.h>

#include <QApplication>
#include <QPainterPath>
#include <limits>
#include <map>

#include "PlotJuggler/plotdata.h"
#include "plot_canvas.h"

// Canvas and view geometry used by all tests.
static constexpr int W = 400;
static constexpr int H = 300;

// Acceptable pixel error for coordinate comparisons. Sub-pixel rounding in
// timeToPixelX / valueToPixelY means results may be off by up to one pixel.
static constexpr double PIXEL_TOL = 1.0;

// view [0, 10] → plot_w=370, plot_h=250
// pixelX(time)  = MARGIN_LEFT + (time/10)*370 = 10 + 37*time
// For sig y_min=0, y_max=1, band_center=0.5, band_height=1.0, scroll=0:
//   band_top=10, band_bottom=260 → pixelY(value) = 260 - value*250
static double pixelX(double time) {
  return PlotCanvas::MARGIN_LEFT +
         time / 10.0 * (W - PlotCanvas::MARGIN_LEFT - PlotCanvas::MARGIN_RIGHT);
}
static double pixelY(double value) {
  double plot_h = H - PlotCanvas::MARGIN_TOP - PlotCanvas::MARGIN_BOTTOM;
  double band_bottom = PlotCanvas::MARGIN_TOP + plot_h;
  return band_bottom - value * plot_h;
}

// Make a PlotData series from a list of (time, value) pairs.
static PJ::PlotData makeSeries(
    const std::vector<std::pair<double, double>>& pts) {
  PJ::PlotData s("test", nullptr);
  for (const auto& [t, v] : pts) {
    s.pushBack(PJ::PlotData::Point(t, v));
  }
  return s;
}

class PlotCanvasPathTest : public ::testing::Test {
 protected:
  void SetUp() override {
    m_canvas = new PlotCanvas;
    m_canvas->resize(W, H);
    m_canvas->setViewRange(0.0, 10.0);
  }
  void TearDown() override { delete m_canvas; }

  QPainterPath buildPath(const SignalEntry& sig, const PJ::PlotData& series,
                         double t_offset, size_t start_idx, bool downsample) {
    return m_canvas->buildSignalPath(sig, series, t_offset, start_idx,
                                     downsample);
  }

  // Signal entry mapping y=[0,1] to the full canvas band (no scroll).
  static SignalEntry makeSig() {
    SignalEntry sig;
    sig.y_min = 0.0;
    sig.y_max = 1.0;
    sig.band_center = 0.5;
    sig.band_height = 1.0;
    return sig;
  }

  PlotCanvas* m_canvas = nullptr;
};

// --- Full-resolution path tests ---

// An empty series has no points to iterate, so buildSignalPath should return
// an empty QPainterPath with no elements.
TEST_F(PlotCanvasPathTest, FullResEmptySeriesGivesEmptyPath) {
  PJ::PlotData series("test", nullptr);
  auto path = buildPath(makeSig(), series, 0.0, 0, false);
  EXPECT_TRUE(path.isEmpty());
}

// Step-wise rendering produces a moveTo at the first point followed by a
// horizontal hold (lineTo at the same y) then a vertical step (lineTo at the
// new y) for each subsequent point.
TEST_F(PlotCanvasPathTest, FullResTwoPointsInViewFormOneStep) {
  auto series = makeSeries({{0.0, 0.0}, {10.0, 1.0}});
  auto path = buildPath(makeSig(), series, 0.0, 0, false);

  // moveTo first point, horizontal hold to second x, then step to second y.
  ASSERT_EQ(path.elementCount(), 3);
  EXPECT_EQ(path.elementAt(0).type, QPainterPath::MoveToElement);
  EXPECT_NEAR(path.elementAt(0).x, pixelX(0.0), PIXEL_TOL);
  EXPECT_NEAR(path.elementAt(0).y, pixelY(0.0), PIXEL_TOL);
  EXPECT_EQ(path.elementAt(1).type, QPainterPath::LineToElement);
  EXPECT_NEAR(path.elementAt(1).x, pixelX(10.0), PIXEL_TOL);
  EXPECT_NEAR(path.elementAt(1).y, pixelY(0.0), PIXEL_TOL);  // horizontal hold
  EXPECT_EQ(path.elementAt(2).type, QPainterPath::LineToElement);
  EXPECT_NEAR(path.elementAt(2).x, pixelX(10.0), PIXEL_TOL);
  EXPECT_NEAR(path.elementAt(2).y, pixelY(1.0), PIXEL_TOL);  // vertical step
}

// When a point lies beyond the view's right edge, the path draws a horizontal
// hold to its pixel X then a vertical step to its Y, giving a clean boundary
// exit that avoids a flat tail at the last in-view value.
TEST_F(PlotCanvasPathTest, FullResPointBeyondViewExitsAtBoundary) {
  // Third point is beyond view — the exit should draw to its position.
  auto series = makeSeries({{2.0, 0.0}, {5.0, 0.5}, {12.0, 1.0}});
  auto path = buildPath(makeSig(), series, 0.0, 0, false);

  // moveTo(2,0), step to (5,0.5): 3 elements; exit at t=12: 2 more = 5 total.
  ASSERT_EQ(path.elementCount(), 5);
  // Last element lands at the boundary point's pixel position.
  EXPECT_NEAR(path.elementAt(4).x, pixelX(12.0), PIXEL_TOL);
  EXPECT_NEAR(path.elementAt(4).y, pixelY(1.0), PIXEL_TOL);
}

// drawSignals() passes a start_idx that is one position before the first
// visible point so the entry hold is correct. Verify that buildSignalPath
// starts iteration from that index and not from the beginning of the series.
TEST_F(PlotCanvasPathTest, FullResStartIdxSkipsEarlierPoints) {
  // start_idx=1 means the first data point is skipped.
  auto series = makeSeries({{0.0, 0.0}, {5.0, 0.5}, {10.0, 1.0}});
  auto path = buildPath(makeSig(), series, 0.0, 1, false);

  // Starts at index 1 (t=5): moveTo + one step = 3 elements.
  ASSERT_EQ(path.elementCount(), 3);
  EXPECT_EQ(path.elementAt(0).type, QPainterPath::MoveToElement);
  EXPECT_NEAR(path.elementAt(0).x, pixelX(5.0), PIXEL_TOL);
}

// Overlay layers carry a per-layer time offset that shifts all sample
// timestamps when converting to pixel X. Verify the first path element lands
// at the offset-adjusted screen position.
TEST_F(PlotCanvasPathTest, FullResTimeOffsetShiftsDataIntoView) {
  // Series local times [3, 8]; with t_offset=2 they appear at [5, 10].
  auto series = makeSeries({{3.0, 0.5}, {8.0, 0.5}});
  auto path = buildPath(makeSig(), series, 2.0, 0, false);

  ASSERT_GE(path.elementCount(), 1);
  EXPECT_EQ(path.elementAt(0).type, QPainterPath::MoveToElement);
  EXPECT_NEAR(path.elementAt(0).x, pixelX(5.0), PIXEL_TOL);  // 3 + 2 = 5
}

// --- Downsampled path tests ---

// Both rendering paths should begin at the same screen position for the same
// data. This verifies the downsampled path's moveTo uses identical coordinate
// transforms as the full-resolution path.
TEST_F(PlotCanvasPathTest, DownsampledStartsAtSamePositionAsFullRes) {
  // Well-separated points — each in its own pixel column.
  auto series = makeSeries({{0.0, 0.0}, {5.0, 1.0}});
  auto path_full = buildPath(makeSig(), series, 0.0, 0, false);
  auto path_ds = buildPath(makeSig(), series, 0.0, 0, true);

  ASSERT_GE(path_ds.elementCount(), 1);
  ASSERT_GE(path_full.elementCount(), 1);
  EXPECT_NEAR(path_ds.elementAt(0).x, path_full.elementAt(0).x, PIXEL_TOL);
  EXPECT_NEAR(path_ds.elementAt(0).y, path_full.elementAt(0).y, PIXEL_TOL);
}

// When multiple data points fall in the same pixel column the downsampled path
// must still reach the min and max pixel Y values seen in that column, so no
// vertical excursion is lost at any zoom level.
TEST_F(PlotCanvasPathTest, DownsampledCoversFullYRangeOfColumn) {
  // Two points very close in time map to the same pixel column (px_col=47):
  //   pixelX(1.000) = 10 + 37.000 = 47.0 → rounded to 47
  //   pixelX(1.005) = 10 + 37.185 = 47.185 → rounded to 47
  // A third point at t=5 forces the column flush.
  auto series = makeSeries({{1.000, 0.2}, {1.005, 0.8}, {5.0, 0.5}});
  auto path = buildPath(makeSig(), series, 0.0, 0, true);
  ASSERT_FALSE(path.isEmpty());

  double path_min_y = std::numeric_limits<double>::max();
  double path_max_y = std::numeric_limits<double>::lowest();
  for (int i = 0; i < path.elementCount(); i++) {
    path_min_y = std::min(path_min_y, path.elementAt(i).y);
    path_max_y = std::max(path_max_y, path.elementAt(i).y);
  }
  // pixelY(0.8) = 260 - 200 = 60  (smallest y = highest screen position)
  // pixelY(0.2) = 260 - 50  = 210 (largest y  = lowest  screen position)
  EXPECT_NEAR(path_min_y, pixelY(0.8), PIXEL_TOL);
  EXPECT_NEAR(path_max_y, pixelY(0.2), PIXEL_TOL);
}

// A trace with isolated points (one per pixel column) and clusters (multiple
// points per column) should produce the same per-column Y range in both the
// full-resolution and downsampled paths. For isolated points each rendering
// mode draws the same hold + step geometry; for clusters the downsampled path
// folds all intermediate values into a single min/max excursion that spans
// exactly the same Y range as the individual steps in the full-res path.
TEST_F(PlotCanvasPathTest, DownsampledMatchesFullResPerColumnYRange) {
  // Mix of isolated points (one per pixel column) and clusters.
  // Column numbers at W=400, view [0,10]: pixelX(t) = 10 + t/10*370
  //   t=0.0   → col 10   (isolated)
  //   t=1.0   → col 47   (isolated)
  //   t=2.0   → col 84   (pair start)
  //   t=2.005 → col 84   (same column: pair)
  //   t=4.0   → col 158  (isolated)
  //   t=6.0   → col 232  (triple start)
  //   t=6.003 → col 232  (same column: triple)
  //   t=6.006 → col 232  (same column: triple)
  //   t=8.0   → col 306  (isolated)
  //   t=9.0   → col 343  (isolated)
  auto series = makeSeries({
      {0.0,   0.0},
      {1.0,   0.3},
      {2.0,   0.5},
      {2.005, 0.9},
      {4.0,   0.1},
      {6.0,   0.7},
      {6.003, 0.2},
      {6.006, 0.6},
      {8.0,   0.4},
      {9.0,   0.8},
  });

  auto path_full = buildPath(makeSig(), series, 0.0, 0, false);
  auto path_ds = buildPath(makeSig(), series, 0.0, 0, true);

  // Extract per-column Y range (min, max) from a path by grouping elements
  // by their rounded pixel X coordinate.
  auto perColumnYRange = [](const QPainterPath& path) {
    std::map<int, std::pair<double, double>> ranges;
    for (int i = 0; i < path.elementCount(); i++) {
      int col = (int)std::round(path.elementAt(i).x);
      double y = path.elementAt(i).y;
      auto it = ranges.find(col);
      if (it == ranges.end()) {
        ranges[col] = {y, y};
      } else {
        auto& [min_y, max_y] = it->second;
        min_y = std::min(min_y, y);
        max_y = std::max(max_y, y);
      }
    }
    return ranges;
  };

  auto full_ranges = perColumnYRange(path_full);
  auto ds_ranges = perColumnYRange(path_ds);

  ASSERT_FALSE(full_ranges.empty());
  // Every column in the full-res path must appear in the downsampled path with
  // a matching Y range (within one pixel).
  for (const auto& [col, range] : full_ranges) {
    ASSERT_TRUE(ds_ranges.count(col)) << "column " << col << " missing from downsampled path";
    const auto& [full_min_y, full_max_y] = range;
    const auto& [ds_min_y, ds_max_y] = ds_ranges[col];
    EXPECT_NEAR(ds_min_y, full_min_y, PIXEL_TOL) << "min_y mismatch at column " << col;
    EXPECT_NEAR(ds_max_y, full_max_y, PIXEL_TOL) << "max_y mismatch at column " << col;
  }
}

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
