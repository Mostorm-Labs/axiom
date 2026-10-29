#include "minimal_runtime_facade.hpp"
#include "host_font_resources.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

namespace {
void require(bool condition, std::string_view expression, int line) {
    if (condition) return;
    std::fprintf(stderr, "line %d: %.*s\n", line, static_cast<int>(expression.size()), expression.data());
    std::abort();
}
}
#define REQUIRE(expression) require(static_cast<bool>(expression), #expression, __LINE__)

int main() {
    using namespace canvas;
    using namespace canvas::runtime;
    minimal_runtime_host::MinimalRuntimeHost host;
    auto& composition = host.composition();
    REQUIRE(composition.bindSurface(640, 480));
    REQUIRE(minimal_runtime_host::bindHostTextFont(composition, FC02_HOST_FONT));
    minimal_runtime_host::MinimalRuntimeFacade adapter(host);
    RuntimeFacade& consumer = adapter;
    REQUIRE(consumer.setMultiSelectionPermission(MultiSelectionPermissionMode::kLocalSingleUser));
    const auto publicMulti = consumer.readMultiSelection();
    const auto ownerMulti = composition.controlFacade().readMultiSelection();
    REQUIRE(publicMulti.available && ownerMulti.available);
    REQUIRE(publicMulti.stamp == ownerMulti.stamp && publicMulti.roots == ownerMulti.roots);
    const auto catalog = consumer.readFontCatalog();
    REQUIRE(!catalog.entries.empty());
    const auto font = foundation::ObjectId::fromUint64(0xFC0202);
    REQUIRE(catalog == composition.controlFacade().readFontCatalog());
    const auto resource = consumer.readFontResourceStatus(font);
    REQUIRE(resource == composition.controlFacade().readFontResourceStatus(font));
    REQUIRE(resource && resource->byteLength > 1000000);
    REQUIRE(resource->contentHash == "sha256:" + std::string(minimal_runtime_host::kHostTextFontSha256));
    const auto fontEntry = std::find_if(catalog.entries.begin(), catalog.entries.end(), [&](const auto& entry) { return entry.id == font; });
    REQUIRE(fontEntry != catalog.entries.end() && !fontEntry->family.empty());
    REQUIRE(!consumer.readFontResourceStatus(foundation::ObjectId::fromUint64(999)).has_value());

    std::uint64_t request = 0;
    const auto intent = [&](TextIntentKind kind, TextFormatPatch patch = {}, double width = 320) {
        TextIntent value;
        value.target = consumer.mountedCanvasTargets().front();
        value.clientId = 782;
        value.requestId = ++request;
        value.kind = kind;
        value.token = consumer.readTextEditor().token;
        value.viewX = 100;
        value.viewY = 100;
        value.format = patch;
        value.width = width;
        REQUIRE(consumer.submitTextIntent(value).status == TextStatus::kQueued);
        REQUIRE(composition.ownerPump());
        const auto receipt = consumer.textIntentReceipt(782, request);
        REQUIRE(receipt);
        return receipt->status;
    };
    REQUIRE(intent(TextIntentKind::kCreate) == TextStatus::kApplied);
    const auto initial = consumer.readTextEditor();
    TextInputEvent replace;
    replace.token = initial.token;
    replace.sequence = initial.nextInputSequence;
    replace.kind = TextInputKind::kReplace;
    replace.text = u"ABcd";
    REQUIRE(consumer.submitTextInput(replace).status == TextStatus::kQueued);
    REQUIRE(composition.ownerPump());
    REQUIRE(consumer.readTextEditor().projection == u"ABcd");
    REQUIRE(intent(TextIntentKind::kSelectAll) == TextStatus::kObserved);
    TextFormatPatch patch;
    patch.font = font;
    patch.color = std::array<float, 4>{0.125F, 0.25F, 0.75F, 1};
    patch.fontSize = 24;
    patch.bold = patch.italic = patch.underline = patch.strikethrough = patch.bulleted = true;
    patch.alignment = 1;
    REQUIRE(intent(TextIntentKind::kFormat, patch) == TextStatus::kApplied);
    const auto formatted = consumer.readTextEditor();
    REQUIRE(formatted.format == patch);
    REQUIRE(formatted.format == composition.controlFacade().readTextEditor().format);
    REQUIRE(consumer.readTextGeometry().available);
    const auto beforeRejected = composition.readback().semanticGeneration;
    REQUIRE(intent(TextIntentKind::kSetWidth, {}, 420) == TextStatus::kApplied);
    const auto validWidth = composition.readback().semanticGeneration;
    REQUIRE(validWidth > beforeRejected);
    REQUIRE(intent(TextIntentKind::kSetWidth, {}, 0) == TextStatus::kRejected);
    REQUIRE(consumer.readTextEditor().width == 420);
    REQUIRE(composition.readback().semanticGeneration == validWidth);

    TextIntent directRejectedWidth;
    directRejectedWidth.target = composition.controlFacade().mountedCanvasTargets().front();
    directRejectedWidth.clientId = 783;
    directRejectedWidth.requestId = 1;
    directRejectedWidth.kind = TextIntentKind::kSetWidth;
    directRejectedWidth.token = composition.controlFacade().readTextEditor().token;
    directRejectedWidth.width = 0;
    REQUIRE(composition.controlFacade().submitTextIntent(directRejectedWidth).status == TextStatus::kQueued);
    REQUIRE(composition.ownerPump());
    const auto directReceipt = composition.controlFacade().textIntentReceipt(783, 1);
    REQUIRE(directReceipt && directReceipt->status == TextStatus::kRejected);
    REQUIRE(consumer.readTextEditor().width == composition.controlFacade().readTextEditor().width);
    REQUIRE(composition.readback().semanticGeneration == validWidth);

    auto editor = consumer.readTextEditor();
    TextInputEvent begin;
    begin.token = editor.token;
    begin.sequence = editor.nextInputSequence;
    begin.composition = 91;
    begin.kind = TextInputKind::kBegin;
    begin.range = editor.replacement;
    REQUIRE(consumer.submitTextInput(begin).status == TextStatus::kQueued);
    REQUIRE(composition.ownerPump());

    const auto updatePreedit = [&](TextRange caret) {
        const auto state = consumer.readTextEditor();
        TextInputEvent update;
        update.token = state.token;
        update.sequence = state.nextInputSequence;
        update.composition = 91;
        update.kind = TextInputKind::kUpdate;
        update.range = state.replacement;
        update.text = u"abcd";
        update.preeditSelection = caret;
        REQUIRE(consumer.submitTextInput(update).status == TextStatus::kQueued);
        REQUIRE(composition.ownerPump());
        return consumer.readTextGeometry();
    };
    const auto preeditOne = updatePreedit({1, 0});
    const auto preeditThree = updatePreedit({3, 0});
    REQUIRE(consumer.readTextEditor().preeditSelection == (TextRange{3, 0}));
    REQUIRE(preeditOne.available && preeditThree.available);
    REQUIRE(preeditOne.caret[0] < preeditThree.caret[0]);
    REQUIRE(preeditThree.caret == composition.controlFacade().readTextGeometry().caret);
    REQUIRE(composition.readback().semanticGeneration == validWidth);

    std::puts("FC02 ordinary Minimal RuntimeFacade consumer: PASS");
}
