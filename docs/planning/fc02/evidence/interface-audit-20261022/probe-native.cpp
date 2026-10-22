#include "minimal_runtime_facade.hpp"
#include "host_font_resources.hpp"
#include <cstdio>

int main(int argc, char** argv) {
    using namespace canvas::runtime;
    if (argc != 2) return 2;
    canvas::minimal_runtime_host::MinimalRuntimeHost host;
    auto& c = host.composition();
    if (!c.bindSurface(640,480) ||
        !canvas::minimal_runtime_host::bindHostTextFont(c,argv[1])) return 3;
    canvas::minimal_runtime_host::MinimalRuntimeFacade wrapped(host);
    auto& direct = c.controlFacade();
    const auto catalog = direct.readFontCatalog();
    const auto wrappedCatalog = wrapped.readFontCatalog();
    const auto id = catalog.entries.front().id;
    TextIntent create;
    create.target = direct.mountedCanvasTargets().front();
    create.clientId = 991;
    create.requestId = 1;
    create.viewX = 100;
    create.viewY = 100;
    const auto queued = direct.submitTextIntent(create);
    if (queued.status != TextStatus::kQueued || !c.ownerPump()) return 4;
    const auto event = [&](TextInputKind kind, const std::u16string& text,
                           std::optional<TextRange> caret) {
        const auto s = direct.readTextEditor();
        TextInputEvent e;
        e.token = s.token;
        e.sequence = s.nextInputSequence;
        e.kind = kind;
        e.composition = 1;
        e.range = s.replacement;
        e.text = text;
        e.preeditSelection = caret;
        if (direct.submitTextInput(e).status != TextStatus::kQueued || !c.ownerPump()) return false;
        return direct.textInputReceipt(e.token,e.sequence)->status == TextStatus::kObserved;
    };
    if (!event(TextInputKind::kBegin,u"",std::nullopt) ||
        !event(TextInputKind::kUpdate,u"abcd",TextRange{1,0})) return 5;
    const auto g1 = direct.readTextGeometry();
    if (!event(TextInputKind::kUpdate,u"abcd",TextRange{3,0})) return 6;
    const auto g3 = direct.readTextGeometry();
    TextIntent width = create;
    width.token = direct.readTextEditor().token;
    width.kind = TextIntentKind::kEscape;
    width.requestId = 2;
    (void)direct.submitTextIntent(width); (void)c.ownerPump();
    width.kind = TextIntentKind::kSetWidth;
    width.token = direct.readTextEditor().token;
    width.requestId = 3;
    width.width = 0;
    (void)direct.submitTextIntent(width); (void)c.ownerPump();
    const auto receipt = direct.textIntentReceipt(991,3);
    TextIntent format = width;
    format.kind = TextIntentKind::kFormat;
    format.requestId = 4;
    format.token = direct.readTextEditor().token;
    format.format.bold = true;
    (void)direct.submitTextIntent(format); (void)c.ownerPump();
    const auto formatted = direct.textIntentReceipt(991,4);
    const auto bold = direct.readTextEditor().format.bold;
    std::printf("{\"direct_catalog\":%zu,\"wrapped_catalog\":%zu,\"editor_font_options\":%zu,"
                "\"direct_resource_status\":%s,\"wrapped_resource_status\":%s,"
                "\"preedit_caret_one\":%.6f,\"preedit_caret_three\":%.6f,"
                "\"preedit_geometry_available\":%s,\"zero_width_status\":%d,"
                "\"format_status\":%d,\"format_bold\":%s}\n",
                catalog.entries.size(),wrappedCatalog.entries.size(),direct.readTextEditor().fonts.size(),
                direct.readFontResourceStatus(id)?"true":"false",
                wrapped.readFontResourceStatus(id)?"true":"false",g1.caret[0],g3.caret[0],
                g1.available&&g3.available?"true":"false",receipt?static_cast<int>(receipt->status):-1,
                formatted?static_cast<int>(formatted->status):-1,bold&&*bold?"true":"false");
    return 0;
}
