#include "canvas/semantic/rich_text_state.hpp"

#include "object_record_semantics_internal.hpp"
#include "protobuf_codec_internal.hpp"
#include "protobuf_object_mapping.hpp"
#include "sha256_internal.hpp"

#include <cmath>
#include <limits>
#if defined(CANVAS_SEMANTIC_PROTOBUF)
#include "auditoryworks/axiom/v1/object.pb.h"
#include "auditoryworks/axiom/v1/rich_text.pb.h"
#endif
namespace canvas::semantic {
std::vector<std::uint8_t> richTextStateDigest(DocumentId document, const ObjectRecord& record) {
#if !defined(CANVAS_SEMANTIC_PROTOBUF)
    (void)document;
    (void)record;
    return {};
#else
    if (document.isZero())
        return {};
    auto normalized = record;
    if (!internal::normalizeObjectRecord(normalized) || !internal::validateObjectRecord(normalized))
        return {};
    bool embeddedShapeLabel = false;
    if (normalized.kind == ObjectKind::kShape && normalized.kind_version == 3U) {
        const auto* shape = std::get_if<ShapeContent>(&normalized.content);
        if (shape == nullptr || !shape->label.has_value()) return {};
        embeddedShapeLabel = true;
        normalized.kind = ObjectKind::kRichText;
        normalized.kind_version = 3U;
        normalized.content = *shape->label;
    }
    if (normalized.kind != ObjectKind::kRichText) return {};
    auditoryworks::axiom::v1::ObjectRecord wire;
    if (!internal::toProtobufObjectRecord(normalized, wire))
        return {};
    auditoryworks::axiom::v1::RichTextStateV2 state;
    state.set_kind_version(normalized.kind_version);
    *state.mutable_document() = wire.content().rich_text().document();
    if (wire.content().rich_text().has_layout())
        *state.mutable_layout() = wire.content().rich_text().layout();
    const auto encoded = internal::canonicalEncodeProto(state);
    if (!encoded.ok())
        return {};
    std::vector<std::uint8_t> bytes;
    const auto number = [&](std::uint64_t n, unsigned width) {
        for (unsigned i = 0; i < width; ++i)
            bytes.push_back(static_cast<std::uint8_t>(n >> (8 * i)));
    };
    const std::string domain = "axiom.richtext.state.v2";
    number(domain.size(), 4);
    bytes.insert(bytes.end(), domain.begin(), domain.end());
    bytes.insert(bytes.end(), document.value().bytes.begin(), document.value().bytes.end());
    bytes.insert(bytes.end(), record.id.bytes.begin(), record.id.bytes.end());
    if (embeddedShapeLabel) bytes.push_back(1U);
    number(encoded.bytes.size(), 8);
    bytes.insert(bytes.end(), encoded.bytes.begin(), encoded.bytes.end());
    const auto digest = internal::sha256(bytes);
    return {digest.begin(), digest.end()};
#endif
}
} // namespace canvas::semantic
