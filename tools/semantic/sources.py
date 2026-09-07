"""Producer-only verified upstream sources. Consumers never import this module."""
from __future__ import annotations

from pathlib import Path
import os
import tarfile
import tempfile
import urllib.request

from tools.sdk.archive import verify_sha256
from tools.sdk.model import SdkError, validate_digest
from tools.sdk.transport import SafeRedirectHandler


def source_archive(name: str, lock: dict, cache: Path) -> Path:
    dependency = lock["dependencies"][name]
    digest = validate_digest(dependency["source_sha256"])
    target = cache / digest / (name + ".tar.gz")
    if target.exists():
        verify_sha256(target, digest)
        return target
    url = dependency["source_url"]
    if not url.startswith("https://"):
        raise SdkError("semantic: upstream source must use HTTPS")
    target.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=target.parent, delete=False) as file:
        temporary = Path(file.name)
    try:
        opener = urllib.request.build_opener(SafeRedirectHandler())
        with opener.open(url, timeout=60) as response, temporary.open("wb") as output:
            size = 0
            while chunk := response.read(1024 * 1024):
                size += len(chunk)
                if size > 512 * 1024**2:
                    raise SdkError("semantic: upstream source exceeds size limit")
                output.write(chunk)
            output.flush()
            os.fsync(output.fileno())
        verify_sha256(temporary, digest)
        temporary.replace(target)
        return target
    finally:
        temporary.unlink(missing_ok=True)


def source_license(archive: Path, member_name: str, output: Path) -> Path:
    """Read exactly one license from an already SHA-verified source archive."""
    with tarfile.open(archive, "r:gz") as source:
        members = [m for m in source.getmembers() if m.name == member_name]
        if len(members) != 1 or not members[0].isfile() or members[0].size > 1024**2:
            raise SdkError(f"semantic: missing or invalid license {member_name}")
        incoming = source.extractfile(members[0])
        if incoming is None:
            raise SdkError(f"semantic: cannot read license {member_name}")
        with incoming:
            content = incoming.read()
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(content)
    return output
