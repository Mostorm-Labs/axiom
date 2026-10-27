# H03 input correction proposal (not accepted)

The original root-children fixtures use four-byte integer OrderKeys. Keys at 256, 512, ... end in 00 and violate the existing OrderKey invariant. Both original cases reject during fixture seed decoding, before the intended F/U admission test.

Proposed versioned overlay: append 01 to every four-byte root key, including the introduced Section. This preserves object identity, sibling order, object count, geometry and all expected values. Existing accepted inputs, writer/oracles, expected files and historical failures remain byte-for-byte unchanged.

`proposal.json` binds original identities and the independently reconstructed proposed F/U hashes. The overlay is PROPOSED_NOT_ACCEPTED. It must not be counted as the original H03 product PASS.
