#include "canvas/scene/full_scene_compiler.hpp"

namespace canvas::scene {
foundation::Result<RuntimeSceneProjection> FullSceneCompiler::compile(const semantic::SemanticReadView& view, text::RichTextLayoutService* textService, bool deferRoutes) {
    return projectSemanticScene(view,textService,deferRoutes);
}
} // namespace canvas::scene
