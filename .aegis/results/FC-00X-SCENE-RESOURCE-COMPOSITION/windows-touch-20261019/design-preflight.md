# Windows Minimal Host touch narrow repair

Execution authorization: user request in this chat, baseline
`f1d72f22ad20209c8ccdbdd7474d86c651434914`, repository
`github:Mostorm-Labs/axiom`. The third handoff remains the owning P32 package.
The user explicitly limits this continuation to Windows/input/interaction/
composition/render wiring and its diagnostics. Historical Web/Android work is
not replayed. No Control Review, Gate or PHYS verdict is produced here.

Repository preflight: original checkout is on
`codex/g4-5-common-brush-processing` at
`b52125a2b62cf3ad1f7d44069e0bc0c40ebfc72a`, with existing untracked logs/images.
It is preserved. Fetch through the existing local proxy resolved
`origin/codex/fc00x-scene-resource-composition-p32-f08-resume` to the exact user
baseline (zero delta). The package anchor `0e9ba85f94d62e6e3939d5d1397b1c681c1b1a2a`
is an ancestor. An isolated managed worktree and
`codex/p32-windows-minimal-touch` branch are used. Only root AGENTS.md exists in
the tracked tree. The five package binding hashes and four oracle/input hashes
were recomputed successfully before production mutation.

Current boundary: provider forwards WM_POINTER messages, owner ignores them;
mouse ingress fixes source/pointer at 1/1. RuntimeAppBinding already rejects
mixed-source batches and PlatformInteractionIngress owns pointer generations.
Runtime owns AutoIntent arbitration and viewport navigation. The Debug UI is a
separate owned HWND and controls Runtime exclusively through RuntimeFacade.

Target flow: OS GetPointerInfo plus typed touch/pen history -> owner maps screen
to client to logical view once -> one-source PlatformPointerBatch -> existing
composition -> RuntimeAppBinding/PlatformInteractionIngress -> existing Runtime
arbitration/dispatch. No host gesture recognizer. The host keeps a full accepted
PointerKey receipt for each native contact; keyed cancellation delegates through
composition to the existing Runtime cancellation owner. Rejected delivery must
release any admitted ingress/session. No global mouse capture for touch.

Lifecycle: pointer-up ends only that contact; pointer cancellation/capture loss
aborts the exact key; focus loss, cancel mode, resize/DPI transitions and close
abort all owned contacts before changing the surface. Late updates cannot create
a contact without Down. Native timestamp watermarks fence history from prior
lifetimes. Touch/pen promoted mouse is consumed without canvas delivery. Mouse
keeps its existing coalesced history, with unavailable pressure marked absent.

Metadata is mask-gated. Unknown pressure/geometry/history is not invented. Typed
native history is reversed into observation order and deduplicated against its
last native observation. Pressure/tool/contact metadata is preserved only when
provided. No rotation capability is added or promised.

User clarification: preserve frozen AutoIntent. Pending contacts within 250 ms
may claim viewport; beyond 250 ms the second contact becomes Ink/MultiInk. Once
the first contact crossed Ink slop, a later contact remains Ink even within the
window. Matrix B3 is split accordingly. No strategy or Authority change.

RED oracle: production WindowProc receives fixture WM_POINTERDOWN with real
Runtime and surface binding; baseline must produce zero normalized trace entries
instead of one. This test is run before production edits. Follow-up tests cover
history, metadata, identity reuse, cancellation, promotion, multi-contact policy,
mixed-source rejection, failed Down, resize, canonical commit and next stroke.
Existing Brush/frame retry/composition/facade/capture and surface tests are also
required. Native D3D12 executable must be rebuilt from a committed source revision,
hashed, launched, and its PID/path recorded. All unobserved manual items remain
PHYSICAL_PENDING; implementation failures remain FAIL in the diagnostic history.

Package/Evidence preflight: exact local package refs are internally valid;
required proof seams are executable; no new semantic decisions remain after the
user clarification. Scope, Authority and verification conflicts: none. Dependency
policy: locked Semantic Store and installed locked Skia only; MSVC 19.51 x64.
