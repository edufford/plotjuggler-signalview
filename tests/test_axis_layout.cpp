#include <gtest/gtest.h>

#include "y_axis_panel.h"

// Helper: create a SignalEntry positioned at the given band center and height.
static SignalEntry makeEntry(double center, double height) {
  SignalEntry e;
  e.name = "test";
  e.color = QColor(Qt::red);
  e.band_center_norm = center;
  e.band_height_norm = height;
  return e;
}

static constexpr int TOP = PlotCanvas::MARGIN_TOP;     // 10
static constexpr int BOT = PlotCanvas::MARGIN_BOTTOM;  // 40

// --- bandTopY ---

// A full-height band centered at 0.5 has its top at the margin.
TEST(BandTopY, CenteredFullHeight) {
  auto e = makeEntry(0.5, 1.0);
  EXPECT_DOUBLE_EQ(AxisLayout::bandTopY(e, 500), 10.0);
}

// A half-height band centered at 0.5 starts at 25% of the plot area.
TEST(BandTopY, CenteredHalfHeight) {
  auto e = makeEntry(0.5, 0.5);
  EXPECT_DOUBLE_EQ(AxisLayout::bandTopY(e, 500), 122.5);
}

// Scroll offset shifts bands upward, producing negative pixel values.
TEST(BandTopY, WithScrollOffset) {
  auto e = makeEntry(0.5, 1.0);
  EXPECT_DOUBLE_EQ(AxisLayout::bandTopY(e, 500, 0.1), -35.0);
}

// --- bandBottomY ---

// A full-height band's bottom is at the plot area's lower edge.
TEST(BandBottomY, CenteredFullHeight) {
  auto e = makeEntry(0.5, 1.0);
  EXPECT_DOUBLE_EQ(AxisLayout::bandBottomY(e, 500), 460.0);
}

// A half-height band's bottom is at 75% of the plot area.
TEST(BandBottomY, CenteredHalfHeight) {
  auto e = makeEntry(0.5, 0.5);
  EXPECT_DOUBLE_EQ(AxisLayout::bandBottomY(e, 500), 347.5);
}

// The pixel distance from top to bottom equals plot_h * band_height_norm.
TEST(BandGeometry, HeightEqualsExpected) {
  auto e = makeEntry(0.3, 0.4);
  double top = AxisLayout::bandTopY(e, 600);
  double bot = AxisLayout::bandBottomY(e, 600);
  double plot_h = 600.0 - TOP - BOT;
  EXPECT_NEAR(bot - top, plot_h * 0.4, 1e-10);
}

// --- textRowYOffsets ---

// An empty entry list produces an empty offset list.
TEST(TextRowYOffsets, EmptyEntries) {
  std::vector<SignalEntry> entries;
  auto offsets = AxisLayout::textRowYOffsets(entries, 500, 15);
  EXPECT_TRUE(offsets.empty());
}

// A single entry gets zero offset (no collision possible).
TEST(TextRowYOffsets, SingleEntry_ZeroOffset) {
  std::vector<SignalEntry> entries = {makeEntry(0.5, 0.5)};
  auto offsets = AxisLayout::textRowYOffsets(entries, 500, 15);
  ASSERT_EQ(offsets.size(), 1u);
  EXPECT_DOUBLE_EQ(offsets[0], 0.0);
}

// Two entries at the same position: the second is pushed down by row_height.
TEST(TextRowYOffsets, TwoEntries_SamePosition_SecondPushedDown) {
  std::vector<SignalEntry> entries = {makeEntry(0.5, 0.5), makeEntry(0.5, 0.5)};
  auto offsets = AxisLayout::textRowYOffsets(entries, 500, 15);
  ASSERT_EQ(offsets.size(), 2u);
  EXPECT_DOUBLE_EQ(offsets[0], 0.0);
  EXPECT_GE(offsets[1], 14.5);
}

// Two entries far apart have no collision and both get zero offset.
TEST(TextRowYOffsets, TwoEntries_FarApart_NoCollision) {
  std::vector<SignalEntry> entries = {makeEntry(0.2, 0.1), makeEntry(0.8, 0.1)};
  auto offsets = AxisLayout::textRowYOffsets(entries, 500, 15);
  ASSERT_EQ(offsets.size(), 2u);
  EXPECT_DOUBLE_EQ(offsets[0], 0.0);
  EXPECT_DOUBLE_EQ(offsets[1], 0.0);
}

// Three entries at the same position stack progressively downward.
TEST(TextRowYOffsets, ThreeEntries_AllSamePosition_ProgressiveOffsets) {
  double c = 0.5;
  std::vector<SignalEntry> entries = {makeEntry(c, 0.3), makeEntry(c, 0.3),
                                      makeEntry(c, 0.3)};
  auto offsets = AxisLayout::textRowYOffsets(entries, 500, 15);
  ASSERT_EQ(offsets.size(), 3u);
  EXPECT_DOUBLE_EQ(offsets[0], 0.0);
  EXPECT_GT(offsets[1], 0.0);
  EXPECT_GT(offsets[2], offsets[1]);
}

// At the same band position, lower index (added first) claims the top slot.
TEST(TextRowYOffsets, InsertionOrderStability) {
  std::vector<SignalEntry> entries = {makeEntry(0.5, 0.3), makeEntry(0.5, 0.3)};
  auto offsets = AxisLayout::textRowYOffsets(entries, 500, 15);
  EXPECT_DOUBLE_EQ(offsets[0], 0.0);
  EXPECT_GT(offsets[1], 0.0);
}

// A signal higher on screen gets zero offset even if it has a higher index.
TEST(TextRowYOffsets, TopSignalGetsTopSlot) {
  std::vector<SignalEntry> entries = {makeEntry(0.8, 0.1), makeEntry(0.2, 0.1)};
  auto offsets = AxisLayout::textRowYOffsets(entries, 500, 15);
  ASSERT_EQ(offsets.size(), 2u);
  EXPECT_DOUBLE_EQ(offsets[0], 0.0);
  EXPECT_DOUBLE_EQ(offsets[1], 0.0);
}
