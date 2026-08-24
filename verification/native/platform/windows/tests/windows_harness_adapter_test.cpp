#include <axiom/verification/windows_harness_adapter.hpp>

#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

namespace avp = axiom::verification::platform;

namespace {
void require(bool condition) { if (!condition) std::abort(); }
}

void write_trace(const std::string& path, const avp::WindowsHarnessAdapter& adapter,
                 const avp::WindowsVerificationProfile& profile,
                 const avp::PointerSampleBatch& batch,
                 avp::PublishDisposition stale_disposition,
                 bool attached_after_rebind) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  require(output.good());
  output << "{\n"
         << "  \"schema_version\": 1,\n"
         << "  \"trace_kind\": \"verification-windows-native-adapter\",\n"
         << "  \"profile\": {\"profile_id\": \"" << profile.profile_id
         << "\", \"platform_family\": \"" << profile.platform_family
         << "\", \"backend\": \"" << profile.backend
         << "\", \"arc_enabled\": " << (profile.arc_enabled ? "true" : "false") << "},\n"
         << "  \"generations\": {\"metrics\": " << adapter.metrics_generation()
         << ", \"surface\": " << adapter.surface_generation()
         << ", \"device\": " << adapter.device_generation() << "},\n"
         << "  \"document_continuity\": {\"attached_after_rebind\": "
         << (attached_after_rebind ? "true" : "false")
         << ", \"semantic_revision\": " << adapter.semantic_revision() << "},\n"
         << "  \"pointer_batch\": {\"correlation_id\": \"" << batch.correlation_id
         << "\", \"count\": " << batch.samples.size() << "},\n"
         << "  \"arc_ownership\": {\"canonical_owner\": \""
         << adapter.target_ownership().canonical_owner << "\", \"preview_owner\": \""
         << adapter.target_ownership().preview_owner << "\", \"distinct\": "
         << (adapter.target_ownership().distinct ? "true" : "false") << "},\n"
         << "  \"native_surface_ready\": "
         << (adapter.native_surface_ready() ? "true" : "false") << ",\n"
         << "  \"destroy_stale\": {\"disposition\": \""
         << (stale_disposition == avp::PublishDisposition::DroppedStaleScope
                 ? "DROPPED_STALE_SCOPE"
                 : "UNEXPECTED")
         << "\", \"events_after_destroy\": " << adapter.events_after_destroy() << "}\n}\n";
  require(output.good());
}

int main(int argc, char** argv) {
  const auto profile = avp::windows_verification_profile();
  require(profile.profile_id == "windows-native-reference-v0.1");
  require(profile.platform_family == "WINDOWS");
  require(profile.backend == "D3D12");
  require(profile.arc_enabled);

  avp::WindowsHarnessAdapter adapter;
#ifdef _WIN32
  require(adapter.native_surface_ready());
#else
  require(!adapter.native_surface_ready());
#endif
  const auto semantic_before = adapter.semantic_revision();
  adapter.create_canvas();
  adapter.attach_host();
  adapter.attach_document();
  adapter.update_metrics({1280, 720, 2560, 1440, 2.0F, true, false});
  require(adapter.metrics_generation() == 2);
  require(adapter.semantic_revision() == semantic_before);

  const auto surface_before = adapter.surface_generation();
  adapter.surface_lost(surface_before);
  adapter.rebind_surface();
  require(adapter.surface_generation() == surface_before + 1);
  require(adapter.document_attached());
  const bool attached_after_rebind = adapter.document_attached();

  const auto device_before = adapter.device_generation();
  adapter.device_lost(device_before);
  adapter.device_recover();
  require(adapter.device_generation() == device_before + 1);
  require(adapter.semantic_revision() == semantic_before);

  const std::vector<avp::NativePointerHistory> history{
      {10.0F, 20.0F, 0.25F, 1.0F, 2.0F, 100},
      {11.0F, 21.0F, 0.5F, 2.0F, 3.0F, 101},
  };
  const auto batch = avp::normalize_windows_pointer_history("batch:1", history);
  require(batch.correlation_id == "batch:1");
  require(batch.samples.size() == 2);
  require(batch.samples[1].pressure == 0.5F);

  const auto ownership = adapter.target_ownership();
  require(ownership.canonical_owner == "AXIOM");
  require(ownership.preview_owner == "ARC");
  require(ownership.distinct);

  adapter.hold_present_completions();
  adapter.destroy_canvas();
  const auto stale = adapter.release_held_after_destroy();
  require(stale == avp::PublishDisposition::DroppedStaleScope);
  require(adapter.events_after_destroy() == 0);
  if (argc == 3 && std::string(argv[1]) == "--trace") {
    write_trace(argv[2], adapter, profile, batch, stale, attached_after_rebind);
  } else {
    require(argc == 1);
  }
  return 0;
}
