"""r2 input correction and four-run continuation of the existing narrow runner."""
import argparse
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil

ROOT=Path(__file__).resolve().parents[3]
HERE=Path(__file__).resolve().parent
IDS=('UR1ATTR2-A1','UR1ATTR2-B1','UR1ATTR2-B2','UR1ATTR2-A2')
PACKAGE_SHA='51ba367a8e5243b9b1fb5633071f79fed11a403f4475dd29590845528020e127'
R1=ROOT/'.aegis/results/G5-UR1a-attribution-r1/local-comparison'

def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()

def custody_allocation(fixtures):
    documents={s['state']['documentId'] for n,s in fixtures.items() if n in ('S0','S2')}
    if len(documents)!=1:raise ValueError('foreign fixture document')
    counter=0
    return {'task':'G5-UR1a-attribution-r2','issuer':'verification-fixture-data-owner',
            'documentId':documents.pop(),'counter_before':str(counter),'binding':str(counter+1),
            'snapshot_sha256':{n:s['sha256'] for n,s in fixtures.items() if n in ('S0','S2')},
            'scope':'new caller-issued custody input for this owned fixture set; not a source SDK readback'}

def derive_envelope(source,allocation,config):
    if (source['state']['documentId']!=allocation['documentId'] or source['sha256']!=allocation['snapshot_sha256'][source['name']]
            or allocation['binding']!='1' or source['metadata']['resourceBinding']!='0'):
        raise ValueError('source/caller binding mismatch')
    s=copy.deepcopy(source);s['source_capture_metadata']=copy.deepcopy(source['metadata'])
    s['metadata']['resourceBinding']=allocation['binding'];s['restore_custody']=copy.deepcopy(allocation);s['expected_config']=copy.deepcopy(config)
    return s

def prepare(output):
    package=ROOT/'.aegis/packages/G5-UR1a-attribution-r2/package.json'
    if digest(package)!=PACKAGE_SHA:raise ValueError('r2 package identity')
    p=json.loads(package.read_text())
    # These references are immutable inputs. The driver paths are explicitly
    # authorized to change and receive a fresh execution freeze, not an old hash.
    for r in p['authority']['existing_refs']:
        if r['path'].startswith('verification/runners/g5/transient_attribution'):continue
        if digest(ROOT/r['path'])!=r['sha256']:raise ValueError('authority/input identity: '+r['path'])
    if (output/'sampling-ledger.json').exists():raise ValueError('r2 window already reserved')
    fixtures=json.loads((R1/'fixtures/fixtures.json').read_text())
    config=json.loads((R1/'UR1ATTR-SEED-A0/init.json').read_text())['config']
    # Bind the seed's public config readback to its independently verified archive.
    manifest=ROOT/'verification/evidence/performance/g5-ur1a-attribution-r1/84174b75cf1784efc801c6037d19bcde46622192e052e3bd57a6ea301f06d429/execution-r1/manifest.json'
    item=json.loads(manifest.read_text())['files']['result/local-comparison/UR1ATTR-SEED-A0/init.json']
    if digest(R1/'UR1ATTR-SEED-A0/init.json')!=item['sha256']:raise ValueError('seed config identity')
    allocation=custody_allocation(fixtures);directory=output/'fixtures';directory.mkdir(parents=True,exist_ok=False)
    derived={'head':fixtures['head']}
    for name in ('S0','S2'):
        source=fixtures[name]
        if digest(R1/'fixtures'/f'{name}.axsnap')!=source['sha256']:raise ValueError('snapshot byte identity')
        derived[name]=derive_envelope(source,allocation,config)
        shutil.copy2(R1/'fixtures'/f'{name}.axsnap',directory/f'{name}.axsnap')
    (directory/'fixtures.json').write_text(json.dumps(derived,indent=2)+'\n')
    (output/'restore-custody-allocation.json').write_text(json.dumps(allocation,indent=2)+'\n')
    (output/'source-capture-metadata.json').write_text(json.dumps({n:fixtures[n]['metadata'] for n in ('S0','S2')},indent=2)+'\n')
    return directory

def main():
    p=argparse.ArgumentParser()
    for name in ('a-sdk','b-sdk','oracle','output'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--execute',action='store_true');a=p.parse_args()
    if not a.execute:p.error('--execute required')
    a.output.mkdir(parents=True,exist_ok=True);directory=prepare(a.output)
    spec=importlib.util.spec_from_file_location('attribution_runner',HERE/'transient_attribution_runner.py')
    runner=importlib.util.module_from_spec(spec);spec.loader.exec_module(runner)
    runner.execute(a.a_sdk,a.b_sdk,a.oracle,a.output,ids=IDS,historical_starts=38,historical_seconds=1328,fixtures=directory)

if __name__=='__main__':main()
