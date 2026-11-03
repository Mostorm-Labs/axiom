
namespace {
using TextJson=nlohmann::json;
std::string hexId(foundation::ObjectId id){std::string text;constexpr char hex[]="0123456789abcdef";for(auto b:id.bytes){text+=hex[b>>4];text+=hex[b&15];}return text;}
foundation::ObjectId rawId(const std::string& text){if(text.size()!=32)throw std::invalid_argument("Id128");foundation::ObjectId id;const auto digit=[](char c)->int{if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;throw std::invalid_argument("hex");};for(std::size_t i=0;i<16;++i)id.bytes[i]=static_cast<std::uint8_t>(digit(text[i*2])*16+digit(text[i*2+1]));return id;}
std::uint64_t decimal(const std::string& value){if(value.empty()||value.size()>20||value.find_first_not_of("0123456789")!=std::string::npos)throw std::invalid_argument("uint64");std::size_t count=0;auto number=std::stoull(value,&count);if(count!=value.size())throw std::invalid_argument("uint64");return number;}
TextJson jsonToken(const runtime::TextSessionToken& t){return {{"runtime",std::to_string(t.runtimeInstance)},{"attachment",std::to_string(t.attachment)},{"view",std::to_string(t.view)},{"adapter",std::to_string(t.adapter)},{"session",std::to_string(t.session)},{"projection",std::to_string(t.projection)},{"document",hexId(t.document)},{"object",hexId(t.object)},{"row",hexId(t.row)},{"column",hexId(t.column)}};}
runtime::TextSessionToken parseToken(const char* value){if(!value || std::strlen(value)>1024)throw std::invalid_argument("token");const auto j=TextJson::parse(value);return {decimal(j.at("runtime").get<std::string>()),decimal(j.at("attachment").get<std::string>()),decimal(j.at("view").get<std::string>()),decimal(j.at("adapter").get<std::string>()),decimal(j.at("session").get<std::string>()),decimal(j.at("projection").get<std::string>()),rawId(j.at("document").get<std::string>()),rawId(j.at("object").get<std::string>()),j.contains("row")?rawId(j.at("row").get<std::string>()):foundation::ObjectId{},j.contains("column")?rawId(j.at("column").get<std::string>()):foundation::ObjectId{}};}
TextJson jsonTarget(const runtime::CanvasTargetKey& t) {
    return {{"runtimeEpoch", std::to_string(t.runtimeEpoch)}, {"viewId", std::to_string(t.viewId)},
            {"viewEpoch", std::to_string(t.viewEpoch)}, {"attachmentEpoch", std::to_string(t.attachmentEpoch)}};
}
runtime::CanvasTargetKey parseTarget(const TextJson& j) {
    return {decimal(j.at("runtimeEpoch").get<std::string>()), decimal(j.at("viewId").get<std::string>()),
            decimal(j.at("viewEpoch").get<std::string>()), decimal(j.at("attachmentEpoch").get<std::string>())};
}
TextJson jsonFormat(const runtime::TextFormatPatch& p) {
    TextJson j = TextJson::object();
    if (p.color) j["color"] = *p.color;
    if (p.fontSize) j["fontSize"] = *p.fontSize;
    if (p.font) j["font"] = hexId(*p.font);
    if (p.bold) j["bold"] = *p.bold;
    if (p.italic) j["italic"] = *p.italic;
    if (p.underline) j["underline"] = *p.underline;
    if (p.strikethrough) j["strikethrough"] = *p.strikethrough;
    if (p.bulleted) j["bulleted"] = *p.bulleted;
    if (p.alignment) j["alignment"] = *p.alignment;
    return j;
}
runtime::TextFormatPatch parseFormat(const TextJson& j) {
    if (!j.is_object()) throw std::invalid_argument("format");
    runtime::TextFormatPatch p;
    for (const auto& [key, value] : j.items()) {
        if (key == "color") p.color = value.get<std::array<float, 4>>();
        else if (key == "fontSize") p.fontSize = value.get<double>();
        else if (key == "font") p.font = rawId(value.get<std::string>());
        else if (key == "alignment") {
            if (value == 1) p.alignment = 1;
            else if (value == 2) p.alignment = 2;
            else if (value == 3) p.alignment = 3;
            else throw std::invalid_argument("alignment");
        } else {
            if (!value.is_boolean()) throw std::invalid_argument("format flag");
            if (key == "bold") p.bold = value.get<bool>();
            else if (key == "italic") p.italic = value.get<bool>();
            else if (key == "underline") p.underline = value.get<bool>();
            else if (key == "strikethrough") p.strikethrough = value.get<bool>();
            else if (key == "bulleted") p.bulleted = value.get<bool>();
            else throw std::invalid_argument("format leaf");
        }
    }
    return p;
}
TextJson jsonReceipt(const runtime::TextReceipt& r) {
    return {{"status", r.status}, {"token", jsonToken(r.token)}, {"request", std::to_string(r.request)},
            {"semanticGeneration", std::to_string(r.semanticGeneration)}, {"operation", hexId(r.operation)}, {"replay", r.replay}};
}
TextJson jsonFont(const runtime::FontCatalogEntry& f) {
    return {{"id", hexId(f.id)}, {"family", f.family}, {"revision", std::to_string(f.revision)},
            {"contentHash", f.contentHash}, {"byteLength", f.byteLength}, {"status", f.status}, {"diagnostic", f.diagnostic}};
}
}
extern "C" {
EMSCRIPTEN_KEEPALIVE const char* axiom_web_text_snapshot(std::uintptr_t h){static thread_local std::string result;auto* v=instance(h);if(!v)return "{}";auto& f=v->lifecycle.composition().controlFacade();const auto s=f.readTextEditor();const auto g=f.readTextGeometry();
 TextJson j={{"phase",s.phase},{"token",jsonToken(s.token)},{"sequence",std::to_string(s.nextInputSequence)},{"composition",std::to_string(s.composition)},{"effect",std::to_string(s.finishEffect)},{"exit",s.exitStatus},{"selection",{s.selection.start,s.selection.length}},{"replacement",{s.replacement.start,s.replacement.length}},{"width",s.width},{"projection",std::vector<std::uint16_t>(s.projection.begin(),s.projection.end())},{"preedit",std::vector<std::uint16_t>(s.preedit.begin(),s.preedit.end())},{"geometry",{{"diagnostics",g.diagnostics},{"available",g.available},{"caret",g.caret},{"layout",g.token.layout},{"camera",std::to_string(g.token.camera)}}}};
 j["format"] = jsonFormat(s.format); j["formatObject"] = hexId(s.formatObject); j["mixedFormat"] = s.mixedFormat;
 j["projectionLimit"] = s.projectionLimit; j["fonts"] = TextJson::array();
 for (const auto& font : s.fonts) j["fonts"].push_back({{"id",hexId(font.id)},{"name",font.name}});
 j["preeditSelection"] = s.preeditSelection ? TextJson{ s.preeditSelection->start, s.preeditSelection->length } : TextJson(nullptr);
 j["geometry"]["selection"] = g.selection; j["geometry"]["token"] = {{"session",jsonToken(g.token.session)},{"target",jsonTarget(g.token.target)},{"camera",std::to_string(g.token.camera)},{"layout",g.token.layout}};
 const auto targets = f.mountedCanvasTargets(); if (!targets.empty()) j["target"] = jsonTarget(targets.front());
 result=j.dump();return result.c_str();}
EMSCRIPTEN_KEEPALIVE int axiom_web_text_intent_json(std::uintptr_t h, const char* data) {
    auto* v = instance(h);
    if (!v || !data || std::strlen(data) > 8192) return 4;
    try {
        const auto j = TextJson::parse(data);
        if(!j.at("kind").is_number_integer()||j.at("kind").get<std::uint64_t>()>static_cast<std::uint64_t>(runtime::TextIntentKind::kFormat))return 4;
        const int kind = j.at("kind").get<int>();
        runtime::TextIntent request;
        request.kind = static_cast<runtime::TextIntentKind>(kind);
        request.clientId = decimal(j.at("clientId").get<std::string>());
        request.requestId = decimal(j.at("requestId").get<std::string>());
        request.target = parseTarget(j.at("target"));
        request.token = parseToken(j.at("token").dump().c_str());
        if(j.contains("row"))request.row=rawId(j.at("row").get<std::string>());
        if(j.contains("column"))request.column=rawId(j.at("column").get<std::string>());
        if (j.contains("object")) request.object = rawId(j.at("object").get<std::string>());
        request.viewX = j.value("x", 0.0F); request.viewY = j.value("y", 0.0F);
        request.width = j.value("width", 320.0);
        if (j.contains("format")) request.format = parseFormat(j.at("format"));
        return static_cast<int>(v->lifecycle.facade().submitTextIntent(request).status);
    } catch (...) { return 4; }
}
EMSCRIPTEN_KEEPALIVE const char* axiom_web_text_intent_receipt(std::uintptr_t h, const char* client, const char* request) {
    static thread_local std::string result;
    auto* v = instance(h);
    if (!v || !client || !request) return "null";
    try {
        const auto receipt = v->lifecycle.facade().textIntentReceipt(decimal(client), decimal(request));
        result = receipt ? jsonReceipt(*receipt).dump() : "null";
        return result.c_str();
    } catch (...) { return "null"; }
}
EMSCRIPTEN_KEEPALIVE const char* axiom_web_font_catalog(std::uintptr_t h) {
    static thread_local std::string result;
    auto* v = instance(h); if (!v) return "null";
    const auto catalog = v->lifecycle.facade().readFontCatalog();
    TextJson entries = TextJson::array(); for (const auto& f : catalog.entries) entries.push_back(jsonFont(f));
    result = TextJson{{"manifestRevision",std::to_string(catalog.manifestRevision)},{"bindingGeneration",std::to_string(catalog.bindingGeneration)},{"entries",std::move(entries)}}.dump();
    return result.c_str();
}
EMSCRIPTEN_KEEPALIVE const char* axiom_web_font_status(std::uintptr_t h, const char* id) {
    static thread_local std::string result;
    auto* v = instance(h); if (!v || !id) return "null";
    try { const auto font = v->lifecycle.facade().readFontResourceStatus(rawId(id)); result = font ? jsonFont(*font).dump() : "null"; return result.c_str(); }
    catch (...) { return "null"; }
}
EMSCRIPTEN_KEEPALIVE int axiom_web_text_intent(std::uintptr_t h,int kind,const char* token,float x,float y,double width){auto* v=instance(h);if(!v || kind<0 || kind>static_cast<int>(runtime::TextIntentKind::kCompleteComposition))return 4;
 try{auto& f=v->lifecycle.composition().controlFacade();runtime::TextIntent request;request.clientId=0xFC0205;request.requestId=++v->request;request.target=f.mountedCanvasTargets().front();request.kind=static_cast<runtime::TextIntentKind>(kind);if(token)request.token=parseToken(token);request.viewX=x;request.viewY=y;request.width=width;v->lastTextIntent=request.requestId;return static_cast<int>(f.submitTextIntent(request).status);}catch(...){return 4;}}
EMSCRIPTEN_KEEPALIVE int axiom_web_text_intent_status(std::uintptr_t h){auto* v=instance(h);if(!v)return 4;const auto receipt=v->lifecycle.composition().controlFacade().textIntentReceipt(0xFC0205,v->lastTextIntent);return receipt?static_cast<int>(receipt->status):4;}
EMSCRIPTEN_KEEPALIVE int axiom_web_text_pump(std::uintptr_t h){auto* v=instance(h);return v && v->lifecycle.composition().ownerPump();}
EMSCRIPTEN_KEEPALIVE int axiom_web_text_input(std::uintptr_t h,const char* token,const char* sequence,int kind,const char16_t* text,std::uint32_t size,std::uint32_t start,std::uint32_t length,const char* composition,const char* effect,const char* preeditSelection){auto* v=instance(h);if(!v || !sequence || !composition || !effect || size>65536 || kind<0 || kind>static_cast<int>(runtime::TextInputKind::kFinishFailed))return 4;
 try{runtime::TextInputEvent event;event.token=parseToken(token);event.sequence=decimal(sequence);event.kind=static_cast<runtime::TextInputKind>(kind);if(size){if(!text)return 4;event.text.assign(text,text+size);}event.range={start,length};event.composition=decimal(composition);event.finishEffect=decimal(effect);if(preeditSelection){const auto range=TextJson::parse(preeditSelection);if(!range.is_null())event.preeditSelection=runtime::TextRange{range.at(0).get<std::uint32_t>(),range.at(1).get<std::uint32_t>()};}return static_cast<int>(v->lifecycle.facade().submitTextInput(event).status);}catch(...){return 4;}}
EMSCRIPTEN_KEEPALIVE int axiom_web_text_status(std::uintptr_t h,const char* token,const char* sequence,int ack){auto* v=instance(h);if(!v)return 4;try{auto& f=v->lifecycle.composition().controlFacade();const auto t=parseToken(token);const auto seq=decimal(sequence);const auto receipt=f.textInputReceipt(t,seq);if(!receipt)return -1;if(ack)(void)f.acknowledgeTextInput(t,seq);return static_cast<int>(receipt->status);}catch(...){return 4;}}
EMSCRIPTEN_KEEPALIVE int axiom_web_text_tool(std::uintptr_t h){auto* v=instance(h);if(!v)return 0;auto& f=v->lifecycle.composition().controlFacade();runtime::CanvasControlRequest request;request.target=f.mountedCanvasTargets().front();request.clientId=0xFC0206;request.requestId=++v->request;request.payload.kind=runtime::CanvasControlPayloadKind::kSelectTool;request.payload.tool=runtime::CanvasToolKind::kText;return f.submitCanvasControl(request).state==runtime::ProductControlState::kQueued;}
}
