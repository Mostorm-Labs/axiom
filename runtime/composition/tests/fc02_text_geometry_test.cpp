#include "canvas/runtime/runtime_composition.hpp"
#include "canvas/text/text_layout.hpp"

#include <fstream>
#include <gtest/gtest.h>
#include <iterator>
using namespace canvas;
namespace {
struct Host {
    runtime::RuntimeComposition runtime;
    runtime::RuntimeFacade& facade = runtime.controlFacade();
    std::uint64_t request = 0;
    Host() {
        if (!runtime.open() || !runtime.bindSurface(256, 256))
            throw std::runtime_error("bind");
        semantic::ResourceManifest manifest;
        semantic::ResourceManifestEntry font;
        font.resource_id = semantic::ResourceId{foundation::ObjectId::fromUint64(500)};
        font.resource_kind = semantic::ResourceKind::kFont;
        font.resource_revision = 1;
        font.content_hash = "sha256:" + std::string(text::kRobotoSha256);
        font.decode_version = 1;
        font.layout_version = 1;
        font.metadata = {8, 1, 16, 0, 24, 1};
        std::ifstream file(FC02_LATIN_FONT, std::ios::binary);
        std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(file), {}};
        font.byte_length = bytes.size();
        manifest.entries = {font};
        if (!runtime.bindManifest(manifest, 1) ||
            !runtime.publishResource(font.resource_id.value, std::move(bytes)) ||
            !runtime.ownerPump())
            throw std::runtime_error("font");
    }
    runtime::TextReceipt intent(runtime::TextIntentKind kind) {
        runtime::TextIntent event;
        event.kind = kind;
        event.clientId = 15;
        event.requestId = ++request;
        event.target = facade.mountedCanvasTargets().front();
        event.token = facade.readTextEditor().token;
        event.viewX = 20;
        event.viewY = 30;
        auto r = facade.submitTextIntent(event);
        if (r.status != runtime::TextStatus::kQueued)
            return r;
        (void)runtime.ownerPump();
        return *facade.textIntentReceipt(15, request);
    }
    runtime::TextReceipt input(runtime::TextInputKind kind, std::u16string text = {}) {
        const auto s = facade.readTextEditor();
        runtime::TextInputEvent event;
        event.token = s.token;
        event.sequence = s.nextInputSequence;
        event.kind = kind;
        event.text = std::move(text);
        event.range = s.composition ? s.replacement : s.selection;
        event.composition = kind == runtime::TextInputKind::kBegin ? 1 : s.composition;
        event.finishEffect =
            kind == runtime::TextInputKind::kCommit || kind == runtime::TextInputKind::kFinishFailed
                ? s.finishEffect
                : 0;
        auto r = facade.submitTextInput(event);
        if (r.status != runtime::TextStatus::kQueued)
            return r;
        (void)runtime.ownerPump();
        return *facade.textInputReceipt(event.token, event.sequence);
    }
};
TEST(FC02Geometry, RealFontCaretPreeditFrameAndStaleGeometry) {
    Host h;
    ASSERT_EQ(h.intent(runtime::TextIntentKind::kCreate).status, runtime::TextStatus::kApplied);
    const auto empty = h.facade.readTextGeometry();
    ASSERT_TRUE(empty.available);
    ASSERT_EQ(h.input(runtime::TextInputKind::kBegin).status, runtime::TextStatus::kObserved);
    ASSERT_EQ(h.input(runtime::TextInputKind::kUpdate, u"preview").status,
              runtime::TextStatus::kObserved);
    const auto composed = h.facade.readTextGeometry();
    ASSERT_TRUE(composed.available);
    EXPECT_GT(composed.caret[0], empty.caret[0]);
    runtime::RuntimeFrameOpportunity frame;
    ASSERT_TRUE(h.runtime.serviceFrameOpportunity(false, frame));
    EXPECT_TRUE(frame.canonicalRequested);
    EXPECT_EQ(h.runtime.semanticObjectCount(), 0U);
    EXPECT_EQ(h.runtime.qualificationRuntime().submittedOperationCount(), 0U);
    std::vector<std::uint8_t> pixels(256 * 256 * 4);
    ASSERT_TRUE(h.runtime.readCanonicalRgba(pixels));
    bool ink = false;
    for (std::size_t i = 0; i < pixels.size(); i += 4)
        if (pixels[i] < 200 && pixels[i + 1] < 200 && pixels[i + 2] < 200)
            ink = true;
    EXPECT_TRUE(ink);
    ASSERT_TRUE(h.runtime.resizeSurface(300, 220));
    EXPECT_FALSE(h.facade.readTextGeometry(composed.token).available);
    EXPECT_TRUE(h.facade.readTextGeometry().available);
    EXPECT_EQ(h.facade.readTextEditor().token, composed.token.session);
    EXPECT_EQ(h.intent(runtime::TextIntentKind::kEscape).status, runtime::TextStatus::kObserved);
    EXPECT_EQ(h.runtime.semanticObjectCount(), 0U);
}
TEST(FC02Geometry, SharedGraphemeDeletionAndNativeCompletionFailure) {
    Host h;
    ASSERT_EQ(h.intent(runtime::TextIntentKind::kCreate).status, runtime::TextStatus::kApplied);
    ASSERT_EQ(h.input(runtime::TextInputKind::kReplace, u"A\u0301").status,
              runtime::TextStatus::kApplied);
    EXPECT_EQ(h.intent(runtime::TextIntentKind::kBackspace).status, runtime::TextStatus::kApplied);
    EXPECT_TRUE(h.facade.readTextEditor().projection.empty());
    EXPECT_EQ(h.runtime.semanticObjectCount(), 1U);
    ASSERT_EQ(h.input(runtime::TextInputKind::kBegin).status, runtime::TextStatus::kObserved);
    ASSERT_EQ(h.input(runtime::TextInputKind::kUpdate, u"preedit").status,
              runtime::TextStatus::kObserved);
    ASSERT_EQ(h.intent(runtime::TextIntentKind::kFinish).status, runtime::TextStatus::kQueued);
    const auto effect = h.facade.readTextEditor().finishEffect;
    const auto before = h.runtime.qualificationRuntime().submittedOperationCount();
    EXPECT_EQ(h.input(runtime::TextInputKind::kFinishFailed).status,
              runtime::TextStatus::kRejected);
    EXPECT_EQ(h.runtime.qualificationRuntime().submittedOperationCount(), before);
    EXPECT_EQ(h.facade.readTextEditor().preedit, u"preedit");
    ASSERT_EQ(h.intent(runtime::TextIntentKind::kFinish).status, runtime::TextStatus::kQueued);
    EXPECT_NE(h.facade.readTextEditor().finishEffect, effect);
    EXPECT_EQ(h.input(runtime::TextInputKind::kCommit, u"confirmed").status,
              runtime::TextStatus::kApplied);
    EXPECT_EQ(h.facade.readTextEditor().phase, runtime::TextPhase::kClosed);
}
TEST(FC02Geometry, CompleteCompositionKeepsEditorAndFinalizesReceipt) {
    Host h;
    using namespace runtime;
    ASSERT_EQ(h.intent(TextIntentKind::kCreate).status, TextStatus::kApplied);
    ASSERT_EQ(h.input(TextInputKind::kBegin).status, TextStatus::kObserved);
    ASSERT_EQ(h.input(TextInputKind::kUpdate, u"format").status, TextStatus::kObserved);
    ASSERT_EQ(h.intent(TextIntentKind::kCompleteComposition).status, TextStatus::kQueued);
    const auto request = h.request;
    ASSERT_EQ(h.input(TextInputKind::kCommit, u"format").status, TextStatus::kApplied);
    EXPECT_EQ(h.facade.readTextEditor().phase, TextPhase::kEditing);
    EXPECT_EQ(h.facade.textIntentReceipt(15, request)->status, TextStatus::kApplied);
    EXPECT_FALSE(h.facade.textIntentReceipt(15, request)->operation.isZero());
    const auto geometry = h.facade.readTextGeometry();
    ASSERT_TRUE(geometry.available);
    EXPECT_EQ(h.intent(TextIntentKind::kSelectAll).status, TextStatus::kObserved);
    EXPECT_FALSE(h.facade.readTextGeometry(geometry.token).available);
    EXPECT_FALSE(h.facade.readTextGeometry().selection.empty());
    EXPECT_EQ(h.intent(TextIntentKind::kSetWidth).status, TextStatus::kApplied);
    EXPECT_EQ(h.facade.readTextEditor().phase, TextPhase::kEditing);
    EXPECT_EQ(h.intent(TextIntentKind::kFinish).status, TextStatus::kApplied);
}
TEST(FC02Geometry, TextToolCreatesAndSelectsWithoutInkAndMissingGlyphIsExplicit) {
    Host h;
    using namespace runtime;
    CanvasControlRequest tool;
    tool.clientId = 19;
    tool.requestId = 1;
    tool.target = h.facade.mountedCanvasTargets().front();
    tool.payload.kind = CanvasControlPayloadKind::kSelectTool;
    tool.payload.tool = CanvasToolKind::kText;
    ASSERT_EQ(h.facade.submitCanvasControl(tool).state, ProductControlState::kQueued);
    ASSERT_TRUE(h.runtime.ownerPump());
    const auto pointer = [&](std::uint64_t seq, input::PointerPhase phase, float x) {
        input::PlatformPointerBatch b;
        b.samples.push_back({17,
                             9,
                             seq,
                             seq * 1000,
                             x,
                             30,
                             0,
                             0,
                             0,
                             {},
                             {},
                             input::SampleProvenance::kConfirmedCurrent,
                             phase});
        return h.runtime.submit(b);
    };
    ASSERT_TRUE(pointer(1, input::PointerPhase::kDown, 20));
    ASSERT_TRUE(pointer(2, input::PointerPhase::kUp, 20));
    EXPECT_EQ(h.facade.readTextEditor().phase, TextPhase::kEditing);
    EXPECT_EQ(h.runtime.semanticObjectCount(), 0U);
    ASSERT_EQ(h.input(TextInputKind::kReplace, u"Canvas").status, TextStatus::kApplied);
    ASSERT_TRUE(pointer(3, input::PointerPhase::kDown, 20));
    ASSERT_TRUE(pointer(4, input::PointerPhase::kMove, 65));
    ASSERT_TRUE(pointer(5, input::PointerPhase::kUp, 65));
    EXPECT_GT(h.facade.readTextEditor().selection.length, 0U);
    EXPECT_EQ(h.runtime.semanticObjectCount(), 1U);
    EXPECT_EQ(h.runtime.qualificationRuntime().submittedOperationCount(), 1U);
    ASSERT_EQ(h.input(TextInputKind::kReplace, u"\u4f60\u597d").status, TextStatus::kApplied);
    const auto g = h.facade.readTextGeometry();
    EXPECT_FALSE(g.available);
    ASSERT_FALSE(g.diagnostics.empty());
    EXPECT_EQ(g.diagnostics.front(), "font-glyph-unavailable");
    EXPECT_EQ(h.facade.readTextEditor().projection.substr(0, 2), u"\u4f60\u597d");
}

} // namespace
