#pragma once
// Private, compile-time diagnostic only. No SDK accessor, callbacks, draw
// decisions, or GPU completion claims. Disabled macros evaluate no arguments.
#if defined(AXIOM_ENABLE_FRAME_TIMING)
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace canvas::foundation::internal::frame_timing {
enum class Stage : std::size_t {
    attempt, visibility, reference_build, reference_entries, reference_encode,
    reference_digest, presentation_adapt, frame_plan, render, acquire,
    validation, cache_begin, group_update, damage, cache_prepare, prepare_scan,
    cache_draw, coverage_record, scratch_draw, coverage_select, contour_compile, contour_record, contour_replay, contour_draw, cpu_coverage, gpu_coverage_submit, coverage_flush, material_submit,
    raster_snapshot, scene_draw, navigation_snapshot, provider_present,
    owner_pump,pending_owner,target_prepare,scene_stamp,history_prepare,publication_prepare,
    atomic_apply,apply_observer,scene_publish,transient_frame,navigation_frame,canonical_frame,resource_service,count
};
inline constexpr const char* names[]{
    "attempt","visibility","reference_build","reference_entries","reference_encode",
    "reference_digest","presentation_adapt","frame_plan","render","acquire",
    "validation","cache_begin","group_update","damage","cache_prepare","prepare_scan",
    "cache_draw","coverage_record","scratch_draw","coverage_select","contour_compile","contour_record","contour_replay","contour_draw","cpu_coverage","gpu_coverage_submit","coverage_flush","material_submit",
    "raster_snapshot","scene_draw","navigation_snapshot","provider_present",
    "owner_pump","pending_owner","target_prepare","scene_stamp","history_prepare","publication_prepare",
    "atomic_apply","apply_observer","scene_publish","transient_frame","navigation_frame","canonical_frame","resource_service"
};
enum class Counter : std::size_t { candidates, sources, encoded_bytes, plan_entries,
    prepare_knots, built_groups, coverage_tiles, coverage_pixels, gpu_interior_pixels, cpu_edge_pixels,
    prepare_direct, prepare_budget, exact_demand_reuse, source_points,
    compiled_contour_knots,record_candidates,record_replays,record_rejected,record_groups,record_selection_yields,
    checkpoint_replays,checkpoint_replay_knots,scalar_coverage_tiles,
    gpu_coverage_tiles,cpu_coverage_tiles,coverage_no_gpu_context,coverage_cpu_policy,coverage_target_fallback,
    pending_serial,pending_phase,pending_jobs,commit_ordinal,after_generation,after_scene,
    stamp_primitives,stamp_bytes,stamp_calls,atomic_start_ns,atomic_end_ns,
    resource_used,resource_pending_release,resource_gpu_epochs,render_pending,render_alloc_denials,
    body_builds,body_advances,body_compile,body_replay,mask_primitive_draws,mask_tile_resets,coverage_steps,count };
static_assert(sizeof(names)/sizeof(*names)==static_cast<std::size_t>(Stage::count));
inline constexpr const char* counterNames[]{"candidates","sources","encoded_bytes","plan_entries",
    "prepare_knots","built_groups","coverage_tiles","coverage_pixels","gpu_interior_pixels","cpu_edge_pixels",
    "prepare_direct","prepare_budget","exact_demand_reuse","source_points",
    "compiled_contour_knots","record_candidates","record_replays","record_rejected","record_groups","record_selection_yields",
    "checkpoint_replays","checkpoint_replay_knots","scalar_coverage_tiles",
    "gpu_coverage_tiles","cpu_coverage_tiles","coverage_no_gpu_context","coverage_cpu_policy","coverage_target_fallback",
    "pending_serial","pending_phase","pending_jobs","commit_ordinal","after_generation","after_scene",
    "stamp_primitives","stamp_bytes","stamp_calls","atomic_start_ns","atomic_end_ns",
    "resource_used","resource_pending_release","resource_gpu_epochs","render_pending","render_alloc_denials",
    "body_builds","body_advances","body_compile","body_replay","mask_primitive_draws","mask_tile_resets","coverage_steps"};
static_assert(sizeof(counterNames)/sizeof(*counterNames)==static_cast<std::size_t>(Counter::count));
struct Sample {std::uint64_t inclusive=0,exclusive=0,calls=0;};
struct Scope;
struct Context {
    std::array<Sample,static_cast<std::size_t>(Stage::count)> stages{};
    std::array<std::uint64_t,static_cast<std::size_t>(Counter::count)> counters{};
    Scope* top=nullptr;
};
inline thread_local Context* active=nullptr;
inline std::uint64_t now() noexcept {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}
struct Scope final {
    Context* context=active;
    Scope* parent=nullptr;
    Stage stage;
    std::uint64_t started=0,children=0;
    explicit Scope(Stage s) noexcept:stage(s){if(context){parent=context->top;context->top=this;started=now();}}
    Scope(const Scope&)=delete;
    ~Scope(){finish();}
    void finish() noexcept {
        if(!context)return;
        const auto elapsed=now()-started;
        auto& sample=context->stages[static_cast<std::size_t>(stage)];
        sample.inclusive+=elapsed;sample.exclusive+=elapsed-children;++sample.calls;
        if(parent)parent->children+=elapsed;
        context->top=parent;context=nullptr;
    }
};
inline void add(Counter counter,std::uint64_t value) noexcept {
    if(active)active->counters[static_cast<std::size_t>(counter)]+=value;
}
inline void set(Counter counter,std::uint64_t value) noexcept {
    if(active)active->counters[static_cast<std::size_t>(counter)]=value;
}
class Attempt final {
    Context context_{};
    Context* previous_=active;
    std::uint64_t frame_,semantic_,scene_,camera_,surface_,metrics_,view_,started_;
    bool settle_;
    const char* role_;
    const char* outcome_="rejected";
    // Root is initialized only after the context is installed.
    struct Install {explicit Install(Context& c){active=&c;}} install_{context_};
    Scope root_{Stage::attempt};
 public:
    Attempt(bool settle,std::uint64_t view,std::uint64_t frame,std::uint64_t semantic,
        std::uint64_t scene,std::uint64_t camera,std::uint64_t surface,std::uint64_t metrics,const char* role="canonical") noexcept
      :frame_(frame),semantic_(semantic),scene_(scene),camera_(camera),surface_(surface),metrics_(metrics),view_(view),started_(0),settle_(settle),role_(role){started_=root_.started;}
    Attempt(const Attempt&)=delete;
    void outcome(const char* value) noexcept {outcome_=value;}
    ~Attempt(){
        root_.finish();active=previous_;
        // Fixed-size numeric record, one write per attempt, outside timed scope.
        char record[16384];std::size_t offset=0;
        auto append=[&](const char* format,auto... args){
            if(offset>=sizeof(record))return;
            int n=0;
            if constexpr(sizeof...(args)==0)n=std::snprintf(record+offset,sizeof(record)-offset,"%s",format);
            else n=std::snprintf(record+offset,sizeof(record)-offset,format,args...);
            if(n>0)offset+=static_cast<std::size_t>(n);
        };
        append("AXIOM_FRAME_TIMING {\"version\":1,\"role\":\"%s\",\"settle\":%s,\"view\":\"%llu\",\"frame\":\"%llu\",\"semantic\":\"%llu\",\"scene\":\"%llu\",\"camera\":\"%llu\",\"surface\":\"%llu\",\"metrics\":\"%llu\",\"start_ns\":%.0f,\"outcome\":\"%s\",\"stages\":{",
            role_,settle_?"true":"false",static_cast<unsigned long long>(view_),static_cast<unsigned long long>(frame_),static_cast<unsigned long long>(semantic_),static_cast<unsigned long long>(scene_),static_cast<unsigned long long>(camera_),static_cast<unsigned long long>(surface_),static_cast<unsigned long long>(metrics_),double(started_),outcome_);
        bool first=true;
        for(std::size_t i=0;i<context_.stages.size();++i){const auto& s=context_.stages[i];if(!s.calls)continue;
            append("%s\"%s\":{\"inclusive_ns\":%.0f,\"exclusive_ns\":%.0f,\"calls\":%.0f}",first?"":",",names[i],double(s.inclusive),double(s.exclusive),double(s.calls));first=false;}
        append("},\"counts\":{");
        for(std::size_t i=0;i<context_.counters.size();++i)append("%s\"%s\":%.0f",i?",":"",counterNames[i],double(context_.counters[i]));
        append("}}\n");
        if(offset<sizeof(record))std::fwrite(record,1,offset,stderr);
        else std::fprintf(stderr,"AXIOM_FRAME_TIMING_LOSS record_truncated role=%s\n",role_);
    }
};
}
#define AXIOM_FRAME_TIMING_BEGIN(...) ::canvas::foundation::internal::frame_timing::Attempt axiomFrameTiming{__VA_ARGS__}
#define AXIOM_FRAME_TIMING_OUTCOME(value) axiomFrameTiming.outcome(value)
#define AXIOM_FRAME_TIMING_SCOPE(name,stage) ::canvas::foundation::internal::frame_timing::Scope name{::canvas::foundation::internal::frame_timing::Stage::stage}
#define AXIOM_FRAME_TIMING_END(name) name.finish()
#define AXIOM_FRAME_TIMING_ADD(counter,value) ::canvas::foundation::internal::frame_timing::add(::canvas::foundation::internal::frame_timing::Counter::counter,value)
#define AXIOM_FRAME_TIMING_SET(counter,value) ::canvas::foundation::internal::frame_timing::set(::canvas::foundation::internal::frame_timing::Counter::counter,value)
#else
#define AXIOM_FRAME_TIMING_BEGIN(...) ((void)0)
#define AXIOM_FRAME_TIMING_OUTCOME(value) ((void)0)
#define AXIOM_FRAME_TIMING_SCOPE(name,stage) ((void)0)
#define AXIOM_FRAME_TIMING_END(name) ((void)0)
#define AXIOM_FRAME_TIMING_ADD(counter,value) ((void)0)
#define AXIOM_FRAME_TIMING_SET(counter,value) ((void)0)
#endif
