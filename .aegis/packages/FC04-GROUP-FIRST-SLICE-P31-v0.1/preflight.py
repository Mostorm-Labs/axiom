import hashlib
import json
import subprocess
import sys
from pathlib import Path

root = Path.cwd()
package = Path(__file__).resolve().parent
record = json.loads((package / "package.json").read_text())
locked = (package / "source-lock.json").read_bytes()
if hashlib.sha256(locked).hexdigest() != record["source_lock_sha256"]:
    sys.exit("FAIL package integrity")
remote = subprocess.check_output(["git", "remote", "get-url", "origin"], text=True).strip().lower().removesuffix(".git")
if remote != "https://github.com/mostorm-labs/axiom":
    sys.exit("FAIL repository identity")
if subprocess.call(["git", "merge-base", "--is-ancestor", record["source_revision"], "HEAD"]):
    sys.exit("FAIL ancestry")
for name, expected in json.loads(locked).items():
    if hashlib.sha256((root / name).read_bytes()).hexdigest() != expected:
        sys.exit("FAIL source binding: " + name)
if not record["p32_authorized"] or not record["continue_until_terminal_state"]:
    sys.exit("FAIL bounded authorization")
print("PASS identity, ancestry, source bindings, and bounded user authorization")
