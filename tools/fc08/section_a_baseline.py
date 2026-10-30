"""Read-only baseline comparison for the pre-existing Shape/RichText assertion."""
import hashlib,json,os,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT))
from tools.semantic.cmake_consumer import consumer_cmake_arguments
SOURCE=Path("C:/Users/Chado/.codex/worktrees/fc08-verification-materialization/canvas")
BASE="427580ae13b60a39e88959b282cbffc695c9aabb"
GIT="D:/Softwares/Git/cmd/git.exe"

def main():
    out=ROOT/".aegis/results/FC-08-SECTION-A/baseline-regression-001"
    out.mkdir(exist_ok=False)
    head=subprocess.check_output([GIT,"rev-parse","HEAD"],cwd=SOURCE,text=True).strip()
    assert head==BASE
    assert not subprocess.check_output([GIT,"diff","HEAD","--","runtime/semantic","runtime/foundation","CMakeLists.txt","cmake","schema","semantic-sdk.lock.json"],cwd=SOURCE)
    same=subprocess.check_output([GIT,"diff","--name-only",BASE,"9d731f40515d69959c3d3941d889e062aa6bcdcf","--","runtime/semantic","runtime/foundation","CMakeLists.txt","cmake","schema","semantic-sdk.lock.json"],cwd=ROOT,text=True)
    assert not same.strip()
    facts=json.loads((ROOT/"out/fc08-section-a/environment.json").read_text())
    env={**os.environ,**facts["environment"],"PYTHONUTF8":"1"}
    build=ROOT/"out/fc08-section-a/baseline-build"
    invocations=[]
    def run(name,argv):
        r=subprocess.run(list(map(str,argv)),cwd=ROOT,env=env,capture_output=True,text=True,encoding="utf-8",errors="replace")
        (out/(name+".stdout.log")).write_text(r.stdout,encoding="utf-8")
        (out/(name+".stderr.log")).write_text(r.stderr,encoding="utf-8")
        invocations.append({"argv":list(map(str,argv)),"exit_code":r.returncode})
        print(name+": "+str(r.returncode),flush=True)
        return r.returncode
    code=run("configure",["cmake","-S",SOURCE,"-B",build,"-G","Ninja","-DCMAKE_BUILD_TYPE=Release","-DBUILD_TESTING=ON","-DCANVAS_BUILD_POC01=OFF","-DCANVAS_BUILD_SEMANTIC=ON","-DCANVAS_SEMANTIC_ENABLE_PROTOBUF=ON","-DAXIOM_BUILD_RUNTIME_QUALIFICATION=OFF","-DCANVAS_BUILD_RENDER=OFF","-DCANVAS_BUILD_RF01=OFF","-DCANVAS_SKIA_SDK_VARIANT=release","-DAXIOM_FC08_QUALIFICATION_HOOKS=OFF",*consumer_cmake_arguments(Path(env["AXIOM_SEMANTIC_RUNTIME_ROOT"]),Path(env["AXIOM_PROTOC"]))])
    if code==0:code=run("build",["cmake","--build",build,"--target","canvas_semantic_stateful_kind_applicability_test","--parallel","6"])
    if code==0:code=run("ctest",["ctest","--test-dir",build,"-R","^StatefulKindApplicability.StateRuleMatrixMatchesOracleForAllKinds$","--output-on-failure","--output-junit",out/"ctest.xml"])
    (out/"invocations.json").write_text(json.dumps({"subject_revision":head,"source":str(SOURCE),"semantic_sources_identical_to_slice_A_start":True,"invocations":invocations,"exit_code":code},indent=2)+"\n")
    return code
if __name__=="__main__":raise SystemExit(main())
