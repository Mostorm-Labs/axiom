"""Verified release-file transfer shared by indexes and package archives."""
from __future__ import annotations

from dataclasses import dataclass
import json
import os
from pathlib import Path
import shutil
import tempfile
import urllib.error
import urllib.parse
import urllib.request

from .archive import verify_sha256
from .model import ArtifactRef, OfflineError, ReleaseIndexRef, SdkError, SourceKind

ReleaseRef = ArtifactRef | ReleaseIndexRef
MAX_DOWNLOAD_BYTES = 2 * 1024**3


@dataclass(frozen=True)
class ReleaseFile:
    path: Path
    source: SourceKind
    network_used: bool


class SafeRedirectHandler(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        old = urllib.parse.urlsplit(req.full_url)
        new = urllib.parse.urlsplit(newurl)
        if new.scheme not in {"https", "http"} or (old.scheme == "https" and new.scheme != "https"):
            raise SdkError("SDK transfer refused an unsafe redirect")
        redirected = super().redirect_request(req, fp, code, msg, headers, newurl)
        if redirected is not None and (old.scheme, old.netloc) != (new.scheme, new.netloc):
            redirected.remove_header("Authorization")
            redirected.remove_header("Cookie")
        return redirected


def _open(request: urllib.request.Request):
    return urllib.request.build_opener(SafeRedirectHandler()).open(request, timeout=30)


def release_url(ref: ReleaseRef) -> str:
    tag = urllib.parse.quote(ref.release_tag, safe="")
    asset = urllib.parse.quote(ref.asset, safe="")
    return f"https://github.com/{ref.repository}/releases/download/{tag}/{asset}"


def _download(request: urllib.request.Request, destination: Path) -> None:
    total = 0
    with _open(request) as source, destination.open("wb") as target:
        while chunk := source.read(1024 * 1024):
            total += len(chunk)
            if total > MAX_DOWNLOAD_BYTES:
                raise SdkError("release file exceeds download limit")
            target.write(chunk)
        target.flush()
        os.fsync(target.fileno())


def _github_download(ref: ReleaseRef, destination: Path) -> None:
    token = os.environ.get("GH_TOKEN") or os.environ.get("GITHUB_TOKEN")
    if not token:
        _download(urllib.request.Request(release_url(ref)), destination)
        return
    # Browser download URLs are not an authenticated private-asset API.
    api = f"https://api.github.com/repos/{ref.repository}/releases"
    headers = {"Authorization": f"Bearer {token}", "Accept": "application/vnd.github+json"}
    url = api + "/tags/" + urllib.parse.quote(ref.release_tag, safe="")
    with _open(urllib.request.Request(url, headers=headers)) as source:
        raw = source.read(5 * 1024 * 1024 + 1)
    if len(raw) > 5 * 1024 * 1024:
        raise SdkError("GitHub release metadata exceeds limit")
    metadata = json.loads(raw)
    matches = [asset for asset in metadata.get("assets", []) if asset.get("name") == ref.asset]
    if len(matches) != 1 or type(matches[0].get("id")) is not int or matches[0]["id"] <= 0:
        raise SdkError(f"locked GitHub release asset is missing or ambiguous: {ref.asset}")
    headers["Accept"] = "application/octet-stream"
    _download(urllib.request.Request(f"{api}/assets/{matches[0]['id']}", headers=headers), destination)


def _mirror_download(ref: ReleaseRef, mirror: str, destination: Path) -> bool:
    """Return whether network was used; FileNotFoundError means a clean miss."""
    parsed = urllib.parse.urlsplit(mirror)
    if parsed.scheme in {"https", "http"}:
        if parsed.username or parsed.password or parsed.query or parsed.fragment:
            raise SdkError("SDK mirror must be a credential-free base URL")
        suffix = "/" + urllib.parse.quote(ref.release_tag, safe="") + "/" + urllib.parse.quote(ref.asset, safe="")
        try:
            _download(urllib.request.Request(mirror.rstrip("/") + suffix), destination)
        except urllib.error.HTTPError as error:
            if error.code == 404:
                raise FileNotFoundError(ref.asset) from error
            raise
        return True
    if parsed.scheme == "file":
        if parsed.netloc not in {"", "localhost"}:
            raise SdkError("use a mounted filesystem path for a remote file mirror")
        root = Path(urllib.request.url2pathname(parsed.path))
    elif parsed.scheme and not (len(parsed.scheme) == 1 and len(mirror) > 2 and mirror[1] == ":"):
        raise SdkError(f"unsupported SDK mirror scheme: {parsed.scheme}")
    else:
        root = Path(mirror).expanduser()
    shutil.copyfile(root.joinpath(*ref.release_tag.split("/"), ref.asset), destination)
    return False


def ensure_release_file(ref: ReleaseRef, destination: Path, *, mirror: str | None = None,
                        offline: bool = False) -> ReleaseFile:
    """Return verified bytes. A corrupt copy is an error, never a cache miss.

    The resolver serializes shared destinations with SdkStore.lock. Neither
    mirrors nor credentials can change the identity/digest being requested.
    """
    try:
        if destination.exists() or destination.is_symlink():
            verify_sha256(destination, ref.sha256)
            return ReleaseFile(destination, "store", False)
        if offline:
            raise OfflineError("required release file is not materialized")
        destination.parent.mkdir(parents=True, exist_ok=True)
        with tempfile.NamedTemporaryFile(dir=destination.parent, prefix=".sdk-transfer-", delete=False) as file:
            staging = Path(file.name)
        try:
            source: SourceKind = "github"
            network = True
            if mirror is not None:
                try:
                    network = _mirror_download(ref, mirror, staging)
                    source = "mirror"
                except FileNotFoundError:
                    _github_download(ref, staging)
            else:
                _github_download(ref, staging)
            verify_sha256(staging, ref.sha256)
            staging.replace(destination)
            return ReleaseFile(destination, source, network)
        finally:
            staging.unlink(missing_ok=True)
    except SdkError as error:
        raise type(error)(f"{ref.family}: {error}") from error
    except (OSError, ValueError, urllib.error.URLError) as error:
        raise SdkError(f"{ref.family}: release transfer failed for {ref.asset}: {error}") from error
