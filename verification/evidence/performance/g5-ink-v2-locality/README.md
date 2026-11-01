# Ink v2 locality evidence

The bounded summary is `comparison.json`. Full observations and long logs are
losslessly compressed as `.gz`; use Python `gzip.open(path).read()` or `gzip -dc`
to inspect them. `SHA256.json` binds all bundled bytes and the uncompressed
identities of compressed files. Logs have trailing whitespace normalized; raw
original logs remain in the run output directories. Original per-run manifests
retain their original directory inventory and are provenance, not the inventory
of this bundled subset. No official G5, P34, physical, or platform Gate PASS is
claimed. The source trees were dirty; use their file hashes in addition to HEAD.

`ff5b0739-integration/` is compatibility verification of the later visual commit;
it is separate from the fixed `6c46f540` before/after benchmark.
