#pragma once

namespace canvas::runtime {

template <typename T>
struct FeatureDiagnosticsSection final {
  bool supported = false;
  bool available = false;
  T value{};
};

struct InkDiagnosticsSnapshot final {};
struct ShapeDiagnosticsSnapshot final {};
struct RichTextDiagnosticsSnapshot final {};
struct ConnectorDiagnosticsSnapshot final {};
struct SnapDiagnosticsSnapshot final {};
struct ImageDiagnosticsSnapshot final {};

struct FeatureDiagnosticsSnapshot final {
  FeatureDiagnosticsSection<InkDiagnosticsSnapshot> ink{};
  FeatureDiagnosticsSection<ShapeDiagnosticsSnapshot> shape{};
  FeatureDiagnosticsSection<RichTextDiagnosticsSnapshot> richText{};
  FeatureDiagnosticsSection<ConnectorDiagnosticsSnapshot> connector{};
  FeatureDiagnosticsSection<SnapDiagnosticsSnapshot> snap{};
  FeatureDiagnosticsSection<ImageDiagnosticsSnapshot> image{};
};

}  // namespace canvas::runtime
