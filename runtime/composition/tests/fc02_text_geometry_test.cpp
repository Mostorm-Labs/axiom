#include "canvas/runtime/internal/runtime_composition_core.hpp"
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
    runtime::RuntimeCompositionCore runtime;
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
semantic::ObjectRecord labelShape(unsigned kind = 1) {
    semantic::ObjectRecord record;
    record.id = foundation::ObjectId::fromUint64(811);
    record.kind = semantic::ObjectKind::kShape;
    record.kind_version = kind == 3 ? 2 : 1;
    record.content = semantic::ShapeContent{kind, 200, 120};
    record.placement.order_key = semantic::OrderKey({1});
    record.transform.tx = 20;
    record.transform.ty = 30;
    return record;
}
void seedShape(Host& h, const semantic::ObjectRecord& shape) {
    const semantic::Operation seed{semantic::OperationId{foundation::ObjectId::fromUint64(812)},
        semantic::DocumentId{foundation::ObjectId::fromUint64(1)}, 1, 1,
        semantic::InsertObjectsOp{{shape}}};
    ASSERT_EQ(h.runtime.applyExternalOperation(seed, semantic::ApplySource::kRemoteSync).disposition,
              semantic::ApplyDisposition::kApplied);
    runtime::CanvasControlRequest tool;
    tool.clientId = 86; tool.requestId = 1;
    tool.target = h.facade.mountedCanvasTargets().front();
    tool.payload.kind = runtime::CanvasControlPayloadKind::kSelectTool;
    tool.payload.tool = runtime::CanvasToolKind::kText;
    ASSERT_EQ(h.facade.submitCanvasControl(tool).state, runtime::ProductControlState::kQueued);
    ASSERT_TRUE(h.runtime.ownerPump());
}
void seedSticky(Host& h, const semantic::ObjectRecord& sticky) {
    const semantic::Operation seed{semantic::OperationId{foundation::ObjectId::fromUint64(812)} ,
        semantic::DocumentId{foundation::ObjectId::fromUint64(1)}, 1, 1,
        semantic::InsertObjectsOp{{sticky}}};
    ASSERT_EQ(h.runtime.applyExternalOperation(seed, semantic::ApplySource::kRemoteSync).disposition,
              semantic::ApplyDisposition::kApplied);
    runtime::CanvasControlRequest tool;
    tool.clientId = 86; tool.requestId = 1;
    tool.target = h.facade.mountedCanvasTargets().front();
    tool.payload.kind = runtime::CanvasControlPayloadKind::kSelectTool;
    tool.payload.tool = runtime::CanvasToolKind::kText;
    ASSERT_EQ(h.facade.submitCanvasControl(tool).state, runtime::ProductControlState::kQueued);
    ASSERT_TRUE(h.runtime.ownerPump());
}
TEST(FC02ShapeLabel, EmptyCaretStartsInsideAllThreeShapeCenters) {
    for (unsigned kind : {1U, 2U, 3U}) {
        SCOPED_TRACE(kind);
        Host h(true); seedShape(h, labelShape(kind));
        runtime::TextIntent edit;
        edit.clientId = 15; edit.requestId = ++h.request;
        edit.target = h.facade.mountedCanvasTargets().front();
        edit.kind = runtime::TextIntentKind::kEditAtPoint;
        edit.viewX = 120; edit.viewY = 90;
        ASSERT_EQ(h.facade.submitTextIntent(edit).status, runtime::TextStatus::kQueued);
        ASSERT_TRUE(h.runtime.ownerPump());
        ASSERT_EQ(h.facade.textIntentReceipt(15, h.request)->status, runtime::TextStatus::kApplied);
        const auto caret = h.facade.readTextGeometry();
        ASSERT_TRUE(caret.available);
        EXPECT_NEAR(caret.caret[0], 120, 2);
        EXPECT_GT(caret.caret[1], 65);
        if (kind == 3U) {
            EXPECT_GT(caret.caret[1], 100);
            EXPECT_LT(caret.caret[5], 142);
        } else EXPECT_LT(caret.caret[5], 125);
        EXPECT_EQ(h.runtime.semanticObjectCount(), 1U);
        EXPECT_EQ(h.runtime.qualificationRuntime().submittedOperationCount(), 0U);
    }
}
TEST(FC02StickyLabel, TypingKeepsSizeUntilSelectedExpand) {
    Host h(true);
    semantic::ObjectRecord record;
    record.id = foundation::ObjectId::fromUint64(901);
    record.kind = semantic::ObjectKind::kSticky;
    record.kind_version = 1;
    record.placement.order_key = semantic::OrderKey({1});
    record.transform.tx = 20;
    record.transform.ty = 100;
    record.content = semantic::StickyContent{48, 48};
    seedSticky(h, record);
    runtime::TextIntent edit;
    edit.clientId = 15; edit.requestId = ++h.request;
    edit.target = h.facade.mountedCanvasTargets().front();
    edit.kind = runtime::TextIntentKind::kEditAtPoint;
    edit.object = record.id; edit.viewX = 32; edit.viewY = 112;
    ASSERT_EQ(h.facade.submitTextIntent(edit).status, runtime::TextStatus::kQueued);
    ASSERT_TRUE(h.runtime.ownerPump());
    ASSERT_EQ(h.facade.textIntentReceipt(15, h.request)->status, runtime::TextStatus::kApplied);
    const auto before = *h.runtime.qualificationRuntime().qualificationObjectRecord(record.id);
    ASSERT_EQ(h.input(runtime::TextInputKind::kReplace, u"A\nB\nC\nD\nE\nF").status,
              runtime::TextStatus::kApplied);
    const auto* after = h.runtime.qualificationRuntime().qualificationObjectRecord(record.id);
    ASSERT_NE(after, nullptr);
    const auto& grown = std::get<semantic::StickyContent>(after->content);
    const auto& original = std::get<semantic::StickyContent>(before.content);
    EXPECT_EQ(grown.width, grown.height);
    EXPECT_EQ(grown.width, original.width);
    EXPECT_EQ(after->transform.ty, before.transform.ty);
    EXPECT_DOUBLE_EQ(after->transform.ty, before.transform.ty);
    const auto geometry = h.facade.readTextGeometry();
    ASSERT_TRUE(geometry.available);
    EXPECT_GE(geometry.caret[1], after->transform.ty);
    EXPECT_LE(geometry.caret[5], after->transform.ty + grown.height);
}
TEST(FC02StickyLabel, LongUnbrokenTextWrapsBeforeSquareExpansion) {
    Host h(true);
    semantic::ObjectRecord record;
    record.id = foundation::ObjectId::fromUint64(902);
    record.kind = semantic::ObjectKind::kSticky;
    record.kind_version = 1;
    record.placement.order_key = semantic::OrderKey({1});
    record.transform.tx = 20;
    record.transform.ty = 100;
    record.content = semantic::StickyContent{96, 96};
    seedSticky(h, record);
    runtime::TextIntent edit;
    edit.clientId = 15; edit.requestId = ++h.request;
    edit.target = h.facade.mountedCanvasTargets().front();
    edit.kind = runtime::TextIntentKind::kEditAtPoint;
    edit.object = record.id; edit.viewX = 32; edit.viewY = 112;
    ASSERT_EQ(h.facade.submitTextIntent(edit).status, runtime::TextStatus::kQueued);
    ASSERT_TRUE(h.runtime.ownerPump());
    ASSERT_EQ(h.facade.textIntentReceipt(15, h.request)->status, runtime::TextStatus::kApplied);
    std::u16string digits(160, u'2');
    ASSERT_EQ(h.input(runtime::TextInputKind::kReplace, digits).status,
              runtime::TextStatus::kApplied);
    const auto* after = h.runtime.qualificationRuntime().qualificationObjectRecord(record.id);
    ASSERT_NE(after, nullptr);
    const auto& sticky = std::get<semantic::StickyContent>(after->content);
    EXPECT_EQ(sticky.width, sticky.height);
    EXPECT_EQ(sticky.height, 96.0);
    EXPECT_LT(sticky.height, 4096.0);
    EXPECT_TRUE(h.facade.readTextGeometry().available);
    const auto geometry = h.facade.readTextGeometry();
    ASSERT_TRUE(geometry.available);
    EXPECT_LE(geometry.caret[2], after->transform.tx + sticky.width);
    EXPECT_LE(geometry.caret[5], after->transform.ty + sticky.height);
}
TEST(FC02StickyLabel, SelectedExpandButtonCommitsSquareGrowth) {
    Host h(true);
    semantic::ObjectRecord record;
    record.id = foundation::ObjectId::fromUint64(903);
    record.kind = semantic::ObjectKind::kSticky;
    record.kind_version = 1;
    record.placement.order_key = semantic::OrderKey({1});
    record.transform.tx = 20;
    record.transform.ty = 40;
    record.content = semantic::StickyContent{48, 48};
    seedSticky(h, record);
    runtime::TextIntent edit;
    edit.clientId = 15; edit.requestId = ++h.request;
    edit.target = h.facade.mountedCanvasTargets().front();
    edit.kind = runtime::TextIntentKind::kEditAtPoint;
    edit.object = record.id; edit.viewX = 32; edit.viewY = 52;
    ASSERT_EQ(h.facade.submitTextIntent(edit).status, runtime::TextStatus::kQueued);
    ASSERT_TRUE(h.runtime.ownerPump());
    ASSERT_EQ(h.facade.textIntentReceipt(15, h.request)->status, runtime::TextStatus::kApplied);
    ASSERT_EQ(h.input(runtime::TextInputKind::kReplace, u"A\nB\nC\nD\nE\nF").status,
              runtime::TextStatus::kApplied);
    ASSERT_TRUE(h.runtime.qualificationRuntime().setSelectionMode(true));
    ASSERT_TRUE(h.runtime.qualificationRuntime().selectAtViewPoint(32, 52));
    ASSERT_EQ(h.runtime.qualificationRuntime().selectedPrimaryObject(), record.id);
    ASSERT_TRUE(h.runtime.registerSource(86));
    input::PlatformPointerBatch batch;
    // Sticky is at (20,40) with a 48x48 local box.  The rendered affordance
    // occupies the inset bottom-right corner; click its center rather than
    // relying on the inclusive outer edge of the hit rectangle.
    batch.samples.push_back({86, 1, 1, 1000, 48, 68, 0, 0, 0, {}, {},
        input::SampleProvenance::kConfirmedCurrent, input::PointerPhase::kDown});
    ASSERT_TRUE(h.runtime.submit(batch));
    const auto* after = h.runtime.qualificationRuntime().qualificationObjectRecord(record.id);
    ASSERT_NE(after, nullptr);
    const auto& sticky = std::get<semantic::StickyContent>(after->content);
    EXPECT_GT(sticky.width, 48.0);
    EXPECT_EQ(sticky.width, sticky.height);
    // Sticky editing first creates its lazy RichText child, then commits the
    // text and the explicit square expansion.
    EXPECT_EQ(h.runtime.qualificationRuntime().submittedOperationCount(), 3U);
}
TEST(FC02StickyLabel, SelectedExpandButtonGrowsForHorizontalOverflow) {
    Host h(true);
    semantic::ObjectRecord record;
    record.id = foundation::ObjectId::fromUint64(904);
    record.kind = semantic::ObjectKind::kSticky;
    record.kind_version = 1;
    record.placement.order_key = semantic::OrderKey({1});
    record.transform.tx = 20;
    record.transform.ty = 40;
    record.content = semantic::StickyContent{200, 200};
    seedSticky(h, record);
    runtime::TextIntent edit;
    edit.clientId = 15; edit.requestId = ++h.request;
    edit.target = h.facade.mountedCanvasTargets().front();
    edit.kind = runtime::TextIntentKind::kEditAtPoint;
    edit.object = record.id; edit.viewX = 32; edit.viewY = 52;
    ASSERT_EQ(h.facade.submitTextIntent(edit).status, runtime::TextStatus::kQueued);
    ASSERT_TRUE(h.runtime.ownerPump());
    ASSERT_EQ(h.facade.textIntentReceipt(15, h.request)->status, runtime::TextStatus::kApplied);
    ASSERT_EQ(h.input(runtime::TextInputKind::kReplace,
                      std::u16string(300, u'W')).status,
              runtime::TextStatus::kApplied);
    ASSERT_TRUE(h.runtime.qualificationRuntime().setSelectionMode(true));
    ASSERT_TRUE(h.runtime.qualificationRuntime().selectAtViewPoint(32, 52));
    ASSERT_EQ(h.runtime.qualificationRuntime().selectedPrimaryObject(), record.id);
    ASSERT_TRUE(h.runtime.registerSource(86));
    input::PlatformPointerBatch batch;
    batch.samples.push_back({86, 1, 1, 1000, 200, 220, 0, 0, 0, {}, {},
        input::SampleProvenance::kConfirmedCurrent, input::PointerPhase::kDown});
    ASSERT_TRUE(h.runtime.submit(batch));
    const auto* after = h.runtime.qualificationRuntime().qualificationObjectRecord(record.id);
    ASSERT_NE(after, nullptr);
    const auto& sticky = std::get<semantic::StickyContent>(after->content);
    EXPECT_GT(sticky.width, 200.0);
    EXPECT_EQ(sticky.width, sticky.height);
}
TEST(FC02StickyLabel, WrappedTextThatFitsDoesNotShowExpand) {
    runtime::MemoryResourceProvider resources;
    const semantic::ResourceId font{foundation::ObjectId::fromUint64(0xFC0202)};
    std::ifstream file(FC02_HOST_FONT, std::ios::binary);
    resources.publish(font.value, {std::istreambuf_iterator<char>(file), {}});
    text::RichTextLayoutService service(resources, {300, 1});
    service.registerFont({font, std::string(minimal_runtime_host::kHostTextFontSha256), 0});
    semantic::TextStyle style{font, 16, 400, false, false, {0, 0, 0, 1}};
    semantic::RichTextContent label;
    label.document.paragraphs = {{foundation::ObjectId::fromUint64(905),
        {semantic::ParagraphAlignment::kLeft, 1, 0, 0},
        {{"878787878787878787878787878787 878787878787", style}}}};
    const semantic::StickyContent sticky{200, 200};
    const auto layout = service.resolveStickyLabel(
        foundation::ObjectId::fromUint64(905), label, sticky, false);
    ASSERT_TRUE(layout->diagnostics.empty());
    EXPECT_FALSE(layout->overflow);
    EXPECT_GT(layout->lines.size(), 1U);
    EXPECT_LE(layout->localBounds.bottom, 188.0F);
}
TEST(FC02StickyLabel, SelectedExpandAfterManualScaleUsesVisibleButton) {
    Host h(true);
    semantic::ObjectRecord record;
    record.id = foundation::ObjectId::fromUint64(906);
    record.kind = semantic::ObjectKind::kSticky;
    record.kind_version = 1;
    record.placement.order_key = semantic::OrderKey({1});
    record.transform.tx = 20;
    record.transform.ty = 40;
    // Equivalent to manually shrinking a 200x200 Sticky to half size: the
    // canonical content size stays fixed while the transform carries the
    // visible scale.
    record.transform.a = 0.5;
    record.transform.d = 0.5;
    record.content = semantic::StickyContent{200, 200};
    seedSticky(h, record);
    runtime::TextIntent edit;
    edit.clientId = 15; edit.requestId = ++h.request;
    edit.target = h.facade.mountedCanvasTargets().front();
    edit.kind = runtime::TextIntentKind::kEditAtPoint;
    edit.object = record.id; edit.viewX = 35; edit.viewY = 55;
    ASSERT_EQ(h.facade.submitTextIntent(edit).status, runtime::TextStatus::kQueued);
    ASSERT_TRUE(h.runtime.ownerPump());
    ASSERT_EQ(h.facade.textIntentReceipt(15, h.request)->status, runtime::TextStatus::kApplied);
    ASSERT_EQ(h.input(runtime::TextInputKind::kReplace, std::u16string(300, u'W')).status,
              runtime::TextStatus::kApplied);
    ASSERT_TRUE(h.runtime.qualificationRuntime().setSelectionMode(true));
    ASSERT_TRUE(h.runtime.qualificationRuntime().selectAtViewPoint(35, 55));
    ASSERT_EQ(h.runtime.qualificationRuntime().selectedPrimaryObject(), record.id);
    ASSERT_TRUE(h.runtime.registerSource(86));
    input::PlatformPointerBatch batch;
    // The renderer applies the inverse label scale inside the Sticky
    // transform. The visible button therefore lands at world center (100,120)
    // for this 0.5-scaled 200px Sticky, not at the canonical local midpoint.
    batch.samples.push_back({86, 1, 1, 1000, 100, 120, 0, 0, 0, {}, {},
        input::SampleProvenance::kConfirmedCurrent, input::PointerPhase::kDown});
    ASSERT_TRUE(h.runtime.submit(batch));
    const auto* after = h.runtime.qualificationRuntime().qualificationObjectRecord(record.id);
    ASSERT_NE(after, nullptr);
    const auto& sticky = std::get<semantic::StickyContent>(after->content);
    EXPECT_GT(sticky.width, 200.0);
    EXPECT_EQ(sticky.width, sticky.height);
}
TEST(FC02ShapeLabel, RealDownBeforeSecondDoubleClickDoesNotCreateOrBlockEditor) {
    Host h(true); seedShape(h, labelShape());
    runtime::TextIntent edit;
    edit.clientId = 15; edit.requestId = ++h.request;
    edit.target = h.facade.mountedCanvasTargets().front();
    edit.kind = runtime::TextIntentKind::kEditAtPoint; edit.viewX = 120; edit.viewY = 90;
    ASSERT_EQ(h.facade.submitTextIntent(edit).status, runtime::TextStatus::kQueued);
    ASSERT_TRUE(h.runtime.ownerPump());
    ASSERT_EQ(h.input(runtime::TextInputKind::kReplace, u"existing label").status, runtime::TextStatus::kApplied);
    ASSERT_EQ(h.intent(runtime::TextIntentKind::kFinish).status, runtime::TextStatus::kApplied);
    const auto operations = h.runtime.qualificationRuntime().submittedOperationCount();
    ASSERT_TRUE(h.runtime.registerSource(86));
    for (auto phase : {input::PointerPhase::kDown, input::PointerPhase::kUp}) {
        input::PlatformPointerBatch batch;
        const auto seq = phase == input::PointerPhase::kDown ? 1U : 2U;
        batch.samples.push_back({86, 1, seq, seq * 1000, 120, 90, 0, 0, 0, {}, {},
            input::SampleProvenance::kConfirmedCurrent, phase});
        ASSERT_TRUE(h.runtime.submit(batch));
    }
    edit.requestId = ++h.request;
    ASSERT_EQ(h.facade.submitTextIntent(edit).status, runtime::TextStatus::kQueued);
    ASSERT_TRUE(h.runtime.ownerPump());
    EXPECT_EQ(h.facade.textIntentReceipt(15, h.request)->status, runtime::TextStatus::kApplied);
    EXPECT_EQ(h.facade.readTextEditor().token.object, labelShape().id);
    EXPECT_EQ(h.facade.readTextEditor().projection, u"existing label");
    EXPECT_EQ(h.runtime.qualificationRuntime().submittedOperationCount(), operations);
}
TEST(FC02ShapeLabel, ReenterUsesTheClickedGlyphAndPointerCanMoveCaret) {
    Host h(true); seedShape(h, labelShape());
    runtime::TextIntent edit;
    edit.clientId=15; edit.requestId=++h.request;
    edit.target=h.facade.mountedCanvasTargets().front();
    edit.kind=runtime::TextIntentKind::kEditAtPoint; edit.viewX=120; edit.viewY=90;
    ASSERT_EQ(h.facade.submitTextIntent(edit).status,runtime::TextStatus::kQueued);
    ASSERT_TRUE(h.runtime.ownerPump());
    ASSERT_EQ(h.input(runtime::TextInputKind::kReplace,u"Hello Axiom").status,runtime::TextStatus::kApplied);
    const auto end=h.facade.readTextGeometry(); ASSERT_TRUE(end.available);
    ASSERT_EQ(h.intent(runtime::TextIntentKind::kFinish).status,runtime::TextStatus::kApplied);
    edit.requestId=++h.request; edit.viewX=end.caret[0]+2; edit.viewY=(end.caret[1]+end.caret[5])*0.5F;
    ASSERT_EQ(h.facade.submitTextIntent(edit).status,runtime::TextStatus::kQueued);
    ASSERT_TRUE(h.runtime.ownerPump());
    EXPECT_EQ(h.facade.readTextEditor().selection,(runtime::TextRange{11,0}));
    ASSERT_TRUE(h.runtime.registerSource(86));
    const auto click=[&](std::uint64_t seq,float x) {
        for(auto phase:{input::PointerPhase::kDown,input::PointerPhase::kUp}) {
            input::PlatformPointerBatch batch;
            batch.samples.push_back({86,1,seq,seq*1000,x,edit.viewY,0,0,0,{},{},
                input::SampleProvenance::kConfirmedCurrent,phase});
            EXPECT_TRUE(h.runtime.submit(batch)); ++seq;
        }
    };
    click(1,70); EXPECT_EQ(h.facade.readTextEditor().selection,(runtime::TextRange{0,0}));
    click(3,edit.viewX); EXPECT_EQ(h.facade.readTextEditor().selection,(runtime::TextRange{11,0}));
    EXPECT_EQ(h.facade.readTextEditor().phase,runtime::TextPhase::kEditing);
}
TEST(FC02ShapeLabel, OutsideDownFinishesWithoutCreatingAnotherDraft) {
    for(bool composing:{false,true}) {
        SCOPED_TRACE(composing);
        Host h(true); seedShape(h,labelShape());
        runtime::TextIntent edit;
        edit.clientId=15;edit.requestId=++h.request;edit.target=h.facade.mountedCanvasTargets().front();
        edit.kind=runtime::TextIntentKind::kEditAtPoint;edit.viewX=120;edit.viewY=90;
        ASSERT_EQ(h.facade.submitTextIntent(edit).status,runtime::TextStatus::kQueued);
        ASSERT_TRUE(h.runtime.ownerPump());
        ASSERT_EQ(h.input(runtime::TextInputKind::kReplace,u"Confirmed").status,runtime::TextStatus::kApplied);
        if(composing) {
            ASSERT_EQ(h.input(runtime::TextInputKind::kBegin).status,runtime::TextStatus::kObserved);
            ASSERT_EQ(h.input(runtime::TextInputKind::kUpdate,u"preedit").status,runtime::TextStatus::kObserved);
        }
        const auto operations=h.runtime.qualificationRuntime().submittedOperationCount();
        ASSERT_TRUE(h.runtime.registerSource(86));
        for(auto phase:{input::PointerPhase::kDown,input::PointerPhase::kUp}) {
            const auto seq=phase==input::PointerPhase::kDown?1U:2U;
            input::PlatformPointerBatch batch;
            batch.samples.push_back({86,1,seq,seq*1000,250,240,0,0,0,{},{},
                input::SampleProvenance::kConfirmedCurrent,phase});
            ASSERT_TRUE(h.runtime.submit(batch));
        }
        const auto state=h.facade.readTextEditor();
        if(composing) {
            EXPECT_EQ(state.phase,runtime::TextPhase::kFinishing);
            EXPECT_NE(state.finishEffect,0U);
            EXPECT_EQ(state.projection,u"Confirmed");
            EXPECT_EQ(h.runtime.qualificationRuntime().submittedOperationCount(),operations);
            ASSERT_EQ(h.input(runtime::TextInputKind::kCommit,u"native result").status,runtime::TextStatus::kApplied);
        }
        EXPECT_EQ(h.facade.readTextEditor().phase,runtime::TextPhase::kClosed);
        EXPECT_EQ(h.runtime.semanticObjectCount(),1U);
        EXPECT_EQ(h.runtime.qualificationRuntime().submittedOperationCount(),operations+(composing?1:0));
    }
}
TEST(FC02ShapeLabel, FirstShapeLabelTextDefaultsToCenteredParagraph) {
    Host h(true); seedShape(h, labelShape());
    runtime::TextIntent edit;
    edit.clientId = 15; edit.requestId = ++h.request;
    edit.target = h.facade.mountedCanvasTargets().front();
    edit.kind = runtime::TextIntentKind::kEditAtPoint; edit.viewX = 120; edit.viewY = 90;
    ASSERT_EQ(h.facade.submitTextIntent(edit).status, runtime::TextStatus::kQueued);
    ASSERT_TRUE(h.runtime.ownerPump());
    ASSERT_EQ(h.input(runtime::TextInputKind::kReplace, u"AB").status,
              runtime::TextStatus::kApplied);
    const auto* object = h.runtime.qualificationRuntime().qualificationObjectRecord(labelShape().id);
    ASSERT_NE(object, nullptr);
    const auto* shape = std::get_if<semantic::ShapeContent>(&object->content);
    ASSERT_NE(shape, nullptr); ASSERT_TRUE(shape->label.has_value());
    ASSERT_FALSE(shape->label->document.paragraphs.empty());
    EXPECT_EQ(shape->label->document.paragraphs.front().style.alignment,
              semantic::ParagraphAlignment::kCenter);
    const auto geometry = h.facade.readTextGeometry();
    ASSERT_TRUE(geometry.available);
    EXPECT_GT(geometry.caret[0], 120.0F);
    EXPECT_LT(geometry.caret[0], 145.0F);
}

TEST(FC02ShapeLabel, TriangleAndEllipseUseInteriorLayoutGeometry) {
    runtime::MemoryResourceProvider resources;
    const semantic::ResourceId font{foundation::ObjectId::fromUint64(0xFC0202)};
    std::ifstream file(FC02_HOST_FONT, std::ios::binary);
    resources.publish(font.value, {std::istreambuf_iterator<char>(file), {}});
    text::RichTextLayoutService service(resources, {300, 1});
    service.registerFont({font, std::string(minimal_runtime_host::kHostTextFontSha256), 0});
    semantic::TextStyle style{font, 16, 400, false, false, {0, 0, 0, 1}};
    semantic::RichTextContent label;
    label.document.paragraphs = {{foundation::ObjectId::fromUint64(818),
        {semantic::ParagraphAlignment::kCenter, 1, 0, 0},
        {{"A long label that wraps inside the shape", style}}}};
    for (const auto kind : {2U, 3U}) {
        auto record = labelShape(kind); auto& shape = std::get<semantic::ShapeContent>(record.content);
        shape.width = 200; shape.height = 120; shape.label = label; record.kind_version = 3;
        const auto layout = service.resolveShape(record.id, shape, record.transform);
        ASSERT_FALSE(layout->runs.empty());
        EXPECT_LT(layout->context.width, 0.8F * static_cast<float>(shape.width));
        EXPECT_GT(layout->labelOffsetY, kind == 3U ? 0.30F * static_cast<float>(shape.height) : 0.0F);
        // Check line corners, rather than only their centers: text clipped by
        // the sloping boundary must wrap before it reaches that boundary.
        for (const auto& line : layout->lines)
            for (const auto x : {line.bounds.left, line.bounds.right})
                for (const auto y : {line.bounds.top, line.bounds.bottom}) {
                    if (kind == 2U) {
                        const auto dx = (x - 100.0F) / 100.0F;
                        const auto dy = (y - 60.0F) / 60.0F;
                        EXPECT_LT(dx * dx + dy * dy, 1.0F);
                    } else {
                        EXPECT_GT(y, 60.0F);
                        EXPECT_LT(y, 120.0F);
                        EXPECT_LT(std::abs(x - 100.0F), y * (100.0F / 120.0F));
                    }
                }
        EXPECT_EQ(*shape.label, label) << "Derived wrapping must preserve canonical content";
        shape.label->document.paragraphs.front().runs.front().text = "AB";
        const auto shortLabel = service.resolveShape(record.id, shape, record.transform);
        ASSERT_EQ(shortLabel->lines.size(), 1U);
        const auto& line = shortLabel->lines.front().bounds;
        EXPECT_NEAR((line.left + line.right) * 0.5F, 100, 1);
        EXPECT_NEAR((line.top + line.bottom) * 0.5F, kind == 3U ? 90 : 60, 2);
        const auto caret = service.shapeEditingGeometry(record, 2, 0);
        ASSERT_TRUE(caret.available);
        EXPECT_NEAR(caret.caret.right - 1, line.right, 2);
        EXPECT_NEAR(caret.caret.top, line.top, 2);
        const auto hit = service.shapeEditingGeometry(record, 2, 0,
            foundation::WorldPoint{caret.caret.left, (caret.caret.top + caret.caret.bottom) * 0.5F});
        EXPECT_EQ(hit.hit, 2U);
        // Multiple paragraphs share the same interior height budget.
        auto& paragraphs = shape.label->document.paragraphs;
        const auto first = paragraphs.front();
        for (unsigned i = 0; i < 8; ++i) {
            auto next = first;
            next.id = foundation::ObjectId::fromUint64(820 + i);
            paragraphs.push_back(next);
        }
        const auto multi = service.resolveShape(record.id, shape, record.transform);
        EXPECT_TRUE(multi->overflow);
        EXPECT_LE(multi->localBounds.bottom, multi->labelViewport.bottom + 0.1F);
    }
}
TEST(FC02ShapeLabel, FixedTypographyPaddingEllipsisAndPlaceholderAreDerived) {
    runtime::MemoryResourceProvider resources;
    const semantic::ResourceId font{foundation::ObjectId::fromUint64(0xFC0202)};
    std::ifstream file(FC02_HOST_FONT, std::ios::binary);
    resources.publish(font.value, {std::istreambuf_iterator<char>(file), {}});
    text::RichTextLayoutService service(resources, {300, 1});
    service.registerFont({font, std::string(minimal_runtime_host::kHostTextFontSha256), 0});
    auto record = labelShape();
    auto& shape = std::get<semantic::ShapeContent>(record.content);
    auto unselected = service.resolveShape(record.id, shape, record.transform);
    EXPECT_FALSE(unselected->placeholder);
    EXPECT_TRUE(unselected->runs.empty());
    auto placeholder = service.resolveShape(record.id, shape, record.transform, true);
    ASSERT_TRUE(placeholder->placeholder);
    EXPECT_FALSE(placeholder->runs.empty());
    EXPECT_FALSE(shape.label.has_value());
    semantic::TextStyle style{font, 16, 400, false, false, {0, 0, 0, 1}};
    semantic::RichTextContent label;
    label.layout = semantic::RichTextLayout{200};
    label.document.paragraphs = {{foundation::ObjectId::fromUint64(813),
        {semantic::ParagraphAlignment::kCenter, 1, 0, 0},
        {{"Axiom label has much more content than this small shape can display", style}}}};
    shape.label = label; shape.width = 80; shape.height = 40;
    const auto small = service.resolveShape(record.id, shape, record.transform);
    EXPECT_TRUE(small->overflow);
    EXPECT_GT(small->labelViewport.left, 0);
    EXPECT_LT(small->labelViewport.right, 80);
    EXPECT_LE(small->localBounds.bottom, 32.01F);
    ASSERT_FALSE(small->runs.empty());
    EXPECT_EQ(small->runs.front().fontSize, 16);
    record.transform.a = 2; record.transform.d = 3;
    const auto large = service.resolveShape(record.id, shape, record.transform);
    ASSERT_FALSE(large->runs.empty());
    EXPECT_FLOAT_EQ(large->runs.front().fontSize, 16);
    EXPECT_FLOAT_EQ(large->labelScaleX, 0.5F);
    EXPECT_FLOAT_EQ(large->labelScaleY, 1.0F / 3);
    EXPECT_EQ(*shape.label, label) << "Display truncation must preserve full canonical text";
    EXPECT_NE(small->digest, large->digest);
    record.transform.a = 0.5;
    record.transform.d = 0.5;
    const auto compact = service.resolveShape(record.id, shape, record.transform);
    EXPECT_TRUE(compact->overflow);
    EXPECT_FLOAT_EQ(compact->labelScaleX, 2.0F);
    EXPECT_FLOAT_EQ(compact->labelScaleY, 2.0F);
}
TEST(FC02ShapeLabel, TypingLongLabelExpandsUpwardFromTheBottomEdge) {
    runtime::MemoryResourceProvider resources;
    const semantic::ResourceId font{foundation::ObjectId::fromUint64(0xFC0202)};
    std::ifstream file(FC02_HOST_FONT, std::ios::binary);
    resources.publish(font.value, {std::istreambuf_iterator<char>(file), {}});
    text::RichTextLayoutService service(resources, {300, 1});
    service.registerFont({font, std::string(minimal_runtime_host::kHostTextFontSha256), 0});
    for (const auto kind : {1U, 2U, 3U}) {
      for (const bool existingLabel : {false, true}) {
        SCOPED_TRACE(kind);
        SCOPED_TRACE(existingLabel);
        Host h(true);
        auto record = labelShape(kind);
        auto& shape = std::get<semantic::ShapeContent>(record.content);
        shape.width = 80; shape.height = 40;
        semantic::TextStyle style{semantic::ResourceId{foundation::ObjectId::fromUint64(0xFC0202)},
                                  16, 400, false, false, {0, 0, 0, 1}};
        semantic::RichTextContent emptyLabel;
        emptyLabel.layout = semantic::RichTextLayout{80};
        emptyLabel.document.paragraphs = {{foundation::ObjectId::fromUint64(814),
            {semantic::ParagraphAlignment::kCenter, 1, 0, 0}, {{"", style}}}};
        if (existingLabel) {
            record.kind_version = 3;
            shape.label = emptyLabel;
        }
        seedShape(h, record);
        runtime::TextIntent edit;
        edit.clientId = 15; edit.requestId = ++h.request;
        edit.target = h.facade.mountedCanvasTargets().front();
        edit.kind = runtime::TextIntentKind::kEditAtPoint; edit.object = record.id;
        edit.viewX = 60; edit.viewY = 50;
        ASSERT_EQ(h.facade.submitTextIntent(edit).status, runtime::TextStatus::kQueued);
        ASSERT_TRUE(h.runtime.ownerPump());
        ASSERT_EQ(h.facade.textIntentReceipt(15, h.request)->status,
                  runtime::TextStatus::kApplied);
        const auto before = *h.runtime.qualificationRuntime().qualificationObjectRecord(record.id);
        const auto& beforeShape = std::get<semantic::ShapeContent>(before.content);
        const auto beforeBottom = before.transform.ty + before.transform.b * beforeShape.width * 0.5 +
                                  before.transform.d * beforeShape.height;
        ASSERT_EQ(h.input(runtime::TextInputKind::kReplace,
                          u"A\nB\nC\nD\nE\nF").status,
                  runtime::TextStatus::kApplied);
        const auto* after = h.runtime.qualificationRuntime().qualificationObjectRecord(record.id);
        ASSERT_NE(after, nullptr);
        const auto& afterShape = std::get<semantic::ShapeContent>(after->content);
        EXPECT_GT(after->transform.d * afterShape.height,
                  before.transform.d * beforeShape.height);
        EXPECT_NEAR(after->transform.ty + after->transform.b * afterShape.width * 0.5 +
                        after->transform.d * afterShape.height,
                    beforeBottom, 0.01);
        const auto layout = service.resolveShape(after->id, afterShape, after->transform);
        EXPECT_FALSE(layout->overflow) << "Expansion must reveal the complete label";
        EXPECT_EQ(layout->lines.size(), 6U);
        EXPECT_DOUBLE_EQ(afterShape.label->document.paragraphs.front().runs.front().style.font_size, 16);
        EXPECT_EQ(h.runtime.qualificationRuntime().submittedOperationCount(), 2U);
        const auto grownHeight = after->transform.d * afterShape.height;
        ASSERT_EQ(h.input(runtime::TextInputKind::kReplace, u"\nG").status,
                  runtime::TextStatus::kApplied);
        const auto* continued = h.runtime.qualificationRuntime().qualificationObjectRecord(record.id);
        ASSERT_NE(continued, nullptr);
        const auto& continuedShape = std::get<semantic::ShapeContent>(continued->content);
        EXPECT_GT(continued->transform.d * continuedShape.height, grownHeight);
        EXPECT_NEAR(continued->transform.ty + continued->transform.b * continuedShape.width * 0.5 +
                        continued->transform.d * continuedShape.height,
                    beforeBottom, 0.01);
        EXPECT_TRUE(h.facade.readTextGeometry().available);
      }
    }
}
TEST(FC02ShapeLabel, EmptyPromptAppearsOnlyWhileShapeIsSelected) {
    Host h(true);
    const auto shape = labelShape();
    seedShape(h, shape);
    runtime::RuntimeFrameOpportunity frame;
    ASSERT_TRUE(h.runtime.serviceFrameOpportunity(false, frame));
    std::vector<std::uint8_t> pixels(256 * 256 * 4);
    ASSERT_TRUE(h.runtime.readCanonicalRgba(pixels));
    const auto promptPixels = [&] {
        std::size_t dark = 0;
        for (std::size_t y = 65; y < 115; ++y)
            for (std::size_t x = 60; x < 180; ++x) {
                const auto i = (y * 256 + x) * 4;
                if (pixels[i] < 160 && pixels[i + 1] < 160 && pixels[i + 2] < 160) ++dark;
            }
        return dark;
    };
    EXPECT_EQ(promptPixels(), 0U) << "Unselected empty shapes must not show input guidance";

    ASSERT_TRUE(h.runtime.qualificationRuntime().setSelectionMode(true));
    ASSERT_TRUE(h.runtime.qualificationRuntime().selectAtViewPoint(120, 90));
    ASSERT_TRUE(h.runtime.serviceFrameOpportunity(false, frame));
    ASSERT_TRUE(h.runtime.readCanonicalRgba(pixels));
    EXPECT_GT(promptPixels(), 0U) << "Selecting an empty shape must reveal its input guidance";
    h.runtime.qualificationRuntime().clearSelection();
    ASSERT_TRUE(h.runtime.serviceFrameOpportunity(false, frame));
    ASSERT_TRUE(h.runtime.readCanonicalRgba(pixels));
    EXPECT_EQ(promptPixels(), 0U) << "Clearing selection must hide the derived input guidance";
    EXPECT_EQ(h.runtime.semanticObjectCount(), 1U);
    EXPECT_EQ(h.runtime.qualificationRuntime().submittedOperationCount(), 0U);
}
TEST(FC02ShapeLabel, ExistingTextCaretFollowsTheRenderedLabelLine) {
    Host h(true);
    auto record = labelShape();
    auto& shape = std::get<semantic::ShapeContent>(record.content);
    shape.width = 200; shape.height = 120;
    const semantic::ResourceId font{foundation::ObjectId::fromUint64(0xFC0202)};
    semantic::TextStyle style{font, 16, 400, false, false, {0, 0, 0, 1}};
    semantic::RichTextContent label;
    label.layout = semantic::RichTextLayout{200};
    label.document.paragraphs = {{foundation::ObjectId::fromUint64(815),
        {semantic::ParagraphAlignment::kLeft, 1, 0, 0}, {{"1234", style}}}};
    shape.label = label; record.kind_version = 3;
    seedShape(h, record);
    runtime::TextIntent edit;
    edit.clientId = 15; edit.requestId = ++h.request;
    edit.target = h.facade.mountedCanvasTargets().front();
    edit.kind = runtime::TextIntentKind::kEditAtPoint;
    edit.viewX = 120; edit.viewY = 90;
    ASSERT_EQ(h.facade.submitTextIntent(edit).status, runtime::TextStatus::kQueued);
    ASSERT_TRUE(h.runtime.ownerPump());
    ASSERT_EQ(h.facade.textIntentReceipt(15, h.request)->status, runtime::TextStatus::kApplied);
    auto editor = h.facade.readTextEditor();
    runtime::TextInputEvent select;
    select.token = editor.token; select.sequence = editor.nextInputSequence;
    select.kind = runtime::TextInputKind::kSelection; select.range = {4, 0};
    ASSERT_EQ(h.facade.submitTextInput(select).status, runtime::TextStatus::kQueued);
    ASSERT_TRUE(h.runtime.ownerPump());
    const auto caret = h.facade.readTextGeometry();
    ASSERT_TRUE(caret.available);
    EXPECT_GT(caret.caret[0], 55.0F) << "Caret must follow the four rendered characters";
    EXPECT_GT(caret.caret[1], 60.0F) << "Caret must share the vertically centered label line";
    EXPECT_LT(caret.caret[5], 140.0F);
}
TEST(FC02ShapeLabel, CompositionPreeditChangesRenderedShapePixelsBeforeCommit) {
    Host h(true);
    auto record = labelShape();
    auto& shape = std::get<semantic::ShapeContent>(record.content);
    shape.width = 200; shape.height = 120;
    const semantic::ResourceId font{foundation::ObjectId::fromUint64(0xFC0202)};
    semantic::TextStyle style{font, 16, 400, false, false, {0, 0, 0, 1}};
    semantic::RichTextContent label;
    label.layout = semantic::RichTextLayout{200};
    label.document.paragraphs = {{foundation::ObjectId::fromUint64(816),
        {semantic::ParagraphAlignment::kLeft, 1, 0, 0}, {{"1234", style}}}};
    shape.label = label; record.kind_version = 3;
    seedShape(h, record);
    runtime::RuntimeFrameOpportunity frame;
    ASSERT_TRUE(h.runtime.serviceFrameOpportunity(false, frame));
    std::vector<std::uint8_t> before(256 * 256 * 4), during(256 * 256 * 4);
    ASSERT_TRUE(h.runtime.readCanonicalRgba(before));
    runtime::TextIntent edit;
    edit.clientId = 15; edit.requestId = ++h.request;
    edit.target = h.facade.mountedCanvasTargets().front();
    edit.kind = runtime::TextIntentKind::kEditAtPoint;
    edit.viewX = 120; edit.viewY = 90;
    ASSERT_EQ(h.facade.submitTextIntent(edit).status, runtime::TextStatus::kQueued);
    ASSERT_TRUE(h.runtime.ownerPump());
    ASSERT_EQ(h.input(runtime::TextInputKind::kBegin).status, runtime::TextStatus::kObserved);
    ASSERT_EQ(h.input(runtime::TextInputKind::kUpdate, u"MMMM").status,
              runtime::TextStatus::kObserved);
    ASSERT_TRUE(h.runtime.serviceFrameOpportunity(false, frame));
    ASSERT_TRUE(h.runtime.readCanonicalRgba(during));
    const auto darkLabelPixels = [](const auto& pixels) {
        std::size_t count = 0;
        for (std::size_t y = 35; y < 145; ++y)
            for (std::size_t x = 25; x < 215; ++x) {
                const auto i = (y * 256 + x) * 4;
                if (pixels[i] < 120 && pixels[i + 1] < 120 && pixels[i + 2] < 120) ++count;
            }
        return count;
    };
    EXPECT_GT(darkLabelPixels(during), darkLabelPixels(before) + 30U)
        << "IME preedit must be painted into the current Shape label before commit";
    EXPECT_EQ(h.facade.readTextEditor().projection, u"1234");
    EXPECT_EQ(h.facade.readTextEditor().preedit, u"MMMM");
}
TEST(FC02ShapeLabel, PreeditOverflowGrowsThePresentationWithoutCommittingTheShape) {
    Host h(true);
    auto record = labelShape(2);
    auto& shape = std::get<semantic::ShapeContent>(record.content);
    shape.width = 80; shape.height = 40;
    seedShape(h, record);
    runtime::TextIntent edit;
    edit.clientId = 15; edit.requestId = ++h.request;
    edit.target = h.facade.mountedCanvasTargets().front();
    edit.kind = runtime::TextIntentKind::kEditAtPoint; edit.viewX = 60; edit.viewY = 50;
    ASSERT_EQ(h.facade.submitTextIntent(edit).status, runtime::TextStatus::kQueued);
    ASSERT_TRUE(h.runtime.ownerPump());
    ASSERT_EQ(h.input(runtime::TextInputKind::kBegin).status, runtime::TextStatus::kObserved);
    ASSERT_EQ(h.input(runtime::TextInputKind::kUpdate, u"A\nB\nC\nD\nE\nF").status,
              runtime::TextStatus::kObserved);
    runtime::RuntimeFrameOpportunity frame;
    ASSERT_TRUE(h.runtime.serviceFrameOpportunity(false, frame));
    std::vector<std::uint8_t> during(256 * 256 * 4), cancelled(during.size());
    ASSERT_TRUE(h.runtime.readCanonicalRgba(during));
    const auto redAboveOriginalShape = [](const auto& pixels) {
        std::size_t count = 0;
        for (std::size_t y = 5; y < 20; ++y)
            for (std::size_t x = 50; x < 70; ++x) {
                const auto i = (y * 256 + x) * 4;
                if (pixels[i] > 200 && pixels[i + 1] < 60 && pixels[i + 2] < 60) ++count;
            }
        return count;
    };
    EXPECT_GT(redAboveOriginalShape(during), 100U);
    const auto* canonical = h.runtime.qualificationRuntime().qualificationObjectRecord(record.id);
    ASSERT_NE(canonical, nullptr);
    EXPECT_EQ(canonical->transform, record.transform);
    EXPECT_EQ(h.runtime.qualificationRuntime().submittedOperationCount(), 0U);
    ASSERT_EQ(h.input(runtime::TextInputKind::kCancel).status, runtime::TextStatus::kObserved);
    ASSERT_TRUE(h.runtime.serviceFrameOpportunity(false, frame));
    ASSERT_TRUE(h.runtime.readCanonicalRgba(cancelled));
    EXPECT_EQ(redAboveOriginalShape(cancelled), 0U);
}
TEST(FC02ShapeLabel, UnselectedOverflowHasNoExpandPixelsOrExpandHit) {
    Host h(true);
    auto record=labelShape();auto& shape=std::get<semantic::ShapeContent>(record.content);
    shape.width=80;shape.height=40;record.kind_version=3;
    semantic::TextStyle style{semantic::ResourceId{foundation::ObjectId::fromUint64(0xFC0202)},
        16,400,false,false,{0,0,0,1}};
    semantic::RichTextContent label;label.layout=semantic::RichTextLayout{80};
    label.document.paragraphs={{foundation::ObjectId::fromUint64(841),
        {semantic::ParagraphAlignment::kCenter,1,0,0},{{std::string(100,'M'),style}}}};
    shape.label=label;seedShape(h,record);
    runtime::CanvasControlRequest tool;
    tool.clientId=86;tool.requestId=2;tool.target=h.facade.mountedCanvasTargets().front();
    tool.payload.kind=runtime::CanvasControlPayloadKind::kSelectTool;
    tool.payload.tool=runtime::CanvasToolKind::kSelect;
    ASSERT_EQ(h.facade.submitCanvasControl(tool).state,runtime::ProductControlState::kQueued);
    ASSERT_TRUE(h.runtime.ownerPump());
    runtime::RuntimeFrameOpportunity frame;std::vector<std::uint8_t> pixels(256*256*4);
    const auto whiteInButton=[&] {
        EXPECT_TRUE(h.runtime.serviceFrameOpportunity(false,frame));
        EXPECT_TRUE(h.runtime.readCanonicalRgba(pixels));std::size_t count=0;
        for(unsigned y=48;y<61;++y)for(unsigned x=78;x<91;++x) {
            const auto i=(y*256+x)*4;
            if(pixels[i]>240&&pixels[i+1]>240&&pixels[i+2]>240)++count;
        }
        return count;
    };
    EXPECT_EQ(whiteInButton(),0U) << "Hidden affordance must not paint";
    ASSERT_TRUE(h.runtime.registerSource(86));
    for(auto phase:{input::PointerPhase::kDown,input::PointerPhase::kUp}) {
        const auto seq=phase==input::PointerPhase::kDown?1U:2U;
        input::PlatformPointerBatch batch;
        batch.samples.push_back({86,1,seq,seq*1000,84,54,0,0,0,{},{},
            input::SampleProvenance::kConfirmedCurrent,phase});
        ASSERT_TRUE(h.runtime.submit(batch));
        if(phase==input::PointerPhase::kDown) {
            ASSERT_TRUE(h.runtime.readPreviewRgba(pixels));std::size_t white=0;
            for(unsigned y=48;y<61;++y)for(unsigned x=78;x<91;++x) {
                const auto i=(y*256+x)*4;
                if(pixels[i]>240&&pixels[i+1]>240&&pixels[i+2]>240&&pixels[i+3]>240)++white;
            }
            EXPECT_GT(white,20U) << "The selected affordance must also appear in the transient frame";
        }
    }
    EXPECT_EQ(h.runtime.qualificationRuntime().submittedOperationCount(),0U)
        << "First click selects the shape; a hidden affordance cannot expand it";
    EXPECT_GT(whiteInButton(),20U);
    h.runtime.qualificationRuntime().clearSelection();
    EXPECT_EQ(whiteInButton(),0U);
}
TEST(FC02ShapeLabel, ExpandAffordanceUsesUndoableRuntimeSizeOperation) {
    Host h(true);
    auto shapeRecord = labelShape();
    auto& shape = std::get<semantic::ShapeContent>(shapeRecord.content);
    shape.width = 80; shape.height = 40;
    semantic::TextStyle style{semantic::ResourceId{foundation::ObjectId::fromUint64(0xFC0202)},
                              16, 400, false, false, {0, 0, 0, 1}};
    semantic::RichTextContent label;
    label.layout = semantic::RichTextLayout{80};
    label.document.paragraphs = {{foundation::ObjectId::fromUint64(814),
        {semantic::ParagraphAlignment::kLeft, 1, 0, 0},
        {{"A long shape label that needs the expand affordance", style}}}};
    shape.label = label; shapeRecord.kind_version = 3;
    seedShape(h, shapeRecord);
    ASSERT_TRUE(h.runtime.qualificationRuntime().setSelectionMode(true));
    ASSERT_TRUE(h.runtime.qualificationRuntime().selectAtViewPoint(60,50));
    ASSERT_TRUE(h.runtime.registerSource(86));
    input::PlatformPointerBatch batch;
    batch.samples.push_back({86, 1, 1, 1000, 84, 54, 0, 0, 0, {}, {},
        input::SampleProvenance::kConfirmedCurrent, input::PointerPhase::kDown});
    ASSERT_TRUE(h.runtime.submit(batch));
    EXPECT_EQ(h.facade.readTextEditor().phase, runtime::TextPhase::kClosed);
    const auto* expanded = h.runtime.qualificationRuntime().qualificationObjectRecord(shapeRecord.id);
    ASSERT_NE(expanded, nullptr);
    const auto& expandedShape = std::get<semantic::ShapeContent>(expanded->content);
    EXPECT_DOUBLE_EQ(expandedShape.width, 80);
    EXPECT_GT(expandedShape.height * expanded->transform.d, 40);
    EXPECT_NEAR(expanded->transform.tx + expanded->transform.a * expandedShape.width * 0.5 +
                    expanded->transform.c * expandedShape.height,
                60, 0.01);
    EXPECT_NEAR(expanded->transform.ty + expanded->transform.b * expandedShape.width * 0.5 +
                    expanded->transform.d * expandedShape.height,
                70, 0.01);
    EXPECT_EQ(h.runtime.qualificationRuntime().submittedOperationCount(), 1U);
    batch.samples.front().phase = input::PointerPhase::kUp;
    batch.samples.front().sequence = 2;
    batch.samples.front().timestampNs = 2000;
    ASSERT_TRUE(h.runtime.submit(batch));
    EXPECT_FALSE(h.runtime.qualificationRuntime().selectionTransformActive());
    const auto target = h.facade.mountedCanvasTargets().front();
    ASSERT_EQ(h.facade.undo(2, target.runtimeEpoch, 86).state, runtime::ProductControlState::kQueued);
    ASSERT_TRUE(h.runtime.ownerPump());
    const auto* restored = h.runtime.qualificationRuntime().qualificationObjectRecord(shapeRecord.id);
    ASSERT_NE(restored, nullptr);
    EXPECT_EQ(std::get<semantic::ShapeContent>(restored->content).width, 80);
    EXPECT_EQ(std::get<semantic::ShapeContent>(restored->content).height, 40);
    EXPECT_EQ(restored->transform, shapeRecord.transform);
}
TEST(FC02ShapeLabel, ExpandButtonRemainsClickableOutsideTheEllipseContour) {
    Host h(true);
    auto record = labelShape(2);
    auto& shape = std::get<semantic::ShapeContent>(record.content);
    semantic::TextStyle style{semantic::ResourceId{foundation::ObjectId::fromUint64(0xFC0202)},
                              16, 400, false, false, {0, 0, 0, 1}};
    semantic::RichTextContent label;
    label.layout = semantic::RichTextLayout{200};
    label.document.paragraphs = {{foundation::ObjectId::fromUint64(825),
        {semantic::ParagraphAlignment::kCenter, 1, 0, 0},
        {{std::string(500, 'M'), style}}}};
    shape.label = label; record.kind_version = 3;
    seedShape(h, record);
    ASSERT_TRUE(h.runtime.qualificationRuntime().setSelectionMode(true));
    ASSERT_TRUE(h.runtime.qualificationRuntime().selectAtViewPoint(120,90));
    runtime::MemoryResourceProvider resources;
    const semantic::ResourceId font{foundation::ObjectId::fromUint64(0xFC0202)};
    std::ifstream fontFile(FC02_HOST_FONT, std::ios::binary);
    resources.publish(font.value, {std::istreambuf_iterator<char>(fontFile), {}});
    text::RichTextLayoutService layoutService(resources, {300, 1});
    layoutService.registerFont({font, std::string(minimal_runtime_host::kHostTextFontSha256), 0});
    const auto derived = layoutService.resolveShape(record.id, shape, record.transform);
    ASSERT_TRUE(derived->overflow);
    ASSERT_LE(derived->expandButton.left, 184);
    ASSERT_GE(derived->expandButton.right, 184);
    ASSERT_LE(derived->expandButton.top, 104);
    ASSERT_GE(derived->expandButton.bottom, 104);
    ASSERT_TRUE(h.runtime.registerSource(86));
    input::PlatformPointerBatch batch;
    // The Shape itself is 200x120 at (20,30); its expand affordance is
    // derived in displayed world coordinates, including the 8px inset.
    batch.samples.push_back({86, 1, 1, 1000, 204, 134, 0, 0, 0, {}, {},
        input::SampleProvenance::kConfirmedCurrent, input::PointerPhase::kDown});
    ASSERT_TRUE(h.runtime.submit(batch));
    const auto* after = h.runtime.qualificationRuntime().qualificationObjectRecord(record.id);
    ASSERT_NE(after, nullptr);
    EXPECT_GT(after->transform.d, 1);
    EXPECT_DOUBLE_EQ(after->transform.a, 1);
}
TEST(FC02ShapeLabel, ShrinkPreviewAndCommittedLabelKeepTheSameFontPixels) {
    Host h(true);
    auto record = labelShape();
    auto& shape = std::get<semantic::ShapeContent>(record.content);
    semantic::TextStyle style{semantic::ResourceId{foundation::ObjectId::fromUint64(0xFC0202)},
                              16, 400, false, false, {0, 0, 1, 1}};
    semantic::RichTextContent label;
    label.layout = semantic::RichTextLayout{200};
    label.document.paragraphs = {{foundation::ObjectId::fromUint64(819),
        {semantic::ParagraphAlignment::kCenter, 1, 0, 0},
        {{"A long label that must become an ellipsis when the shape shrinks", style}}}};
    shape.label = label; record.kind_version = 3;
    seedShape(h, record);
    auto& editor = h.runtime.qualificationRuntime();
    ASSERT_TRUE(editor.setSelectionMode(true));
    ASSERT_TRUE(editor.selectAtViewPoint(120, 90));
    ASSERT_TRUE(editor.renderSelectionOverlay());
    const auto corner = editor.selectionOverlay()->handle(render::HandleKind::kBottomRight).center;
    ASSERT_TRUE(editor.selectionPointer(18, input::PointerPhase::kDown, corner.x, corner.y));
    ASSERT_TRUE(editor.selectionPointer(18, input::PointerPhase::kMove, corner.x - 100, corner.y - 60));
    ASSERT_TRUE(editor.selectionTransformActive());
    runtime::RuntimeFrameOpportunity frame;
    ASSERT_TRUE(h.runtime.serviceFrameOpportunity(false, frame));
    std::vector<std::uint8_t> preview(256 * 256 * 4), committed(preview.size()),
                              overlay(preview.size());
    ASSERT_TRUE(h.runtime.readCanonicalRgba(preview));
    ASSERT_TRUE(h.runtime.readPreviewRgba(overlay));
    ASSERT_TRUE(editor.selectionPointer(18, input::PointerPhase::kUp, corner.x - 100, corner.y - 60));
    ASSERT_TRUE(h.runtime.serviceFrameOpportunity(false, frame));
    ASSERT_TRUE(h.runtime.readCanonicalRgba(committed));
    const auto blueMask = [](const auto& pixels) {
        std::vector<bool> mask;
        for (std::size_t y = 40; y < 80; ++y)
            for (std::size_t x = 35; x < 105; ++x) {
                const auto i = (y * 256 + x) * 4;
                mask.push_back(pixels[i] < 100 && pixels[i + 1] < 100 && pixels[i + 2] > 150);
            }
        return mask;
    };
    const auto expected = blueMask(committed);
    ASSERT_GT(std::count(expected.begin(), expected.end(), true), 20);
    EXPECT_EQ(blueMask(preview), expected)
        << "Live resize must re-layout at the new boundary without scaling the glyphs";
    EXPECT_EQ(blueMask(overlay), expected);
    const auto* after = editor.qualificationObjectRecord(record.id);
    ASSERT_NE(after, nullptr);
    EXPECT_LT(after->transform.a, 1);
    EXPECT_LT(after->transform.d, 1);
    EXPECT_EQ(std::get<semantic::ShapeContent>(after->content).label, shape.label);
}
TEST(FC02ShapeLabel, SelectedShapeEditorRetiresItsPreviousOpaqueSelectionPreview) {
    Host h(true);
    seedShape(h, labelShape());
    ASSERT_TRUE(h.runtime.setPlatformPresentationDeferred(true));
    ASSERT_TRUE(h.runtime.qualificationRuntime().setTextTool(false));
    ASSERT_TRUE(h.runtime.qualificationRuntime().setSelectionMode(true));
    ASSERT_TRUE(h.runtime.registerSource(86));
    for (const auto seq : {1U, 2U, 3U, 4U}) {
        const auto phase = seq % 2 ? input::PointerPhase::kDown : input::PointerPhase::kUp;
        input::PlatformPointerBatch batch;
        batch.samples.push_back({86, 1, seq, seq * 1000, 120, 90, 0, 0, 0, {}, {},
            input::SampleProvenance::kConfirmedCurrent, phase});
        ASSERT_TRUE(h.runtime.submit(batch));
    }
    runtime::TextIntent edit;
    edit.clientId=15; edit.requestId=++h.request;
    edit.target=h.facade.mountedCanvasTargets().front();
    edit.kind=runtime::TextIntentKind::kEditAtPoint; edit.viewX=120; edit.viewY=90;
    ASSERT_EQ(h.facade.submitTextIntent(edit).status,runtime::TextStatus::kQueued);
    ASSERT_TRUE(h.runtime.ownerPump());
    ASSERT_EQ(h.facade.textIntentReceipt(15,h.request)->status,runtime::TextStatus::kApplied);
    ASSERT_EQ(h.input(runtime::TextInputKind::kBegin).status,runtime::TextStatus::kObserved);
    ASSERT_EQ(h.input(runtime::TextInputKind::kUpdate,u"MMMM").status,runtime::TextStatus::kObserved);
    runtime::RuntimeFrameOpportunity frame;
    ASSERT_TRUE(h.runtime.serviceFrameOpportunity(false,frame));
    std::vector<std::uint8_t> pixels(256*256*4);
    ASSERT_TRUE(h.runtime.readPreviewRgba(pixels));
    std::size_t opaque=0;
    for(std::size_t y=50;y<130;++y)for(std::size_t x=40;x<200;++x)
        if(pixels[(y*256+x)*4+3])++opaque;
    EXPECT_EQ(opaque,0U) << "An old opaque selection preview must not obscure live text";
}
TEST(FC02ShapeLabel, RuntimeFacadeFormatsSelectedShapeLabelWithoutAnEditorSession) {
    Host h(true);
    auto record = labelShape();
    auto& shape = std::get<semantic::ShapeContent>(record.content);
    shape.width = 200; shape.height = 120;
    const semantic::ResourceId font{foundation::ObjectId::fromUint64(0xFC0202)};
    semantic::TextStyle style{font, 16, 400, false, false, {0, 0, 0, 1}};
    semantic::RichTextContent label;
    label.layout = semantic::RichTextLayout{200};
    label.document.paragraphs = {{foundation::ObjectId::fromUint64(817),
        {semantic::ParagraphAlignment::kLeft, 1, 0, 0}, {{"Shape label", style}}}};
    shape.label = label; record.kind_version = 3;
    seedShape(h, record);
    ASSERT_EQ(h.facade.readTextEditor().phase, runtime::TextPhase::kClosed);
    ASSERT_TRUE(h.runtime.qualificationRuntime().setSelectionMode(true));
    ASSERT_TRUE(h.runtime.qualificationRuntime().selectAtViewPoint(120, 90));
    runtime::TextIntent format;
    format.clientId = 15; format.requestId = ++h.request;
    format.target = h.facade.mountedCanvasTargets().front();
    format.kind = runtime::TextIntentKind::kFormat;
    format.object = record.id;
    format.format.bold = true;
    format.format.italic = true;
    const auto catalog = h.facade.readFontCatalog();
    ASSERT_GT(catalog.entries.size(), 1U);
    const auto nextFont = std::find_if(catalog.entries.begin(), catalog.entries.end(),
        [&](const auto& f) { return f.id != font.value; });
    ASSERT_NE(nextFont, catalog.entries.end());
    format.format.font = nextFont->id;
    format.format.fontSize = 24.0;
    ASSERT_EQ(h.facade.submitTextIntent(format).status, runtime::TextStatus::kQueued);
    ASSERT_TRUE(h.runtime.ownerPump());
    const auto receipt = h.facade.textIntentReceipt(15, h.request);
    ASSERT_NE(receipt, std::nullopt);
    EXPECT_EQ(receipt->status, runtime::TextStatus::kApplied);
    const auto* updated = h.runtime.qualificationRuntime().qualificationObjectRecord(record.id);
    ASSERT_NE(updated, nullptr);
    const auto* updatedShape = std::get_if<semantic::ShapeContent>(&updated->content);
    ASSERT_NE(updatedShape, nullptr);
    ASSERT_TRUE(updatedShape->label.has_value());
    ASSERT_FALSE(updatedShape->label->document.paragraphs.front().runs.empty());
    const auto& updatedStyle = updatedShape->label->document.paragraphs.front().runs.front().style;
    EXPECT_EQ(updatedStyle.weight, 700);
    EXPECT_DOUBLE_EQ(updatedStyle.font_size, 24.0);
    EXPECT_TRUE(updatedStyle.italic);
    EXPECT_EQ(updatedStyle.font_resource_id->value, nextFont->id);
    EXPECT_EQ(h.facade.readTextEditor().formatObject, record.id);
    EXPECT_TRUE(h.facade.readTextEditor().format.bold.value_or(false));
    EXPECT_EQ(h.runtime.qualificationRuntime().submittedOperationCount(), 1U);
    EXPECT_EQ(h.facade.readTextEditor().phase, runtime::TextPhase::kClosed);
    const auto target = h.facade.mountedCanvasTargets().front();
    ASSERT_EQ(h.facade.undo(2, target.runtimeEpoch, 86).state, runtime::ProductControlState::kQueued);
    ASSERT_TRUE(h.runtime.ownerPump());
    EXPECT_EQ(*h.runtime.qualificationRuntime().qualificationObjectRecord(record.id), record);
    ASSERT_EQ(h.facade.redo(3, target.runtimeEpoch, 86).state, runtime::ProductControlState::kQueued);
    ASSERT_TRUE(h.runtime.ownerPump());
    EXPECT_TRUE(h.facade.readTextEditor().format.bold.value_or(false));
}
TEST(FC02ShapeLabel, WholeLabelFormattingPreservesEditorAndRejectsStaleTargets) {
    Host h(true); seedShape(h, labelShape());
    runtime::TextIntent edit;
    edit.clientId=15; edit.requestId=++h.request;
    edit.target=h.facade.mountedCanvasTargets().front();
    edit.kind=runtime::TextIntentKind::kEditAtPoint; edit.viewX=120; edit.viewY=90;
    ASSERT_EQ(h.facade.submitTextIntent(edit).status,runtime::TextStatus::kQueued);
    ASSERT_TRUE(h.runtime.ownerPump());
    ASSERT_EQ(h.input(runtime::TextInputKind::kReplace,u"label").status,runtime::TextStatus::kApplied);
    const auto before=h.facade.readTextEditor();
    ASSERT_EQ(before.formatObject,labelShape().id);
    const auto operations=h.runtime.qualificationRuntime().submittedOperationCount();
    runtime::TextIntent f;
    f.clientId=15; f.requestId=++h.request; f.target=edit.target;
    f.kind=runtime::TextIntentKind::kFormat; f.token=before.token; f.object=before.formatObject;
    f.format.bold=true; f.format.italic=true;
    ASSERT_EQ(h.facade.submitTextIntent(f).status,runtime::TextStatus::kQueued);
    ASSERT_TRUE(h.runtime.ownerPump());
    EXPECT_EQ(h.facade.textIntentReceipt(15,h.request)->status,runtime::TextStatus::kApplied);
    auto after=h.facade.readTextEditor();
    EXPECT_EQ(after.phase,runtime::TextPhase::kEditing);
    EXPECT_EQ(after.selection,before.selection);
    EXPECT_EQ(after.projection,before.projection);
    EXPECT_EQ(after.token.session,before.token.session);
    EXPECT_TRUE(after.format.bold.value_or(false)); EXPECT_TRUE(after.format.italic.value_or(false));
    EXPECT_EQ(h.runtime.qualificationRuntime().submittedOperationCount(),operations+1);
    // A captured request from the old projection cannot mutate the new one.
    f.requestId=++h.request; f.format.bold=false;
    ASSERT_EQ(h.facade.submitTextIntent(f).status,runtime::TextStatus::kQueued);
    ASSERT_TRUE(h.runtime.ownerPump());
    EXPECT_EQ(h.facade.textIntentReceipt(15,h.request)->status,runtime::TextStatus::kStale);
    EXPECT_EQ(h.runtime.qualificationRuntime().submittedOperationCount(),operations+1);
    // An actual character selection retains range formatting and readback.
    runtime::TextInputEvent selection;
    selection.token=after.token; selection.sequence=after.nextInputSequence;
    selection.kind=runtime::TextInputKind::kSelection; selection.range={1,2};
    ASSERT_EQ(h.facade.submitTextInput(selection).status,runtime::TextStatus::kQueued);
    ASSERT_TRUE(h.runtime.ownerPump());
    after=h.facade.readTextEditor(); EXPECT_TRUE(after.formatObject.isZero());
    f.requestId=++h.request; f.object={}; f.token=after.token; f.format.italic=false;
    ASSERT_EQ(h.facade.submitTextIntent(f).status,runtime::TextStatus::kQueued);
    ASSERT_TRUE(h.runtime.ownerPump());
    EXPECT_EQ(h.facade.textIntentReceipt(15,h.request)->status,runtime::TextStatus::kApplied);
    const auto* object=h.runtime.qualificationRuntime().qualificationObjectRecord(before.token.object);
    ASSERT_NE(object,nullptr);
    const auto& runs=std::get<semantic::ShapeContent>(object->content).label->document.paragraphs.front().runs;
    ASSERT_EQ(runs.size(),3U); EXPECT_TRUE(runs.front().style.italic); EXPECT_FALSE(runs[1].style.italic);
    ASSERT_EQ(h.intent(runtime::TextIntentKind::kFinish).status,runtime::TextStatus::kApplied);
    ASSERT_TRUE(h.runtime.qualificationRuntime().setSelectionMode(true));
    ASSERT_TRUE(h.runtime.qualificationRuntime().selectAtViewPoint(120,90));
    f.token={}; f.object=before.token.object; f.requestId=++h.request;
    ASSERT_EQ(h.facade.submitTextIntent(f).status,runtime::TextStatus::kQueued);
    h.runtime.qualificationRuntime().clearSelection();
    ASSERT_TRUE(h.runtime.ownerPump());
    EXPECT_EQ(h.facade.textIntentReceipt(15,h.request)->status,runtime::TextStatus::kStale);
}
TEST(FC02ShapeLabel, EmptyLabelCanBeFormattedAndLostWritesAreRejected) {
    Host h(true); seedShape(h,labelShape());
    ASSERT_TRUE(h.runtime.qualificationRuntime().setSelectionMode(true));
    ASSERT_TRUE(h.runtime.qualificationRuntime().selectAtViewPoint(120,90));
    runtime::TextIntent f;
    f.clientId=15; f.requestId=++h.request; f.target=h.facade.mountedCanvasTargets().front();
    f.kind=runtime::TextIntentKind::kFormat; f.object=labelShape().id;
    f.format.fontSize=24; f.format.bold=true;
    ASSERT_EQ(h.facade.submitTextIntent(f).status,runtime::TextStatus::kQueued);
    ASSERT_TRUE(h.runtime.ownerPump());
    ASSERT_EQ(h.facade.textIntentReceipt(15,h.request)->status,runtime::TextStatus::kApplied);
    EXPECT_EQ(h.runtime.semanticObjectCount(),1U);
    EXPECT_TRUE(h.facade.readTextEditor().format.bold.value_or(false));
    const auto count=h.runtime.qualificationRuntime().submittedOperationCount();
    ASSERT_TRUE(h.runtime.loseSurface());
    f.requestId=++h.request; f.format.bold=false;
    const auto queued=h.facade.submitTextIntent(f);
    if(queued.status==runtime::TextStatus::kQueued){
        (void)h.runtime.ownerPump();
        EXPECT_NE(h.facade.textIntentReceipt(15,h.request)->status,runtime::TextStatus::kApplied);
    } else EXPECT_NE(queued.status,runtime::TextStatus::kApplied);
    EXPECT_EQ(h.runtime.qualificationRuntime().submittedOperationCount(),count);
}
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

TEST(FC02Geometry, DrawnShapeFirstLabelHasRealCaretPreeditAndCanonicalPixels) {
    using namespace runtime;
    Host h(true);
    ASSERT_TRUE(h.runtime.registerSource(39));
    CanvasControlRequest control;
    control.clientId = 39;
    control.requestId = 1;
    control.target = h.facade.mountedCanvasTargets().front();
    control.payload.kind = CanvasControlPayloadKind::kSelectCreationPreset;
    control.payload.creationPreset = CanvasCreationPreset::kRectangle;
    ASSERT_EQ(h.facade.submitCanvasControl(control).state, ProductControlState::kQueued);
    ASSERT_TRUE(h.runtime.ownerPump());
    const auto pointer = [&](std::uint64_t seq, input::PointerPhase phase, float x, float y) {
        input::PlatformPointerBatch batch;
        batch.samples.push_back({39, 1, seq, seq * 1000, x, y, 0, 0, 0, {}, {},
                                 input::SampleProvenance::kConfirmedCurrent, phase});
        return h.runtime.submit(batch);
    };
    ASSERT_TRUE(pointer(1, input::PointerPhase::kDown, 20, 30));
    ASSERT_TRUE(pointer(2, input::PointerPhase::kMove, 220, 130));
    ASSERT_TRUE(pointer(3, input::PointerPhase::kUp, 220, 130));
    ASSERT_EQ(h.runtime.semanticObjectCount(), 1U);
    const auto operations = h.runtime.qualificationRuntime().submittedOperationCount();
    control.requestId++;
    control.payload.kind = CanvasControlPayloadKind::kSelectTool;
    control.payload.tool = CanvasToolKind::kText;
    ASSERT_EQ(h.facade.submitCanvasControl(control).state, ProductControlState::kQueued);
    ASSERT_TRUE(h.runtime.ownerPump());
    ASSERT_EQ(h.intent(TextIntentKind::kEditAtPoint).status, TextStatus::kApplied);
    const auto token = h.facade.readTextEditor().token;
    EXPECT_TRUE(h.facade.readTextGeometry().available);
    EXPECT_EQ(h.facade.readTextEditor().width, 200);
    ASSERT_EQ(h.input(TextInputKind::kBegin).status, TextStatus::kObserved);
    ASSERT_EQ(h.input(TextInputKind::kUpdate, u"\u4f60\u597d").status, TextStatus::kObserved);
    EXPECT_TRUE(h.facade.readTextGeometry().available);
    EXPECT_EQ(h.runtime.qualificationRuntime().submittedOperationCount(), operations);
    ASSERT_EQ(h.input(TextInputKind::kCommit, u"\u4f60\u597d").status, TextStatus::kApplied);
    EXPECT_EQ(h.runtime.semanticObjectCount(), 1U);
    EXPECT_EQ(h.facade.readTextEditor().token.object, token.object);
    ASSERT_EQ(h.intent(TextIntentKind::kFinish).status, TextStatus::kApplied);
    const auto* committed = h.runtime.qualificationRuntime().qualificationObjectRecord(token.object);
    ASSERT_NE(committed, nullptr);
    const auto* committedShape = std::get_if<semantic::ShapeContent>(&committed->content);
    ASSERT_NE(committedShape, nullptr);
    ASSERT_TRUE(committedShape->label.has_value());
    ASSERT_FALSE(committedShape->label->document.paragraphs.empty());
    ASSERT_FALSE(committedShape->label->document.paragraphs.front().runs.empty());
    EXPECT_EQ(committedShape->label->document.paragraphs.front().runs.front().text,
              std::string("\xE4\xBD\xA0\xE5\xA5\xBD", 6));
    RuntimeFrameOpportunity frame;
    ASSERT_TRUE(h.runtime.serviceFrameOpportunity(false, frame));
    std::vector<std::uint8_t> pixels(256 * 256 * 4);
    ASSERT_TRUE(h.runtime.readCanonicalRgba(pixels));
    std::size_t dark = 0;
    // The first Shape paragraph is centered within its padded label viewport.
    for (unsigned y = 35; y < 85; ++y)
        for (unsigned x = 75; x < 165; ++x) {
            const auto i = (y * 256U + x) * 4U;
            if (pixels[i] < 100 && pixels[i + 1] < 100 && pixels[i + 2] < 100) ++dark;
        }
    EXPECT_GT(dark, 30U);
    EXPECT_EQ(h.runtime.qualificationRuntime().submittedOperationCount(), operations + 1U);
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
TEST(FC08SectionTextGeometry, DoubleClickGroupedShapeEntersScopeBeforeTextEditing) {
    Host h(true);auto a=labelShape();a.content=semantic::ShapeContent{1,80,60};
    auto b=a;b.id=foundation::ObjectId::fromUint64(8812);b.placement.order_key=semantic::OrderKey({2});b.transform.tx=150;
    seedShape(h,a);
    const semantic::Operation seed{semantic::OperationId{foundation::ObjectId::fromUint64(8813)},
        semantic::DocumentId{foundation::ObjectId::fromUint64(1)},1,1,semantic::InsertObjectsOp{{b}}};
    ASSERT_EQ(h.runtime.applyExternalOperation(seed,semantic::ApplySource::kRemoteSync).disposition,semantic::ApplyDisposition::kApplied);
    const auto group=foundation::ObjectId::fromUint64(8814);
    ASSERT_EQ(h.facade.groupObjects({a.id,b.id},group,1,8815).state,runtime::ProductControlState::kQueued);
    ASSERT_TRUE(h.runtime.ownerPump());
    ASSERT_EQ(h.facade.setSelectionMode(true,2,h.facade.readRuntimeState().identity.runtimeGeneration,8815).state,runtime::ProductControlState::kQueued);
    ASSERT_TRUE(h.runtime.ownerPump());
    runtime::TextIntent edit;edit.clientId=8815;edit.target=h.facade.mountedCanvasTargets().front();
    edit.kind=runtime::TextIntentKind::kEditAtPoint;edit.viewX=40;edit.viewY=50;edit.requestId=3;
    ASSERT_EQ(h.facade.submitTextIntent(edit).status,runtime::TextStatus::kQueued);ASSERT_TRUE(h.runtime.ownerPump());
    EXPECT_EQ(h.facade.readSelectionSummary().groupScope,group);EXPECT_EQ(h.facade.readSelectionSummary().primary,a.id);
    EXPECT_EQ(h.facade.readTextEditor().phase,runtime::TextPhase::kClosed);
    edit.requestId=4;ASSERT_EQ(h.facade.submitTextIntent(edit).status,runtime::TextStatus::kQueued);ASSERT_TRUE(h.runtime.ownerPump());
    EXPECT_EQ(h.facade.readTextEditor().phase,runtime::TextPhase::kEditing);
    EXPECT_EQ(h.facade.readTextEditor().token.object,a.id);
    EXPECT_TRUE(h.runtime.close());
}

TEST(FC08SectionTextGeometry, WrappedShapeEditingFollowsAncestorWorldPose) {
    Host h(true);auto record=labelShape();seedShape(h,record);
    const auto edit=[&](float x,float y) {
        runtime::TextIntent intent;intent.clientId=15;intent.requestId=++h.request;
        intent.target=h.facade.mountedCanvasTargets().front();intent.kind=runtime::TextIntentKind::kEditAtPoint;
        intent.viewX=x;intent.viewY=y;
        EXPECT_EQ(h.facade.submitTextIntent(intent).status,runtime::TextStatus::kQueued);
        EXPECT_TRUE(h.runtime.ownerPump());EXPECT_EQ(h.facade.textIntentReceipt(15,h.request)->status,runtime::TextStatus::kApplied);
    };
    edit(120,90);ASSERT_EQ(h.input(runtime::TextInputKind::kReplace,u"Section child text").status,runtime::TextStatus::kApplied);
    ASSERT_EQ(h.intent(runtime::TextIntentKind::kFinish).status,runtime::TextStatus::kApplied);
    edit(120,90);const auto before=h.facade.readTextGeometry();ASSERT_TRUE(before.available);
    ASSERT_EQ(h.intent(runtime::TextIntentKind::kFinish).status,runtime::TextStatus::kApplied);
    runtime::SectionIntent wrap;wrap.action=runtime::SectionAction::kWrap;wrap.objects={record.id};
    wrap.objectId=foundation::ObjectId::fromUint64(6001);
    ASSERT_EQ(h.facade.submitSectionIntent(wrap,1,8806).state,runtime::ProductControlState::kQueued);ASSERT_TRUE(h.runtime.ownerPump());
    runtime::CanvasControlRequest move;move.clientId=8806;move.requestId=2;move.target=h.facade.mountedCanvasTargets()[0];
    move.payload.kind=runtime::CanvasControlPayloadKind::kSelectedTransform;move.payload.transform={1,0,0,1,30,40};
    ASSERT_EQ(h.facade.submitCanvasControl(move).state,runtime::ProductControlState::kQueued);ASSERT_TRUE(h.runtime.ownerPump());
    edit(150,130);const auto after=h.facade.readTextGeometry();ASSERT_TRUE(after.available);
    EXPECT_NEAR(after.caret[0]-before.caret[0],30,0.01F);EXPECT_NEAR(after.caret[1]-before.caret[1],40,0.01F);
}
TEST(FC08SectionTextGeometry, MetadataTitleUsesRegisteredFontAndDrawsGlyphPixels) {
    Host h(true);
    runtime::SectionIntent intent;intent.objectId=foundation::ObjectId::fromUint64(6002);
    intent.content.title="Section title";intent.transform.tx=10;intent.transform.ty=10;
    ASSERT_EQ(h.facade.submitSectionIntent(intent,1,8807).state,runtime::ProductControlState::kQueued);
    ASSERT_TRUE(h.runtime.ownerPump());
    const auto before=h.facade.readSection(intent.objectId);
    EXPECT_TRUE(before.titleLayoutReady);EXPECT_GT(before.titleGlyphCount,0U);EXPECT_TRUE(before.titleDiagnostics.empty());
    runtime::RuntimeFrameOpportunity frame;ASSERT_TRUE(h.runtime.serviceFrameOpportunity(false,frame));
    std::vector<std::uint8_t> titlePixels(256*256*4);ASSERT_TRUE(h.runtime.readCanonicalRgba(titlePixels));
    intent.action=runtime::SectionAction::kRename;intent.content.title="";
    ASSERT_EQ(h.facade.submitSectionIntent(intent,2,8807).state,runtime::ProductControlState::kQueued);
    ASSERT_TRUE(h.runtime.ownerPump());ASSERT_TRUE(h.runtime.serviceFrameOpportunity(false,frame));
    std::vector<std::uint8_t> blankPixels(titlePixels.size());ASSERT_TRUE(h.runtime.readCanonicalRgba(blankPixels));
    EXPECT_NE(titlePixels,blankPixels)<<"Plain metadata titles must produce glyph pixels";
}
TEST(FC08SectionTextGeometry, LateFontPublicationRefreshesMetadataTitle) {
    runtime::RuntimeCompositionCore composition;ASSERT_TRUE(composition.open());ASSERT_TRUE(composition.bindSurface(256,256));
    auto& facade=composition.controlFacade();runtime::SectionIntent intent;
    intent.objectId=foundation::ObjectId::fromUint64(6003);intent.content.title="Late font";
    ASSERT_EQ(facade.submitSectionIntent(intent,1,8807).state,runtime::ProductControlState::kQueued);
    ASSERT_TRUE(composition.ownerPump());EXPECT_FALSE(facade.readSection(intent.objectId).titleLayoutReady);
    const auto generation=composition.readback().semanticGeneration;
    ASSERT_TRUE(minimal_runtime_host::bindHostTextFonts(composition,FC02_HOST_FONT,FC02_FONT_CATALOG_ROOT));
    const auto after=facade.readSection(intent.objectId);
    EXPECT_EQ(composition.readback().semanticGeneration,generation);EXPECT_TRUE(after.titleLayoutReady);
    EXPECT_GT(after.titleGlyphCount,0U);EXPECT_TRUE(after.titleDiagnostics.empty());
}

void selectTestRegion(Host& h,float x,float y,float right,float bottom) {
    ASSERT_EQ(h.facade.setSelectionAcquisitionMode(runtime::CanvasSelectionMode::kRectangle,++h.request,6001).state,runtime::ProductControlState::kQueued);
    ASSERT_TRUE(h.runtime.ownerPump());
    const auto send=[&](input::PointerPhase phase,float px,float py) {
        input::PlatformPointerSample p;p.source=6001;p.pointer=1;p.sequence=++h.request;p.timestampNs=h.request*1000000;
        p.phase=phase;p.x=px;p.y=py;input::PlatformPointerBatch batch;batch.samples={p};return h.runtime.submit(batch);
    };
    ASSERT_TRUE(send(input::PointerPhase::kDown,x,y));ASSERT_TRUE(send(input::PointerPhase::kMove,right,bottom));
    ASSERT_TRUE(send(input::PointerPhase::kUp,right,bottom));
}
TEST(SelectionBatch2Text, RealFontTextRegionUsesPublishedLayoutNotWrapBox) {
    Host h;ASSERT_TRUE(h.runtime.registerSource(6001));
    ASSERT_EQ(h.intent(runtime::TextIntentKind::kCreate).status,runtime::TextStatus::kApplied);
    ASSERT_EQ(h.input(runtime::TextInputKind::kReplace,u"Hello").status,runtime::TextStatus::kApplied);
    ASSERT_EQ(h.intent(runtime::TextIntentKind::kFinish).status,runtime::TextStatus::kApplied);
    const auto before=h.runtime.readback();const auto target=h.facade.mountedCanvasTargets().front();
    const auto flags=h.facade.readCanvasControlSnapshot(target);
    selectTestRegion(h,18,28,85,65);
    auto state=h.facade.readCanvasControlSnapshot(target);EXPECT_EQ(state.selectionQueryStatus,runtime::SelectionQueryStatus::kComplete);
    ASSERT_EQ(state.selectedObjectCount,1U);
    EXPECT_EQ(h.runtime.qualificationRuntime().qualificationObjectRecord(*state.selectedObject)->kind,semantic::ObjectKind::kRichText);
    selectTestRegion(h,170,35,185,45);EXPECT_EQ(h.facade.readCanvasControlSnapshot(target).selectedObjectCount,0U);
    const auto after=h.runtime.readback();EXPECT_EQ(after.semanticGeneration,before.semanticGeneration);
    const auto unchanged=h.facade.readCanvasControlSnapshot(target);EXPECT_EQ(unchanged.lastAppliedOperation,flags.lastAppliedOperation);
    EXPECT_EQ(unchanged.canUndo,flags.canUndo);EXPECT_EQ(unchanged.canRedo,flags.canRedo);
}
TEST(SelectionBatch2Text, RealStickyRichTextChildIsOneUserFacingRoot) {
    Host h;ASSERT_TRUE(h.runtime.registerSource(6001));
    semantic::ObjectRecord record;record.id=foundation::ObjectId::fromUint64(9901);record.kind=semantic::ObjectKind::kSticky;
    record.kind_version=1;record.placement.order_key=semantic::OrderKey({1});record.transform.tx=20;record.transform.ty=30;
    record.content=semantic::StickyContent{200,200};seedSticky(h,record);
    runtime::TextIntent edit;edit.kind=runtime::TextIntentKind::kEditAtPoint;edit.object=record.id;edit.viewX=40;edit.viewY=50;
    edit.clientId=15;edit.requestId=++h.request;edit.target=h.facade.mountedCanvasTargets().front();
    ASSERT_EQ(h.facade.submitTextIntent(edit).status,runtime::TextStatus::kQueued);ASSERT_TRUE(h.runtime.ownerPump());
    ASSERT_EQ(h.facade.textIntentReceipt(15,h.request)->status,runtime::TextStatus::kApplied);
    ASSERT_EQ(h.input(runtime::TextInputKind::kReplace,u"Sticky label").status,runtime::TextStatus::kApplied);
    ASSERT_EQ(h.intent(runtime::TextIntentKind::kFinish).status,runtime::TextStatus::kApplied);
    ASSERT_EQ(h.runtime.semanticObjectCount(),2U);const auto before=h.runtime.readback();
    selectTestRegion(h,10,20,230,240);
    EXPECT_EQ(h.facade.readSection().selectedRoots,std::vector{record.id});
    EXPECT_EQ(h.facade.readCanvasControlSnapshot(edit.target).selectionQueryStatus,runtime::SelectionQueryStatus::kComplete);
    EXPECT_EQ(h.runtime.readback().semanticGeneration,before.semanticGeneration);EXPECT_EQ(h.runtime.semanticObjectCount(),2U);
}

TEST(SelectionBatch2Text, UnavailableFontGeometryDoesNotBecomeAnEmptySuccessfulQuery) {
    Host h;ASSERT_TRUE(h.runtime.registerSource(6001));
    // Retain the admitted font manifest without publishing its bytes in
    // a fresh composition, rather than manufacture a layout snapshot.
    runtime::RuntimeCompositionCore cold;ASSERT_TRUE(cold.open());ASSERT_TRUE(cold.bindSurface(256,256));
    ASSERT_TRUE(cold.bindManifest(h.runtime.resources().manifest,1));ASSERT_TRUE(cold.registerSource(6002));auto& facade=cold.controlFacade();
    runtime::StickyCreateRequest sticky;sticky.objectId=foundation::ObjectId::fromUint64(9970);sticky.transform.tx=200;sticky.transform.ty=200;sticky.content={30,30};
    ASSERT_EQ(facade.createSticky(sticky).state,runtime::ConnectorCommandState::kApplied);
    ASSERT_TRUE(cold.qualificationRuntime().setSelectionMode(true));ASSERT_TRUE(cold.qualificationRuntime().selectAtViewPoint(215,215));
    semantic::TextStyle style;style.font_resource_id=semantic::ResourceId{foundation::ObjectId::fromUint64(500)};style.font_size=16;style.weight=400;style.color={0,0,0,1};
    semantic::Paragraph paragraph;paragraph.id=foundation::ObjectId::fromUint64(9972);paragraph.style.alignment=semantic::ParagraphAlignment::kLeft;paragraph.style.line_height=1.2;paragraph.runs={{"missing font geometry",style}};
    semantic::RichTextContent text;text.document.paragraphs={paragraph};
    semantic::ObjectRecord record;record.id=foundation::ObjectId::fromUint64(9971);record.kind=semantic::ObjectKind::kRichText;record.kind_version=1;
    record.content=text;record.transform.tx=100;record.transform.ty=100;record.placement.order_key=semantic::OrderKey({2});
    semantic::Operation insert{semantic::OperationId{foundation::ObjectId::fromUint64(9973)},semantic::DocumentId{foundation::ObjectId::fromUint64(1)},1,1,semantic::InsertObjectsOp{{record}}};
    ASSERT_EQ(cold.applyExternalOperation(insert,semantic::ApplySource::kRemoteSync).disposition,semantic::ApplyDisposition::kApplied);
    ASSERT_EQ(facade.setSelectionAcquisitionMode(runtime::CanvasSelectionMode::kRectangle,1,6002).state,runtime::ProductControlState::kQueued);ASSERT_TRUE(cold.ownerPump());
    std::uint64_t seq=0;const auto send=[&](input::PointerPhase phase,float x,float y) {
        input::PlatformPointerSample p;p.source=6002;p.pointer=1;p.sequence=++seq;p.timestampNs=seq*1000000;p.phase=phase;p.x=x;p.y=y;
        input::PlatformPointerBatch batch;batch.samples={p};return cold.submit(batch);
    };
    ASSERT_TRUE(send(input::PointerPhase::kDown,110,105));ASSERT_TRUE(send(input::PointerPhase::kMove,145,125));ASSERT_TRUE(send(input::PointerPhase::kUp,145,125));
    const auto state=facade.readCanvasControlSnapshot(facade.mountedCanvasTargets().front());
    EXPECT_EQ(state.selectionQueryStatus,runtime::SelectionQueryStatus::kPartial);
    EXPECT_EQ(facade.readSection().selectedRoots,std::vector{sticky.objectId});
    EXPECT_EQ(state.regionUnsupportedKinds,std::vector<std::uint32_t>{static_cast<unsigned>(semantic::ObjectKind::kRichText)});
    EXPECT_TRUE(cold.close());
}
TEST(SelectionBatch2Text, PartiallyAvailableMixedFontGeometryAlsoPreservesSelection) {
    Host h;ASSERT_TRUE(h.runtime.registerSource(6001));
    // Retain the admitted font manifest without publishing its bytes in
    // a fresh composition, rather than manufacture a layout snapshot.
    runtime::RuntimeCompositionCore cold;ASSERT_TRUE(cold.open());ASSERT_TRUE(cold.bindSurface(256,256));
    auto manifest=h.runtime.resources().manifest;auto missing=manifest.entries.front();
    missing.resource_id=semantic::ResourceId{foundation::ObjectId::fromUint64(501)};manifest.entries.push_back(missing);
    ASSERT_TRUE(cold.bindManifest(manifest,1));
    std::ifstream file(FC02_LATIN_FONT,std::ios::binary);std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(file),{}};
    ASSERT_FALSE(bytes.empty());ASSERT_TRUE(cold.publishResource(foundation::ObjectId::fromUint64(500),std::move(bytes)));ASSERT_TRUE(cold.ownerPump());
    ASSERT_TRUE(cold.registerSource(6002));auto& facade=cold.controlFacade();
    runtime::StickyCreateRequest sticky;sticky.objectId=foundation::ObjectId::fromUint64(9970);sticky.transform.tx=200;sticky.transform.ty=200;sticky.content={30,30};
    ASSERT_EQ(facade.createSticky(sticky).state,runtime::ConnectorCommandState::kApplied);
    ASSERT_TRUE(cold.qualificationRuntime().setSelectionMode(true));ASSERT_TRUE(cold.qualificationRuntime().selectAtViewPoint(215,215));
    semantic::TextStyle style;style.font_resource_id=semantic::ResourceId{foundation::ObjectId::fromUint64(500)};style.font_size=16;style.weight=400;style.color={0,0,0,1};
    semantic::Paragraph paragraph;paragraph.id=foundation::ObjectId::fromUint64(9972);paragraph.style.alignment=semantic::ParagraphAlignment::kLeft;paragraph.style.line_height=1.2;paragraph.runs={{"missing font geometry",style}};
    auto unavailable=paragraph;unavailable.id=foundation::ObjectId::fromUint64(9974);
    unavailable.runs.front().style.font_resource_id=semantic::ResourceId{foundation::ObjectId::fromUint64(501)};
    semantic::RichTextContent text;text.document.paragraphs={paragraph,unavailable};
    semantic::ObjectRecord record;record.id=foundation::ObjectId::fromUint64(9971);record.kind=semantic::ObjectKind::kRichText;record.kind_version=1;
    record.content=text;record.transform.tx=100;record.transform.ty=100;record.placement.order_key=semantic::OrderKey({2});
    semantic::Operation insert{semantic::OperationId{foundation::ObjectId::fromUint64(9973)},semantic::DocumentId{foundation::ObjectId::fromUint64(1)},1,1,semantic::InsertObjectsOp{{record}}};
    ASSERT_EQ(cold.applyExternalOperation(insert,semantic::ApplySource::kRemoteSync).disposition,semantic::ApplyDisposition::kApplied);
    ASSERT_EQ(facade.setSelectionAcquisitionMode(runtime::CanvasSelectionMode::kRectangle,1,6002).state,runtime::ProductControlState::kQueued);ASSERT_TRUE(cold.ownerPump());
    std::uint64_t seq=0;const auto send=[&](input::PointerPhase phase,float x,float y) {
        input::PlatformPointerSample p;p.source=6002;p.pointer=1;p.sequence=++seq;p.timestampNs=seq*1000000;p.phase=phase;p.x=x;p.y=y;
        input::PlatformPointerBatch batch;batch.samples={p};return cold.submit(batch);
    };
    ASSERT_TRUE(send(input::PointerPhase::kDown,110,105));ASSERT_TRUE(send(input::PointerPhase::kMove,145,125));ASSERT_TRUE(send(input::PointerPhase::kUp,145,125));
    const auto state=facade.readCanvasControlSnapshot(facade.mountedCanvasTargets().front());
    EXPECT_EQ(state.selectionQueryStatus,runtime::SelectionQueryStatus::kPartial);
    EXPECT_EQ(facade.readSection().selectedRoots,std::vector{sticky.objectId});
    EXPECT_EQ(state.regionUnsupportedKinds,std::vector<std::uint32_t>{static_cast<unsigned>(semantic::ObjectKind::kRichText)});
    EXPECT_TRUE(cold.close());
}

TEST(SelectionBatch2Text, UnfilledShapeVisibleLabelParticipatesInRegionSelection) {
    Host h;ASSERT_TRUE(h.runtime.registerSource(6001));auto record=labelShape();record.kind_version=3;record.transform={};
    std::get<semantic::ShapeContent>(record.content).width=100;std::get<semantic::ShapeContent>(record.content).height=100;
    semantic::Paragraph paragraph;paragraph.id=foundation::ObjectId::fromUint64(9980);
    paragraph.style.alignment=semantic::ParagraphAlignment::kCenter;paragraph.style.line_height=1.2;
    semantic::TextStyle style;style.font_resource_id=semantic::ResourceId{foundation::ObjectId::fromUint64(500)};
    style.font_size=16;style.weight=400;style.color={0,0,0,1};paragraph.runs={{"AB",style}};
    semantic::RichTextContent label;label.layout=semantic::RichTextLayout{100};label.document.paragraphs={paragraph};std::get<semantic::ShapeContent>(record.content).label=label;
    record.properties.entries={{0x100U,semantic::FillStyleValue{semantic::NoFill{}}},
        {0x101U,semantic::StrokeStyleValue{semantic::SolidStroke{{0,0,0,1},2,semantic::StrokeCap::kButt,semantic::MiterJoin{4},semantic::SolidDash{}}}}};
    seedShape(h,record);ASSERT_EQ(h.runtime.semanticObjectCount(),1U);const auto before=h.runtime.readback();selectTestRegion(h,45,45,49,49);
    EXPECT_EQ(h.facade.readCanvasControlSnapshot(h.facade.mountedCanvasTargets().front()).selectionQueryStatus,runtime::SelectionQueryStatus::kComplete);
    EXPECT_EQ(h.facade.readSection().selectedRoots,std::vector{record.id});
    EXPECT_EQ(h.runtime.readback().semanticGeneration,before.semanticGeneration);
}
} // namespace
