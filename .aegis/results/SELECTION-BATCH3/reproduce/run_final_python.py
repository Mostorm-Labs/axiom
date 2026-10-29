from pathlib import Path
import subprocess,json,os,sys
root=Path.cwd();out=root/'out/selection-batch3';os.environ.update(json.loads((out/'environment.json').read_text())['environment'])
commands=json.loads((out/'precommit-python-commands.json').read_text())
for i,row in enumerate(commands):
 with (out/f'final-python-{i}.log').open('w',encoding='utf-8') as log:r=subprocess.run(row['argv'],stdout=log,stderr=subprocess.STDOUT)
 row['exit_code']=r.returncode
 print(row['argv'][4:],'exit',r.returncode,flush=True)
 if r.returncode:raise SystemExit(r.returncode)
(out/'final-python-commands.json').write_text(json.dumps(commands,indent=2))
