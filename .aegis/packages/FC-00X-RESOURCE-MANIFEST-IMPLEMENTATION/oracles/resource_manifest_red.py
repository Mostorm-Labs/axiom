#!/usr/bin/env python3
from pathlib import Path
import argparse, sys

def main():
 p=argparse.ArgumentParser(); p.add_argument('--root',type=Path,required=True); p.add_argument('--expect',choices=('baseline-red','implemented'),required=True); a=p.parse_args(); r=a.root.resolve()
 checks={
  'snapshot_type':r/'runtime/semantic/include/canvas/semantic/snapshot.hpp',
  'operation_type':r/'runtime/semantic/include/canvas/semantic/operation_payload.hpp',
  'operation_kind':r/'runtime/semantic/include/canvas/semantic/operation.hpp',
  'changeset_type':r/'runtime/semantic/include/canvas/semantic/change_set.hpp',
  'snapshot_proto':r/'schema/axiom/v1/proto/auditoryworks/axiom/v1/snapshot.proto',
  'operation_proto':r/'schema/axiom/v1/proto/auditoryworks/axiom/v1/operation.proto',
 }
 needles={
  'snapshot_type':('ResourceManifest','manifest'),
  'operation_type':('UpdateResourceManifestOp',),
  'operation_kind':('kUpdateResourceManifest',),
  'changeset_type':('ResourceManifestChangeFlags','resourceChanges'),
  'snapshot_proto':('ResourceManifest','document_digest','checkpoint_digest'),
  'operation_proto':('UpdateResourceManifestOp','update_resource_manifest = 16'),
 }
 found={}
 for key,path in checks.items():
  try:text=path.read_text(encoding='utf-8')
  except OSError:text=''
  found[key]=all(n in text for n in needles[key])
 if a.expect=='baseline-red':
  if any(found.values()): print('VERIFICATION_DESIGN_DEFECT: accepted semantic implementation markers already present at baseline',found); return 2
  print('RED: typed ResourceManifest, OperationKind=16, ChangeSet resourceChanges and Snapshot schema3 are absent at baseline'); return 1
 if not all(found.values()): print('FAIL: incomplete ResourceManifest implementation markers',found); return 1
 print('PASS: ResourceManifest implementation boundary present'); return 0
if __name__=='__main__':sys.exit(main())
