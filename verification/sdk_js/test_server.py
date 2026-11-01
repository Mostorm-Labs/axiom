import queue
from pathlib import Path
import subprocess
import sys
import tempfile
import threading
import unittest
from urllib.request import urlopen


class StandaloneServerTest(unittest.TestCase):
    def _serves_entry(self, entry):
        script = Path(__file__).parents[1] / 'sdk_web_external/consumer/serve.py'
        with tempfile.TemporaryDirectory(prefix='axiom-js-server-') as temp:
            root = Path(temp)
            (root / entry).write_text('<title>SDK</title>', encoding='utf-8')
            (root / 'axiom.mjs').write_text('export const version=1;', encoding='utf-8')
            (root / 'axiom.wasm').write_bytes(b'\0asm')
            process = subprocess.Popen(
                [sys.executable, str(script), '--directory', str(root), '--port', '0'],
                stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            try:
                output = queue.Queue()
                threading.Thread(target=lambda: output.put(process.stdout.readline()), daemon=True).start()
                url = output.get(timeout=10).strip()
                suffix = '/' if entry == 'index.html' else '/example.html'
                self.assertTrue(url.endswith(suffix), url)
                with urlopen(url, timeout=5) as response:
                    self.assertEqual(response.status, 200)
                base = url.removesuffix('example.html')
                with urlopen(base + 'axiom.mjs', timeout=5) as response:
                    self.assertIn('javascript', response.headers['Content-Type'])
                with urlopen(base + 'axiom.wasm', timeout=5) as response:
                    self.assertEqual(response.headers['Content-Type'], 'application/wasm')
            finally:
                process.terminate()
                process.communicate(timeout=10)

    def test_example_only_package_starts_and_serves_modules(self):
        self._serves_entry('example.html')

    def test_legacy_index_package_retains_root_url(self):
        self._serves_entry('index.html')


if __name__ == '__main__':
    unittest.main()
