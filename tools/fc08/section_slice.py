"""Materialize and preflight the directly user-authorized Section A slice."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

ROOT=Path(__file__).resolve().parents[2]
BASE='9d731f40515d69959c3d3941d889e062aa6bcdcf'
PKG='.aegis/packages/FC-08-SECTION-A-v0.1'
GIT='D:/Softwares/Git/cmd/git.exe'
ALLOWED=[
 'schema/axiom/v1/proto/auditoryworks/axiom/v1/section.proto',
 'schema/axiom/v1/proto/auditoryworks/axiom/v1/container_operations.proto',
 'schema/axiom/v1/proto/auditoryworks/axiom/v1/object.proto',
 'schema/axiom/v1/proto/auditoryworks/axiom/v1/operation.proto',
 'schema/axiom/v1/registry/object_kind_registry_v1.yaml',
 'schema/axiom/v1/registry/operation_registry_v1.yaml',
 'schema/axiom/v1/registry/field_registry_v1.yaml',
 'schema/axiom/v1/descriptor/descriptor.current.pb',
 'schema/axiom/v1/descriptor/descriptor.current.sha256',
 'runtime/semantic/include/canvas/semantic/section_content.hpp',
 'runtime/semantic/include/canvas/semantic/object_content.hpp',
 'runtime/semantic/include/canvas/semantic/object_record.hpp',
 'runtime/semantic/src/semantic_types.cpp','runtime/semantic/src/protobuf_object_mapping.cpp',
 'runtime/semantic/src/protobuf_object_mapping.hpp','runtime/semantic/src/codec.cpp',
 'runtime/semantic/src/normalizer.cpp','runtime/semantic/src/validator.cpp',
 'runtime/semantic/src/hierarchy_capability_validation.cpp',
 'runtime/semantic/src/operation_specific_validation.cpp',
 'runtime/semantic/src/snapshot.cpp','runtime/semantic/src/snapshot_digest.cpp',
 'runtime/semantic/src/snapshot_bootstrap.cpp','runtime/semantic/include/canvas/semantic/snapshot.hpp',
 'runtime/semantic/src/stateful_validation.cpp','runtime/semantic/src/field_registry_v1.cpp',
 'runtime/semantic/tests/CMakeLists.txt','runtime/semantic/tests/fc08_section_codec_test.cpp',
 'runtime/semantic/tests/semantic_types_test.cpp',
 'verification/tests/test_current_semantic_descriptor.py','verification/tools/validate_semantic_contract.py',
 'tools/fc08/section_slice.py','tools/fc08/section_a_*.py',
 '.aegis/packages/FC-08-SECTION-A-v0.1/**','.aegis/results/FC-08-SECTION-A/**']

def ref(path):
 data=(ROOT/path).read_bytes();return {'path':path,'sha256':hashlib.sha256(data).hexdigest()}
def save(path,value):
 p=ROOT/path;p.parent.mkdir(parents=True,exist_ok=True)
 with p.open('x',encoding='utf-8',newline='\n') as f:f.write(value if isinstance(value,str) else json.dumps(value,ensure_ascii=False,indent=2)+'\n')
 return ref(path)
def materialize():
 prior=json.loads((ROOT/'.aegis/packages/FC-08-PRODUCT-P31-v0.1/authority.lock.json').read_text())
 authority=save(PKG+'/authority.lock.json',{'inherited_accepted_authority':ref('.aegis/packages/FC-08-PRODUCT-P31-v0.1/authority.lock.json'),
  'accepted_sources':[ref('.aegis/materialized/FC-08/sources/accepted-exports/'+s+'.md') for s in ['P12','P13','P14','P15','P16','P17','P20']],
  'semantic_identity':prior['current_authorities']['semantic_decisions']['section_identity'],
  'control_update_scope':'Direct user authorization supersedes the full-runner-before-any-product sequencing condition for this A slice only; no global authority/gate/registry verdict changes.'})
 acceptance=save(PKG+'/authorization.json',{'source':'Direct human instruction: FC-08 Section — Start Implementation Using the Existing Runtime',
  'instruction_excerpt':'Make the smallest necessary, versioned control update to authorize the first bounded Section implementation slice, using this user instruction as the requested implementation direction.',
  'sequencing_excerpt':'The full product runner can be completed alongside implementation as real observation seams become available',
  'product_P32_authorized':True,'authorized_slice':'A: schema, SectionContent/1, canonical identity, domain/presence/normalization and codec tests',
  'not_claimed':['Original f67 product package flag changed','Independent DEV/P34 approval','B-E implementation authorization from this package'],
  'source_revision':BASE,'execution_ref':'refs/heads/codex/fc08-section-foundation'})
 verification=save(PKG+'/verification.lock.json',{
  'design':'FC08-P20-v0.1+CR01 plus accepted eb12 input/matrix amendment',
  'accepted_amendment':ref('.aegis/packages/FC-08-P20-P31-INPUT-AMENDMENT-v0.2/package.json'),
  'bootstrap_acceptance':ref('.aegis/packages/FC-08-P31-BOOTSTRAP-v0.2/acceptance.json'),
  'bootstrap_result':ref('.aegis/results/FC-08-P31-BOOTSTRAP/attempts/bootstrap-003/terminal-result.json'),
  'successful_preflights':[ref('.aegis/results/FC-08-P31-BOOTSTRAP/attempts/bootstrap-003/qualified-001/'+s+'.json') for s in ['package-preflight','evidence-preflight','implementation-design-preflight']],
  'A_oracle':{'requirement':'Accepted Section/1 identity, content domain and lossless canonical codec',
              'inputs':'Frozen W02 41 variants; exact independent Section record/insert carriers; existing codec/normalizer/validation regressions',
              'oracle':'Unmodified frozen expected booleans, independent raw wire and literal model preimages; no DUT-written expected',
              'execution':'Real SemanticCodec / normal operation ingress / normalizer / validation. SCW06 uses model-to-writer, not invented raw permissiveness.',
              'evidence':'Exact CTest cases/exit codes/raw logs, descriptor reproducibility and preservation, compiler/SDK identities'},
  'remaining_E01_E10':'Retained. B-E operations/History/Scene/host assertions stay NOT_RUN, not prerequisites to writing A.',
  'full_product_runner_complete':False})
 closure=save(PKG+'/execution-contract.json',{'allowed_files':ALLOWED,'source_revision':BASE,
  'required':['Publish accepted additive IDL and registry identities; preserve existing fields and historical descriptor locks',
              'Add SectionContent and ObjectKind10/version1 to existing canonical model, mapper, normalizer and domain validation',
              'Enforce mandatory presence, valid UTF8, finite/positive boundary dimensions, finite endpoints, nonnegative padding/header, negative-zero normalization and Section property/mask applicability',
              'Run real RED before implementation and GREEN on A frozen inputs plus touched existing regressions',
              'Persist exact revision, raw results and explicit B-E NOT_RUN'],
  'schema_vs_behavior':'Publish accepted17/18 IDL tags only; canonical operation variants/OperationEngine17/18 behavior belong to B and remain unsupported in A.',
  'forbidden':['Historical descriptor.lock files','Frozen FC08 inputs/expected/oracles/prior results','Qualification hook semantics/default OFF','Global Aegis registries/default branch','RuntimeFacade/History/Scene/host implementation in this A slice','SDK lock/source rebuild','Unrelated producer behavior changes'],
  'terminal_success':['A real tests and relevant regressions pass','Current descriptor additive/reproducible; historical locks unchanged','No forbidden source/evidence changes','Reviewer-accessible exact result'],
  'terminal_blockers':['AUTHORITY_CONFLICT','ENVIRONMENT_BLOCKER','FROZEN_VERIFICATION_FAILURE','UNRESOLVED_SEMANTIC_DECISION'],
  'continue_until_terminal_state':True})
 evidence=save(PKG+'/evidence-contract.json',{'scope':'A implementation evidence only','required':['Authorization exact revision and source bindings','Real baseline RED and A GREEN','Frozen W02 mapping and explicit remaining NOT_RUN','Build/test argv, exit codes, toolchain facts','Old source/descriptor lock/evidence preservation','Exact result revision and materialized ref'],
  'product_P32_authorized':'A only','DEV_PASS':False,'P34_PASS':False,'PHYS_PASS':False})
 context=save(PKG+'/implementation-context.md','# Section A\n\nUse existing Semantic object/content, protobuf mapper, codec, normalizer and validator. Append the new content alternative without changing old indexes. Keep Group identity and membership unchanged. New17/18 schema allocation is not executable17/18 support until B. Existing Scene/Facade/History remain untouched. Full runner completeness follows real implementation under the direct user sequencing instruction.\n')
 package={'format':'aegis-repo-local-execution-package-v1','package_id':'FC-08-SECTION-A-v0.1','task_id':'FC-08-SECTION-A',
  'repository':{'provider':'github','full_name':'Mostorm-Labs/axiom'},'source_revision':BASE,
  'execution_ref':'refs/heads/codex/fc08-section-foundation','task_anchor':{'revision':BASE,'relation':'ancestor'},
  'status':'AUTHORIZED_BY_EXPLICIT_USER_DIRECTION','product_P32_authorized':True,'authorized_slice':'A only',
  'authority_lock_ref':authority,'authorization_ref':acceptance,'verification_lock_ref':verification,
  'execution_contract_ref':closure,'evidence_contract_ref':evidence,'implementation_context_ref':context,
  'continue_until_terminal_state':True,'return_surface':'CONTROL_REVIEW'}
 print(json.dumps(save(PKG+'/package.json',package),indent=2))

def preflight(revision,phase,output):
 errors=[];checked=[]
 try:
  def git(*args):return subprocess.check_output([GIT,*args],cwd=ROOT).decode().strip()
  assert git('remote','get-url','origin')=='https://github.com/Mostorm-Labs/axiom.git'
  assert len(revision)==40
  subprocess.run([GIT,'merge-base','--is-ancestor',BASE,'HEAD'],cwd=ROOT,check=True)
  package=json.loads((ROOT/PKG/'package.json').read_text())
  assert package['product_P32_authorized'] and package['authorized_slice']=='A only'
  def check(value):
   if isinstance(value,dict):
    if 'path' in value and 'sha256' in value:
     path=(ROOT/value['path']).resolve();assert path.is_relative_to(ROOT)
     assert hashlib.sha256(path.read_bytes()).hexdigest()==value['sha256'],value['path'];checked.append(value['path'])
    for v in value.values():check(v)
   elif isinstance(value,list):
    for v in value:check(v)
  check(package)
  for name in ['package.json','authorization.json','authority.lock.json','verification.lock.json','execution-contract.json','evidence-contract.json','implementation-context.md']:
   data=(ROOT/PKG/name).read_bytes();assert data==subprocess.check_output([GIT,'show',revision+':'+PKG+'/'+name],cwd=ROOT)
   if name.endswith('.json'):check(json.loads(data))
  assert hashlib.sha256((ROOT/'schema/axiom/v1/descriptor/descriptor.lock.pb').read_bytes()).hexdigest()=='8c1da1c19b754f9345472f96452e58505ebd18c0874b04480e378eb95a38654d'
 except (AssertionError,ValueError,OSError,subprocess.CalledProcessError) as exc:errors.append(str(exc))
 report={'preflight_name':'PackageBindingPreflight' if phase=='package' else 'EvidenceContractPreflight','package_revision':revision,
         'status':'PASS_SLICE_A_'+phase.upper() if not errors else 'BLOCKED','errors':errors,'verified_paths':checked,
         'product_P32_authorized':'A only','full_302_runner_required_before_A':False}
 save(output,report);print(json.dumps(report));return int(bool(errors))

if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('action',choices=['materialize','preflight']);p.add_argument('--repo',required=True);p.add_argument('--revision');p.add_argument('--phase',choices=['package','evidence']);p.add_argument('--output');a=p.parse_args()
 assert Path(a.repo).resolve()==ROOT
 if a.action=='materialize':materialize()
 else:raise SystemExit(preflight(a.revision,a.phase,a.output))
