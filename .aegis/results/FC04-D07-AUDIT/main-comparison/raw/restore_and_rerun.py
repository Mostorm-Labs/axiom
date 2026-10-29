from pathlib import Path
import json,hashlib,shutil,subprocess,xml.etree.ElementTree as ET
root=Path.cwd();out=root/'out/fc04-d07';cache=Path('C:/Users/Chado/.codex/worktrees/fc08-p31-bootstrap/canvas')
receipt='.aegis/results/FC-08-SECTION-C-H03-v0.1/qualified-H03-raw/invocations.json'
rows=json.loads((root/receipt).read_text())['input_observations']
rows=[{**r,'binding':receipt} for r in rows]
other='.aegis/results/FC-08-SECTION-C/qualified-001/budget-input-receipts.json'
for r in json.loads((root/other).read_text()):
 if r['variant_id'].startswith('FC08-H04/'):
  wire=next(w for w in r['wire_hashes'] if w['label']=='F');rows.append({'path':'out/fc08-section-c/frozen-budgets/'+r['variant_id'].split('/')[1]+'.pb','sha256':wire['sha256'],'bytes':wire['size'],'binding':other})
for r in rows:
 source=cache/r['path'];dest=root/r['path'];assert source.is_file(),source
 assert hashlib.sha256(source.read_bytes()).hexdigest()==r['sha256'],source
 assert dest.resolve().is_relative_to((root/'out/fc08-section-c').resolve())
 if dest.exists():assert hashlib.sha256(dest.read_bytes()).hexdigest()==r['sha256']
 else:dest.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(source,dest)
 assert hashlib.sha256(dest.read_bytes()).hexdigest()==r['sha256']
 r['source']=source.as_posix();r['action']='restored ignored cache bytes, no input/expected regeneration'
(out/'input-cache-restoration.json').write_text(json.dumps(rows,indent=2),newline='\n');print('verified restored artifacts',len(rows),flush=True)
failed=[c.attrib['name'] for c in ET.parse(out/'existing.xml').findall('.//testcase') if c.find('failure') is not None];assert len(failed)==6
(out/'restored-rerun-names.txt').write_text('\n'.join(failed)+'\n')
cmd=['ctest','--test-dir',str(out/'build'),'--tests-from-file',str(out/'restored-rerun-names.txt'),'--no-tests=error','--output-on-failure','--timeout','300','--output-junit',str(out/'restored-rerun.xml')]
with (out/'restored-rerun.log').open('w',encoding='utf-8') as log:r=subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT)
(out/'restored-rerun-command.json').write_text(json.dumps({'argv':cmd,'exit_code':r.returncode},indent=2));print('rerun exit',r.returncode);raise SystemExit(r.returncode)
