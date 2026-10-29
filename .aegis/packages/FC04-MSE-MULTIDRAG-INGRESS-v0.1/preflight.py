import hashlib,json,subprocess,sys
from pathlib import Path
root=Path.cwd(); p=Path(__file__).resolve().parent
j=json.loads((p/'package.json').read_text(encoding='utf-8'))
identity=subprocess.check_output(['git','remote','get-url','origin'],text=True).strip()
if 'Mostorm-Labs/axiom' not in identity and 'mostorm-labs/axiom' not in identity.lower(): raise SystemExit('repository identity mismatch')
if subprocess.call(['git','merge-base','--is-ancestor',j['baseline'],'HEAD']) != 0: raise SystemExit('anchor not ancestor')
for row in j['allowed_existing']:
    data=(root/row['path']).read_bytes()
    if hashlib.sha256(data).hexdigest()!=row['baseline_sha256']: raise SystemExit('source hash mismatch: '+row['path'])
    blob=subprocess.check_output(['git','rev-parse','HEAD:'+row['path']],text=True).strip()
    if blob!=row['git_blob']: raise SystemExit('source blob mismatch: '+row['path'])
if not (root/'.aegis/packages/FC04-MSE-PHASE2-P31-v0.4/package.json').is_file(): raise SystemExit('parent missing')
print('PASS repository, ancestry, exact untouched source and parent package')
