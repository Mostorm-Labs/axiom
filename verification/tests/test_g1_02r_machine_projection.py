"""GT-G1-02R descriptor-level projection contract.

The parser below only understands the generic protobuf wire structures used by
``google.protobuf.FileDescriptorSet``.  It does not import the production
semantic runtime or use a generated Axiom DTO; it deliberately examines a
fresh descriptor emitted from the checked-in Proto sources.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
PROTO_ROOT = ROOT / "schema/axiom/v1/proto"
PROTOC = ROOT / ".deps/protobuf/bin/protoc"

TYPE_DOUBLE = 1
TYPE_FLOAT = 2
TYPE_UINT32 = 13
TYPE_ENUM = 14
TYPE_MESSAGE = 11
TYPE_FIXED64 = 6


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
            raise ValueError(f"unsupported descriptor wire type {wire_type}")
        if offset > len(data):
            raise ValueError("truncated descriptor")
        yield number, wire_type, value


def _strings(data: bytes, number: int) -> list[str]:
    return [value.decode("utf-8") for field, wire, value in _fields(data) if field == number and wire == 2]


def _varints(data: bytes, number: int) -> list[int]:
    return [value for field, wire, value in _fields(data) if field == number and wire == 0]


@dataclass(frozen=True)
class Field:
    name: str
    number: int
    type: int
    type_name: str
    oneof_index: int | None


@dataclass(frozen=True)
class Enum:
    name: str
    values: tuple[tuple[str, int], ...]


@dataclass
class Message:
    name: str
    fields: dict[str, Field] = field(default_factory=dict)
    enums: dict[str, Enum] = field(default_factory=dict)
    messages: dict[str, "Message"] = field(default_factory=dict)


def _parse_field(data: bytes) -> Field:
    names = _strings(data, 1)
    numbers = _varints(data, 3)
    types = _varints(data, 5)
    type_names = _strings(data, 6)
    oneofs = _varints(data, 9)
    if len(names) != 1 or len(numbers) != 1 or len(types) != 1:
        raise ValueError("invalid FieldDescriptorProto")
    return Field(names[0], numbers[0], types[0], type_names[0] if type_names else "", oneofs[0] if oneofs else None)


def _parse_enum(data: bytes) -> Enum:
    names = _strings(data, 1)
    values: list[tuple[str, int]] = []
    for field, wire, value in _fields(data):
        if field != 2 or wire != 2:
            continue
        value_names, value_numbers = _strings(value, 1), _varints(value, 2)
        if len(value_names) != 1 or len(value_numbers) != 1:
            raise ValueError("invalid EnumValueDescriptorProto")
        values.append((value_names[0], value_numbers[0]))
    if len(names) != 1:
        raise ValueError("invalid EnumDescriptorProto")
    return Enum(names[0], tuple(values))


def _parse_message(data: bytes) -> Message:
    names = _strings(data, 1)
    if len(names) != 1:
        raise ValueError("invalid DescriptorProto")
    message = Message(names[0])
    for number, wire, value in _fields(data):
        if wire != 2:
            continue
        if number == 2:
            item = _parse_field(value)
            message.fields[item.name] = item
        elif number == 3:
            nested = _parse_message(value)
            message.messages[nested.name] = nested
        elif number == 4:
            enum = _parse_enum(value)
            message.enums[enum.name] = enum
    return message


def fresh_messages() -> dict[str, Message]:
    if not PROTOC.is_file():
        raise RuntimeError(f"pinned protoc is unavailable: {PROTOC}")
    sources = sorted(path.relative_to(PROTO_ROOT).as_posix() for path in PROTO_ROOT.rglob("*.proto"))
    with tempfile.TemporaryDirectory() as temporary:
        descriptor = Path(temporary) / "descriptor.pb"
        subprocess.run(
            [str(PROTOC), f"--proto_path={PROTO_ROOT}", "--include_imports", f"--descriptor_set_out={descriptor}", *sources],
            cwd=PROTO_ROOT,
            check=True,
            capture_output=True,
            text=True,
        )
        result: dict[str, Message] = {}
        for number, wire, file_proto in _fields(descriptor.read_bytes()):
            if number != 1 or wire != 2:
                continue
            package = _strings(file_proto, 2)
            if package != ["auditoryworks.axiom.v1"]:
                continue
            for child_number, child_wire, message_data in _fields(file_proto):
                if child_number == 4 and child_wire == 2:
                    message = _parse_message(message_data)
                    result[message.name] = message
            for child_number, child_wire, enum_data in _fields(file_proto):
                if child_number == 5 and child_wire == 2:
                    enum = _parse_enum(enum_data)
                    result[enum.name] = Message(enum.name, enums={enum.name: enum})
        return result


def _field_contract(message: Message, name: str, number: int, field_type: int, type_name: str = "") -> str | None:
    actual = message.fields.get(name)
    if actual is None:
        return f"{message.name}.{name}: missing"
    expected = (number, field_type, type_name)
    observed = (actual.number, actual.type, actual.type_name)
    if observed != expected:
        return f"{message.name}.{name}: expected {expected}, observed {observed}"
    return None


class G1_02RMachineProjectionTest(unittest.TestCase):
    def test_image_content_mode_machine_projection_matches_release_identity(self) -> None:
        messages = fresh_messages()
        image_mode = messages.get("ImageContentMode")
        self.assertIsNotNone(image_mode, "IMG-05: ImageContentMode enum is missing")
        assert image_mode is not None
        self.assertEqual(
            image_mode.enums.get("ImageContentMode"),
            Enum(
                "ImageContentMode",
                (("IMAGE_CONTENT_MODE_INVALID", 0), ("IMAGE_CONTENT_MODE_FIT", 1),
                 ("IMAGE_CONTENT_MODE_FILL", 2), ("IMAGE_CONTENT_MODE_STRETCH", 3)),
            ),
            "IMG-05: ImageContentMode enum identity/values are not frozen",
        )

    def test_richtext_machine_projection_matches_current_reconciliation_authority(self) -> None:
        messages = fresh_messages()
        errors: list[str] = []

        alignment = messages.get("ParagraphAlignment")
        if alignment is None or alignment.enums.get("ParagraphAlignment") != Enum(
            "ParagraphAlignment", (("INVALID", 0), ("LEFT", 1), ("CENTER", 2), ("RIGHT", 3), ("JUSTIFY", 4)),
        ):
            errors.append("RT-D01: ParagraphAlignment enum identity/values are not frozen")
        paragraph_style = messages.get("ParagraphStyle", Message("ParagraphStyle"))
        for name, number, kind, type_name in (
            ("alignment", 1, TYPE_ENUM, ".auditoryworks.axiom.v1.ParagraphAlignment"),
            ("line_height", 2, TYPE_DOUBLE, ""),
            ("spacing_before", 3, TYPE_DOUBLE, ""),
            ("spacing_after", 4, TYPE_DOUBLE, ""),
        ):
            if failure := _field_contract(paragraph_style, name, number, kind, type_name):
                errors.append(f"RT-D01/RT-D02: {failure}")
        delete_step = messages.get("DeleteTextStep", Message("DeleteTextStep"))
        if failure := _field_contract(delete_step, "scalar_count", 3, TYPE_UINT32):
            errors.append(f"RT-D03: {failure}")
        inline_step = messages.get("SetInlineStyleStep", Message("SetInlineStyleStep"))
        for name, number, kind, type_name in (
            ("paragraph_id", 1, TYPE_MESSAGE, ".auditoryworks.axiom.v1.Id128"),
            ("start_scalar", 2, TYPE_UINT32, ""),
            ("scalar_count", 3, TYPE_UINT32, ""),
            ("style", 4, TYPE_MESSAGE, ".auditoryworks.axiom.v1.TextStyle"),
        ):
            if failure := _field_contract(inline_step, name, number, kind, type_name):
                errors.append(f"RT-D04: {failure}")
        rich_text_step = messages.get("RichTextStep", Message("RichTextStep"))
        for name, number, type_name in (
            ("insert_text", 1, ".auditoryworks.axiom.v1.InsertTextStep"),
            ("delete_text", 2, ".auditoryworks.axiom.v1.DeleteTextStep"),
            ("split_paragraph", 3, ".auditoryworks.axiom.v1.SplitParagraphStep"),
            ("merge_paragraph", 4, ".auditoryworks.axiom.v1.MergeParagraphStep"),
            ("set_inline_style", 5, ".auditoryworks.axiom.v1.SetInlineStyleStep"),
            ("set_paragraph_style", 6, ".auditoryworks.axiom.v1.SetParagraphStyleStep"),
        ):
            if failure := _field_contract(rich_text_step, name, number, TYPE_MESSAGE, type_name):
                errors.append(f"RT-D05: {failure}")
        delta = messages.get("RichTextDelta", Message("RichTextDelta"))
        for name, number, kind, type_name in (
            ("delta_version", 1, TYPE_UINT32, ""),
            ("steps", 2, TYPE_MESSAGE, ".auditoryworks.axiom.v1.RichTextStep"),
        ):
            if failure := _field_contract(delta, name, number, kind, type_name):
                errors.append(f"RT-D06: {failure}")
        self.assertFalse(errors, "\n".join(errors))

    def test_stroke_machine_projection_matches_current_reconciliation_authority(self) -> None:
        messages = fresh_messages()
        errors: list[str] = []
        curve_point = messages.get("CurvePoint01", Message("CurvePoint01"))
        for name, number in (("x", 1), ("y", 2)):
            if failure := _field_contract(curve_point, name, number, TYPE_FLOAT):
                errors.append(f"ST-D01: {failure}")
        curve = messages.get("PiecewiseLinearCurve01", Message("PiecewiseLinearCurve01"))
        if failure := _field_contract(curve, "points", 1, TYPE_MESSAGE, ".auditoryworks.axiom.v1.CurvePoint01"):
            errors.append(f"ST-D01: {failure}")
        pressure = messages.get("PressureMapping", Message("PressureMapping"))
        for name in ("size_curve", "opacity_curve"):
            if failure := _field_contract(pressure, name, 2 if name == "size_curve" else 3, TYPE_MESSAGE, ".auditoryworks.axiom.v1.PiecewiseLinearCurve01"):
                errors.append(f"ST-D01: {failure}")
        spacing = messages.get("SpacingSettings", Message("SpacingSettings"))
        if failure := _field_contract(spacing, "normalized_spacing", 1, TYPE_FLOAT):
            errors.append(f"ST-D02: {failure}")
        brush = messages.get("BrushDescriptor", Message("BrushDescriptor"))
        for name, number, kind, type_name in (
            ("spacing", 9, TYPE_MESSAGE, ".auditoryworks.axiom.v1.SpacingSettings"),
            ("texture_resource_id", 10, TYPE_MESSAGE, ".auditoryworks.axiom.v1.Id128"),
            ("blend_mode", 11, TYPE_ENUM, ".auditoryworks.axiom.v1.BrushBlendMode"),
        ):
            if failure := _field_contract(brush, name, number, kind, type_name):
                errors.append(f"ST-D02/ST-D03: {failure}")
        dab = messages.get("DabInstance", Message("DabInstance"))
        if failure := _field_contract(dab, "center", 1, TYPE_MESSAGE, ".auditoryworks.axiom.v1.Vec2"):
            errors.append(f"ST-D05: {failure}")
        dab_stroke = messages.get("DabStrokeData", Message("DabStrokeData"))
        if failure := _field_contract(dab_stroke, "dabs", 1, TYPE_MESSAGE, ".auditoryworks.axiom.v1.DabInstance"):
            errors.append(f"ST-D05: {failure}")
        stroke = messages.get("StrokeRecord", Message("StrokeRecord"))
        if failure := _field_contract(stroke, "deterministic_seed", 2, TYPE_FIXED64):
            errors.append(f"ST-D04: {failure}")
        self.assertFalse(errors, "\n".join(errors))


if __name__ == "__main__":
    unittest.main()
