# GT-G4-5-C2 P31 v0.4 implementation context

The former C2 package is superseded for the new Brush Engine path. Begin with
PackageBindingPreflight and RED against the old family/algorithm-centric path.
Implement the new contracts in vertical slices: package/resolve, plan compile,
session/output, VectorPathNode adapter, operation intent, then platform ownership.

Perfect Freehand R1 is a versioned node adapter and oracle. It is not the product
brush model. Old programmable-brush code can be used only to inspect released V1
behavior and must be isolated behind legacy_v1_readonly; do not extend it.
