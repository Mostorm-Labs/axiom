#!/usr/bin/env python3
"""Serve the external SDK demo with explicit WebAssembly and module MIME types."""
import argparse
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path


class DemoHandler(SimpleHTTPRequestHandler):
    extensions_map = {
        **SimpleHTTPRequestHandler.extensions_map,
        '.mjs': 'text/javascript',
        '.js': 'text/javascript',
        '.wasm': 'application/wasm',
        '.data': 'application/octet-stream',
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--directory', type=Path, default=Path(__file__).resolve().parent)
    parser.add_argument('--port', type=int, default=4186)
    args = parser.parse_args()
    directory = args.directory.resolve(strict=True)
    if not directory.is_dir() or not (directory / 'index.html').is_file():
        parser.error('directory must contain index.html')
    with ThreadingHTTPServer(('127.0.0.1', args.port), partial(DemoHandler, directory=str(directory))) as server:
        print(f'http://127.0.0.1:{server.server_port}/', flush=True)
        server.serve_forever()


if __name__ == '__main__':
    main()
