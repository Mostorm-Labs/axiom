from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
QUEUE = ROOT / "debug_ui" / "include" / "canvas" / "debug_ui" / "debug_command_queue.hpp"


def test_generation_and_capacity_conditions_are_explicitly_parenthesized():
    source = QUEUE.read_text(encoding="utf-8")
    assert "if (command.requestId == 0U ||" in source
    assert "(command.expectedRuntimeGeneration != 0U &&" in source
    assert "(command.expectedDocumentGeneration != 0U &&" in source
    assert "if ((it->expectedRuntimeGeneration != 0U &&" in source
    assert "(it->expectedDocumentGeneration != 0U &&" in source
    assert "if ((capacity_ == 0U || pending_.size() >= capacity_) ||" in source


def test_queue_semantics_remain_generation_and_deadline_fenced():
    source = QUEUE.read_text(encoding="utf-8")
    assert "AxiomDebugCommandState::kStaleGeneration" in source
    assert "AxiomDebugCommandState::kExpired" in source
    assert "AxiomDebugCommandState::kQueueFull" in source
