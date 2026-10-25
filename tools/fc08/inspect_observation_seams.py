"""Read-only inventory: source-bound existing and missing observation seams."""
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT/'.aegis/materialized/FC-08'
GIT = 'D:/Softwares/Git/cmd/git.exe'


def source(path):
    data = (ROOT/path).read_bytes()
    return {'path':path,'sha256':hashlib.sha256(data).hexdigest(),
            'git_blob':subprocess.check_output([GIT,'rev-parse','HEAD:'+path],cwd=ROOT,text=True).strip()}


def main():
    rows = [
        {'role':'raw operation ingress','status':'BOUND',
         'interface':'SemanticCodec::decodeProtobufOperation',
         'source':source('runtime/semantic/include/canvas/semantic/codec.hpp')},
        {'role':'raw snapshot ingress','status':'BOUND',
         'interface':'SnapshotCodec::decode',
         'source':source('runtime/semantic/include/canvas/semantic/snapshot.hpp')},
        {'role':'ordinary public consumer','status':'BOUND_BASELINE_CONTAINER_API_ABSENT',
         'interface':'RuntimeComposition::controlFacade() -> RuntimeFacade',
         'future_interface':'P15 section2 submitContainerIntent/readContainerState/readContainerReceipt',
         'source':source('runtime/foundation/include/canvas/runtime/runtime_facade.hpp')},
        {'role':'Scene fault/publication observer','status':'EXISTING_QUALIFICATION_SEAM',
         'interface':'IncrementalRuntimeTestAccess::failAt/observe via existing friend',
         'source':source('runtime/scene/tests/incremental_runtime_test_access.hpp')},
        {'role':'canonical prepared apply owner','status':'READ_PREPARE_EXISTS_NO_CHECKPOINT_CALLBACK',
         'interface':'OperationEngine::prepare/apply',
         'source':source('runtime/semantic/include/canvas/semantic/operation_engine.hpp')},
        {'role':'History admission checkpoint','status':'NO_EXISTING_FAULT_OBSERVER',
         'interface':'EditorHistory::prepareRecord/commitPrepared (no callback or injection parameter)',
         'source':source('runtime/interaction/include/canvas/interaction/editor_history.hpp')},
    ]
    # This inventory does not equate a missing observation hook with a product
    # capability absence. Their closure policies are different in M03/M04.
    value = {'format':'fc08-observation-seam-inventory-v1','source_revision':
             subprocess.check_output([GIT,'rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
             'seams':rows,'product_P32_authorized':False}
    (OUT/'seam-inventory.json').write_text(json.dumps(value,indent=2)+'\n')
    print(json.dumps(value,indent=2))


if __name__ == '__main__': main()
