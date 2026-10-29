# FC04 phase 2 / Group execution preparation — source audit

This is read-only source preparation and targeted automated regression at exact source revision `4eee69946dad61e0fc7678f9453423066c51750c`. It is not a new product execution package, independent review, DEV/P34/PHYS pass, or Group implementation authorization. The user has requested physical tests only after the Group stage.

## Existing mechanisms and remaining work

- Semantic `EditContainerTopology` already represents IntroduceWrappers, ReleaseWrappers and RelocateObjects; its existing planning and validation and `undoable_command.cpp` build inverses. Section already uses these mechanisms via `canvas_section_commands.cpp`. Its wrapper creation/release is explicitly Section-specific, so its presence is not proof of Group/Ungroup interaction.
- `SelectionSession` already resolves Group ancestors and enters/leaves Group scope. `FC08SectionConsumer.ExistingGroupScopeResolvesSectionWithoutNewMembershipModel` exercises a Group containing Section and scoped selection with world transforms. `canvas_selection_summary.cpp` explicitly reports Group/Ungroup as NotImplemented. There is no demonstrated product Group/Ungroup command or host control in this audit.
- Candidate execution source owners to bind in a separate current Group-only P31: existing selection/session and root reduction; existing topology and history preparation; Section/Group world transform and Scene publication; RuntimeComposition/Facade ingress and selection readback; Debug UI and Windows/Web host input. Exact allowed files and any ABI constraints still need a source-level package and control review. Do not create a second hierarchy/History or redo the accepted Group/Section product semantics.
- Bind executable real tests for Group and Ungroup; nesting and Section combinations; insertion/drop and selection result; transformed parents and world coordinate preservation; separate fresh Undo/Redo; rejection and scene/history atomicity; producer and input regressions; actual Chrome/WASM and eventual physical mouse/touch. Test IDs and independent oracles are NOT frozen by this audit.

## Current FC08 authority and baseline

The old product P31 at `f67b9db29090449799bd3e05ef7f1d269dab6d31` retains `P32_authorized=false` and was authored for source `427580ae13b60a39e88959b282cbffc695c9aabb`, before the independently authorized Section slices. It cannot be treated as a current Group implementation authorization. The old bootstrap-003 full runner blocker was recorded against that older product subject; the new Group preparation must distinguish the now-implemented Section seams from still-missing Group consumer seams, and bind a viable, independently reviewable verification path. Do not require or assert that the historical 302 variants have passed; do not silently weaken their frozen requirements or rewrite old evidence.

The requested preparatory scope does not authorize Group product mutation. A fresh bounded Group execution package must resolve its actual current baseline, approved Authority, scope, preflights, test oracles and any genuinely applicable runner dependencies before code execution. The preparer cannot silently declare that control acceptance occurred.

## Phase 2 status

The actual locked/stale and explicit-intent/History/Scene paths have targeted tests; MSE-20 per-object denial cannot be established because the local host has only the user-accepted explicit local-allow/unknown-fail-closed modes. MSE-26 requires a real failing or absent property provider while current eligibility/default lookup is static. MSE-23 has real semantic and Scene failure observations but not all frozen capability/History reservation failures. The full Chrome/WASM matrix is still incomplete. Do not treat absence or a fake provider as PASS or redesign Authority inside implementation. Preserve D07's separate known OOM blocker.

Targeted existing native execution: 15 CTests passed, exit 0. Its exact command, JUnit, logs, hashed input paths and current head are in facts.json and raw.zip; no physical validation was performed. `origin/main` has advanced in history beyond the common base, but its observed net tree diff from `2fee75f...` is empty; this is not a merge, ancestry equivalence, or authorization to rewrite either branch.
