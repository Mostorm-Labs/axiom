#include "canvas/runtime/canvas_runtime_facade.hpp"
namespace canvas::runtime {
SectionSnapshot CanvasRuntimeFacade::readSection(foundation::ObjectId id) const {
    if(!ownerThread())return {};
    auto snapshot=runtime_.sectionSnapshot(id);snapshot.target=target();return snapshot;
}
}
