"""Bind the pending-acceptance return; never infer a control acceptance."""
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / '.aegis/results/FC-08-P20-P31-CLOSURE/attempts/control-001'
PACKAGE = '.aegis/packages/FC-08-P20-P31-INPUT-AMENDMENT-v0.2/package.json'


def ref(path):
    data = (ROOT / path).read_bytes()
    return {'path':path,'sha256':hashlib.sha256(data).hexdigest(),'size':len(data)}


def write(name, value):
    path = OUT / name
    with path.open('x',encoding='utf-8',newline='\n') as stream:
        stream.write(json.dumps(value,ensure_ascii=False,indent=2)+'\n')
    return ref(path.relative_to(ROOT).as_posix())


def main():
    git = ['D:/Softwares/Git/cmd/git.exe','-C',str(ROOT)]
    head = subprocess.check_output(git+['rev-parse','HEAD'],text=True).strip()
    assert head == 'eb12da1acc4516e115d36767073f8bdc607f8a4d'
    package = json.loads((ROOT/PACKAGE).read_text(encoding='utf-8'))
    assert package['accepted'] is False
    run = json.loads((OUT/'run-002/invocations.json').read_text(encoding='utf-8'))
    assert run['observed_head'] == head and run['all_commands_exit_zero']
    dispositions = write('candidate-dispositions.json', {
        'candidate01':{'status':'SUPERSEDED_UNACCEPTED_CANDIDATE',
                      'package':ref('.aegis/packages/FC-08-P20-P31-INPUT-AMENDMENT-v0.1/package.json'),
                      'audit':ref((OUT/'candidate-01-audit.json').relative_to(ROOT).as_posix()),
                      'finding':'FC08-S05/empty-Group-paint and empty-Group-identity retained stale graph S/a/b while canonical records contained only empty G.',
                      'product_failure_claimed':False},
        'candidate02':{'status':'PROPOSED_PENDING_ACCEPTANCE', 'package':ref(PACKAGE),
                      'audit':ref((OUT/'run-002/audit.json').relative_to(ROOT).as_posix()),
                      'accepted':False}})
    # All artifacts in this task except the self-referential index/manifest/receipt.
    files = subprocess.check_output(git+['ls-files','--others','--exclude-standard','--cached'],text=True).splitlines()
    paths = sorted({p for p in files if p.startswith((
        '.aegis/materialized/FC-08/attempts/v0.3-control-remediation',
        '.aegis/packages/FC-08-P20-P31-INPUT-AMENDMENT-',
        '.aegis/results/FC-08-P20-P31-CLOSURE/')) or p in {
            'tools/fc08/propose_input_amendment.py','tools/fc08/audit_input_amendment.py',
            'tests/fc08/input_amendment_audit_test.py'}})
    manifest = write('evidence-manifest.json', {
        'format':'fc08-control-remediation-evidence-v1',
        'candidate_result_revision':head, 'artifacts':[ref(p) for p in paths],
        'nonclaim':'These files establish reviewable candidates and control-tool checks, not P31 acceptance, product execution or a Gate verdict.'})
    result = {
        'task_id':'FC-08-P20-P31-BLOCKER-CLOSURE', 'stage':'P20/P31 control remediation',
        'terminal_state':'BLOCKED_PENDING_VERIFICATION_ACCEPTANCE','return_surface':'CONTROL_REVIEW',
        'success':False,'product_P32_authorized':False,
        'repository':{'provider':'github','full_name':'Mostorm-Labs/axiom'},
        'result_revision':head,'materialized_ref':'refs/heads/codex/fc08-p20-p31-closure',
        'receipt_policy':'An evidence-only descendant stores exact-revision post-commit run002 and this receipt; it does not alter candidate inputs/tools at result_revision. Final response identifies that receipt commit.',
        'qualification_subject_revision':'427580ae13b60a39e88959b282cbffc695c9aabb',
        'bootstrap_receipt_revision':'63ea152b2944d21120c968d5e11e166d1096b44f',
        'parent_product_package_revision':'f67b9db29090449799bd3e05ef7f1d269dab6d31',
        'candidate_package_ref':dict(ref(PACKAGE),revision=head),
        'source_truth_decision':package['source_truth_decision'],
        'matrix':package['matrix'],'matrix_schema':package['matrix_schema'],
        'method_bindings':package['method_bindings'],'corpus':package['corpus'],
        'changed_input_ledger':package['change_ledger'],
        'candidate_dispositions':dispositions,'evidence_manifest':manifest,
        'actual_invocations':ref((OUT/'run-002/invocations.json').relative_to(ROOT).as_posix()),
        'tool_checks':{'candidate_consistency':'PASS_CANDIDATE_CONSISTENCY_ONLY','unit_tests':8,
                       'all_recorded_command_exit_codes':0,'Axiom_product_tests_run':0,'SUT_invocations':[]},
        'coverage':{'logical_cases':60,'unique_variants':302,'families':10,'profile_cells':906,
                    'corrected_inputs':16,'unchanged_expected_files':302,
                    'windows_native_applicable':302,'web_wasm_applicable':302,
                    'headless_applicable':148,'headless_not_applicable':154,
                    'shared_controls_per_profile':3,'environment_assessment':'NOT_ASSESSED'},
        'blockers':[
            {'id':'BOOT-VER-01','classification':'MISSING_VERIFICATION_AUTHORITY_ACCEPTANCE',
             'owner':'P20 Verification / control review',
             'status':'SOURCE_BACKED_VERSIONED_CORRECTIONS_PROPOSED_NOT_ACCEPTED',
             'decision_required':'Accept or amend exact candidate02 decisions: shared ellipse13, original S04 rectangles1, empty-Group graph2. Expected files/oracles unchanged.'},
            {'id':'BOOT-PKG-02','classification':'MISSING_PROFILE_BINDING_ACCEPTANCE',
             'owner':'P31 control review',
             'status':'EXACT_302_ID_MATRIX_MATERIALIZED_NOT_ACCEPTED',
             'decision_required':'Accept or amend the bound matrix/schema/method book; verify explicit headless exclusions and shared host/oracle-control roles.'}],
        'downstream':{name:{'status':'NOT_RUN_PENDING_VERIFICATION_ACCEPTANCE','exit_code':None}
                       for name in ['PackageBindingPreflight','EvidenceContractPreflight','ImplementationDesignPreflight','actual_product_runner_and_tests','FC08-RED-01']},
        'resume_cursor':{'execution_ref':'refs/heads/codex/fc08-p20-p31-closure',
                         'revision':head,'candidate_package_sha256':ref(PACKAGE)['sha256'],
                         'next_action':'Obtain explicit control acceptance of candidate02 source truth and profile matrix. Then rerun audits, bind revised exact P31 locks, both package preflights, ImplementationDesignPreflight, real runner/tests and FC08-RED-01. Do not reuse old preflight PASS or start product P32.'},
        'preserved':'All previously tracked files are byte-identical to63ea. Old inputs/expected/oracles/catalog/CR01/descriptor locks/history remain intact. No production runtime/schema/global registry edits or main merge.',
        'physical_debt':'Existing PH01-PH04/PX0/FC13 obligations unchanged; no physical observations claimed.'}
    result_ref = write('terminal-result.json',result)
    print(json.dumps({'terminal_state':result['terminal_state'],'result_revision':head,'receipt':result_ref},indent=2))


if __name__ == '__main__':
    main()
