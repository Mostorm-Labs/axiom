# GT-G3-11 implementation context

The ten G3 work packages are closed and integrated on `main@b1eb2db741fe80822d608675f1646ea50b6acbc0`. G3-11 is the remaining cross-platform oracle and Gate Report slice.

Use the existing G0 platform workspace as an input source only. Do not relabel or mutate the G0 report schema. The first incomplete action is to add a G3-specific generated report path that independently validates the closed WP matrix and required evidence categories.

Preserve all predecessor result and integration identities. A report generated from missing or mismatched inputs must be `BLOCKED`, not a synthetic PASS.
