import pathlib,json,subprocess,hashlib,time,concurrent.futures
R=pathlib.Path.cwd();B=R/'.aegis/results/G5-UR2a-mask-isolation-p36-r1';O=pathlib.Path(__file__).resolve().parent
read=lambda p:json.loads(p.read_text())
def digest(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  for block in iter(lambda:f.read(1024*1024),b''):h.update(block)
 return h.hexdigest()
artifacts=read(B/'native-artifact-manifest.json')['files']
for x in artifacts:assert digest(R/x['path'])==x['sha256']
def run(name,cmd,cwd=R,timeout=180):
 start=time.monotonic()
 with (O/(name+'.log')).open('w') as out:
  p=subprocess.run(cmd,cwd=cwd,stdout=out,stderr=subprocess.STDOUT,timeout=timeout)
 return {'name':name,'command':cmd,'cwd':str(cwd),'returncode':p.returncode,'elapsed_s':time.monotonic()-start,'log':str((O/(name+'.log')).relative_to(R))}
def native():
 rows=[]
 red=R/'out/G5-UR2a-mask-isolation-p36-r1/causal-red-binary'
 assert digest(red)=='75d25beccc00e8d3fa5884db43e39a0e74d6717a930fba36c98d9b5a36db0c11'
 for mode in ['direct','prepared']:
  row=run('causal-red-'+mode,[str(red),mode]);assert row['returncode']==1;rows.append(row)
 red_rows=rows;rows=[]
 for t in read(B/'native-final-registered.json')['tests']:
  props={x['name']:x['value'] for x in t['properties']}
  rows.append(run('native-'+t['name'],t['command'],props['WORKING_DIRECTORY'],max(180,props.get('TIMEOUT',0))))
 props={'WORKING_DIRECTORY':str(R)}
 v=read(R/'.aegis/packages/G5-UR2a-mask-isolation-p36-r1/verification.lock.json')
 command=[str(R/'out/G5-UR2a-mask-isolation-p36-r1/build-native/runtime/composition/axiom_fc02_text_tests'),v['native']['additional_GTest_filter']]
 targeted=run('targeted-GTest',command);assert targeted['returncode']==0
 return {'causal_RED':red_rows,'targeted_GTest':targeted,'name':'native' ,'cases':rows,'passed':sum(x['returncode']==0 for x in rows),'failed':sum(x['returncode']!=0 for x in rows),'registered':len(rows),'method':'Run exact registered commands without rewriting producer CTest logs'}
python_files=['test_g5_canonical_prepared_body_working_set.py','test_g5_transient_reference.py','test_g5_prepared_body_index_reverify.py','test_g5_render_execution.py','test_g5_transient_attribution.py','test_g5_canonical_prepared_body.py','test_g5_prepared_body_index.py','test_g5_resource_recovery.py','test_g5_resource_recovery_loss_reverify.py','test_g5_ur2a_handoff.py']
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
 jobs=[pool.submit(native),pool.submit(run,'node',read(B/'node-command.json')),pool.submit(run,'python',['python3.12','-m','unittest',*[str(pathlib.Path('verification/tests')/n) for n in python_files]]),pool.submit(run,'boundaries',['python3.12','tools/check_runtime_boundaries.py','--root','.'])]
 rows=[x.result() for x in jobs]
for x in artifacts:assert digest(R/x['path'])==x['sha256']
out={'status':'VERIFIED' if rows[0]['failed']==0 and all(x['returncode']==0 for x in rows[1:]) else 'FAILED','browser_starts':0,'new_builds':0,'binary_hashes_unchanged':True,'results':rows}
(O/'regressions.json').write_text(json.dumps(out,indent=2)+'\n')
print(json.dumps({'status':out['status'],'native':rows[0]['passed'],'other_returncodes':[x['returncode'] for x in rows[1:]],'browser_starts':0,'native_registered':rows[0]['registered']}))
raise SystemExit(out['status']!='VERIFIED')
