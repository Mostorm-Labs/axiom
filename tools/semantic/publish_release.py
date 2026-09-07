#!/usr/bin/env python3
"""Publish a complete dependency prerelease, never replace an existing asset."""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile

if __package__ in {None, ""}:
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.sdk.archive import canonical_bytes, file_sha256
from tools.sdk.model import IntegrityError, ReleaseIndexRef, SdkError
from tools.semantic.aggregate import INDEX_ASSET, verify_release_directory


def _run(arguments: list[str], *, optional: bool = False) -> str | None:
    result = subprocess.run(["gh", *arguments], capture_output=True, text=True)
    if result.returncode:
        if optional and "(HTTP 404)" in result.stderr:
            return None
        raise SdkError(f"semantic: GitHub command failed ({result.returncode}): {result.stderr.strip()}")
    return result.stdout


def _api(path: str, *, optional: bool = False) -> dict | None:
    text = _run(["api", path], optional=optional)
    if text is None:
        return None
    try:
        result = json.loads(text)
    except (ValueError, TypeError) as error:
        raise SdkError("semantic: malformed GitHub response") from error
    if not isinstance(result, dict):
        raise SdkError("semantic: malformed GitHub object")
    return result


def _release_metadata(repository: str, tag: str) -> dict | None:
    metadata = _api(f"repos/{repository}/releases/tags/{tag}", optional=True)
    if metadata is not None:
        return metadata
    # REST's tag lookup targets published releases. A pending draft tag must
    # instead be found in the authenticated release listing (all pages).
    text = _run(["api", f"repos/{repository}/releases?per_page=100", "--paginate", "--slurp"])
    try:
        pages = json.loads(text)
    except (ValueError, TypeError) as error:
        raise SdkError("semantic: malformed draft release listing") from error
    if not isinstance(pages, list) or any(not isinstance(page, list) for page in pages):
        raise SdkError("semantic: malformed paginated release listing")
    releases = [entry for page in pages for entry in page]
    if any(not isinstance(entry, dict) for entry in releases):
        raise SdkError("semantic: malformed release listing entry")
    matches = [entry for entry in releases if entry.get("tag_name") == tag]
    if len(matches) > 1:
        raise IntegrityError("semantic: ambiguous existing release tag")
    return matches[0] if matches else None


def _verify_remote(directory: Path, release: dict, repository: str, commit: str, metadata: dict) -> None:
    if (metadata.get("tag_name") != release["tag"] or metadata.get("target_commitish") != commit
            or metadata.get("prerelease") is not True or type(metadata.get("draft")) is not bool):
        raise IntegrityError("semantic: existing release tag, target commit or dependency-release state differs")
    entries = metadata.get("assets")
    if not isinstance(entries, list) or any(not isinstance(a, dict) or not isinstance(a.get("name"), str) for a in entries):
        raise IntegrityError("semantic: invalid remote asset metadata")
    if sorted(a["name"] for a in entries) != release["assets"]:
        raise IntegrityError("semantic: existing release asset set differs; assets are immutable")
    actual = _api(f"repos/{repository}/commits/{release['tag']}", optional=metadata["draft"])
    if actual is not None and actual.get("sha") != commit:
        raise IntegrityError("semantic: release Git tag does not resolve to the exact producer commit")
    with tempfile.TemporaryDirectory(prefix="semantic-release-verify-") as temporary:
        _run(["release", "download", release["tag"], "--repo", repository, "--dir", temporary])
        remote = Path(temporary)
        if {p.name for p in remote.iterdir()} != set(release["assets"]):
            raise IntegrityError("semantic: downloaded release asset set differs")
        for name in release["assets"]:
            path = remote / name
            if path.is_symlink() or not path.is_file() or file_sha256(path) != file_sha256(directory / name):
                raise IntegrityError(f"semantic: existing release asset is not byte-identical: {name}")


def publish_release(directory: Path, repository: str, target_commit: str, *, dry_run: bool = False) -> dict:
    if not re.fullmatch(r"[0-9a-f]{40}", target_commit):
        raise SdkError("semantic: publication requires an exact lowercase 40-character commit SHA")
    directory = directory.resolve(strict=True)
    release = verify_release_directory(directory)
    # Validate repository and all URL/identity components before using the CLI.
    ReleaseIndexRef("semantic", release["releaseSetId"], repository, release["tag"], INDEX_ASSET, release["indexSha256"])
    facts = {"tag": release["tag"], "releaseSetId": release["releaseSetId"],
             "targetCommit": target_commit, "assets": release["assets"]}
    if dry_run:
        return {**facts, "status": "dry-run"}
    metadata = _release_metadata(repository, release["tag"])
    existed = metadata is not None
    if not existed:
        # A pre-existing tag is not made safe merely by passing --target.
        tagged = _api(f"repos/{repository}/commits/{release['tag']}", optional=True)
        if tagged is not None and tagged.get("sha") != target_commit:
            raise IntegrityError("semantic: existing Git tag targets a different commit")
        notes = (f"Immutable Semantic dependency Release Set {release['releaseSetId']}. "
                 "Contains three host tools and nine target runtimes; not an Axiom application release.")
        _run(["release", "create", release["tag"], "--repo", repository, "--draft", "--prerelease",
              "--latest=false", "--target", target_commit, "--title", release["tag"], "--notes", notes,
              *[str(directory / name) for name in release["assets"]]])
        metadata = _release_metadata(repository, release["tag"])
    if metadata is None:
        raise IntegrityError("semantic: release metadata is missing after creation")
    _verify_remote(directory, release, repository, target_commit, metadata)
    if metadata["draft"]:
        _run(["release", "edit", release["tag"], "--repo", repository, "--draft=false", "--latest=false"])
        metadata = _release_metadata(repository, release["tag"])
        if metadata is None or metadata.get("draft") is not False:
            raise IntegrityError("semantic: release is still a draft after publication")
        _verify_remote(directory, release, repository, target_commit, metadata)
        return {**facts, "status": "published"}
    return {**facts, "status": "already-published" if existed else "published"}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--directory", type=Path, required=True)
    parser.add_argument("--repository", default="Mostorm-Labs/axiom")
    parser.add_argument("--target-commit", required=True)
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()
    print(canonical_bytes(publish_release(args.directory, args.repository, args.target_commit, dry_run=args.dry_run)).decode())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
