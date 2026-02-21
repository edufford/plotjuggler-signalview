#include "overlay_manager.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

OverlayManager::OverlayManager() = default;

void OverlayManager::setBaseData(PJ::PlotDataMapRef* data) {
  _base_data = data;

  // If there's already a base layer entry, update its pointer
  for (auto& layer : _layers) {
    if (layer.file_path.empty() && !layer.owned_data) {
      layer.data = data;
      return;
    }
  }

  // Always create a base layer entry (PJ data may be empty at init and fill
  // later)
  if (data) {
    OverlayLayer base;
    base.index = 1;
    base.display_name = "PJ Data";
    base.data = data;
    _layers.insert(_layers.begin(), std::move(base));

    // Re-number if needed
    for (int i = 0; i < (int)_layers.size(); i++) _layers[i].index = i + 1;
  }
}

bool OverlayManager::hasBaseLayer() const {
  for (const auto& layer : _layers) {
    if (layer.file_path.empty() && !layer.owned_data) return true;
  }
  return false;
}

int OverlayManager::loadOverlayFile(const std::string& file_path) {
  auto data = parseCSV(file_path);
  if (!data || data->numeric.empty()) return -1;

  int new_index = nextLayerIndex();

  OverlayLayer layer;
  layer.index = new_index;
  layer.display_name = std::filesystem::path(file_path).filename().string();
  layer.file_path = file_path;
  layer.owned_data = std::move(data);
  layer.data = layer.owned_data.get();
  _layers.push_back(std::move(layer));

  return new_index;
}

bool OverlayManager::removeOverlay(int layer_index) {
  auto it = std::find_if(
      _layers.begin(), _layers.end(),
      [layer_index](const OverlayLayer& l) { return l.index == layer_index; });
  if (it == _layers.end()) return false;
  // Don't allow removing the PJ base layer
  if (!it->owned_data) return false;

  _layers.erase(it);
  return true;
}

OverlayLayer* OverlayManager::layerByIndex(int index) {
  for (auto& layer : _layers) {
    if (layer.index == index) return &layer;
  }
  return nullptr;
}

bool OverlayManager::hasOverlays() const {
  int overlay_count = 0;
  for (const auto& layer : _layers) {
    if (layer.owned_data) overlay_count++;
  }
  return overlay_count > 0;
}

std::optional<ResolvedSignal> OverlayManager::resolveSignal(
    const std::string& prefixed_name) const {
  auto parsed = parsePrefixedName(prefixed_name);

  if (parsed.layer == 0) {
    // Unprefixed name — treat as layer 1
    parsed.layer = 1;
  }

  for (const auto& layer : _layers) {
    if (layer.index != parsed.layer || !layer.data) continue;

    auto it = layer.data->numeric.find(parsed.raw_name);
    if (it != layer.data->numeric.end()) {
      ResolvedSignal result;
      result.series = &it->second;
      result.time_offset = layer.time_offset;
      result.layer_index = layer.index;
      return result;
    }
  }
  return std::nullopt;
}

std::vector<std::string> OverlayManager::allAvailableSignals() const {
  std::vector<std::string> result;
  bool use_prefix = _layers.size() > 1;

  for (const auto& layer : _layers) {
    if (!layer.data) continue;
    for (const auto& [name, _] : layer.data->numeric) {
      if (use_prefix)
        result.push_back(makePrefixedName(layer.index, name));
      else
        result.push_back(name);
    }
  }
  std::sort(result.begin(), result.end());
  return result;
}

std::vector<std::string> OverlayManager::findMatchingSignals(
    int layer_index, const std::vector<std::string>& raw_names) const {
  std::vector<std::string> matches;
  for (const auto& layer : _layers) {
    if (layer.index != layer_index || !layer.data) continue;
    for (const auto& raw : raw_names) {
      if (layer.data->numeric.count(raw))
        matches.push_back(makePrefixedName(layer_index, raw));
    }
  }
  return matches;
}

double OverlayManager::timeOffset(int layer_index) const {
  for (const auto& layer : _layers) {
    if (layer.index == layer_index) return layer.time_offset;
  }
  return 0.0;
}

void OverlayManager::setTimeOffset(int layer_index, double offset) {
  for (auto& layer : _layers) {
    if (layer.index == layer_index) {
      layer.time_offset = offset;
      return;
    }
  }
}

// --- Static name helpers ---

OverlayManager::ParsedName OverlayManager::parsePrefixedName(
    const std::string& name) {
  ParsedName result;
  // Expected format: "#N/raw_name"
  if (name.size() >= 3 && name[0] == '#') {
    auto slash = name.find('/', 1);
    if (slash != std::string::npos) {
      std::string num_str = name.substr(1, slash - 1);
      char* end = nullptr;
      long val = std::strtol(num_str.c_str(), &end, 10);
      if (end != num_str.c_str() && *end == '\0' && val > 0) {
        result.layer = static_cast<int>(val);
        result.raw_name = name.substr(slash + 1);
        return result;
      }
    }
  }
  // Unprefixed
  result.layer = 0;
  result.raw_name = name;
  return result;
}

std::string OverlayManager::makePrefixedName(int layer,
                                             const std::string& raw_name) {
  return "#" + std::to_string(layer) + "/" + raw_name;
}

std::string OverlayManager::rawName(const std::string& prefixed_name) {
  return parsePrefixedName(prefixed_name).raw_name;
}

// --- CSV parser ---

char CsvUtil::detectDelimiter(const std::string& line) {
  // Count occurrences of common delimiters
  int commas = 0, tabs = 0, semicolons = 0;
  for (char c : line) {
    if (c == ',')
      commas++;
    else if (c == '\t')
      tabs++;
    else if (c == ';')
      semicolons++;
  }
  if (tabs >= commas && tabs >= semicolons && tabs > 0) return '\t';
  if (semicolons >= commas && semicolons > 0) return ';';
  return ',';
}

std::unique_ptr<PJ::PlotDataMapRef> OverlayManager::parseCSV(
    const std::string& file_path) {
  std::ifstream file(file_path);
  if (!file.is_open()) return nullptr;

  auto data = std::make_unique<PJ::PlotDataMapRef>();

  // Read header line
  std::string header_line;
  if (!std::getline(file, header_line)) return nullptr;

  // Detect delimiter from header
  char delim = CsvUtil::detectDelimiter(header_line);

  // Parse header to get column names
  std::vector<std::string> col_names;
  {
    std::stringstream ss(header_line);
    std::string token;
    while (std::getline(ss, token, delim)) {
      // Trim whitespace
      auto start = token.find_first_not_of(" \t\r\n");
      auto end = token.find_last_not_of(" \t\r\n");
      if (start != std::string::npos)
        token = token.substr(start, end - start + 1);
      else
        token.clear();
      col_names.push_back(token);
    }
  }

  if (col_names.size() < 2) return nullptr;

  // First column is time. Create a PlotData entry for each remaining column.
  std::vector<PJ::TimeseriesMap::iterator> series_iters;
  for (size_t i = 1; i < col_names.size(); i++) {
    auto it = data->addNumeric(col_names[i]);
    series_iters.push_back(it);
  }

  // Parse data rows
  std::string line;
  while (std::getline(file, line)) {
    if (line.empty()) continue;

    std::vector<double> values;
    std::stringstream ss(line);
    std::string token;
    while (std::getline(ss, token, delim)) {
      // Trim whitespace
      auto start = token.find_first_not_of(" \t\r\n");
      auto end = token.find_last_not_of(" \t\r\n");
      if (start != std::string::npos)
        token = token.substr(start, end - start + 1);
      else
        token = "0";

      char* endp = nullptr;
      double val = std::strtod(token.c_str(), &endp);
      if (endp == token.c_str()) val = 0.0;  // parse failure
      values.push_back(val);
    }

    if (values.size() < 2) continue;

    double time = values[0];
    for (size_t i = 0; i < series_iters.size() && (i + 1) < values.size();
         i++) {
      series_iters[i]->second.pushBack(
          PJ::PlotData::Point(time, values[i + 1]));
    }
  }

  return data;
}

int OverlayManager::nextLayerIndex() const {
  int max_idx = 0;
  for (const auto& layer : _layers) max_idx = std::max(max_idx, layer.index);
  return max_idx + 1;
}
