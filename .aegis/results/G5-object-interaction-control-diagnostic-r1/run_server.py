from http.server import ThreadingHTTPServer,SimpleHTTPRequestHandler
from pathlib import Path
import os
root=Path(__file__).parent
os.chdir(root)
ThreadingHTTPServer(('127.0.0.1',4312),SimpleHTTPRequestHandler).serve_forever()
