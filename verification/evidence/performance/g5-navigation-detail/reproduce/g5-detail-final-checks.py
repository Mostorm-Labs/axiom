from pathlib import Path
import hashlib,json,re,subprocess
P='g5-detail-final'
def write(n,v):Path('out/'+P+'-'+n+'.json').write_text(json.dumps(v,indent=2)+'\n')
s=Path('out/'+P+'-native.log').read_text();failed=re.findall(r'^\s*\d+ - ([^ ]+) \(',s,re.M);old=json.loads(Path('out/g5-budget-closed-native-comparison.json').read_text())['failed_names']
write('native-comparison',{'tests':112,'passed':112-len(failed),'failed_names':failed,'baseline_failed_names':old,'new_failures':sorted(set(failed)-set(old)),'resolved_failures':sorted(set(old)-set(failed))})
assert '112' in s and set(failed)==set(old)
rows=[]
for mode in ['on','off']:
 r=Path('out/'+P+'-'+mode+'/js');files={str(f.relative_to(r)):hashlib.sha256(f.read_bytes()).hexdigest() for f in sorted(r.rglob('*')) if f.is_file()}
 forbidden=[f for f in files if f.endswith(('.hpp','.h','.a','.lib')) or '/qualification' in f or '/runtime/' in f]
 facts={k:v for k,v in re.findall(r'^([^#/:=]+):[^=]+=([^\n]*)$',Path('out/'+P+'-'+mode+'-CMakeCache.txt').read_text(),re.M) if k in ['AXIOM_RENDER_CACHE_MIB','AXIOM_RENDER_SHARED_MIB','AXIOM_ENABLE_FRAME_TIMING','AXIOM_INSTALL_DEVELOPER_COMPONENTS']}
 exports=json.loads((r/'package.json').read_text())['exports']
 rows.append({'profile':mode,'files':files,'count':len(files),'forbiddenFiles':forbidden,'buildFacts':facts,'exports':exports,'passed':not forbidden and list(exports)==['.'] and facts=={'AXIOM_ENABLE_FRAME_TIMING':mode.upper(),'AXIOM_INSTALL_DEVELOPER_COMPONENTS':'OFF','AXIOM_RENDER_CACHE_MIB':'32','AXIOM_RENDER_SHARED_MIB':'96'}})
write('package-checks',{'profiles':rows,'passed':all(r['passed'] for r in rows)})
assert all(r['passed'] for r in rows)
print('Known native failures only;normal root packages and unchanged budgets verified')
