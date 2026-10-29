#!/usr/bin/env python3
"""Validate draft P20 case structure; this does not accept it or run product tests."""
import argparse
import json
from pathlib import Path
import sys


def validate(data,root):
    errors=[]
    rows=data.get('cases',[])
    if not isinstance(rows,list) or not rows:return ['case list missing']
    if data.get('case_count')!=len(rows):errors.append('case count mismatch')
    ids=[x.get('id') for x in rows if isinstance(x,dict)]
    if len(ids)!=len(rows) or len(set(ids))!=len(ids) or any(not x for x in ids):errors.append('missing or duplicate case ID')
    if {x.get('family') for x in rows if isinstance(x,dict)}!={f'API{i:02}' for i in range(1,13)}:errors.append('missing API family')
    for row in rows:
        if not isinstance(row,dict):continue
        cid=row.get('id','<missing>')
        for key in ('family','requirement','fixture','expected','source_anchor','evidence'):
            if not row.get(key):errors.append(f'{cid}: missing {key}')
        if row.get('planned_status')!='NOT_MATERIALIZED':errors.append(f'{cid}: cannot represent a draft as product pass')
        path=(root/row.get('source_anchor','')).resolve()
        if not path.is_relative_to(root.resolve()) or not path.is_file():errors.append(f'{cid}: source anchor missing or outside repository')
    if data.get('status')!='DRAFT_NOT_EXECUTED_NOT_ACCEPTED':errors.append('draft status mismatch')
    return errors


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--repo',type=Path,required=True)
    parser.add_argument('--cases',type=Path,required=True)
    args=parser.parse_args()
    try:errors=validate(json.loads(args.cases.read_text(encoding='utf-8')),args.repo.resolve())
    except (OSError,TypeError,ValueError,json.JSONDecodeError) as exc:
        print('BLOCKED_INPUT:',exc);return 2
    for error in errors:print('INVALID:',error)
    if errors:return 1
    print('DRAFT_CASE_STRUCTURE_OK: no product test or authority pass implied')
    return 0

if __name__=='__main__':sys.exit(main())
