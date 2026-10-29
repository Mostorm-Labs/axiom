# Group first slice — bounded execution

The user explicitly requested implementation of Group/Ungroup and previously accepted same-preparer review. This first slice uses existing topology, Selection and History owners and preserves the existing C ABI and wire schema. It authorizes real tests and the minimum ordinary Runtime command path; it does not claim independent review, Gate, PHYS or release success.

The prior FC08 package and frozen 302 expected values remain unchanged. The user requested incremental implementation without requiring the entire historical runner before product development. That sequencing does not waive any later applicable acceptance obligation. The known D07 Protobuf/Abseil allocation hang remains a separate blocker for final closure and release.

Run real failing tests before production changes. The first test must exercise selection and Group through the ordinary Runtime consumer; verify preserved world transforms and one Undo/Redo. Extend to Ungroup and nested Group/Section in successive tests. Windows/Web integration and physical validation remain subsequent work. The user deferred physical testing until Group is complete.
