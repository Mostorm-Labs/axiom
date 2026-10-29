#!/usr/bin/env python3
"""P31 structural/evidence checks only: no assertion that P32 is implemented."""
import argparse, hashlib, json, pathlib, subprocess, sys, traceback, xml.etree.ElementTree as ET

P = pathlib.Path(__file__).resolve().parent
R = P.parents[2]
ANCHOR='15e76458f3872be4d1b51d4aac05eb8402719014'
FEATURE='c7f874395d55952e9d5288e2a336a21747f37f11'
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def read(p): return json.loads(p.read_text(encoding='utf-8'))
def cmd(*args): return subprocess.check_output(args,cwd=R,text=True).strip()
def require(v,msg):
 if not v: raise AssertionError(msg)
def git_ancestor(a,b): subprocess.run(['git','merge-base','--is-ancestor',a,b],cwd=R,check=True)
def locked_file(item):
 p=R/item['path']; require(p.is_file(),'missing '+str(p)); require(sha(p)==item['sha256'],'hash mismatch '+str(p))
def structural_common():
 package=read(P/'package.json'); lock=read(P/'source-lock.json'); ilock=read(P/'input-lock.json'); integrity=read(P/'integrity.json')
 require(package['task_anchor']['revision']==ANCHOR and lock['subject']==ANCHOR,'anchor mismatch')
 require(package['feature_anchor']['revision']==FEATURE,'feature anchor mismatch')
 for x in lock['files']+ilock['historical_inputs']: locked_file(x)
 for x in integrity['files']:
  f=P/x['path']; require(f.is_file() and sha(f)==x['sha256'],'package integrity '+str(f))
 return package,lock,ilock
def package_binding():
 package,lock,ilock=structural_common()
 require(cmd('git','branch','--show-current')=='codex/fc04-d07-selection-audit','execution branch mismatch')
 remote=cmd('git','remote','get-url','origin').removesuffix('.git').lower()
 require(remote in ('https://github.com/mostorm-labs/axiom','git@github.com:mostorm-labs/axiom'),'repository identity mismatch '+remote)
 head=cmd('git','rev-parse','HEAD'); git_ancestor(ANCHOR,head);git_ancestor(FEATURE,head);git_ancestor('832866a2d001a247c4eb374c1fb1899258a6f6cf',head)
 require(package['product_P32_authorized_if_four_preflights_pass'] is True,'authorization binding')
 require(package['inherited_package']['sha256']=='56962b59726eca31f3cb015779234df84926bbbac427b382e285aea8dd70aa77','v0.1 identity')
 for key,expected in [('current.json','3f94c57a590c81b8ad8ceb2239759cba'),('acceptance.json','4064c57a590c819aabc5e9efbf9a2b53'),('integration-07-14.json','3c44c57a590c81028a5ff4a974c9240d'),('scene-correction.json','4064c57a590c8175b733ee21089f122f')]:
  a=read(P/'authority'/key); require(expected in json.dumps(a),'authority page identity '+key)
 print('PASS: exact refs, ancestry, source/input/package hashes, authority identities and inherited package bound; HEAD='+head)
def evidence():
 structural_common(); result=read(R/'.aegis/results/FC04-D07-AB-P32-001/result.json')
 require(result['terminal_status']=='TASK_PACKAGE_DEFECT','historical blocker changed')
 require(result['native_d07']['count']==17 and result['native_d07']['passed']==16 and len(result['native_d07']['failed'])==1 and result['native_d07']['skipped']==0,'D07 historical result mismatch')
 require(result['native_selection']['count']==160 and result['native_selection']['passed']==160 and result['native_selection']['skipped']==0,'selection historical result mismatch')
 require(result['single_command_oom']=={'baseline_exit':86,'fixed_exit':0},'OOM evidence mismatch')
 manifest=read(R/'.aegis/results/FC04-D07-AB-P32-001/manifest.json')
 for x in manifest['files']: locked_file({'path':'.aegis/results/FC04-D07-AB-P32-001/'+x['path'],'sha256':x['sha256']})
 d=ET.parse(R/'.aegis/results/FC04-D07-AB-P32-001/raw/d07-final.xml').getroot(); s=ET.parse(R/'.aegis/results/FC04-D07-AB-P32-001/raw/selection-final.xml').getroot()
 dt=d.findall('.//testcase'); st=s.findall('.//testcase')
 require(len(dt)==17 and len(st)==160,'JUnit test counts')
 failures=[t.attrib['name'] for t in dt if t.find('failure') is not None or t.find('error') is not None]
 require(failures==['fc04.d07.D07.ExplicitIntentGroupProducesOneUndoUnit'],'group failure not preserved: '+str(failures))
 require(not any(t.find('failure') is not None or t.find('error') is not None or t.find('skipped') is not None for t in st),'selection raw failure/skip')
 names=(R/'.aegis/results/FC04-D07-P31-CORRECTION-v0.2/raw/selection-names.txt').read_text().splitlines()
 require(len(names)==160 and len(set(names))==160 and set(names)=={t.attrib['name'] for t in st},'selection IDs mismatch')
 dnames=(R/'.aegis/results/FC04-D07-P31-CORRECTION-v0.2/raw/d07-names.txt').read_text().splitlines()
 require(len(dnames)==14 and len(set(dnames))==14 and set(dnames)<={t.attrib['name'] for t in dt},'historical D07 IDs mismatch')
 print('PASS: immutable historical manifest, exact 160 Selection and 14 D07 IDs, raw 17/16/1 group failure, blocking OOM evidence; no new product result implied')
def design():
 structural_common(); ctx=read(P/'implementation-context.json')
 expected={'runtime/scene/include/canvas/scene/scene_record_store.hpp','runtime/scene/src/scene_record_store.cpp','runtime/scene/src/direct_render_scene.cpp'}
 require(set(ctx['activated_conditional_owners'])==expected,'conditional scope')
 store=(R/'runtime/scene/src/scene_record_store.cpp').read_text(); render=(R/'runtime/scene/src/direct_render_scene.cpp').read_text(); scene=(R/'runtime/scene/src/scene.cpp').read_text()
 storecommit=store[store.index('SceneRecordStore::commit('):]; rendercommit=render[render.index('DirectRenderScene::commit('):]
 require(all(x in storecommit for x in ['noexcept','_arena.emplace','_orderIndex.emplace','_handles[']),'record store finding no longer matches source')
 require(all(x in rendercommit for x in ['noexcept','_records.push_back','_index[']),'render finding no longer matches source')
 require('publishStagedSnapshot() noexcept' in scene and '_stablePublishedInvalidation = _publishedInvalidation' in scene,'Scene no-fail finding mismatch')
 sink=(R/'runtime/interaction/include/canvas/interaction/local_operation_sink.hpp').read_text(); require('applied(' in sink and 'CanonicalCommitRecord' in sink,'real bridge owner mismatch')
 cmake=(R/'runtime/scene/tests/CMakeLists.txt').read_text(); require('canvas_g2_a6_atomic_publication' in cmake and 'incremental_runtime_atomic_publication_test.cpp' in cmake,'test target binding')
 print('PASS: source findings and conditional participants bound; 07-14 actual callback compatibility is NOT_VERIFIED pending P32 real-owner tests; stop on actual authority conflict')
def d07():
 structural_common(); matrix=read(P/'test-matrix.json'); delta=read(P/'input/03-verification-matrix-delta.json')
 require(matrix['scene_oracles']==delta['new_scene_oracles'],'scene oracle changed')
 ids=[x['id'] for x in matrix['scene_oracles']]; require(ids==['SCENE-PREP-%02d'%n for n in range(1,7)]+['D07-GROUP-POSITIVE','D07-OOM-BLOCKING'],'logical IDs')
 require(matrix['scene_case_binding']['status']=='PLANNED_NOT_RUN' and matrix['test_creation_status']=='NOT_RUN_PENDING_P32','false execution claim')
 tests={x['id']:x for x in matrix['tests']}
 for name in ['fc04.d07.D07.ExplicitIntentGroupProducesOneUndoUnit','fc04.d07.D07.TwoIndependentCommandsNeedTwoUndoUnits','fc04.d07.D07.NaiveSequentialFallbackCannotSatisfyOneUndo_RED','FC04D07PostcommitReceiptGrowthFault','FC04D07PostcommitFault']:
  require(name in tests,'missing required binding '+name)
 require(matrix['frozen_regressions']['selection_160'].endswith('selection-names.txt') and matrix['frozen_regressions']['preserved_passing_count']==11,'frozen regression binding')
 require(matrix['no_zero_match_as_pass'] is True,'zero match')
 print('PASS: SCENE-PREP-01..06, real intent, separate Undo, negative control, OOM and frozen regressions bound; all new product tests NOT_RUN')

if __name__=='__main__':
 ap=argparse.ArgumentParser();ap.add_argument('--repo',required=True);ap.add_argument('--check',required=True,choices=['PackageBindingPreflight','EvidenceContractPreflight','DesignPreflight','D07PackagePreflight']);args=ap.parse_args()
 try:
  require(pathlib.Path(args.repo).resolve()==R,'--repo must identify actual worktree root')
  {'PackageBindingPreflight':package_binding,'EvidenceContractPreflight':evidence,'DesignPreflight':design,'D07PackagePreflight':d07}[args.check]()
 except Exception:
  traceback.print_exc();sys.exit(1)
