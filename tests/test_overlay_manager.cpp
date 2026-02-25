#include <gtest/gtest.h>

#include "overlay_manager.h"

// Helper: create a PlotDataMapRef with named series and data points.
static PJ::PlotDataMapRef makeTestData(
    const std::vector<
        std::pair<std::string, std::vector<std::pair<double, double>>>>&
        series_data) {
  PJ::PlotDataMapRef data;
  for (const auto& [name, points] : series_data) {
    auto it = data.addNumeric(name);
    for (const auto& [t, v] : points) {
      it->second.pushBack(PJ::PlotData::Point(t, v));
    }
  }
  return data;
}

class OverlayManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    m_base_data = makeTestData({
        {"velocity", {{0, 1}, {1, 2}, {2, 3}}},
        {"position", {{0, 10}, {1, 20}, {2, 30}}},
    });
  }

  PJ::PlotDataMapRef m_base_data;
  OverlayManager m_mgr;
};

// --- Initial state ---

// A freshly constructed manager has no layers.
TEST_F(OverlayManagerTest, InitialState_NoLayers) {
  EXPECT_EQ(m_mgr.layerCount(), 0);
  EXPECT_FALSE(m_mgr.hasBaseLayer());
  EXPECT_FALSE(m_mgr.hasOverlays());
}

// --- setBaseData ---

// Setting base data creates a layer 1 named "PJ Data".
TEST_F(OverlayManagerTest, SetBaseData_CreatesLayer1) {
  m_mgr.setBaseData(&m_base_data);
  EXPECT_TRUE(m_mgr.hasBaseLayer());
  EXPECT_EQ(m_mgr.layerCount(), 1);
  EXPECT_EQ(m_mgr.layers()[0].index, 1);
  EXPECT_EQ(m_mgr.layers()[0].display_name, "PJ Data");
  EXPECT_TRUE(m_mgr.layers()[0].file_path.empty());
}

// Calling setBaseData a second time updates the data pointer in place.
TEST_F(OverlayManagerTest, SetBaseData_Twice_UpdatesPointer) {
  m_mgr.setBaseData(&m_base_data);

  PJ::PlotDataMapRef other_data;
  other_data.addNumeric("new_signal");
  m_mgr.setBaseData(&other_data);

  EXPECT_EQ(m_mgr.layerCount(), 1);
  auto sig_list = m_mgr.allAvailableSignals();
  EXPECT_EQ(sig_list.size(), 1u);
  EXPECT_EQ(sig_list[0], "new_signal");
}

// --- allAvailableSignals ---

// With a single layer, signal names are returned without "#N/" prefix.
TEST_F(OverlayManagerTest, AllAvailableSignals_SingleLayer_NoPrefixes) {
  m_mgr.setBaseData(&m_base_data);
  auto sig_list = m_mgr.allAvailableSignals();
  ASSERT_EQ(sig_list.size(), 2u);
  EXPECT_EQ(sig_list[0], "position");
  EXPECT_EQ(sig_list[1], "velocity");
}

// Verify no signal starts with '#' when there is only one layer.
TEST_F(OverlayManagerTest, AllAvailableSignals_SingleLayer_NoHashPrefix) {
  m_mgr.setBaseData(&m_base_data);
  auto sig_list = m_mgr.allAvailableSignals();
  for (const auto& s : sig_list) {
    EXPECT_EQ(s.find('#'), std::string::npos);
  }
}

// --- resolveSignal ---

// An unprefixed name resolves against layer 1 by default.
TEST_F(OverlayManagerTest, ResolveSignal_UnprefixedName) {
  m_mgr.setBaseData(&m_base_data);
  auto result = m_mgr.resolveSignal("velocity");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->layer_index, 1);
  EXPECT_EQ(result->time_offset, 0.0);
  EXPECT_NE(result->series, nullptr);
  EXPECT_EQ(result->series->size(), 3u);
}

// An explicitly prefixed "#1/name" also resolves to layer 1.
TEST_F(OverlayManagerTest, ResolveSignal_PrefixedName) {
  m_mgr.setBaseData(&m_base_data);
  auto result = m_mgr.resolveSignal("#1/velocity");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->layer_index, 1);
  EXPECT_EQ(result->series->size(), 3u);
}

// A signal name that doesn't exist returns nullopt.
TEST_F(OverlayManagerTest, ResolveSignal_Nonexistent) {
  m_mgr.setBaseData(&m_base_data);
  auto result = m_mgr.resolveSignal("nonexistent");
  EXPECT_FALSE(result.has_value());
}

// Requesting a signal from a layer that doesn't exist returns nullopt.
TEST_F(OverlayManagerTest, ResolveSignal_WrongLayer) {
  m_mgr.setBaseData(&m_base_data);
  auto result = m_mgr.resolveSignal("#2/velocity");
  EXPECT_FALSE(result.has_value());
}

// --- findMatchingSignals ---

// Returns prefixed names for raw names that exist in the layer.
TEST_F(OverlayManagerTest, FindMatchingSignals_MatchExists) {
  m_mgr.setBaseData(&m_base_data);
  auto matches = m_mgr.findMatchingSignals(1, {"velocity", "nonexistent"});
  ASSERT_EQ(matches.size(), 1u);
  EXPECT_EQ(matches[0], "#1/velocity");
}

// Returns empty when no raw names match.
TEST_F(OverlayManagerTest, FindMatchingSignals_NoMatch) {
  m_mgr.setBaseData(&m_base_data);
  auto matches = m_mgr.findMatchingSignals(1, {"nonexistent"});
  EXPECT_TRUE(matches.empty());
}

// Returns empty when the specified layer doesn't exist.
TEST_F(OverlayManagerTest, FindMatchingSignals_WrongLayer) {
  m_mgr.setBaseData(&m_base_data);
  auto matches = m_mgr.findMatchingSignals(99, {"velocity"});
  EXPECT_TRUE(matches.empty());
}

// --- timeOffset ---

// Newly created layers have zero time offset.
TEST_F(OverlayManagerTest, TimeOffset_DefaultZero) {
  m_mgr.setBaseData(&m_base_data);
  EXPECT_DOUBLE_EQ(m_mgr.timeOffset(1), 0.0);
}

// setTimeOffset stores the value and timeOffset retrieves it.
TEST_F(OverlayManagerTest, SetTimeOffset_RoundTrip) {
  m_mgr.setBaseData(&m_base_data);
  m_mgr.setTimeOffset(1, 5.5);
  EXPECT_DOUBLE_EQ(m_mgr.timeOffset(1), 5.5);
}

// Querying a nonexistent layer returns zero.
TEST_F(OverlayManagerTest, TimeOffset_NonexistentLayer) {
  EXPECT_DOUBLE_EQ(m_mgr.timeOffset(99), 0.0);
}

// resolveSignal includes the layer's time offset in the result.
TEST_F(OverlayManagerTest, ResolveSignal_WithTimeOffset) {
  m_mgr.setBaseData(&m_base_data);
  m_mgr.setTimeOffset(1, 3.14);
  auto result = m_mgr.resolveSignal("velocity");
  ASSERT_TRUE(result.has_value());
  EXPECT_DOUBLE_EQ(result->time_offset, 3.14);
}

// --- removeOverlay ---

// The base PJ layer cannot be removed.
TEST_F(OverlayManagerTest, RemoveOverlay_CannotRemoveBaseLayer) {
  m_mgr.setBaseData(&m_base_data);
  EXPECT_FALSE(m_mgr.removeOverlay(1));
  EXPECT_EQ(m_mgr.layerCount(), 1);
}

// Returns false when the layer index does not exist.
TEST_F(OverlayManagerTest, RemoveOverlay_NonexistentLayer) {
  m_mgr.setBaseData(&m_base_data);
  EXPECT_FALSE(m_mgr.removeOverlay(99));
}

// A file-loaded overlay layer can be removed; layer count decreases.
TEST_F(OverlayManagerTest, RemoveOverlay_RemovesFileLayer) {
  m_mgr.setBaseData(&m_base_data);
  std::string fixture = std::string(TEST_FIXTURE_DIR) + "/comma.csv";
  int layer = m_mgr.loadOverlayFile(fixture);
  ASSERT_GT(layer, 0);
  EXPECT_EQ(m_mgr.layerCount(), 2);

  EXPECT_TRUE(m_mgr.removeOverlay(layer));
  EXPECT_EQ(m_mgr.layerCount(), 1);
  EXPECT_FALSE(m_mgr.hasOverlays());
}

// clearOverlays removes all file-loaded layers and leaves the base layer.
TEST_F(OverlayManagerTest, ClearOverlays_LeavesBaseLayer) {
  m_mgr.setBaseData(&m_base_data);
  std::string fixture = std::string(TEST_FIXTURE_DIR) + "/comma.csv";
  ASSERT_GT(m_mgr.loadOverlayFile(fixture), 0);
  ASSERT_GT(m_mgr.loadOverlayFile(fixture), 0);
  EXPECT_EQ(m_mgr.layerCount(), 3);

  m_mgr.clearOverlays();

  EXPECT_EQ(m_mgr.layerCount(), 1);
  EXPECT_FALSE(m_mgr.hasOverlays());
}

// Simulates loading a layout twice: clearing before reload prevents stacking.
TEST_F(OverlayManagerTest, ClearOverlays_PreventStackingOnReload) {
  m_mgr.setBaseData(&m_base_data);
  std::string fixture = std::string(TEST_FIXTURE_DIR) + "/comma.csv";

  // First "layout load": add two overlays.
  ASSERT_GT(m_mgr.loadOverlayFile(fixture), 0);
  ASSERT_GT(m_mgr.loadOverlayFile(fixture), 0);
  EXPECT_EQ(m_mgr.layerCount(), 3);

  // Second "layout load": clear first, then add one overlay.
  m_mgr.clearOverlays();
  ASSERT_GT(m_mgr.loadOverlayFile(fixture), 0);
  EXPECT_EQ(m_mgr.layerCount(), 2);
}

// --- layerByIndex ---

// Returns a pointer to the layer with the given index.
TEST_F(OverlayManagerTest, LayerByIndex_Valid) {
  m_mgr.setBaseData(&m_base_data);
  auto* layer = m_mgr.layerByIndex(1);
  ASSERT_NE(layer, nullptr);
  EXPECT_EQ(layer->display_name, "PJ Data");
}

// Returns nullptr for an index that doesn't exist.
TEST_F(OverlayManagerTest, LayerByIndex_Invalid) {
  m_mgr.setBaseData(&m_base_data);
  EXPECT_EQ(m_mgr.layerByIndex(99), nullptr);
}
