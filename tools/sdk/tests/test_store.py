"""Behavioral contracts for generic SDK paths, archives and materialization."""
from dataclasses import replace
import hashlib
import os
import multiprocessing
import time
from pathlib import Path
import stat
import tempfile
import unittest
import zipfile

from tools.sdk.archive import canonical_bytes, create_deterministic_zip, safe_extract_zip
from tools.sdk.model import ArtifactRef, IntegrityError, ReleaseIndexRef, SdkError, detect_host_platform
from tools.sdk.store import SdkStore, default_store_root


def artifact(**changes):
    return replace(ArtifactRef("semantic", "runtimes", "linux-x86_64", "a" * 64,
                               "Mostorm-Labs/axiom", "release-a", "runtime.zip", "b" * 64), **changes)


class HostAndPathTest(unittest.TestCase):
    def test_supported_host_aliases(self):
        cases = [("Linux", "AMD64", "linux-x86_64"), ("Windows", "AMD64", "windows-x64"),
                 ("Darwin", "aarch64", "macos-arm64"), ("Darwin", "x86_64", "macos-x64")]
        for system, machine, expected in cases:
            with self.subTest(system=system, machine=machine):
                self.assertEqual(detect_host_platform(system, machine).key, expected)

    def test_unknown_host_is_not_guessed(self):
        with self.assertRaisesRegex(SdkError, "Linux/aarch64"):
            detect_host_platform("Linux", "aarch64")

    def test_platform_roots_and_override(self):
        with tempfile.TemporaryDirectory() as directory:
            home = Path(directory)
            cases = [("Windows", {"LOCALAPPDATA": str(home / "Local")}, home / "Local/Axiom/sdk"),
                     ("Darwin", {}, home / "Library/Application Support/Axiom/sdk"),
                     ("Linux", {}, home / ".local/share/axiom/sdk"),
                     ("Linux", {"XDG_DATA_HOME": str(home / "Data")}, home / "Data/axiom/sdk")]
            for system, env, expected in cases:
                self.assertEqual(default_store_root(system, env, home), expected)
            self.assertEqual(default_store_root("Windows", {"AXIOM_SDK_STORE": str(home / "shared")}, home), home / "shared")

    def test_missing_windows_localappdata_fails(self):
        with self.assertRaises(SdkError):
            default_store_root("Windows", {}, Path.home())

    def test_refs_reject_namespace_or_asset_escape(self):
        for changes in ({"family": "../skia"}, {"kind": "/tmp"}, {"asset": "../asset.zip"},
                        {"asset": "C:asset.zip"}, {"asset": "NUL.zip"}, {"sha256": "BAD"},
                        {"identity": "../escape"}, {"repository": "org/repo/extra"}):
            with self.subTest(changes=changes), self.assertRaises(SdkError):
                artifact(**changes)

    def test_content_paths_do_not_include_release_tag_or_project(self):
        with tempfile.TemporaryDirectory() as directory:
            store = SdkStore(Path(directory))
            ref = artifact()
            self.assertEqual(store.package_path(ref), store.root / "packages/semantic/runtimes" / ref.identity)
            self.assertEqual(store.archive_path(ref), store.root / "archives/sha256" / ref.sha256 / ref.asset)
            self.assertEqual(store.package_path(ref), store.package_path(replace(ref, release_tag="release-b")))
            index = ReleaseIndexRef(ref.family, "c" * 64, ref.repository, ref.release_tag, "index.json", "d" * 64)
            self.assertEqual(store.release_index_path(index), store.root / "release-sets/semantic" / index.identity / index.asset)


class ArchiveTest(unittest.TestCase):
    def test_canonical_json_does_not_depend_on_dict_order(self):
        self.assertEqual(canonical_bytes({"b": 2, "a": 1}), b'{"a":1,"b":2}')
        with self.assertRaises(ValueError):
            canonical_bytes({"a": float("nan")})

    def test_archive_is_deterministic_and_preserves_declared_modes(self):
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            root = base / "root"
            (root / "bin").mkdir(parents=True)
            (root / "bin/tool").write_bytes(b"tool")
            (root / "readme.txt").write_bytes(b"readme")
            a, b = base / "a.zip", base / "b.zip"
            modes = {"bin/tool": 0o755, "readme.txt": 0o644}
            create_deterministic_zip(root, a, modes=modes)
            os.utime(root / "bin/tool", (123456, 123456))
            create_deterministic_zip(root, b, modes=modes)
            self.assertEqual(a.read_bytes(), b.read_bytes())
            with zipfile.ZipFile(a) as zipped:
                self.assertEqual(zipped.namelist(), ["bin/tool", "readme.txt"])
                self.assertEqual(zipped.getinfo("bin/tool").date_time, (1980, 1, 1, 0, 0, 0))
            safe_extract_zip(a, base / "unpacked")
            self.assertEqual((base / "unpacked/bin/tool").read_bytes(), b"tool")
            if os.name != "nt":
                self.assertEqual(stat.S_IMODE((base / "unpacked/bin/tool").stat().st_mode), 0o755)

    def test_unsafe_zip_paths_fail_before_writing_payload(self):
        bad_names = ["../escape", "/escape", "C:/escape", "a\\..\\escape", "a/../escape",
                     "a//b", "./b", "payload:stream", "NUL", "a/CON.txt", "a./b"]
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            for number, name in enumerate(bad_names):
                with self.subTest(name=name):
                    archive = base / f"bad{number}.zip"
                    with zipfile.ZipFile(archive, "w") as zipped:
                        zipped.writestr("valid.txt", "valid")
                        zipped.writestr(name, "bad")
                    destination = base / f"dest{number}"
                    with self.assertRaises(SdkError):
                        safe_extract_zip(archive, destination)
                    self.assertFalse((destination / "valid.txt").exists())

    def test_zip_rejects_case_collisions_symlinks_and_limits(self):
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            archive = base / "bad.zip"
            with zipfile.ZipFile(archive, "w") as zipped:
                zipped.writestr("One", "a")
                zipped.writestr("one", "b")
            with self.assertRaises(SdkError):
                safe_extract_zip(archive, base / "case")
            with zipfile.ZipFile(archive, "w") as zipped:
                link = zipfile.ZipInfo("link")
                link.create_system = 3
                link.external_attr = (stat.S_IFLNK | 0o777) << 16
                zipped.writestr(link, "../../escape")
            with self.assertRaises(SdkError):
                safe_extract_zip(archive, base / "link")
            with zipfile.ZipFile(archive, "w") as zipped:
                zipped.writestr("file", b"12345")
            with self.assertRaises(SdkError):
                safe_extract_zip(archive, base / "limit", max_bytes=4)

    def test_nonempty_destination_is_never_overwritten(self):
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            archive = base / "archive.zip"
            with zipfile.ZipFile(archive, "w") as zipped:
                zipped.writestr("file", "new")
            dest = base / "dest"
            dest.mkdir()
            (dest / "file").write_text("old")
            with self.assertRaises(SdkError):
                safe_extract_zip(archive, dest)
            self.assertEqual((dest / "file").read_text(), "old")


def concurrent_install_worker(directory, barrier, results):
    base = Path(directory)
    archive = base / "candidate.zip"
    ref = artifact(sha256=hashlib.sha256(archive.read_bytes()).hexdigest())
    store = SdkStore(base / "store")
    def install(source, staging):
        (base / "calls" / str(os.getpid())).write_text("called")
        time.sleep(0.15)
        (staging / "payload.txt").write_text("new")
    def validate(root):
        if not (root / "payload.txt").is_file() or (root / "payload.txt").read_text() != "new":
            raise IntegrityError("invalid payload")
    try:
        barrier.wait(timeout=10)
        store.materialize_package(ref, archive, install, validate)
        results.put("ok")
    except Exception as error:
        results.put(type(error).__name__ + ": " + str(error))


class StoreTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.store = SdkStore(Path(self.temp.name) / "store")
        self.archive = Path(self.temp.name) / "candidate.zip"
        self.archive.write_bytes(b"candidate")
        self.ref = artifact(sha256=hashlib.sha256(b"candidate").hexdigest())

    @staticmethod
    def validate_new(root):
        if not (root / "payload.txt").is_file() or (root / "payload.txt").read_text() != "new":
            raise IntegrityError("invalid payload")

    @staticmethod
    def install_new(archive, root):
        (root / "payload.txt").write_text("new")

    def test_two_processes_materialize_one_package_once(self):
        (Path(self.temp.name) / "calls").mkdir()
        context = multiprocessing.get_context("spawn")
        barrier = context.Barrier(2)
        results = context.Queue()
        processes = [context.Process(target=concurrent_install_worker,
                     args=(self.temp.name, barrier, results)) for _ in range(2)]
        for process in processes:
            process.start()
        try:
            outputs = [results.get(timeout=15) for _ in processes]
            for process in processes:
                process.join(timeout=15)
                self.assertEqual(process.exitcode, 0)
            self.assertEqual(outputs, ["ok", "ok"])
            self.assertEqual(len(list((Path(self.temp.name) / "calls").iterdir())), 1)
        finally:
            for process in processes:
                if process.is_alive():
                    process.terminate()
                process.join(timeout=5)
            results.close()

    def test_miss_materializes_and_next_use_does_not_reinstall(self):
        root = self.store.materialize_package(self.ref, self.archive, self.install_new, self.validate_new)
        self.assertEqual(root, self.store.package_path(self.ref))
        def unexpected_install(archive, target):
            self.fail("valid immutable package was reinstalled")
        again = self.store.materialize_package(self.ref, self.archive, unexpected_install, self.validate_new)
        self.assertEqual(root, again)

    def test_failed_repair_preserves_old_invalid_directory(self):
        root = self.store.package_path(self.ref)
        root.mkdir(parents=True)
        (root / "payload.txt").write_text("old")
        def fail_install(archive, staging):
            self.assertEqual(archive, self.archive)
            (staging / "payload.txt").write_text("new")
            raise RuntimeError("injected install failure")
        with self.assertRaisesRegex(RuntimeError, "injected install failure"):
            self.store.materialize_package(self.ref, self.archive, fail_install, self.validate_new)
        self.assertEqual((root / "payload.txt").read_text(), "old")
        self.assertEqual([p.name for p in root.parent.iterdir()], [self.ref.identity])

    def test_failed_staging_validation_never_installs(self):
        def invalid(archive, staging):
            (staging / "payload.txt").write_text("wrong")
        with self.assertRaises(IntegrityError):
            self.store.materialize_package(self.ref, self.archive, invalid, self.validate_new)
        self.assertFalse(self.store.package_path(self.ref).exists())

    def test_valid_repair_replaces_invalid_directory(self):
        root = self.store.package_path(self.ref)
        root.mkdir(parents=True)
        (root / "payload.txt").write_text("bad")
        self.store.materialize_package(self.ref, self.archive, self.install_new, self.validate_new)
        self.assertEqual((root / "payload.txt").read_text(), "new")

    def test_archive_hash_is_checked_before_installer(self):
        def unexpected(archive, root):
            self.fail("installer executed before archive verification")
        with self.assertRaises(IntegrityError):
            self.store.materialize_package(artifact(), self.archive, unexpected, self.validate_new)

    @unittest.skipIf(os.name == "nt", "POSIX symlink fixture; Windows path tests run separately")
    def test_store_rejects_symlinked_family_path(self):
        outside = Path(self.temp.name) / "outside"
        outside.mkdir()
        (self.store.root / "packages").mkdir(parents=True)
        (self.store.root / "packages/semantic").symlink_to(outside, target_is_directory=True)
        with self.assertRaises(SdkError):
            self.store.materialize_package(self.ref, self.archive, self.install_new, self.validate_new)
        self.assertEqual(list(outside.iterdir()), [])


if __name__ == "__main__":
    unittest.main()
