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

### 2026-10-21 - `docs:` the Windows build story, and one reverted prototype

**Two findings, one of which changes what the DX layer can claim.**

**The full build fails on this machine, and the reason is upstream.** The whole
tree compiles - all 162 vendored grammars, the tree-sitter runtime, SQLite,
mimalloc, xxhash, yyjson - and reaches the link stage. It then fails to compile
three call sites:

```
src/pipeline/helm_template.c:2202:72: error: unknown conversion type character 'z' in format
internal/cbm/doc_pdf.c:1337:70: error: unknown conversion type character 'z' in format
src/pipeline/pass_k8s.c:4672:58: error: too many arguments for format
```

`src/foundation/arena.h:82` declares `cbm_arena_sprintf` with
`__attribute__((format(printf, 2, 3)))`. On **MinGW** that attribute resolves to
`ms_printf` semantics, which do not support `%zu`. Upstream's Windows binary is
an **MSVC** build - this box has MinGW and no MSVC (`vswhere` is absent), so the
project's own sanctioned Windows path (`scripts/test-windows.ps1`) cannot run
here.

**What this means for the DX layer, stated plainly.** Every DX module is
compile-verified with the project's own flags and unit-tested standalone, but
**the full server build has never been run in this environment**, and the DX
suites have never run inside the house test binary. `CB-20` records the two
ways out - build on MSVC, or make the format attribute portable so the tree
builds with the toolchain that is actually present.

**One prototype reverted rather than shipped.** The DX-native org graph layer
(`CB-19`) was designed, written and prototyped. Its output carried corrupted
string values, the cause was not found in the time available, and **unverified
code does not go in** - so it was reverted, and the design and the two defects
found are recorded instead:

- a size calculation was wrong (`+8` where the id needed `+10`), so a
  dependency id was silently truncated to `dx:dep:bot->metasear`;
- a folder that was both a route target and a dependency source produced two
  nodes with one id, and a graph with the same id twice is not a graph.

**Executed.**

| Check | Result |
| --- | --- |
| `dx_surface.c` compiles clean; its contract tests pass | exit 0, **34 passed** |
| `dx_governance.c` compiles clean; its contract tests pass | exit 0, **23 passed** |
| `dx_memory.c` compiles clean; its contract tests pass | exit 0, **12 passed** |
| `mcp.c`, `dx_surface.c`, `dx_governance.c`, `dx_memory.c` and all three test files pass `-fsyntax-only` with the project include set | exit 0 |
| The full server build | **FAILS** - the MinGW `ms_printf` attribute rejects `%zu` (`CB-20`) |

### 2026-10-21 - `feat:` anchored memory - a memory that says when it went stale

**Milestone M5, policy half.** The base stores decisions, but nothing it stores
can be checked against the code it describes. A decision written three weeks ago
about a function that was rewritten yesterday reads exactly the same as one that
is still true.

**What was built (DX).**

| File | What it is |
| --- | --- |
| `src/mcp/dx_memory.h` | The four anchor states and the contract. |
| `src/mcp/dx_memory.c` | The policy. **Pure**: it reads no files and calls no graph. |
| `tests/test_dx_memory.c` | 12 contract tests. |

**The four states.**

| State | Means |
| --- | --- |
| `valid` | the anchored span is byte-for-byte what it was |
| `changed` | the span moved or its content differs |
| `gone` | the file or the symbol no longer exists |
| `unanchored` | the span could not be located, so nothing can be claimed |

**"gone" and "unanchored" are different answers and are never merged.** A memory
that was never anchored to a span recalls as `unanchored`, never as `valid`: a
memory nobody can check is not a memory anybody can trust. And every memory comes
back with a **reason** - a verdict without a reason is a claim, and this module
makes claims only about things it checked.

**The module is pure on purpose.** It does not read the file; the caller
supplies the current content of the anchored span and the module decides what
that means. That is what makes every state verifiable without a store, a graph
or a build - and it is why the store side can be written later without touching
the policy.

**No new dependency.** The hash is the vendored `xxhash` (XXH64, lowercase
hex), so the same text always hashes the same and nothing is fetched at
runtime.

**Executed.**

| Check | Result |
| --- | --- |
| `dx_memory.c` compiles clean under the project flags | exit 0 |
| The memory contract tests pass | **12 passed, exit 0** |
| `dx_memory.c` and `test_dx_memory.c` pass `-fsyntax-only` with the project include set | exit 0 |

**Not yet done.** The store side - writing a memory with its anchor hash, and
resolving an anchor to the current span at recall time - needs the base's store
and its Windows-safe file access. `CB-11` records it.

### 2026-10-21 - `feat:` the governance verdicts - the tool can refute, not just answer

**Milestone M3.** A retrieval tool informs. This one **refutes**. Three of the
four verdicts are now reachable through the single DX tool.

**What was built (DX).**

| File | What it is |
| --- | --- |
| `src/mcp/dx_governance.h` | The four verdicts and their contracts. |
| `src/mcp/dx_governance.c` | The verdict **policy**. Depends on yyjson alone - no store, no tree-sitter, no SQLite - so every verdict is verifiable without a graph. |
| `tests/test_dx_governance.c` | 23 contract tests. |
| `src/mcp/mcp.c` | The handlers, and the three request types the surface routes to them. |
| `src/mcp/dx_surface.c` | The new request types: `verify`, `certificate`, `enforce`. |

**The rule the whole module follows: never certify what cannot be checked.**

| Request | Answers |
| --- | --- |
| `verify` | `confirmed` / `refuted` / `unverifiable`, with a citation receipt. The subject is resolved through the base's own snippet lookup in **outline** mode - a verdict needs a location, not the code. |
| `certificate` | `clean` / `opened`: did this change **open a new path** into a declared boundary? Reachable after, not before, and inside the boundary. |
| `enforce` | The commit gate: `held` or not, with blocking, new-frozen, accepted-frozen, advisory and off findings separated. |
| `certify_public_surface` | **Policy done, not wired.** The graph side needs the diff's changed exports and their consumers. |

**The three decisions that carry the weight.**

1. **An unresolved subject is "unverifiable", not "refuted".** The graph not
   knowing is not evidence that a claim is false. A resolved subject with no
   located evidence is also unverifiable - the base's own agent instructions
   say to treat an unknown as unresolved, never as an all-clear.
2. **A path that was already reachable is not an opening.** The certificate is
   differential. A path that merely moved is not a new path, and a boundary
   prefix matches on a **segment**, so `/src/api` covers `/src/api/x.c` and
   not `/src/apix.c`.
3. **A frozen kind the baseline already accepts is adopted debt, not a
   failure.** That is the difference between freezing a rule and failing a
   build. And an unrecognised action is **advisory, never blocking**: an
   unknown policy must not fail a build.

**Nothing is called safe on a shrug.** An export whose compatibility is missing
or unrecognised is `potentially-breaking`, and a breaking export with no known
consumer is still reported as breaking - the absence of a known consumer is not
evidence that there is none.

**Executed.**

| Check | Result |
| --- | --- |
| `dx_governance.c` compiles clean under the project flags | exit 0 |
| The governance contract tests pass | **23 passed, exit 0** |
| `dx_surface.c` compiles clean and its contract tests pass | exit 0, **34 passed** |
| `mcp.c` and `dx_governance.c` pass `-fsyntax-only` with the project include set | exit 0 |
| The full server build | **NOT RUN** - the tree-sitter runtime and the 162 vendored grammars are not built in this environment |

**Two real bugs the tests caught, and one that was not a bug.**

- The first cut of the module used `yyjson_obj_foreach` on **arrays**. The
  macro walks an object's members, so every array came back empty. Twelve
  tests failed. Worse, two tests *passed* for the wrong reason - they expected
  a clean result and got one from an empty iteration. A green suite is not
  proof; the array cases are now the ones that would have caught it.
- A boundary-prefix case failed because the **test** was wrong, not the code:
  it added a *new* path inside the boundary and expected "clean", when a new
  path inside a boundary is exactly an opening. The test was split into two -
  the "nothing new" case and the "only the new path counts" case.
- A naming trap worth recording: `yyjson_arr_foreach` is for arrays and
  `yyjson_obj_foreach` is for objects. The compiler does not catch the
  mistake, because both are macros and both compile.

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
