"""Serve exact sampled rectangle candidate assets; label HTML response only."""
from pathlib import Path
from http.server import SimpleHTTPRequestHandler,ThreadingHTTPServer
from functools import partial
import argparse
class Demo(SimpleHTTPRequestHandler):
 extensions_map={**SimpleHTTPRequestHandler.extensions_map,'.mjs':'text/javascript','.wasm':'application/wasm','.data':'application/octet-stream'}
 def do_GET(self):
  if self.path.split('?',1)[0]in['/','/example.html']:
   text=(Path(self.directory)/'example.html').read_text();text=text.replace('<title>Axiom SDK Demo</title>','<title>Axiom 4335 · settled zoom rectangle candidate</title>');badge='<aside style="position:fixed;right:16px;bottom:44px;z-index:8;max-width:460px;padding:7px 12px;border:1px solid #deb969;border-radius:8px;background:#fff8df;color:#725419;font:12px system-ui;pointer-events:none">4335 · 缩小启动修复候选 · 固定边缘AA差异已独立核验 · 待控制面审查；整体G5未签PASS</aside>';text=text.replace('<main id="workspace">',badge+'<main id="workspace">');data=text.encode();self.send_response(200);self.send_header('Content-Type','text/html;charset=utf-8');self.send_header('Content-Length',str(len(data)));self.end_headers();self.wfile.write(data);return
  super().do_GET()
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--directory',type=Path,required=True);p.add_argument('--port',type=int,default=4335);a=p.parse_args()
 with ThreadingHTTPServer(('127.0.0.1',a.port),partial(Demo,directory=str(a.directory.resolve())))as server:
  print(f'http://127.0.0.1:{a.port}/example.html',flush=True);server.serve_forever()
