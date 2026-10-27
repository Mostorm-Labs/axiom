#include "canvas/interaction/canvas_editing_runtime.hpp"
#include "canvas/scene/hierarchy_geometry.hpp"
namespace canvas::interaction {
struct CanvasEditingRuntime::ConnectorPreviewRouting {
    std::uint64_t generation=0;
    semantic::ObjectId selected{};
    std::optional<semantic::Transform2D> transform;
    std::optional<semantic::ObjectRecord> creation;
    std::vector<semantic::ObjectRecord> source;
    std::vector<semantic::ObjectRecord> queue;
    std::size_t cursor=0;
    std::unique_ptr<scene::ConnectorRouteJob> job;
    std::unordered_map<semantic::ObjectId,scene::ConnectorGeometry,foundation::ObjectIdHash> results;
};
bool CanvasEditingRuntime::connectorPreviewRoutingPending() const noexcept {
    return connectorPreviewRouting_&&connectorPreviewRouting_->cursor<connectorPreviewRouting_->queue.size();
}
scene::ConnectorGeometry CanvasEditingRuntime::previewConnectorGeometry(const semantic::ObjectRecord& record) {
    const auto* transform=transformOverrides_.find(selection_.primary());
    std::optional<semantic::ObjectRecord> creation;
    if(creationActive()&&creationPreview().valid&&creationPreview().record.kind==semantic::ObjectKind::kConnector)
        creation=creationPreview().record;
    const std::optional<semantic::Transform2D> current=transform?std::optional(*transform):std::nullopt;
    if(!connectorPreviewRouting_||connectorPreviewRouting_->generation!=semanticGeneration_.current().value()||
       connectorPreviewRouting_->selected!=selection_.primary()||connectorPreviewRouting_->transform!=current||
       connectorPreviewRouting_->creation!=creation) {
        auto next=std::make_shared<ConnectorPreviewRouting>();next->generation=semanticGeneration_.current().value();
        next->selected=selection_.primary();next->transform=current;next->creation=creation;
        next->source=semanticObjects_.allObjects();
        if(transform)for(auto& object:next->source)if(object.id==next->selected) {
            semantic::Transform2D inverseParent{};
            if(object.placement.parent_id) {
                const auto* parent=sceneCoordinator_->runtimeScene().find(*object.placement.parent_id);
                if(!parent||!scene::inverseTransform(parent->transform,inverseParent))break;
            }
            object.transform=scene::composeTransform(inverseParent,*transform);
        }
        next->source=scene::projectWorldObjects(next->source);
        if(creation)next->queue.push_back(*creation);
        else for(const auto& object:next->source) {
            if(const auto* c=std::get_if<semantic::ConnectorContent>(&object.content);
               c&&c->routing==semantic::ConnectorRouting::kOrthogonal)next->queue.push_back(object);
        }
        if(connectorPreviewRouting_&&connectorPreviewRouting_->job)connectorPreviewRouting_->job->cancel();
        connectorPreviewRouting_=std::move(next);
    }
    const auto found=connectorPreviewRouting_->results.find(record.id);
    if(found!=connectorPreviewRouting_->results.end())return found->second;
    auto connector=record;
    for(const auto& world:connectorPreviewRouting_->source)if(world.id==record.id){connector=world;break;}
    auto route=*scene::connectorGeometry(connector,connectorPreviewRouting_->source,true);
    if(route.status!=runtime::ConnectorRouteStatus::kPending)
        connectorPreviewRouting_->results.emplace(record.id,route);
    return route;
}
unsigned CanvasEditingRuntime::serviceConnectorPreviewRouting(unsigned quanta) {
    if(!transformDrag_.active()&&!creationActive()) {connectorPreviewRouting_.reset();return 0;}
    if(!connectorPreviewRouting_)return 0;
    auto& preview=*connectorPreviewRouting_;
    unsigned used=0;
    while(used<quanta&&preview.cursor<preview.queue.size()) {
        const auto& object=preview.queue[preview.cursor];
        if(preview.results.contains(object.id)) {++preview.cursor;continue;}
        if(!preview.job)preview.job=std::make_unique<scene::ConnectorRouteJob>(object,preview.source);
        ++used;
        if(preview.job->advance()==runtime::ConnectorRouteStatus::kPending)continue;
        preview.results[object.id]=preview.job->result();preview.job.reset();++preview.cursor;
        canonicalPresentationDirty_=true;transientOverlayDirty_=true;
    }
    return used;
}
void CanvasEditingRuntime::cancelConnectorRouting() noexcept {
    if(connectorRoutingJob_) connectorRoutingJob_->cancel();
    connectorRoutingJob_.reset();connectorRoutingSource_.clear();connectorRoutingQueue_.clear();
    connectorRoutingCursor_=0;connectorRoutingPending_=false;
    connectorRoutingGeneration_=std::numeric_limits<std::uint64_t>::max();
    if(connectorPreviewRouting_&&connectorPreviewRouting_->job)connectorPreviewRouting_->job->cancel();
    connectorPreviewRouting_.reset();
}
bool CanvasEditingRuntime::serviceConnectorRouting() noexcept {
    if(closed_||std::this_thread::get_id()!=ownerThread_||publicationCallbackActive_)return false;
    const auto generation=semanticGeneration_.current();
    if(connectorRoutingGeneration_!=generation.value()) {
        cancelConnectorRouting();
        connectorRoutingSource_=scene::projectWorldObjects(semanticObjects_.allObjects());
        connectorRoutingGeneration_=generation.value();
        for(std::size_t i=0;i<connectorRoutingSource_.size();++i) {
            const auto* c=std::get_if<semantic::ConnectorContent>(&connectorRoutingSource_[i].content);
            if(c&&c->routing==semantic::ConnectorRouting::kOrthogonal)connectorRoutingQueue_.push_back(i);
        }
    }
    std::vector<std::pair<semantic::ObjectId,scene::ConnectorGeometry>> completed;
    // One live search arena, at most four quanta per owner opportunity. The
    // immutable source snapshot survives each yield; a new generation cancels
    // it before any further work or publication. Failed jobs never self-retry.
    const unsigned previewQuanta=serviceConnectorPreviewRouting(2);
    for(unsigned quantum=previewQuanta;quantum<4&&connectorRoutingCursor_<connectorRoutingQueue_.size();++quantum) {
        const auto& object=connectorRoutingSource_[connectorRoutingQueue_[connectorRoutingCursor_]];
        if(!connectorRoutingJob_)connectorRoutingJob_=std::make_unique<scene::ConnectorRouteJob>(object,connectorRoutingSource_);
        if(connectorRoutingJob_->advance()==runtime::ConnectorRouteStatus::kPending)continue;
        completed.emplace_back(object.id,connectorRoutingJob_->result());
        connectorRoutingJob_.reset();++connectorRoutingCursor_;
    }
    connectorRoutingPending_=connectorRoutingCursor_<connectorRoutingQueue_.size();
    if(!completed.empty()) {
        if(!sceneCoordinator_->publishConnectorRoutes(completed,generation)) {
            connectorRoutingCursor_-=completed.size();connectorRoutingPending_=true;
            return false;
        }
        canonicalPresentationDirty_=true;
    }
    return true;
}
}
