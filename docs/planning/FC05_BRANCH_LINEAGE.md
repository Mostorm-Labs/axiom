# FC05 branch lineage and handoff

Status: **ARCHITECTURE_WORKTREE_READY / DESIGN_REVIEW_PENDING**

This record keeps FC03 and FC05 work separated. It is repository execution
metadata, not a semantic or Gate authority.

| Line | Exact revision | Role |
| --- | --- | --- |
| `origin/main` | `10dd7e4bf783ebcf299a1305d74f49e960f9ac69` | Main after the merged FC03 hotfix PR #151 |
| `codex/fc03-image-init-hotfix` | `3bd0c71c35e04d0c056cf1832997b721c812602e` | Preserved FC03 hotfix line; do not reuse for FC05 |
| `codex/fc05-connector-sticky-design` | `d6a47d53dfc6fa5381e20a528ecceb1e2878fa55` | FC05 design ancestor and draft PR #152 |
| `codex/fc05-architecture-p20` | `d6a47d53dfc6fa5381e20a528ecceb1e2878fa55` + this line | Separate architecture/P20 preparation line |

The architecture line is intentionally stacked on the FC05 design line. It must
not be rebased onto `main`, force-pushed, or merged into the design branch. A
future implementation branch must start from the exact accepted P31
materialization commit, after an independent review has accepted the design and
P20 has been frozen. No FC05 product implementation is authorized by this file.

## Branch rules

- Keep the FC03 hotfix line and PR #151 history intact.
- Keep draft PR #152 as the design ancestor; do not duplicate its files against
  `main`.
- Push architecture/P20 changes only to `codex/fc05-architecture-p20`.
- Do not merge either FC05 branch without an explicit user instruction and a
  separate Gate decision.
- Leave unrelated primary-checkout changes and `imgui.ini` untouched.

