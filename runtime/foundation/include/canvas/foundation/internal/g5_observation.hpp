#pragma once
// Private qualification observations. No canonical owner or consumer SDK API.
#include <cstdint>
namespace canvas::foundation::internal::g5 {
enum class Kind : unsigned { Stage=1, Begin, End, Admission, Basis, Commit, Scene, Decision, Leaf, Counter, Submit, Complete, GpuSubmit, GpuComplete, GpuRetire, Terminal };
enum class Tag : unsigned { input=1, eraser_begin, eraser_up, exact_prepare, publication_prepare, semantic_apply, observer, history, scene_apply, canonical, transient, navigation, preview, reference_build, reference_encode, reference_digest, coverage, material, provider, lookup, geometry_build, geometry_replay, copy, target, pointer_identity, canonical_basis, canonical_commit, input_result, canonical_receipt, cached_pixels, erase_damage, mask, prepare, render_direct, record_commands, routed_sample, eraser_revision, history_prepare, history_finalize, pathops, navigation_basis, brush_revision, resource_lease, control_result, display_record, display_roster, composition_proof, exact_body, mask_history, mask_scene };
enum class Route : unsigned { none, canonical_prepared, canonical_direct, edit_plain, edit_mask_isolated, edit_mask_direct, erase_layers, erase_resident, erase_miss, nav_coarse_gpu, nav_coarse_cpu, nav_unprepared, nav_retained, preview_prefix, preview_tail, creation_canonical, creation_transient, recording };
enum class Purpose : unsigned { none, input, canonical, transient, navigation, preview, exact_query, export_oracle };
}
#if defined(AXIOM_G5_UR0_DIAGNOSTICS)
#define AXIOM_G5_PREPARED_GPU_OBSERVATION 1
#include <atomic>
#include <array>
#include <bit>
#include <initializer_list>
#include <memory>
namespace canvas::foundation::internal::g5 {
template<class Id> std::uint64_t word(const Id& id,unsigned part) noexcept {
 if constexpr(requires{id.bytes;}){std::uint64_t value=0;for(unsigned i=0;i<8;++i)value|=std::uint64_t(id.bytes[part*8+i])<<(i*8);return value;}
 else return word(id.value(),part);
}
struct Run;
inline std::uint64_t bits(double value)noexcept{return std::bit_cast<std::uint64_t>(value);}
struct Context {std::uint64_t event=0,job=0,attempt=0,basis=0,span=0;Route route=Route::none;Purpose purpose=Purpose::none;};
bool enabled() noexcept;
std::uint64_t nowNs() noexcept;
std::uint64_t nextId() noexcept;
Context context() noexcept;
class OwnerScope final {
 std::array<std::uint64_t,8> previous_{};bool previousValid_=false,bound_=false;
 public:
 OwnerScope()=default;
 void bind(std::initializer_list<std::uint64_t>)noexcept;
 ~OwnerScope();
};
std::uint64_t finishedJob(Purpose) noexcept;

void emit(Kind,unsigned,std::initializer_list<std::uint64_t>,bool critical=false) noexcept;
inline void emit(Kind k,Tag t,std::initializer_list<std::uint64_t> v,bool c=false) noexcept {emit(k,static_cast<unsigned>(t),v,c);}
template<class F> auto observedCall(Tag tag,Purpose purpose,F&& f) {
 if(!enabled())return f();
 try {
  auto result=f();
  if constexpr(requires{result.code;})emit(Kind::Terminal,tag,{finishedJob(purpose),1,static_cast<std::uint64_t>(result.code)},true);
  else emit(Kind::Terminal,tag,{finishedJob(purpose),0,result?1ULL:0ULL},true);
  return result;
 }catch(...){emit(Kind::Terminal,tag,{finishedJob(purpose),2},true);throw;}
}
class RouteScope final {Route previous_;public: explicit RouteScope(Route r) noexcept;~RouteScope();};
void observeTarget(std::uintptr_t target,unsigned actual,std::uint64_t width,std::uint64_t height,std::uint64_t format,bool requestedGpu) noexcept;
template<class T> auto* targetPointer(const T& value)noexcept{if constexpr(requires{value.get();})return value.get();else return value;}
class Scope final {
 Context previous_{};std::uint64_t id_=0,started_=0,children_=0;unsigned tag_=0;bool root_=false;
 Scope* previousScope_=nullptr;int aggregate_=-1;
 public:
 explicit Scope(Tag, Purpose purpose=Purpose::none) noexcept;
 ~Scope(){finish();}
 Scope(const Scope&)=delete;
 void finish() noexcept;
};
struct GpuToken final {
 std::shared_ptr<Run> run;Context origin{};std::uint64_t group=0,contextId=0,epoch=0;
 std::atomic<bool> completed{false};
 GpuToken()=default;
 GpuToken(std::shared_ptr<Run>,Context,std::uint64_t,std::uint64_t) noexcept;
 GpuToken(GpuToken&&) noexcept;
 GpuToken& operator=(GpuToken&&) noexcept;
 GpuToken(const GpuToken&)=delete;
};
GpuToken gpuSubmitted(std::uint64_t contextId,std::uint64_t epoch) noexcept;
GpuToken gpuPrepared(std::uint64_t contextId,std::uint64_t epoch) noexcept;
void gpuSubmissionObserved(GpuToken&,bool accepted) noexcept;
void gpuCompleted(GpuToken&,bool abandoned,bool physicalLost) noexcept;
void gpuRetired(GpuToken&,std::uint64_t epoch) noexcept;
// Ticket addresses are diagnostic lookup labels only. The bounded collector
// owns the association; no field or allocation is added to product tickets.
void gpuTicketPrepared(std::uintptr_t,std::uint64_t contextId,std::uint64_t epoch) noexcept;
void gpuTicketSubmitted(std::uintptr_t,bool accepted) noexcept;
void gpuTicketCompleted(std::uintptr_t,bool abandoned,bool physicalLost) noexcept;
void gpuTicketRetired(std::uintptr_t,std::uint64_t epoch) noexcept;
void gpuTicketDiscarded(std::uintptr_t,std::uint64_t reason) noexcept;
void gpuTicketCancelled(std::uintptr_t) noexcept;
int control(std::uint32_t opcode,std::uint32_t value) noexcept;
const char* bytes() noexcept;
std::uint32_t size() noexcept;
}
#define G5_OBSERVE(kind,tag,critical,...) do { if(::canvas::foundation::internal::g5::enabled()) ::canvas::foundation::internal::g5::emit(kind,tag,{__VA_ARGS__},critical); } while(false)
#define G5_SCOPE(name,tag,purpose) ::canvas::foundation::internal::g5::Scope name{tag,purpose}
#define G5_SCOPE_END(name) name.finish()
#define G5_CALL(tag,purpose,...) (::canvas::foundation::internal::g5::observedCall(tag,purpose,[&](){return (__VA_ARGS__);}))
#define G5_ROUTE(name,route) ::canvas::foundation::internal::g5::RouteScope name{::canvas::foundation::internal::g5::enabled()?(route) : ::canvas::foundation::internal::g5::Route::none}
#define G5_TARGET(surface,requested) do {if(::canvas::foundation::internal::g5::enabled()){const auto* g5Target=::canvas::foundation::internal::g5::targetPointer(surface);if(g5Target)::canvas::foundation::internal::g5::observeTarget(reinterpret_cast<std::uintptr_t>(g5Target),g5Target->recordingContext()?2U:1U,g5Target->width(),g5Target->height(),static_cast<std::uint64_t>(g5Target->imageInfo().colorType()),requested);}}while(false)
#define G5_CANVAS(g5CanvasValue,requested) do {if(::canvas::foundation::internal::g5::enabled()){auto* g5Surface=(g5CanvasValue).getSurface();if(g5Surface){G5_TARGET(g5Surface,requested);}else ::canvas::foundation::internal::g5::observeTarget(reinterpret_cast<std::uintptr_t>(&(g5CanvasValue)),3,0,0,0,requested);}}while(false)
#else
#define G5_OBSERVE(...) ((void)0)
#define G5_SCOPE(...) ((void)0)
#define G5_SCOPE_END(...) ((void)0)
#define G5_CALL(tag,purpose,...) (__VA_ARGS__)
#define G5_ROUTE(...) ((void)0)
#define G5_TARGET(...) ((void)0)
#define G5_CANVAS(...) ((void)0)
#endif
