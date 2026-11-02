# SDK 0.1.1 release

Patch iteration on published 0.1.0. The user authorizes committing and pushing the
Web interaction correction branch, integrating it into main, publishing matching
Windows/Web SDK and Demo assets, and publishing `@mostorm/axiom@0.1.1` as `latest`
to the existing `http://npm.nearhub.cc/` registry. React Native remains on its
separate development branch and is excluded from this release.

Changes: full-viewport Web canvas with floating controls; coalesced live resize
without implicit camera movement or pixel stretching; retained display until a
qualified new-binding presentation; actual WebGL provider resize; zero-size shape
Down suppression; native wheel pan direction; compact round/marker acquisition;
partial region selection that skips unsupported brush coverage while retaining
fail-closed missing-layout behavior; Compact Ink v2 container topology acceptance.

Existing Operation/History/Scene/render owners and private transport are reused.
ABI and transport versions remain 1. No schema, descriptor or dependency lock
changes. No published tag/asset/version may be overwritten.

Local proof includes matching Chrome/WebGL2 external consumers, GPU handoff and
context restoration, Shell/Axis, region/group operations, live resize pixel and
camera assertions, affected native CTest, types and package validation. Hosted
Windows/Web consumers and archive/registry provenance must pass at the release's
exact clean main revision before publication is reported as successful.

Known limits remain: physical IME/touch/platform qualification is separate,
persistence/synchronization belong to the external application, checkpoint restore
does not restore Undo history, and D07 injected allocation-failure work is deferred.
This release does not declare a Gate/P34 PASS.
