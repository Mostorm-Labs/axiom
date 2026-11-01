#!/usr/bin/env python3
"""Check a proposed SDK scope matrix against the immutable V1 declaration set.

The matrix is draft planning data; this tool cannot grant authority or prove SDK functionality.
"""
import argparse
import json
from pathlib import Path
import re
import sys

REQUIRED=set('''canvas_runtime_get_abi_version canvas_status_message canvas_get_last_error
canvas_runtime_create canvas_runtime_destroy canvas_runtime_get_capabilities canvas_runtime_copy_diagnostics
canvas_document_open canvas_document_close canvas_document_get_state canvas_view_create canvas_view_destroy
canvas_view_set_viewport canvas_view_set_camera canvas_view_get_camera canvas_view_pan_by canvas_view_zoom_at
canvas_view_screen_to_world canvas_view_world_to_screen canvas_view_push_pointer_batch canvas_view_push_wheel_event
canvas_view_push_key_event canvas_view_set_active_tool canvas_view_set_brush canvas_view_set_eraser
canvas_view_get_tool_state canvas_view_get_brush canvas_view_get_eraser canvas_view_execute_command canvas_view_undo
canvas_view_redo canvas_view_begin_command_batch canvas_view_end_command_batch canvas_view_cancel_command_batch
canvas_view_get_selection_summary canvas_view_copy_selection_ids'''.split())
OPTIONAL=set('''canvas_document_update_operation_durability canvas_document_apply_remote_presence
canvas_view_set_local_presence canvas_view_set_debug_overlay'''.split())
DEFERRED=set('''canvas_document_copy_recovery_frontier canvas_document_create_snapshot canvas_document_apply_remote_operations
canvas_document_provide_resource canvas_document_fail_resource canvas_document_evict_resource canvas_view_attach_surface
canvas_view_update_surface canvas_view_detach_surface canvas_view_push_ime_event canvas_view_get_text_input_state
canvas_view_copy_surrounding_text canvas_view_on_vsync canvas_view_render_frame'''.split())


def validate(header, matrix):
    errors=[]
    names=re.findall(r'CANVAS_RUNTIME_API\s+[^;]*?\b(canvas_[a-z0-9_]+)\s*\(',header,re.S)
    if len(names)!=54 or len(set(names))!=54:
        errors.append('expected 54 unique V1 declarations')
    rows=matrix.get('functions',[])
    if not isinstance(rows,list): return errors+['functions must be an array']
    symbols=[r.get('symbol') for r in rows if isinstance(r,dict)]
    if len(symbols)!=len(rows) or len(set(symbols))!=len(symbols): errors.append('duplicate or malformed function rows')
    if set(names)!=set(symbols): errors.append('header and matrix symbols differ')
    if REQUIRED|OPTIONAL|DEFERRED!=set(names) or REQUIRED&OPTIONAL or REQUIRED&DEFERRED or OPTIONAL&DEFERRED:
        errors.append('scope baseline is incomplete or overlapping')
    groups=matrix.get('case_groups',{})
    for row in rows:
        if not isinstance(row,dict):continue
        symbol=row.get('symbol'); expected='required' if symbol in REQUIRED else 'optional' if symbol in OPTIONAL else 'deferred'
        if row.get('disposition')!=expected: errors.append(f'{symbol}: unexpected disposition')
        if not row.get('owner') or not row.get('inventory_note') or not row.get('case_group') or row['case_group'] not in groups:
            errors.append(f'{symbol}: owner or case binding missing')
        if expected!='required' and (not row.get('future_owner') or row.get('unimplemented_behavior')!='not_supported_no_mutation'):
            errors.append(f'{symbol}: deferred/optional failure handling or owner missing')
        if expected=='required' and row.get('unimplemented_behavior')!='not_applicable': errors.append(f'{symbol}: required behavior cannot be deferred')
    for group,value in groups.items():
        if not isinstance(value,dict) or any(not value.get(k) for k in ('applies_to','fixture','oracle')): errors.append(f'{group}: case group incomplete')
    families=matrix.get('families',{})
    if set(families)!={f'API{i:02}' for i in range(1,13)}: errors.append('all API01-API12 families required')
    for key,value in families.items():
        if not value.get('sdk01_gate') or not value.get('future_owner'): errors.append(f'{key}: staged applicability incomplete')
    if not matrix.get('additional_required_candidate') or len(matrix.get('qualifications',[]))<3: errors.append('multi-selection or preserved commitments missing')
    return errors


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--header',type=Path,required=True)
    parser.add_argument('--matrix',type=Path,required=True)
    args=parser.parse_args()
    try:
        errors=validate(args.header.read_text(encoding='utf-8'),json.loads(args.matrix.read_text(encoding='utf-8')))
    except (OSError,ValueError,TypeError,KeyError,json.JSONDecodeError) as exc:
        print('BLOCKED_INPUT:',exc);return 2
    for error in errors: print('INVALID:',error)
    if errors:return 1
    print('DRAFT_SCOPE_STRUCTURALLY_VALID: 54 symbols and API01-API12; not authority or product evidence')
    return 0

if __name__=='__main__':sys.exit(main())
