#include <gtest/gtest.h>

#include "overlay_manager.h"

// --- parsePrefixedName ---

// Parse a standard "#1/name" prefix into layer=1 and raw_name="name".
TEST(ParsePrefixedName, ValidPrefix_Layer1) {
  auto p = OverlayManager::parsePrefixedName("#1/velocity");
  EXPECT_EQ(p.layer, 1);
  EXPECT_EQ(p.raw_name, "velocity");
}

// Parse a high layer number; slashes in the raw name are preserved.
TEST(ParsePrefixedName, ValidPrefix_Layer99) {
  auto p = OverlayManager::parsePrefixedName("#99/some/nested/path");
  EXPECT_EQ(p.layer, 99);
  EXPECT_EQ(p.raw_name, "some/nested/path");
}

// A valid prefix with nothing after the slash yields an empty raw_name.
TEST(ParsePrefixedName, ValidPrefix_EmptyRawName) {
  auto p = OverlayManager::parsePrefixedName("#1/");
  EXPECT_EQ(p.layer, 1);
  EXPECT_EQ(p.raw_name, "");
}

// A plain name without "#N/" is treated as unprefixed (layer 0).
TEST(ParsePrefixedName, Unprefixed_PlainName) {
  auto p = OverlayManager::parsePrefixedName("velocity");
  EXPECT_EQ(p.layer, 0);
  EXPECT_EQ(p.raw_name, "velocity");
}

// An empty string is unprefixed with empty raw_name.
TEST(ParsePrefixedName, Unprefixed_EmptyString) {
  auto p = OverlayManager::parsePrefixedName("");
  EXPECT_EQ(p.layer, 0);
  EXPECT_EQ(p.raw_name, "");
}

// A hash followed by digits but no slash is not a valid prefix.
TEST(ParsePrefixedName, Unprefixed_HashNoSlash) {
  auto p = OverlayManager::parsePrefixedName("#1velocity");
  EXPECT_EQ(p.layer, 0);
  EXPECT_EQ(p.raw_name, "#1velocity");
}

// Non-numeric text between # and / is not a valid prefix.
TEST(ParsePrefixedName, Unprefixed_HashSlashNoNumber) {
  auto p = OverlayManager::parsePrefixedName("#abc/velocity");
  EXPECT_EQ(p.layer, 0);
  EXPECT_EQ(p.raw_name, "#abc/velocity");
}

// Negative layer numbers are rejected (layers are 1-based).
TEST(ParsePrefixedName, Unprefixed_NegativeLayer) {
  auto p = OverlayManager::parsePrefixedName("#-1/velocity");
  EXPECT_EQ(p.layer, 0);
  EXPECT_EQ(p.raw_name, "#-1/velocity");
}

// Layer 0 is not valid (layers are 1-based).
TEST(ParsePrefixedName, Unprefixed_ZeroLayer) {
  auto p = OverlayManager::parsePrefixedName("#0/velocity");
  EXPECT_EQ(p.layer, 0);
  EXPECT_EQ(p.raw_name, "#0/velocity");
}

// A lone "#" is too short to contain a prefix.
TEST(ParsePrefixedName, Unprefixed_JustHash) {
  auto p = OverlayManager::parsePrefixedName("#");
  EXPECT_EQ(p.layer, 0);
  EXPECT_EQ(p.raw_name, "#");
}

// "#/" has no digits between # and /, so it is unprefixed.
TEST(ParsePrefixedName, Unprefixed_HashSlash) {
  auto p = OverlayManager::parsePrefixedName("#/");
  EXPECT_EQ(p.layer, 0);
  EXPECT_EQ(p.raw_name, "#/");
}

// --- makePrefixedName ---

// Formats layer + raw_name into "#N/raw_name".
TEST(MakePrefixedName, Basic) {
  EXPECT_EQ(OverlayManager::makePrefixedName(1, "velocity"), "#1/velocity");
  EXPECT_EQ(OverlayManager::makePrefixedName(42, "x/y/z"), "#42/x/y/z");
}

// An empty raw_name still produces a valid prefix form "#N/".
TEST(MakePrefixedName, EmptyRawName) {
  EXPECT_EQ(OverlayManager::makePrefixedName(1, ""), "#1/");
}

// --- rawName ---

// Strips the "#N/" prefix and returns just the raw signal name.
TEST(RawName, Prefixed) {
  EXPECT_EQ(OverlayManager::rawName("#3/accel"), "accel");
}

// An unprefixed name is returned unchanged.
TEST(RawName, Unprefixed) {
  EXPECT_EQ(OverlayManager::rawName("accel"), "accel");
}

// --- Round-trip ---

// makePrefixedName followed by parsePrefixedName recovers original values.
TEST(NameRoundTrip, MakeThenParse) {
  std::string prefixed = OverlayManager::makePrefixedName(7, "sensor/temp");
  auto parsed = OverlayManager::parsePrefixedName(prefixed);
  EXPECT_EQ(parsed.layer, 7);
  EXPECT_EQ(parsed.raw_name, "sensor/temp");
}

// parsePrefixedName followed by makePrefixedName recovers original string.
TEST(NameRoundTrip, ParseThenMake) {
  auto parsed = OverlayManager::parsePrefixedName("#5/motor_rpm");
  std::string rebuilt =
      OverlayManager::makePrefixedName(parsed.layer, parsed.raw_name);
  EXPECT_EQ(rebuilt, "#5/motor_rpm");
}
