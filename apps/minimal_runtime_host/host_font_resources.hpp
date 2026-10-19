#pragma once
#include "canvas/runtime/runtime_composition.hpp"
#include "canvas/text/text_layout.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>
namespace canvas::minimal_runtime_host {
inline bool bindHostTextFont(runtime::RuntimeComposition& composition,
                             const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file)
        return false;
    std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(file), {}};
    if (bytes.empty())
        return false;
    semantic::ResourceManifest manifest;
    semantic::ResourceManifestEntry font;
    font.resource_id = semantic::ResourceId{foundation::ObjectId::fromUint64(0xFC0201)};
    font.resource_kind = semantic::ResourceKind::kFont;
    font.resource_revision = 1;
    font.content_hash = "sha256:" + std::string(text::kRobotoSha256);
    font.byte_length = bytes.size();
    font.decode_version = 1;
    font.layout_version = 1;
    font.metadata = {8, 1, 16, 0, 24, 1};
    manifest.entries = {font};
    return composition.bindManifest(manifest, 1) &&
           composition.publishResource(font.resource_id.value, std::move(bytes)) &&
           composition.ownerPump();
}
} // namespace canvas::minimal_runtime_host
