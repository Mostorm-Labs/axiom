"""Reconstruct only scoped C budget inputs from the immutable independent writer."""
import hashlib,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tests/fc08/reference'))
from fixtures import budget_recipe,record,key,operation
from wire import Writer

def materialize():
 out=ROOT/'out/fc08-section-c/frozen-budgets';out.mkdir(parents=True,exist_ok=True)
 manifest=json.loads((ROOT/'.aegis/materialized/FC-08/attempts/v0.3-control-remediation-02/corpus-manifest.json').read_text(encoding='utf-8'))
 receipts=[]
 for row in manifest['variants']:
  if not (row['case_id']=='FC08-H03' or (row['case_id']=='FC08-H04' and '/operation-' in row['variant_id'])):continue
  name=row['variant_id'].split('/')[1]; values,recipe=budget_recipe(name)
  hashes=[]
  for label,root,data in values:
   frozen=next(v for v in row['wire'] if v['label']==label)
   digest=hashlib.sha256(data).hexdigest();assert digest==frozen['sha256'] and len(data)==frozen['size']
   hashes.append({'label':label,'sha256':digest,'size':len(data)})
   if label=='F':
    target=out/(name+'.pb')
    if target.exists():assert target.read_bytes()==data
    else:target.write_bytes(data)
  if row['case_id']=='FC08-H03':
   before=[record(100+i,'Shape',key=key(i+1)) for i in range(recipe['N'])]
   data=Writer().encode('Operation',operation({'insert_objects':{'objects':before}},999))
   target=out/(name+'.seed.pb')
   if target.exists():assert target.read_bytes()==data
   else:target.write_bytes(data)
   hashes.append({'label':'independent-before-seed','sha256':hashlib.sha256(data).hexdigest(),'size':len(data)})
  receipts.append({'variant_id':row['variant_id'],'input':row['input'],'expected':row['expected'],'wire_hashes':hashes,'fixture_role':'independent input construction only; frozen U not submitted to product'})
 return receipts
if __name__=='__main__':print(json.dumps(materialize(),indent=2))
