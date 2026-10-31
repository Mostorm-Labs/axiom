"""Consumer-only SDK packages. No component or dependency archives are shipped."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import sys
import tempfile
import zipfile
sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
from tools.release.preview_package import package_tree, sha256

NATIVE_REQUIRED=('bin/axiom.dll','lib/axiom.lib','include/axiom/api.h','include/axiom/axiom.hpp',
 'include/axiom/detail/json.hpp','lib/cmake/AxiomSDK/AxiomSDKConfig.cmake',
 'bin/share/axiom/icudtl.dat','bin/share/axiom/axiom-font.otf','bin/share/axiom/fonts/catalog.json',
 'licenses/semantic/Protobuf.txt','licenses/semantic/Abseil.txt','licenses/skia/Skia.txt',
 'licenses/noto-cjk/LICENSE.txt','licenses/nlohmann-json/LICENSE.MIT')
JS_REQUIRED=('axiom.mjs','axiom.d.mts','internal/axiom.mjs','axiom.wasm','axiom.data','pointer_input.mjs','text_input.mjs','package.json','example.html','serve.py','licenses/nlohmann-json-LICENSE.MIT')

def validate_native_payload(prefix):
    libs={p.relative_to(prefix).as_posix() for p in prefix.rglob('*.lib')}
    if libs-{'lib/axiom.lib'} or list(prefix.rglob('*.a')) or (prefix/'dependencies').exists():
        raise ValueError('internal archives/dependency SDKs in consumer payload')
    if {p.relative_to(prefix).as_posix() for p in prefix.rglob('*.dll')}-{'bin/axiom.dll'}:
        raise ValueError('internal dependency DLLs in consumer payload')
    missing=[name for name in NATIVE_REQUIRED if not (prefix/name).is_file()]
    if missing:raise ValueError('missing consumer assets: '+', '.join(missing))

def package(platform,prefix,demo,repo,output,revision):
    output.mkdir(parents=True,exist_ok=True)
    version=json.loads((repo/'sdk/version.json').read_text(encoding='utf-8'))['version']
    assets=[]
    with tempfile.TemporaryDirectory(prefix='axiom-consumer-package-') as tmp:
        root=Path(tmp)
        if platform=='windows':
            validate_native_payload(prefix)
            stage=root/'sdk';shutil.copytree(prefix,stage)
            shutil.copytree(repo/'verification/sdk_windows_dll/consumer',stage/'example')
            shutil.copy2(repo/'sdk/windows/README.md',stage/'README.md')
            name='axiom-sdk-windows.zip';required=[*NATIVE_REQUIRED,'README.md','example/CMakeLists.txt'];kind='windows-sdk'
        else:
            stage=root/'sdk';shutil.copytree(prefix/'js',stage)
            if json.loads((stage/'package.json').read_text())['version']!=version:raise ValueError('JS version mismatch')
            name='axiom-sdk-js.zip';required=list(JS_REQUIRED);kind='js-sdk'
        package_tree(stage,output/name,required,revision,kind,version=version);assets.append(output/name)
        app=root/'demo';shutil.copytree(demo if platform=='windows' else stage,app)
        if platform=='windows':
            shutil.copytree(prefix/'licenses',app/'licenses')
            required=['axiom_example.exe','axiom.dll','icudtl.dat','share/axiom/fonts/catalog.json']
            (app/'README.md').write_text('Axiom DLL demo. Run axiom_example.exe. Keep DLL, ICU, share/ and licenses/ together. Tools/Create/Edit/Image/Text format/Font menus exercise the public SDK. Native IME/touch still require physical validation.\n',encoding='utf-8')
        else:
            shutil.copy2(app/'example.html',app/'index.html');required=[*JS_REQUIRED,'index.html']
        name=f'axiom-demo-{platform}.zip'
        package_tree(app,output/name,required,revision,platform+'-demo',version=version);assets.append(output/name)
    return assets

def verify_release(directory,revision):
    expected={'axiom-sdk-windows.zip':'windows-sdk','axiom-sdk-js.zip':'js-sdk','axiom-demo-windows.zip':'windows-demo','axiom-demo-web.zip':'web-demo'}
    actual={p.name:p for p in directory.glob('*.zip')}
    if set(actual)!=set(expected):raise ValueError(f'wrong consumer release assets: {sorted(actual)}')
    versions=set()
    for name,path in actual.items():
        with zipfile.ZipFile(path) as z:
            members=z.namelist()
            if len(set(members))!=len(members) or any(n.startswith('/') or '..' in Path(n).parts or '\\' in n for n in members):raise ValueError('unsafe archive')
            m=json.loads(z.read('manifest.json'));versions.add(m.get('sdk_version'))
            if m.get('source_revision')!=revision or m.get('kind')!=expected[name]:raise ValueError('release identity mismatch')
            if set(m['files'])!=set(members)-{'manifest.json'}:raise ValueError('manifest file set mismatch')
            for member,digest in m['files'].items():
                if hashlib.sha256(z.read(member)).hexdigest()!=digest:raise ValueError('member hash mismatch')
            if name=='axiom-sdk-windows.zip':
                if {n for n in members if n.endswith(('.lib','.a'))}!={'lib/axiom.lib'}:raise ValueError('internal archives leaked')
                if any(n.startswith('dependencies/') for n in members):raise ValueError('dependency SDK leaked')
            if name=='axiom-sdk-js.zip' and json.loads(z.read('package.json'))['version']!=m['sdk_version']:raise ValueError('npm version mismatch')
    if len(versions)!=1 or None in versions:raise ValueError('SDK release versions differ')
    (directory/'SHA256SUMS').write_text(''.join(f'{sha256(actual[n])}  {n}\n' for n in sorted(actual)),encoding='ascii')
    (directory/'release.json').write_text(json.dumps({'format':'axiom-consumer-release-v1','source_revision':revision,'sdk_version':versions.pop(),'assets':{n:sha256(p) for n,p in sorted(actual.items())}},indent=2)+'\n',encoding='utf-8')

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--verify-release',type=Path);p.add_argument('--revision',required=True)
    a=p.parse_args();verify_release(a.verify_release,a.revision)
