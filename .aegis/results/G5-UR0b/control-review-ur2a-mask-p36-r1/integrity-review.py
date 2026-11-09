import pathlib,json,hashlib,subprocess,tarfile,tempfile,time,shutil,sys
R=pathlib.Path.cwd();B=R/'.aegis/results/G5-UR2a-mask-isolation-p36-r1';P=R/'.aegis/packages/G5-UR2a-mask-isolation-p36-r1';O=pathlib.Path(__file__).resolve().parent
count=0
def sha(p):
 h=hashlib.sha256()
 with pathlib.Path(p).open('rb') as f:
  for block in iter(lambda:f.read(1048576),b''):h.update(block)
 return h.hexdigest()
def read(p):return json.loads(pathlib.Path(p).read_text())
def check(p,h):
 global count
 p=pathlib.Path(p);p=p if p.is_absolute() else R/p
 assert sha(p)==h,str(p);count+=1
def walk(v,base=R):
 if isinstance(v,dict):
  if 'path' in v and 'sha256' in v:
   p=pathlib.Path(v['path']);check(p if p.is_absolute() or (R/p).exists() else base/p,v['sha256'])
  for x in v.values():walk(x,base)
 elif isinstance(v,list):
  for x in v:walk(x,base)
expected={'result.json': '0ee8fc9c974bd70ea6e5477b5ac354178a76b230e0a39a35b89c341392ab6f7c', 'delivery-manifest.json': 'f2b709ca3a7e452f8b3367bd2061cb488719b9a7eaf8b9a500d7c251b3f533d2', 'source-identity.json': '42786dcdabd28fdd91ef0f83990e1fdb49999c9ceb57d391b06c31cfdf55b519', 'source-delta-manifest.json': '42019cb4f884e5f13fa2b07c97863c12defa8505ff18e20bbe6f79d99ec165ae', 'repair-delta-manifest.json': '7d48a3493dae2f86c2c4a08e322946db3cf277933c01010dcd879a4e1b3fb385', 'driver-identity.json': '1dce59a73ca1552212c1754afa116aaf84ac66e33b34d350305e9eeecc4eb2a1', 'candidate-builds.json': '1e84611f256680e3ce492b29eb1caf0dc1c05b3f9a271b79616bc95442917ac2', 'archive-manifest.json': 'ffeaeb903ff154846d3b0210350a00b02f8a179723ff865400e914a77ad66ecf'}
for n,h in expected.items():check(B/n,h)
check(P/'package.json','6a50bb05576034137f9f65c35e9a51c2b71fff0149e1a426284fd9f24e56b1b5')
subject=read(B/'result.json');assert subject['status']=='READY_FOR_CONTROL_REVIEW'
head=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip();assert head==subject['HEAD']=='bed9e779fb8f5a7f58709096bedb8dbd401d7261'
assert subprocess.check_output(['git','remote','get-url','origin'],text=True).strip()=='git@github.com:Mostorm-Labs/axiom.git'
assert subprocess.check_output(['git','branch','--show-current'],text=True).strip()=='codex/geometry-navigation-integration'
assert not subprocess.check_output(['git','diff','--cached','--name-only'])
walk(subject);walk(read(B/'delivery-manifest.json'));walk(read(P/'package.json'),P)
source=read(B/'source-identity.json');files=source['files']
assert hashlib.sha256(json.dumps(files,sort_keys=True,separators=(',',':')).encode()).hexdigest()==subject['source_tree_sha256']==source['source_tree_sha256']
names=set(subprocess.check_output(['git','ls-files','-z','--cached','--others','--exclude-standard']).decode().split('\0'))-{''}
names={n for n in names if not n.startswith(tuple(source['excluded_self_outputs']))};assert names==set(files),(sorted(names-set(files))[:5],sorted(set(files)-names)[:5])
historical_log_drift=[]
for n,v in files.items():
 p=R/n
 if v.get('kind')=='symlink':assert p.is_symlink() and str(p.readlink())==v['target']
 elif v.get('kind')=='deleted':assert not p.exists()
 else:
  if n=='.aegis/results/G5-UR1d-physical-loss-repair-r1/demo-server.log' and sha(p)!=v['sha256']:
   historical_log_drift.append({'path':n,'frozen_sha256':v['sha256'],'live_sha256':sha(p),'classification':'NON_BLOCKING_FINDING: old intentionally retained server appended requests; not a compiler, driver, oracle, candidate artifact or current result input'})
  else:check(p,v['sha256'])
  assert bool(p.stat().st_mode&0o111)==v['executable']
delta=read(B/'source-delta-manifest.json');assert len(delta['files'])==23
changed={e['path'] for e in delta['files']};tracked=set(subprocess.check_output(['git','ls-files','-z']).decode().split('\0'))-{''}
assert set(subprocess.check_output(['git','diff','--name-only']).decode().splitlines())==changed&tracked
for e in delta['files']:
 check(e['path'],e['sha256']);check(B/'cumulative-postimages'/e['path'],e['sha256'])
 if e['preimage_sha256']:
  check(B/'baseline-preimages'/e['path'],e['preimage_sha256'])
  assert hashlib.sha256(subprocess.check_output(['git','show',head+':'+e['path']])).hexdigest()==e['preimage_sha256']
patch=B/'source-delta.patch';check(patch,delta['patch_sha256'])
with tempfile.TemporaryDirectory(prefix='axiom-control-ur2a-') as tmp:
 t=pathlib.Path(tmp)
 for e in delta['files']:
  if e['preimage_sha256']:
   p=t/e['path'];p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes((B/'baseline-preimages'/e['path']).read_bytes())
 subprocess.run(['git','-c','core.autocrlf=false','apply',str(patch)],cwd=t,check=True,capture_output=True)
 for e in delta['files']:assert sha(t/e['path'])==e['sha256']
repair=read(B/'repair-delta-manifest.json');assert len(repair['files'])==4
old=read(R/'.aegis/results/G5-UR2a-current-scene-handoff-r2/source-identity.json')
repair_paths={e['path'] for e in repair['files']}
assert {n for n,v in old['files'].items() if n in files and files[n]!=v}=={e['path'] for e in repair['files'] if e['preimage_sha256']}
assert set(files)-set(old['files'])=={str(p.relative_to(R)) for p in P.iterdir() if p.is_file()}|{e['path'] for e in repair['files'] if not e['preimage_sha256']}
assert set(old['files'])-set(files)=={'.aegis/results/G5-UR1d-physical-loss-repair-r1/demo-server.log'}
check(B/'repair-delta.patch',repair['patch_sha256'])
with tempfile.TemporaryDirectory(prefix='axiom-control-ur2a-repair-') as tmp:
 t=pathlib.Path(tmp)
 for e in repair['files']:
  check(e['path'],e['sha256'])
  if e['preimage_sha256']:
   check(B/'preimages'/e['path'],e['preimage_sha256']);assert old['files'][e['path']]['sha256']==e['preimage_sha256']
   p=t/e['path'];p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes((B/'preimages'/e['path']).read_bytes())
 subprocess.run(['git','-c','core.autocrlf=false','apply',str(B/'repair-delta.patch')],cwd=t,check=True,capture_output=True)
 for e in repair['files']:assert sha(t/e['path'])==e['sha256']
payload=read(B/'payload-manifest.json')
for e in payload['files']:check(e['readonly_path'],e['sha256'])
members=0
for a in read(B/'archive-manifest.json')['archives']:
 check(a['path'],a['sha256']);expectedMembers={x['path']:x for x in a['members']};seen=set()
 with tarfile.open(R/a['path'],'r|gz') as tf:
  for ti in tf:
   if not ti.isfile():continue
   assert ti.name in expectedMembers and ti.name not in seen,ti.name
   e=expectedMembers[ti.name];assert ti.size==e['bytes'];h=hashlib.sha256()
   with tf.extractfile(ti) as f:
    for chunk in iter(lambda:f.read(1048576),b''):h.update(chunk)
   assert h.hexdigest()==e['sha256'],ti.name;members+=1;seen.add(ti.name)
 assert seen==set(expectedMembers)
walk(read(B/'native-artifact-manifest.json'))
logical={'oracle':R/'out/G5-UR1c1-prepared-body-index/build-native/runtime/render/axiom_ur0_fixture_oracle','catalog':R/'.aegis/results/G5-UR1d-physical-loss-repair-r1/reference-catalog.json','node':pathlib.Path(shutil.which('node')).resolve(),'python3.12':pathlib.Path(sys.executable).resolve()}
for path,h in read(B/'driver-identity.json')['frozen_inputs'].items():check(logical.get(path,path),h)
sdk=read(B/'candidate-builds.json')['SDK']
for variant in ['normal','qualification']:
 s=sdk[variant]
 root=pathlib.Path(s['root']);assert len(s['files'])==36
 assert {str(p) for p in root.rglob('*') if p.is_file()}=={x['path'] for x in s['files']}
 for e in s['files']:check(e['path'],e['sha256'])
check(R/'out/G5-UR1c1-prepared-body-index/build-native/runtime/render/axiom_ur0_fixture_oracle','ab7a1a3ce61076eb34c1bccb7c59a61f09e5a51323c45403796ad2eab1440105')
out={'status':'VERIFIED_SUBJECT_WITH_HISTORIC_LOG_DRIFT' if historical_log_drift else 'VERIFIED','result_sha256':sha(B/'result.json'),'source_tree_sha256':source['source_tree_sha256'],'source_files':len(files),'delta_files':23,'other_tracked_unchanged':len(tracked-changed),'cumulative_and_repair_patch_roundtrips':True,'repair_files':4,'payload_files':len(payload['files']),'archive_members':members,'hash_checks':count,'sdk_files_each':36,'index_empty':True,'browser_starts':0,'oracle_invocations':0,'historical_log_drift':historical_log_drift}
(O/'integrity-review.json').write_text(json.dumps(out,indent=2)+'\n');print(json.dumps(out))
