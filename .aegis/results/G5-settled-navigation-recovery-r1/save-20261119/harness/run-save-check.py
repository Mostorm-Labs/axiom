from pathlib import Path
import datetime,json,subprocess,sys,time
w=Path('/Users/qing/.codex/worktrees/g5-settled-navigation-recovery')
s=Path(__file__).resolve().parent.parent
name=sys.argv[1]
argv=sys.argv[2:]
start=time.monotonic()
r=subprocess.run(argv,cwd=w,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
log=s/(name+'.log');log.write_text(r.stdout)
record={'at':datetime.datetime.now(datetime.timezone.utc).isoformat(),'name':name,'argv':argv,'cwd':str(w),'exit':r.returncode,'seconds':time.monotonic()-start,'log':str(log)}
(s/(name+'.command.json')).write_text(json.dumps(record,indent=2)+'\n')
print(json.dumps(record));print(r.stdout)
sys.exit(r.returncode)
