#!/usr/bin/env python3
import argparse,hashlib,json,sys
from pathlib import Path

def main():
 p=argparse.ArgumentParser(); p.add_argument('--root',type=Path,required=True); a=p.parse_args(); root=a.root.resolve(); d=root/'.aegis/packages/FC-00X-RESOURCE-MANIFEST-IMPLEMENTATION'; pkg=json.loads((d/'package.json').read_text())
 refs=['authority_lock_ref','verification_lock_ref','execution_contract_ref','implementation_context_ref','evidence_contract_ref']; errs=[]
 for key in refs:
  x=pkg[key]; got=hashlib.sha256((d/x['path']).read_bytes()).hexdigest(); errs.append((key,got,x.get('sha256','')))
 for x in pkg['oracle_refs']:
  got=hashlib.sha256((d/x['path']).read_bytes()).hexdigest(); errs.append((x['path'],got,x.get('sha256','')))
 print(json.dumps({'refs':errs},indent=2)); return 1 if any(g!=e for _,g,e in errs) else 0
if __name__=='__main__':sys.exit(main())
