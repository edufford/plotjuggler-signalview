#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "PlotJuggler/plotdata.h"

namespace CsvUtil {
char detectDelimiter(const std::string& line);
}

struct OverlayLayer {
  int index = 0;             // 1-based layer number
  std::string display_name;  // "PJ Data" or filename
  std::string file_path;     // empty for PJ base layer
  double time_offset = 0.0;
  PJ::PlotDataMapRef* data = nullptr;  // points to PJ data or owned data
  std::unique_ptr<PJ::PlotDataMapRef>
      owned_data;  // non-null for overlay layers
};

struct ResolvedSignal {
  PJ::PlotData* series = nullptr;
  double time_offset = 0.0;
  int layer_index = 0;
};

/// Manages the base PJ data layer and any overlay file layers.
/// Provides unified signal name resolution with `#N/` prefix convention.
class OverlayManager {
 public:
  OverlayManager();

  /// Set the base PlotJuggler data source (layer 1 if non-empty).
  void setBaseData(PJ::PlotDataMapRef* data);

  /// Returns true if PJ provided a non-empty base data layer.
  bool hasBaseLayer() const;

  /// Load an overlay file (CSV). Returns the new layer index, or -1 on failure.
  int loadOverlayFile(const std::string& file_path);

  /// Remove an overlay layer by index. Returns true if removed.
  bool removeOverlay(int layer_index);

  /// Get all layers (read-only).
  const std::vector<OverlayLayer>& layers() const { return _layers; }

  /// Get a mutable layer by index.
  OverlayLayer* layerByIndex(int index);

  /// Number of layers currently loaded.
  int layerCount() const { return static_cast<int>(_layers.size()); }

  /// Whether there are any overlay layers (layers beyond a single base).
  bool hasOverlays() const;

  /// Resolve a (potentially prefixed) signal name to its PlotData and time
  /// offset.
  std::optional<ResolvedSignal> resolveSignal(
      const std::string& prefixed_name) const;

  /// Get all available signal names (prefixed) across all layers.
  std::vector<std::string> allAvailableSignals() const;

  /// For a given layer, find signals whose raw names match any of the given raw
  /// names.
  std::vector<std::string> findMatchingSignals(
      int layer_index, const std::vector<std::string>& raw_names) const;

  /// Get/set time offset for a layer.
  double timeOffset(int layer_index) const;
  void setTimeOffset(int layer_index, double offset);

  // --- Name parsing helpers ---
  struct ParsedName {
    int layer = 0;  // 0 means unprefixed
    std::string raw_name;
  };

  static ParsedName parsePrefixedName(const std::string& name);
  static std::string makePrefixedName(int layer, const std::string& raw_name);
  static std::string rawName(const std::string& prefixed_name);

 private:
  /// Parse a CSV file into a PlotDataMapRef. Returns nullptr on failure.
  std::unique_ptr<PJ::PlotDataMapRef> parseCSV(const std::string& file_path);

  /// Assign the next available layer index.
  int nextLayerIndex() const;

  PJ::PlotDataMapRef* _base_data = nullptr;
  std::vector<OverlayLayer> _layers;
};
