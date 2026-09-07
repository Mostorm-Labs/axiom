"""Publication validates all bytes and never overwrites existing Release assets."""
import copy
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest import mock

from tools.sdk.model import SdkError
from tools.semantic.aggregate import aggregate
from tools.semantic.publish_release import publish_release
from tools.semantic.tests.release_fixtures import release_fixture

COMMIT = "a" * 40
REPO = "Mostorm-Labs/axiom"


class PublishReleaseTest(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.base = Path(temporary.name)
        assets, _ = release_fixture(self.base)
        self.directory = self.base / "release"
        self.release = aggregate(assets, self.directory)
        self.metadata = {"tag_name": self.release["tag"], "target_commitish": COMMIT,
                         "draft": False, "prerelease": True,
                         "assets": [{"name": name} for name in self.release["assets"]]}
        self.commands = []
        self.exists = True
        self.tag_commit = COMMIT
        self.corrupt = False
        self.lookup_error = None
        self.downloads = 0
        self.created_files = []
        self.tag_exists = True

    def run_gh(self, command, **kwargs):
        self.commands.append(command)
        if command[:2] == ["gh", "api"]:
            if "/releases/tags/" in command[2]:
                if self.lookup_error:
                    return subprocess.CompletedProcess(command, 1, "", self.lookup_error)
                if not self.exists or self.metadata["draft"]:
                    return subprocess.CompletedProcess(command, 1, "", "gh: Not Found (HTTP 404)")
                value = self.metadata
            elif "/releases?" in command[2]:
                self.assertIn("--paginate", command)
                self.assertIn("--slurp", command)
                value = [[], [self.metadata] if self.exists else []]
            else:
                if not self.tag_exists:
                    return subprocess.CompletedProcess(command, 1, "", "gh: Not Found (HTTP 404)")
                value = {"sha": self.tag_commit}
            return subprocess.CompletedProcess(command, 0, json.dumps(value), "")
        if command[:3] == ["gh", "release", "download"]:
            destination = Path(command[command.index("--dir") + 1])
            for name in self.release["assets"]:
                shutil.copyfile(self.directory / name, destination / name)
            if self.corrupt:
                (destination / "SHA256SUMS").write_bytes(b"changed remotely")
            self.downloads += 1
        elif command[:3] == ["gh", "release", "create"]:
            self.exists = True
            self.metadata["draft"] = True
            self.created_files = [arg for arg in command if Path(arg).is_file()]
        elif command[:3] == ["gh", "release", "edit"]:
            self.assertGreater(self.downloads, 0, "a draft must be byte-verified before visibility changes")
            self.metadata["draft"] = False
            self.tag_exists = True
        else:
            self.fail("unexpected command: " + repr(command))
        return subprocess.CompletedProcess(command, 0, "", "")

    def invoke(self, **options):
        with mock.patch("tools.semantic.publish_release.subprocess.run", side_effect=self.run_gh):
            return publish_release(self.directory, REPO, COMMIT, **options)

    def test_existing_identical_release_is_read_only_and_download_verified(self):
        self.assertEqual(self.invoke()["status"], "already-published")
        self.assertGreater(self.downloads, 0)
        self.assertFalse(any(c[:3] in (["gh", "release", "create"], ["gh", "release", "edit"]) for c in self.commands))

    def test_existing_commit_asset_set_or_bytes_mismatch_fails_without_writes(self):
        original = copy.deepcopy(self.metadata)
        mutations = [lambda: self.metadata.update(target_commitish="b" * 40),
                     lambda: self.metadata["assets"].pop(),
                     lambda: self.metadata["assets"].append({"name": "extra.zip"}),
                     lambda: self.metadata["assets"].append(self.metadata["assets"][0]),
                     lambda: setattr(self, "corrupt", True),
                     lambda: setattr(self, "tag_commit", "b" * 40)]
        for mutate in mutations:
            self.metadata = copy.deepcopy(original)
            self.corrupt = False
            self.tag_commit = COMMIT
            self.commands = []
            mutate()
            with self.assertRaises(SdkError):
                self.invoke()
            self.assertFalse(any(c[:3] in (["gh", "release", "create"], ["gh", "release", "edit"]) for c in self.commands))

    def test_new_release_is_staged_as_draft_verified_then_published(self):
        self.exists = False
        self.assertEqual(self.invoke()["status"], "published")
        create = next(c for c in self.commands if c[:3] == ["gh", "release", "create"])
        self.assertIn("--draft", create)
        self.assertEqual({Path(p).name for p in self.created_files}, set(self.release["assets"]))
        self.assertNotIn("release.json", {Path(p).name for p in self.created_files})
        self.assertFalse(any("--clobber" in c or "delete" in c for c in self.commands))

    def test_auth_failure_is_not_treated_as_absent_release(self):
        self.lookup_error = "gh: Forbidden (HTTP 403)"
        with self.assertRaises(SdkError):
            self.invoke()
        self.assertEqual(len(self.commands), 1)

    def test_local_corruption_and_nonexact_commit_fail_before_network(self):
        with mock.patch("tools.semantic.publish_release.subprocess.run", side_effect=AssertionError("network")):
            with self.assertRaises(SdkError):
                publish_release(self.directory, REPO, "main")
            (self.directory / "SHA256SUMS").write_bytes(b"bad")
            with self.assertRaises(SdkError):
                publish_release(self.directory, REPO, COMMIT)

    def test_dry_run_does_not_call_github(self):
        with mock.patch("tools.semantic.publish_release.subprocess.run", side_effect=AssertionError("network")):
            result = publish_release(self.directory, REPO, COMMIT, dry_run=True)
        self.assertEqual(result["status"], "dry-run")
        self.assertEqual(result["assets"], self.release["assets"])


    def test_complete_draft_can_resume_but_partial_draft_is_never_repaired_in_place(self):
        self.metadata["draft"] = True
        self.assertEqual(self.invoke()["status"], "published")
        self.metadata["draft"] = True
        self.metadata["assets"].pop()
        self.commands = []
        with self.assertRaises(SdkError):
            self.invoke()
        self.assertFalse(any(c[:3] == ["gh", "release", "edit"] for c in self.commands))

    def test_preexisting_git_tag_mismatch_blocks_new_release(self):
        self.exists = False
        self.tag_commit = "b" * 40
        with self.assertRaises(SdkError):
            self.invoke()
        self.assertFalse(any(c[:3] == ["gh", "release", "create"] for c in self.commands))

    def test_failed_draft_verification_never_makes_release_visible(self):
        self.exists = False
        self.corrupt = True
        with self.assertRaises(SdkError):
            self.invoke()
        self.assertTrue(self.metadata["draft"])
        self.assertFalse(any(c[:3] == ["gh", "release", "edit"] for c in self.commands))


    def test_pending_draft_tag_need_not_exist_until_publication(self):
        self.exists = False
        self.tag_exists = False
        self.assertEqual(self.invoke()["status"], "published")
        self.assertTrue(self.tag_exists)


if __name__ == "__main__":
    unittest.main()
