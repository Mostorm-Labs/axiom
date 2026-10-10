#include "ink_playground_host.hpp"
#include "ink_playground_history_test_access.hpp"
#include "canvas/semantic/snapshot.hpp"
#include "../../../runtime/text/src/sha256.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
using Host=canvas::ink_playground::InkPlaygroundHost;
using Access=canvas::ink_playground::InkPlaygroundHistoryTestAccess;
using Json=nlohmann::json;
using Clock=std::chrono::steady_clock;
void require(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
std::string sha(std::span<const std::uint8_t> bytes) {
  canvas::text::internal::Sha256 hash; hash.Update(bytes); const auto value=hash.Finish();
  constexpr char hex[]="0123456789abcdef"; std::string output;
  for(auto b:value) { output+=hex[b>>4]; output+=hex[b&15]; } return output;
}
std::string semanticDigest(const Host& host) {
  auto encoded=canvas::semantic::SnapshotCodec::encode({
    canvas::semantic::DocumentId(canvas::foundation::ObjectId::fromUint64(1)),2,Access::objects(host)});
  require(encoded.ok(),"snapshot encoding failed"); return sha(encoded.bytes);
}
double percentile(std::vector<double> values,double p) {
  std::sort(values.begin(),values.end()); return values[static_cast<std::size_t>((values.size()-1)*p)];
}
Json replay(std::string_view scenario,std::size_t count,const std::filesystem::path& capture) {
  Host host; require(host.bindSurface(256,256),"bind failed");
  require(host.configureBundledTextResources(),"bundled fonts unavailable");
  const auto start=Clock::now(); require(host.seedTextScenario(scenario,count),"scenario insert failed");
  const auto seededMs=std::chrono::duration<double,std::milli>(Clock::now()-start).count();
  const auto initial=host.textLayoutMetrics(); const auto canonical=semanticDigest(host);
  Json actions=Json::array();
  auto checkpoint=[&](std::string_view action,const auto& run,bool relayout) {
    const auto before=host.textLayoutMetrics(); const auto canonicalBefore=semanticDigest(host);
    require(run(),"workload action failed"); const auto after=host.textLayoutMetrics();
    require(after.objectLayouts-before.objectLayouts==(relayout?1U:0U),"nonlocal text layout");
    if(!relayout && action=="camera") require(semanticDigest(host)==canonicalBefore,"camera mutated canonical");
    actions.push_back({{"action",action},{"object_layouts",after.objectLayouts-before.objectLayouts},
      {"paragraph_layouts",after.paragraphLayouts-before.paragraphLayouts}});
  };
  if(scenario=="text-edit-local") {
    for(const auto* action:{"insert","delete","split","merge","inline-style","paragraph-style"})
      checkpoint(action,[&]{return host.applyTextScenarioEdit(count/2,action);},true);
    for(int i=0;i<12;++i) checkpoint("repeated-insert",[&]{return host.editTextScenario(count/2);},true);
  }
  if(scenario=="text-transform") checkpoint("transform",[&]{return host.transformTextScenario(0);},false);
  if(scenario=="text-camera") checkpoint("camera",[&]{return host.applyViewportNavigation(
    {canvas::interaction::ViewportNavigationKind::kBrowserGesture,0,0,128,128,1.25F});},false);
  if(scenario=="text-font-cold-warm") {
    const auto generation=host.semanticGeneration(); const auto operations=host.submittedOperationCount();
    require(host.setTextFontsAvailable(false) && host.setTextFontsAvailable(true),"resource transition failed");
    require(host.semanticGeneration()==generation && host.submittedOperationCount()==operations &&
      semanticDigest(host)==canonical,"resource transition mutated canonical");
    actions.push_back({{"action","missing-ready"},{"object_layouts",count*2},{"canonical_mutation",false}});
  }
  std::vector<double> frameMs; std::string raster; Json query;
  const auto beforeFrames=host.textLayoutMetrics();
  for(int i=0;i<12;++i) {
    const auto frameStart=Clock::now();
    require(host.presentCanonicalFrame(host.canonicalFrameCount()+1,0,false),"frame rejected");
    frameMs.push_back(std::chrono::duration<double,std::milli>(Clock::now()-frameStart).count());
    std::vector<std::uint8_t> pixels(256*256*4);
    require(host.activeSurfaceProvider()->readbackRgba(pixels).code==canvas::render::BackendSubmissionCode::kAccepted,"readback rejected");
    const auto digest=sha(pixels); if(!raster.empty()) require(digest==raster,"warm frame raster drift"); raster=digest;
    if(i==0 && !capture.empty()) { std::ofstream out(capture,std::ios::binary); out.write(reinterpret_cast<const char*>(pixels.data()),pixels.size()); }
  }
  require(host.textLayoutMetrics().objectLayouts==beforeFrames.objectLayouts,"frame caused layout");
  const auto observation=Access::observeQualification(host);
  auto state=Json::parse(host.textQualificationJson());
  return {{"scenario",scenario},{"count",count},{"total_objects",host.qualificationObjectCount()},
    {"canonical_sha256",semanticDigest(host)},{"raster_sha256",raster},{"state",std::move(state)},
    {"actions",std::move(actions)},{"seed_layouts",initial.objectLayouts},{"seed_ms",seededMs},
    {"frame_ms",{{"p50",percentile(frameMs,.5)},{"p95",percentile(frameMs,.95)},{"p99",percentile(frameMs,.99)}}},
    {"visible_candidates",observation.candidatesExamined},{"physical_observation",false}};
}
}
int main(int argc,char** argv) {
  try {
    const std::filesystem::path output=argc>1?argv[1]:"g47-qualification";
    const std::size_t grid=argc>2?std::stoull(argv[2]):32;
    std::filesystem::create_directories(output); Json workloads=Json::array();
    for(const auto* scenario:{"text-many-small","text-long","text-style-mixed","text-edit-local",
      "text-font-cold-warm","text-transform","text-camera","structured-grid-proxy"}) {
      const auto count=std::string_view(scenario)=="structured-grid-proxy"?grid:
        std::string_view(scenario)=="text-long"?16U:128U;
      const auto first=replay(scenario,count,output/(std::string(scenario)+".rgba"));
      const auto second=replay(scenario,count,{});
      require(first["canonical_sha256"]==second["canonical_sha256"] && first["raster_sha256"]==second["raster_sha256"] &&
        first["state"]["layout_digests"]==second["state"]["layout_digests"],"independent replay drift");
      workloads.push_back(first); std::cout<<scenario<<" replay PASS count="<<count<<'\n';
    }
    std::ofstream(output/"playground-metrics.json")<<workloads.dump(2)<<'\n';
    Json manifest={{"format","g47-workload-corpus-v1"},{"profile",canvas::text::kTextStackProfile},
      {"layout_context",{{"width",128},{"revision",1}}},{"grid_total_objects",grid*2},
      {"fonts",{{"Roboto",canvas::text::kRobotoSha256},{"NotoCJK",canvas::text::kNotoSha256}}},
      {"workloads",Json::array()}};
    for(const auto& w:workloads) manifest["workloads"].push_back({{"scenario",w["scenario"]},
      {"canonical_sha256",w["canonical_sha256"]},{"raster_sha256",w["raster_sha256"]},{"independent_replays",2}});
    std::ofstream(output/"workload-corpus-manifest.json")<<manifest.dump(2)<<'\n';
    return 0;
  } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
