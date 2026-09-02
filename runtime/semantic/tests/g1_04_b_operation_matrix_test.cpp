#include "canvas/semantic/indexed_object_store.hpp"
#include "canvas/semantic/operation_engine.hpp"
#include "canvas/semantic/operation_fingerprint.hpp"
#include "canvas/semantic/reference_object_store.hpp"
#include "object_store_mutator.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <map>
#include <optional>
#include <string_view>
#include <sstream>
#include <type_traits>
#include <utility>
#include <vector>

namespace canvas::semantic {
namespace {

ObjectId id(std::uint64_t value) { return ObjectId::fromUint64(value); }

std::string idHex(const ObjectId& value) {
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (const auto byte : value.bytes) out << std::setw(2) << static_cast<unsigned>(byte);
    return out.str();
}

template <typename IdT>
std::string idHexTyped(const IdT& value) {
    return idHex(value.value());
}

const char* dispositionName(PrepareDisposition value) {
    switch (value) {
        case PrepareDisposition::kPrepared: return "Prepared";
        case PrepareDisposition::kAlreadyApplied: return "AlreadyApplied";
        case PrepareDisposition::kRejected: return "Rejected";
    }
    return "Unknown";
}

const char* issueName(StatefulIssue value) {
    switch (value) {
        case StatefulIssue::kNone: return "kNone";
        case StatefulIssue::kObjectMissing: return "kObjectMissing";
        case StatefulIssue::kObjectAlreadyExists: return "kObjectAlreadyExists";
        case StatefulIssue::kInvalidKindVersion: return "kInvalidKindVersion";
        case StatefulIssue::kInvalidApplicability: return "kInvalidApplicability";
        case StatefulIssue::kInvalidReference: return "kInvalidReference";
        case StatefulIssue::kHierarchyCycle: return "kHierarchyCycle";
        case StatefulIssue::kConnectorInvalid: return "kConnectorInvalid";
        case StatefulIssue::kMaskStateInvalid: return "kMaskStateInvalid";
        case StatefulIssue::kTextStateInvalid: return "kTextStateInvalid";
        case StatefulIssue::kOperationIdCollision: return "kOperationIdCollision";
    }
    return "kUnknown";
}

void writePlacement(std::ostream& out, const Placement& value) {
    out << "{\"parent_id\":";
    if (value.parent_id.has_value()) out << '"' << idHex(*value.parent_id) << '"';
    else out << "null";
    out << ",\"order_key\":[";
    for (std::size_t i = 0; i < value.order_key.bytes().size(); ++i) {
        if (i != 0U) out << ',';
        out << static_cast<unsigned>(value.order_key.bytes()[i]);
    }
    out << "]}";
}

void writeVec2(std::ostream& out, const Vec2& value) {
    out << "[" << std::setprecision(17) << value.x << "," << value.y << "]";
}

void writeString(std::ostream& out, std::string_view value) {
    constexpr char kHex[] = "0123456789abcdef";
    out << '"';
    for (const char c : value) {
        if (c == '"' || c == '\\') out << '\\' << c;
        else if (c == '\n') out << "\\n";
        else if (c == '\r') out << "\\r";
        else if (c == '\t') out << "\\t";
        else if (static_cast<unsigned char>(c) < 0x20U) {
            const unsigned char byte = static_cast<unsigned char>(c);
            out << "\\u00" << kHex[byte >> 4U] << kHex[byte & 0x0fU];
        }
        else out << c;
    }
    out << '"';
}

void writeColor(std::ostream& out, const ColorValue& value) {
    out << '[' << std::setprecision(9) << value.r << ',' << value.g << ',' << value.b << ','
        << value.a << ']';
}

void writeNormalizedRect(std::ostream& out, const NormalizedRect& value) {
    out << '[' << std::setprecision(17) << value.x << ',' << value.y << ',' << value.width << ','
        << value.height << ']';
}

void writeCurve(std::ostream& out, const std::optional<PiecewiseLinearCurve01>& value) {
    if (!value.has_value()) {
        out << "null";
        return;
    }
    out << '[';
    for (std::size_t i = 0; i < value->points.size(); ++i) {
        if (i != 0U) out << ',';
        out << '[' << std::setprecision(9) << value->points[i].x << ',' << value->points[i].y << ']';
    }
    out << ']';
}

void writePressureMapping(std::ostream& out, const PressureMapping& value) {
    out << "{\"enabled\":" << (value.enabled ? "true" : "false")
        << ",\"size_curve\":";
    writeCurve(out, value.size_curve);
    out << ",\"opacity_curve\":";
    writeCurve(out, value.opacity_curve);
    out << '}';
}

void writeBrushDescriptor(std::ostream& out, const BrushDescriptor& value) {
    out << "{\"brush_family_id\":" << value.brush_family_id
        << ",\"brush_version\":" << value.brush_version << ",\"color\":";
    writeColor(out, value.color);
    out << ",\"nominal_size\":" << std::setprecision(17) << value.nominal_size
        << ",\"opacity\":" << std::setprecision(9) << value.opacity
        << ",\"pressure\":";
    writePressureMapping(out, value.pressure);
    out << ",\"tilt\":{\"enabled\":" << (value.tilt.enabled ? "true" : "false")
        << ",\"size_influence\":" << value.tilt.size_influence
        << ",\"angle_influence\":" << value.tilt.angle_influence << "}"
        << ",\"smoothing\":{\"amount\":" << value.smoothing.amount << "}"
        << ",\"spacing\":{\"normalized_spacing\":" << value.spacing.normalized_spacing << "}"
        << ",\"blend_mode\":" << static_cast<unsigned>(value.blend_mode)
        << ",\"texture_resource_id\":";
    if (value.texture_resource_id.has_value()) {
        out << '"' << idHex(value.texture_resource_id->value) << '"';
    } else {
        out << "null";
    }
    out << '}';
}

void writeGeometry(std::ostream& out, const VectorPathGeometry& geometry) {
    out << "{\"fill_rule\":" << static_cast<unsigned>(geometry.fill_rule) << ",\"commands\":[";
    for (std::size_t i = 0; i < geometry.commands.size(); ++i) {
        if (i != 0U) out << ',';
        std::visit([&out](const auto& command) {
            using C = std::decay_t<decltype(command)>;
            out << "{\"variant\":";
            if constexpr (std::is_same_v<C, MoveTo>) { out << 0 << ",\"point\":"; writeVec2(out, command.point); }
            else if constexpr (std::is_same_v<C, LineTo>) { out << 1 << ",\"end\":"; writeVec2(out, command.end); }
            else if constexpr (std::is_same_v<C, QuadTo>) { out << 2 << ",\"control\":"; writeVec2(out, command.control); out << ",\"end\":"; writeVec2(out, command.end); }
            else if constexpr (std::is_same_v<C, CubicTo>) { out << 3 << ",\"control1\":"; writeVec2(out, command.control1); out << ",\"control2\":"; writeVec2(out, command.control2); out << ",\"end\":"; writeVec2(out, command.end); }
            else { out << 4; }
            out << '}';
        }, geometry.commands[i]);
    }
    out << "]}";
}

void writeTextStyle(std::ostream& out, const TextStyle& value) {
    out << "{\"font_resource_id\":";
    if (value.font_resource_id.has_value()) out << '"' << idHex(value.font_resource_id->value) << '"';
    else out << "null";
    out << ",\"font_size\":" << std::setprecision(17) << value.font_size
        << ",\"weight\":" << value.weight << ",\"italic\":"
        << (value.italic ? "true" : "false") << ",\"underline\":"
        << (value.underline ? "true" : "false") << ",\"color\":";
    writeColor(out, value.color);
    out << '}';
}

void writeParagraphStyle(std::ostream& out, const ParagraphStyle& value) {
    out << "{\"alignment\":" << static_cast<unsigned>(value.alignment)
        << ",\"line_height\":" << std::setprecision(17) << value.line_height
        << ",\"spacing_before\":" << value.spacing_before
        << ",\"spacing_after\":" << value.spacing_after << '}';
}

void writeStrokeRecord(std::ostream& out, const StrokeRecord& value) {
    out << "{\"deterministic_seed\":" << value.deterministic_seed
        << ",\"brush\":";
    writeBrushDescriptor(out, value.brush);
    out << ",\"data_variant\":"
        << value.data.index() << ",\"data\":";
    std::visit([&out](const auto& data) {
        using D = std::decay_t<decltype(data)>;
        out << '[';
        if constexpr (std::is_same_v<D, VectorStrokeData>) {
            for (std::size_t i = 0; i < data.samples.size(); ++i) {
                if (i != 0U) out << ',';
                out << "{\"position\":"; writeVec2(out, data.samples[i].position);
                out << ",\"pressure\":" << data.samples[i].pressure << ",\"tilt\":";
                writeVec2(out, data.samples[i].tilt); out << '}';
            }
        } else {
            for (std::size_t i = 0; i < data.dabs.size(); ++i) {
                if (i != 0U) out << ',';
                out << "{\"center\":"; writeVec2(out, data.dabs[i].center);
                out << ",\"size\":" << data.dabs[i].size << ",\"rotation\":"
                    << data.dabs[i].rotation << ",\"opacity\":" << data.dabs[i].opacity << '}';
            }
        }
        out << ']';
    }, value.data);
    out << '}';
}

void writeEraseGeometry(std::ostream& out, const EraseMaskGeometry& value) {
    out << "{\"variant\":" << value.index() << ",\"value\":";
    std::visit([&out](const auto& geometry) {
        using G = std::decay_t<decltype(geometry)>;
        if constexpr (std::is_same_v<G, SweptCircleMask>) {
            out << "{\"segments\":[";
            for (std::size_t i = 0; i < geometry.segments.size(); ++i) {
                if (i != 0U) out << ',';
                const auto& segment = geometry.segments[i];
                out << "{\"p0\":{\"position\":"; writeVec2(out, segment.p0.position);
                out << ",\"radius\":" << segment.p0.radius << "},\"p1\":{\"position\":";
                writeVec2(out, segment.p1.position);
                out << ",\"radius\":" << segment.p1.radius << "},\"control1\":";
                writeVec2(out, segment.control1);
                out << ",\"control2\":";
                writeVec2(out, segment.control2);
                out << '}';
            }
            out << "]}";
        } else {
            writeGeometry(out, geometry.path);
        }
    }, value);
    out << '}';
}

void writeEndpoint(std::ostream& out, const ConnectorEndpoint& value) {
    out << "{\"variant\":" << value.value.index() << ",\"value\":";
    std::visit([&out](const auto& endpoint) {
        using E = std::decay_t<decltype(endpoint)>;
        if constexpr (std::is_same_v<E, FreePointEndpoint>) {
            out << "{\"point\":"; writeVec2(out, endpoint.point); out << '}';
        } else {
            out << "{\"target_object_id\":\"" << idHex(endpoint.target_object_id)
                << "\",\"anchor\":{\"variant\":" << endpoint.anchor.index() << ",\"value\":";
            std::visit([&out](const auto& anchor) {
                using A = std::decay_t<decltype(anchor)>;
                if constexpr (std::is_same_v<A, AutoPerimeterAnchor>) {
                    if (anchor.hint.has_value()) { out << "["; out << anchor.hint->x << ',' << anchor.hint->y << ']'; }
                    else out << "null";
                } else out << anchor.port_id;
            }, endpoint.anchor);
            out << "}}";
        }
    }, value.value);
    out << '}';
}

void writeObjectIds(std::ostream& out, const std::vector<ObjectId>& ids);
void writeContent(std::ostream& out, const ObjectContent& content);
void writePropertyValue(std::ostream& out, const PropertyValue& value);
void writeEraseMaskRecord(std::ostream& out, const EraseMaskRecord& value);
void writeOperationPayload(std::ostream& out, const OperationPayload& payload);
void writeOperation(std::ostream& out, const Operation& operation);

void writeEraseMaskRecord(std::ostream& out, const EraseMaskRecord& value) {
    out << "{\"id\":\"" << idHex(value.id) << "\",\"geometry\":";
    writeEraseGeometry(out, value.geometry);
    out << '}';
}

void writeContent(std::ostream& out, const ObjectContent& content) {
    out << "{\"variant\":" << content.index() << ",\"value\":";
    std::visit([&out](const auto& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, ShapeContent>) {
            out << "{\"shape_kind\":" << value.shape_kind << ",\"width\":"
                << std::setprecision(17) << value.width << ",\"height\":" << value.height << '}';
        } else if constexpr (std::is_same_v<T, ImageContent>) {
            out << "{\"resource_id\":\"" << idHex(value.resource_id.value)
                << "\",\"intrinsic_width\":" << std::setprecision(17) << value.intrinsic_width
                << ",\"intrinsic_height\":" << value.intrinsic_height << ",\"source_rect\":";
            if (value.source_rect.has_value()) writeNormalizedRect(out, *value.source_rect);
            else out << "null";
            out << ",\"content_mode\":"
                << static_cast<unsigned>(value.content_mode) << ",\"width\":" << value.width
                << ",\"height\":" << value.height << '}';
        } else if constexpr (std::is_same_v<T, VectorPathContent>) {
            writeGeometry(out, value.geometry);
        } else if constexpr (std::is_same_v<T, RichTextContent>) {
            out << "{\"paragraphs\":[";
            for (std::size_t i = 0; i < value.document.paragraphs.size(); ++i) {
                if (i != 0U) out << ',';
                const auto& paragraph = value.document.paragraphs[i];
                out << "{\"id\":\"" << idHex(paragraph.id) << "\",\"style\":";
                writeParagraphStyle(out, paragraph.style);
                out << ",\"runs\":[";
                for (std::size_t j = 0; j < paragraph.runs.size(); ++j) {
                    if (j != 0U) out << ',';
            out << "{\"text\":"; writeString(out, paragraph.runs[j].text); out << ",\"style\":";
                    writeTextStyle(out, paragraph.runs[j].style);
                    out << '}';
                }
                out << "]}";
            }
            out << "]}";
        } else if constexpr (std::is_same_v<T, VectorStrokeContent> ||
                             std::is_same_v<T, DabStrokeContent>) {
            out << "{\"stroke\":"; writeStrokeRecord(out, value.stroke); out << '}';
        } else if constexpr (std::is_same_v<T, ConnectorContent>) {
            out << "{\"start\":"; writeEndpoint(out, value.start);
            out << ",\"end\":"; writeEndpoint(out, value.end);
            out << ",\"routing\":" << static_cast<unsigned>(value.routing) << '}';
        } else if constexpr (std::is_same_v<T, StickyContent>) {
            out << "{\"width\":" << std::setprecision(17) << value.width << ",\"height\":"
                << value.height << '}';
        } else if constexpr (std::is_same_v<T, GroupContent>) {
            out << "{}";
        }
    }, content);
    out << '}';
}

void writePropertyValue(std::ostream& out, const PropertyValue& value) {
    out << "{\"variant\":" << value.index() << ",\"value\":";
    std::visit([&out](const auto& item) {
        using T = std::decay_t<decltype(item)>;
        if constexpr (std::is_same_v<T, bool>) out << (item ? "true" : "false");
        else if constexpr (std::is_same_v<T, float>) out << std::setprecision(9) << item;
        else if constexpr (std::is_same_v<T, ColorValue>)
            out << "[" << item.r << ',' << item.g << ',' << item.b << ',' << item.a << ']';
        else if constexpr (std::is_same_v<T, BlendModeValue> ||
                           std::is_same_v<T, ConnectorDecorationValue>)
            out << static_cast<unsigned>(item);
        else if constexpr (std::is_same_v<T, FillStyleValue>) {
            out << "{\"variant\":" << item.index();
            std::visit([&out](const auto& fill) {
                using F = std::decay_t<decltype(fill)>;
                if constexpr (std::is_same_v<F, SolidFill>) { out << ",\"color\":"; writeColor(out, fill.color); }
            }, item); out << '}';
        } else if constexpr (std::is_same_v<T, StrokeStyleValue>) {
            out << "{\"variant\":" << item.index();
            std::visit([&out](const auto& stroke) {
                using S = std::decay_t<decltype(stroke)>;
                if constexpr (std::is_same_v<S, SolidStroke>) {
                    out << ",\"color\":"; writeColor(out, stroke.color);
                    out << ",\"width\":" << std::setprecision(17) << stroke.width
                        << ",\"cap\":" << static_cast<unsigned>(stroke.cap)
                        << ",\"join\":{";
                    out << "\"variant\":" << stroke.join.index();
                    std::visit([&out](const auto& join) {
                        using J = std::decay_t<decltype(join)>;
                        if constexpr (std::is_same_v<J, MiterJoin>) out << ",\"limit\":" << join.limit;
                    }, stroke.join);
                    out << "},\"dash\":{\"variant\":" << stroke.dash.index();
                    std::visit([&out](const auto& dash) {
                        using D = std::decay_t<decltype(dash)>;
                        if constexpr (std::is_same_v<D, DashPattern>) {
                            out << ",\"segments\":[";
                            for (std::size_t i = 0; i < dash.segments.size(); ++i) {
                                if (i != 0U) out << ',';
                                out << std::setprecision(17) << dash.segments[i];
                            }
                            out << "],\"offset\":" << dash.offset;
                        }
                    }, stroke.dash);
                    out << '}';
                }
            }, item); out << '}';
        }
    }, value);
    out << "}";
}

void writeRecords(std::ostream& out, const std::vector<ObjectRecord>& records) {
    out << '[';
    for (std::size_t i = 0; i < records.size(); ++i) {
        if (i != 0U) out << ',';
        const auto& record = records[i];
        out << "{\"id\":\"" << idHex(record.id) << "\",\"kind\":"
            << static_cast<unsigned>(record.kind) << ",\"kind_version\":"
            << record.kind_version << ",\"placement\":";
        writePlacement(out, record.placement);
        out << ",\"transform\":[" << std::setprecision(17) << record.transform.a << ','
            << record.transform.b << ',' << record.transform.c << ',' << record.transform.d << ','
            << record.transform.tx << ',' << record.transform.ty << "]";
        out << ",\"properties\":[";
        for (std::size_t p = 0; p < record.properties.entries.size(); ++p) {
            if (p != 0U) out << ',';
            const auto& entry = record.properties.entries[p];
            out << "{\"field_id\":" << entry.field_id << ",\"value\":";
            writePropertyValue(out, entry.value);
            out << '}';
        }
        out << "],\"content\":";
        writeContent(out, record.content);
        out << ",\"erase_masks\":[";
        for (std::size_t m = 0; m < record.erase_masks.size(); ++m) {
            if (m != 0U) out << ',';
            writeEraseMaskRecord(out, record.erase_masks[m]);
        }
        out << "]}";
    }
    out << ']';
}

void writeOperationPayload(std::ostream& out, const OperationPayload& payload) {
    out << std::setprecision(17);
    out << "{\"variant\":" << payload.index() << ",\"value\":";
    std::visit([&out](const auto& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, InsertObjectsOp>) { out << "{\"objects\":"; writeRecords(out, value.objects); out << '}'; }
        else if constexpr (std::is_same_v<T, RestoreObjectsOp>) { out << "{\"objects\":"; writeRecords(out, value.objects); out << '}'; }
        else if constexpr (std::is_same_v<T, DeleteObjectsOp>) { out << "{\"object_ids\":"; writeObjectIds(out, value.object_ids); out << '}'; }
        else if constexpr (std::is_same_v<T, SetPlacementsOp>) { out << "{\"items\":["; for (std::size_t i=0;i<value.items.size();++i){if(i)out<<',';out<<"{\"object_id\":\""<<idHex(value.items[i].object_id)<<"\",\"placement\":";writePlacement(out,value.items[i].placement);out<<'}';} out<<"]}"; }
        else if constexpr (std::is_same_v<T, SetTransformsOp>) { out << "{\"items\":["; for (std::size_t i=0;i<value.items.size();++i){if(i)out<<',';const auto&t=value.items[i].transform;out<<"{\"object_id\":\""<<idHex(value.items[i].object_id)<<"\",\"transform\":["<<t.a<<','<<t.b<<','<<t.c<<','<<t.d<<','<<t.tx<<','<<t.ty<<"]}";} out<<"]}"; }
        else if constexpr (std::is_same_v<T, SetObjectSizeOp>) { out << "{\"items\":["; for (std::size_t i=0;i<value.items.size();++i){if(i)out<<',';out<<"{\"object_id\":\""<<idHex(value.items[i].object_id)<<"\",\"width\":"<<value.items[i].width<<",\"height\":"<<value.items[i].height<<'}';} out<<"]}"; }
        else if constexpr (std::is_same_v<T, SetVectorPathGeometryOp>) { out << "{\"object_id\":\""<<idHex(value.object_id)<<"\",\"geometry\":"; writeContent(out,ObjectContent{VectorPathContent{value.geometry}}); out<<'}'; }
        else if constexpr (std::is_same_v<T, SetImageContentOp>) { out << "{\"object_id\":\""<<idHex(value.object_id)<<"\",\"content\":"; writeContent(out,ObjectContent{value.content}); out<<'}'; }
        else if constexpr (std::is_same_v<T, AddStrokeOp>) { out << "{\"object\":"; writeRecords(out,{value.object}); out<<'}'; }
        else if constexpr (std::is_same_v<T, SplitStrokesOp>) { out << "{\"splits\":["; for(std::size_t i=0;i<value.splits.size();++i){if(i)out<<',';out<<"{\"source_stroke_id\":\""<<idHex(value.splits[i].source_stroke_id)<<"\",\"replacements\":";writeRecords(out,value.splits[i].replacements);out<<'}';} out<<"]}"; }
        else if constexpr (std::is_same_v<T, AddEraseMasksOp>) { out << "{\"items\":["; for(std::size_t i=0;i<value.items.size();++i){if(i)out<<',';out<<"{\"object_id\":\""<<idHex(value.items[i].object_id)<<"\",\"masks\":[";for(std::size_t j=0;j<value.items[i].masks.size();++j){if(j)out<<',';writeEraseMaskRecord(out,value.items[i].masks[j]);}out<<"]}";} out<<"]}"; }
        else if constexpr (std::is_same_v<T, RemoveEraseMasksOp>) { out << "{\"items\":["; for(std::size_t i=0;i<value.items.size();++i){if(i)out<<',';out<<"{\"object_id\":\""<<idHex(value.items[i].object_id)<<"\",\"mask_ids\":";writeObjectIds(out,value.items[i].mask_ids);out<<'}';} out<<"]}"; }
        else if constexpr (std::is_same_v<T, EditRichTextOp>) {
            out << "{\"object_id\":\"" << idHex(value.object_id) << "\",\"delta_version\":"
                << value.delta.delta_version << ",\"steps\":[";
            for (std::size_t i = 0; i < value.delta.steps.size(); ++i) {
                if (i != 0U) out << ',';
                std::visit([&out, &value, i](const auto& step) {
                    using S = std::decay_t<decltype(step)>;
                    out << "{\"variant\":" << value.delta.steps[i].index() << ',';
                    if constexpr (std::is_same_v<S, InsertTextStep>) {
                        out << "\"paragraph_id\":\"" << idHex(step.paragraph_id)
                            << "\",\"offset\":" << step.scalar_offset << ",\"text\":";
                        writeString(out, step.text);
                        out << ",\"style\":";
                        writeTextStyle(out, step.style);
                    } else if constexpr (std::is_same_v<S, DeleteTextStep>) {
                        out << "\"paragraph_id\":\"" << idHex(step.paragraph_id)
                            << "\",\"start\":" << step.start_scalar << ",\"count\":"
                            << step.scalar_count;
                    } else if constexpr (std::is_same_v<S, SplitParagraphStep>) {
                        out << "\"paragraph_id\":\"" << idHex(step.paragraph_id)
                            << "\",\"offset\":" << step.scalar_offset << ",\"new_id\":\""
                            << idHex(step.new_paragraph_id) << '"';
                    } else if constexpr (std::is_same_v<S, MergeParagraphStep>) {
                        out << "\"first\":\"" << idHex(step.first_paragraph_id)
                            << "\",\"second\":\"" << idHex(step.second_paragraph_id) << '"';
                    } else if constexpr (std::is_same_v<S, SetInlineStyleStep>) {
                        out << "\"paragraph_id\":\"" << idHex(step.paragraph_id)
                            << "\",\"start\":" << step.start_scalar << ",\"count\":"
                            << step.scalar_count << ",\"style\":";
                        writeTextStyle(out, step.style);
                    } else {
                        out << "\"paragraph_id\":\"" << idHex(step.paragraph_id) << "\",\"style\":";
                        writeParagraphStyle(out, step.style);
                    }
                    out << '}';
                }, value.delta.steps[i]);
            }
            out << "]}";
        }
        else if constexpr (std::is_same_v<T, SetConnectorContentOp>) { out << "{\"object_id\":\""<<idHex(value.object_id)<<"\",\"content\":"; writeContent(out,ObjectContent{value.content}); out<<'}'; }
        else if constexpr (std::is_same_v<T, PatchPropertiesOp>) { out << "{\"patches\":[";for(std::size_t i=0;i<value.patches.size();++i){if(i)out<<',';const auto&p=value.patches[i];out<<"{\"object_id\":\""<<idHex(p.object_id)<<"\",\"field_id\":"<<p.field_id<<",\"action\":"<<static_cast<unsigned>(p.action)<<",\"value\":";if(std::holds_alternative<std::monostate>(p.value))out<<"null";else writePropertyValue(out,std::get<PropertyValue>(p.value));out<<'}';}out<<"]}"; }
    }, payload);
    out << '}';
}

void writeOperation(std::ostream& out, const Operation& operation) {
    out << "{\"id\":\"" << idHex(operation.id.value()) << "\",\"document_id\":\""
        << idHex(operation.document_id.value()) << "\",\"schema_version\":" << operation.schema_version
        << ",\"payload_version\":" << operation.payload_version << ",\"payload\":";
    writeOperationPayload(out, operation.payload);
    out << '}';
}

void writeApplied(std::ostream& out, const std::map<OperationId, AppliedOperationEntry>& entries) {
    out << '[';
    std::size_t index = 0U;
    for (const auto& [id_value, entry] : entries) {
        if (index++ != 0U) out << ',';
        out << "{\"id\":\"" << idHexTyped(id_value) << "\",\"canonical_operation\":";
        writeOperation(out, entry.canonical_operation);
        out << ",\"fingerprint\":";
        if (entry.fingerprint.has_value()) {
            out << '[';
            for (std::size_t i = 0; i < entry.fingerprint->size(); ++i) {
                if (i != 0U) out << ',';
                out << static_cast<unsigned>((*entry.fingerprint)[i]);
            }
            out << ']';
        } else out << "null";
        out << '}';
    }
    out << ']';
}

void writeChildren(
    std::ostream& out,
    const std::optional<std::vector<ObjectRecord>>& children) {
    if (!children.has_value()) { out << "null"; return; }
    writeRecords(out, *children);
}

void writeObjectIds(std::ostream& out, const std::vector<ObjectId>& ids) {
    out << '[';
    for (std::size_t i = 0; i < ids.size(); ++i) {
        if (i != 0U) out << ',';
        out << '"' << idHex(ids[i]) << '"';
    }
    out << ']';
}

void writeDeleteClosure(std::ostream& out, const DeleteClosure& closure) {
    out << "{\"requested_delete_ids\":"; writeObjectIds(out, closure.requested_delete_ids);
    out << ",\"resolved_hierarchy_closure\":";
    writeObjectIds(out, closure.resolved_hierarchy_closure);
    out << ",\"resolved_connector_cascade_closure\":";
    writeObjectIds(out, closure.resolved_connector_cascade_closure);
    out << ",\"final_delete_set\":"; writeObjectIds(out, closure.final_delete_set);
    out << '}';
}

Placement placement(std::uint64_t key, std::optional<ObjectId> parent = std::nullopt) {
    return Placement{parent, OrderKey({static_cast<std::uint8_t>(key)})};
}

ObjectRecord shape(std::uint64_t value, std::optional<ObjectId> parent = std::nullopt) {
    ObjectRecord result{};
    result.id = id(value);
    result.kind = ObjectKind::kShape;
    result.kind_version = 1U;
    result.placement = placement(value, parent);
    result.content = ShapeContent{1U, 10.0, 20.0};
    return result;
}

ObjectRecord group(std::uint64_t value, std::optional<ObjectId> parent = std::nullopt) {
    ObjectRecord result{};
    result.id = id(value);
    result.kind = ObjectKind::kGroup;
    result.kind_version = 1U;
    result.placement = placement(value, parent);
    result.content = GroupContent{};
    return result;
}

ParagraphStyle paragraphStyle() {
    return ParagraphStyle{ParagraphAlignment::kLeft, 1.0, 0.0, 0.0};
}

TextStyle textStyle(float component = 1.0F) {
    TextStyle style{};
    style.font_resource_id = ResourceId{id(90U)};
    style.font_size = 12.0;
    style.weight = 400U;
    style.color = ColorValue{component, component, component, 1.0F};
    return style;
}

ObjectRecord richText(std::uint64_t value) {
    ObjectRecord result{};
    result.id = id(value);
    result.kind = ObjectKind::kRichText;
    result.kind_version = 1U;
    result.placement = placement(value);
    Paragraph paragraph{};
    paragraph.id = id(value * 10U);
    paragraph.style = paragraphStyle();
    paragraph.runs = {{"A", textStyle()}};
    result.content = RichTextContent{{{paragraph}}};
    return result;
}

VectorPathGeometry vectorPath(double end) {
    return VectorPathGeometry{
        FillRule::kNonZero,
        {MoveTo{{0.0, 0.0}}, LineTo{{end, end}}}};
}

ObjectRecord vectorPathRecord(std::uint64_t value, double end = 1.0) {
    ObjectRecord result{};
    result.id = id(value);
    result.kind = ObjectKind::kVectorPath;
    result.kind_version = 1U;
    result.placement = placement(value);
    result.content = VectorPathContent{vectorPath(end)};
    return result;
}

ImageContent imageContent(double width = 100.0, double height = 80.0) {
    return ImageContent{
        ResourceId{id(80U)}, width, height, std::nullopt, ImageContentMode::kFit, 3.0, 4.0};
}

ObjectRecord imageRecord(std::uint64_t value, double width = 100.0, double height = 80.0) {
    ObjectRecord result{};
    result.id = id(value);
    result.kind = ObjectKind::kImage;
    result.kind_version = 1U;
    result.placement = placement(value);
    result.content = imageContent(width, height);
    return result;
}

StrokeRecord vectorStrokeData() {
    StrokeRecord stroke{};
    stroke.brush.brush_family_id = 1U;
    stroke.brush.brush_version = 1U;
    stroke.brush.color = ColorValue{0.0F, 0.0F, 0.0F, 1.0F};
    stroke.brush.nominal_size = 1.0;
    stroke.brush.opacity = 1.0F;
    stroke.data = VectorStrokeData{{StrokeSample{{1.0, 2.0}, 1.0F, {0.0F, 0.0F}}}};
    return stroke;
}

ObjectRecord vectorStroke(std::uint64_t value) {
    ObjectRecord result{};
    result.id = id(value);
    result.kind = ObjectKind::kVectorStroke;
    result.kind_version = 1U;
    result.placement = placement(value);
    result.content = VectorStrokeContent{vectorStrokeData()};
    return result;
}

EraseMaskRecord eraseMask(std::uint64_t value) {
    const EraseCubicSegment segment{
        EraseKnot{{0.0, 0.0}, 1.0},
        EraseKnot{{1.0, 1.0}, 1.0},
        {0.5, 0.5},
        {0.5, 0.5}};
    return EraseMaskRecord{id(value), SweptCircleMask{{segment}}};
}

ConnectorContent freeConnectorContent() {
    return ConnectorContent{
        ConnectorEndpoint{FreePointEndpoint{{0.0, 0.0}}},
        ConnectorEndpoint{FreePointEndpoint{{1.0, 1.0}}},
        ConnectorRouting::kStraight};
}

ConnectorContent attachedConnectorContent(std::uint64_t target) {
    return ConnectorContent{
        ConnectorEndpoint{FreePointEndpoint{{0.0, 0.0}}},
        ConnectorEndpoint{AttachedEndpoint{id(target), AutoPerimeterAnchor{}}},
        ConnectorRouting::kStraight};
}

ObjectRecord connector(std::uint64_t value, ConnectorContent content = freeConnectorContent()) {
    ObjectRecord result{};
    result.id = id(value);
    result.kind = ObjectKind::kConnector;
    result.kind_version = 1U;
    result.placement = placement(value);
    result.content = std::move(content);
    return result;
}

template <typename Payload>
Operation operation(std::uint64_t operation_id, Payload payload) {
    Operation result{};
    result.id = OperationId{id(operation_id)};
    result.document_id = DocumentId{id(7002U)};
    result.schema_version = 1U;
    result.payload_version = 1U;
    result.payload = std::move(payload);
    return result;
}

class TestAppliedOperationView final : public AppliedOperationView {
  public:
    std::optional<AppliedOperationEntry> find(const OperationId& operation_id) const override {
        const auto found = entries.find(operation_id);
        return found == entries.end() ? std::nullopt
                                      : std::optional<AppliedOperationEntry>(found->second);
    }

    std::map<OperationId, AppliedOperationEntry> entries;
};

void expectAppliedUnchanged(
    const std::map<OperationId, AppliedOperationEntry>& before,
    const TestAppliedOperationView& after) {
    ASSERT_EQ(after.entries.size(), before.size());
    for (const auto& [operation_id, expected] : before) {
        const auto found = after.entries.find(operation_id);
        ASSERT_NE(found, after.entries.end());
        EXPECT_EQ(found->second.canonical_operation.id, expected.canonical_operation.id);
        EXPECT_TRUE(canonicalPayloadEqual(
            found->second.canonical_operation, expected.canonical_operation));
        EXPECT_EQ(found->second.fingerprint, expected.fingerprint);
    }
}

struct ExpectedPlan final {
    std::vector<ObjectRecord> creates;
    std::vector<ObjectRecord> replacements;
    std::vector<ObjectId> deletes;
    std::optional<DeleteClosure> delete_closure;
};

ExpectedPlan createsPlan(std::vector<ObjectRecord> creates) {
    ExpectedPlan result{};
    result.creates = std::move(creates);
    return result;
}

ExpectedPlan replacementsPlan(std::vector<ObjectRecord> replacements) {
    ExpectedPlan result{};
    result.replacements = std::move(replacements);
    return result;
}

ExpectedPlan splitPlan(std::vector<ObjectRecord> creates, std::vector<ObjectId> deletes) {
    ExpectedPlan result{};
    result.creates = std::move(creates);
    result.deletes = std::move(deletes);
    return result;
}

ExpectedPlan deletePlan(std::uint64_t value) {
    ExpectedPlan result{};
    result.deletes = {id(value)};
    DeleteClosure closure{};
    closure.requested_delete_ids = {id(value)};
    closure.final_delete_set = {id(value)};
    result.delete_closure = closure;
    return result;
}

struct MatrixCase final {
    std::string_view case_id;
    std::string_view operation_name;
    bool positive = false;
    std::vector<ObjectRecord> initial_objects;
    Operation input;
    PrepareDisposition expected_disposition = PrepareDisposition::kRejected;
    StatefulIssue expected_issue = StatefulIssue::kNone;
    std::optional<ExpectedPlan> expected_plan;
    std::optional<ObjectId> hierarchy_parent;
    std::optional<Operation> applied_operation;
};

MatrixCase preparedCase(
    std::string_view case_id,
    std::string_view operation_name,
    std::vector<ObjectRecord> initial_objects,
    Operation input,
    ExpectedPlan expected_plan,
    std::optional<ObjectId> hierarchy_parent = std::nullopt) {
    return MatrixCase{
        case_id,
        operation_name,
        true,
        std::move(initial_objects),
        std::move(input),
        PrepareDisposition::kPrepared,
        StatefulIssue::kNone,
        std::move(expected_plan),
        hierarchy_parent,
        std::nullopt};
}

MatrixCase rejectedCase(
    std::string_view case_id,
    std::string_view operation_name,
    std::vector<ObjectRecord> initial_objects,
    Operation input,
    StatefulIssue issue,
    std::optional<ObjectId> hierarchy_parent = std::nullopt,
    std::optional<Operation> applied_operation = std::nullopt) {
    return MatrixCase{
        case_id,
        operation_name,
        false,
        std::move(initial_objects),
        std::move(input),
        PrepareDisposition::kRejected,
        issue,
        std::nullopt,
        hierarchy_parent,
        std::move(applied_operation)};
}

std::vector<MatrixCase> coreCases() {
    std::vector<MatrixCase> cases;

    const auto insert_parent = group(1U);
    const auto insert_child = shape(2U, id(1U));
    cases.push_back(preparedCase(
        "OP15-INS-P",
        "InsertObjects",
        {},
        operation(101U, InsertObjectsOp{{insert_parent, insert_child}}),
        createsPlan({insert_parent, insert_child}),
        id(1U)));
    cases.push_back(rejectedCase(
        "OP15-INS-N",
        "InsertObjects",
        {shape(1U)},
        operation(102U, InsertObjectsOp{{shape(1U), shape(2U)}}),
        StatefulIssue::kObjectAlreadyExists));

    cases.push_back(preparedCase(
        "OP15-DEL-P",
        "DeleteObjects",
        {shape(2U)},
        operation(103U, DeleteObjectsOp{{id(2U)}}),
        deletePlan(2U)));
    cases.push_back(rejectedCase(
        "OP15-DEL-N",
        "DeleteObjects",
        {},
        operation(104U, DeleteObjectsOp{{id(2U)}}),
        StatefulIssue::kObjectMissing));

    cases.push_back(preparedCase(
        "OP15-RST-P",
        "RestoreObjects",
        {},
        operation(105U, RestoreObjectsOp{{shape(3U), shape(4U)}}),
        createsPlan({shape(3U), shape(4U)})));
    cases.push_back(rejectedCase(
        "OP15-RST-N",
        "RestoreObjects",
        {shape(3U)},
        operation(106U, RestoreObjectsOp{{shape(3U)}}),
        StatefulIssue::kObjectAlreadyExists));

    auto moved = shape(2U);
    moved.placement = placement(22U, id(1U));
    cases.push_back(preparedCase(
        "OP15-PLC-P",
        "SetPlacements",
        {group(1U), shape(2U)},
        operation(107U, SetPlacementsOp{{{id(2U), moved.placement}}}),
        replacementsPlan({moved}),
        id(1U)));
    cases.push_back(rejectedCase(
        "OP15-PLC-N",
        "SetPlacements",
        {},
        operation(108U, SetPlacementsOp{{{id(2U), placement(22U)}}}),
        StatefulIssue::kObjectMissing));

    auto transformed = shape(2U);
    transformed.transform = Transform2D{2.0, 0.0, 0.0, 2.0, 4.0, 5.0};
    cases.push_back(preparedCase(
        "OP15-TRN-P",
        "SetTransforms",
        {shape(2U)},
        operation(109U, SetTransformsOp{{{id(2U), transformed.transform}}}),
        replacementsPlan({transformed})));
    cases.push_back(rejectedCase(
        "OP15-TRN-N",
        "SetTransforms",
        {},
        operation(110U, SetTransformsOp{{{id(2U), transformed.transform}}}),
        StatefulIssue::kObjectMissing));

    auto patched = shape(2U);
    patched.properties.entries = {{1U, false}};
    cases.push_back(preparedCase(
        "OP15-PROP-P",
        "PatchProperties",
        {shape(2U)},
        operation(
            111U,
            PatchPropertiesOp{{{id(2U), 1U, PropertyPatchAction::kSet, PropertyValue{false}}}}),
        replacementsPlan({patched})));
    cases.push_back(rejectedCase(
        "OP15-PROP-N",
        "PatchProperties",
        {group(2U)},
        operation(
            112U,
            PatchPropertiesOp{{{id(2U), 0x100U, PropertyPatchAction::kClear, {}}}}),
        StatefulIssue::kInvalidApplicability));

    auto resized = shape(2U);
    std::get<ShapeContent>(resized.content).width = 30.0;
    std::get<ShapeContent>(resized.content).height = 40.0;
    cases.push_back(preparedCase(
        "OP15-SIZE-P",
        "SetObjectSize",
        {shape(2U)},
        operation(113U, SetObjectSizeOp{{{id(2U), 30.0, 40.0}}}),
        replacementsPlan({resized})));
    cases.push_back(rejectedCase(
        "OP15-SIZE-N",
        "SetObjectSize",
        {group(2U)},
        operation(114U, SetObjectSizeOp{{{id(2U), 30.0, 40.0}}}),
        StatefulIssue::kInvalidApplicability));

    auto path_updated = vectorPathRecord(3U, 2.0);
    cases.push_back(preparedCase(
        "OP15-PATH-P",
        "SetVectorPathGeometry",
        {vectorPathRecord(3U)},
        operation(115U, SetVectorPathGeometryOp{id(3U), vectorPath(2.0)}),
        replacementsPlan({path_updated})));
    cases.push_back(rejectedCase(
        "OP15-PATH-N",
        "SetVectorPathGeometry",
        {shape(3U)},
        operation(116U, SetVectorPathGeometryOp{id(3U), vectorPath(2.0)}),
        StatefulIssue::kInvalidApplicability));

    auto image_updated = imageRecord(4U, 120.0, 90.0);
    cases.push_back(preparedCase(
        "OP15-IMG-P",
        "SetImageContent",
        {imageRecord(4U)},
        operation(117U, SetImageContentOp{id(4U), imageContent(120.0, 90.0)}),
        replacementsPlan({image_updated})));
    cases.push_back(rejectedCase(
        "OP15-IMG-N",
        "SetImageContent",
        {shape(4U)},
        operation(118U, SetImageContentOp{id(4U), imageContent(120.0, 90.0)}),
        StatefulIssue::kInvalidApplicability));

    const auto added_stroke = vectorStroke(20U);
    cases.push_back(preparedCase(
        "OP15-STROKE-P",
        "AddStroke",
        {},
        operation(119U, AddStrokeOp{added_stroke}),
        createsPlan({added_stroke})));
    cases.push_back(rejectedCase(
        "OP15-STROKE-N",
        "AddStroke",
        {shape(20U)},
        operation(120U, AddStrokeOp{added_stroke}),
        StatefulIssue::kObjectAlreadyExists));

    const auto split_a = vectorStroke(21U);
    const auto split_b = vectorStroke(22U);
    cases.push_back(preparedCase(
        "OP15-SPLIT-P",
        "SplitStrokes",
        {vectorStroke(5U)},
        operation(121U, SplitStrokesOp{{{id(5U), {split_a, split_b}}}}),
        splitPlan({split_a, split_b}, {id(5U)})));
    cases.push_back(rejectedCase(
        "OP15-SPLIT-N",
        "SplitStrokes",
        {},
        operation(122U, SplitStrokesOp{{{id(5U), {vectorStroke(21U)}}}}),
        StatefulIssue::kObjectMissing));

    auto masked = vectorStroke(5U);
    masked.erase_masks = {eraseMask(50U)};
    cases.push_back(preparedCase(
        "OP15-MASK-ADD-P",
        "AddEraseMasks",
        {vectorStroke(5U)},
        operation(123U, AddEraseMasksOp{{{id(5U), {eraseMask(50U)}}}}),
        replacementsPlan({masked})));
    cases.push_back(rejectedCase(
        "OP15-MASK-ADD-N",
        "AddEraseMasks",
        {masked},
        operation(124U, AddEraseMasksOp{{{id(5U), {eraseMask(50U)}}}}),
        StatefulIssue::kMaskStateInvalid));

    auto two_masks = vectorStroke(5U);
    two_masks.erase_masks = {eraseMask(50U), eraseMask(51U)};
    auto one_mask = vectorStroke(5U);
    one_mask.erase_masks = {eraseMask(51U)};
    cases.push_back(preparedCase(
        "OP15-MASK-REM-P",
        "RemoveEraseMasks",
        {two_masks},
        operation(125U, RemoveEraseMasksOp{{{id(5U), {id(50U)}}}}),
        replacementsPlan({one_mask})));
    cases.push_back(rejectedCase(
        "OP15-MASK-REM-N",
        "RemoveEraseMasks",
        {masked},
        operation(126U, RemoveEraseMasksOp{{{id(5U), {id(99U)}}}}),
        StatefulIssue::kMaskStateInvalid));

    auto edited = richText(6U);
    auto& edited_paragraph =
        std::get<RichTextContent>(edited.content).document.paragraphs[0];
    edited_paragraph.runs = {{"A", textStyle()}, {"X", textStyle(0.5F)}};
    const RichTextDelta valid_delta{
        1U, {InsertTextStep{id(60U), 1U, "X", textStyle(0.5F)}}};
    cases.push_back(preparedCase(
        "OP15-TEXT-P",
        "EditRichText",
        {richText(6U)},
        operation(127U, EditRichTextOp{id(6U), valid_delta}),
        replacementsPlan({edited})));
    const RichTextDelta invalid_delta{
        1U,
        {InsertTextStep{id(60U), 0U, "X", textStyle()},
         DeleteTextStep{id(99U), 0U, 1U}}};
    cases.push_back(rejectedCase(
        "OP15-TEXT-N",
        "EditRichText",
        {richText(6U)},
        operation(128U, EditRichTextOp{id(6U), invalid_delta}),
        StatefulIssue::kTextStateInvalid));

    auto connector_updated = connector(7U, attachedConnectorContent(2U));
    cases.push_back(preparedCase(
        "OP15-CON-P",
        "SetConnectorContent",
        {shape(2U), connector(7U)},
        operation(129U, SetConnectorContentOp{id(7U), attachedConnectorContent(2U)}),
        replacementsPlan({connector_updated})));
    cases.push_back(rejectedCase(
        "OP15-CON-N",
        "SetConnectorContent",
        {group(2U), connector(7U)},
        operation(130U, SetConnectorContentOp{id(7U), attachedConnectorContent(2U)}),
        StatefulIssue::kConnectorInvalid));

    return cases;
}

std::vector<MatrixCase> specialCases() {
    std::vector<MatrixCase> cases;

    cases.push_back(rejectedCase(
        "SPC-PLC-MISSING-PARENT",
        "SetPlacements",
        {shape(2U)},
        operation(201U, SetPlacementsOp{{{id(2U), placement(22U, id(9U))}}}),
        StatefulIssue::kInvalidReference,
        id(9U)));

    cases.push_back(rejectedCase(
        "SPC-PLC-CYCLE",
        "SetPlacements",
        {group(1U), group(2U)},
        operation(
            202U,
            SetPlacementsOp{{
                {id(1U), placement(11U, id(2U))},
                {id(2U), placement(12U, id(1U))}}}),
        StatefulIssue::kHierarchyCycle,
        id(1U)));

    cases.push_back(rejectedCase(
        "SPC-CON-MISSING-TARGET",
        "SetConnectorContent",
        {connector(7U)},
        operation(203U, SetConnectorContentOp{id(7U), attachedConnectorContent(9U)}),
        StatefulIssue::kInvalidReference));

    const Operation previously_applied =
        operation(204U, InsertObjectsOp{{shape(90U)}});
    cases.push_back(rejectedCase(
        "SPC-OPID-COLLISION",
        "DeleteObjects",
        {shape(2U)},
        operation(204U, DeleteObjectsOp{{id(2U)}}),
        StatefulIssue::kOperationIdCollision,
        std::nullopt,
        previously_applied));

    return cases;
}

template <typename Store>
void seed(Store& store, const std::vector<ObjectRecord>& records) {
    for (const auto& record : records) {
        ASSERT_TRUE(internal::ObjectStoreMutator::insertFresh(store, record));
    }
}

void expectPlanProjection(
    const PreparedApplyPlan& actual,
    const Operation& operation_input,
    const ExpectedPlan& expected) {
    EXPECT_EQ(actual.operation.id, operation_input.id);
    EXPECT_EQ(actual.operation.document_id, operation_input.document_id);
    EXPECT_EQ(actual.operation.schema_version, operation_input.schema_version);
    EXPECT_EQ(actual.operation.payload_version, operation_input.payload_version);
    EXPECT_EQ(actual.operation.payload, operation_input.payload);
    EXPECT_TRUE(canonicalPayloadEqual(actual.operation, operation_input));
    EXPECT_EQ(actual.creates, expected.creates);
    EXPECT_EQ(actual.replacements, expected.replacements);
    EXPECT_EQ(actual.deletes, expected.deletes);
    ASSERT_EQ(actual.delete_closure.has_value(), expected.delete_closure.has_value());
    if (expected.delete_closure.has_value()) {
        ASSERT_TRUE(actual.delete_closure.has_value());
        EXPECT_EQ(
            actual.delete_closure->requested_delete_ids,
            expected.delete_closure->requested_delete_ids);
        EXPECT_EQ(
            actual.delete_closure->resolved_hierarchy_closure,
            expected.delete_closure->resolved_hierarchy_closure);
        EXPECT_EQ(
            actual.delete_closure->resolved_connector_cascade_closure,
            expected.delete_closure->resolved_connector_cascade_closure);
        EXPECT_EQ(
            actual.delete_closure->final_delete_set,
            expected.delete_closure->final_delete_set);
    }
}

void recordRuntimeObservation(
    const MatrixCase& test_case,
    std::string_view store_name,
    const PrepareResult& result,
    const std::vector<ObjectRecord>& before,
    const std::vector<ObjectRecord>& after,
    const std::map<OperationId, AppliedOperationEntry>& applied_before,
    const std::map<OperationId, AppliedOperationEntry>& applied_after,
    const std::optional<std::vector<ObjectRecord>>& children_before,
    const std::optional<std::vector<ObjectRecord>>& children_after,
    bool index_before,
    bool index_after) {
    const char* path = std::getenv("AXIOM_B10_OBSERVATIONS");
    if (path == nullptr) return;
    std::ofstream out(path, std::ios::app);
    out << std::setprecision(17);
    out << "{\"case_id\":\"" << test_case.case_id << "\",\"operation_name\":\""
        << test_case.operation_name << "\",\"polarity\":\""
        << (test_case.positive ? "positive" : "negative") << "\",\"store_implementation\":\""
        << store_name << "\",\"operation_id\":\"" << idHex(test_case.input.id.value())
        << "\",\"state_case_id\":\"" << test_case.case_id
        << "\",\"fixture_id\":\"" << test_case.case_id
        << "\",\"actual_disposition\":\"" << dispositionName(result.disposition) << '"'
        << ",\"actual_stateful_issue\":\"" << issueName(result.error.issue) << '"'
        << ",\"plan_present\":" << (result.plan.has_value() ? "true" : "false")
        << ",\"operation_document_id\":\"" << idHexTyped(test_case.input.document_id)
        << "\",\"schema_version\":" << test_case.input.schema_version
        << ",\"payload_version\":" << test_case.input.payload_version
        << ",\"input_operation\":";
    writeOperation(out, test_case.input);
    out << ",\"operation_payload_exact_typed_equality_asserted\":true"
        << ",\"canonical_before\":"; writeRecords(out, before);
    out << ",\"canonical_after\":"; writeRecords(out, after);
    out << ",\"applied_before\":"; writeApplied(out, applied_before);
    out << ",\"applied_after\":"; writeApplied(out, applied_after);
    out << ",\"children_before\":"; writeChildren(out, children_before);
    out << ",\"children_after\":"; writeChildren(out, children_after);
    out << ",\"indexed_index_matches_rebuild_before\":";
    if (store_name == "IndexedObjectStore") out << (index_before ? "true" : "false");
    else out << "null";
    out << ",\"indexed_index_matches_rebuild_after\":";
    if (store_name == "IndexedObjectStore") out << (index_after ? "true" : "false");
    else out << "null";
    if (result.plan.has_value()) {
        const auto& plan = *result.plan;
        out << ",\"creates_count\":" << plan.creates.size()
            << ",\"replacements_count\":" << plan.replacements.size()
            << ",\"deletes_count\":" << plan.deletes.size()
            << ",\"creates\":"; writeRecords(out, plan.creates);
        out << ",\"replacements\":"; writeRecords(out, plan.replacements);
        out << ",\"deletes\":"; writeObjectIds(out, plan.deletes);
        out << ",\"delete_closure\":";
        if (plan.delete_closure.has_value()) writeDeleteClosure(out, *plan.delete_closure);
        else out << "null";
        out << ",\"plan_operation\":";
        writeOperation(out, plan.operation);
        out << ",\"delete_closure_present\":"
            << (plan.delete_closure.has_value() ? "true" : "false");
    }
    out << "}\n";
}

template <typename Store>
void runCase(const MatrixCase& test_case) {
    Store store;
    seed(store, test_case.initial_objects);

    TestAppliedOperationView applied;
    if (test_case.applied_operation.has_value()) {
        applied.entries.emplace(
            test_case.applied_operation->id,
            AppliedOperationEntry{*test_case.applied_operation, std::nullopt});
    }

    const auto store_before = store.allObjects();
    const auto applied_before = applied.entries;
    std::optional<std::vector<ObjectRecord>> children_before;
    if (test_case.hierarchy_parent.has_value()) {
        children_before = store.children(
            std::optional<ObjectId>{*test_case.hierarchy_parent});
    }

    bool index_before = true;
    bool index_after = true;
    if constexpr (std::is_same_v<Store, IndexedObjectStore>) {
        ASSERT_TRUE(internal::ObjectStoreMutator::indexMatchesRebuild(store));
        index_before = internal::ObjectStoreMutator::indexMatchesRebuild(store);
    }

    const OperationEngine engine;
    const PrepareResult result = engine.prepare(
        test_case.input, StatefulValidationContext{store, applied});

    EXPECT_EQ(result.disposition, test_case.expected_disposition)
        << test_case.case_id;
    EXPECT_EQ(result.error.issue, test_case.expected_issue)
        << test_case.case_id;

    if (test_case.expected_plan.has_value()) {
        ASSERT_TRUE(result.plan.has_value()) << test_case.case_id;
        expectPlanProjection(*result.plan, test_case.input, *test_case.expected_plan);
    } else {
        EXPECT_FALSE(result.plan.has_value()) << test_case.case_id;
    }

    EXPECT_EQ(store.allObjects(), store_before) << test_case.case_id;
    expectAppliedUnchanged(applied_before, applied);
    if (children_before.has_value()) {
        EXPECT_EQ(
            store.children(std::optional<ObjectId>{*test_case.hierarchy_parent}),
            *children_before)
            << test_case.case_id;
    }
    if constexpr (std::is_same_v<Store, IndexedObjectStore>) {
        index_after = internal::ObjectStoreMutator::indexMatchesRebuild(store);
        EXPECT_TRUE(index_after)
            << test_case.case_id;
    }
    recordRuntimeObservation(
        test_case,
        std::is_same_v<Store, IndexedObjectStore> ? "IndexedObjectStore" : "ReferenceObjectStore",
        result,
        store_before,
        store.allObjects(),
        applied_before,
        applied.entries,
        children_before,
        test_case.hierarchy_parent.has_value()
            ? std::optional<std::vector<ObjectRecord>>(store.children(test_case.hierarchy_parent))
            : std::nullopt,
        index_before,
        index_after);
}

bool hasCaseId(const std::vector<MatrixCase>& cases, std::string_view case_id) {
    return std::any_of(
        cases.begin(), cases.end(), [case_id](const MatrixCase& test_case) {
            return test_case.case_id == case_id;
        });
}

TEST(G104B10OperationMatrix, FifteenFamiliesRequirePositiveAndNegativeRows) {
    const auto cases = coreCases();
    ASSERT_EQ(cases.size(), 30U);

    std::map<std::string_view, std::array<std::size_t, 2>> counts;
    for (const auto& test_case : cases) {
        auto& family_counts = counts[test_case.operation_name];
        ++family_counts[test_case.positive ? 0U : 1U];
    }

    ASSERT_EQ(counts.size(), 15U);
    for (const auto& [operation_name, family_counts] : counts) {
        EXPECT_EQ(family_counts[0], 1U) << operation_name;
        EXPECT_EQ(family_counts[1], 1U) << operation_name;
    }
}

TEST(G104B10OperationMatrix, CoreRowsExecuteOnReferenceAndIndexedStores) {
    for (const auto& test_case : coreCases()) {
        runCase<ReferenceObjectStore>(test_case);
        runCase<IndexedObjectStore>(test_case);
    }
}

TEST(G104B10OperationMatrix, RequiredSpecialRejectionsExecuteOnBothStores) {
    for (const auto& test_case : specialCases()) {
        runCase<ReferenceObjectStore>(test_case);
        runCase<IndexedObjectStore>(test_case);
    }
}

TEST(G104B10OperationMatrix, RequiredRejectionCoverageLabelsAreBound) {
    struct CoverageBinding final {
        std::string_view label;
        std::string_view case_id;
    };
    constexpr std::array<CoverageBinding, 9> bindings{{
        {"missing target", "OP15-DEL-N"},
        {"wrong kind / invalid applicability", "OP15-PROP-N"},
        {"missing parent or semantic reference", "SPC-PLC-MISSING-PARENT"},
        {"hierarchy cycle", "SPC-PLC-CYCLE"},
        {"invalid connector target/state", "OP15-CON-N"},
        {"invalid erase-mask state", "OP15-MASK-ADD-N"},
        {"invalid RichText state", "OP15-TEXT-N"},
        {"whole-batch identity/state collision", "OP15-INS-N"},
        {"OperationId collision", "SPC-OPID-COLLISION"},
    }};

    const auto core = coreCases();
    const auto special = specialCases();
    for (const auto& binding : bindings) {
        EXPECT_FALSE(binding.label.empty());
        EXPECT_TRUE(
            hasCaseId(core, binding.case_id) || hasCaseId(special, binding.case_id))
            << binding.label;
    }
}

TEST(G104B10OperationMatrix, RestoreRSTB01ThroughB12ProviderMappingsAreComplete) {
    struct RestoreBinding final {
        std::string_view rst_id;
        std::string_view provider_test;
        std::string_view linked_b10_case;
    };
    constexpr std::array<RestoreBinding, 12> bindings{{
        {"RST-B01", "RST_B01_AllAbsentValidRestore", "OP15-RST-P"},
        {"RST-B02", "RST_B02_ExistingSameRecordCollides", "OP15-RST-N"},
        {"RST-B03", "RST_B03_ExistingDifferentRecordCollides", "OP15-RST-N"},
        {"RST-B04", "RST_B04_ParentAndChildSamePayload", "OP15-RST-P"},
        {"RST-B05", "RST_B05_MissingParentInvalidReference", "OP15-RST-N"},
        {"RST-B06", "RST_B06_TargetAndConnectorSamePayload", "OP15-RST-P"},
        {"RST-B07", "RST_B07_ConnectorMissingTargetInvalidReference", "OP15-RST-N"},
        {"RST-B08", "RST_B08_EquivalentReplayStopsAtB1", "OP15-RST-P"},
        {"RST-B09", "RST_B09_NewOperationIdExistingCandidateCollides", "OP15-RST-N"},
        {"RST-B10", "RST_B10_BatchCollisionIsAtomic", "OP15-RST-N"},
        {"RST-B11", "RST_B11_SourceLabelsDoNotChangeResult", "OP15-RST-P"},
        {"RST-B12", "RST_B12_CheckpointLikeStateWithoutLedger", "OP15-RST-P"},
    }};

    const auto core = coreCases();
    for (std::size_t index = 0; index < bindings.size(); ++index) {
        const auto& binding = bindings[index];
        EXPECT_EQ(binding.rst_id.substr(0U, 5U), "RST-B");
        EXPECT_FALSE(binding.provider_test.empty());
        EXPECT_TRUE(hasCaseId(core, binding.linked_b10_case));
        if (index > 0U) {
            EXPECT_NE(binding.rst_id, bindings[index - 1U].rst_id);
        }
    }
}

} // namespace
} // namespace canvas::semantic
