from pathlib import Path
import json,tarfile,hashlib,subprocess,importlib.util
R=Path(__file__).resolve().parents[3];B=Path(__file__).resolve().parent
read=lambda p:json.loads(p.read_text())
def sha(p):
 with p.open('rb')as f:return hashlib.file_digest(f,'sha256').hexdigest()
def ref(p):return{'path':str(p.relative_to(R)),'sha256':sha(p),'bytes':p.stat().st_size}
rows=[];total=0
for role,v in read(B/'archives.json').items():
 archive=R/v['archive']['path'];index=R/v['members']['path'];assert sha(archive)==v['archive']['sha256']and sha(index)==v['members']['sha256'];assert not archive.stat().st_mode&0o222 and not index.stat().st_mode&0o222
 members={r['archive_member']:r for r in read(index)['files']};seen=set()
 with tarfile.open(archive,'r|gz')as t:
  for entry in t:
   assert entry.isfile()and not(entry.mode&0o222),entry.name;assert entry.name in members and entry.name not in seen
   f=t.extractfile(entry);h=hashlib.file_digest(f,'sha256').hexdigest();r=members[entry.name];assert h==r['sha256']and entry.size==r['bytes'],entry.name;seen.add(entry.name)
 assert seen==members.keys();total+=len(seen);rows.append({'role':role,'members':len(seen),'archive':ref(archive),'all_members_exact':True,'readonly':True});print(role,len(seen),flush=True)
proof={'status':'VERIFIED','archives':rows,'members':total,'no_extraction_or_old_artifact_mutation':True};(B/'archive-verification.json').write_text(json.dumps(proof,indent=2)+'\n')
manifest=read(B/'manifest.json');manifest['archive_verification']=ref(B/'archive-verification.json');(B/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
result=read(B/'result.json');result['manifest']=ref(B/'manifest.json');result['archive_verification']=ref(B/'archive-verification.json');(B/'result.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({'result':ref(B/'result.json'),'manifest':ref(B/'manifest.json'),'verification':ref(B/'archive-verification.json')}))
