#!/usr/bin/env python3
"""Check draft additive C declarations; no export or compatibility is proven."""
import argparse
from pathlib import Path
import re
import sys

EXTENSION=set('''canvas_sdk_runtime_pump canvas_sdk_multi_set_permission canvas_sdk_multi_capture
canvas_sdk_multi_copy_roots canvas_sdk_multi_copy_eligible canvas_sdk_multi_copy_excluded
canvas_sdk_multi_copy_uniform_value canvas_sdk_multi_copy_root_value canvas_sdk_multi_submit
canvas_sdk_multi_receipt canvas_sdk_multi_copy_changed canvas_sdk_multi_copy_unchanged
canvas_sdk_multi_copy_skipped canvas_sdk_multi_release_token'''.split())


def symbols(source):
    return re.findall(r'CANVAS_RUNTIME_API\s+[^;]*?\b(canvas_[a-z0-9_]+)\s*\(',source,re.S)


def validate(base,extension):
    errors=[]
    old=symbols(base);new=symbols(extension)
    if len(old)!=54 or len(set(old))!=54:errors.append('V1 declaration inventory mismatch')
    if len(new)!=len(set(new)) or set(new)!=EXTENSION:errors.append('extension symbols differ or duplicate')
    if set(old)&set(new):errors.append('extension collides with V1')
    if '#include "canvas_runtime_api_v1.h"' not in extension:errors.append('canonical V1 include absent')
    if '#ifdef __cplusplus' not in extension or 'extern "C"' not in extension:errors.append('C language linkage missing')
    return errors


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--base',type=Path,required=True)
    parser.add_argument('--extension',type=Path,required=True)
    args=parser.parse_args()
    try:errors=validate(args.base.read_text(encoding='utf-8'),args.extension.read_text(encoding='utf-8'))
    except OSError as exc:print('BLOCKED_INPUT:',exc);return 2
    for error in errors:print('INVALID:',error)
    if errors:return 1
    print('DRAFT_ADDITIVE_SYMBOLS_OK: 54 V1 and 14 extension declarations; not a compatibility or export test')
    return 0

if __name__=='__main__':sys.exit(main())
