#pragma once
#include "canvas/runtime/runtime_composition.hpp"
#include "canvas/text/text_layout.hpp"
#include "axiom/host_text_font.hpp"

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
    // New bytes get a new identity; prior Roboto resources are not substituted.
    font.resource_id = semantic::ResourceId{foundation::ObjectId::fromUint64(0xFC0202)};
    font.resource_kind = semantic::ResourceKind::kFont;
    font.resource_revision = 1;
    font.content_hash = "sha256:" + std::string(kHostTextFontSha256);
    font.byte_length = bytes.size();
    font.decode_version = 1;
    font.layout_version = 1;
    font.metadata = {8, 1, 16, 0, 24, 1};
    manifest.entries = {font};
    return composition.bindManifest(manifest, 1) &&
           composition.publishResource(font.resource_id.value, std::move(bytes)) &&
           composition.ownerPump();
}
// Application assembly only: immutable identities come from the checked-in
// catalog; admission, font registration and refresh remain owned by Runtime.
inline bool bindHostTextFonts(runtime::RuntimeComposition& composition,
                              const std::filesystem::path& cjkPath,
                              const std::filesystem::path& catalogRoot) {
    struct Pending { foundation::ObjectId id; std::vector<std::uint8_t> bytes; };
    std::vector<Pending> pending;
    semantic::ResourceManifest manifest;
    const auto add = [&](foundation::ObjectId id, const std::filesystem::path& path,
                         std::string_view hash, std::uint64_t size) {
        std::ifstream file(path, std::ios::binary);
        if (!file) return false;
        std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(file), {}};
        if (bytes.size() != size) return false;
        semantic::ResourceManifestEntry entry;
        entry.resource_id = semantic::ResourceId{id};
        entry.resource_kind = semantic::ResourceKind::kFont;
        entry.resource_revision = 1;
        entry.content_hash = "sha256:" + std::string(hash);
        entry.byte_length = size;
        entry.decode_version = entry.layout_version = 1;
        entry.metadata = {8, 1, 16, 0, 24, 1};
        manifest.entries.push_back(std::move(entry));
        pending.push_back({id, std::move(bytes)});
        return true;
    };
    std::error_code error;
    const auto cjkSize = std::filesystem::file_size(cjkPath, error);
    if (error || !add(foundation::ObjectId::fromUint64(0xFC0202), cjkPath,
                      kHostTextFontSha256, cjkSize)) return false;
    for (const auto& font : kHostImportedFonts)
        if (!add(foundation::ObjectId::fromUint64(font.id), catalogRoot / font.file,
                 font.sha256, font.bytes)) return false;
    if (!semantic::normalizeResourceManifest(manifest) ||
        !composition.bindManifest(manifest, 1)) return false;
    for (auto& item : pending)
        if (!composition.publishResource(item.id, std::move(item.bytes))) return false;
    return composition.ownerPump();
}
} // namespace canvas::minimal_runtime_host
