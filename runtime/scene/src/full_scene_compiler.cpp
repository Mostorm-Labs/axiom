#include "canvas/scene/full_scene_compiler.hpp"

namespace canvas::scene {
foundation::Result<RuntimeSceneProjection> FullSceneCompiler::compile(const semantic::SemanticReadView& view, text::RichTextLayoutService* textService) {
    return projectSemanticScene(view,textService);
}
} // namespace canvas::scene
