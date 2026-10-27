"""Accepted H03 input overlay; independent construction, never a product inverse."""
import hashlib,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tests/fc08/reference'))
from fixtures import record,key,operation,topology
from wire import Writer
OVERLAY=ROOT/'.aegis/packages/FC-08-SECTION-C-H03-v0.1/input-overlay.json'
OUTPUT=ROOT/'out/fc08-section-c/h03-input-overlay-v0.1'
def sha(data):return hashlib.sha256(data).hexdigest()
def root_key(ordinal):
 if not 1<=ordinal<=65536:raise ValueError('H03 ordinal outside the accepted recipe')
 return key(ordinal)+'01'
def validate_refs(overlay):
 if overlay['status']!='ACCEPTED_BY_USER_FOR_C_VERIFICATION':raise ValueError('overlay is not accepted')
 for v in overlay['variants']:
  for ref in [v['original_input'],v['expected']]:
   path=(ROOT/ref['path']).resolve()
   if not path.is_relative_to(ROOT) or sha(path.read_bytes())!=ref['sha256']:raise ValueError('immutable input/expected binding mismatch: '+ref['path'])
  if v['recipe']['N'] not in [65534,65535] or v['recipe']['section_id']!=90000:raise ValueError('recipe scope changed')
  if v['expected_command_admitted']!=(v['recipe']['N']==65534):raise ValueError('expected admission changed')
def write_once(path,data):
 path.parent.mkdir(parents=True,exist_ok=True)
 try:
  with path.open('xb') as f:f.write(data)
 except FileExistsError:
  if path.read_bytes()!=data:raise ValueError('refusing to overwrite existing input: '+str(path))
def verify_wire(variant,label,data):
 bound=next(v for v in variant['active_wire'] if v['label']==label)
 if sha(data)!=bound['sha256'] or len(data)!=bound['size']:raise ValueError('accepted wire identity mismatch: '+variant['variant_id']+'/'+label)
 return {'label':label,'sha256':sha(data),'size':len(data)}
def materialize(output=OUTPUT):
 overlay=json.loads(OVERLAY.read_text(encoding='utf-8'));validate_refs(overlay);output=Path(output);receipts=[]
 for v in overlay['variants']:
  n=v['recipe']['N'];name=v['variant_id'].split('/')[1]
  before=[record(100+i,'Shape',key=root_key(i+1)) for i in range(n)]
  after=before+[record(90000,'Section',key=root_key(n+1))]
  # Use the frozen independent builder/Writer, not the DUT codec or planner.
  hashes=[]
  for label,model in [('F',topology(before,after)),('U',topology(after,before,1002))]:
   data=Writer().encode('Operation',model);hashes.append(verify_wire(v,label,data))
   write_once(output/(name+('.pb' if label=='F' else '.reference-u.pb')),data)
  data=Writer().encode('Operation',operation({'insert_objects':{'objects':before}},999))
  write_once(output/(name+'.seed.pb'),data)
  hashes.append({'label':'independent-before-seed','sha256':sha(data),'size':len(data)})
  receipts.append({'variant_id':v['variant_id'],'input_version':'H03-v0.1','overlay_sha256':sha(OVERLAY.read_bytes()),'original_input':v['original_input'],'expected':v['expected'],'wire_hashes':hashes,'root_members_before':n,'inverse_complete_root_members':n+1,'all_key_lengths':5,'all_key_terminal_byte':1,'SUT_inverse':'Actual EditorHistory retained inverse only; reference U file is never submitted'})
 return receipts
if __name__=='__main__':print(json.dumps(materialize(),indent=2))
