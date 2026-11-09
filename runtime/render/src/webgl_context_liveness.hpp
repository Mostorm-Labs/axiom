#pragma once

namespace canvas::render::internal {

enum class WebGlContextLiveness { kAlive, kPhysicalLost, kUnknown };

// The pinned attributes API cannot distinguish a deleted registry entry from
// a live lost context. The drawing-buffer API independently validates both the
// registry entry and GLctx; its dimensions may legitimately be zero during loss.
// isContextLost alone is insufficient too: Emscripten returns true for deletion.
template<class DrawingBufferProbe, class LossProbe>
WebGlContextLiveness probeWebGlContext(
    int handle, DrawingBufferProbe drawingBuffer, LossProbe isLost) noexcept {
    int width=0,height=0;
    if(handle<=0||!drawingBuffer(handle,&width,&height))return WebGlContextLiveness::kUnknown;
    return isLost(handle)?WebGlContextLiveness::kPhysicalLost:WebGlContextLiveness::kAlive;
}

} // namespace canvas::render::internal
