#include "canvas/runtime/internal/runtime_composition_core.hpp"
#include "canvas/runtime/runtime_facade.hpp"
#include <cassert>
#include <iostream>

using namespace canvas;
static foundation::ObjectId id(std::uint64_t v){return foundation::ObjectId::fromUint64(v);}
static runtime::ConnectorSnapshot settled(runtime::RuntimeCompositionCore& c,runtime::RuntimeFacade& f,
                                           foundation::ObjectId object) {
    for(unsigned i=0;i<64;++i) {
        assert(c.ownerPump());
        const auto snapshot=f.readConnectorState(object);
        assert(snapshot.available);
        if(snapshot.routeStatus!=runtime::ConnectorRouteStatus::kPending)return snapshot;
    }
    assert(false&&"route must terminate within work budget");return {};
}
int main() {
    runtime::RuntimeCompositionCore c;assert(c.open()&&c.bindSurface(800,600));
    runtime::RuntimeFacade& f=c.controlFacade();
    runtime::StickyCreateRequest a{};a.objectId=id(100);a.content={80,80};a.transform.tx=20;a.transform.ty=100;
    auto b=a;b.objectId=id(101);b.transform.tx=400;
    assert(f.createSticky(a).state==runtime::ConnectorCommandState::kApplied);
    assert(f.createSticky(b).state==runtime::ConnectorCommandState::kApplied);
    runtime::ConnectorCreateRequest wire{};wire.objectId=id(102);
    wire.content.start.value=semantic::AttachedEndpoint{a.objectId,semantic::StablePortAnchor{2}};
    wire.content.end.value=semantic::AttachedEndpoint{b.objectId,semantic::StablePortAnchor{4}};
    wire.content.routing=semantic::ConnectorRouting::kOrthogonal;
    assert(f.createConnector(wire).state==runtime::ConnectorCommandState::kApplied);
    // The tool profiles are independent: a plain line is straight, a connection is orthogonal.
    const auto chooseTool=[&](runtime::CanvasCreationPreset preset,std::uint64_t sequence) {
        runtime::CanvasControlRequest q;q.target=f.mountedCanvasTargets().front();q.clientId=881;q.requestId=sequence;
        q.payload.kind=runtime::CanvasControlPayloadKind::kSelectCreationPreset;q.payload.creationPreset=preset;
        assert(f.submitCanvasControl(q).state==runtime::ProductControlState::kQueued && c.ownerPump());
    };
    chooseTool(runtime::CanvasCreationPreset::kLine,1);
    assert(f.readConnectorRoutingPreset()==semantic::ConnectorRouting::kStraight);
    chooseTool(runtime::CanvasCreationPreset::kArrow,2);
    assert(f.readConnectorRoutingPreset()==semantic::ConnectorRouting::kOrthogonal);
    const auto generation=c.readback().semanticGeneration;
    const auto snapshot=settled(c,f,wire.objectId);
    assert(snapshot.routeStatus==runtime::ConnectorRouteStatus::kReady);
    assert(snapshot.route.front().x==100&&snapshot.route.back().x==400);
    assert(snapshot.displayRoute.front().x==104 && snapshot.displayRoute.back().x==396);
    assert(snapshot.endArrow && !snapshot.startArrow);
    assert(c.readback().semanticGeneration==generation);

    // An unrelated obstacle invalidates the route without editing ConnectorContent.
    auto obstacle=a;obstacle.objectId=id(103);obstacle.transform.tx=200;
    assert(f.createSticky(obstacle).state==runtime::ConnectorCommandState::kApplied);
    const auto afterObstacle=c.readback().semanticGeneration;
    const auto avoiding=settled(c,f,wire.objectId);
    assert(avoiding.routeStatus==runtime::ConnectorRouteStatus::kReady);
    assert(avoiding.route.size()>=4);
    assert(avoiding.content==snapshot.content);
    assert(c.readback().semanticGeneration==afterObstacle);
    assert(avoiding.work.expanded<=8192&&avoiding.work.predicates<=65536);

    // Change routing once; derived publication is not another Undo step.
    runtime::ConnectorRoutingRequest change{wire.objectId,semantic::ConnectorRouting::kStraight,afterObstacle};
    assert(f.setConnectorRouting(change).historyChanged);
    assert(f.readConnectorState(wire.objectId).content.routing==semantic::ConnectorRouting::kStraight);
    auto state=f.readRuntimeState();
    assert(f.undo(901,state.identity.runtimeGeneration).state==runtime::ProductControlState::kQueued);
    assert(c.ownerPump());
    assert(settled(c,f,wire.objectId).content.routing==semantic::ConnectorRouting::kOrthogonal);
    state=f.readRuntimeState();
    assert(f.redo(902,state.identity.runtimeGeneration).state==runtime::ProductControlState::kQueued);
    assert(c.ownerPump());
    assert(f.readConnectorState(wire.objectId).content.routing==semantic::ConnectorRouting::kStraight);

    runtime::ConnectorRoutingRequest preset{};preset.routing=semantic::ConnectorRouting::kOrthogonal;
    const auto beforePreset=c.readback().semanticGeneration;
    assert(f.setConnectorRouting(preset).state==runtime::ConnectorCommandState::kApplied);
    assert(f.readConnectorRoutingPreset()==semantic::ConnectorRouting::kOrthogonal);
    assert(c.readback().semanticGeneration==beforePreset);

    // Invalid/no-route create fails atomically with a typed reason.
    auto invalid=wire;invalid.objectId=id(104);
    invalid.content.start.value=semantic::FreePointEndpoint{{230,130}};
    const auto count=c.semanticObjectCount();const auto before=c.readback().semanticGeneration;
    assert(f.createConnector(invalid).reason==runtime::ConnectorFailureReason::kNoRouteInDomain);
    assert(c.semanticObjectCount()==count&&c.readback().semanticGeneration==before);
    auto straight=invalid;straight.objectId=id(106);straight.content.routing=semantic::ConnectorRouting::kStraight;
    assert(f.createConnector(straight).state==runtime::ConnectorCommandState::kApplied);
    const auto beforeRejectedSwitch=c.readback().semanticGeneration;
    const auto rejection=f.setConnectorRouting({straight.objectId,semantic::ConnectorRouting::kOrthogonal,0});
    assert(rejection.reason==runtime::ConnectorFailureReason::kNoRouteInDomain);
    assert(c.readback().semanticGeneration==beforeRejectedSwitch);
    assert(f.readConnectorState(straight.objectId).content.routing==semantic::ConnectorRouting::kStraight);

    assert(f.setConnectorRouting({wire.objectId,semantic::ConnectorRouting::kOrthogonal,0}).historyChanged);
    auto blocking=a;blocking.objectId=id(105);blocking.transform.tx=90;
    assert(f.createSticky(blocking).state==runtime::ConnectorCommandState::kApplied);
    const auto blockedGeneration=c.readback().semanticGeneration;
    const auto blocked=settled(c,f,wire.objectId);
    assert(blocked.routeStatus==runtime::ConnectorRouteStatus::kNoRouteInDomain);
    for(unsigned idle=0;idle<8;++idle)assert(c.ownerPump());
    assert(f.readConnectorState(wire.objectId).work==blocked.work);
    assert(c.readback().semanticGeneration==blockedGeneration);
    state=f.readRuntimeState();
    assert(f.undo(903,state.identity.runtimeGeneration).state==runtime::ProductControlState::kQueued);
    assert(c.ownerPump());
    assert(settled(c,f,wire.objectId).routeStatus==runtime::ConnectorRouteStatus::kReady);
    // Object-scoped commands use connector-local endpoints and preserve the opposite end.
    auto degenerate=wire;degenerate.objectId=id(109);degenerate.content.end=degenerate.content.start;
    degenerate.content.routing=semantic::ConnectorRouting::kStraight;
    assert(f.createConnector(degenerate).reason==runtime::ConnectorFailureReason::kInvalidEndpoint);
    auto revision=c.readback().semanticGeneration;
    auto endpoint=runtime::ConnectorEndpointRequest{wire.objectId,false,wire.content.end,revision};
    assert(!f.setConnectorEndpoint(endpoint).historyChanged && c.readback().semanticGeneration==revision);
    endpoint.endpoint.value=semantic::FreePointEndpoint{{500,40}};
    assert(f.setConnectorEndpoint(endpoint).historyChanged && c.readback().semanticGeneration==revision+1);
    assert(f.setConnectorEndpoint(endpoint).reason==runtime::ConnectorFailureReason::kStale);
    auto revised=settled(c,f,wire.objectId);
    assert(revised.content.start==wire.content.start && revised.content.end==endpoint.endpoint);
    assert(f.setConnectorDecoration({wire.objectId,true,false,c.readback().semanticGeneration}).historyChanged);
    revised=settled(c,f,wire.objectId);
    assert(revised.startArrow && !revised.endArrow && f.readConnectorDefaults().endArrow);
    change.expectedSemanticGeneration=1;
    assert(f.setConnectorRouting(change).state==runtime::ConnectorCommandState::kRejected);
    assert(c.close());
    assert(f.setConnectorRouting(preset).reason==runtime::ConnectorFailureReason::kNotReady);
    std::cout<<"PASS FC05 public routing, invalidation, derived History, failure and lifecycle\n";
}
