"""Persistent family-namespaced Store; it is never dependency authority."""
from __future__ import annotations

from contextlib import contextmanager
import errno
import hashlib
import os
from pathlib import Path
import platform
import shutil
import tempfile
import time
from typing import Callable, Mapping
import uuid

from .archive import verify_sha256
from .model import ArtifactRef, IntegrityError, ReleaseIndexRef, SdkError


def default_store_root(system: str | None = None, env: Mapping[str, str] | None = None,
                       home: Path | None = None) -> Path:
    system = platform.system() if system is None else system
    env = os.environ if env is None else env
    home = Path.home() if home is None else home
    if env.get("AXIOM_SDK_STORE"):
        return Path(env["AXIOM_SDK_STORE"]).expanduser().resolve()
    if system == "Windows":
        if not env.get("LOCALAPPDATA"):
            raise SdkError("LOCALAPPDATA or AXIOM_SDK_STORE is required on Windows")
        return Path(env["LOCALAPPDATA"]) / "Axiom/sdk"
    if system == "Darwin":
        return home / "Library/Application Support/Axiom/sdk"
    if system == "Linux":
        base = Path(env["XDG_DATA_HOME"]) if env.get("XDG_DATA_HOME") else home / ".local/share"
        if not base.is_absolute():
            raise SdkError("XDG_DATA_HOME must be absolute")
        return base / "axiom/sdk"
    raise SdkError(f"unsupported Store platform: {system}")


class SdkStore:
    def __init__(self, root: Path):
        self.root = root.expanduser().resolve()

    def _path(self, *parts: str) -> Path:
        path = self.root
        for part in parts:
            path = path / part
            if path.is_symlink() or (hasattr(path, "is_junction") and path.is_junction()):
                raise SdkError(f"Store namespace contains a link: {path}")
        return path

    def archive_path(self, ref: ArtifactRef | ReleaseIndexRef) -> Path:
        return self._path("archives", "sha256", ref.sha256, ref.asset)

    def package_path(self, ref: ArtifactRef) -> Path:
        return self._path("packages", ref.family, ref.kind, ref.identity)

    def release_index_path(self, ref: ReleaseIndexRef) -> Path:
        return self._path("release-sets", ref.family, ref.identity, ref.asset)

    @contextmanager
    def lock(self, key: str, *, timeout: float = 30.0):
        """Serialize Store operations across processes; never delete lock files."""
        digest = hashlib.sha256(key.encode("utf-8")).hexdigest()
        path = self._path("locks", digest + ".lock")
        path.parent.mkdir(parents=True, exist_ok=True)
        descriptor = os.open(path, os.O_CREAT | os.O_RDWR, 0o600)
        acquired = False
        try:
            if os.fstat(descriptor).st_size == 0:
                os.write(descriptor, b"0")
            deadline = time.monotonic() + timeout
            while True:
                try:
                    os.lseek(descriptor, 0, os.SEEK_SET)
                    if os.name == "nt":
                        import msvcrt
                        msvcrt.locking(descriptor, msvcrt.LK_NBLCK, 1)
                    else:
                        import fcntl
                        fcntl.flock(descriptor, fcntl.LOCK_EX | fcntl.LOCK_NB)
                    acquired = True
                    break
                except OSError as error:
                    if error.errno not in {errno.EACCES, errno.EAGAIN, errno.EDEADLK}:
                        raise
                    if time.monotonic() >= deadline:
                        raise SdkError(f"Store lock timed out: {key}") from error
                    time.sleep(0.05)
            yield
        finally:
            if acquired:
                os.lseek(descriptor, 0, os.SEEK_SET)
                if os.name == "nt":
                    import msvcrt
                    msvcrt.locking(descriptor, msvcrt.LK_UNLCK, 1)
                else:
                    import fcntl
                    fcntl.flock(descriptor, fcntl.LOCK_UN)
            os.close(descriptor)

    def materialize_package(self, ref: ArtifactRef, archive: Path,
                            installer: Callable[[Path, Path], None],
                            validator: Callable[[Path], None]) -> Path:
        with self.lock(f"package:{ref.family}:{ref.kind}:{ref.identity}"):
            return self._materialize_locked(ref, archive, installer, validator)

    def _materialize_locked(self, ref: ArtifactRef, archive: Path,
                            installer: Callable[[Path, Path], None],
                            validator: Callable[[Path], None]) -> Path:
        """Validate first, stage a repair completely, publish only on success.

        Validators raise IntegrityError for invalid content; other failures are
        not interpreted as permission to replace a package.
        """
        destination = self.package_path(ref)
        if destination.exists():
            try:
                validator(destination)
                return destination
            except IntegrityError:
                pass
        verify_sha256(archive, ref.sha256)
        destination.parent.mkdir(parents=True, exist_ok=True)
        staging = Path(tempfile.mkdtemp(prefix=".sdk-staging-", dir=destination.parent))
        backup = destination.with_name(f".sdk-backup-{uuid.uuid4().hex}")
        try:
            installer(archive, staging)
            validator(staging)
            # Recheck namespace after provider work, before publication.
            self.package_path(ref)
            if destination.exists():
                destination.rename(backup)
            try:
                staging.rename(destination)
            except OSError:
                if backup.exists():
                    backup.rename(destination)
                raise
            if backup.exists():
                if backup.is_dir():
                    shutil.rmtree(backup)
                else:
                    backup.unlink()
            return destination
        finally:
            if staging.exists():
                shutil.rmtree(staging)
