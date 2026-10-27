// Typed transport only. All planning, transforms, guards and History remain
// behind the same public facade used by the native host and normal consumers.
namespace {
bool sectionIdValid(const TextJson& value) {
    if(!value.is_string())return false;
    const auto& s=value.get_ref<const std::string&>();
    return s.size()==32&&s.find_first_not_of("0123456789abcdef")==std::string::npos;
}
bool sectionUint64Valid(const TextJson& value) {
    if(!value.is_string())return false;
    const auto& s=value.get_ref<const std::string&>();
    if(s.empty()||s.size()>20||s.find_first_not_of("0123456789")!=std::string::npos)return false;
    std::uint64_t n=0;
    for(char c:s) {const auto digit=static_cast<unsigned>(c-'0');if(n>(UINT64_MAX-digit)/10)return false;n=n*10+digit;}
    return true;
}
bool sectionNumbersValid(const TextJson& value,std::size_t count) {
    return value.is_array()&&value.size()==count&&std::all_of(value.begin(),value.end(),[](const auto& v){return v.is_number()&&std::isfinite(v.template get<double>());});
}
bool sectionEnvelopeValid(const TextJson& j) {
    if(!j.is_object()||!j.contains("target")||!j["target"].is_object())return false;
    for(const auto* field:{"runtimeEpoch","viewId","viewEpoch","attachmentEpoch"})
        if(!j["target"].contains(field)||!sectionUint64Valid(j["target"][field]))return false;
    for(const auto* field:{"clientId","requestId"})if(!j.contains(field)||!sectionUint64Valid(j[field]))return false;
    if(!j.contains("action")||!j["action"].is_number_integer()||j["action"].get<double>()<0||
       j["action"].get<double>()>static_cast<unsigned>(canvas::runtime::SectionAction::kLeaveGroupScope))return false;
    if(j.contains("objectId")&&!sectionIdValid(j["objectId"]))return false;
    if(j.contains("parentId")&&!j["parentId"].is_null()&&!sectionIdValid(j["parentId"]))return false;
    if(j.contains("objects")&&(!j["objects"].is_array()||j["objects"].size()>65535||
       !std::all_of(j["objects"].begin(),j["objects"].end(),sectionIdValid)))return false;
    if(j.contains("title")&&(!j["title"].is_string()||j["title"].get_ref<const std::string&>().size()>1048576))return false;
    if(j.contains("boundary")&&!sectionNumbersValid(j["boundary"],4))return false;
    if(j.contains("transform")&&!sectionNumbersValid(j["transform"],6))return false;
    for(const auto* field:{"generation","selectionRevision"})if(j.contains(field)&&!sectionUint64Valid(j[field]))return false;
    return true;
}
TextJson sectionOutcomeJson(const canvas::runtime::CanvasControlReceipt& r) {
    TextJson j{{"state",static_cast<unsigned>(r.state)},{"error",static_cast<unsigned>(r.error)},
        {"clientId",std::to_string(r.key.clientId)},{"requestId",std::to_string(r.key.requestId)}};
    if(r.section) {
        const auto& s=*r.section;
        j["section"]={{"failure",static_cast<unsigned>(s.failure)},{"canonical",static_cast<unsigned>(s.canonical)},
            {"projection",static_cast<unsigned>(s.projection)},{"objectId",hexId(s.objectId)},
            {"operationId",hexId(s.operationId)},{"generation",std::to_string(s.semanticGeneration)},
            {"sceneRevision",std::to_string(s.sceneRevision)},{"commitEpoch",std::to_string(s.commitEpoch)},
            {"commitOrdinal",std::to_string(s.commitOrdinal)}};
    }
    return j;
}
TextJson sectionTransformJson(const canvas::semantic::Transform2D& t) {
    return TextJson::array({t.a,t.b,t.c,t.d,t.tx,t.ty});
}
}
extern "C" {
EMSCRIPTEN_KEEPALIVE const char* axiom_web_section_intent(std::uintptr_t h,const char* bytes) {
    static std::string result;
    const auto invalid=[&](){result="{\"transportError\":\"invalid Section envelope\"}";return result.c_str();};
    try {
        auto* v=instance(h);if(!v||!bytes||std::strlen(bytes)>2*1024*1024)return invalid();
        const auto j=TextJson::parse(bytes,nullptr,false);
        if(!sectionEnvelopeValid(j))return invalid();
        canvas::runtime::CanvasControlRequest request;
        request.target=parseTarget(j.at("target"));request.clientId=decimal(j.at("clientId").get<std::string>());
        request.requestId=decimal(j.at("requestId").get<std::string>());
        request.payload.kind=canvas::runtime::CanvasControlPayloadKind::kSection;
        auto& intent=request.payload.section;const auto action=j.at("action").get<unsigned>();
        if(action>static_cast<unsigned>(canvas::runtime::SectionAction::kLeaveGroupScope))throw std::invalid_argument("Section action");
        intent.action=static_cast<canvas::runtime::SectionAction>(action);
        if(j.contains("objectId"))intent.objectId=rawId(j.at("objectId").get<std::string>());
        if(j.contains("parentId")&&!j.at("parentId").is_null())intent.parentId=rawId(j.at("parentId").get<std::string>());
        if(j.contains("objects")) {
            const auto& ids=j.at("objects");if(!ids.is_array()||ids.size()>65535)throw std::invalid_argument("Section targets");
            for(const auto& id:ids)intent.objects.push_back(rawId(id.get<std::string>()));
        }
        if(j.contains("title"))intent.content.title=j.at("title").get<std::string>();
        if(j.contains("boundary")) {
            const auto b=j.at("boundary").get<std::array<double,4>>();
            intent.content.boundary={b[0],b[1],b[2],b[3]};
        }
        if(j.contains("transform")) {
            const auto t=j.at("transform").get<std::array<double,6>>();
            intent.transform={t[0],t[1],t[2],t[3],t[4],t[5]};
        }
        if(j.contains("generation"))intent.expectedSemanticGeneration=decimal(j.at("generation").get<std::string>());
        if(j.contains("selectionRevision"))intent.selectionRevision=decimal(j.at("selectionRevision").get<std::string>());
        const auto submitted=v->lifecycle.facade().submitCanvasControl(request);
        result=submitted.control?sectionOutcomeJson(*submitted.control).dump():TextJson{{"state",static_cast<unsigned>(submitted.state)}}.dump();
        v->forceCanonical=true;
    } catch(const std::exception& e) {result=TextJson{{"transportError",e.what()}}.dump();}
    return result.c_str();
}
EMSCRIPTEN_KEEPALIVE const char* axiom_web_section_receipt(std::uintptr_t h,const char* client,const char* request) {
    static std::string result;
    try {
        auto* v=instance(h);if(!v||!client||!request||!sectionUint64Valid(TextJson(client))||!sectionUint64Valid(TextJson(request))) {
            result="{\"transportError\":\"invalid Section receipt key\"}";return result.c_str();
        }
        const auto receipt=v->lifecycle.facade().canvasControlReceipt({decimal(client),decimal(request)});
        result=receipt?sectionOutcomeJson(*receipt).dump():"{}";
    } catch(const std::exception& e){result=TextJson{{"transportError",e.what()}}.dump();}
    return result.c_str();
}
EMSCRIPTEN_KEEPALIVE const char* axiom_web_section_snapshot(std::uintptr_t h,const char* object) {
    static std::string result;
    try {
        auto* v=instance(h);if(!v||(object&&*object&&!sectionIdValid(TextJson(object)))) {
            result="{\"transportError\":\"invalid Section view or object\"}";return result.c_str();
        }
        const auto s=v->lifecycle.facade().readSection(object&&*object?rawId(object):canvas::foundation::ObjectId{});
        TextJson j{{"target",jsonTarget(s.target)},{"available",s.available},{"projectionReady",s.projectionReady},
            {"titleLayoutReady",s.titleLayoutReady},{"titleGlyphCount",s.titleGlyphCount},{"titleDiagnostics",s.titleDiagnostics},
            {"busy",s.busy},{"objectId",hexId(s.objectId)},{"generation",std::to_string(s.semanticGeneration)},
            {"sceneRevision",std::to_string(s.sceneRevision)},{"selectionRevision",std::to_string(s.selectionRevision)},{"title",s.content.title},
            {"boundary",TextJson::array({s.content.boundary.x,s.content.boundary.y,s.content.boundary.width,s.content.boundary.height})},
            {"localTransform",sectionTransformJson(s.localTransform)},{"worldTransform",sectionTransformJson(s.worldTransform)},
            {"children",TextJson::array()},{"selectedRoots",TextJson::array()},{"members",TextJson::array()}};
        for(auto id:s.directChildren)j["children"].push_back(hexId(id));
        for(auto id:s.selectedRoots)j["selectedRoots"].push_back(hexId(id));
        for(const auto& member:s.members)j["members"].push_back({{"objectId",hexId(member.objectId)},
            {"kind",static_cast<unsigned>(member.kind)},{"localTransform",sectionTransformJson(member.localTransform)},
            {"worldTransform",sectionTransformJson(member.worldTransform)}});
        result=j.dump();
    }catch(const std::exception& e){result=TextJson{{"transportError",e.what()}}.dump();}
    return result.c_str();
}
}
