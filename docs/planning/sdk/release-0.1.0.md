# SDK 0.1.0 release execution

Repository github.com/Mostorm-Labs/axiom; anchor5254b087468db9e91e04f35c705f77f63040e103
(ancestor, current main). User authorizes a new SDK release from main and, separately,
four-platform React Native SDK design/implementation. Publish the existing matching
Windows/Web SDK and Demo first; React Native work uses a descendant feature branch
and is not represented as qualified by this release.

Version0.1.0 is already aligned in sdk/version.json, Web wrapper/types/package and
all build manifests. Latest regular published version is0.0.1. This is the next minor
feature iteration (Table, extended vector/appearance/font APIs and compact brushes).
The historical0.1.0-alpha.1 is a different immutable identity; no stable0.1.0 exists.
Reuse the locked Semantic/Skia SDKs, existing tag-only release workflow and existing
AXIOM_NPM_TOKEN GitHub Secret. Never log credentials, overwrite a publication or
weaken a required consumer test. Local registry readback currently returns401;
hosted publication must establish its own registry result, not infer local auth.

Existing PR157 Windows build failed C4702 under /WX: EditTable's unconditional
return makes the generic visitor's trailing return unreachable in that specialization.
Use the same failure check as adjacent alternatives, preserving codec behavior.

EXECUTION_CLOSURE_CONTRACT
Required: portable strict compile fix, existing codec/package/JS/TS/boundary tests,
exact clean source commit pushed and integrated into main, matching unused v0.1.0
tag, hosted Windows/Web consumer verification, GitHub Release and npm latest,
registry/archive identity readback and fresh published-consumer verification.
Forbidden: dependency/descriptor changes, public ABI/semantic changes, old tag/asset
rewrite, credential exposure, skipping required consumers or calling physical/Gate PASS.
Hosted build/publish is blocking. Local physical input is corroborative. Registry
permission, frozen consumer failure or dependency resolver failure is an explicit
blocker; preserve exact source/run and continue independent RN implementation work.
Continue until published and verified or a concrete blocker; no publication claim
from a tag push alone.
