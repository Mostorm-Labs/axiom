"""Serial guarded ordinary/diagnostic observation, with owned-process cleanup."""
import hashlib
import json
import math
import os
from pathlib import Path
import signal
import shutil
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[4]
OUT = ROOT / "out/G5-DPR4-render-sync-r1"
RESULT = ROOT / ".aegis/results/G5-DPR4-render-sync-r1"


def family(pid):
    records = {}
    for row in subprocess.check_output(["ps", "-axo", "pid=,ppid=,rss="], text=True).splitlines():
        p, parent, rss = map(int, row.split())
        records[p] = (parent, rss)
    owned = {pid}
    while True:
        more = {p for p, (parent, _) in records.items() if parent in owned}
        if more <= owned:
            break
        owned |= more
    return owned, sum(records.get(p, (0, 0))[1] for p in owned) * 1024


def main(label, sdk, mode):
    destination = OUT / label
    destination.mkdir(parents=True, exist_ok=True)
    driver = ROOT / "verification/runners/g5/dpr4_render_sync/wax_dpr4.mjs"
    command = [shutil.which("node"), str(driver), sdk, str(destination), mode]
    started = time.time()
    ledger = {"label": label, "sdk": sdk, "mode": mode, "command": command,
              "driverSha256": hashlib.sha256(driver.read_bytes()).hexdigest(),
              "started": started, "guardSeconds": 300, "guardRssBytes": 3 * 1024**3}
    peak, known = 0, set()
    with (destination / "worker.log").open("wb") as log:
        child = subprocess.Popen(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT,
                                 start_new_session=True)
        ledger["pid"] = child.pid
        try:
            while child.poll() is None:
                owned, rss = family(child.pid)
                known |= owned
                peak = max(peak, rss)
                if time.time() - started > ledger["guardSeconds"] or rss > ledger["guardRssBytes"]:
                    ledger["guardTriggered"] = "time" if rss <= ledger["guardRssBytes"] else "rss"
                    os.killpg(child.pid, signal.SIGTERM)
                    break
                time.sleep(1)
            try:
                child.wait(timeout=8)
            except subprocess.TimeoutExpired:
                os.killpg(child.pid, signal.SIGKILL)
                child.wait()
        finally:
            for pid in known - {child.pid}:
                try:
                    os.kill(pid, signal.SIGTERM)
                except ProcessLookupError:
                    pass
    survivors = []
    for pid in known:
        try:
            os.kill(pid, 0)
            survivors.append(pid)
        except ProcessLookupError:
            pass
    ledger.update(exitCode=child.returncode, seconds=time.time() - started,
                  actualCeilSeconds=math.ceil(time.time() - started),
                  peakRssBytes=peak, ownedPids=sorted(known), cleanupSurvivors=survivors)
    RESULT.mkdir(parents=True, exist_ok=True)
    (RESULT / (label + "-ledger.json")).write_text(json.dumps(ledger, indent=2) + "\n")
    print(json.dumps(ledger), flush=True)
    return child.returncode


if __name__ == "__main__":
    sys.exit(main(*sys.argv[1:]))
