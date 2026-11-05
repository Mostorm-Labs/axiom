import json,hashlib
from pathlib import Path
packages={}
for mode in ['on','off']:
 root=Path('out/g5-gpu-closed-'+mode+'/js');base=Path('out/g5-reuse-final-'+mode+'/js')
 names=lambda p:sorted(str(x.relative_to(p)) for x in p.rglob('*') if x.is_file())
 files=names(root);wasm=(root/'axiom.wasm').read_bytes()
 packages[mode]={'file_count':len(files),'same_normal_file_set':files==names(base),'private_headers_or_archives':[p for p in files if p.endswith(('.h','.hpp','.a','.lib'))],
 'timing_markers':{s:s.encode() in wasm for s in ['AXIOM_FRAME_TIMING','AXIOM_NAVIGATION_TIMING','navigation_staging_bytes','navigation_pending_release_bytes']},
 'wasm_sha256':hashlib.sha256(wasm).hexdigest(),'wasm_bytes':len(wasm)}
passed=all(v['file_count']==36 and v['same_normal_file_set'] and not v['private_headers_or_archives'] and all(v['timing_markers'].values())==(mode=='on') for mode,v in packages.items())
# OFF must have none, rather than merely not all markers.
passed &= not any(packages['off']['timing_markers'].values())
j={'passed':passed,'packages':packages};Path('out/g5-gpu-closed-package-checks.json').write_text(json.dumps(j,indent=2));print(j)
raise SystemExit(0 if passed else 1)
