#!/usr/bin/env python3
import argparse,json,pathlib,subprocess,sys,traceback,xml.etree.ElementTree as ET
P=pathlib.Path(__file__).resolve().parent
ROOT=P.parents[2]
BRANCH="codex/fc04-mse-phase2-p31-v0-1-d07cursor"
TASK="2fee75f66a7f96f6aac834228ede372a9abda59a"
CURSOR="6bcf2d4bbf3314e5252951190c3c475403f52665"
def git(*args): return subprocess.check_output(["git",*args],cwd=ROOT,text=True,stderr=subprocess.STDOUT).strip()
def read(p): return json.loads(p.read_text(encoding="utf-8"))
def require(ok,msg):
 if not ok: raise AssertionError(msg)
def ancestor(a,b): subprocess.run(["git","merge-base","--is-ancestor",a,b],cwd=ROOT,check=True)
def blob(rev,path): return git("rev-parse",f"{rev}:{path}")
def verify(lock_name,key):
 data=read(P/lock_name); require(data["subject_revision"]==CURSOR,f"{lock_name} subject mismatch")
 for item in data[key]:
  actual=blob(CURSOR,item["path"]); require(actual==item["git_blob_sha"],f"locked blob mismatch: {item['path']}")
def package_binding():
 pkg=read(P/"package.json")
 require(git("branch","--show-current")==BRANCH,"wrong branch")
 remote=git("remote","get-url","origin").lower().removesuffix(".git")
 require(remote in ("https://github.com/mostorm-labs/axiom","git@github.com:mostorm-labs/axiom"),"wrong origin "+remote)
 head=git("rev-parse","HEAD"); ancestor(TASK,head); ancestor(CURSOR,head)
 require(pkg["task_anchor"]["revision"]==TASK and pkg["resume_cursor"]["revision"]==CURSOR,"anchor/cursor mismatch")
 verify("source-lock.json","files"); verify("input-lock.json","files")
 integrity=read(P/"integrity.json")
 for item in integrity["files"]:
  require(blob(head,P.relative_to(ROOT).as_posix()+"/"+item["path"])==item["git_blob_sha"],"package integrity mismatch "+item["path"])
 require(pkg["p32_authorized"] is False and pkg["requires_control_review"] is True,"P32 prematurely authorized")
 print("PASS PackageBindingPreflight: repository, branch, anchors, cursor, source/input locks, package integrity; HEAD="+head)
def evidence_contract():
 verify("source-lock.json","files"); verify("input-lock.json","files")
 m=read(P/"test-matrix.json")
 require(len(m["test_cases"])==27 and len({x["id"] for x in m["test_cases"]})==27,"MSE case set is not exactly 27")
 for row in m["inherited_regressions"]:
  path=ROOT/row["source"]
  content=path.read_text(encoding="utf-8")
  require(len(content)>0,"empty inherited evidence "+row["id"])
  if path.suffix==".xml":
   root=ET.fromstring(content)
   names=[node.attrib.get("name","") for node in root.iter("testcase")]
   require(len(names)==row["expected_count"],"inherited test count mismatch "+row["id"])
   require(not any(node.tag in ("failure","error","skipped") for node in root.iter()),"inherited JUnit has failure/error/skip "+row["id"])
  else:
   names=[line.strip() for line in content.splitlines() if line.strip()]
   require(len(names)==row["expected_count"],"frozen ID list count mismatch "+row["id"])
 d07_names=[line.strip() for line in (ROOT/".aegis/results/FC04-D07-P31-CORRECTION-v0.2/raw/d07-names.txt").read_text(encoding="utf-8").splitlines() if line.strip()]
 selection_names=[line.strip() for line in (ROOT/".aegis/results/FC04-D07-P31-CORRECTION-v0.2/raw/selection-names.txt").read_text(encoding="utf-8").splitlines() if line.strip()]
 d07_xml=ET.fromstring((ROOT/".aegis/results/FC04-D07-FUNCTIONAL-P32-001/raw/final-551/d07_14.xml").read_text(encoding="utf-8"))
 selection_xml=ET.fromstring((ROOT/".aegis/results/FC04-D07-FUNCTIONAL-P32-001/raw/final-551/selection_160.xml").read_text(encoding="utf-8"))
 d07_actual=[node.attrib.get("name","") for node in d07_xml.iter("testcase")]
 selection_actual=[node.attrib.get("name","") for node in selection_xml.iter("testcase")]
 require(len(d07_actual)==14 and set(d07_actual)==set(d07_names),"D07 frozen IDs do not exactly match JUnit")
 require(len(selection_actual)==160 and set(selection_actual)==set(selection_names),"Selection frozen IDs do not exactly match JUnit")
 require(not any(node.tag in ("failure","error","skipped") for node in d07_xml.iter()),"D07 inherited JUnit has failure/error/skip")
 require(not any(node.tag in ("failure","error","skipped") for node in selection_xml.iter()),"Selection inherited JUnit has failure/error/skip")
 require(m["oom_blocker"]["status"]=="BLOCKED_KNOWN_HIGH_IMPACT_OOM_HANG","OOM blocker lost")
 print("PASS EvidenceContractPreflight: 27 MSE IDs, inherited D07/Selection evidence and OOM blocker bound")
def design():
 closure=(P/"closure.md").read_text(encoding="utf-8")
 d=(P/"design.md").read_text(encoding="utf-8")
 c=read(P/"implementation-context.json")
 require("D06-B" in closure and "MSE-01..MSE-27" in closure,"closure missing authority or tests")
 require("No new FieldId" in closure and "No Group/Ungroup" in closure,"scope boundaries missing")
 require("No public RuntimeFacade virtual methods" in json.dumps(c),"public ABI prohibition missing")
 require("BLOCKED_KNOWN_HIGH_IMPACT_OOM_HANG" in closure,"OOM terminal status missing")
 require("Indeterminate/Unavailable" in d,"unknown applicability rule missing")
 print("PASS DesignPreflight: D06-B, D07 intent, phase boundary and ABI/OOM limits bound")
def functional_scope():
 c=read(P/"implementation-context.json"); m=read(P/"test-matrix.json")
 allow=set(c["allowed_modification_paths"])
 require("runtime/interaction/src/canvas_editing_runtime.cpp" in allow,"Runtime owner not allowed")
 require("debug_ui/src/panels/control.cpp" in allow,"UI owner not allowed")
 require("schema/axiom/v1/registry/field_registry_v1.yaml" in c["readonly_paths"],"registry must remain read-only")
 require("runtime/foundation/include/canvas/runtime/runtime_facade.hpp" in c["forbidden_paths"],"RuntimeFacade ABI must be forbidden")
 require("Group/Ungroup/Wrap/Unwrap/Snap/Alignment product implementation" in c["forbidden_paths"],"Phase 3 scope boundary missing")
 require(len(m["required_property_rows"])==8,"property matrix incomplete")
 print("PASS FunctionalScopePreflight: property matrix and source/phase allowlist bound")
def main():
 ap=argparse.ArgumentParser(); ap.add_argument("--repo",required=True); ap.add_argument("--check",choices=["PackageBindingPreflight","EvidenceContractPreflight","DesignPreflight","FunctionalScopePreflight","all"],default="all"); a=ap.parse_args()
 try:
  require(pathlib.Path(a.repo).resolve()==ROOT.resolve(),"--repo must identify this worktree root")
  checks={"PackageBindingPreflight":package_binding,"EvidenceContractPreflight":evidence_contract,"DesignPreflight":design,"FunctionalScopePreflight":functional_scope}
  for name in checks if a.check=="all" else [a.check]: checks[name]()
 except Exception:
  traceback.print_exc(); sys.exit(2)
if __name__=="__main__": main()
