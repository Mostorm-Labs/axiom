#!/usr/bin/env python3
"""Review the GT-G1-02R descriptor refreeze without using generated DTOs.

This verifier understands only the generic protobuf descriptor wire format.  It
compares the accepted historical descriptor with the freshly frozen descriptor,
then requires every observed machine-identity change to be attributed to one of
the reconciliation authority's closed RT-D/ST-D defects.
"""

from __future__ import annotations

from dataclasses import asdict, dataclass
import argparse
import hashlib
import json
from pathlib import Path
from typing import Any


def _sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def _varint(data: bytes, offset: int) -> tuple[int, int]:
    value = 0
    shift = 0
    while offset < len(data):
        byte = data[offset]
        offset += 1
        value |= (byte & 0x7F) << shift
        if byte < 0x80:
            return value, offset
        shift += 7
        if shift > 63:
            break
    raise ValueError("invalid protobuf varint")


def _fields(data: bytes):
    offset = 0
    while offset < len(data):
        key, offset = _varint(data, offset)
        number, wire_type = key >> 3, key & 0x07
        if wire_type == 0:
            value, offset = _varint(data, offset)
        elif wire_type == 1:
            value, offset = data[offset:offset + 8], offset + 8
        elif wire_type == 2:
            size, offset = _varint(data, offset)
            value, offset = data[offset:offset + size], offset + size
        elif wire_type == 5:
            value, offset = data[offset:offset + 4], offset + 4
        else:
            raise ValueError(f"unsupported protobuf descriptor wire type {wire_type}")
        if offset > len(data):
            raise ValueError("truncated protobuf descriptor")
        yield number, wire_type, value


def _strings(data: bytes, number: int) -> list[str]:
    return [value.decode("utf-8") for field, wire, value in _fields(data) if field == number and wire == 2]


def _uints(data: bytes, number: int) -> list[int]:
    return [value for field, wire, value in _fields(data) if field == number and wire == 0]


@dataclass(frozen=True)
class FieldSignature:
    name: str
    number: int
    label: int
    type: int
    type_name: str
    oneof_index: int | None


@dataclass(frozen=True)
class EnumSignature:
    name: str
    values: tuple[tuple[str, int], ...]


@dataclass(frozen=True)
class MessageSignature:
    name: str
    fields: tuple[FieldSignature, ...]


@dataclass(frozen=True)
class FileSignature:
    name: str
    messages: tuple[MessageSignature, ...]
    enums: tuple[EnumSignature, ...]


def _parse_field(data: bytes) -> FieldSignature:
    names = _strings(data, 1)
    numbers = _uints(data, 3)
    labels = _uints(data, 4)
    types = _uints(data, 5)
    type_names = _strings(data, 6)
    oneofs = _uints(data, 9)
    if len(names) != 1 or len(numbers) != 1 or len(labels) != 1 or len(types) != 1:
        raise ValueError("invalid FieldDescriptorProto")
    return FieldSignature(
        name=names[0],
        number=numbers[0],
        label=labels[0],
        type=types[0],
        type_name=type_names[0] if type_names else "",
        oneof_index=oneofs[0] if oneofs else None,
    )


def _parse_message(data: bytes) -> MessageSignature:
    names = _strings(data, 1)
    if len(names) != 1:
        raise ValueError("invalid DescriptorProto")
    fields = [_parse_field(value) for number, wire, value in _fields(data) if number == 2 and wire == 2]
    return MessageSignature(names[0], tuple(sorted(fields, key=lambda field: field.number)))


def _parse_enum(data: bytes) -> EnumSignature:
    names = _strings(data, 1)
    if len(names) != 1:
        raise ValueError("invalid EnumDescriptorProto")
    values: list[tuple[str, int]] = []
    for number, wire, value in _fields(data):
        if number != 2 or wire != 2:
            continue
        value_names, numbers = _strings(value, 1), _uints(value, 2)
        if len(value_names) != 1 or len(numbers) != 1:
            raise ValueError("invalid EnumValueDescriptorProto")
        values.append((value_names[0], numbers[0]))
    return EnumSignature(names[0], tuple(values))


def parse_descriptor(data: bytes) -> dict[str, FileSignature]:
    files: dict[str, FileSignature] = {}
    for number, wire, raw_file in _fields(data):
        if number != 1 or wire != 2:
            continue
        names = _strings(raw_file, 1)
        if len(names) != 1:
            raise ValueError("invalid FileDescriptorProto")
        messages = [_parse_message(value) for field, kind, value in _fields(raw_file) if field == 4 and kind == 2]
        enums = [_parse_enum(value) for field, kind, value in _fields(raw_file) if field == 5 and kind == 2]
        files[names[0]] = FileSignature(names[0], tuple(sorted(messages, key=lambda item: item.name)), tuple(sorted(enums, key=lambda item: item.name)))
    return files


def _message_map(file: FileSignature) -> dict[str, MessageSignature]:
    return {message.name: message for message in file.messages}


def _enum_map(file: FileSignature) -> dict[str, EnumSignature]:
    return {enum.name: enum for enum in file.enums}


def _wire_type(field: FieldSignature | None) -> int | None:
    if field is None:
        return None
    if field.type in {1, 6, 16}:  # double, fixed64, sfixed64
        return 1
    if field.type in {2, 7, 15}:  # float, fixed32, sfixed32
        return 5
    if field.type in {9, 11, 12}:  # string, message, bytes
        return 2
    return 0


def _display(value: object) -> object:
    return None if value is None else asdict(value) if hasattr(value, "__dataclass_fields__") else value


def _mapping(file_name: str, kind: str, name: str, field: int | None) -> tuple[tuple[str, ...], str, str] | None:
    """Return defect IDs, classification and wire impact for a known change."""
    if file_name.endswith("rich_text.proto"):
        if kind == "enum" and name == "ParagraphAlignment":
            return (("RT-D01",), "DESCRIPTOR_IDENTITY_CORRECTION", "enum identity (wire 0 values)")
        if name == "ParagraphStyle":
            if field == 1:
                return (("RT-D01",), "DESCRIPTOR_IDENTITY_CORRECTION", "uint32 to enum; wire 0 retained")
            if field in {2, 3, 4}:
                return (("RT-D02",), "MISSING_FIELD_MATERIALIZATION", "double / fixed64 added")
        if name == "DeleteTextStep" and field == 3:
            return (("RT-D03",), "SEMANTIC_NAME_CORRECTION", "same varint wire; count replaces absolute end meaning")
        if name in {"SetTextStyleStep", "SetInlineStyleStep"}:
            return (("RT-D04",), "SEMANTIC_NAME_CORRECTION", "released inline-style identity and count semantics")
        if name == "RichTextStep" and field in {3, 4, 5}:
            return (("RT-D05",), "WIRE_BREAKING_PRE_RELEASE_REPAIR", "oneof branch tag identity corrected")
        if name == "RichTextDelta" and field in {1, 2}:
            return (("RT-D06",), "WIRE_BREAKING_PRE_RELEASE_REPAIR", "version and ordered-step field tags corrected")
    if file_name.endswith("object.proto"):
        if kind == "enum" and name == "ImageContentMode":
            return (("IMG-05",), "DESCRIPTOR_IDENTITY_CORRECTION", "enum identity values corrected to released order")
    if file_name.endswith("brush_stroke.proto"):
        if name in {"CurvePoint01", "PiecewiseLinearCurve01"}:
            return (("ST-D01",), "MISSING_FIELD_MATERIALIZATION", "curve message materialized")
        if name == "PressureMapping" and field in {2, 3}:
            return (("ST-D01",), "WIRE_BREAKING_PRE_RELEASE_REPAIR", "float carrier to nested curve message")
        if name == "SpacingSettings":
            return (("ST-D02",), "MISSING_FIELD_MATERIALIZATION", "spacing message materialized")
        if name == "BrushDescriptor" and field == 9:
            return (("ST-D02",), "WIRE_BREAKING_PRE_RELEASE_REPAIR", "double carrier to SpacingSettings message")
        if name == "BrushDescriptor" and field in {10, 11}:
            return (("ST-D03",), "WIRE_BREAKING_PRE_RELEASE_REPAIR", "texture and blend numeric tags corrected")
        if name == "StrokeRecord" and field == 2:
            return (("ST-D04",), "WIRE_BREAKING_PRE_RELEASE_REPAIR", "uint64 varint to fixed64")
        if name in {"Dab", "DabInstance", "DabStrokeData"}:
            return (("ST-D05",), "SEMANTIC_NAME_CORRECTION", "DabInstance center descriptor identity")
    return None


def _change(file_name: str, kind: str, name: str, field: int | None, before: object, after: object) -> dict[str, Any]:
    mapping = _mapping(file_name, kind, name, field)
    entry: dict[str, Any] = {
        "file": file_name,
        "kind": kind,
        "messageOrType": name,
        "fieldTag": field,
        "before": _display(before),
        "after": _display(after),
    }
    if mapping is None:
        entry.update({"defectIds": [], "classification": "UNMAPPED", "wireImpact": "requires authority classification"})
    else:
        defect_ids, classification, wire_impact = mapping
        entry.update({"defectIds": list(defect_ids), "classification": classification, "wireImpact": wire_impact})
    return entry


def _diff_file(before: FileSignature, after: FileSignature) -> list[dict[str, Any]]:
    changes: list[dict[str, Any]] = []
    before_messages, after_messages = _message_map(before), _message_map(after)
    for name in sorted(before_messages.keys() - after_messages.keys()):
        changes.append(_change(before.name, "message", name, None, before_messages[name], None))
    for name in sorted(after_messages.keys() - before_messages.keys()):
        changes.append(_change(before.name, "message", name, None, None, after_messages[name]))
    for name in sorted(before_messages.keys() & after_messages.keys()):
        old_fields = {field.number: field for field in before_messages[name].fields}
        new_fields = {field.number: field for field in after_messages[name].fields}
        for tag in sorted(old_fields.keys() | new_fields.keys()):
            old_field, new_field = old_fields.get(tag), new_fields.get(tag)
            if old_field != new_field:
                changes.append(_change(before.name, "field", name, tag, old_field, new_field))
    before_enums, after_enums = _enum_map(before), _enum_map(after)
    for name in sorted(before_enums.keys() | after_enums.keys()):
        if before_enums.get(name) != after_enums.get(name):
            changes.append(_change(before.name, "enum", name, None, before_enums.get(name), after_enums.get(name)))
    return changes


def _registry_signature(model: dict[str, FileSignature], name: str) -> tuple[FieldSignature, ...] | None:
    for file in model.values():
        message = _message_map(file).get(name)
        if message is not None:
            return message.fields
    return None


def compare(before_data: bytes, after_data: bytes, *, authority_baseline: str) -> dict[str, Any]:
    before, after = parse_descriptor(before_data), parse_descriptor(after_data)
    changes: list[dict[str, Any]] = []
    for file_name in sorted(before.keys() | after.keys()):
        old_file, new_file = before.get(file_name), after.get(file_name)
        if old_file is None or new_file is None:
            changes.append(_change(file_name, "file", file_name, None, old_file, new_file))
        else:
            changes.extend(_diff_file(old_file, new_file))
    unmapped = [change for change in changes if not change["defectIds"]]
    result = {
        "format": "axiom-gt-g1-02r-descriptor-refreeze-diff-v1",
        "authorityBaseline": authority_baseline,
        "beforeDescriptorSha256": _sha256(before_data),
        "afterDescriptorSha256": _sha256(after_data),
        "changes": changes,
        "unmappedChanges": unmapped,
        "outerRegistryPreserved": {
            name: _registry_signature(before, name) == _registry_signature(after, name)
            for name in ("ObjectContent", "Operation")
        },
    }
    # Keep the returned contract identical to the JSON review artifact: tuples
    # in descriptor signatures are not a second, Python-only output shape.
    return json.loads(json.dumps(result))


def write_review_artifact(before_data: bytes, after_data: bytes, output: Path, *, authority_baseline: str) -> dict[str, Any]:
    result = compare(before_data, after_data, authority_baseline=authority_baseline)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, ensure_ascii=False, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--before", type=Path, required=True)
    parser.add_argument("--after", type=Path, required=True)
    parser.add_argument("--authority-baseline", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = write_review_artifact(
        args.before.read_bytes(), args.after.read_bytes(), args.output, authority_baseline=args.authority_baseline
    )
    print(json.dumps(result, ensure_ascii=False, indent=2, sort_keys=True))
    return 0 if not result["unmappedChanges"] and all(result["outerRegistryPreserved"].values()) else 2


if __name__ == "__main__":
    raise SystemExit(main())
