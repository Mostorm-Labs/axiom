# SDK 0.2.0 release

SDK 0.2.0 is the next minor feature iteration after the published 0.1.1 line.
It combines the shared asynchronous Controller/API boundary, Web continuity and
interaction fixes, and the React Native native-host development package in one
source revision. The stable consumer release remains the Windows x64 and Web/
WASM package produced by `sdk-preview-release.yml`; the React Native package
remains private development delivery (`0.2.0`, `NOT_QUALIFIED`) until its own
consumer publication contract and complete platform qualification are frozen.

The release must use one clean `main` commit, tag `v0.2.0`, and publish
`@mostorm/axiom@0.2.0` with the `latest` tag. Existing tags and registry versions
are immutable. The release workflow verifies exact source identity, package
manifests, Windows external consumers, Chrome/WebGL2 consumers, TypeScript
consumers, npm packing, and SHA-256 provenance before publication.

This release includes the four-platform RN implementation and physical smoke
evidence as repository evidence. Those records remain scoped observations; they
do not change the stable Web/Windows SDK ABI contract or imply a Gate, P34, IME,
stylus/pressure, performance, or production qualification result.

Historical POC/reference and task-qualification workflows remain available by
explicit `workflow_dispatch` only. They are not dependencies of SDK publication
and are not scheduled by ordinary SDK or `main` changes.
