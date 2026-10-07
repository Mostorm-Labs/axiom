#include "ink_playground_host.hpp"
#include "ink_playground_history_test_access.hpp"
#include "canvas/ink/programmable_brush.hpp"
#include "canvas/render/skia_brush_renderer.hpp"
#include "canvas/semantic/snapshot.hpp"
#include "include/core/SkCanvas.h"
#include "include/core/SkSurface.h"
#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using Host = canvas::ink_playground::InkPlaygroundHost;
using Access = canvas::ink_playground::InkPlaygroundHistoryTestAccess;
using Phase = canvas::input::PointerPhase;
using Handle = canvas::render::HandleKind;
using Point = canvas::render::ScreenPoint;
using Json = nlohmann::json;
using Clock = std::chrono::steady_clock;
constexpr std::uint64_t kOffset = 1469598103934665603ULL;

void require(bool value, const char* message) {
  if (!value) throw std::runtime_error(message);
}
std::string digest(std::span<const std::uint8_t> bytes) {
  auto hash = kOffset;
  for (auto byte : bytes) { hash ^= byte; hash *= 1099511628211ULL; }
  std::ostringstream out; out << std::hex << hash; return out.str();
}
std::string digest(const std::string& value) {
  return digest(std::span<const std::uint8_t>(
      reinterpret_cast<const std::uint8_t*>(value.data()), value.size()));
}
std::vector<std::uint8_t> semanticBytes(const Host& host) {
  // Codec is the existing canonical serialization owner; qualification only
  // reads the store via the existing test utility.
  const auto encoded = canvas::semantic::SnapshotCodec::encode({
      canvas::semantic::DocumentId(canvas::foundation::ObjectId::fromUint64(1U)),
      2U, Access::objects(host)});
  require(encoded.ok(), "canonical snapshot encoding unavailable");
  return encoded.bytes;
}
Point center(const Host& host) {
  require(host.selectionOverlay() != nullptr, "missing editing overlay");
  const auto& corners = host.selectionOverlay()->selectionOutline().corners;
  return {(corners[0].x + corners[2].x) * 0.5F,
          (corners[0].y + corners[2].y) * 0.5F};
}
Point handle(const Host& host, Handle kind) {
  require(host.selectionOverlay() != nullptr, "missing editing overlay");
  return host.selectionOverlay()->handle(kind).center;
}

struct Replay final {
  Host host;
  Json trace = Json::array();
  Json inputTrace = Json::array();
  std::vector<double> frameTimes;
  std::vector<double> queryTimes;
  std::string presentationDigest;
  std::uint64_t transientPeak = 0U;
  std::uint64_t selectedPeak = 0U;
  std::uint64_t guidePeak = 0U;

  explicit Replay(std::size_t objects) {
    require(host.bindSurface(1024U, 768U), "surface bind failed");
    require(host.seedQualificationFixture(objects), "fixture insert operation failed");
    host.setPlatformPresentationDeferred(true);
    inputTrace.push_back({{"action","fixture"},{"objects",objects},
        {"width",1024},{"height",768},{"layout","shape24-grid32-columns8-v1"}});
  }
  void frame(bool retire = false) {
    const auto start = Clock::now();
    require(host.presentCanonicalFrame(host.canonicalFrameCount()+1U, 0.0, retire),
            "canonical frame rejected");
    const double elapsed = std::chrono::duration<double, std::milli>(Clock::now()-start).count();
    frameTimes.push_back(elapsed);
    host.recordPresentation("qualification-measured-frame", host.pendingCanonicalHandoffCount(), elapsed);
    std::vector<std::uint8_t> pixels(1024U*768U*4U);
    require(host.activeSurfaceProvider()->readbackRgba(pixels).code ==
        canvas::render::BackendSubmissionCode::kAccepted, "canonical readback failed");
    presentationDigest = digest(pixels);
    selectedPeak = std::max(selectedPeak, static_cast<std::uint64_t>(host.selectedObjectCount()));
    transientPeak = std::max(transientPeak, static_cast<std::uint64_t>(host.transientTransformCount()));
    if (host.selectionOverlay()) guidePeak = std::max(guidePeak,
        static_cast<std::uint64_t>(host.selectionOverlay()->guides().size()));
    const auto observation=Access::observeQualification(host);
    queryTimes.push_back(observation.queryMs);
    inputTrace.push_back({{"action","frame"},{"retire_preview",retire}});
    trace.push_back({{"action","frame"},{"camera_generation",host.cameraGeneration()},
        {"scene_generation",host.sceneRevision()},{"operations",host.submittedOperationCount()},
        {"transient_transforms",host.transientTransformCount()},
        {"selected",host.selectedObjectCount()},{"pixels",presentationDigest},
        {"candidates_examined",observation.candidatesExamined},
        {"invalidation_rects",observation.invalidationRectCount},
        {"full_scene_invalidation",observation.fullSceneInvalidation}});
  }
  void select() {
    require(host.setSelectionMode(true) && host.selectAtViewPoint(16.0F,16.0F), "select failed");
    require(host.selectedObjectCount()==1U, "hit test did not select fixture object");
    const Json event={{"action","select"},{"x",16},{"y",16}};
    trace.push_back(event); inputTrace.push_back(event);
    frame();
  }
  void pointer(Phase phase, Point p) {
    require(host.selectionPointer(104U,phase,p.x,p.y), "transform pointer rejected");
    const Json event={{"action","pointer"},{"phase",static_cast<int>(phase)},
        {"x",p.x},{"y",p.y}};
    trace.push_back(event); inputTrace.push_back(event);
    frame();
  }
  void navigation(canvas::interaction::ViewportNavigationSample sample) {
    require(host.applyViewportNavigation(sample), "viewport navigation rejected");
    const Json event={{"action","navigation"},{"kind",static_cast<int>(sample.kind)},
        {"dx",sample.deltaX},{"dy",sample.deltaY},{"anchor_x",sample.anchorX},
        {"anchor_y",sample.anchorY},{"factor",sample.scaleDelta}};
    trace.push_back(event); inputTrace.push_back(event);
    frame();
  }
};

double percentile(std::vector<double> values, double fraction) {
  if (values.empty()) return 0.0;
  std::sort(values.begin(),values.end());
  const auto index = static_cast<std::size_t>(std::ceil(fraction*static_cast<double>(values.size())))-1U;
  return values[std::min(index,values.size()-1U)];
}

void transform(Replay& replay, Json& result, bool commit) {
  replay.select();
  // Zoom separates the frozen 12px hit areas on the 24x24 fixture, without
  // changing either the handle geometry or the input-routing policy.
  replay.navigation({canvas::interaction::ViewportNavigationKind::kBrowserGesture,
                     0,0,0,0,3.0F});
  const auto before = replay.host.submittedOperationCount();
  auto p = center(replay.host);
  require(replay.host.selectionOverlay()->hitTest(p)==Handle::kNone,"interior overlaps handle");
  const auto semanticBefore = digest(semanticBytes(replay.host));
  replay.pointer(Phase::kDown,p);
  const Point moved{p.x+40.0F,p.y+40.0F};
  replay.pointer(Phase::kMove,moved);
  require(digest(semanticBytes(replay.host))==semanticBefore,"transient move mutated canonical");
  replay.pointer(commit ? Phase::kUp : Phase::kCancel,moved);
  if (!commit) {
    require(digest(semanticBytes(replay.host))==semanticBefore,"cancel mutated canonical");
    result["transform_committed"]=0; return;
  }
  auto right=handle(replay.host,Handle::kRight);
  replay.pointer(Phase::kDown,right);
  replay.pointer(Phase::kMove,{right.x+25.0F,right.y});
  replay.pointer(Phase::kUp,{right.x+25.0F,right.y});
  auto rotation=handle(replay.host,Handle::kRotation);
  replay.pointer(Phase::kDown,rotation);
  replay.pointer(Phase::kMove,{rotation.x+30.0F,rotation.y+10.0F});
  replay.pointer(Phase::kUp,{rotation.x+30.0F,rotation.y+10.0F});
  result["transform_committed"]=replay.host.submittedOperationCount()-before;
  require(result["transform_committed"]==3U,"move/resize/rotate did not commit exactly three operations");
}

void inkLaser(Replay& replay, Json& result) {
  replay.select();
  replay.inputTrace.push_back({{"action","pen-laser-coexist"},{"pen_pointer",900},
      {"pen_profile","vector-solid-v1"},{"pen_samples",{{128,128,0.5,1},{160,148,0.8,2}}},
      {"laser_family","Laser"},{"laser_size",7},{"laser_opacity",0.8},
      {"laser_spacing",0.08},{"laser_seed",0x4600U},
      {"laser_samples",{{120,140,0.5,1},{160,160,0.8,2}}}});
  const auto before = replay.host.submittedOperationCount();
  require(replay.host.beginBrushSession(900U), "Pen session rejected");
  require(replay.host.appendBrushSample(900U,128,128,0.5,1U) &&
          replay.host.appendBrushSample(900U,160,148,0.8,2U), "Pen samples rejected");
  require(replay.host.previewActive(), "active Pen preview missing");
  replay.frame();
  canvas::ink::ResourceCatalog resources;
  canvas::ink::BrushDefinition definition;
  definition.definitionId=7U; definition.family=canvas::ink::BrushFamily::kLaser;
  definition.nominalSize=7.0F; definition.opacity=0.8F; definition.spacing=0.08F;
  const auto compiled = canvas::ink::BrushCompiler{}.compile(definition,
      {.pressure=true,.tilt=true,.shapeResource=true,.grainResource=true,.temporalTransient=true});
  require(static_cast<bool>(compiled), "existing Laser compiler rejected profile");
  canvas::ink::BrushRuntime laser(resources);
  const canvas::ink::BrushSessionId session{7001U};
  require(laser.begin(session,*compiled.program,0x4600U),"Laser session rejected");
  const std::array samples{canvas::ink::BrushInputSample{120,140,0.5F,0,0,1},
                           canvas::ink::BrushInputSample{160,160,0.8F,0,0,2}};
  const auto output=laser.append(session,samples);
  require(static_cast<bool>(output) && !output.preview.primitives.empty(),"Laser preview missing");
  const auto canonicalBefore=semanticBytes(replay.host);
  // Qualification composition reuses the common renderer on the same surface
  // as editing chrome. It does not invent a Laser brush catalog profile.
  auto* provider=replay.host.activeSurfaceProvider();
  const auto acquired=provider->acquire();
  require(acquired.code==canvas::render::SkiaSurfaceAcquireCode::kAcquired,"Laser acquire failed");
  canvas::render::internal::drawBrushPrimitivesToSkCanvas(
      *acquired.frame.surface->getCanvas(),output.preview.primitives);
  provider->release();
  require(provider->present().code==canvas::render::BackendSubmissionCode::kAccepted,"Laser present failed");
  std::vector<std::uint8_t> pixels(1024U*768U*4U);
  require(provider->readbackRgba(pixels).code==canvas::render::BackendSubmissionCode::kAccepted,"Laser readback failed");
  const auto combined=digest(pixels);
  require(combined!=replay.presentationDigest,"Laser did not change editing presentation pixels");
  const auto finished=laser.finish(session);
  require(static_cast<bool>(finished) && !finished.commit.canonicalMutation,"Laser mutated canonical");
  require(semanticBytes(replay.host)==canonicalBefore,"Laser changed host canonical bytes");
  replay.frame();
  require(combined!=replay.presentationDigest,"Laser did not disappear on next canonical frame");
  require(replay.host.finishBrushSession(900U),"Pen commit rejected");
  replay.frame(true);
  require(!replay.host.previewActive(),"Pen handoff left preview active");
  result["pen_commit_count"]=replay.host.submittedOperationCount()-before;
  result["laser_canonical_mutation"]=finished.commit.canonicalMutation;
  result["laser_preview_digest"]=combined;
  result["laser_primitive_count"]=output.preview.primitives.size();
  require(result["pen_commit_count"]==1U,"Pen did not commit exactly once");
}

Json run(const std::string& id, std::size_t objects, const std::filesystem::path& root) {
  Json result={{"id",id},{"objects",objects},{"selected_count",0},{"selected_peak",0},
      {"transform_committed",0},{"guide_count",0},{"handle_hits",0},
      {"laser_canonical_mutation",nullptr},{"pen_commit_count",0}};
  const auto started=Clock::now();
  try {
    Replay replay(objects);
    result["camera_generation_before"]=replay.host.cameraGeneration();
    const auto semanticGeneration=replay.host.semanticGeneration().value();
    if (id=="select-deselect") {
      replay.select(); replay.host.clearSelection();
      replay.inputTrace.push_back({{"action","clear-selection"}}); replay.frame();
      require(replay.host.selectedObjectCount()==0U && replay.host.selectionOverlay()==nullptr,"deselect left chrome");
    } else if (id=="multi-select") {
      replay.select(); require(replay.host.toggleSelectionAtViewPoint(48,16),"toggle selection failed");
      replay.inputTrace.push_back({{"action","toggle-selection"},{"x",48},{"y",16}});
      replay.frame(); require(replay.host.selectedObjectCount()==2U,"two-object selection missing");
      require(replay.host.toggleSelectionAtViewPoint(48,16),"toggle removal failed");
      replay.inputTrace.push_back({{"action","toggle-selection"},{"x",48},{"y",16}});
      replay.frame(); require(replay.host.selectedObjectCount()==1U,"toggle did not remove second object");
    } else if (id=="handle-hover") {
      replay.select(); replay.navigation({canvas::interaction::ViewportNavigationKind::kBrowserGesture,0,0,0,0,3});
      for (auto kind : {Handle::kTopLeft,Handle::kTop,Handle::kTopRight,Handle::kRight,
                        Handle::kBottomRight,Handle::kBottom,Handle::kBottomLeft,Handle::kLeft,Handle::kRotation}) {
        const auto p=handle(replay.host,kind);
        require(replay.host.selectionOverlay()->hitTest(p)==kind,"handle acquisition mismatch");
        result["handle_hits"]=result["handle_hits"].get<unsigned>()+1U;
        replay.trace.push_back({{"action","hit-test"},{"handle",static_cast<int>(kind)},{"x",p.x},{"y",p.y}});
        replay.inputTrace.push_back(replay.trace.back());
      }
    } else if (id=="move-resize-rotate" || id=="100K-transform-overlay") {
      transform(replay,result,true);
    } else if (id=="move-snap-guide") {
      replay.select(); replay.navigation({canvas::interaction::ViewportNavigationKind::kBrowserGesture,0,0,0,0,3});
      const auto p=center(replay.host);
      replay.pointer(Phase::kDown,p);
      replay.pointer(Phase::kMove,{p.x+24,p.y});
      require(replay.guidePeak>0U,"snap did not publish active guides");
      replay.pointer(Phase::kMove,{p.x+400,p.y+250});
      require(replay.host.selectionOverlay()->guides().empty(),
              "snap guides remained after leaving all targets");
      replay.pointer(Phase::kMove,{p.x+24,p.y});
      require(!replay.host.selectionOverlay()->guides().empty(),"snap guide did not re-engage");
      replay.pointer(Phase::kCancel,p);
      require(replay.host.selectionOverlay()->guides().empty(),"cancel left snap guides");
    } else if (id=="100K-active-ink-laser") {
      inkLaser(replay,result);
    } else {
      using Kind=canvas::interaction::ViewportNavigationKind;
      if (id=="slow-fast-pan-zoom-fit") {
        for (float delta : {1.0F,2.0F,4.0F,32.0F,64.0F}) replay.navigation({Kind::kWheelPan,delta,delta,0,0,1});
        replay.inputTrace.push_back({{"action","fit-content"}});
        require(replay.host.fitViewportToContent(),"fit after pan failed"); replay.frame();
      } else if (id=="continuous-zoom" || id=="zoom-settle") {
        const auto anchor=replay.host.viewToContent(320,240);
        for (unsigned i=0;i<8U;++i) {
          replay.navigation({Kind::kBrowserGesture,0,0,320,240,1.1F});
          const auto after=replay.host.viewToContent(320,240);
          require(std::abs(after.first-anchor.first)<0.001F &&
                  std::abs(after.second-anchor.second)<0.001F,"zoom anchor moved");
        }
        result["zoom_anchor_preserved"]=true;
        if (id=="zoom-settle") {
          const auto generation=replay.host.cameraGeneration(); const auto pixels=replay.presentationDigest;
          for (unsigned i=0;i<3U;++i) replay.frame();
          result["settled_camera_unchanged"]=generation==replay.host.cameraGeneration() && pixels==replay.presentationDigest;
          require(result["settled_camera_unchanged"].get<bool>(),"settled zoom changed camera/pixels");
        }
      } else if (id=="zoom-to-fit") {
        replay.inputTrace.push_back({{"action","fit-content"}});
        require(replay.host.fitViewportToContent(),"document fit failed"); replay.frame();
        const auto fit=replay.host.cameraGeneration();
        replay.inputTrace.push_back({{"action","fit-content"}});
        require(replay.host.fitViewportToContent(),"repeated fit failed");
        require(replay.host.cameraGeneration()==fit,"idempotent fit changed camera generation");
        // Select using the mapped first fixture point, then fit the selection.
        const auto view=replay.host.viewportGesture();
        require(replay.host.setSelectionMode(true) && replay.host.selectAtViewPoint(
            16*view.scale+view.translationX,16*view.scale+view.translationY),"selection after fit failed");
        replay.inputTrace.push_back({{"action","select"},{"x",16*view.scale+view.translationX},
            {"y",16*view.scale+view.translationY}});
        replay.inputTrace.push_back({{"action","fit-selection"}});
        require(replay.host.fitViewportToSelection(),"selection fit failed"); replay.frame();
      } else throw std::runtime_error("unknown scenario selector");
      require(replay.host.semanticGeneration().value()==semanticGeneration,"camera navigation mutated canonical");
    }
    result["selected_count"]=replay.host.selectedObjectCount();
    result["selected_peak"]=replay.selectedPeak;
    result["guide_count"]=replay.guidePeak;
    result["transient_transform_peak"]=replay.transientPeak;
    result["document_generation"]=replay.host.semanticGeneration().value();
    result["scene_generation"]=replay.host.sceneRevision();
    result["camera_generation_after"]=replay.host.cameraGeneration();
    result["overlay_updates"]=replay.host.selectionOverlayUpdates();
    result["canonical_operations"]=replay.host.submittedOperationCount();
    const auto observation=replay.host.qualificationObservation();
    const auto workloadObservation = Access::observeQualification(replay.host);
    result["candidates_examined"]=workloadObservation.candidatesExamined;
    result["fixture_candidates_examined"]=workloadObservation.candidatesExamined;
    result["snap_candidates_examined"]=replay.host.snapCandidateCount();
    result["invalidation_rects"]=workloadObservation.invalidationRectCount;
    result["full_scene_invalidation"]=workloadObservation.fullSceneInvalidation;
    result["fixture_invalidation_rects"]=workloadObservation.invalidationRectCount;
    result["fixture_full_scene_invalidation"]=workloadObservation.fullSceneInvalidation;
    result["semantic_digest"]=digest(semanticBytes(replay.host));
    result["presentation_digest"]=replay.presentationDigest;
    result["workload_input_digest"]=digest(replay.inputTrace.dump());
    result["replay_digest"]=digest(result.dump());
    result["frame_ms_samples"]=replay.frameTimes;
    result["frame_p50_ms"]=percentile(replay.frameTimes,0.5);
    result["frame_p95_ms"]=percentile(replay.frameTimes,0.95);
    result["frame_p99_ms"]=percentile(replay.frameTimes,0.99);
    result["query_ms_samples"]=replay.queryTimes;
    result["query_p95_ms"]=percentile(replay.queryTimes,0.95);
    result["query_ms"]=workloadObservation.queryMs;
    result["fixture_render_ms"]=observation.renderMs;
    std::ofstream(root/(id+"-trace.json")) << replay.trace.dump(2) << '\n';
    std::ofstream(root/(id+"-input.json")) << replay.inputTrace.dump(2) << '\n';
    result["passed"]=true;
  } catch (const std::exception& e) {
    result["passed"]=false; result["failure"]=e.what();
  }
  result["elapsed_ms"]=std::chrono::duration<double,std::milli>(Clock::now()-started).count();
  std::cout << id << ": " << (result["passed"].get<bool>()?"PASS":"FAIL") << std::endl;
  return result;
}
} // namespace

int main(int argc, char** argv) {
  const std::filesystem::path output=argc>1?argv[1]:"qualification-out";
  const std::string revision=argc>2?argv[2]:"working-tree";
  bool small=false; std::string selector;
  for (int i=3;i<argc;++i) {
    if (std::string(argv[i])=="--small") small=true;
    else selector=argv[i];
  }
  std::filesystem::create_directories(output);
  Json artifact={{"schema_version","GT-G4.6-C-P32-v2"},{"result_revision",revision},
      {"capability_profile","native-common-raster"},{"small_behavior_profile",small},{"runs",Json::array()}};
  for (const char* id : {"select-deselect","multi-select","handle-hover","move-resize-rotate",
      "move-snap-guide","slow-fast-pan-zoom-fit","continuous-zoom","zoom-settle","zoom-to-fit",
      "100K-transform-overlay","100K-active-ink-laser"}) {
    if (!selector.empty() && selector!=id) continue;
    const bool large=std::string(id).starts_with("100K");
    artifact["runs"].push_back(run(id,large&&!small?100000U:3U,output));
  }
  if (artifact["runs"].empty()) { std::cerr << "unknown selector\n"; return 2; }
  std::ofstream(output/"qualification-run.json") << artifact.dump(2) << '\n';
  const bool all=std::all_of(artifact["runs"].begin(),artifact["runs"].end(),
      [](const Json& r){return r["passed"].get<bool>();});
  return all?0:2;
}
