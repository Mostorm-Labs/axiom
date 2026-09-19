# GT-G4-05

Anchor: `7643f50d8e26dc53390bd2d91884c3cf477eb3ed`.

Build transient PreviewModel and a presentation-only Arc bridge. Reuse the
accepted Arc semantics but do not modify Arc ABI. The bridge may degrade to
canonical-only; it cannot commit, qualify presentation, emit CanonicalVisible,
or retire a preview. Those remain G4-06/render-owner work.
