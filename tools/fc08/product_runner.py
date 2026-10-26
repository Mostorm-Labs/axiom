"""Actual baseline SUT observations vs frozen expectations. No product behavior.

Missing owners/fields remain NOT_RUN. Pure codec rejection cannot qualify a
History, selection, Scene or temporal assertion. R01 uses independently checked
positive carriers and actual SUT execution, never a capability-summary stub.
"""
from datetime import datetime,timezone
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tests/fc08/reference'))
from oracles import at,evaluate
from wire import Writer

SUBJECT='427580ae13b60a39e88959b282cbffc695c9aabb'
PACKAGE_REVISION='6497f7ed199b98181863f43a04e8a6eecd5f2d60'
PACKAGE='.aegis/packages/FC-08-P31-BOOTSTRAP-v0.2'
GIT='D:/Softwares/Git/cmd/git.exe'

def now():return datetime.now(timezone.utc).isoformat()
def load(path):return json.loads(Path(path).read_text(encoding='utf-8'))
def digest(data):return hashlib.sha256(data).hexdigest()
def ref(path):
    path=Path(path);data=path.read_bytes()
    return {'path':str(path),'sha256':digest(data),'size':len(data)}
def save(path,value):
    path=Path(path);path.parent.mkdir(parents=True,exist_ok=True)
    with path.open('x',encoding='utf-8',newline='\n') as stream:stream.write(json.dumps(value,ensure_ascii=False,indent=2)+'\n')
    return ref(path)
def checked(binding):
    path=ROOT/binding['path'];data=path.read_bytes()
    if digest(data)!=binding['sha256']:raise ValueError('Frozen hash mismatch: '+str(path))
    return path

def evaluate_observation(predicates,observation):
    missing=[];failures=[]
    for index,predicate in enumerate(predicates):
        paths=([predicate['path']] if 'path' in predicate else [])+([predicate['before'],predicate['after']] if 'before' in predicate else [])
        try:
            for path in paths:at(observation,path)
        except (KeyError,IndexError,TypeError):
            missing.append({'predicate':index,'paths':paths});continue
        mismatch=evaluate([predicate],observation)
        if mismatch:failures.append({'predicate':index,'finding':mismatch})
    return {'result':'FAIL' if failures else 'NOT_RUN' if missing else 'PASS',
            'mismatches':failures,'missing_observations':missing}

def classify_red(observation,exit_code,fixture_validated=False):
    if exit_code or observation is None:
        return {'result':'BLOCKED_ENVIRONMENT','classification':'SUT_PROCESS_FAILURE','valid_product_RED':False}
    if not fixture_validated or 'root' in observation.get('category','').lower():
        return {'result':'NOT_RUN','classification':'UNQUALIFIED_FIXTURE_OR_OBSERVATION_ROOT','valid_product_RED':False}
    accepted=observation.get('normal_ingress_ok',observation.get('accepted',observation.get('ok')))
    if accepted is True:
        return {'result':'PASS','classification':'VERIFICATION_DESIGN_DEFECT','valid_product_RED':False}
    if accepted is False:
        return {'result':'FAIL','classification':'BASELINE_POSITIVE_CONTRACT_NOT_MET','valid_product_RED':True}
    return {'result':'NOT_RUN','classification':'MISSING_REAL_OBSERVATION','valid_product_RED':False}

def parse_output(raw):
    if raw['exit_code']!=0:raise RuntimeError('Actual SUT process failed')
    line=next((line for line in reversed(raw['stdout'].splitlines()) if line.startswith('{')),None)
    if line is None:raise RuntimeError('Actual SUT returned no JSON observation')
    return json.loads(line)

def native_call(program,argv,output):
    command=[str(program),*map(str,argv)];started=now()
    try:
        run=subprocess.run(command,cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=60)
        raw={'argv':command,'cwd':str(ROOT),'started_at':started,'ended_at':now(),
             'exit_code':run.returncode,'stdout':run.stdout,'stderr':run.stderr}
    except (OSError,subprocess.TimeoutExpired) as exc:
        raw={'argv':command,'cwd':str(ROOT),'started_at':started,'ended_at':now(),
             'exit_code':None,'stdout':'','stderr':str(exc)}
    raw_ref=save(output,raw)
    return raw_ref,raw

def qualify_binary(build,profile,role):
    records=load(ROOT/'.aegis/results/FC-08-VERIFY-MATERIALIZE/attempts/v0.2-obs01/profiles.json')
    frozen=next(b for b in records['binaries'] if b['tag']=='v02-off' and b['profile']==profile and b['role']==role)
    extension='wasm' if profile=='web-wasm' else 'exe'
    path=build/'tests/fc08'/('axiom_fc08_'+role+'.'+extension)
    identity=ref(path)
    if identity['sha256']!=frozen['sha256']:raise ValueError('Binary differs from exact baseline qualification: '+str(path))
    cache=(build/'CMakeCache.txt').read_text(encoding='utf-8')
    if 'AXIOM_FC08_QUALIFICATION_HOOKS:BOOL=OFF' not in cache:raise ValueError('Qualification hooks are not verified OFF')
    compiler=next((build/'CMakeFiles').rglob('CMakeCXXCompiler.cmake'))
    result={**identity,'historical_binding':frozen,'cache':ref(build/'CMakeCache.txt'),'compiler':ref(compiler),
            'reuse_policy':'Reuse exact verified baseline binary; every new run invokes it again. No historical observation is substituted.',
            'source_equivalence':'0d3b10bb production tree equals427580ae and current production tree, checked by actual git diff before execution.'}
    if profile=='web-wasm':
        browser_binding=ROOT/'.aegis/results/FC-08-VERIFY-MATERIALIZE/attempts/v0.2-obs01/browser/off-final/profile.json'
        programs=load(browser_binding)['programs'];javascript=path.with_suffix('.js')
        original=next(p for p in programs if Path(p['path']).name==javascript.name)
        if digest(javascript.read_bytes())!=original['sha256']:raise ValueError('WASM JavaScript loader identity changed')
        result['javascript']=ref(javascript);result['historical_browser_binding']=ref(browser_binding)
    result['source_files']=[]
    for source in ['tests/fc08/native/baseline_probe.cpp','tests/fc08/native/public_consumer.cpp','tests/fc08/native/headless_probe.cpp']:
        blob=subprocess.check_output([GIT,'show','0d3b10bba1ac4a31d669dd4a45f94975ec24f3c3:'+source],cwd=ROOT)
        if blob!=(ROOT/source).read_bytes():raise ValueError('Baseline observer source changed: '+source)
        result['source_files'].append(ref(ROOT/source))
    return result

def qualify_fixture(probe,protoc,output):
    path=checked(probe);typed=load(path.with_suffix('.input.json'))
    expected=Writer().encode(typed['message_type'],typed['value'])
    if typed['message_type']!=probe['root'] or expected!=path.read_bytes():raise ValueError('Independent field-table reconstruction mismatch: '+probe['id'])
    descriptor=ROOT/'.aegis/materialized/FC-08/proposed-descriptor/descriptor.pb'
    command=[str(protoc),'--descriptor_set_in='+str(descriptor),'--decode=auditoryworks.axiom.v1.'+probe['root']]
    run=subprocess.run(command,input=expected,capture_output=True,cwd=ROOT)
    raw={'argv':command,'stdin':ref(path),'exit_code':run.returncode,
         'stdout':run.stdout.decode('utf-8',errors='replace'),'stderr':run.stderr.decode('utf-8',errors='replace')}
    if run.returncode:raise ValueError('Accepted proposed descriptor rejects fixture: '+probe['id'])
    # These are the fixed, previously authored R01 positive carriers, not an
    # arbitrary protobuf stream. Type/domain constraints come from their exact
    # accepted typed model, plus the independent field table and frozen hashes.
    model=typed['value']
    if probe['root']=='ObjectRecord':
        assert model['kind_id']==10 and model['kind_version']==1
        body=model['content']['section'];assert body['boundary']['width']>0 and body['boundary']['height']>0 and body['padding']>=0 and body['header_height']>=0
        assert 'section {' in raw['stdout']
    if probe['id']=='op17-content':assert 'set_section_content {' in raw['stdout']
    if probe['id']=='op18-introduce':assert 'edit_container_topology {' in raw['stdout']
    return save(output,{'status':'FIXED_R01_POSITIVE_INDEPENDENTLY_RECONSTRUCTED_AND_DECODED',
                        'wire':ref(path),'typed_input':ref(path.with_suffix('.input.json')),'reference_writer':ref(ROOT/'tests/fc08/reference/wire.py'),
                        'descriptor':ref(descriptor),'protoc':ref(protoc),'decode':raw,
                        'production_codec_used_as_expected':False})

def message_fields(data):
    """Independent wire framing only; does not decide product acceptance."""
    offset=0;fields={}
    def varint():
        nonlocal offset
        value=0
        for shift in range(0,70,7):
            if offset>=len(data):raise ValueError('Truncated observed varint')
            byte=data[offset];offset+=1;value|=(byte&127)<<shift
            if byte<128:return value
        raise ValueError('Oversized observed varint')
    while offset<len(data):
        key=varint();number,kind=key>>3,key&7
        if not number:raise ValueError('Zero observed field number')
        if kind==0:value=varint()
        elif kind in [1,2,5]:
            size=varint() if kind==2 else 8 if kind==1 else 4
            if size>len(data)-offset:raise ValueError('Truncated observed field')
            value=data[offset:offset+size];offset+=size
        else:raise ValueError('Unsupported observed wire kind')
        fields.setdefault(number,[]).append(value)
    return fields

def observed_record_bytes(carrier,writer):
    # Strip only the observation transport envelope. Its fromUint64 IDs are
    # unrelated to the frozen opaque ObjectIds in each record body.
    data=bytes.fromhex(carrier)
    for message,name in [('Operation','payload'),('OperationPayload','insert_objects')]:
        number=next(f['number'] for f in writer.messages[message] if f['name']==name)
        values=message_fields(data)[number]
        if len(values)!=1:raise ValueError('Ambiguous observer carrier')
        data=values[0]
    fields=writer.messages['InsertObjectsOp']
    number=next(f['number'] for f in fields if f['name']=='objects')
    values=message_fields(data)[number]
    if len(values)!=1:raise ValueError('Observer must export one record per carrier')
    return values[0].hex()

def observation_for(request,actual,scenario):
    if request['mode']=='LegacyImport':
        result={'ready':actual['ok']}
        if actual['ok'] and 'wire_model' in scenario:
            writer=Writer();objects=scenario['wire_model'].get('objects',[])
            expected=[writer.encode('ObjectRecord',obj).hex() for obj in sorted(objects,key=lambda o:o['id']['value'])]
            result['legacy_fields_preserved']=[observed_record_bytes(carrier,writer) for carrier in actual['records']]==expected
        return result
    result={}
    if 'normal_ingress_ok' in actual:result['accepted']=actual['normal_ingress_ok']
    elif 'accepted' in actual:result['accepted']=actual['accepted']
    elif 'ok' in actual:result['accepted']=actual['ok']
    if 'canonical_hex' in actual:result['canonical_hex']=actual['canonical_hex']
    return result

def baseline_observation_mode(scenario,wire):
    if scenario.get('direction','normal-wire-ingress')!='normal-wire-ingress':
        return None  # Decoder is not a replacement for model-to-writer execution.
    if wire['root']=='DocumentSnapshot':
        return 'LegacyImport' if scenario.get('explicit_legacy_import') is True else None
    if scenario.get('before'):
        return None  # No stateful applicability assertion without actual loaded prestate.
    return 'Ingress' if wire['root'] in ['Operation','ObjectRecord'] else None

def execute(args):
    root=ROOT;out=args.output.resolve()
    out.mkdir(parents=True,exist_ok=False)
    git=lambda *values:subprocess.check_output([GIT,*values],cwd=root,text=True,encoding='utf-8').strip()
    started=now();head=git('rev-parse','HEAD')
    for rev in [SUBJECT,PACKAGE_REVISION]:subprocess.run([GIT,'merge-base','--is-ancestor',rev,'HEAD'],cwd=root,check=True)
    for a,b in [('0d3b10bba1ac4a31d669dd4a45f94975ec24f3c3',SUBJECT),(SUBJECT,'HEAD')]:
        if git('diff','--name-only',a,b,'--','runtime','schema','apps','debug_ui','CMakeLists.txt','semantic-sdk.lock.json','r1-full-skia-sdk.lock.json'):raise ValueError('Unmodified production subject not established')
    design=load(args.design);manifest=load(checked(design['design']['corpus_manifest']))
    matrix=load(checked(design['profile_policy']['exact_matrix']));mapping={m['variant_id']:m for m in matrix['variants']}
    rows=manifest['variants'] if args.case in ['all','discover','FC08-RED-01'] else [r for r in manifest['variants'] if args.case in [r['case_id'],r['variant_id']]]
    if not rows:raise ValueError('Unknown case or empty discovery')
    save(out/'discovery.json',{'profile':args.profile,'variant_ids':[r['variant_id'] for r in rows],
                              'matrix':ref(checked(design['profile_policy']['exact_matrix'])),'product_P32_authorized':False})
    if args.case=='discover':return 0
    native=args.native_build.resolve();web=args.web_build.resolve()
    binary_profile='web-wasm' if args.profile=='web-wasm' else 'windows-native'
    build=web if args.profile=='web-wasm' else native
    binaries={role:qualify_binary(build,binary_profile,role+'_probe') for role in ['baseline','headless']}
    save(out/'baseline-program-bindings.json',binaries)
    r01=load(root/'.aegis/materialized/FC-08/r01-inputs.json');requests=[];proofs={}
    for probe in r01:
        proofs[probe['id']]=qualify_fixture(probe,args.protoc,out/'fixture-proofs'/(probe['id']+'.json'))
        requests.append({'id':'r01-'+probe['id'],'program':'headless' if args.profile=='headless' else 'baseline',
                         'mode':'Ingress','root':probe['root'],'path':str(checked(probe)),'sha256':probe['sha256']})
    case_requests={};scenarios={};expectations={}
    for n,row in enumerate(rows):
        scenario=load(checked(row['input']));expected=load(checked(row['expected']));scenarios[row['variant_id']]=scenario;expectations[row['variant_id']]=expected
        cell=mapping[row['variant_id']]['profiles'][args.profile]
        if cell['applicability']!='APPLICABLE' or args.case=='FC08-RED-01':continue
        wires=[w for w in row['wire'] if 'path' in w and w['label']=='input' and w['root'] in ['Operation','ObjectRecord','DocumentSnapshot']]
        if not wires or 'wire_model' not in scenario:continue
        wire=wires[0];mode=baseline_observation_mode(scenario,wire)
        if mode is None:continue
        program='headless' if wire['root']=='DocumentSnapshot' or args.profile=='headless' else 'baseline'
        request={'id':f'case-{n:03}','program':program,'mode':mode,
                 'root':wire['root'],'path':str(checked(wire)),'sha256':wire['sha256']}
        requests.append(request);case_requests[row['variant_id']]=request
    observations={};environment={}
    if args.profile=='web-wasm':
        req_path=out/'browser-requests.json';save(req_path,requests)
        command=[str(args.node),str(root/'tests/fc08/product/browser_transport.mjs'),'--build',str(web),'--requests',str(req_path),
                 '--chrome',str(args.chrome),'--output',str(out/'browser')]
        run=subprocess.run(command,cwd=root,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=1200)
        save(out/'browser-invocation.json',{'argv':command,'exit_code':run.returncode,'stdout':run.stdout,'stderr':run.stderr})
        if run.returncode:raise RuntimeError('Actual browser/WASM execution failed; inspect raw browser invocation')
        environment=load(out/'browser/environment.json')
        for request in requests:
            raw_path=out/'browser'/(request['id']+'.json');raw=load(raw_path)
            observations[request['id']]={'raw':ref(raw_path),'process':raw,'observation':parse_output(raw)}
    else:
        for request in requests:
            program=Path(binaries[request['program']]['path'])
            argv=([request['mode'],request['root'],request['path']] if request['program']=='headless' else [request['root'],request['path']])
            raw_ref,raw=native_call(program,argv,out/'native'/(request['id']+'.json'))
            observations[request['id']]={'raw':raw_ref,'process':raw,'observation':parse_output(raw)}
        environment={'runtime':'Windows native process','python':sys.version,'binaries':binaries}
    red=[];red_errors=[]
    for probe in r01:
        actual=observations['r01-'+probe['id']];obs=actual['observation']
        # headless actual ok is production observation, not an expected value.
        if 'accepted' not in obs and 'ok' in obs:obs={**obs,'accepted':obs['ok']}
        classification=classify_red(obs,actual['process']['exit_code'],True) if probe['new_feature'] else {
            'result':'PASS' if obs.get('accepted') is True else 'FAIL',
            'classification':'INHERITED_NORMAL_INGRESS_CONTROL','valid_product_RED':False}
        if (probe['new_feature'] and not classification['valid_product_RED']) or (not probe['new_feature'] and classification['result']!='PASS'):red_errors.append(probe['id'])
        red.append({'probe_id':probe['id'],'input':probe,'expected':'ACCEPT','independent_fixture_proof':proofs[probe['id']],
                    'actual':obs,'raw':actual['raw'],**classification})
    cases=[]
    for row in rows:
        cell=mapping[row['variant_id']]['profiles'][args.profile];observed_ref=None;basis='No real observation supplied for this owner/variant';result='NOT_RUN';failure=None
        evaluated=None;request=case_requests.get(row['variant_id'])
        if cell['applicability']=='NOT_APPLICABLE':basis='Explicit accepted profile exclusion: '+cell['reason']
        elif request:
            actual=observations[request['id']];observed_ref=actual['raw'];basis='Actual '+request['program']+' normal production '+request['mode']+'; no invented state/History/Scene fields'
            adapted=observation_for(request,actual['observation'],scenarios[row['variant_id']])
            evaluated=evaluate_observation(expectations[row['variant_id']]['predicates'],adapted);result=evaluated['result']
            # Old rejection of any new feature is not coverage of a specific
            # malformed-domain negative, even when the accepted boolean matches.
            if result=='PASS' and any(p.get('expected') is False for p in expectations[row['variant_id']]['predicates']):
                result='NOT_RUN';basis+='; rejection at missing-feature boundary cannot qualify the intended negative condition'
            if evaluated['mismatches']:failure='normal production '+request['mode']+' / frozen predicate mismatch'
        cases.append({'case_id':row['case_id'],'variant_id':row['variant_id'],'family_id':mapping[row['variant_id']]['family_id'],
                      'input_hash':row['input']['sha256'],'expected_hash':row['expected']['sha256'],'observed_artifact':observed_ref,
                      'observation_basis':basis,'result':result,'failure_boundary':failure,
                      'applicability':cell['applicability'],'evidence_role':cell['evidence_role'],'predicate_evaluation':evaluated})
    authority=ref(root/PACKAGE/'authority.lock.json')
    result={'format':'fc08-verification-run-v1','design_id':design['design']['id'],'authority_id':'fc08-container-extension-semantic-v0.1',
            'authority_binding_hash':authority['sha256'],'verification_manifest_hash':ref(checked(design['design']['corpus_manifest']))['sha256'],
            'qualification_subject_revision':SUBJECT,'runner_revision':head,'oracle_revision':'0d3b10bba1ac4a31d669dd4a45f94975ec24f3c3',
            'platform_profile':args.profile,'build_configuration':'Reused exact locked baseline OFF binaries; unmodified production byte-equivalence checked',
            'toolchain':environment,'source_tree_dirty':bool(git('status','--porcelain')),'started_at':started,'ended_at':now(),
            'invocation':sys.argv,'cases':cases,'FC08_RED_01':{'status':'REAL_R01_RED_OBSERVED' if not red_errors else 'BLOCKED_REAL_RED',
                                                           'rows':red,'errors':red_errors},
            'coverage_limits':'Codec/snapshot actual baseline adapters only. Missing command, History, selection, Scene, preview and actual host bridge observations stay NOT_RUN; this runner does not qualify all302 product variants.',
            'product_P32_authorized':False,'DEV_PASS':False,'P34_PASS':False,'PHYS_PASS':False}
    save(out/'results.json',result)
    save(out/'coverage.json',{'profile':args.profile,'case_results':{s:sum(c['result']==s for c in cases) for s in ['PASS','FAIL','NOT_RUN','BLOCKED_ENVIRONMENT']},
                            'real_R01_RED_observed':not red_errors,'all_product_observation_seams_materialized':False,
                            'unobserved_applicable_variant_ids':[c['variant_id'] for c in cases if c['applicability']=='APPLICABLE' and c['observed_artifact'] is None]})
    print(json.dumps({'profile':args.profile,'red_status':result['FC08_RED_01']['status'],'case_count':len(cases),'output':str(out)},indent=2))
    return 0 if not red_errors else 2
