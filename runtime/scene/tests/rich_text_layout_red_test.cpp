#include "canvas/scene/bounds_system.hpp"

#include <iostream>

int main() {
    canvas::semantic::ObjectRecord record{};
    record.kind = canvas::semantic::ObjectKind::kRichText;
    canvas::semantic::TextStyle style{};
    style.font_size = 16;
    style.weight = 400;
    canvas::semantic::Paragraph p{};
    p.style = {canvas::semantic::ParagraphAlignment::kLeft, 1, 0, 0};
    p.runs = {{"iiii", style}};
    record.content = canvas::semantic::RichTextContent{{{p}}};
    const auto narrow = canvas::computeBounds(record);
    std::get<canvas::semantic::RichTextContent>(record.content).document.paragraphs[0].runs[0].text = "WWWW";
    const auto wide = canvas::computeBounds(record);
    if (narrow.geometry != canvas::foundation::WorldRect{} || wide.geometry != canvas::foundation::WorldRect{}) {
        std::cerr << "G47-SCENE-003 RED: absent derived layout must not fabricate heuristic bounds\n";
        return 1;
    }
    return 0;
}
