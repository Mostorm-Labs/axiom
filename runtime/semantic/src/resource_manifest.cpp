#include "canvas/semantic/resource_manifest.hpp"
#include "sha256_internal.hpp"
#include <algorithm>
#include <cctype>
#include <cstring>

namespace {
std::vector<std::uint8_t> lineageProjection(
    const canvas::foundation::ObjectId& document_id,
    const canvas::semantic::ResourceManifestTombstone& tombstone) {
    std::vector<std::uint8_t> bytes;
    constexpr char domain[] = "axiom.resource.tombstone.v1";
    bytes.insert(bytes.end(), domain, domain + sizeof(domain) - 1U);
    bytes.insert(bytes.end(), document_id.bytes.begin(), document_id.bytes.end());
    bytes.insert(bytes.end(), tombstone.resource_id.value.bytes.begin(), tombstone.resource_id.value.bytes.end());
    for (unsigned i=0;i<8U;++i) bytes.push_back(static_cast<std::uint8_t>(tombstone.retired_revision>>(8U*i)));
    bytes.insert(bytes.end(), tombstone.preimage.content_hash.begin(), tombstone.preimage.content_hash.end());
    return bytes;
}
}

namespace canvas::semantic {
namespace {
bool idValid(const ResourceId& id) noexcept { return !id.value.isZero(); }
bool kindValid(ResourceKind kind) noexcept { auto v=static_cast<std::uint32_t>(kind); return v>=1U&&v<=3U; }
}
bool contentHashValid(const ContentHash& value) noexcept {
 if(value.size()!=71 || value.rfind("sha256:",0)!=0) return false;
 for(std::size_t i=7;i<value.size();++i) if(!std::isxdigit(static_cast<unsigned char>(value[i])) || std::isupper(static_cast<unsigned char>(value[i]))) return false;
 return true;
}
std::vector<std::uint8_t> resourceManifestLineageToken(const canvas::foundation::ObjectId& document_id, const ResourceManifestTombstone& tombstone) noexcept {
 const auto projection = lineageProjection(document_id, tombstone);
 const auto digest = internal::sha256(projection);
 return {digest.begin(), digest.end()};
}
bool resourceKindMetadataValid(ResourceKind kind,const std::vector<std::uint8_t>& metadata) noexcept {
 const auto limit = kind == ResourceKind::kFont ? 8192U : 4096U;
 if(metadata.empty() || metadata.size()>limit || !kindValid(kind)) return false;
 // Metadata is a canonical protobuf message with explicit required fields.
 // Keep this validator renderer independent and reject unknown/duplicate fields.
 std::size_t offset = 0U;
 std::uint32_t seen = 0U;
 std::uint32_t values[32]{};
 auto readVarint = [&](std::uint64_t& value) noexcept {
  value = 0U; unsigned shift = 0U;
  while (offset < metadata.size() && shift < 64U) {
   const auto byte = metadata[offset++];
   value |= static_cast<std::uint64_t>(byte & 0x7fU) << shift;
   if ((byte & 0x80U) == 0U) return true;
   shift += 7U;
  }
  return false;
 };
 while (offset < metadata.size()) {
  std::uint64_t key = 0U, value = 0U;
  if (!readVarint(key) || key == 0U || (key & 7U) != 0U || key > (0x1fffffffULL << 3U)) return false;
  const auto field = static_cast<std::uint32_t>(key >> 3U);
  if (field == 0U || field > 32U || (seen & (1U << (field - 1U))) != 0U) return false;
  seen |= 1U << (field - 1U);
  if (!readVarint(value)) return false;
  if (value > 0xffffffffULL) return false;
  values[field - 1U] = static_cast<std::uint32_t>(value);
 }
 const auto required = kind == ResourceKind::kImage ? 0xFU : kind == ResourceKind::kFont ? 0x7U : 0x1FU;
 const auto max_field = kind == ResourceKind::kImage ? 4U : kind == ResourceKind::kFont ? 3U : 5U;
 if ((seen >> max_field) != 0U) return false;
 if ((seen & required) != required) return false;
 if (values[0] != 1U) return false;
 if (kind == ResourceKind::kImage) return values[1] > 0U && values[2] > 0U && values[3] == 1U;
 if (kind == ResourceKind::kFont) return values[2] == 1U;
 return (values[1] == 1U || values[1] == 2U) && values[2] > 0U && values[3] > 0U && values[4] == 1U;
}
bool validateResourceManifestEntry(const ResourceManifestEntry& e) noexcept {
 return idValid(e.resource_id)&&kindValid(e.resource_kind)&&e.resource_revision>0&&contentHashValid(e.content_hash)&&e.byte_length>0&&e.decode_version==1U&&(e.resource_kind==ResourceKind::kFont ? e.layout_version==1U : e.layout_version==0U)&&resourceKindMetadataValid(e.resource_kind,e.metadata);
}
bool normalizeResourceManifestEntry(ResourceManifestEntry& e) noexcept { return validateResourceManifestEntry(e); }
bool normalizeResourceManifest(ResourceManifest& m) noexcept {
 if(m.schema_version!=1U) return false;
 std::sort(m.entries.begin(),m.entries.end(),[](const auto&a,const auto&b){return a.resource_id.value<b.resource_id.value;});
 std::sort(m.retired_entries.begin(),m.retired_entries.end(),[](const auto&a,const auto&b){return a.resource_id.value<b.resource_id.value;});
 return validateResourceManifest(m);
}
bool validateResourceManifest(const ResourceManifest& m) noexcept {
 if(m.schema_version!=1U||m.entries.size()>65535U||m.retired_entries.size()>65535U) return false;
 for(std::size_t i=0;i<m.entries.size();++i){if(!validateResourceManifestEntry(m.entries[i]))return false;if(i&&!(m.entries[i-1].resource_id.value<m.entries[i].resource_id.value))return false;}
 for(std::size_t i=0;i<m.retired_entries.size();++i){const auto&t=m.retired_entries[i];if(!idValid(t.resource_id)||t.retired_revision==0||t.removal_operation_id.isZero()||t.lineage_token.size()!=32||!validateResourceManifestEntry(t.preimage)||t.preimage.resource_id!=t.resource_id||t.last_content_hash!=t.preimage.content_hash||t.retired_revision!=t.preimage.resource_revision+1)return false;if(i&&!(m.retired_entries[i-1].resource_id.value<t.resource_id.value))return false;}
 for (const auto& entry : m.entries) for (const auto& tombstone : m.retired_entries) if (entry.resource_id == tombstone.resource_id && (entry.resource_kind != tombstone.preimage.resource_kind || entry.resource_revision <= tombstone.retired_revision)) return false;
 return true;
}
} // namespace canvas::semantic
