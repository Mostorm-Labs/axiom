# UR2b finite derived-path proof repair

Original publication 2a1ab6a is complete. UR2b r1 has independently verified structural sharing, 634 original decisions, 91 native+4 targeted tests, 80 pixel stages and 20 roundtrips. All three paired Up p95 values improved, but remain around200ms.

Control found a narrow frozen invalid-semantics failure, not the normal long-line performance root cause. Three-knot finite source with x=1e40/1e100/1e308 survives codec. Old exact kernel returns Invalid3; prepared query returns Miss1. Double finite checks are insufficient to replace old all-derived-SkPath finiteness validation before early-out. See bound witnesses, not prose alone.

Repair only that proof/admission/fallback in existing Render owners. Preserve performance gains and old artifacts. Mask incremental/Scene work remains UR2c. No commit or push in this task. Callback control on terminal.
