#include "canvas/render/render_group_manager.hpp"
#include "canvas/text/text_layout.hpp"

#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace canvas;
using namespace canvas::render;

namespace {
using Layout = std::shared_ptr<const text::TextLayoutSnapshot>;
constexpr foundation::WorldRect kClip{0,0,900,700};

void commandLayout(ReferenceTraversalEntry& entry,const Layout& layout) {
    std::visit([&](auto& command) {
        if constexpr(requires { command.labelLayout; })command.labelLayout=layout;
        else if constexpr(requires { command.titleLayout; })command.titleLayout=layout;
        else if constexpr(requires { command.layout; })command.layout=layout;
    },entry.command);
}

ReferenceTraversalEntry source(unsigned kind,const Layout& layout) {
    ReferenceTraversalEntry entry;
    entry.record.objectId=foundation::ObjectId::fromUint64(77+kind);
    entry.record.kindVersion=1;
    entry.record.worldBounds={0,0,120,80};
    entry.record.textLayout=layout;
    switch(kind) {
    case 0:
        entry.record.kind=semantic::ObjectKind::kShape;
        entry.record.content=semantic::ShapeContent{1,120,80};
        entry.command=ShapeReferenceCommand{semantic::ShapeContent{1,120,80},layout,true};
        break;
    case 1:
        entry.record.kind=semantic::ObjectKind::kSticky;
        entry.record.content=semantic::StickyContent{200,200};
        entry.command=StickyReferenceCommand{semantic::StickyContent{200,200},layout,true};
        break;
    case 2:
        entry.record.kind=semantic::ObjectKind::kTable;
        entry.record.content=semantic::TableContent{};
        entry.command=TableReferenceCommand{semantic::TableContent{},layout,0};
        break;
    case 3:
        entry.record.kind=semantic::ObjectKind::kRichText;
        entry.record.content=semantic::RichTextContent{};
        entry.command=RichTextReferenceCommand{semantic::RichTextContent{},layout,true,false};
        break;
    case 4:
        entry.record.kind=semantic::ObjectKind::kSection;
        entry.record.content=semantic::SectionContent{};
        entry.command=SectionReferenceCommand{semantic::SectionContent{},layout};
        break;
    default:throw std::runtime_error("unknown fixture kind");
    }
    return entry;
}

void check(const ReferenceTraversalEntry& before,const ReferenceTraversalEntry& after,
           bool changed,unsigned kind,const std::string& name) {
    RenderGroupManager manager;
    (void)manager.update({&before,1},kClip,1);
    const auto initial=manager.find(before.record.objectId)->revisions;
    const auto update=manager.update({&after,1},kClip,1);
    const auto final=manager.find(before.record.objectId)->revisions;
    const bool damaged=!update.contributions.empty();
    std::cout<<"{\"kind\":"<<kind<<",\"case\":\""<<name
        <<"\",\"expected_change\":"<<changed<<",\"damage\":"<<damaged
        <<",\"revision_before\":"<<initial.content<<",\"revision_after\":"<<final.content<<"}\n";
    if(damaged!=changed||final.content!=initial.content+(changed?1:0))
        throw std::runtime_error("layout identity: "+name+" kind "+std::to_string(kind));
    if(!manager.update({&after,1},kClip,1).contributions.empty())
        throw std::runtime_error("same pointer repeatedly damaged: "+name);
}

void identities() {
    const std::vector<std::pair<std::string,std::function<void(text::TextLayoutSnapshot&)>>> changes{
        {"digest",[](auto& layout){layout.digest="changed";}},
        {"content",[](auto& layout){layout.contentDigest="changed";}},
        {"profile",[](auto& layout){layout.profile="other-profile";}},
        {"context-width",[](auto& layout){++layout.context.width;}},
        {"context-revision",[](auto& layout){++layout.context.revision;}},
        {"context-height",[](auto& layout){++layout.context.height;}},
        {"context-lines",[](auto& layout){++layout.context.maxLines;}},
        {"bounds",[](auto& layout){++layout.localBounds.right;}},
        {"shape-label",[](auto& layout){layout.shapeLabel=!layout.shapeLabel;}},
        {"placeholder",[](auto& layout){layout.placeholder=!layout.placeholder;}},
        {"overflow",[](auto& layout){layout.overflow=!layout.overflow;}},
        {"scale-x",[](auto& layout){++layout.labelScaleX;}},
        {"scale-y",[](auto& layout){++layout.labelScaleY;}},
        {"viewport",[](auto& layout){++layout.labelViewport.right;}},
        {"expand",[](auto& layout){++layout.expandButton.right;}},
        {"offset-x",[](auto& layout){++layout.labelOffsetX;}},
        {"offset-y",[](auto& layout){++layout.labelOffsetY;}},
        {"font-id",[](auto& layout){layout.fonts[0].resourceId.value=foundation::ObjectId::fromUint64(9);}},
        {"font-generation",[](auto& layout){++layout.fonts[0].generation;}},
        {"font-hash",[](auto& layout){layout.fonts[0].sha256="different-font";}},
        {"font-index",[](auto& layout){++layout.fonts[0].collectionIndex;}},
        {"font-weight",[](auto& layout){++layout.fonts[0].weight;}},
        {"font-italic",[](auto& layout){layout.fonts[0].italic=true;}},
        {"font-synthetic",[](auto& layout){layout.fonts[0].syntheticStyle=true;}},
        {"diagnostics",[](auto& layout){layout.diagnostics.push_back("unavailable");}},
        {"missing-digest",[](auto& layout){layout.digest.clear();}}
    };
    for(unsigned kind=0;kind<5;++kind) {
        auto layout=std::make_shared<text::TextLayoutSnapshot>();
        layout->digest="same-layout";layout->contentDigest="same-content";
        layout->localBounds={0,0,120,80};
        layout->fonts.push_back({});layout->fonts[0].sha256="font";
        const auto first=source(kind,layout);
        const auto clone=std::make_shared<text::TextLayoutSnapshot>(*layout);
        for(unsigned mode=0;mode<3;++mode) {
            auto equal=first;
            if(mode!=1)equal.record.textLayout=clone;
            if(mode!=0)commandLayout(equal,clone);
            check(first,equal,false,kind,"equal-"+std::to_string(mode));
            for(const auto& [name,mutate]:changes) {
                auto changedLayout=std::make_shared<text::TextLayoutSnapshot>(*layout);mutate(*changedLayout);
                auto changed=first;
                if(mode!=1)changed.record.textLayout=changedLayout;
                if(mode!=0)commandLayout(changed,changedLayout);
                check(first,changed,true,kind,name+"-"+std::to_string(mode));
            }
        }
        auto missing=std::make_shared<text::TextLayoutSnapshot>(*layout);missing->digest.clear();
        const auto missingSource=source(kind,missing);
        check(missingSource,source(kind,std::make_shared<text::TextLayoutSnapshot>(*missing)),true,kind,"both-digests-missing");
        auto nullRecord=first;nullRecord.record.textLayout.reset();
        check(first,nullRecord,true,kind,"record-null");
        auto nullCommand=first;commandLayout(nullCommand,nullptr);
        check(first,nullCommand,true,kind,"command-null");
    }
    // A table's legacy aggregate digest does not carry all of its children's
    // external basis. Equal nested wrappers reuse; changed child basis cannot.
    auto table=std::make_shared<text::TextLayoutSnapshot>();table->digest="table-v1:cell:";
    auto cell=std::make_shared<text::TextLayoutSnapshot>();cell->digest="cell";cell->fonts.push_back({});
    table->tableCells.push_back(cell);
    auto equal=std::make_shared<text::TextLayoutSnapshot>(*table);
    equal->tableCells[0]=std::make_shared<text::TextLayoutSnapshot>(*cell);
    check(source(2,table),source(2,equal),false,2,"nested-equal");
    auto child=std::make_shared<text::TextLayoutSnapshot>(*cell);++child->fonts[0].generation;
    equal->tableCells[0]=child;
    check(source(2,table),source(2,equal),true,2,"nested-font-generation");
    for(unsigned kind:{0U,1U,2U,3U}) {
        auto before=source(kind,cell),after=before;
        std::visit([](auto& command){
            if constexpr(requires { command.showExpandButton; })command.showExpandButton=!command.showExpandButton;
            if constexpr(requires { command.activeCell; })command.activeCell=1;
        },after.command);
        check(before,after,true,kind,"command-affordance");
    }
    auto before=source(3,cell),after=before;
    std::get<RichTextReferenceCommand>(after.command).suppressBorder=true;
    check(before,after,true,3,"command-border");
    auto semanticChanged=source(0,cell);
    std::get<ShapeReferenceCommand>(semanticChanged.command).content.update([](auto& content){++content.width;});
    check(source(0,cell),semanticChanged,true,0,"command-content");
}
}

int main() {
    try {identities();return 0;}
    catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
