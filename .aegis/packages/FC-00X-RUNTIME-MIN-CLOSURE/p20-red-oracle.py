"""Read-only P20 RED oracle for the runtime minimum closure.

It deliberately runs against the untouched source tree.  It is not an
implementation test and never edits production files.  `baseline-red` must
fail while both known boundary defects are present; `candidate-green` is the
post-implementation contract check used by a later accepted P32 package.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", required=True)
    parser.add_argument("--expect", choices=("baseline-red",), required=True)
    args = parser.parse_args()
    root = Path(args.root).resolve()
    binding = (root / "runtime/render/src/runtime_app_binding.cpp").read_text(encoding="utf-8")
    binding_h = (root / "runtime/render/include/canvas/render/runtime_app_binding.hpp").read_text(encoding="utf-8")
    control = (root / "apps/ink_playground/common/canvas_control_binding.cpp").read_text(encoding="utf-8")
    control_h = (root / "apps/ink_playground/common/canvas_control_binding.hpp").read_text(encoding="utf-8")

    s1_baseline_defect = (
        "(void)ingress_.sourceLost" in binding
        and "return ingress_.sourceLost" not in binding
        and "cancel" not in binding_h.lower()
        and "unregisterInputSource" in binding
        and "sourceLost" not in binding.split("bool RuntimeAppBinding::unregisterInputSource", 1)[1].split("}", 1)[0]
    )
    s3_baseline_defect = (
        "out.targetAvailable = true" in control
        and "out.writable = true" in control
        and "out.busy = false" in control
        and "targetAvailable" not in control_h
        and "busy" not in control_h
    )
    observations = {
        "FC00X-RED-S1-source-cancel-delivery": {
            "baseline_defect_observed": s1_baseline_defect,
            "details": "notifySourceLost discards ingress result and unregister has no source-scoped cancellation path",
        },
        "FC00X-RED-S3-control-readback-truth": {
            "baseline_defect_observed": s3_baseline_defect,
            "details": "snapshot unconditionally reports targetAvailable/writable/busy without owner state",
        },
    }
    all_defects = all(v["baseline_defect_observed"] for v in observations.values())
    payload = {"oracle": "FC-00X-P20-RED-v1", "expect": args.expect, "observations": observations}
    print(json.dumps(payload, sort_keys=True))
    return 1 if all_defects else 0


if __name__ == "__main__":
    raise SystemExit(main())
