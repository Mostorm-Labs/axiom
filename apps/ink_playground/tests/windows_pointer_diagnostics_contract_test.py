from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
WINDOWS = ROOT / "apps/ink_playground/platform/windows"


def test_contact_updates_are_not_silently_capped_before_second_finger():
    source = (WINDOWS / "main.cpp").read_text(encoding="utf-8")
    assert "pointerDiagnosticMoves++ < 100U" not in source


def test_owner_and_overlay_record_input_origin_before_routing():
    owner = (WINDOWS / "main.cpp").read_text(encoding="utf-8")
    overlay = (WINDOWS / "windows_d3d12_skia_surface_provider.cpp").read_text(encoding="utf-8")
    assert 'logInputMessage(value->pointerDiagnostic, "owner"' in owner
    assert 'logInputMessage(*diagnostic, "overlay"' in overlay


def test_diagnostics_distinguish_promoted_mouse_and_native_pointer_targets():
    source = (WINDOWS / "windows_input_diagnostics.hpp").read_text(encoding="utf-8")
    assert "GetMessageExtraInfo()" in source
    assert "isPromotedPointerMouseMessage" in source
    assert "GetCurrentInputMessageSource" in source
    assert "info.hwndTarget" in source
    assert "GetPointerFrameTouchInfo" in source


def test_second_down_records_existing_contact_dispositions_and_commits():
    source = (WINDOWS / "main.cpp").read_text(encoding="utf-8")
    assert '"active-contact id="' in source
    assert "pointerDisposition(contactKey)" in source
    assert '" canonical_strokes="' in source
    assert "submittedOperationCount()" in source
