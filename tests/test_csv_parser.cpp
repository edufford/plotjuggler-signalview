#include <gtest/gtest.h>

#include "overlay_manager.h"

// --- CsvUtil::detectDelimiter ---

// Standard comma-separated header is detected as comma.
TEST(DetectDelimiter, CommaSeparated) {
  EXPECT_EQ(CsvUtil::detectDelimiter("time,velocity,position"), ',');
}

// Tab-separated header is detected as tab.
TEST(DetectDelimiter, TabSeparated) {
  EXPECT_EQ(CsvUtil::detectDelimiter("time\tvelocity\tposition"), '\t');
}

// Semicolon-separated header is detected as semicolon.
TEST(DetectDelimiter, SemicolonSeparated) {
  EXPECT_EQ(CsvUtil::detectDelimiter("time;velocity;position"), ';');
}

// A line with no delimiters defaults to comma.
TEST(DetectDelimiter, NoDelimiters_DefaultComma) {
  EXPECT_EQ(CsvUtil::detectDelimiter("singlecolumn"), ',');
}

// An empty line defaults to comma.
TEST(DetectDelimiter, EmptyLine_DefaultComma) {
  EXPECT_EQ(CsvUtil::detectDelimiter(""), ',');
}

// When tabs outnumber commas, tab wins.
TEST(DetectDelimiter, MixedDelimiters_TabWins) {
  EXPECT_EQ(CsvUtil::detectDelimiter("a,b\tc\td"), '\t');
}

// When semicolons outnumber commas, semicolon wins.
TEST(DetectDelimiter, MixedDelimiters_SemicolonWins) {
  EXPECT_EQ(CsvUtil::detectDelimiter("a,b;c;d"), ';');
}

// --- loadOverlayFile (integration tests for parseCSV) ---

class CsvParserTest : public ::testing::Test {
 protected:
  OverlayManager mgr_;
  std::string fixture_dir_ = TEST_FIXTURE_DIR;
};

// A comma-delimited CSV loads successfully with the expected signal count.
TEST_F(CsvParserTest, LoadCommaCSV) {
  int idx = mgr_.loadOverlayFile(fixture_dir_ + "/comma.csv");
  EXPECT_GT(idx, 0);
  EXPECT_EQ(mgr_.layerCount(), 1);
  auto sig_list = mgr_.allAvailableSignals();
  ASSERT_EQ(sig_list.size(), 2u);
}

// A tab-delimited file loads successfully.
TEST_F(CsvParserTest, LoadTabTSV) {
  int idx = mgr_.loadOverlayFile(fixture_dir_ + "/tab.tsv");
  EXPECT_GT(idx, 0);
  auto sig_list = mgr_.allAvailableSignals();
  ASSERT_EQ(sig_list.size(), 2u);
}

// A semicolon-delimited file loads successfully.
TEST_F(CsvParserTest, LoadSemicolonCSV) {
  int idx = mgr_.loadOverlayFile(fixture_dir_ + "/semicolon.csv");
  EXPECT_GT(idx, 0);
  auto sig_list = mgr_.allAvailableSignals();
  ASSERT_EQ(sig_list.size(), 2u);
}

// A file with only one column (no data) fails to load.
TEST_F(CsvParserTest, LoadSingleColumn_Fails) {
  int idx = mgr_.loadOverlayFile(fixture_dir_ + "/single_col.csv");
  EXPECT_EQ(idx, -1);
  EXPECT_EQ(mgr_.layerCount(), 0);
}

// An empty file fails to load.
TEST_F(CsvParserTest, LoadEmptyFile_Fails) {
  int idx = mgr_.loadOverlayFile(fixture_dir_ + "/empty.csv");
  EXPECT_EQ(idx, -1);
  EXPECT_EQ(mgr_.layerCount(), 0);
}

// A nonexistent file path fails to load.
TEST_F(CsvParserTest, LoadNonexistentFile_Fails) {
  int idx = mgr_.loadOverlayFile("/nonexistent/path.csv");
  EXPECT_EQ(idx, -1);
}

// After loading, sig_list are resolvable by name.
TEST_F(CsvParserTest, LoadThenResolve) {
  int idx = mgr_.loadOverlayFile(fixture_dir_ + "/comma.csv");
  ASSERT_GT(idx, 0);
  auto result = mgr_.resolveSignal("signal_a");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->series->size(), 3u);
}

// Loading multiple files assigns incrementing layer indices.
TEST_F(CsvParserTest, LoadMultipleOverlays_LayerIndicesIncrease) {
  int idx1 = mgr_.loadOverlayFile(fixture_dir_ + "/comma.csv");
  int idx2 = mgr_.loadOverlayFile(fixture_dir_ + "/tab.tsv");
  EXPECT_GT(idx2, idx1);
  EXPECT_EQ(mgr_.layerCount(), 2);
}

// An overlay layer can be removed after loading.
TEST_F(CsvParserTest, RemoveOverlay) {
  int idx = mgr_.loadOverlayFile(fixture_dir_ + "/comma.csv");
  ASSERT_GT(idx, 0);
  EXPECT_TRUE(mgr_.removeOverlay(idx));
  EXPECT_EQ(mgr_.layerCount(), 0);
}

// Parsed data points have the correct time and value from the CSV.
TEST_F(CsvParserTest, LoadedDataHasCorrectValues) {
  int idx = mgr_.loadOverlayFile(fixture_dir_ + "/comma.csv");
  ASSERT_GT(idx, 0);
  auto result = mgr_.resolveSignal("signal_b");
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->series->size(), 3u);
  auto it = result->series->begin();
  EXPECT_DOUBLE_EQ(it->x, 0.0);
  EXPECT_DOUBLE_EQ(it->y, 10.0);
  ++it;
  EXPECT_DOUBLE_EQ(it->x, 1.0);
  EXPECT_DOUBLE_EQ(it->y, 20.0);
  ++it;
  EXPECT_DOUBLE_EQ(it->x, 2.0);
  EXPECT_DOUBLE_EQ(it->y, 30.0);
}
