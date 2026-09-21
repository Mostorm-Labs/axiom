#include "brush_lab.hpp"
#include "reference_brushes.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
  if (argc == 1) {
    std::cout << canvas::brush_lab::manifestJson() << '\n';
    return 0;
  }
  if (argc != 2) {
    std::cerr << "usage: axiom_brush_lab [output-directory]\n";
    return 2;
  }
  const std::filesystem::path output(argv[1]);
  std::filesystem::create_directories(output);
  std::ofstream manifest(output / "manifest.json", std::ios::binary);
  manifest << canvas::brush_lab::manifestJson() << '\n';
  std::ofstream referenceManifest(output / "reference-brush-manifest.json", std::ios::binary);
  referenceManifest << canvas::brush_lab::referenceBrushManifestJson() << '\n';
  for (const auto family : {canvas::ink::BrushFamily::kPen,
                            canvas::ink::BrushFamily::kPencil,
                            canvas::ink::BrushFamily::kChalk,
                            canvas::ink::BrushFamily::kMarker,
                            canvas::ink::BrushFamily::kWaterColorLite,
                            canvas::ink::BrushFamily::kHighlighter,
                            canvas::ink::BrushFamily::kLaser}) {
    const auto scenario = canvas::brush_lab::evaluate(family);
    std::ofstream svg(output / (scenario.name + ".svg"), std::ios::binary);
    svg << scenario.svg << '\n';
  }
  return manifest && referenceManifest && !manifest.bad() && !referenceManifest.bad() ? 0 : 1;
}
