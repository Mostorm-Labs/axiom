import subprocess,json,os
from pathlib import Path
r=Path.cwd();out=r/'.aegis/results/FC-08-SECTION-DE/section-drop-fix-001'
args=['cmake', '-S', 'C:\\Users\\Chado\\.codex\\worktrees\\fc08-p31-bootstrap\\canvas', '-B', 'C:\\Users\\Chado\\.codex\\worktrees\\fc08-p31-bootstrap\\canvas\\out\\fc08-section-c\\native', '-DCMAKE_RUNTIME_OUTPUT_DIRECTORY=C:\\Users\\Chado\\.codex\\worktrees\\fc08-p31-bootstrap\\canvas\\out\\fc08-section-drop\\bin']
x=subprocess.run(args,capture_output=True,text=True,encoding='utf-8',errors='replace')
(out/'output-directory.stdout.log').write_text(x.stdout,encoding='utf-8')
(out/'output-directory.stderr.log').write_text(x.stderr,encoding='utf-8')
(out/'output-directory.json').write_text(json.dumps({'argv':args,'exit_code':x.returncode,'reason':'Preserve running prior application; stage adjacent font assets before launch'},indent=2)+'\n')
raise SystemExit(x.returncode)
