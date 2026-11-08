"""Read-only integrity check for this isolated source checkpoint."""
from pathlib import Path
import hashlib
import json


def main():
    root = Path(__file__).resolve().parent
    manifest = json.loads((root / "manifest.json").read_text())
    failures = []
    for name, expected in manifest["files"].items():
        path = root / name
        if not path.is_file():
            failures.append(name + ": missing")
            continue
        data = path.read_bytes()
        if len(data) != expected["bytes"] or hashlib.sha256(data).hexdigest() != expected["sha256"]:
            failures.append(name + ": digest/size mismatch")
    if failures:
        raise SystemExit("\n".join(failures))
    print(f"Verified {len(manifest['files'])} archived payloads; status=SHELVED_NOT_ACCEPTED; no active source changed.")


if __name__ == "__main__":
    main()
