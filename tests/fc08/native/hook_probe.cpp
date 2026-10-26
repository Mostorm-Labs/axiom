#include "qualification_access.hpp"
#include <iostream>
#include <functional>
#include <set>
#include <utility>

namespace {
using namespace canvas;
using fc08::Json;
using Read = ink_playground::InkPlaygroundHistoryTestAccess;

Json snapshot(const runtime::RuntimeComposition& c) { return Read::snapshot(c.qualificationRuntime()); }
void open(runtime::RuntimeComposition& c) {
  if (!c.open() || !c.bindSurface(800,600)) throw std::logic_error("real owner open/bind failed");
}
bool create(runtime::RuntimeComposition& c, std::uint64_t n) {
  runtime::StickyCreateRequest request{};
  request.objectId=fc08::literal(n);
  request.content={100,100}; request.transform.tx=static_cast<double>((n-101)*150);
  return c.controlFacade().createSticky(request).state==runtime::ConnectorCommandState::kApplied;
}
bool undo(runtime::RuntimeComposition& c) {
  auto& facade=c.controlFacade(); const auto state=facade.readRuntimeState();
  const auto result=facade.undo(7001,state.identity.runtimeGeneration);
  return result.state==runtime::ProductControlState::kQueued && c.ownerPump();
}

#if defined(AXIOM_FC08_QUALIFICATION_HOOKS)
using qualification::Point;
class Trace final : public qualification::Observer {
 public:
  explicit Trace(std::function<Json()> read) : read_(std::move(read)) { events.get_ref<Json::array_t&>().reserve(32); }
  void observe(const qualification::Event& event) noexcept override {
    try {
      if(events.size()>=32) { overflow=true; return; }
      Json value{{"phase",qualification::pointNames[static_cast<unsigned>(event.point)]},
        {"owner_id",fc08::hex(event.ownerIdentity)},{"owner_epoch",event.ownerEpoch},
        {"operation_id",fc08::id(event.operation.value())},{"document_id",fc08::id(event.document.value())},
        {"resources",event.preparedResources},{"rejection_selected",event.rejectionSelected},
        {"owner_address",reinterpret_cast<std::uintptr_t>(event.owner)},
        {"generation",event.semanticGeneration?Json(*event.semanticGeneration):Json(nullptr)},
        {"commit_ordinal",event.commitOrdinal?Json(*event.commitOrdinal):Json(nullptr)},
        {"history_size",event.historySize?Json(*event.historySize):Json(nullptr)},
        {"history_cursor",event.historyCursor?Json(*event.historyCursor):Json(nullptr)},
        {"vector",read_()}};
      events.push_back(std::move(value));
    } catch (...) { observerError=true; }
  }
  Json events=Json::array();
  bool overflow=false,observerError=false;
 private:
  std::function<Json()> read_;
};
std::array<std::uint8_t,16> ownerId(std::uint8_t high=0x71) {
  auto value=fc08::literal(42).bytes; value[0]=high; return value;
}
Json scenario(std::optional<Point> fault) {
  runtime::RuntimeComposition c; open(c);
  if(!create(c,101)||!create(c,102)||!undo(c)) throw std::logic_error("valid real-owner redo-tail setup failed");
  auto trace=std::make_shared<Trace>([&c]{return snapshot(std::as_const(c));});
  auto registration=qualification::Access::bind(c,trace,ownerId(),1);
  const bool noPostFault=!registration.engine.arm(Point::kSemDone)&&!registration.history.arm(Point::kHistDone);
  if (fault) {
    bool armed=false;
    if(*fault==Point::kHistBeforeReserve||*fault==Point::kHistReady) armed=registration.history.arm(*fault);
    else if(*fault==Point::kCmdAdmitted) armed=registration.command.arm(*fault);
    else armed=registration.engine.arm(*fault);
    if(!armed) throw std::logic_error("qualification registration failed");
  }
  const auto before=snapshot(c);
  const bool accepted=create(c,103);
  const auto after=snapshot(c);
  return {{"fault",fault?Json(qualification::pointNames[static_cast<unsigned>(*fault)]):Json(nullptr)},
    {"accepted",accepted},{"before",before},{"after",after},{"events",trace->events},
    {"postcommit_fault_refused",noPostFault},{"trace_overflow",trace->overflow},
    {"observer_error",trace->observerError},{"fault_remains_armed",registration.engine.armed()||registration.history.armed()||registration.command.armed()}};
}
Json isolation() {
  runtime::RuntimeComposition a,b;open(a);open(b);
  auto ta=std::make_shared<Trace>([&a]{return snapshot(std::as_const(a));});
  auto tb=std::make_shared<Trace>([&b]{return snapshot(std::as_const(b));});
  auto ra=qualification::Access::bind(a,ta,ownerId(0x71),1);
  auto rb=qualification::Access::bind(b,tb,ownerId(0x72),2);
  if(!ra.engine.arm(Point::kSemPublish)) throw std::logic_error("arm A failed");
  const bool bAccepted=create(b,101),aStillArmed=ra.engine.armed();
  a.qualificationRuntime().cancelAllPointers();
  const bool cancelCleared=!ra.engine.armed();
  const bool aAccepted=create(a,101);
  if(!ra.history.arm(Point::kHistReady)) throw std::logic_error("rearm before close failed");
  const bool detached=a.stop()&&a.detach();
  const bool detachCleared=detached&&!ra.engine.arm(Point::kSemPublish);
  if(!a.close()) throw std::logic_error("close A failed");
  const bool closeCleared=!ra.history.armed()&&!ra.engine.arm(Point::kSemPublish);
  const bool bAfterClose=create(b,104);
  return {{"B_accepted_while_A_armed",bAccepted},{"A_fault_preserved_while_B_ran",aStillArmed},
    {"cancel_cleared",cancelCleared},{"A_accepted_after_cancel",aAccepted},{"close_cleared",closeCleared},
    {"detach_cleared",detachCleared},{"B_accepted_after_A_closed",bAfterClose},
    {"A_events",ta->events},{"B_events",tb->events}};
}
Json destroyedRegistration() {
  auto owner=std::make_unique<runtime::RuntimeComposition>();open(*owner);
  const auto* raw=owner.get();
  auto trace=std::make_shared<Trace>([raw]{return snapshot(*raw);});
  auto registration=qualification::Access::bind(*owner,trace,ownerId(0x73),3);
  const bool liveArm=registration.engine.arm(Point::kSemPublish);
  owner.reset();
  return {{"live_arm",liveArm},{"destroyed_arm_refused",!registration.engine.arm(Point::kSemPublish)},
    {"no_post_destroy_callback",trace->events.empty()},{"observer_reference_released",trace.use_count()==1}};
}
#endif
} // namespace

int main() {
  try {
    Json output{{"binding",false},{"phases",Json::array()}};
#if defined(AXIOM_FC08_QUALIFICATION_HOOKS)
    output["binding"]=true; output["scenarios"]=Json::array();
    output["scenarios"].push_back(scenario({}));
    for (const auto point:{Point::kSemStaged,Point::kSemReserved,Point::kSemPublish,
                          Point::kHistBeforeReserve,Point::kHistReady,Point::kCmdAdmitted})
      output["scenarios"].push_back(scenario(point));
    std::set<std::string> phases;
    for (const auto& item:output["scenarios"]) for(const auto& event:item["events"]) phases.insert(event["phase"].get<std::string>());
    output["phases"]=phases; output["isolation"]=isolation();output["lifetime"]=destroyedRegistration();
#else
    runtime::RuntimeComposition c;open(c);
    if(!create(c,101)||!create(c,102)||!undo(c)) throw std::logic_error("OFF predecessor setup failed");
    const auto before=snapshot(c);const bool accepted=create(c,103);const auto after=snapshot(c);
    output["passive"]={{"before",before},{"after",after},{"accepted",accepted}};
#endif
    std::cout << output.dump() << '\n';
    return 0;
  } catch (const std::exception& error) { std::cerr<<error.what()<<'\n';return 2; }
}
