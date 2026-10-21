# CHANGELOG.md - What Changed in `essence-dx/codebase`

Dated, receipted, per release. One event has one record: when an item in
[`TODO.md`](TODO.md) closes, it is deleted there and a line is added here **in
the same commit**. Every entry names the `file:line` or the command output that
earns it.

**Claim tiers** apply to every line below. *repo-managed* means committed and
readable. *executed* means run here with a named command. *runtime / hosted*
means true on another host. *release-qualified* means shipping verified end to
end on a real device. **Nothing in this file is release-qualified** - this fork
has published no release of its own.

**This repository is a fork.** Everything not marked *DX* below is upstream's
work, by `DeusData/codebase-memory-mcp`, under MIT. DX's own changes are marked
*DX* and are the only ones this ledger claims.

---

## Unreleased

### 2026-10-21 - `chore:` the fork is established as `essence-dx/codebase`

**The DX layer is created. The upstream base is untouched.**

**What was done.**

- `G:\Dx\codebase` is now this repository's root. The previous
  `G:\Dx\codebase\codebase-memory-mcp\` level was removed and its contents -
  including `.git` - were moved up one level, so the folder layout is the
  upstream project layout with no wrapper.
- `essence-dx/codebase` was created as a **real GitHub fork** of
  `DeusData/codebase-memory-mcp` (public, default branch `main`).
  `gh repo view essence-dx/codebase --json parent` returns
  `{"name":"codebase-memory-mcp","owner":{"login":"DeusData"}}`.
- Remotes are set the standard fork way: `origin` is
  `https://github.com/essence-dx/codebase.git` and `upstream` is
  `https://github.com/DeusData/codebase-memory-mcp.git`.
- The local checkout was fast-forwarded from `a2227157` to `3457b21b`. The
  clone was 74 commits behind upstream; upstream landed 74 commits between
  2026-10-20 06:34 and 2026-10-21 03:44.
- The repository slug carries no second `dx` prefix: the organisation is
  already `essence-dx`, so the repo is `essence-dx/codebase`, not
  `essence-dx/dx-codebase`.

**DX files added.**

| File | What it is |
| --- | --- |
| `TECH.md` | The decision record: what this fork is, why this upstream and not the other one, the five features that make it a DX product, and the evidence index. |
| `TODO.md` | The DX work ledger. 12 items, each with evidence and a done-when check. |
| `CHANGELOG.md` | This file. |

**The other candidate was removed, and here is the record.**

`G:\Dx\codebase\codegraph` - a clean checkout of `colbymchenry/codegraph` at
`8beab37`, 1550 commits, 204.5 MB - was deleted on 2026-10-21 under explicit
user approval, after the comparison in `TECH.md` decided this fork's base.

| Field | Value |
| --- | --- |
| Target | `G:\Dx\codebase\codegraph` (entire tree) |
| Size | 204.5 MB, 1550 files |
| State at deletion | clean working tree, 0 uncommitted changes |
| Upstream | `https://github.com/colbymchenry/codegraph` |
| Regenerate | `git clone https://github.com/colbymchenry/codegraph` - it restores at `8beab37` |

Nothing unique was lost: the working tree was clean and every commit was
already pushed to the upstream remote. The comparison evidence it carried is
preserved in `TECH.md`, cited against the upstream commit
`colbymchenry/codegraph` at `8beab37`.

**No upstream source was modified.** `git diff --stat upstream/main..main` shows
exactly four files, all of them DX additions - 488 insertions, 0 deletions:

```
 .gitignore   |   6 ++
 CHANGELOG.md |  84 ++++++++++++++++++++++++++++++++++++++++++++++++++++++
 TECH.md      | 183 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 TODO.md      | 215 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
```

The one upstream file touched is `.gitignore`, and only by **adding** six lines:
upstream ignores `CHANGELOG.md` at line 61 because it publishes release notes on
GitHub instead, so this fork negates that rule in a marked DX block. Without the
negation a plain `git add -A` silently drops the ledger entry that the
same-commit rule requires - the exact failure the hub has been caught with. The
upstream `LICENSE`, `THIRD_PARTY.md` and `DCO` are unchanged.

**Not claimed.** The fork has no release, no `dx` route, no `TOOL_REGISTRY`
entry, and no `docs/` register page. Those are `CB-01` through `CB-04` in
`TODO.md`, and `CB-01` is blocked on the first release.

---

MIT OR Apache-2.0
updated: 2026-10-21 00:00 UTC
