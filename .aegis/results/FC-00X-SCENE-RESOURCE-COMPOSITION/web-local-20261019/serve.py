from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path


class Handler(SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header('Cache-Control', 'no-store')
        super().end_headers()


root = Path(__file__).resolve().parent / 'build/apps'
print(f'Axiom local Web test server: http://127.0.0.1:8765/ink_playground/ root={root}', flush=True)
ThreadingHTTPServer(('127.0.0.1', 8765), partial(Handler, directory=str(root))).serve_forever()
