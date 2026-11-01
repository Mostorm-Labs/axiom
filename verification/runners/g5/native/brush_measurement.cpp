// Internal component probe only. This is not an SDK/Skia/device performance run.
#include "canvas/ink/brush_package_catalog.hpp"
#include "canvas/ink/brush_session.hpp"
#include <nlohmann/json.hpp>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace canvas::ink;
using Clock = std::chrono::steady_clock;
using Json = nlohmann::json;
double ms(Clock::time_point start) {return std::chrono::duration<double,std::milli>(Clock::now()-start).count();}
void require(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
// Equality diagnostic, explicitly not a cryptographic authority digest. The
// exact executable/source/output receive SHA256 identities in the outer runner.
std::string geometryDigest(const BrushCommitIntent& intent) {
  std::uint64_t hash=14695981039346656037ULL;
  auto add=[&](double value){auto bits=std::bit_cast<std::uint64_t>(value);for(unsigned i=0;i<8;++i){hash^=(bits>>(i*8))&255;hash*=1099511628211ULL;}};
  for(const auto& p:intent.outline){add(p.x);add(p.y);}
  for(const auto& k:intent.knots){add(k.x);add(k.y);add(k.pressure);}
  std::ostringstream out;out<<std::hex<<std::setfill('0')<<std::setw(16)<<hash;return out.str();
}
std::vector<BrushSample> fixture(std::size_t count) {
  std::vector<BrushSample> points;points.reserve(count);
  std::uint32_t random=42;
  for(std::size_t i=0;i<count;++i){
    random=random*1664525U+1013904223U;
    points.push_back({30.+static_cast<double>(i)/static_cast<double>(count-1)*700.,
      220.+std::sin(static_cast<double>(i)*.08)*90.+(static_cast<double>(random)/4294967296.-.5),
      .5,false,i+1,(i+1)*4'000'000ULL,false});
  }
  return points;
}
Json measure(const std::string& profile,std::size_t count,std::size_t batch) {
  const auto loadStart=Clock::now();
  const auto loaded=BrushPackageCatalog{}.loadDefault(profile,1);
  require(static_cast<bool>(loaded),"package load failed");
  require(profile!="laser-transient", "persistent component corpus excludes transient laser");
  const auto state=resolveBrushState(loaded.package,42);
  const auto loadMs=ms(loadStart);
  auto points=fixture(count);
  BrushSession session(1,state);require(session.begin(),"begin failed");
  std::vector<double> appendMs;
  BrushPreviewDelta preview;
  for(std::size_t offset=0;offset<count;offset+=batch){
    const auto size=std::min(batch,count-offset);
    const auto start=Clock::now();
    const auto code=session.append(std::span(points).subspan(offset,size),{},preview);
    appendMs.push_back(ms(start));require(code==BrushSessionError::kNone,"append failed");
  }
  BrushCommitIntent sealed;
  const auto sealStart=Clock::now();const auto code=session.seal(sealed);const auto sealMs=ms(sealStart);
  require(code==BrushSessionError::kNone,"seal failed");
  // Fresh independent delivery partition: seal should be equal even though
  // the replay uses one batch. Replay is measured separately from timed append.
  BrushSession replay(2,state);require(replay.begin(),"replay begin failed");
  BrushPreviewDelta replayPreview;BrushCommitIntent replayIntent;
  const auto replayStart=Clock::now();
  require(replay.append(points,{},replayPreview)==BrushSessionError::kNone,"replay append failed");
  require(replay.seal(replayIntent)==BrushSessionError::kNone,"replay seal failed");
  const auto replayMs=ms(replayStart);
  const auto metrics=session.metrics();
  const auto compactMetrics=session.compactMetrics();
  return {{"profile",profile},{"revision",1},{"package_digest",loaded.canonicalDigest},
    {"samples",count},{"batch_size",batch},
    {"confirmed_samples",loaded.package.compactConfig?compactMetrics.samples:sealed.confirmed.size()},
    {"confirmed_count_source",loaded.package.compactConfig?"CompactBrushSession actual confirmed input visits":"sealed.confirmed.size"},
    {"append_ms",appendMs},{"seal_ms",sealMs},{"replay_ms",replayMs},{"load_resolve_ms",loadMs},
    {"outline_points",sealed.outline.size()},{"knots",sealed.knots.size()},
    {"seal_digest",geometryDigest(sealed)},{"replay_digest",geometryDigest(replayIntent)},
    {"equality_digest_algorithm","FNV1a64 over little-endian double geometry; diagnostic only"},
    {"compact_work",{{"input_visits",compactMetrics.samples},{"simplification_visits",compactMetrics.simplificationVisits},
       {"copied_knots",compactMetrics.copiedKnots},{"generated_knots",compactMetrics.generatedKnots}}},
    {"metrics",{{"append_evaluations",metrics.appendEvaluations},{"max_append_input_samples",metrics.maxAppendInputSamples},
      {"max_copied_historical_samples",metrics.maxCopiedHistoricalSamples},{"seal_evaluations",metrics.sealEvaluations}}}};
}
std::vector<std::string> split(const char* text){std::vector<std::string> out;std::istringstream in(text);std::string s;while(std::getline(in,s,','))out.push_back(s);return out;}
}
int main(int argc,char** argv) {
  try{
    require(argc==5,"usage: g5_brush_measurement sample-counts repeats batch-sizes profiles");
    const auto repeats=std::stoul(argv[2]);require(repeats>0&&repeats<=100,"invalid repeats");
    Json output={{"schema_version","g5-preparation-v1"},{"scope","BrushSession component only; no SDK/Scene/Skia/provider timing"},
      {"warmup",Json::array()},{"runs",Json::array()}};
    for(const auto& profile:split(argv[4])){
      output["warmup"].push_back(measure(profile,32,1));
      for(const auto& n:split(argv[1]))for(const auto& b:split(argv[3])){
        const auto count=std::stoul(n),batch=std::stoul(b);
        require(count>=2&&count<=65536&&batch>=1&&batch<=65536,"invalid workload");
        for(std::size_t repeat=0;repeat<repeats;++repeat){auto run=measure(profile,count,batch);run["repeat"]=repeat;output["runs"].push_back(std::move(run));}
      }
    }
    std::puts(output.dump(2).c_str());return 0;
  }catch(const std::exception& error){std::fprintf(stderr,"%s\n",error.what());return 2;}
}
