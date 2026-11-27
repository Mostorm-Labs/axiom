# Axiom shared control contract

DOM/React-free DTOs and async `Controller` for Web and React Native. This source
package is not published until the native release is qualified. The existing Web
`Runtime` API remains available; use additive `createController` for portable UI.
Every owner-dependent call returns a Promise. A command resolves after canonical
apply, separately from platform presentation, local durability and cloud sync.

IDs are full 128-bit hexadecimal strings, uint64 revisions are decimal strings,
and owner-bound tokens must be returned unchanged to their originating Controller.
Byte inputs are copied on admission. Returned bytes are caller-owned. Native
pointer/contact and surface APIs are intentionally absent from this contract.

For a Section title editor, read the projected title frame from `readSection(id)`.
Use `setSectionTitleEditor(id, mode, selectionRevision)` with `inline` for an
on-canvas editor or `panel` for a property field. Release it with `closed` and
the same selection revision. Focus affects only the local view. Submit the
finished text through `renameSection`. Section creation, wrapping and renaming accept
up to `readSection(id).titleMaxCharacters` Unicode code points on one line.

The camera accepts positive absolute zoom targets and clamps them to 10%–300%.
Point-anchored zoom and fit-to-content, selection or object share these limits.
