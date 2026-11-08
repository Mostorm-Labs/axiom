"""Seal this offline repair slice, preserving all closed sampling outcomes."""
import difflib,hashlib,importlib.util,json,re,shutil,subprocess,tarfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[4];RESULT=Path(__file__).resolve().parent;OUT=ROOT/'out/ur0b'
def load(p):return json.loads(Path(p).read_text())
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def write(p,v):p=Path(p);p.parent.mkdir(parents=True,exist_ok=True);p.write_text(json.dumps(v,indent=2)+'\n')
def copy(p,to):to=Path(to);to.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,to)
def git(*args):return subprocess.check_output(['git',*args],cwd=ROOT)
spec=importlib.util.spec_from_file_location('identity',ROOT/'verification/runners/g5/ur0/source_identity.py');identity=importlib.util.module_from_spec(spec);spec.loader.exec_module(identity)
source=identity.identity(ROOT);scope=load(RESULT/'source-scope-audit.json');tree=source['source_tree_sha256']
assert tree==scope['result_source_tree_sha256']
assert all(p.startswith(('verification/runners/g5/ur0/','verification/tests/g5_ur0_','verification/tests/test_g5_ur0_')) for p in scope['changed_paths'])
historical=[]
for result_dir in [ROOT/'.aegis/results/G5-UR0b',ROOT/'.aegis/results/G5-UR0b/p36-r1']:
 for name,value in load(result_dir/'output-sha256.json').items():
  path=result_dir/name if isinstance(value,str) else Path(value['path']);expected=value if isinstance(value,str) else value['sha256'];assert sha(path)==expected,(name,path)
 result=load(result_dir/'result.json');bundle=Path(result['materialized_ref']['path']);manifest=load(bundle/'manifest.json')
 assert sha(bundle/'manifest.json')==result['materialized_ref']['manifest_sha256']
 assert all(sha(bundle/name)==v['sha256'] for name,v in manifest['files'].items())
 historical.append({'result_path':str(result_dir/'result.json'),'result_sha256':sha(result_dir/'result.json'),'manifest_path':str(bundle/'manifest.json'),'manifest_sha256':sha(bundle/'manifest.json'),'file_count':len(manifest['files']),'all_bytes_unchanged':True})
reverified=load(RESULT/'offline-reverification.json');assert reverified['status']=='OFFLINE_REVERIFICATION_VERIFIED' and reverified['fresh_browser_workers']==0
positive=[r for r in reverified['rows'] if r['accepted_raw_count']]
assert all(r['accepted_raw_count']==r['qualified_decoded_count']==r['metric_receipt_count'] and r['canonical_chain_status']=='QUALIFIED' and r['sampling_promotion_rejected'] for r in positive)
witness=load(RESULT/'original-witness-green.json');assert witness['status']=='NOT_REPRODUCED' and witness['decoded_qualified_completions']==len(witness['accepted_raw_completions']) and not witness['missing'] and not witness['errors']
node=int(re.search(r'# pass (\d+)\n# fail 0',(OUT/'p36-r2-node-final.log').read_text())[1]);python=int(re.search(r'Ran (\d+) tests',(OUT/'p36-r2-python-final.log').read_text())[1])
assert (OUT/'p36-r2-python-final.log').read_text().rstrip().endswith('OK')
boundaries=int(re.search(r'passed \((\d+) contract files\)',(OUT/'p36-r2-boundaries-final.log').read_text())[1])
assert 'sampling authorization is closed' in (OUT/'p36-r2-closed-authority.log').read_text()
assert subprocess.run(['git','diff','--check'],cwd=ROOT,capture_output=True).returncode==0
BUNDLE=ROOT/'verification/evidence/performance/g5-unified-render-ur0b'/tree/'p36-r2';assert not BUNDLE.exists(),'never replace a sealed return';BUNDLE.mkdir(parents=True)
write(BUNDLE/'source/full-source-identity.json',source);write(BUNDLE/'source/repair-delta.json',scope)
(BUNDLE/'source/tracked.patch').write_bytes(git('diff','--binary','HEAD'))
untracked=[p for p in git('ls-files','--others','--exclude-standard','-z').decode().split('\0') if p and not p.startswith(identity.EXCLUDED)]
with tarfile.open(BUNDLE/'source/untracked-source.tar.gz','w:gz') as archive:
 for p in untracked:archive.add(ROOT/p,arcname=p,recursive=False)
old_bundle=Path(load(ROOT/'.aegis/results/G5-UR0b/p36-r1/result.json')['materialized_ref']['path'])
with tarfile.open(old_bundle/'source/untracked-source.tar.gz') as archive:
 patches=[]
 for name in scope['changed_paths']:
  try:before=archive.extractfile(name).read().decode();oldname='a/'+name
  except KeyError:before='';oldname='/dev/null'
  patches.extend(difflib.unified_diff(before.splitlines(True),(ROOT/name).read_text().splitlines(True),fromfile=oldname,tofile='b/'+name))
 (BUNDLE/'source/offline-repair-review.diff').write_text(''.join(patches))
write(BUNDLE/'source/git-status.json',{'revision':source['revision'],'branch':git('branch','--show-current').decode().strip(),'porcelain':git('status','--porcelain=v1').decode(),'HEAD_contains_uncommitted_changes':False})
for p in OUT.glob('p36-r2-*'):
 if p.is_file() and p.name!='p36-r2-materialization.log':copy(p,BUNDLE/'checks'/p.name)
shutil.copytree(OUT/'p36-r2/reverification',BUNDLE/'offline-reverification')
for p in RESULT.iterdir():
 if p.is_file():copy(p,BUNDLE/'control-and-proofs'/p.name)
for name in ['qualified-artifact-manifest.json','normal-artifact-manifest.json','worker-accounting.json']:
 copy(old_bundle/name,BUNDLE/'preserved-inputs'/name)
for name in ['package.json','scope-contract.json','execution-contract.json','verification.lock.json','authority.lock.json','evidence-contract.json']:
 copy(ROOT/'.aegis/packages/G5-UR0b'/name,BUNDLE/'authority'/name)
copy(ROOT/'.aegis/results/G5-UR0b/control-reverification-r1/repair-authorization.json',BUNDLE/'authority/closed-repair-authorization.json')
facts={'status':'OFFLINE_REPAIR_SLICE_VERIFIED','new_browser_workers':0,'completed_inputs':len(reverified['rows']),'positive_native_inputs':len(positive),
 'raw_accepted_completions':sum(r['accepted_raw_count'] for r in positive),'decoded_qualified_completions':sum(r['qualified_decoded_count'] for r in positive),'metric_receipts':sum(r['metric_receipt_count'] for r in positive),
 'node_passed':node,'python_passed':python,'boundary_contracts':boundaries,'native_cpp':'UNCHANGED; prior 22-test evidence retained, not rerun or relabeled as fresh',
 'runtime_sdk_build_dependency_authority_changed':False,'sampling_state':'CLOSED; no budget reset, retry or primary timing adoption'}
write(BUNDLE/'repair-verification.json',facts);write(BUNDLE/'historical-preservation.json',historical)
readme=f'''P36-r2：READY_FOR_CONTROL_REVIEW（离线 R4 修复切片）。

成功 attempt 现在由唯一 accepted provider submit 的实际 job/basis 与完整 owner、frame、Scene、camera、surface、metrics 关联。此前的全部准备尝试保留；缺少真实返回类型的尝试继续 MISSING，不推断 Pending。
新增 canonical-chain 对账与字节/编译器绑定。采样准入要求正向 canonical 链；仅 schema 合法、零丢样或旧 QUALIFIED 标签不能放行。OFFLINE_REVERIFICATION 不能提升为新采样。沿用已关闭的授权时，更换输出目录也会在创建 ledger/worker 前拒绝。

离线复验 {facts['completed_inputs']} 份原始记录：{facts['positive_native_inputs']} 份 ON 内的 {facts['raw_accepted_completions']} 条 accepted completion 与解码链及指标 receipt 全部一致；OFF 保持 NOT_OBSERVED。原始时序、终态、快照 SHA、产品错误和 SDK artifact 字节均未改变。旧失败 bundle 及结果保持原样。
新回归：{node} Node、{python} Python、{boundaries} 边界检查通过。C++/Runtime/SDK/锁未改，未重建 SDK；此前 22 项原生结果仅作为未变化的既有证据保留。

本轮没有新浏览器 worker、资格组或主矩阵，没有提交/推送，没有签发性能或 P34 PASS。完整 UR0b 仍待控制面复核并明确后续采样授权，随后完成原冻结的 qualification、realtime、六类 Web correctness 与主矩阵。不能把本修复切片的 READY 状态视作整体 UR0b 完成。
'''
(BUNDLE/'README.md').write_text(readme)
files={str(p.relative_to(BUNDLE)):{'sha256':sha(p),'bytes':p.stat().st_size} for p in sorted(BUNDLE.rglob('*')) if p.is_file()}
write(BUNDLE/'manifest.json',{'schema':'g5-ur0b-p36-r2-manifest-v1','source_revision':source['revision'],'source_tree_sha256':tree,'status':'READY_FOR_CONTROL_REVIEW','scope':'OFFLINE_R4_REPAIR_SLICE_ONLY','files':files})
result={'schema_version':1,'task_id':'G5-UR0b','stage':'P36-r2','status':'READY_FOR_CONTROL_REVIEW','scope':'OFFLINE_R4_REPAIR_AND_SAMPLING_ADMISSION_ONLY','parent_UR0b_status':'SAMPLING_CLOSED; complete qualification and remaining evidence not achieved',
 'starting_revision':load(RESULT/'preflight.json')['starting_revision'],'result_revision':source['revision'],'starting_source_tree_sha256':scope['base_source_tree_sha256'],'source_tree_sha256':tree,'head_contains_dirty_changes':False,
 'package_ref':{'path':'.aegis/packages/G5-UR0b/package.json','sha256':sha(ROOT/'.aegis/packages/G5-UR0b/package.json')},
 'current_user_authorization':'继续下一步: fix successful attempt linkage and sampling admission then return for control review; no new sampling window inferred',
 'materialized_ref':{'kind':'shared-controlled-worktree-content-addressed-bundle','path':str(BUNDLE),'manifest_sha256':sha(BUNDLE/'manifest.json')},'validation_results':facts,'scope_audit':scope,'historical_preservation':historical,
 'recorded_input_source_tree_sha256':'590b1565f9f69b52ed3ca8cdd19f8ca798ded09c9025917cbc47ba90b501e4f2','compiler_identity':'per analysis.compiler_files SHA256; input recording identity kept separate','primary_timings_adopted':False,'new_browser_workers':0,
 'open_boundaries':['Independent control review of this repair and the remaining sampling authority','Existing incomplete qualification preserved; no cohort reset or reuse as a complete six-pair result','Three realtime, six combined Web correctness and thirty main rows still unexecuted','Helper outcomes without real result evidence remain MISSING; no physical display or pure GPU timing claim','Existing product input rejection retained; no product behavior changed'],
 'next_action':'Review this offline repair slice; explicitly reconcile any future bounded sampling authorization before restarting workers','continue_execution':False,'official_gate_pass':False,'committed':False,'pushed':False}
write(RESULT/'result.json',result);(RESULT/'README.md').write_text(readme+'\nBundle: '+str(BUNDLE)+'\n')
checks={'source_tree_exact':identity.identity(ROOT)['source_tree_sha256']==tree,'only_seven_offline_paths_changed':len(scope['changed_paths'])==7 and not scope['runtime_sdk_build_dependency_authority_changes'],
 'all_bundle_hashes_match':all(sha(BUNDLE/name)==v['sha256'] for name,v in files.items()),'old_results_and_bundles_unchanged':all(r['all_bytes_unchanged'] for r in historical),
 'actual_completion_populations_agree':facts['raw_accepted_completions']==facts['decoded_qualified_completions']==facts['metric_receipts'],'no_new_sampling_or_gate_claim':result['new_browser_workers']==0 and not result['official_gate_pass'] and not result['primary_timings_adopted']}
assert all(checks.values()),checks
write(RESULT/'validation.json',{'status':'VERIFIED_REPAIR_RETURN','checks':checks,'bundle_file_count':len(files),'manifest_sha256':sha(BUNDLE/'manifest.json'),'result_sha256':sha(RESULT/'result.json')})
write(RESULT/'output-sha256.json',{'result.json':sha(RESULT/'result.json'),'README.md':sha(RESULT/'README.md'),'validation.json':sha(RESULT/'validation.json'),'manifest':{'path':str(BUNDLE/'manifest.json'),'sha256':sha(BUNDLE/'manifest.json')}})
print(json.dumps({'status':result['status'],'scope':result['scope'],'source_tree_sha256':tree,'manifest_sha256':sha(BUNDLE/'manifest.json'),'bundle':str(BUNDLE),'facts':facts}))
