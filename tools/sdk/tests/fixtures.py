"""Small real ZIP/index fixtures; never imported by production SDK code."""
from dataclasses import asdict
import hashlib
import json
from pathlib import Path

from tools.sdk.archive import canonical_bytes, create_deterministic_zip, file_sha256, safe_extract_zip
from tools.sdk.model import ArtifactRef, IntegrityError, ProviderPlan, ReleaseIndexRef


class FixtureProvider:
    def __init__(self, family, index):
        self.family = family
        self.index = index

    def index_ref(self, request):
        return self.index

    def plan(self, request, index_path):
        data = json.loads(index_path.read_bytes())
        if data["family"] != self.family:
            raise IntegrityError("fixture index family mismatch")
        ref = ArtifactRef(**data["artifact"])
        return ProviderPlan(self.family, (ref,), {"fixture": True})

    def install(self, ref, archive, staging_root):
        safe_extract_zip(archive, staging_root)

    def validate(self, ref, root):
        payload = root / "payload.txt"
        if (not payload.is_file() or payload.is_symlink()
                or file_sha256(payload) != ref.identity
                or {p.name for p in root.iterdir()} != {"payload.txt"}):
            raise IntegrityError("fixture payload mismatch")

    def environment(self, plan, materialized):
        return {self.family.upper().replace("-", "_") + "_ROOT": str(materialized[0].root)}


def make_provider(base: Path, mirror: Path, family="example-a"):
    root = base / (family + "-input")
    root.mkdir(parents=True)
    content = (family + " verified payload").encode()
    (root / "payload.txt").write_bytes(content)
    identity = hashlib.sha256(content).hexdigest()
    tag = "release-" + family
    release = mirror / tag
    release.mkdir(parents=True)
    archive = release / "runtime.zip"
    create_deterministic_zip(root, archive, modes={"payload.txt": 0o644})
    ref = ArtifactRef(family, "runtimes", "test-platform", identity,
                      "Mostorm-Labs/axiom", tag, archive.name, file_sha256(archive))
    index = release / "index.json"
    index.write_bytes(canonical_bytes({"family": family, "artifact": asdict(ref)}))
    index_id = hashlib.sha256((family + "-release").encode()).hexdigest()
    index_ref = ReleaseIndexRef(family, index_id, ref.repository, tag, index.name, file_sha256(index))
    return FixtureProvider(family, index_ref), ref
