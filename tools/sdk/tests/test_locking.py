"""Cross-process locking behavior, including Windows mandatory byte locks."""
import hashlib
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from tools.sdk.store import SdkStore


class StoreLockingTest(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.store = SdkStore(Path(temporary.name) / "store")

    def test_new_lock_does_not_write_an_unowned_initialization_byte(self):
        key = "empty-file-regression"
        path = self.store.root / "locks" / (hashlib.sha256(key.encode()).hexdigest() + ".lock")
        with self.store.lock(key):
            self.assertEqual(path.stat().st_size, 0)
        self.assertEqual(path.stat().st_size, 0)
        with self.store.lock(key):
            self.assertEqual(path.stat().st_size, 0)

    def test_existing_nonempty_lock_file_remains_compatible(self):
        key = "historical-lock-file"
        path = self.store.root / "locks" / (hashlib.sha256(key.encode()).hexdigest() + ".lock")
        path.parent.mkdir(parents=True)
        path.write_bytes(b"0")
        with self.store.lock(key):
            self.assertEqual(path.stat().st_size, 1)
        self.assertEqual(path.read_bytes(), b"0")

    def test_contended_empty_lock_times_out_then_acquires_after_release(self):
        # Windows byte-range locks work beyond EOF. Hold the empty lock file
        # from another process's perspective: the old pre-lock write raises
        # PermissionError instead of entering the bounded acquisition loop.
        key = "contended-empty-file"
        path = self.store.root / "locks" / (hashlib.sha256(key.encode()).hexdigest() + ".lock")
        path.parent.mkdir(parents=True)
        descriptor = os.open(path, os.O_CREAT | os.O_RDWR, 0o600)
        if os.name == "nt":
            import msvcrt
            lock = lambda: msvcrt.locking(descriptor, msvcrt.LK_NBLCK, 1)
            unlock = lambda: msvcrt.locking(descriptor, msvcrt.LK_UNLCK, 1)
        else:
            import fcntl
            lock = lambda: fcntl.flock(descriptor, fcntl.LOCK_EX | fcntl.LOCK_NB)
            unlock = lambda: fcntl.flock(descriptor, fcntl.LOCK_UN)
        code = r"""
import os, sys
from pathlib import Path
from tools.sdk.model import SdkError
from tools.sdk.store import SdkStore
store, key, lock_path, held = SdkStore(Path(sys.argv[1])), sys.argv[2], Path(sys.argv[3]), sys.argv[4] == "held"
if held and os.name == "nt":
    fd = os.open(lock_path, os.O_RDWR)
    try:
        try:
            os.write(fd, b"0")
        except PermissionError:
            print("CONFIRMED: Windows rejects the old unowned initialization write")
        else:
            raise AssertionError("expected a native Windows locking violation")
    finally:
        os.close(fd)
try:
    with store.lock(key, timeout=0.2):
        assert not held, "contender entered the critical section"
        print("acquired")
except SdkError as error:
    assert held and "Store lock timed out" in str(error), str(error)
    print("bounded timeout")
"""
        def contender(state):
            return subprocess.run([sys.executable, "-c", code, str(self.store.root), key, str(path), state],
                                  cwd=Path(__file__).resolve().parents[3], capture_output=True,
                                  text=True, timeout=15)
        acquired = False
        try:
            lock()
            acquired = True
            result = contender("held")
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("bounded timeout", result.stdout)
            if os.name == "nt":
                self.assertIn("CONFIRMED: Windows", result.stdout)
            # The contender did not mutate a file owned by the holder.
            self.assertEqual(os.fstat(descriptor).st_size, 0)
        finally:
            if acquired:
                unlock()
            os.close(descriptor)
        result = contender("released")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("acquired", result.stdout)


if __name__ == "__main__":
    unittest.main()
