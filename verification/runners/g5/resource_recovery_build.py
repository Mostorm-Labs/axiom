"""Assemble private UR1d qualification layout without bypassing consumer install.

The repository deliberately refuses installing UR0 diagnostics as a consumer SDK.
Use the matching normal facade/assets and freshly built qualification engine only
in this task's out directory. The resulting layout is not a consumer distribution.
"""
import argparse
import hashlib
import json
import shutil
from pathlib import Path

def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()

def cache(path):
    return {line.split(':',1)[0]:line.split('=',1)[1] for line in Path(path).read_text().splitlines()
            if ':' in line and '=' in line and not line.startswith(('#','//'))}

def assemble(normal, normal_build, qualification_build, output, manifest):
    n,q=cache(normal_build/'CMakeCache.txt'),cache(qualification_build/'CMakeCache.txt')
    for data in (n,q):
        assert data['AXIOM_INSTALL_DEVELOPER_COMPONENTS']=='OFF'
        assert data['AXIOM_RENDER_CACHE_MIB']=='32' and data['AXIOM_RENDER_SHARED_MIB']=='96'
    assert n['AXIOM_G5_UR0_DIAGNOSTICS']=='OFF' and n['AXIOM_ENABLE_FRAME_TIMING']=='OFF'
    assert q['AXIOM_G5_UR0_DIAGNOSTICS']=='ON' and q['AXIOM_ENABLE_FRAME_TIMING']=='ON'
    names=sorted(str(p.relative_to(normal)) for p in normal.rglob('*') if p.is_file())
    assert len(names)==36 and not any(Path(p).suffix in ('.a','.h','.hpp','.lib') for p in names)
    assert not output.exists(), 'private artifact is immutable once assembled'
    assert any(task in output.parts for task in ('G5-UR1d-resource-recovery','G5-UR1d-physical-loss-repair-r1','G5-UR2a-current-scene-handoff-r2'))
    shutil.copytree(normal,output)
    replacements={}
    for name in ('internal/axiom.mjs','axiom.wasm','axiom.data'):
        source=qualification_build/'sdk/web/internal'/Path(name).name
        assert source.is_file(),source
        shutil.copyfile(source,output/name)
        replacements[name]={'path':str(source),'sha256':sha(source)}
    # Emscripten minifies the WASM export names; their public module aliases
    # are assigned in the matching generated JS, not stored verbatim in WASM.
    assert b'axiom_g5_trace_control' not in (normal/'internal/axiom.mjs').read_bytes()
    assert b'axiom_g5_trace_control' in (output/'internal/axiom.mjs').read_bytes()
    assert b'AXIOM_GPU_RETIREMENT' not in (normal/'axiom.wasm').read_bytes()
    value={'schema':'g5-ur1d-private-qualification-layout-v1',
        'kind':'PRIVATE_VERIFICATION_ONLY_NOT_CONSUMER_INSTALL','consumer_install_succeeded':False,
        'consumer_install_guard':'UR0 qualification artifacts are not consumer install packages',
        'normal_source':str(normal),'qualification_build':str(qualification_build),'output':str(output),
        'normal_cache_sha256':sha(normal_build/'CMakeCache.txt'),
        'qualification_cache_sha256':sha(qualification_build/'CMakeCache.txt'),
        'replacements':replacements,'files':{name:sha(output/name) for name in names},
        'facade_declarations_assets_match_normal':all(sha(normal/name)==sha(output/name) for name in names if name not in replacements)}
    manifest.write_text(json.dumps(value,indent=2)+'\n')
    print('private qualification layout',len(names),'files; consumer install guard preserved')

if __name__=='__main__':
    p=argparse.ArgumentParser()
    for name in ('normal','normal-build','qualification-build','output','manifest'): p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();assemble(a.normal,a.normal_build,a.qualification_build,a.output,a.manifest)
