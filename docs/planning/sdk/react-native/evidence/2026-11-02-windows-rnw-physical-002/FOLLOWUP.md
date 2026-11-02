# Historical evidence notice

Preserved raw results from the earlier 0.83 investigation. Do not use these
results as the current RNW platform conclusion. See ../2026-11-02-windows-rnw-physical-003/report.md.

Run 002 attributed the crash to native session / Promise initialization without
an exception stack; that attribution is superseded. A later CDB stack shows
0x80040154 activating MostormAxiom.ReactPackageProvider in autolink registration,
before session creation. Matching the DLL basename to the activation namespace
allowed the actual RNW app to start. The restored by-value ReactPromise is valid.

Some historical command receipts inferred build success from link output without
capturing the process exit code. Only the later 0.84 build .exit files establish
complete process exit 0. Original observations/manifests remain unchanged.
