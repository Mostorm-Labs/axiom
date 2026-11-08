"""Use the frozen schema and semantic rules on an actual compiled document.
No UR0a source inventory/scope replay and no modification of frozen artifacts.
"""
import importlib.util,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[4]
path=ROOT/'docs/planning/g5-unified-render/ur0a/validate.py'
spec=importlib.util.spec_from_file_location('frozen_ur0a_validator',path)
validator=importlib.util.module_from_spec(spec);spec.loader.exec_module(validator)
data=sys.stdin.read(64*1024*1024+1)
if len(data)>64*1024*1024:raise ValueError('compiled metrics exceed bounded validation input')
record=json.loads(data);schema=validator.load(path.with_name('metrics.schema.json'))
validator.shape(record,schema,schema)
routes=validator.load(path.with_name('routes.json'))
validator.semantics(record,{r['route_id'] for r in routes['routes']})
print(json.dumps({'status':'VALID','schema':'g5-ur0-metrics-v1','dynamic_owner_check':'performed by the actual fail-closed decoder; frozen semantics is additional'}))
