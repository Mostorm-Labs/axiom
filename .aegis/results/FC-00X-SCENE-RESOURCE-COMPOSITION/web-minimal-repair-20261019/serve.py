from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import sys

log = (Path(__file__).resolve().parent / 'server-persistent.log').open('a', buffering=1)
sys.stdout = log
sys.stderr = log


class Handler(SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header('Cache-Control', 'no-store')
        super().end_headers()


root = Path(__file__).resolve().parent / 'build/apps'
print(f'Axiom local Web test server: http://127.0.0.1:8765/axiom_canvas_demo/ root={root}', flush=True)
ThreadingHTTPServer(('127.0.0.1', 8765), partial(Handler, directory=str(root))).serve_forever()

