# Windows Composition boundary — user-directed implementation package

The accompanying user plan defines the full Windows vertical slice. This
versioned package records the new direction without changing or claiming to
revoke the historical packages. It is user-directed and does not claim an
independent Control Review or Gate result. The repository identity, anchor,
plan, 11 exact source inputs and package closure are checked by:

```
python .aegis/packages/AXIOM-SDK-WINDOWS-COMPOSITION-P31-v0.1/preflight.py --repo .
```

That check verifies the structural inventory; it does not compile code, run a
window, or approve a lifecycle/verification decision. The external single-owner
test at `verification/sdk_cpp_composition` is currently RED. Existing external
consumers remain in `verification/sdk_cpp_install`.

Implementation must move the existing Composition's implementation and its
Windows surface execution behind the public/private boundary while preserving
the current Runtime owner and existing work identities. A Windows SDK sample
and adapter do not currently exist; those are required deliverables, not
pre-existing proof. Web SDK and full Document/Resource contracts remain outside
this Windows slice, but Web regressions and the known D07 finding must be
preserved. The complete exit criterion is in `package.json` and the user plan;
do not mark this task complete after the header test alone.
