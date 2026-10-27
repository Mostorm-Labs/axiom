#include "canvas/interaction/editor_history.hpp"
#include <cstdio>
using namespace canvas::semantic;
using namespace canvas::interaction;
struct Port final:HistorySubmitPort {
    unsigned submits=0;
    OperationId allocateOperationId() override{return {};}
    SubmitResult submit(std::span<const Operation>,ApplySource) override{++submits;return SubmitResult::rejected();}
};
int main(){
    Port port;EditorHistory history(port);
    const auto object=ObjectId::fromUint64(2);
    SectionContent before{{0,0,100,80},"before",4,16},after=before;after.title="after";
    Operation f{OperationId{ObjectId::fromUint64(100)},DocumentId{ObjectId::fromUint64(200)},1,1,
        SetSectionContentOp{{{object,1,before,after}},{}}};
    Operation u{OperationId{ObjectId::fromUint64(101)},f.document_id,1,1,
        SetSectionContentOp{{{object,1,after,before}},{}}};
    const bool metadata=history.prepareRecord(f,{u},{},{}).has_value();
    ObjectRecord wrapper;wrapper.id=object;wrapper.kind=ObjectKind::kSection;wrapper.kind_version=1;
    wrapper.placement.order_key=OrderKey({1});wrapper.content=before;
    EditContainerTopologyOp a,b;a.guard.child_sets={{{},{}}};a.effect=IntroduceWrappersEffect{{wrapper}};
    b.guard.nodes={{object,wrapper.kind,1,wrapper.placement,wrapper.transform}};
    b.guard.child_sets={{{},{{object,wrapper.placement.order_key}}},{object,{}}};b.effect=ReleaseWrappersEffect{{wrapper}};
    f.payload=a;u.payload=b;
    const bool topology=history.prepareRecord(f,{u},{},{}).has_value();
    std::printf("Actual baseline EditorHistory::prepareRecord SetSectionContent=%d EditContainerTopology=%d expected=1,1 history=%zu submits=%u\n",metadata,topology,history.size(),port.submits);
    return metadata&&topology?0:1;
}
