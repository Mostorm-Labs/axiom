"""Bind existing owners and accepted target flow, without implementing product state."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

def main():
    p=argparse.ArgumentParser();p.add_argument('--repo',required=True,type=Path);p.add_argument('--output',required=True,type=Path);a=p.parse_args()
    root=a.repo.resolve();out=a.output
    prior=json.loads((root/'.aegis/results/FC-08-P31-BOOTSTRAP/attempts/bootstrap-002/implementation-design-preflight.json').read_text(encoding='utf-8'))
    for binding in prior['source_bindings'].values():
        data=(root/binding['path']).read_bytes()
        assert hashlib.sha256(data).hexdigest()==binding['sha256'],binding['path']
        frozen=subprocess.check_output(['D:/Softwares/Git/cmd/git.exe','show',prior['production_subject_revision']+':'+binding['path']],cwd=root)
        assert data==frozen
    package='.aegis/packages/FC-08-P31-BOOTSTRAP-v0.2'
    approval=json.loads((root/package/'acceptance.json').read_text(encoding='utf-8'))
    assert approval['accepted'] and not approval['product_P32_authorized']
    report={**prior,'status':'PASS_BOOTSTRAP_DESIGN_BINDING',
            'basis':'Exact accepted P20 correction/profile matrix plus unchanged P12-P17 ownership. This is verification-bootstrap design, not implementation of future Section transactions.',
            'unresolved_verification_decisions':[],'unresolved_semantic_decisions':[],
            'authority_conflict':False,'verification_conflict':False,'package_scope_conflict':False,
            'may_continue_verification_bootstrap':True,'may_continue_product_P32':False,
            'acceptance':{'path':package+'/acceptance.json','sha256':hashlib.sha256((root/package/'acceptance.json').read_bytes()).hexdigest()},
            'test_plan':['Use frozen independent predicates, never record expected from SUT',
                         'Actually invoke normal codec/ingress in Native and Chrome/WASM; report earliest real failure',
                         'Keep missing UI/History/Scene observation fields NOT_RUN; no manufactured canonical state or traces',
                         'Native/headless binaries and actual browser WASM remain separate, with explicit actual environment and subject bindings'],
            'verification_adapter_owner':'New Python runner and browser transport own invocation/evidence only; no substitute Section, topology, F/U, History, layout, Scene or selection implementation.'}
    for mapping in report['obligation_to_owner_bindings'].values():
        mapping['test_binding']='Accepted corpus/matrix and frozen predicates; new actual-runner observations must declare supported and missing fields per variant. Existing owner source identity is not product PASS.'
    out.parent.mkdir(parents=True,exist_ok=True)
    with out.open('x',encoding='utf-8',newline='\n') as stream:stream.write(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
    print(report['status'])

if __name__=='__main__':main()
