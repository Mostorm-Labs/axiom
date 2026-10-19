# FC-00-FC-TRACK-AUTHORITY — P31 Implementation Context

Read package.json, this file and execution-contract.json in that order; validate all SHA-256-bound lock refs. Source is the accepted first canonical Aegis v0.6 Axiom snapshot 35f42b54dd29b6844953de83ac54dc1321479b3c, Legacy Import P34 PASS. Official Aegis validator is Mostorm-Labs/aegis@00d3690ca2cdde15b3f919c54ccc8ef6290f55ba.

Append **only** the object stored at expected-authority.json field "append" as the fifth entry of .aegis/authorities.json. No legacy Authority modification; disjoint scope execution:FC/feature-completion; G4 v0.3 is functional-only lineage, NOT physical PASS.

Before writing: verify source/Authority identity, freeze old valid snapshot and check baseline official validate/check. After append: official Aegis CLI transition-check <prior-v0.6-root> <new-v0.6-root>, validate, recompute --write, check, repeat for stable state; independent source equality check 25 Gate Decisions/29 Bindings/34 Evidence unaffected. active_stage=null is acceptable; project.json lifecycle_hint is NOT authorized to change. Existing FC-00 Legacy Import package, evidence and result must remain intact.

Only the two files .aegis/authorities.json, generated .aegis/state.json and new .aegis/results/FC-00-FC-TRACK-AUTHORITY/** may change. Push exact result SHA to reviewer-resolvable branch and return READY_FOR_CONTROL_REVIEW with precise evidence refs; no P34/FC-00/FC-01 PASS. Terminal blocker if pinned tool/source, exact descriptor, official CLI or immutable history checks fail.
