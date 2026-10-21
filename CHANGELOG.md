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

### 2026-10-21 - `feat:` the answer policy - relationships by default, and a budget that gates

**Milestone M2, landed inside M1.** The shape of the answer is decided at the
surface, so it belongs with the surface.

**What was built (DX).**

| Piece | Where | What it does |
| --- | --- | --- |
| `request.source` | `src/mcp/dx_surface.c` | `"outline"` (the default) or `"full"`. `outline` answers with relationships and a `file:line` pointer, so the answer does not grow with the repository. `full` asks for the source text and costs what the source costs. |
| `request.max_tokens` | `src/mcp/dx_surface.c` | The caller's budget. An answer that does not fit is **refused**, not truncated. |
| The gate | `dx_surface_apply_budget` | Measures the composed answer and refuses it when it does not fit, naming the excess, the token count and the budget. |
| `shape` | `dx_surface_shape_json` | Every answer reports the source policy in force, the token count, the budget, whether it was over, the excess, and how many source bodies it included. |

**The base was further along than this feature assumed, and the record says so.**
Every base tool the DX surface can reach already advertises `max_output_tokens`
(default 3200, minimum 128, maximum 1,000,000), the cap is applied on a
deterministic **UTF-8 byte ceiling of four bytes per token**, it is applied only
at whole semantic-unit boundaries, and when the first unit does not fit it
refuses and says *"raise max_output_tokens"* rather than truncating mid-unit.
`get_code_snippet` already has `source_mode: auto | full | outline` and reports
the mode it used. So the base caps; what it did not do is make the **choice**
explicit or price it. The DX surface therefore:
- defaults the shape to `outline` and makes `full` an explicit opt-in,
- adds a **caller-level** gate on top of the base's tool-level cap, and
- reports what the answer carried and what it cost.

**One rate, two files.** The DX surface estimates tokens at four UTF-8 bytes per
token - the base's own stated contract (`MCP_OUTPUT_BYTES_PER_TOKEN_ESTIMATE` in
`src/mcp/mcp.c`). A budget that means two different things in two files is not
a budget, so the constant is named in one place and documented in both.

**Translation, not duplication.** `source` and `max_tokens` are DX fields, so
neither is forwarded to the base verbatim. The surface translates them into the
base's own vocabulary - `source_mode` and `max_output_tokens` - and a caller
who set the base's parameter directly keeps their value, because an explicit
parameter beats a default.

**A refusal that looks like a refusal.** A DX refusal uses the base's own error
shape: the text in `content`, and `{"error": text}` in `structuredContent`. A
refusal that looked different from a base error would read as a different kind
of failure, and the agent would not know how to treat it.

**Executed.**

| Check | Result |
| --- | --- |
| The module compiles clean under the project flags | exit 0 |
| The contract tests pass | **33 passed, exit 0** |
| `mcp.c` and `dx_surface.c` pass `-fsyntax-only` with the project include set | exit 0 |
| The full server build | **NOT RUN** - the tree-sitter runtime and the 162 vendored grammars are not built in this environment |

**One API bug caught before it shipped.** The first cut of
`dx_surface_apply_budget` returned the caller's own pointer when the answer fit
and a fresh string when it did not. A function whose return is sometimes owned
and sometimes borrowed is a bug waiting to happen, so it now always returns a
fresh buffer and the caller always frees it.

### 2026-10-21 - `feat:` the DX surface - one typed tool over the code graph

**Milestone M1 of [`dx/PLAN.md`](dx/PLAN.md).** The first DX feature is landed:
the 17-tool menu is replaced, for DX sessions, by **one tool whose schema makes
the reasoning a required argument**.

**Why this, and not a new index.** A measured autopsy of the base and its two
competitors (`ttsc`, 2026-06-30) found that on the GPT-5.6 grid with the `zod`
repository, `codebase-memory-mcp` spent **66% more tokens than running with no
MCP server at all** (worst cell +82%), `codegraph` 4% more and `serena` 130%
more, while the compiler-backed `@ttsc/graph` cost **11% of baseline**. Their
conclusion: *"In the runs I measured it called the MCP zero times and fell back
to the shell every time. It gave up and grepped. The graph was right there; the
surface buried it."* An independent benchmark agrees from the other side - a
thin wrapper around the base was **cheaper than the raw server in every
setting** (`nefayran/code-graph-vs-grep`, 2026-10-01, p <= 0.021).

**What was built (DX).**

| File | What it is |
| --- | --- |
| `src/mcp/dx_surface.h` | The module's public interface: parse, dispatch target, wrap. |
| `src/mcp/dx_surface.c` | The implementation. Depends on `yyjson` and the C standard library only - no store, no tree-sitter, no SQLite - so the contract is verifiable without the full build. |
| `tests/test_dx_surface.c` | 19 contract tests, in the house framework. |
| `dx/PLAN.md` | The master plan: five features, the order, the measurement commitments. |
| `dx/F1-SURFACE.md` | This feature's contract: the envelope, the validation rules, the dispatch table, the test matrix. |

The envelope is `question`, `draft{reason,type}`, `review` and
`request{type,...fields}`. All four are **required**: a missing one is
rejected at the call boundary, never answered from a default. `request.type` is
a discriminated union over `search | trace | impact | details | outline |
architecture | schema | coverage | changes | escape`, and `escape` is a
first-class no-op for a question that is not a graph question. The request's own
fields are forwarded **verbatim** to the underlying base tool, so the module
owns no per-tool field mapping and cannot drift from the base tools' schemas.
The response gains two fields the base never had: `audit` (what was checked)
and `next` (answer, inspect, or escape).

**Wiring (DX, in the upstream tree).**

- `src/mcp/mcp.h` - a fourth tool profile, `CBM_MCP_TOOL_PROFILE_DX`, which
  exposes exactly one tool.
- `src/mcp/mcp.c` - the `dx_codebase` tool definition, the
  `handle_dx_codebase` handler, the `dx` branch of the profile filter, the
  `--tool-profile=dx` flag, and the dispatcher split so the DX surface can
  reach the base tools the profile does not expose directly. The request-type
  table is that allowlist: the mutating base tools are not reachable from it.
- `Makefile.cbm` - `src/mcp/dx_surface.c` in `MCP_SRCS`, `tests/test_dx_surface.c`
  in `TEST_MCP_SRCS`.
- `tests/test_main.c` - the suite is declared and run with the rest.

**What was executed, and what was not.**

| Check | Command | Result |
| --- | --- | --- |
| The contract compiles clean under the project's flags | `gcc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-sign-compare` on `src/mcp/dx_surface.c` | exit 0 |
| The contract tests pass | the same compile plus `tests/test_dx_surface.c`, then the binary | **19 passed, exit 0** |
| The wiring compiles clean | `gcc -fsyntax-only` with the project's include set on `src/mcp/mcp.c` and `src/mcp/dx_surface.c` | exit 0 |
| The full server build | `make -f Makefile.cbm test` | **NOT RUN** - the tree-sitter runtime and the 162 vendored grammars are not built in this environment |
| The suite inside the house test binary | `tests/test_main.c` | **NOT RUN** - same reason |

The standalone compile is not a substitute for the full build, and this entry
does not claim one. The module was written to depend on nothing but `yyjson`
precisely so that the contract could be *executed* here; the wiring is
compile-verified and awaits the full build.

**One real bug the tests caught.** The first version of the schema test looked
for `request` at the schema root rather than under `properties`, and then for the
enum one level too shallow. The test failed, the test was wrong, and the test
now walks a dotted path (`dx_path`) so the mistake cannot recur.

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
