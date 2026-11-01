#include "ink_playground_host.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>

int main() {
    canvas::ink_playground::InkPlaygroundHost host;
    assert(host.bindSurface(512,512));
    std::ifstream latin(G47_LATIN_FONT,std::ios::binary), cjk(G47_CJK_FONT,std::ios::binary);
    const std::vector<std::uint8_t> latinBytes{std::istreambuf_iterator<char>(latin),{}};
    const std::vector<std::uint8_t> cjkBytes{std::istreambuf_iterator<char>(cjk),{}};
    if(!host.configureTextResources(latinBytes,cjkBytes,{128,1})) {
        std::cerr << "G47-QUAL-007 RED: real playground has no production text binding\n"; return 1;
    }
    assert(host.seedTextScenario("text-many-small",16));
    assert(host.qualificationObjectCount()==16);
    const auto layouts=host.textLayoutMetrics().objectLayouts;
    assert(layouts==16);
    assert(host.presentCanonicalFrame(host.canonicalFrameCount()+1,0,false));
    assert(host.textLayoutMetrics().objectLayouts==layouts);
    std::vector<std::uint8_t> pixels(512*512*4);
    assert(host.activeSurfaceProvider()->readbackRgba(pixels).code==canvas::render::BackendSubmissionCode::kAccepted);
    std::size_t ink=0; for(std::size_t i=0;i<pixels.size();i+=4) if(pixels[i]<250) ++ink;
    assert(ink>100);
    const auto gen=host.semanticGeneration();
    assert(host.setTextFontsAvailable(false));
    assert(host.setTextFontsAvailable(true));
    assert(host.semanticGeneration()==gen);
    assert(host.presentCanonicalFrame(host.canonicalFrameCount()+1,0,false));
    assert(host.editTextScenario(0));
    assert(host.textLayoutMetrics().objectLayouts==layouts+33); // 16 missing + 16 ready + 1 edited
    if(!host.seedTextScenario("text-style-mixed",8)) {
        std::cerr<<"G47 playground selector RED: switching scenario rejected existing fixture IDs\n"; return 1;
    }
    assert(host.qualificationObjectCount()==8);
    if (host.textQualificationJson().find("\"diagnostics\":0") == std::string::npos) {
        std::cerr << "G47 mixed fixture RED: bundled corpus contains unavailable glyphs\n";
        return 1;
    }
    assert(host.seedTextScenario("structured-grid-proxy",8));
    assert(host.qualificationObjectCount()==16);
    for(const auto* action:{"insert","delete","split","merge","inline-style","paragraph-style"})
        assert(host.applyTextScenarioEdit(0,action));
    const auto transformLayouts=host.textLayoutMetrics().objectLayouts;
    assert(host.transformTextScenario(0));
    assert(host.textLayoutMetrics().objectLayouts==transformLayouts);
    assert(host.configureBundledTextResources());
    assert(host.textQualificationJson().find("structured-grid-proxy")!=std::string::npos);
    std::cout << "G47 playground host text/resources/edit/render PASS\n";
}
