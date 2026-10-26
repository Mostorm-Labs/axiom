"""Preflight and native build evidence for the bounded Section B package."""
import argparse,fnmatch,hashlib,json,os,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT))
from tools.semantic.cmake_consumer import consumer_cmake_arguments
GIT="D:/Softwares/Git/cmd/git.exe"
PKG=".aegis/packages/FC-08-SECTION-B-v0.1"
REV="cbe90563cf45f16be54fb8b4f8b6d765de9203ac"
BASE="fecd797a3f6a6fb9c27ac6d96f7edeafdfb9dc4c"

def git(*args):return subprocess.check_output([GIT,*args],cwd=ROOT).decode().strip()
def main():
 p=argparse.ArgumentParser();p.add_argument("action",choices=["preflight","build","audit"]);p.add_argument("--repo",required=True);p.add_argument("--phase",required=True);p.add_argument("--targets",nargs="+",default=["axiom_fc08_section_operation_tests"]);a=p.parse_args()
 assert Path(a.repo).resolve()==ROOT
 out=ROOT/".aegis/results/FC-08-SECTION-B"/a.phase;out.mkdir(parents=True,exist_ok=False)
 def save(name,value):(out/name).write_text(json.dumps(value,indent=2)+"\n",encoding="utf-8")
 if a.action in ["preflight","audit"]:
  errors=[];refs=[]
  try:
   assert git("remote","get-url","origin")=="https://github.com/Mostorm-Labs/axiom.git"
   assert git("symbolic-ref","HEAD")=="refs/heads/codex/fc08-section-operations"
   subprocess.run([GIT,"merge-base","--is-ancestor",BASE,"HEAD"],cwd=ROOT,check=True)
   def check(value):
    if isinstance(value,dict):
     if "path" in value and "sha256" in value:
      path=ROOT/value["path"];assert path.resolve().is_relative_to(ROOT)
      assert hashlib.sha256(path.read_bytes()).hexdigest()==value["sha256"],value["path"];refs.append(value)
     for v in value.values():check(v)
    elif isinstance(value,list):
     for v in value:check(v)
   package=json.loads((ROOT/PKG/"package.json").read_text());check(package)
   for file in (ROOT/PKG).glob("*.json"):
    data=file.read_bytes();assert data==subprocess.check_output([GIT,"show",REV+":"+file.relative_to(ROOT).as_posix()],cwd=ROOT)
    check(json.loads(data))
   assert package["authorized_slice"]=="B only" and package["product_P32_authorized"] is True
   if a.action=="audit":
    allowed=json.loads((ROOT/PKG/"execution-contract.json").read_text())["allowed_files"]
    changed=set(git("diff","--name-only",BASE).splitlines()+git("ls-files","--others","--exclude-standard").splitlines())
    assert all(any(fnmatch.fnmatchcase(path,rule) for rule in allowed) for path in changed),sorted(changed)
    assert not git("diff",BASE,"--","runtime/semantic/src/qualification_checkpoint.hpp","schema/axiom/v1/descriptor","semantic-sdk.lock.json","r1-full-skia-sdk.lock.json","tests/fc08/reference",".aegis/packages/FC-08-SECTION-A-v0.1",".aegis/results/FC-08-SECTION-A")
    corpus=json.loads((ROOT/".aegis/materialized/FC-08/attempts/v0.3-control-remediation-02/corpus-manifest.json").read_text())
    for row in corpus["variants"]:
     for value in [row["input"],row["expected"],*row.get("wire",[])]:
      if "path" in value:assert hashlib.sha256((ROOT/value["path"]).read_bytes()).hexdigest()==value["sha256"],value["path"]
    assert len(corpus["variants"])==302

  except (AssertionError,OSError,ValueError,subprocess.CalledProcessError) as e:errors.append(str(e))
  for name in (["PackageBindingPreflight","EvidenceContractPreflight"] if a.action=="preflight" else ["ScopePreservationAudit"]):
   save(name+".json",{"name":name,"status":"PASS_SLICE_B" if not errors else "BLOCKED","package_revision":REV,"source_head":git("rev-parse","HEAD"),"verified_refs":refs,"errors":errors})
  print(json.dumps({"action":a.action,"errors":errors}));return int(bool(errors))
 source_files=sorted(set(git("ls-files","runtime/semantic").splitlines()+git("ls-files","--others","--exclude-standard","runtime/semantic").splitlines()))
 source_manifest=[{"path":f,"sha256":hashlib.sha256((ROOT/f).read_bytes()).hexdigest()} for f in source_files if (ROOT/f).is_file()]
 facts=json.loads((ROOT/"out/fc08-section-b/environment.json").read_text());env={**os.environ,**facts["environment"],"PYTHONUTF8":"1"};build=ROOT/"out/fc08-section-b/build";journal=[]
 def run(name,argv):
  r=subprocess.run(list(map(str,argv)),cwd=ROOT,env=env,capture_output=True,text=True,encoding="utf-8",errors="replace")
  (out/(name+".stdout.log")).write_text(r.stdout,encoding="utf-8");(out/(name+".stderr.log")).write_text(r.stderr,encoding="utf-8")
  journal.append({"argv":list(map(str,argv)),"exit_code":r.returncode});print(name+": "+str(r.returncode),flush=True);return r.returncode
 code=run("configure",["cmake","-S",ROOT,"-B",build,"-G","Ninja","-DCMAKE_BUILD_TYPE=Release","-DCMAKE_EXPORT_COMPILE_COMMANDS=ON","-DBUILD_TESTING=ON","-DCANVAS_BUILD_POC01=OFF","-DCANVAS_BUILD_SEMANTIC=ON","-DCANVAS_SEMANTIC_ENABLE_PROTOBUF=ON","-DCANVAS_BUILD_INK_PLAYGROUND=OFF","-DCANVAS_BUILD_RENDER=OFF","-DCANVAS_BUILD_RF01=OFF","-DCANVAS_SKIA_SDK_VARIANT=release","-DAXIOM_FC08_QUALIFICATION_HOOKS=OFF",*consumer_cmake_arguments(Path(env["AXIOM_SEMANTIC_RUNTIME_ROOT"]),Path(env["AXIOM_PROTOC"]))])
 if code==0:code=run("build",["cmake","--build",build,"--target",*a.targets,"--parallel","6"])
 if code==0:code=run("discover",["ctest","--test-dir",build,"--show-only=json-v1"])
 if code==0:
  tests=json.loads((out/"discover.stdout.log").read_text())["tests"]
  names=[t["name"] for t in tests if Path(t.get("command",[""])[0]).stem in a.targets];assert names
  (out/"ctest-names.txt").write_text("\n".join(names)+"\n",encoding="utf-8")
  code=run("ctest",["ctest","--test-dir",build,"--tests-from-file",out/"ctest-names.txt","--output-on-failure","--output-junit",out/"ctest.xml"])
 save("invocations.json",{"source_head":git("rev-parse","HEAD"),"cwd":str(ROOT),"commands":journal,"exit_code":code,"SDK_environment":facts["environment"],"dirty_status":git("status","--porcelain"),"semantic_source_manifest":source_manifest})
 return code
if __name__=="__main__":raise SystemExit(main())
