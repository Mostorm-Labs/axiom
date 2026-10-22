"""Verify this evidence bundle; optionally recheck the local shipped directory/ZIP.

This checks artifacts and recorded CLI facts, never physical IME/touch results.
"""
import argparse
import gzip
import hashlib
import json
from pathlib import Path
import zipfile


def load(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def require(condition, detail):
    if not condition:
        raise RuntimeError(detail)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--delivery", action="store_true", help="also inspect paths in delivery.json on the original device")
    args = parser.parse_args()
    bundle = Path(__file__).resolve().parent
    evidence_manifest = load(bundle / "sha256-manifest.json")
    for name, expected in evidence_manifest.items():
        require(Path(name).name == name, "unexpected evidence path")
        data = (bundle / name).read_bytes()
        require(len(data) == expected["bytes"] and sha(data) == expected["sha256"], name)
        if name.endswith(".gz"):
            gzip.decompress(data).decode("utf-8-sig")
    expected_exe = "bf917e023f35f0342e21d0dc5dab54d665bb079da12eba229b970bda8fdc53c8"
    for stage in ("original", "icu-only", "icu-cjk"):
        launch = load(bundle / f"{stage}-launch.json")
        require(launch["has_exited"] and launch["exit_code"] == 1, f"lost original FAIL: {stage}")
        require(launch["sha256"] == expected_exe, "changed failed executable")
    complete = load(bundle / "complete-launch.json")
    require(not complete["has_exited"] and complete["responding"], "missing successful startup observation")
    require(complete["sha256"] == expected_exe, "changed recovered executable")
    log = gzip.decompress((bundle / "complete-input.log.gz").read_bytes()).decode("utf-8-sig")
    require("build_revision=3d7afa351dfbd42f18c449bda35f861089b1fb4f dirty=0" in log, "wrong product")
    require("event=startup accepted=1" in log and "event=frame-opportunity accepted=1" in log, "missing startup/frame")
    raw_error = gzip.decompress((bundle / "original-stderr.log.gz").read_bytes()).decode("utf-8-sig")
    require("SkIcuLoader: datafile missing" in raw_error, "lost ICU diagnostic")
    result = load(bundle / "result.json")
    require(result["original_result"] == "FAIL" and not result["product_code_changed"] and not result["product_rebuilt"], "invalid scope/result")
    require(not result["fc02_completion_claimed"] and not result["independent_review_claimed"], "unsupported completion")
    matrix = load(bundle / "manual-matrix.json")
    require([row["id"] for row in matrix["checks"]] == [f"W{i:02d}" for i in range(1, 11)], "manual ordering changed")
    for row in matrix["checks"]:
        require(row["result"] in ("PASS", "FAIL", "PHYSICAL_PENDING"), "unknown manual outcome")
        require(bool(row["actual"]) and bool(row["expected"]), "missing expected/actual")
        require(result["manual_results"][row["id"]] == row["result"], "stale manual summary")
    delivery_checks = None
    if args.delivery:
        delivery = load(bundle / "delivery.json")
        directory = Path(delivery["directory"])
        archive = Path(delivery["archive"])
        require(sha(archive.read_bytes()) == delivery["archive_sha256"], "delivery ZIP changed")
        require(archive.stat().st_size == delivery["archive_bytes"], "delivery ZIP length changed")
        manifest = load(bundle / "distribution-manifest.json")
        with zipfile.ZipFile(archive) as package:
            for item in manifest["files"]:
                relative = item["path"]
                require(".." not in Path(relative).parts and not Path(relative).is_absolute(), "unsafe package path")
                data = (directory / relative).read_bytes()
                require(len(data) == item["bytes"] and sha(data) == item["sha256"], relative)
                require(package.read(directory.name + "/" + relative) == data, "ZIP/directory mismatch: " + relative)
            require(load(directory / "distribution-manifest.json") == manifest, "package manifest changed")
            require(all(not name.endswith("imgui.ini") for name in package.namelist()), "user ini packaged")
        font_catalog = load(directory / "fonts/catalog.json")
        for font in font_catalog["fonts"]:
            data = (directory / "fonts" / font["file"]).read_bytes()
            require(sha(data) == font["sha256"] and len(data) == font["bytes"], "font catalog mismatch")
            require((directory / "fonts" / font["license_file"]).is_file(), "missing font license")
        delivery_checks = {"package_files": len(manifest["files"]), "imported_fonts": len(font_catalog["fonts"])}
    print(json.dumps({"evidence_files_verified": len(evidence_manifest), "cli_controls": "3 FAIL / 1 startup PASS",
                      "delivery": delivery_checks, "physical_results_inferred": False}, indent=2))


if __name__ == "__main__":
    main()
