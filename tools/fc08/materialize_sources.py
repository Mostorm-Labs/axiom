"""M01: accepted-source exports and additive isolated descriptors.

Reads captured remote facts and exact Git source blobs. Refuses replacement of
an existing artifact with different bytes. Does not edit production schema.
"""
from __future__ import annotations
from dataclasses import asdict
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from verification.tools.descriptor_refreeze_diff import parse_descriptor

MAT = ROOT / '.aegis/materialized/FC-08'
PKG = ROOT / '.aegis/packages/FC-08-VERIFY-MATERIALIZE'
START = 'db21441a1e978ea30fcfb4185691dadbb6f81f78'
GIT = 'D:/Softwares/Git/cmd/git.exe'


def digest(data):
    return hashlib.sha256(data).hexdigest()


def immutable(path, data):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists():
        if path.read_bytes() != data:
            raise ValueError(f'refusing to replace existing different artifact: {path}')
    else:
        path.write_bytes(data)


def json_bytes(value):
    return (json.dumps(value, ensure_ascii=False, indent=2, sort_keys=True) + '\n').encode('utf-8')


def save_json(path, value):
    immutable(path, json_bytes(value))


def git(*args):
    return subprocess.check_output([GIT, *args], cwd=ROOT)


def source_exports():
    acquisition = json.loads((PKG / 'source-acquisition.json').read_text())
    sections = {'P12': (2,7), 'P13': (1,13), 'P14': (1,7), 'P15': (1,8),
                'P16': (1,11), 'P17': (1,10), 'P20 execution': (1,6)}
    exports = []
    for spec in acquisition['accepted_technical_sources']:
        name = spec['id'].split()[0]
        response = MAT / f'sources/captures/{name}.response.json'
        raw = json.loads(response.read_text(encoding='utf-8'))
        if raw.get('isError'):
            raise ValueError(f'source fetch failed: {name}')
        fetched = json.loads(raw['content'][0]['text'])
        if fetched['id'] != spec['url']:
            raise ValueError(f'source identity mismatch: {name}')
        if spec['observed_last_edit'] and fetched['page_last_edited_at'] != spec['observed_last_edit']:
            raise ValueError(f'accepted source drift requires scoped reconciliation: {name}')
        body = fetched['text'].split('<content>',1)[1].split('</content>',1)[0]
        pieces = re.split(r'(?=^## \d+\.)', body, flags=re.M)[1:]
        lower, upper = sections[spec['id']]
        selected = [p for p in pieces if lower <= int(re.match(r'## (\d+)', p).group(1)) <= upper]
        if name == 'P20':
            selected += [p for p in pieces if p.startswith('## 8.')]
        if len(selected) != upper-lower+1 + (name == 'P20'):
            raise ValueError(f'incomplete accepted technical export: {name}')
        path = MAT / f'sources/accepted-exports/{name}.md'
        data = ''.join(selected).rstrip().encode('utf-8') + b'\n'
        immutable(path, data)
        event = ('FC08-P20-CLOSURE-v0.1' if name == 'P20' else
                 'FC08-P23-ACCEPTANCE-v0.1' if name in ('P12','P13') else
                 'FC08-P20-CLOSURE-v0.1: architecture v0.1 + ARC-CR-C01/C02')
        exports.append({**spec, 'path': path.relative_to(ROOT).as_posix(), 'sha256':digest(data),
                        'acceptance_event':event, 'capture_sha256':digest(response.read_bytes()),
                        'captured_last_edit': fetched['page_last_edited_at'],
                        'accepted_sections': [p.splitlines()[0] for p in selected]})
        metadata = {k:v for k,v in fetched.items() if k != 'text'}
        save_json(MAT / f'sources/captures/{name}.metadata.json', metadata)
    return exports


def descriptors():
    paths = git('ls-tree','-r','--name-only',START,'schema/axiom/v1/proto').decode().splitlines()
    facts = json.loads((ROOT / 'out/fc08/native-environment.json').read_text())
    protoc = facts['environment']['AXIOM_PROTOC']
    version = subprocess.check_output([protoc,'--version']).decode().strip()
    all_sources, trees, logs = [], {}, []
    for label in ('baseline','proposed'):
        dest = MAT / f'{label}-descriptor'
        proto = dest / 'proto'
        for p in paths:
            rel = p.removeprefix('schema/axiom/v1/proto/')
            data = git('show', f'{START}:{p}')
            if label == 'proposed':
                if rel.endswith('/object.proto'):
                    data = data.replace(b'message Placement', b'import "auditoryworks/axiom/v1/section.proto";\n\nmessage Placement',1)
                    data = data.replace(b'DabBrushStrokeContent dab_brush_stroke = 11;', b'DabBrushStrokeContent dab_brush_stroke = 11;\n    SectionContent section = 12;',1)
                if rel.endswith('/operation.proto'):
                    data = data.replace(b'message InsertObjectsOp',b'import "auditoryworks/axiom/v1/container_operations.proto";\n\nmessage InsertObjectsOp',1)
                    data = data.replace(b'UpdateResourceManifestOp update_resource_manifest = 16;', b'UpdateResourceManifestOp update_resource_manifest = 16;\n    SetSectionContentOp set_section_content = 17;\n    EditContainerTopologyOp edit_container_topology = 18;',1)
            immutable(proto / rel, data)
            all_sources.append({'profile':label,'path':(proto/rel).relative_to(ROOT).as_posix(),
                                'sha256':digest(data),'git_source':f'{START}:{p}'})
        if label == 'proposed':
            for name in ('section.proto','container_operations.proto'):
                source = MAT / f'sources/accepted-seed/semantic/idl/{name}'
                immutable(proto / f'auditoryworks/axiom/v1/{name}',source.read_bytes())
                all_sources.append({'profile':label,'path':(proto/f'auditoryworks/axiom/v1/{name}').relative_to(ROOT).as_posix(),
                                    'sha256':digest(source.read_bytes()),'seed_source':source.relative_to(ROOT).as_posix()})
        output = dest / 'descriptor.pb'
        # Use a temporary sibling to detect unexpected regeneration differences.
        generated = dest / 'generated.tmp.pb'
        command = [protoc, f'--proto_path={proto}', '--include_imports',
                   f'--descriptor_set_out={generated}', *[p.relative_to(proto).as_posix() for p in sorted(proto.rglob('*.proto'))]]
        result = subprocess.run(command,capture_output=True,text=True,cwd=ROOT)
        logs.append({'profile':label,'argv':command,'exit_code':result.returncode,
                     'stdout':result.stdout,'stderr':result.stderr})
        if result.returncode:
            save_json(MAT / 'descriptor-build-failure.json',logs)
            raise RuntimeError('isolated schema generation failed; not a semantic RED')
        immutable(output, generated.read_bytes())
        generated.unlink()
        trees[label] = parse_descriptor(output.read_bytes())
        save_json(dest / 'field-table.json',{k:asdict(v) for k,v in trees[label].items()})
    base, proposed = trees['baseline'],trees['proposed']
    additions = {}
    for f in base:
        assert base[f].enums == proposed[f].enums
        for msg in base[f].messages:
            after = next(m for m in proposed[f].messages if m.name == msg.name)
            assert set(msg.fields) <= set(after.fields)
            delta = set(after.fields) - set(msg.fields)
            if delta:
                additions[f + ':' + msg.name] = [asdict(x) for x in sorted(delta,key=lambda x:x.number)]
    assert {k: [f['number'] for f in v] for k,v in additions.items()} == {
        'auditoryworks/axiom/v1/object.proto:ObjectContent':[12],
        'auditoryworks/axiom/v1/operation.proto:OperationPayload':[17,18]}
    save_json(MAT / 'descriptor-diff.json',{'status':'SOURCE_COMPLETE_ADDITIVE_MATCH',
        'starting_revision':START,'protoc_version':version,'protoc_sha256':digest(Path(protoc).read_bytes()),
        'preserved_inherited_fields':True,'accepted_existing_message_additions':additions,
        'new_files':sorted(set(proposed)-set(base)),
        'baseline_sha256':digest((MAT/'baseline-descriptor/descriptor.pb').read_bytes()),
        'proposed_sha256':digest((MAT/'proposed-descriptor/descriptor.pb').read_bytes()),
        'historical_descriptor_unchanged':True})
    immutable(MAT/'descriptor-build-log.txt', json_bytes({'commands':logs,'version':version}))
    return all_sources


def main():
    exports = source_exports()
    schemas = descriptors()
    seeds = [{ 'path':p.relative_to(ROOT).as_posix(),'sha256':digest(p.read_bytes())}
             for p in sorted((MAT/'sources/accepted-seed').rglob('*')) if p.is_file()]
    save_json(MAT/'source-index.json',{'format':'fc08-source-index-v1','starting_revision':START,
        'accepted_exports':exports,'seed_sources':seeds,'schema_sources':schemas,
        'inherited_math_export':{
            'path':'.aegis/materialized/FC-08/sources/accepted-exports/GeometryTypes-math.md',
            'sha256':digest((MAT/'sources/accepted-exports/GeometryTypes-math.md').read_bytes()),
            'ref':'https://app.notion.com/p/3c44c57a590c8157a657c7f796ebcdac',
            'captured_last_edit':'2026-08-22T17:05:52.449Z',
            'accepted_sections':'2..8, especially exact inverse formula section8',
            'acceptance_event':'FC08-P23-D06 incorporates Geometry Types via P12 section6.2',
            'capture_path':'.aegis/materialized/FC-08/sources/captures/GeometryTypes.response.json',
            'capture_sha256':digest((MAT/'sources/captures/GeometryTypes.response.json').read_bytes())},
        'precedence':json.loads((PKG/'authority.lock.json').read_text())['precedence'],
        'product_P32_authorized':False})
    print('M01 source exports and source-complete isolated descriptors materialized')


if __name__ == '__main__':
    main()
