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
    base_data_ = makeTestData({
        {"velocity", {{0, 1}, {1, 2}, {2, 3}}},
        {"position", {{0, 10}, {1, 20}, {2, 30}}},
    });
  }

  PJ::PlotDataMapRef base_data_;
  OverlayManager mgr_;
};

// --- Initial state ---

// A freshly constructed manager has no layers.
TEST_F(OverlayManagerTest, InitialState_NoLayers) {
  EXPECT_EQ(mgr_.layerCount(), 0);
  EXPECT_FALSE(mgr_.hasBaseLayer());
  EXPECT_FALSE(mgr_.hasOverlays());
}

// --- setBaseData ---

// Setting base data creates a layer 1 named "PJ Data".
TEST_F(OverlayManagerTest, SetBaseData_CreatesLayer1) {
  mgr_.setBaseData(&base_data_);
  EXPECT_TRUE(mgr_.hasBaseLayer());
  EXPECT_EQ(mgr_.layerCount(), 1);
  EXPECT_EQ(mgr_.layers()[0].index, 1);
  EXPECT_EQ(mgr_.layers()[0].display_name, "PJ Data");
  EXPECT_TRUE(mgr_.layers()[0].file_path.empty());
}

// Calling setBaseData a second time updates the data pointer in place.
TEST_F(OverlayManagerTest, SetBaseData_Twice_UpdatesPointer) {
  mgr_.setBaseData(&base_data_);

  PJ::PlotDataMapRef other_data;
  other_data.addNumeric("new_signal");
  mgr_.setBaseData(&other_data);

  EXPECT_EQ(mgr_.layerCount(), 1);
  auto sig_list = mgr_.allAvailableSignals();
  EXPECT_EQ(sig_list.size(), 1u);
  EXPECT_EQ(sig_list[0], "new_signal");
}

// --- allAvailableSignals ---

// With a single layer, signal names are returned without "#N/" prefix.
TEST_F(OverlayManagerTest, AllAvailableSignals_SingleLayer_NoPrefixes) {
  mgr_.setBaseData(&base_data_);
  auto sig_list = mgr_.allAvailableSignals();
  ASSERT_EQ(sig_list.size(), 2u);
  EXPECT_EQ(sig_list[0], "position");
  EXPECT_EQ(sig_list[1], "velocity");
}

// Verify no signal starts with '#' when there is only one layer.
TEST_F(OverlayManagerTest, AllAvailableSignals_SingleLayer_NoHashPrefix) {
  mgr_.setBaseData(&base_data_);
  auto sig_list = mgr_.allAvailableSignals();
  for (const auto& s : sig_list) {
    EXPECT_EQ(s.find('#'), std::string::npos);
  }
}

// --- resolveSignal ---

// An unprefixed name resolves against layer 1 by default.
TEST_F(OverlayManagerTest, ResolveSignal_UnprefixedName) {
  mgr_.setBaseData(&base_data_);
  auto result = mgr_.resolveSignal("velocity");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->layer_index, 1);
  EXPECT_EQ(result->time_offset, 0.0);
  EXPECT_NE(result->series, nullptr);
  EXPECT_EQ(result->series->size(), 3u);
}

// An explicitly prefixed "#1/name" also resolves to layer 1.
TEST_F(OverlayManagerTest, ResolveSignal_PrefixedName) {
  mgr_.setBaseData(&base_data_);
  auto result = mgr_.resolveSignal("#1/velocity");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->layer_index, 1);
  EXPECT_EQ(result->series->size(), 3u);
}

// A signal name that doesn't exist returns nullopt.
TEST_F(OverlayManagerTest, ResolveSignal_Nonexistent) {
  mgr_.setBaseData(&base_data_);
  auto result = mgr_.resolveSignal("nonexistent");
  EXPECT_FALSE(result.has_value());
}

// Requesting a signal from a layer that doesn't exist returns nullopt.
TEST_F(OverlayManagerTest, ResolveSignal_WrongLayer) {
  mgr_.setBaseData(&base_data_);
  auto result = mgr_.resolveSignal("#2/velocity");
  EXPECT_FALSE(result.has_value());
}

// --- findMatchingSignals ---

// Returns prefixed names for raw names that exist in the layer.
TEST_F(OverlayManagerTest, FindMatchingSignals_MatchExists) {
  mgr_.setBaseData(&base_data_);
  auto matches = mgr_.findMatchingSignals(1, {"velocity", "nonexistent"});
  ASSERT_EQ(matches.size(), 1u);
  EXPECT_EQ(matches[0], "#1/velocity");
}

// Returns empty when no raw names match.
TEST_F(OverlayManagerTest, FindMatchingSignals_NoMatch) {
  mgr_.setBaseData(&base_data_);
  auto matches = mgr_.findMatchingSignals(1, {"nonexistent"});
  EXPECT_TRUE(matches.empty());
}

// Returns empty when the specified layer doesn't exist.
TEST_F(OverlayManagerTest, FindMatchingSignals_WrongLayer) {
  mgr_.setBaseData(&base_data_);
  auto matches = mgr_.findMatchingSignals(99, {"velocity"});
  EXPECT_TRUE(matches.empty());
}

// --- timeOffset ---

// Newly created layers have zero time offset.
TEST_F(OverlayManagerTest, TimeOffset_DefaultZero) {
  mgr_.setBaseData(&base_data_);
  EXPECT_DOUBLE_EQ(mgr_.timeOffset(1), 0.0);
}

// setTimeOffset stores the value and timeOffset retrieves it.
TEST_F(OverlayManagerTest, SetTimeOffset_RoundTrip) {
  mgr_.setBaseData(&base_data_);
  mgr_.setTimeOffset(1, 5.5);
  EXPECT_DOUBLE_EQ(mgr_.timeOffset(1), 5.5);
}

// Querying a nonexistent layer returns zero.
TEST_F(OverlayManagerTest, TimeOffset_NonexistentLayer) {
  EXPECT_DOUBLE_EQ(mgr_.timeOffset(99), 0.0);
}

// resolveSignal includes the layer's time offset in the result.
TEST_F(OverlayManagerTest, ResolveSignal_WithTimeOffset) {
  mgr_.setBaseData(&base_data_);
  mgr_.setTimeOffset(1, 3.14);
  auto result = mgr_.resolveSignal("velocity");
  ASSERT_TRUE(result.has_value());
  EXPECT_DOUBLE_EQ(result->time_offset, 3.14);
}

// --- removeOverlay ---

// The base PJ layer cannot be removed.
TEST_F(OverlayManagerTest, RemoveOverlay_CannotRemoveBaseLayer) {
  mgr_.setBaseData(&base_data_);
  EXPECT_FALSE(mgr_.removeOverlay(1));
  EXPECT_EQ(mgr_.layerCount(), 1);
}

// --- layerByIndex ---

// Returns a pointer to the layer with the given index.
TEST_F(OverlayManagerTest, LayerByIndex_Valid) {
  mgr_.setBaseData(&base_data_);
  auto* layer = mgr_.layerByIndex(1);
  ASSERT_NE(layer, nullptr);
  EXPECT_EQ(layer->display_name, "PJ Data");
}

// Returns nullptr for an index that doesn't exist.
TEST_F(OverlayManagerTest, LayerByIndex_Invalid) {
  mgr_.setBaseData(&base_data_);
  EXPECT_EQ(mgr_.layerByIndex(99), nullptr);
}
