# JS frame handoff closure

The regression in `verification/sdk_js/frame_handoff.html` delays a genuinely
ready WebGL fence without inventing completion, then commits newer input.
Against base `0074763f`, it failed with one canonical object, a delayed fence and
`timeout new canonical pixels after pending frame` (`out/sdk-frame-red.json`).

The Web adapter now fences unconfirmed submitted work, independently of whether
that submission originated in the current owner opportunity. It tracks the full
confirmed submission identity, so a later idle capture is not acknowledged
twice. Completion of old GPU work triggers a new owner opportunity; superseded
work is discarded locally and never qualifies a different identity. Actual
Runtime completion rejection remains an error.

A real lost GL context is detected even before its delayed DOM event. Sync loss
there suspends the surface without advancing presentation counts; repeated
native lost notifications are idempotent. A GL/fence failure while the context
is valid still rejects. This keeps existing owner/work validation intact.

Five real-engine scenarios pass: input during pending work, duplicate-completion
avoidance, resize during pending work, native loss before DOM delivery, and
recovery. The fixture delays the actual captured DOM callback for the fourth
case and verifies there is no fabricated presentation. Its GL/context APIs and
event functions are restored after execution.

The full core/browser/example, npm pack/hash/license checks, offline install and
external TypeScript consumer passed with the same implementation. Records:
`out/sdk-frame-full-green.json`, `out/sdk-frame-native-loss-green.json` and
`out/sdk-frame-closure/roundtrip.log`. This closes the automated JS frame defect,
not physical input/scanout acceptance or the complete Windows DLL/release task.
