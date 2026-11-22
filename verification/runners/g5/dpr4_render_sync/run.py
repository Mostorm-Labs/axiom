"""Record bounded task commands without changing historical artifacts."""
import json
import os
from pathlib import Path
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[4]
OUT = ROOT / "out/G5-DPR4-render-sync-r1"


def run(label, command, timeout=600, env=None):
    OUT.mkdir(parents=True, exist_ok=True)
    started = time.time()
    record = {"command": command, "cwd": str(ROOT), "started": started}
    with (OUT / (label + ".log")).open("wb") as log:
        try:
            process = subprocess.run(command, cwd=ROOT, stdout=log,
                                     stderr=subprocess.STDOUT, timeout=timeout,
                                     env=dict(os.environ, **(env or {})))
            record["exitCode"] = process.returncode
        except subprocess.TimeoutExpired:
            record.update(exitCode=124, timedOut=True)
    record["seconds"] = time.time() - started
    (OUT / (label + ".json")).write_text(json.dumps(record, indent=2) + "\n")
    print(json.dumps({"label": label, **record}), flush=True)
    return record["exitCode"]


if __name__ == "__main__":
    sys.exit(run(sys.argv[1], sys.argv[2:]))
