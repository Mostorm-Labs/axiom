"""Materialize the control-owned P20 correction and an authorized work order."""
from pathlib import Path
import hashlib
import json

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent

def load(p): return json.loads((ROOT / p).read_text())
def ref(p):
    path = ROOT / p
    return {'path': p, 'sha256': hashlib.sha256(path.read_bytes()).hexdigest(), 'identity_kind': 'local-content-addressed'}
def put(name, value): (HERE / name).write_text(json.dumps(value, indent=2, ensure_ascii=False) + '\n')

review = '.aegis/results/G5-UR0b/control-review-resume-r4/'
old = '.aegis/verification/G5-UR0b-initialization-v1/'
version = '.aegis/verification/G5-UR0b-correctness-harness-v1/'
source = load('.aegis/results/G5-UR0b/resume-r4/result.json')['source_tree_sha256']
input_refs = [ref(old + 'evidence-inputs.json'), ref(review + 'result.json'), ref(review + 'pixel-diagnostic.json'),
              ref(review + 'timing-diagnostic.json'), ref(review + 'recompilation-check.json'),
              ref('.aegis/results/G5-UR0b/control-review-init-repair-r1/reuse-acceptance.json')]
bundle = 'verification/evidence/performance/g5-unified-render-ur0b/' + source + '/resume-r4/'
for hz in (120, 240):
    base = bundle + f'execution-and-checks/execution/raw-runs/realtime-{hz}/'
    input_refs += [ref(base + n) for n in ['worker-result.json', 'raw-trace.jsonl', 'run-inputs.json', 'compiled-summary.json']]
put('evidence-inputs.json', {'schema': 'ur0b-correctness-harness-inputs-v1', 'refs': input_refs,
    'immutable_recompilation_population': '15 original initialization-v1 inputs plus archived realtime120 and240 =17 complete recordings',
    'partial_case': 'r4 lower_z_translucent_drag OFF0/OFF1 only; use archived RGBA, AA edge, public state and request for offline RED/diagnosis, never relabel original worker',
    'prior_cumulative_starts': 33, 'prior_cumulative_seconds': 1251, 'all_old_windows': 'CLOSED_UNCHANGED'})

cases = [
    {'id': 'REAL-OLD-HUD-RED', 'oracle': 'Run the original comparator/request against archived OFF1 and assert16112 differences; show the new retained-selection request handles the same archived public selection bounds. Preserve original FAILED and label recomputation OFFLINE.', 'red_required': True},
    {'id': 'RETAINED-SELECTION', 'oracle': 'Hand tool with count1 still excludes the public selection perimeter; count0 excludes none. Reject missing/nonfinite/unordered bounds instead of returning green.'},
    {'id': 'LIVE-SUBJECT', 'oracle': 'Independently compute fixed expected transform from before public bounds and rectangle center; compare after public selection and native-decoded snapshot object transform. Wrong target/id/camera, no move, unsnapped12, wrong generation or incomplete Up must fail applicability.'},
    {'id': 'HUD-SUPPORT', 'oracle': 'Freeze projected public before+expected-live bounds, 7px perimeter and rotation support to38px above top; only the fixed live horizontal guide y260 +/-2logicalpx may be added. Validate DPR1/2/4 projection and20% cap. No guide mask afterUp/hand.'},
    {'id': 'BODY-MUTANTS', 'oracle': 'A body-interior pixel mutation outside HUD/reference AA must still FAIL. Include a lower-z overlap/order or opacity error outside HUD, wrong transform, and mask leakage. These can be deterministic synthetic/reference fixtures; masks must not depend on candidate bytes.'},
    {'id': 'ACTUAL-OFF0-DIAGNOSTIC', 'oracle': 'Recompare archived OFF0 with the frozen independent HUD recipe; preserve all residual mismatches and their coordinates. Do not require it to become green; the outcome is offline diagnosis, not a new paired correctness result.'},
    {'id': 'BOUNDED-TRANSFER', 'oracle': 'Byte-for-byte roundtrip0/1/32768/batch boundary/multi-MiB data; exact offsets/length and SHA. Reject duplicate, overlap, gaps, out of order, over-limit batch, bad encoding and incomplete release. Retain allocation ownership and cancellation.'},
    {'id': 'STAGE-TERMINALS', 'oracle': 'Deterministic fake await timeout records the exact last-started stage; every completed stage has paired begin/end and monotonic clock. Assert original30s phase timer and90s worker guard unchanged. No resetting the timer at toggle/step/batch.'},
    {'id': 'CREATION-PROFILES', 'oracle': 'Original exact worker/consumer profile remains valid; the one exact repaired worker profile is additionally pinned with unchanged consumer and all new cold helpers. Unknown or mismatched worker/consumer/helper hashes fail. No wildcard/current-file-hash auto-acceptance.'},
    {'id': 'PAIRED-INIT-OWNERS', 'oracle': 'If paired reload emits an initialization completion, it must match exactly one recorded public init or paired_init state by full native owner/epoch/view, with generation0/empty/no-operation and existing chain checks. Foreign, ambiguous, restored/nonempty or absent state stays UNRESOLVED; do not invent an Operation or substitute lastCommit epoch.'},
    {'id': 'OLD17-INVARIANCE', 'oracle': 'Recompile17 immutable complete inputs into a new directory. Existing canonical populations, committed receipts, metrics/common stats, snapshot/terminal outcomes and source/artifact identity remain identical apart from explicit compiler provenance. OFFLINE is never fresh.'},
    {'id': 'FROZEN-SCOPE', 'oracle': 'Diff confirms product/SDK/collector/consumer/input schedule and all non-correctness work unchanged. No browser/qualification oracle probe, rebuild, old-window mutation, deadline/AA change or new sampling in this offline package. The two existing malformed-snapshot native unit checks are allowed and separately counted.'}
]
put('oracle-cases.json', {'schema': 'ur0b-correctness-harness-oracles-v1', 'cases': cases,
    'classification': 'Required for this narrow harness repair only; not new G5 product features or a new performance Gate'})

spec = {
    'schema': 'ur0b-correctness-harness-verification-v1', 'status': 'CURRENT_FOR_SUCCESSOR_HARNESS_REPAIR',
    'role': 'aegis-verification / P20, control conversation',
    'authority': {'user': 'Existing explicit design/control here and implementation in adjacent conversation; latest user identified the mutual-wait deadlock. Resolve the known verification gap and dispatch the previously authorized next step.',
                  'predecessor_handoff_ref': ref(review + 'ownership-handoff.json'),
                  'inherited': [ref('.aegis/packages/G5-UR0b/verification.lock.json'), ref(old + 'verification-spec.json'), ref(old + 'completion-contract.json')]},
    'objective': 'Repair the already-frozen lower-z live/canonical correctness evidence and bound its cold transport, without changing product rendering or weakening its oracle.',
    'non_goals': ['No Render/Brush/Input/Operation/Scene/SDK/API change', 'No performance optimization or UR0c/UR1',
                  'No new browser runs/main matrix in this repair slice', 'No official P34 or performance PASS'],
    'required_analysis': ['Current Runtime owns transform/snapping and selection decoration; SDK remains root public facade.',
                          'Pixel candidate is an actual framebuffer; reference remains the existing independent native CPU geometry/AA oracle, never candidate pixels.',
                          'Cold transport and paired fixture work must be separated from product latency and primary sampling.'],
    'live_reference_contract': {
        'fixture_unchanged': 'Existing257-point round-handdrawn-v4 stroke, rectangle from(280,150)to(540,370), lower-z selected stroke, down(100,100), move/up(124,112), fixed camera scale1,x0,y0.',
        'expected_transform': 'Before selection bounds B and identity transform are required. Expected tx=24; ty=260-(B.top+B.bottom)/2. This is the fixed center-alignment fixture expectation, not a general reimplementation of snapping. Expected committed matrix[1,0,0,1,24,ty].',
        'applicability': 'Same document, selected stroke ID, two object identities/order, unchanged camera/DPR across live capture and post-Up reference. Capture before/live/after public state. Before/live generation unchanged; afterUp exactly one transform commit, activeContacts0, canonical completion. The public after transform and native-decoded snapshot object transform must match the independent expected matrix within1e-4logical units. Wrong fixture rejects; no adaptive movement/search.',
        'reference': 'Use actual afterUp opaque snapshot through unchanged native pixel oracle for live candidate only after subject checks; the expected move and same final pointer position define the target state. Snapshot remains original bytes; no candidate-derived geometry. If the live body differs from that expected state outside permitted HUD/reference AA, retain FAILED.',
        'hud': {'source': 'Only fixed fixture geometry plus archived/current public selection bounds, camera and viewport metrics; never candidate colors/difference support.',
                'selection_perimeter_halfwidth_logical_px': 7, 'rotation_support': 'center x +/-7logicalpx; from top-38 through top.38 covers32px gap+4px grip+stroke/AA support.',
                'live_bounds': 'Project original public bounds and the independent expected translated bounds. Do not blindly add raw pointer delta12 or use post-state to choose a convenient expected transform.',
                'live_guide': 'Only lower-z step0: horizontal fixed fixture alignment y260 +/-2logicalpx, clipped to canvas width. At other steps no guide strip.',
                'hand_step': 'Read actual selection; use its public bounds if retained, including in hand mode. No assumption that tool change clears selection.',
                'max_excluded_fraction': 0.20},
        'pixel_tolerance': 'UNCHANGED: <=2 channel levels outside independent native geometry AA edge and declared HUD; no compared pixels or HUD>20% ->MISSING; retained differences ->FAILED.',
        'semantic_and_other_cases': 'Retain six original correctness cases and OFF/ON pairing rules, masks, transparency, background, resource pressure, DPR and feedback requirements. No PRODUCT_RED_IN_BOTH without complete matching pair.'},
    'cold_transport_and_stage_contract': {
        'scope': 'Correctness worker only. Primary timing/input scheduling, diagnostic/cohort/RT paths and public SDK remain unchanged.',
        'unit_bytes_max': 32768, 'batch_decoded_bytes_max': 262144, 'batch_units_max': 8,
        'representation': 'Bounded base64/binary-safe encoding is allowed across the verification host boundary. Existing snapshotChunk/pixelChunk can be invoked several times inside one page.evaluate, preserving each32KiB unit. Never return a document/frame-sized numeric JS array.',
        'validation': 'Sequential exact offset/length, per-unit and batch bounds, exact final byte count and source/received SHA256; explicit finish/cancel ownership and no duplicate read. Pixel source hash is computed once in cold path if needed, not during primary sampling.',
        'stage_markers': ['correctnessNext/render/settle begin/end', 'pixel readback/hash begin/end', 'pixel transfer begin/end', 'snapshot acquire/transfer begin/end',
                          'native reference/compare begin/end', 'close begin/end', 'metrics override/reload begin/end', 'ON init/setup begin/end'],
        'markers_rule': 'Coarse stage records only, not per pixel/sample. If render and readback remain one browser evaluate, add coarse browser markers in the existing correctnessDriver; do not claim exact await attribution beyond available brackets.',
        'correctness_phase_ms': 30000, 'outer_worker_seconds': 90,
        'deadline': 'Keep original phase coverage and arm placement; ON/steps/batches never reset30s. A new timeout still closes its separately authorized future window.'},
    'initialization_profile_compatibility': {
        'old_profile': 'Preserve both original exact worker and consumer hashes. No migration of historical raw evidence.',
        'new_profile': 'May add one exact repaired-worker profile, with the original consumer hash unchanged and helper/driver/pixel hashes pinned. Materialize final literal hashes before use in proof. Original createRuntime/server/shim/clock/raw collection and all non-correctness branches must be unchanged apart from imported cold-only helpers/correctness-gated code. No hash wildcard or accepting whatever current file exists.',
        'paired_state': 'For the repaired correctness profile only, public init and paired_init may be distinct recorded state candidates. Match each initialization completion to a unique state by document+attachment+view+document epoch and the existing full owner/native chain. Enforce all current fresh-empty restrictions and count every accepted raw record.',
        'historical_reuse': 'Original15 plus120/240 keep their exact recorded profiles and identities. All17 must recompile unchanged; repaired profile identity is a distinct applicability case, not historical source rewriting.'},
    'evidence_qualification': {
        'failure_mode': 'HUD or wrong live reference can report false render defects or hide actual order/alpha/mask/transform defects; cold extraction can exhaust a correctness deadline without identifying the stalled stage.',
        'impact': 'Incorrect prerequisite admission or repeated premature stops prevent valid G5 performance comparison.',
        'existing_independent_coverage': 'Native independent CPU geometry/AA oracle, semantic fixtures, actual OFF archives and original native render regressions.',
        'residual_proof_gap': 'Same-subject live-vs-canonical applicability and candidate-independent HUD handling in the actual browser harness.',
        'new_evidence_unique_detection_value': 'Real archived RED plus targeted body/order/alpha negatives establish that excluding known decorations does not exclude document errors; byte-exact transport tests protect captured evidence.',
        'blocking_justification': 'Direct repair of the already-frozen six-case prerequisite; no new product closure target.',
        'corroborative': 'Additional marker detail and cleaner failure text improve diagnosis; no independent product Gate is added for them.'},
    'required_output': ['Minimal harness repair and tested old-archive RED', '17 immutable-input invariant recompilations',
                        'New source/profile identity, honest residual pixel findings, content-addressed result and reuse assessment',
                        'Prepared future six-correctness-plus30main continuation position, not executed'],
    'quality_evidence_gate': {'required': ['All12 bounded oracle groups', 'Node/Python/boundary suites appropriate to allowed changes',
                                          'No old archive/source authority edits; no browser/build/qualification oracle probe in this offline slice. Existing malformed-snapshot native unit checks only are permitted and counted.',
                                          'Return READY_FOR_CONTROL_REVIEW or explicit frozen blocker and notify control thread'],
                              'official_gate_pass': False},
    'handoff': {'next_owner': 'aegis-implementation in adjacent thread', 'work_order': 'implementation-request.json',
                'instruction': 'Materialize P31 from the approved exact work order, then perform the whole offline repair; do not wait for another approval when preflight is clean.'},
    'oracle_ref': ref(version + 'oracle-cases.json'), 'inputs_ref': ref(version + 'evidence-inputs.json'),
    'supersession': 'Additive successor Verification for the new harness repair only. Originalr4 package/window and all prior failures/thresholds stay immutable.'}
put('verification-spec.json', spec)

paths = ['verification/runners/g5/ur0/correctness_driver.mjs', 'verification/runners/g5/ur0/pixel_oracle.mjs',
         'verification/runners/g5/ur0/stream_transfer.mjs', 'verification/runners/g5/ur0/correctness_transfer.mjs',
         'verification/runners/g5/ur0/correctness_contract.mjs', 'verification/runners/g5/ur0/worker.mjs',
         'verification/runners/g5/ur0/initialization_basis.mjs',
         'verification/tests/g5_ur0_pixels.test.mjs', 'verification/tests/g5_ur0_initialization.test.mjs',
         'verification/tests/g5_ur0_correctness.test.mjs', 'verification/tests/g5_ur0_transfer.test.mjs']
request = {'schema': 'ur0b-correctness-harness-work-order-v1', 'status': 'AUTHORIZED_OFFLINE_HARNESS_REPAIR',
    'owner': 'aegis-implementation in existing adjacent conversation', 'stage_request': ['P30/P31 exact package materialization', 'P32 offline repair until terminal return'],
    'authorization': 'User explicitly delegated design/control here and implementation to adjacent thread, requested continuation and terminal notification, and now identified the mutual-wait handoff. This work order makes that already-authorized repair concrete; no separate reconfirmation is needed.',
    'repository': {'provider': 'github', 'full_name': 'Mostorm-Labs/axiom'}, 'worktree': str(ROOT),
    'execution_ref': 'refs/heads/codex/geometry-navigation-integration',
    'resume_cursor': {'revision': 'f651523a40c9b383b7d8e2115fcd32f75ddc38a7', 'dirty': True, 'accepted_predecessor_source_sha256': source,
                      'new_control_delta': '.aegis/verification/G5-UR0b-correctness-harness-v1/** is an explicitly authorized additive control input. Compute actual current full source in the new proof; do not require it to equal the predecessor hash or discard the new control files.',
                      'preserve_existing_dirty_work': True, 'next_action': 'Materialize exact new package and begin offline harness repair now.'},
    'current_verification_spec_ref': ref(version + 'verification-spec.json'), 'oracle_ref': ref(version + 'oracle-cases.json'),
    'evidence_input_ref': ref(version + 'evidence-inputs.json'),
    'package_to_materialize': '.aegis/packages/G5-UR0b-correctness-repair-r1/package.json',
    'package_rule': 'Freeze required/forbidden source scope, tests, exact bindings and terminal criteria before code. Clean scope preflight authorizes implementation without another handoff pause. Producer may not change P20 or self-issue official Gate PASS.',
    'source_scope': paths,
    'scope_details': 'New helper paths are optional. Existing stream API remains compatible; the exact existing test is g5_ur0_transfer.test.mjs. No arbitrary wildcard source allowance.',
    'read_only': ['Runtime, SDK, public docs/API/dependencies/locks/build/collector', 'consumer.mjs and all input/scheduling code',
                  'All old packages/results/evidence/window/ledger bytes', 'AllP20 files in this directory'],
    'output_paths': ['.aegis/packages/G5-UR0b-correctness-repair-r1/**', '.aegis/results/G5-UR0b/correctness-repair-r1/**',
                     'out/ur0b/correctness-repair-r1/**', 'verification/evidence/performance/g5-unified-render-ur0b/<new-source-sha>/correctness-repair-r1/**'],
    'required_changes': ['Implement frozen live subject/HUD contract and real old OFF1 RED.',
                         'Implement byte-exact bounded cold-only batched transfer and coarse stage diagnosis; keep deadlines and non-correctness behavior.',
                         'Pin one exact repaired creation profile additively; handle paired_init only with uniquely matched full native ownership and all existing fresh-empty checks.',
                         'Complete targeted positive/negative tests and all17 immutable recompilations; preserve every original verdict and source identity.',
                         'Assess reuse of accepted old diagnostic/cohort/60/120/240 for a future continuation; report any changed applicable condition honestly.'],
    'required_tests': ['node --test verification/tests/g5_*.test.mjs verification/sdk_web_external/pointer_input_test.mjs verification/sdk_web_external/control_api_test.mjs',
                       "python3.12 -m unittest discover -s verification/tests -p 'test_g5_*.py'", 'python3.12 tools/check_runtime_boundaries.py --root .',
                       '12 frozen oracle groups with old real archive RED;17 recorded input recompilations to separate output', 'git diff --check'],
    'not_required': ['FullCTest or SDK rebuild for unchanged product bytes', 'New browser proof before returning this offline implementation slice',
                     'A requirement for archived OFF0 to become green', 'Fixing old misleading remaining_runner stop wording'],
    'forbidden': ['Any browser sampling/smoke/hidden probe, qualification oracle probe or build in this offline repair. Exception: the two existing test_g5_ur0_fixture_oracle.py malformed-snapshot native unit checks in the mandatory suite; record them accurately and do not call their count0.',
                  'Changing30s/90s/RSS/AA/interior tolerance or original worker phase boundaries',
                  'Candidate-color/difference-derived HUD, treating every residual as an AA edge, or masking body interiors',
                  'Opening/modifying any oldwindow or claiming remaining budget authorizes new runs',
                  'Changing Runtime/API/SDK/renderer/consumer/performance schedule or source_identity exclusions', 'Commit/push or UR0c/UR1'],
    'closure': {'all_of': ['Frozen scope implemented and all required tests/oracles pass', '17 recorded populations/metrics/terminal outcomes invariant',
                          'Old evidence unchanged; exact new source and profile hashes, tests, offline residual findings and content-addressed result/bundle',
                          'READY_FOR_CONTROL_REVIEW or explicit blocker; proactively notify the control conversation'],
                'continue_until_terminal_state': True, 'return_surface': 'CONTROL_REVIEW', 'stop_after': 'OFFLINE_CORRECTNESS_HARNESS_REPAIR',
                'terminal_blockers': ['AUTHORITY_CONFLICT', 'TASK_PACKAGE_DEFECT', 'VERIFICATION_DESIGN_DEFECT', 'FROZEN_VERIFICATION_FAILURE', 'BLOCKED_EVIDENCE', 'ENVIRONMENT_BLOCKER']},
    'new_sampling_workers_max': 0, 'prior_sampling_starts': 33, 'prior_sampling_seconds': 1251,
    'future_uncompleted': 'Six correctness workers (first old attempt incomplete) then30 main; two realtime qualifications remain accepted exact recordings.',
    'commit_push': False, 'performance_or_official_gate_PASS': False,
    'notify_terminal': {'required': True, 'target_thread': '01a1b6f3-e69a-7fe2-8d9b-0222e8741f03', 'hostId': 'local',
                        'when': ['READY_FOR_CONTROL_REVIEW', 'any explicit blocker/frozen stop'],
                        'contents': ['task/package/status', 'revision/dirty/full-source hash', 'result/bundle paths andSHA256',
                                     'test/RED/17-input reconciliation and any residual differences', 'sampling0 and all remaining work'],
                        'delivery_failure': 'Save exact notification text and tool failure with terminal result; never pretend it was sent.'}}
put('implementation-request.json', request)
put('activation.json', {'schema': 'ur0b-correctness-harness-dispatch-v1', 'status': 'READY_TO_DISPATCH',
    'primary_owner': 'aegis-verification', 'specialist_route': 'Prior Gate handoff -> Aegis router -> P20 verification correction -> adjacent implementation owner',
    'explicit_repair_and_resume_authority': 'Existing user delegation/continue request; current user asks to break mutual waiting. Work order authorizes only the concrete offline harness repair.',
    'project_control_bootstrap': {'schema_version': '0.6', 'manifest_files_read': ['project.json','authorities.json','gates.json','evidence.json','integrations.json'],
                                'ur0_entries_present': False, 'state_cache_is_authority': False,
                                'scope': 'Task-local frozen UR0 packages/results and user authority; no global Gate status edit or new release/integration claim.'},
    'spec_ref': ref(version + 'verification-spec.json'), 'work_order_ref': ref(version + 'implementation-request.json'),
    'new_sampling_authorized': False, 'implementation_authorized': True, 'no_second_approval_if_preflight_clean': True})
print(json.dumps({'status': 'READY_TO_DISPATCH', 'spec_ref': ref(version + 'verification-spec.json'), 'work_order_ref': ref(version + 'implementation-request.json')}, ensure_ascii=False))
