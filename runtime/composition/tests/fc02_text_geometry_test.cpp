#include "canvas/runtime/runtime_composition.hpp"
#include "canvas/text/text_layout.hpp"
#include "host_font_resources.hpp"

#include <fstream>
#include <cmath>
#include <gtest/gtest.h>
#include <iterator>
#include <chrono>
#include <thread>
using namespace canvas;
namespace {
struct Host {
    runtime::RuntimeComposition runtime;
    runtime::RuntimeFacade& facade = runtime.controlFacade();
    std::uint64_t request = 0;
    explicit Host(bool hostFont = false) {
        if (!runtime.open() || !runtime.bindSurface(256, 256))
            throw std::runtime_error("bind");
        if (hostFont) {
            if (!minimal_runtime_host::bindHostTextFonts(runtime, FC02_HOST_FONT, FC02_FONT_CATALOG_ROOT))
                throw std::runtime_error("host font");
            return;
        }
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
TEST(FC02Geometry, IdleCaretBlinksWithoutEditingOrContinuousRedraw) {
    Host h(true);
    ASSERT_EQ(h.intent(runtime::TextIntentKind::kCreate).status, runtime::TextStatus::kApplied);
    const auto token = h.facade.readTextEditor().token;
    const auto geometry = h.facade.readTextGeometry();
    ASSERT_TRUE(geometry.available);
    runtime::RuntimeFrameOpportunity frame;
    ASSERT_TRUE(h.runtime.serviceFrameOpportunity(false, frame));
    std::vector<std::uint8_t> pixels(256 * 256 * 4);
    const auto darkPixels = [&] {
        EXPECT_TRUE(h.runtime.readCanonicalRgba(pixels));
        // Sample only the caret's center column and interior rows.  The
        // persisted editor frame must not count as a visible caret.
        const auto x = static_cast<int>(std::floor((geometry.caret[0] + geometry.caret[2]) * 0.5F));
        const auto top = static_cast<int>(std::ceil(geometry.caret[1])) + 1;
        const auto bottom = static_cast<int>(std::floor(geometry.caret[5])) - 1;
        std::size_t count = 0;
        for (int y = top; y <= bottom; ++y)
            if (x >= 0 && y >= 0 && x < 256 && y < 256) {
                const auto i = (static_cast<std::size_t>(y) * 256 + static_cast<std::size_t>(x)) * 4;
                // The caret is blue-black. The achromatic frame may cover
                // half of the same pixel, but cannot satisfy this signature.
                if (pixels[i] < 200 && pixels[i + 1] > pixels[i] + 5 &&
                    pixels[i + 2] > pixels[i + 1] + 5) ++count;
            }
        return count;
    };
    ASSERT_GT(darkPixels(), 0U) << "The empty editor must initially show its actual shaped caret";
    bool hidden = false, restored = false;
    std::size_t redraws = 0;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (std::chrono::steady_clock::now() < deadline && !restored) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        ASSERT_TRUE(h.runtime.serviceFrameOpportunity(false, frame));
        if (!frame.canonicalRequested) continue;
        ++redraws;
        const auto count = darkPixels();
        if (count == 0) hidden = true;
        if (hidden && count > 0) restored = true;
    }
    EXPECT_TRUE(hidden) << "Idle caret never disappeared; Runtime must schedule its blink frame";
    EXPECT_TRUE(restored) << "Caret must reappear without input";
    EXPECT_LE(redraws, 6U) << "Caret animation must not redraw the canvas every owner pump";
    EXPECT_EQ(h.facade.readTextEditor().token, token);
    EXPECT_EQ(h.facade.readTextEditor().phase, runtime::TextPhase::kEditing);
    EXPECT_EQ(h.runtime.semanticObjectCount(), 0U);
    EXPECT_EQ(h.runtime.qualificationRuntime().submittedOperationCount(), 0U);
    ASSERT_EQ(h.intent(runtime::TextIntentKind::kFinish).status, runtime::TextStatus::kApplied);
    ASSERT_TRUE(h.runtime.serviceFrameOpportunity(false, frame));
    EXPECT_EQ(darkPixels(), 0U);
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

TEST(FC02Geometry, HostFontChinesePreeditAndCanonicalUseActualGlyphs) {
    Host h(true);
    using namespace runtime;
    const std::u16string sample = u"\u4f60\u597d\uff0c\u4e16\u754c\uff01 Axiom "
                                  u"\u4e2d\u6587\u8f93\u5165\u6d4b\u8bd5 123 \u7e41\u9ad4\u9f8d";
    ASSERT_EQ(h.intent(TextIntentKind::kCreate).status, TextStatus::kApplied);
    ASSERT_EQ(h.input(TextInputKind::kBegin).status, TextStatus::kObserved);
    ASSERT_EQ(h.input(TextInputKind::kUpdate, sample).status, TextStatus::kObserved);
    const auto preedit = h.facade.readTextGeometry();
    ASSERT_TRUE(preedit.available) << "Host font must shape full Chinese/Latin preedit";
    EXPECT_TRUE(preedit.diagnostics.empty());
    EXPECT_EQ(h.runtime.semanticObjectCount(), 0U);
    EXPECT_EQ(h.runtime.qualificationRuntime().submittedOperationCount(), 0U);
    ASSERT_EQ(h.input(TextInputKind::kCommit, sample).status, TextStatus::kApplied);
    EXPECT_EQ(h.facade.readTextEditor().projection, sample);
    EXPECT_TRUE(h.facade.readTextGeometry().available);
    EXPECT_EQ(h.runtime.semanticObjectCount(), 1U);
    EXPECT_EQ(h.runtime.qualificationRuntime().submittedOperationCount(), 1U);
    runtime::RuntimeFrameOpportunity frame;
    ASSERT_TRUE(h.runtime.serviceFrameOpportunity(false, frame));
    std::vector<std::uint8_t> pixels(256 * 256 * 4);
    ASSERT_TRUE(h.runtime.readCanonicalRgba(pixels));
    std::size_t dark = 0;
    for (std::size_t i = 0; i < pixels.size(); i += 4)
        if (pixels[i] < 200 && pixels[i + 1] < 200 && pixels[i + 2] < 200)
            ++dark;
    EXPECT_GT(dark, 100U);
    EXPECT_EQ(h.intent(TextIntentKind::kFinish).status, TextStatus::kApplied);
}
TEST(FC02Geometry, TextFrameShowsWrapExtentAfterEditingEnds) {
    Host h(true);
    ASSERT_EQ(h.intent(runtime::TextIntentKind::kCreate).status, runtime::TextStatus::kApplied);
    ASSERT_EQ(h.input(runtime::TextInputKind::kReplace, u"A").status, runtime::TextStatus::kApplied);
    runtime::TextIntent width;
    width.clientId = 15;
    width.requestId = ++h.request;
    width.target = h.facade.mountedCanvasTargets().front();
    width.token = h.facade.readTextEditor().token;
    width.kind = runtime::TextIntentKind::kSetWidth;
    width.width = 180;
    ASSERT_EQ(h.facade.submitTextIntent(width).status, runtime::TextStatus::kQueued);
    ASSERT_TRUE(h.runtime.ownerPump());
    ASSERT_EQ(h.intent(runtime::TextIntentKind::kFinish).status, runtime::TextStatus::kApplied);
    const auto operations = h.runtime.qualificationRuntime().submittedOperationCount();
    runtime::RuntimeFrameOpportunity frame;
    ASSERT_TRUE(h.runtime.serviceFrameOpportunity(false, frame));
    std::vector<std::uint8_t> pixels(256 * 256 * 4);
    ASSERT_TRUE(h.runtime.readCanonicalRgba(pixels));
    std::size_t dark = 0;
    // Text starts at x=20 with width=180. Its right border is far from glyph A.
    for (std::size_t y = 30; y < 50; ++y)
        for (std::size_t x = 199; x <= 201; ++x) {
            const auto i = (y * 256 + x) * 4;
            if (pixels[i] < 128 && pixels[i + 1] < 128 && pixels[i + 2] < 128) ++dark;
        }
    EXPECT_GT(dark, 8U) << "A black frame must show the full persisted wrap width";
    EXPECT_EQ(h.runtime.semanticObjectCount(), 1U);
    EXPECT_EQ(h.runtime.qualificationRuntime().submittedOperationCount(), operations);
}

TEST(FC02Geometry, CjkCaretStaysInsideItsLineAndTextFrame) {
    runtime::MemoryResourceProvider resources;
    const semantic::ResourceId font{foundation::ObjectId::fromUint64(0xFC0202)};
    std::ifstream file(FC02_HOST_FONT, std::ios::binary);
    resources.publish(font.value, {std::istreambuf_iterator<char>(file), {}});
    text::RichTextLayoutService service(resources, {300, 1});
    service.registerFont({font, std::string(minimal_runtime_host::kHostTextFontSha256), 0});
    const auto object = foundation::ObjectId::fromUint64(701);
    for (const double size : {16.0, 32.0}) {
        for (const bool bulleted : {false, true}) {
            semantic::TextStyle style{font, size, 400, false, false, {0, 0, 0, 1}};
            semantic::RichTextContent content;
            content.layout = semantic::RichTextLayout{300};
            content.document.paragraphs = {
                {foundation::ObjectId::fromUint64(702),
                 {semantic::ParagraphAlignment::kLeft, 1, 0, 0, bulleted},
                 {{"232323123", style}}},
                {foundation::ObjectId::fromUint64(703),
                 {semantic::ParagraphAlignment::kLeft, 1, 7, 0, bulleted},
                 {{"\xe4\xbd\xa0\xe5\xa5\xbd", style}}}};
            const auto layout = service.resolve(object, content);
            ASSERT_TRUE(layout->diagnostics.empty());
            ASSERT_EQ(layout->lines.size(), 2U);
            for (const std::uint32_t offset : {0U, 9U, 10U, 12U}) {
                const auto geometry = service.editingGeometry(object, content, offset, 0);
                ASSERT_TRUE(geometry.available);
                const auto& line = layout->lines[offset < 10 ? 0 : 1];
                SCOPED_TRACE(::testing::Message() << "size=" << size << " list=" << bulleted
                    << " offset=" << offset << " caret=" << geometry.caret.top << ","
                    << geometry.caret.bottom << " line=" << line.bounds.top << ","
                    << line.bounds.bottom << " frame=" << layout->localBounds.top << ","
                    << layout->localBounds.bottom);
                EXPECT_GE(geometry.caret.top, line.bounds.top - 0.01F);
                EXPECT_LE(geometry.caret.bottom, line.bounds.bottom + 0.01F);
                EXPECT_GT(geometry.caret.bottom - geometry.caret.top, 0);
            }
        }
    }
}


TEST(FC02Formatting, LeafPatchPreservesSelectionStylesAndUndo) {
    Host h; using namespace runtime;
    ASSERT_EQ(h.intent(TextIntentKind::kCreate).status,TextStatus::kApplied);
    ASSERT_EQ(h.input(TextInputKind::kReplace,u"ABC").status,TextStatus::kApplied);
    ASSERT_EQ(h.intent(TextIntentKind::kSelectAll).status,TextStatus::kObserved);
    TextIntent format; format.kind=TextIntentKind::kFormat;format.clientId=15;
    format.requestId=++h.request;format.target=h.facade.mountedCanvasTargets().front();
    format.token=h.facade.readTextEditor().token;format.format.color=std::array<float,4>{0.2F,0.6F,0.8F,0.7F};
    ASSERT_EQ(h.facade.submitTextIntent(format).status,TextStatus::kQueued);ASSERT_TRUE(h.runtime.ownerPump());
    ASSERT_EQ(h.facade.textIntentReceipt(15,h.request)->status,TextStatus::kApplied);
    EXPECT_FLOAT_EQ((*h.facade.readTextEditor().format.color)[1],0.6F);
    EXPECT_DOUBLE_EQ(*h.facade.readTextEditor().format.fontSize,16);
    format.requestId=++h.request;format.token=h.facade.readTextEditor().token;format.format={};
    format.format.strikethrough=true;format.format.bulleted=true;
    ASSERT_EQ(h.facade.submitTextIntent(format).status,TextStatus::kQueued);ASSERT_TRUE(h.runtime.ownerPump());
    ASSERT_EQ(h.facade.textIntentReceipt(15,h.request)->status,TextStatus::kApplied);
    EXPECT_EQ(h.facade.readTextEditor().format.strikethrough,true);
    EXPECT_EQ(h.facade.readTextEditor().format.bulleted,true);
    EXPECT_EQ(h.facade.readTextEditor().projection,u"ABC");
    EXPECT_EQ(h.facade.readTextEditor().selection.length,3U);
    ASSERT_TRUE(h.runtime.qualificationRuntime().undo());
    EXPECT_EQ(h.facade.readTextEditor().format.strikethrough,false);
    EXPECT_EQ(h.facade.readTextEditor().format.bulleted,false);
    EXPECT_FLOAT_EQ((*h.facade.readTextEditor().format.color)[1],0.6F);
}

TEST(FC02Formatting, ListAndStrikeChangeSharedLayoutAndCaret) {
    Host h;using namespace runtime;
    ASSERT_EQ(h.intent(TextIntentKind::kCreate).status,TextStatus::kApplied);
    ASSERT_EQ(h.input(TextInputKind::kReplace,u"ABC").status,TextStatus::kApplied);
    ASSERT_EQ(h.intent(TextIntentKind::kSelectAll).status,TextStatus::kObserved);
    const auto before=h.facade.readTextGeometry();ASSERT_TRUE(before.available);
    TextIntent f;f.kind=TextIntentKind::kFormat;f.clientId=15;f.requestId=++h.request;
    f.target=h.facade.mountedCanvasTargets().front();f.token=h.facade.readTextEditor().token;
    f.format.strikethrough=true;f.format.bulleted=true;
    ASSERT_EQ(h.facade.submitTextIntent(f).status,TextStatus::kQueued);ASSERT_TRUE(h.runtime.ownerPump());
    ASSERT_EQ(h.facade.textIntentReceipt(15,h.request)->status,TextStatus::kApplied);
    const auto after=h.facade.readTextGeometry();ASSERT_TRUE(after.available);
    EXPECT_GT(after.caret[0],before.caret[0]+8);
    EXPECT_NE(after.token.layout,before.token.layout);
    RuntimeFrameOpportunity frame;ASSERT_TRUE(h.runtime.serviceFrameOpportunity(false,frame));
    std::vector<std::uint8_t> pixels(256*256*4);ASSERT_TRUE(h.runtime.readCanonicalRgba(pixels));
    std::size_t marker=0;
    for(std::size_t y=35;y<48;++y)for(std::size_t x=24;x<32;++x){const auto j=(y*256+x)*4;if(pixels[j]<128 && pixels[j+1]<128 && pixels[j+2]<128)++marker;}
    EXPECT_GT(marker,3U);
}

TEST(FC02Formatting, CjkBoldItalicKeepsVisibleGlyphsAndCaret) {
    Host h(true);using namespace runtime;
    ASSERT_EQ(h.intent(TextIntentKind::kCreate).status,TextStatus::kApplied);
    ASSERT_EQ(h.input(TextInputKind::kReplace,u"\u4f60\u597d Axiom").status,TextStatus::kApplied);
    ASSERT_EQ(h.intent(TextIntentKind::kSelectAll).status,TextStatus::kObserved);
    TextIntent f;f.kind=TextIntentKind::kFormat;f.clientId=15;f.requestId=++h.request;f.target=h.facade.mountedCanvasTargets().front();f.token=h.facade.readTextEditor().token;f.format.bold=true;f.format.italic=true;
    ASSERT_EQ(h.facade.submitTextIntent(f).status,TextStatus::kQueued);ASSERT_TRUE(h.runtime.ownerPump());
    ASSERT_EQ(h.facade.textIntentReceipt(15,h.request)->status,TextStatus::kApplied);
    const auto g=h.facade.readTextGeometry();EXPECT_TRUE(g.available);EXPECT_TRUE(g.diagnostics.empty());
}

TEST(FC02Formatting, MixedSelectionReportsAnySelectedEffectAndClearsItAtomically) {
    Host h(true);
    using namespace runtime;
    ASSERT_EQ(h.intent(TextIntentKind::kCreate).status, TextStatus::kApplied);
    ASSERT_EQ(h.input(TextInputKind::kReplace, u"ABCD").status, TextStatus::kApplied);
    const auto select = [&](TextRange range) {
        const auto editor = h.facade.readTextEditor();
        TextInputEvent e;
        e.token = editor.token;
        e.sequence = editor.nextInputSequence;
        e.kind = TextInputKind::kSelection;
        e.range = range;
        EXPECT_EQ(h.facade.submitTextInput(e).status, TextStatus::kQueued);
        EXPECT_TRUE(h.runtime.ownerPump());
        EXPECT_EQ(h.facade.textInputReceipt(e.token, e.sequence)->status, TextStatus::kObserved);
    };
    const auto format = [&](bool enabled) {
        TextIntent f;
        f.kind = TextIntentKind::kFormat;
        f.clientId = 15;
        f.requestId = ++h.request;
        f.target = h.facade.mountedCanvasTargets().front();
        f.token = h.facade.readTextEditor().token;
        f.format.bold = enabled;
        f.format.italic = enabled;
        f.format.underline = enabled;
        f.format.strikethrough = enabled;
        EXPECT_EQ(h.facade.submitTextIntent(f).status, TextStatus::kQueued);
        EXPECT_TRUE(h.runtime.ownerPump());
        EXPECT_EQ(h.facade.textIntentReceipt(15, f.requestId)->status, TextStatus::kApplied);
    };
    select({1, 2});
    format(true);
    select({0, 4}); // Plain first run; effects are only in the middle.
    for (int read = 0; read < 3; ++read) {
        const auto editor = h.facade.readTextEditor();
        EXPECT_EQ(editor.format.bold, true);
        EXPECT_EQ(editor.format.italic, true);
        EXPECT_EQ(editor.format.underline, true);
        EXPECT_EQ(editor.format.strikethrough, true);
        EXPECT_EQ(editor.selection, (TextRange{0, 4}));
    }
    // End-exclusive boundaries must not inspect the adjacent styled run.
    select({0, 1});
    EXPECT_EQ(h.facade.readTextEditor().format.bold, false);
    select({3, 1});
    EXPECT_EQ(h.facade.readTextEditor().format.italic, false);
    select({1, 1}); // Start exactly on the styled run, not its plain predecessor.
    EXPECT_EQ(h.facade.readTextEditor().format.bold, true);
    select({0, 4});
    format(false);
    EXPECT_EQ(h.facade.readTextEditor().format.bold, false);
    EXPECT_EQ(h.facade.readTextEditor().format.italic, false);
    EXPECT_EQ(h.facade.readTextEditor().format.underline, false);
    EXPECT_EQ(h.facade.readTextEditor().format.strikethrough, false);
    EXPECT_TRUE(h.runtime.qualificationRuntime().undo());
    EXPECT_EQ(h.facade.readTextEditor().projection, u"ABCD");
    select({0, 4});
    EXPECT_EQ(h.facade.readTextEditor().format.bold, true);
}

TEST(FC02Formatting, MixedParagraphSelectionReportsAnyBulletAndExcludesEndParagraph) {
    Host h(true);
    using namespace runtime;
    ASSERT_EQ(h.intent(TextIntentKind::kCreate).status, TextStatus::kApplied);
    ASSERT_EQ(h.input(TextInputKind::kReplace, u"plain\nlist").status, TextStatus::kApplied);
    TextIntent f;
    f.kind = TextIntentKind::kFormat;
    f.clientId = 15;
    f.requestId = ++h.request;
    f.target = h.facade.mountedCanvasTargets().front();
    f.token = h.facade.readTextEditor().token;
    f.format.bulleted = true;
    ASSERT_EQ(h.facade.submitTextIntent(f).status, TextStatus::kQueued);
    ASSERT_TRUE(h.runtime.ownerPump());
    ASSERT_EQ(h.facade.textIntentReceipt(15, f.requestId)->status, TextStatus::kApplied);
    ASSERT_EQ(h.intent(TextIntentKind::kSelectAll).status, TextStatus::kObserved);
    EXPECT_EQ(h.facade.readTextEditor().format.bulleted, true);
    const auto editor = h.facade.readTextEditor();
    TextInputEvent e;
    e.token = editor.token;
    e.sequence = editor.nextInputSequence;
    e.kind = TextInputKind::kSelection;
    e.range = {0, 6}; // Newline selected; next paragraph starts at exclusive end.
    ASSERT_EQ(h.facade.submitTextInput(e).status, TextStatus::kQueued);
    ASSERT_TRUE(h.runtime.ownerPump());
    EXPECT_EQ(h.facade.readTextEditor().format.bulleted, false);
}

TEST(FC02Formatting, BoldAndItalicChangeRealCanonicalPixels) {
    using namespace runtime;
    const auto draw = [](bool bold, bool italic) {
        Host h(true);
        EXPECT_EQ(h.intent(TextIntentKind::kCreate).status, TextStatus::kApplied);
        EXPECT_EQ(h.input(TextInputKind::kReplace, u"Axiom \u4f60\u597d").status, TextStatus::kApplied);
        EXPECT_EQ(h.intent(TextIntentKind::kSelectAll).status, TextStatus::kObserved);
        TextIntent f;
        f.kind = TextIntentKind::kFormat;
        f.clientId = 15;
        f.requestId = ++h.request;
        f.target = h.facade.mountedCanvasTargets().front();
        f.token = h.facade.readTextEditor().token;
        f.format.bold = bold;
        f.format.italic = italic;
        EXPECT_EQ(h.facade.submitTextIntent(f).status, TextStatus::kQueued);
        EXPECT_TRUE(h.runtime.ownerPump());
        EXPECT_TRUE(h.facade.readTextGeometry().available);
        EXPECT_EQ(h.intent(TextIntentKind::kFinish).status, TextStatus::kApplied);
        RuntimeFrameOpportunity frame;
        EXPECT_TRUE(h.runtime.serviceFrameOpportunity(false, frame));
        std::vector<std::uint8_t> pixels(256 * 256 * 4);
        EXPECT_TRUE(h.runtime.readCanonicalRgba(pixels));
        return pixels;
    };
    const auto plain = draw(false, false);
    const auto bold = draw(true, false);
    const auto italic = draw(false, true);
    const auto both = draw(true, true);
    EXPECT_NE(plain, bold) << "Bold must change glyph pixels, not only stored metadata";
    EXPECT_NE(plain, italic) << "Italic must change glyph pixels";
    EXPECT_NE(bold, both);
    EXPECT_NE(italic, both);
}

TEST(FC02Formatting, ImportedFamiliesAreNamedSelectableAndActuallyRender) {
    Host h(true);
    using namespace runtime;
    ASSERT_EQ(h.intent(TextIntentKind::kCreate).status, TextStatus::kApplied);
    const auto options = h.facade.readTextEditor().fonts;
    ASSERT_EQ(options.size(), 12U);
    const std::vector<std::string> names{"Comic Neue", "Fira Sans", "IBM Plex Mono", "IBM Plex Sans",
        "Inter", "Inter Tight", "Open Sans", "Oswald", "Roboto", "STIX Two Text", "Ubuntu"};
    ASSERT_EQ(h.input(TextInputKind::kReplace, u"Axiom 123").status, TextStatus::kApplied);
    ASSERT_EQ(h.intent(TextIntentKind::kSelectAll).status, TextStatus::kObserved);
    for (const auto& name : names) {
        const auto option = std::find_if(options.begin(), options.end(), [&](const auto& f) {
            return f.name == name;
        });
        ASSERT_NE(option, options.end()) << name;
        TextIntent f;
        f.kind = TextIntentKind::kFormat;
        f.clientId = 15;
        f.requestId = ++h.request;
        f.target = h.facade.mountedCanvasTargets().front();
        f.token = h.facade.readTextEditor().token;
        f.format.font = option->id;
        ASSERT_EQ(h.facade.submitTextIntent(f).status, TextStatus::kQueued);
        ASSERT_TRUE(h.runtime.ownerPump());
        EXPECT_EQ(h.facade.readTextEditor().format.font, option->id);
        const auto geometry = h.facade.readTextGeometry();
        EXPECT_TRUE(geometry.available) << name;
        EXPECT_TRUE(geometry.diagnostics.empty()) << name;
        ASSERT_EQ(h.intent(TextIntentKind::kSelectAll).status, TextStatus::kObserved);
    }
}
} // namespace
