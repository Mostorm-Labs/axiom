import hashlib
import json
from pathlib import Path
import shutil
import zipfile

repo = Path(__file__).resolve().parents[2]
evidence = repo / 'out/fc02-device'
source = evidence / 'distribution-complete'
delivery = Path(r'C:\Users\NearHub\Desktop\Axiom-FC02-3d7afa35-Windows-x64-20261022')
delivery.mkdir(parents=True, exist_ok=False)
for item in source.iterdir():
    if item.name == 'imgui.ini':
        continue
    if item.is_dir():
        shutil.copytree(item, delivery / item.name)
    else:
        shutil.copy2(item, delivery / item.name)

readme = '''Axiom FC-02 Windows x64 — complete distribution

Product source: 3d7afa351dfbd42f18c449bda35f861089b1fb4f
Documentation baseline: 164dfa2fc0af84df397b3ca2de0f6543fd50da02
Original supplied executable, unchanged SHA-256:
bf917e023f35f0342e21d0dc5dab54d665bb079da12eba229b970bda8fdc53c8

Extract/copy this ENTIRE directory, retaining subdirectories. Run
Start-Axiom.cmd or axiom_minimal_runtime_host_windows-3d7afa35.exe.
Do not copy the executable alone.

Required adjacent resources from the existing CMake packaging contract:
  axiom-font.otf
  axiom-font.LICENSE.txt
  axiom-font.NOTICE.txt
  fonts/                 complete pinned imported font catalog and licenses
  icudtl.dat             locked Skia Windows runtime data

third-party-licenses/ contains the locked SDK notices. Their inclusion does
not introduce dynamic SDK libraries. The executable directly imports Windows
system DLLs; the startup repair did not install a VC runtime or change security
policy, GPU renderer, Runtime code, fonts or SDK locks.

F12 opens Debug UI. Native IME/touch/DPI manual results are separate from startup
recovery. No exhaustive physical or independent FC-02 completion is asserted.
Start-Axiom.cmd records a distinct set of logs for each invocation in logs/.
imgui.ini is user-local state, excluded from this package and Git changes.
distribution-manifest.json lists the immutable package files and SHA-256.
'''
(delivery / 'README.txt').write_text(readme, encoding='utf-8', newline='\r\n')
launcher = '''@echo off
cd /d "%~dp0"
if not exist logs mkdir logs
set "AXIOM_RUN_ID=%RANDOM%-%RANDOM%"
set "AXIOM_MINIMAL_INPUT_LOG=%~dp0logs\\input-%AXIOM_RUN_ID%.log"
echo exe=%~dp0axiom_minimal_runtime_host_windows-3d7afa35.exe>"logs\\launch-%AXIOM_RUN_ID%.log"
"%~dp0axiom_minimal_runtime_host_windows-3d7afa35.exe" 1>"logs\\stdout-%AXIOM_RUN_ID%.log" 2>"logs\\stderr-%AXIOM_RUN_ID%.log"
echo exit_code=%ERRORLEVEL%>>"logs\\launch-%AXIOM_RUN_ID%.log"
'''
(delivery / 'Start-Axiom.cmd').write_text(launcher, encoding='ascii', newline='\r\n')
files = []
for path in sorted(delivery.rglob('*')):
    if path.is_file():
        files.append({'path': path.relative_to(delivery).as_posix(), 'bytes': path.stat().st_size,
                      'sha256': hashlib.sha256(path.read_bytes()).hexdigest()})
manifest = {'product_revision': '3d7afa351dfbd42f18c449bda35f861089b1fb4f',
            'packaging_baseline': '164dfa2fc0af84df397b3ca2de0f6543fd50da02',
            'skia_sdk_id': '29ebc4ef2655e182c7da5996c2a0b23cb07214f1efcacc2bf149dd7c841c99fa',
            'files': files, 'excluded': ['imgui.ini', 'logs/'],
            'manifest_excludes_itself': True}
(delivery / 'distribution-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
archive = delivery.with_suffix('.zip')
with zipfile.ZipFile(archive, 'x', compression=zipfile.ZIP_DEFLATED) as package:
    for path in sorted(delivery.rglob('*')):
        if path.is_file():
            package.write(path, delivery.name + '/' + path.relative_to(delivery).as_posix())
result = {'directory': str(delivery), 'archive': str(archive), 'archive_bytes': archive.stat().st_size,
          'archive_sha256': hashlib.sha256(archive.read_bytes()).hexdigest(), 'payload_files': len(files)}
(evidence / 'delivery.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
shutil.copy2(delivery / 'distribution-manifest.json', evidence / 'distribution-manifest.json')
print(json.dumps(result, indent=2))
