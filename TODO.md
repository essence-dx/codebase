# TODO.md - Open Work for `essence-dx/codebase`

**Snapshot taken 2026-10-21 against the tree, not restated from memory.** Every
item below carries the evidence that proves it is open and a `Done when` check
another agent can run without asking anyone.

**Scope:** the DX layer on top of `DeusData/codebase-memory-mcp`. The upstream
base is not re-implemented here and is not tracked here; its own backlog lives
upstream. Items that touch the upstream source tree are marked `UPSTREAM-AWARE`
and say so.

**The plan this ledger tracks is [`dx/PLAN.md`](dx/PLAN.md).** Feature
specifications live beside it in `dx/`. The order of work is M1..M5 there; this
file is the item-by-item record.

---

## How to use this file

- Take items in priority order: **P0 -> P1 -> P2**. `BLOCKED` items are not
  skipped, they are parked with the blocker named and re-checked.
- Cite the ID (`CB-01`, `CB-02`, ...) in the commit subject and in the
  `CHANGELOG.md` line.
- On completion: **remove the item here and add a `CHANGELOG.md` entry in the
  same commit**. Never leave both records.
- Do not file an item without evidence. Do not resolve an item without running
  its check.
- Nothing here authorises a delete. Where an item's remedy is removal, it stays
  a proposal until approved.

---

## Scoreboard

| Priority | Open | What it guards |
| --- | --- | --- |
| P0 | 4 | The tool is installable and reachable as `dx codebase` |
| P1 | 7 | The fork beats both evaluated upstreams |
| P2 | 5 | The fork stays maintainable, and the claims stay honest |
| **Total** | **16** | |

**Milestones** (from `dx/PLAN.md`): M1 the surface - **in progress**. M2 the
answer shape. M3 governance. M4 the org graph. M5 stale memory and `prove`.

---

## P0 - Make it a reachable DX tool

### CB-01 - Register the tool so `dx` can install it

**Evidence.** `TOOL_REGISTRY` in `cli/src/update_registry.rs:5` lists every DX
tool as `(name, repo, binary, path)`. It has no `codebase` entry, and it must
not have one yet: the registry drives the install contract, and this fork has
published no release, so there is no asset to install.

**Done when.** The first release of this fork exists, and the registry carries

```rust
("codebase", "essence-dx/codebase", "codebase", "bin/codebase"),
```

with `codebase` in the binary name - not `dx-codebase`. The organisation is
already `essence-dx`, so a second `dx` prefix in the slug is redundant. **This
also changes the release-asset convention** the other 20 tools follow
(`dx-<tool>-<target-triple>[.exe]`, proven by `essence-dx/media` v1.0.0). That
convention is a hub-level install contract, so this is a decision for the hub,
not for this repository - see `CB-11`.

**BLOCKED** on the first release (`CB-02`).

### CB-02 - Publish the first release from this fork

**Evidence.** `git tag` in this repository lists upstream's tags only; the fork
pushed no tag of its own. `essence-dx/codebase` has no release, so
`gh release list --repo essence-dx/codebase` is empty.

**Done when.** `gh release list --repo essence-dx/codebase` shows one release
whose assets are named for the six supported target triples, and the upstream
release workflow's SLSA and VirusTotal gates have run on it.

### CB-03 - Add the folder to the `dx` routing manifest

**Evidence.** The `paths` block of `G:\Dx\dx` has no `codebase` entry, so no
`dx` route can resolve it. The hub's own `AGENTS.md` section 3 names the
manifest as the source of truth for "where does tool X live on disk".

**Done when.** `dx` resolves the `codebase` path and `Test-Path` on the
resolved folder is `True` and points at `G:\Dx\codebase`.

### CB-04 - Add the DX `docs/` register page

**Evidence.** `docs/README.md` carries one row per DX product and has no row
for `codebase`. The hub's `AGENTS.md` section 3 names `docs/<tool>.md` as the
canonical answer to "what has folder X actually achieved". The product repo's
own `TECH.md` is the source for that page.

**Done when.** `docs/codebase.md` exists in the hub, `docs/README.md` has the
matching row, and every number in it has a `file:line` in `G:\Dx\codebase` that
points at it.

---

## P1 - Make the fork beat both evaluated upstreams

### CB-05 - One tool, typed, with CoT-compliance - **SHIPPED, in a built binary**

**Evidence.** The base ships 17 MCP tools and a Cypher query dialect. A
measured autopsy of the base and its two competitors (`ttsc`, 2026-06-30) found
that on the GPT-5.6 grid with the `zod` repository, `codebase-memory-mcp` spent
**66% more tokens than running with no MCP server at all** (worst cell +82%),
`codegraph` 4% more, `serena` 130% more, and the compiler-backed `@ttsc/graph`
**11% of baseline**. Their words: *"In the runs I measured it called the MCP
zero times and fell back to the shell every time. It gave up and grepped. The
graph was right there; the surface buried it."* and *"Its MCP `initialize` sends
no instructions at all... you get fourteen tools over the wire with almost
nothing telling the agent which one to reach for or when."* An independent
benchmark agrees: a **thin wrapper** around the base was cheaper than the raw
server in every setting (`nefayran/code-graph-vs-grep`, 2026-10-01, p <= 0.021).

**What to build.** One MCP tool, `dx_codebase`, whose input schema carries the
reasoning as required arguments: `question`, `draft{reason,type}`,
`review`, `request{type,...}`. `request.type` is a discriminated union over
`search | trace | impact | details | outline | architecture | schema |
coverage | changes | escape`. `escape` is a first-class no-op. The response
carries `audit` and `next`. A fourth profile, `dx`, exposes exactly this one
tool.

The rule that makes it work: *"free prose can hide a skipped step; a typed
submission cannot."*

**Status 2026-10-21 - DONE, and verified in a built binary.** The surface is
built, the `dx` profile exposes exactly one tool, and the whole contract is
exercised end to end over real stdio against `build/c/codebase-memory-mcp.exe`.
The receipt is in `CHANGELOG.md` under *"the DX profile could not start the
server at all"*.

**What that verification found, and what it changed.** The first version of
this surface **could not start the server at all** - the profile's enum value
was outside the range the daemon's context handshake accepted, so
`--tool-profile=dx` exited with code 1 and answered zero requests. Every
feature in this repository was unreachable in the product while every unit
test passed. The bound is now written against a name that means what it says
(`CBM_MCP_TOOL_PROFILE_LAST`), the enum's shape is pinned by compile-time
assertions, and the DX profile has its own server instructions - it was
defaulting to text describing a seventeen-tool menu it could not call.

**What is still open.** `CB-21`: the profile is opt-in, so a user who installs
this fork and runs it still gets upstream's menu. The surface is built; making
it the default is the product decision that remains.

**Specification.** [`dx/F1-SURFACE.md`](dx/F1-SURFACE.md).

**Done when.** The server advertises exactly one tool under the `dx` profile;
a call that omits any of the four envelope fields is rejected at the call
boundary with a schema error, never answered from a default; `escape` returns a
clean non-graph answer; and a fixture test proves each `request.type` reaches
the right underlying handler.

**Status 2026-10-21.** The contract module `src/mcp/dx_surface.c` and its 33
tests are landed and passing (`-Wall -Wextra -Werror`, standalone compile, exit
0). The tool, the `dx` profile, the dispatch and the answer policy are wired
into `src/mcp/mcp.c`. **Not yet done:** the full server build and the test
binary in `tests/test_main.c` have not been run in this environment - only the
standalone module test has been executed.

### CB-06 - The org graph with a DX-native semantic layer

**Evidence.** The base can link services across repositories (`README.md:349`,
`README.md:417`) and can follow a path dependency between folders, but it has no
model of a DX tool, a DX rule or a DX register row. The hub's `AGENTS.md`
section 8.1 lists the path dependencies that break a build in another
repository - a real question the graph should answer.

**What to build.** Index the whole `G:\Dx` tree as one graph, and add nodes and
edges for the `dx` manifest routes, the `TOOL_REGISTRY` entries, the
`docs/<tool>.md` register rows, the `AGENTS.md` rule scopes, and the
path-dependency web between product folders. Then a hazard-typed conflict graph
over every in-flight branch, pull request and agent task - the part of the
problem no per-repository tool can reach.

**Done when.** One query answers "which repository owns this symbol, which rule
governs it, and what else breaks if it changes", and the answer names the
repositories **consulted and skipped**, not just the ones it found.

### CB-07 - Test impact that works on day one, on every DX language

**Evidence.** `docs/SMART_CI.md` is explicit: the test layer narrows only C and
C++ whose tests follow upstream's own conventions, and only with a per-test
coverage map the user must produce. "In any other repository the engine answers
'run everything'." The DX product folders are Rust, TypeScript and Go.

**What to build.** Language-agnostic test discovery - Rust `#[test]` and
`#[tokio::test]`, vitest, jest, pytest, `go test`, cargo-nextest - and a
blast-radius-to-test mapping derived from the graph alone, with no coverage map.
Delivered as a **20-line skill plus a grep-able map**, not a menu of tools: in
the one measured study of this shape, a 107-line skill cut to 20 lines
**quadrupled** an agent's resolution rate (12% -> 50%), while TDD *procedural*
instructions without the targeted context made regressions **worse** (6.08% ->
9.94%).

**Done when.** On a real diff in a Rust product folder, the tool names the
affected tests and says "run everything" with a reason when it cannot be sure -
and the "run everything" path is exercised by at least one test.

### CB-08 - Sufficiency receipts: every answer ships its blind spots

**Evidence.** The removed candidate's `AGENTS.md` states that an agent falls
back to `Read`/`Grep` the instant a graph answer is insufficient - that is the
whole product, and neither candidate exposed it. The base already collects the
raw material: `index_status` reports unresolved references by reason and the
`doc_links` block counts the documentation link families that did not clear
their accuracy bar.

**What to build.** The `audit` field that `CB-05` introduces must carry the
blind spots, not just a restatement of the request: `confidence`,
`covered`, `blind_spots` (unresolved references by reason, documentation link
families below their accuracy bar, files skipped) and `next_question`. A
`coverage.testDetection` of `full`, `partial` or `none` - never a
falsely-confident empty set. An empty result is never "no match".

**Done when.** A blind spot that exists in the index appears in the answer's
`blind_spots` list, and a test proves an answer with an unresolved reference
does not claim completeness.

### CB-09 - The answer is the index, never the bodies, under a hard budget

**Evidence.** The measured autopsy calls returning whole source bodies the
token bomb: the tool that does it spent **4% more tokens than running with no
MCP at all**. The benchmark that makes tokens-per-correct-answer the primary
axis is `sverklo`'s (naive grep 22,704 tokens per task against 652 for the
leader). And a tool that cuts the bill and then spends four minutes indexing
has moved the cost rather than removed it.

**What the base already had, corrected.** The base is further along than this
item first assumed, and the record says so: every base tool the DX surface can
reach already advertises `max_output_tokens` (default 3200, minimum 128, maximum
1,000,000), the cap is applied on a deterministic **UTF-8 byte ceiling of four
bytes per token**, it is applied only at whole semantic-unit boundaries, and
when the first unit does not fit it refuses and says *"raise max_output_tokens"*
rather than truncating mid-unit. `get_code_snippet` also already has
`source_mode: auto | full | outline` and reports the mode it used. So the base
caps; what it does not do is make the **choice** explicit or price it.

**What to build.** Three things the base does not have:

1. **A default shape.** The answer is relationships and a `file:line` pointer,
   not source text. The DX surface sets `source_mode: "outline"` by default;
   `source: "full"` is an explicit, priced opt-in.
2. **A hard gate.** The DX surface measures the composed answer and **refuses**
   one that does not fit, naming the excess. The base's cap is a tool-level
   limit; this is a caller-level contract.
3. **A report.** Every answer carries a `shape` object: the source policy in
   force, the token count, the budget, whether it was over, the excess, and how
   many bodies it included.

**Status 2026-10-21.** Landed with M1. `request.source` and `request.max_tokens`
are parsed and validated, translated into the base's `source_mode` and
`max_output_tokens`, and the gate and the `shape` report are in
`src/mcp/dx_surface.c`, with 13 of the 33 contract tests covering them. The
base's own byte rate is reused, so the two files agree on what a token is.

**Done when.** A default call returns no source bodies, the response carries
its token count, and a call whose budget is exceeded fails with a non-zero exit
code naming the budget.

**Not yet done.** The gate is enforced on the composed MCP result. The base's
own per-tool byte cap still applies underneath it, so the two limits compose
rather than replace each other.

### CB-10 - Governance as verdicts, not prose

**Evidence.** `OpenLore` publishes four deterministic verdicts with no LLM in
the path: `verify_claim` -> `confirmed` / `refuted` / `unverifiable` with a
citation receipt; `certify_public_surface` -> per export
`breaking` / `non-breaking` / `potentially-breaking` with the consumers named;
`change_impact_certificate` -> differential reachability, *"did this diff open a
new path into a boundary you declared?"*; and one commit gate mapping every
finding to `blocking` / `frozen` / `advisory` / `off`. None of it is in the
base, and none of it is in a retrieval tool.

**What to build.** The same four verdicts over the base's graph, and the DX
rules expressed as machine checks. `AGENTS.md` section 2 rule 1 - never absorb
a child repo - becomes a check that refuses a diff opening a path across a
declared boundary.

**Status 2026-10-21.** The **policy** is done and tested:
`src/mcp/dx_governance.c` implements all four verdicts with 23 contract tests,
and the module depends on yyjson alone, so every verdict is verifiable without
a store. Three of the four are now wired through the DX surface as request
types - `verify` (graph-backed, through the base's own snippet lookup),
`certificate` and `enforce` - and the surface grew from 34 to 34 tests with
the new types covered.

**Not yet done.** `certify_public_surface` - the per-export breaking-change
verdict - is implemented and tested as policy but **not wired**: the graph side
needs the diff's changed exports and their consumers, which is the next slice.
It is the one remaining piece of this item.

**Done when.** A diff that introduces a path dependency across a declared
boundary is refused by the gate with the offending edge named, and the refusal
reproduces from the same tree state.

### CB-11 - Anchored, self-invalidating memory

**Evidence.** The base's ADR store holds decisions but cannot say when they
went stale. Two competitors do: `CGraph` anchors each memory link to a sha256
of the symbol's own source span, so a recall reports `valid` / `changed` /
`unanchored` / `gone`; `OpenLore`'s "Epistemic Lease" puts a factual staleness
note on every response - *"facts, never commands"*. The base's own `docs/`
records the same idea from the other side: `agent-memory-hub` tracks "codebase
conclusions that go stale when their files change".

**What to build.** Every memory the fork records carries the hash of the exact
span of code the memory is about. A recall re-checks the anchor and reports the
state, so an agent is *told* when its memory is out of date instead of serving
a confident stale fact.

**Status 2026-10-21.** The **policy** is done and tested:
`src/mcp/dx_memory.c` implements the four states with 12 contract tests, and the
module is **pure** - it reads no files and calls no graph, so the caller
supplies the current content of the anchored span and the module decides what
that means. The hash is the vendored `xxhash` (XXH64, hex), so it is stable
and needs no new dependency.

**Not yet done.** The store side: writing a memory with its anchor hash, and
resolving an anchor to the current span at recall time, both need the base's
store and its Windows-safe file access. That is the next slice.

**Done when.** A memory whose symbol was edited recalls as `changed`, and one
whose symbol was deleted recalls as `gone`, with no false certainty - and a
memory that was never anchored recalls as `unanchored`, never as `valid`.

### CB-12 - `dx-codebase prove`: measure it before you trust it

**Evidence.** `OpenLore` publishes its own losses - *"+43%"* on a small,
familiar repo - and says *"Don't guess from our repos - run `openlore prove` on
yours."* Its `prove --estimate` projects the orientation cost from the
repository's own call graph, offline, with no API key. The base's measurement
guide sets the standard the fork inherits: *"Never turn a result from one
repository, question set, model, or machine into a universal savings claim"*
(`docs/MEASURING_SAVINGS.md:277`).

**What to build.** One command that measures this tool on a repository the
caller names, and prints the cost against the grep baseline **including the
case where it loses**.

**Done when.** `dx-codebase prove` on a DX product folder prints a cost
projection with its own baseline, and a case where the tool is slower than grep
is reported rather than hidden.

---

## P2 - Keep the fork maintainable, and the claims honest

### CB-13 - The measurement programme: pre-register, then measure

**Evidence.** The benchmarks to submit to are public and do not require trusting
our own harness. **Agent Retrieval Bench** (arXiv 2607.24882) covers four task
types nobody else has framed this way - `code2test`, `comment2context`,
`trace2code`, `edit2ripple` - over 427 samples in 25 repositories, 392,000
files and 7.9 million chunks, and its headline finding is that *"no single
retrieval family dominates"*. **Sverklo's bench** accepts a new baseline by pull
request and its CI runs it in about ten minutes without any local run, and
publishes every open loss because *"a bench that only shows wins is marketing,
not measurement"*. And the pre-registration pattern is demonstrated: a
competing benchmark wrote its hypotheses, budget and analysis down **before**
the first run and listed every later deviation with its reason.

**What to build.** A committed pre-registration document (hypotheses, repos,
budget, analysis, all fixed before the first measurement run), then submissions
to both public benchmarks, then a published table with the wins and the losses
side by side.

**Done when.** A pre-registration file is committed and dated before the first
measurement run, this tool appears in the Agent Retrieval Bench results, and the
published table shows a case where it loses.

**Partial measurement, 2026-10-21 - the surface size, on the built binary.**
The `tools/list` payload the agent receives, under each profile, at the base's
own four-bytes-per-token rate:

| Profile | Tools | Schema bytes | Tokens |
| --- | --- | --- | --- |
| ALL (upstream's surface) | 18 | 24111 | 6028 |
| DX (this fork's surface) | 1 | 3182 | 796 |

**An 86.8% reduction in the schema the agent receives**, measured on
`build/c/codebase-memory-mcp.exe`. What this does **not** measure, and what
this item is actually for: the whole-session token cost, with a real agent, on
a real repository, against a real baseline. The schema is what the agent
receives once; a session receives it once and then spends its tokens on
answers, so the session number could be smaller, larger, or the same. It is
not measured, and until the pre-registration exists it should not be quoted
as if it were.

This is also not the base's +66% figure, which is a whole-session measurement
on the `zod` repository with the GPT-5.6 grid. Two different measurements, two
different claims.

**What to build next, in order.**

1. A **pre-registration**: the hypothesis (the DX surface costs fewer tokens
   per answered question than the full menu, for the same answer quality),
   the repositories, the agent, the budget, the analysis, and the failure
   conditions - all fixed and committed **before** the first run.
2. The run, on a repository that is indexed on this machine.
3. The published table, with the losses beside the wins.

### CB-14 - Fix the upstream sync cadence

**Evidence.** Upstream landed 74 commits on this repository's `main` in one
day, 2026-10-20 to 2026-10-21. The local checkout was 74 commits behind until
it was fast-forwarded on 2026-10-21. Upstream's README was restructured in the
same window, which invalidated a dozen line-number citations in this
repository's own `TECH.md`.

**Done when.** A scheduled or documented cadence exists, and `TECH.md` cites
upstream line numbers against a named commit rather than against "the README".

### CB-15 - Get the hub's release-asset naming decision

**Evidence.** Every other DX tool publishes `dx-<tool>-<target-triple>[.exe]`
(`essence-dx/media` v1.0.0 is the proof in `DX.md:38`). This fork's tool is
named `codebase`, not `dx-codebase`, because the organisation is already
`essence-dx`. That breaks the pattern the hub's own installer depends on. The
live check on 2026-10-21 found all 14 registry entries using `dx-<tool>`.

**Done when.** The hub states in writing whether `codebase` is the exception, or
the tool is renamed.

### CB-16 - Decide what happens to CBM Lens in a DX product

**Evidence.** CBM Lens is upstream's human interface, in open beta at v0.0.1
(`README.md:21`), with a browser-local model that downloads weights from
Hugging Face on first use. It is a 3D architecture map and Galaxy graph
(`README.md:29`) - upstream's own choice, not a DX requirement, and it is the
largest part of the release artifact.

**Done when.** A decision is recorded in `CHANGELOG.md` stating whether the
fork ships Lens, ships it disabled, or removes it, with the artifact-size
consequence named.

### CB-17 - Decide whether the graph comes from the compiler

**Evidence.** `@ttsc/graph` reads the program the TypeScript compiler has
**already type-checked**, so `tsconfig` path aliases, pnpm-monorepo
cross-package references, symlinks and barrel re-exports land on the real
declaration rather than on a text guess. Its index built in **29 seconds** on
VS Code's three million lines where `codegraph` took **twelve minutes** and
serena four and a half. The base's own "Hybrid LSP" is a hand-rolled
approximation of rust-analyzer, tsserver, pyright, gopls, Roslyn and JDT. The
same author estimates a tree-sitter-backed version of the same interface at
roughly 3x rather than 10x - *"one language handled at compiler depth is worth
more to an agent than 158 skimmed off the top"*.

**What to build.** A decision, not necessarily the feature: for Rust and
TypeScript, read the compiler's own program instead of re-deriving it.

**Done when.** The decision is recorded in `CHANGELOG.md` with the effort
estimate for the Rust path named, and the measurement that justifies it.

### CB-18 - Offline two-stage retrieval: embed, then rerank

**Evidence.** The base bundles `nomic-embed-code` (768d, int8) and scores with
11 signals (`src/semantic/semantic.h:1-45`) but has **no** reranker -
`grep -ri rerank src/` returns nothing. The removed candidate had no embeddings
at all, so its retrieval ceiling was lexical.

**Counter-evidence, recorded so this is not a straight yes.** A competing
implementation ships the reranker as **opt-in**, because *"prose-trained
rerankers can rank documentation above code, so code-heavy corpora often
retrieve better without one"*, and the model's size is a real cost (150 MB in
one case, 23 MB in another).

**Done when.** A measured retrieval benchmark shows the reranked ranking beats
the embedding-only ranking on the same queries, with the command and the
machine named - **and** a query set on which it does not, so the default stays
honest.

### CB-19 - The DX-native org graph layer

**Evidence.** The code graph knows the code. It does not know that `dx-check` is
built from `essence-dx/check`, that the manifest routes `check` to the folder
`check`, or that `bot` path-depends on `metasearch` and breaks when the second
moves. Those are organisational facts, they live in the hub, and until they are
in the graph the tool cannot answer the question the hub actually asks: which
repository owns this, which rule governs it, and what else breaks if it changes.
CBM's cross-repo mode links *services*; it has no model of a DX tool, a DX rule
or a DX register row.

**What to build.** A module that turns three inputs into DX-native nodes and
edges - the `dx` workspace manifest (its `paths(...)` block), the published tool
registry, and the declared path dependencies - with a node vocabulary of
`DXRoute`, `DXTool`, `DXFolder`, `DXRepo` and `DXPathDependency`. It reads no
files and calls no graph, so it is verifiable without a workspace, on the same
pattern as `dx_surface.c` and `dx_governance.c`.

**Status 2026-10-21.** Designed and prototyped, then **reverted rather than
shipped**: the prototype's output carried corrupted string values and the cause
was not found in the time available. Unverified code does not go in. The
design above and the two defects found are the record:

- **A size calculation was wrong.** The dependency id is
  `"dx:dep:" + from + "->" + to`, and the buffer was allocated
  `strlen(from) + strlen(to) + 8` when it needed `+ 10`, so the id was
  silently truncated to `dx:dep:bot->metasear`.
- **The same node id was emitted twice.** A folder that is both a route target
  and a dependency source produced two nodes with one id, and a graph with the
  same id twice is not a graph. The fix was a seen-set, so the duplicate
  cannot come back.

**Done when.** One query answers "which repository owns this symbol, which rule
governs it, and what else breaks if it changes", and the answer names the
repositories **consulted and skipped**.

### CB-20 - The Windows build story: the build works, the house tests do not run yet

**Status 2026-10-21 - the build blocker is FIXED.** Option 2 below was taken.
A new `src/foundation/printf_attr.h` provides `CBM_PRINTF_FMT`, which selects
`gnu_printf` - always the C99-conformant format - and the three sites that
carried `format(printf, ...)` now use it. `make -f Makefile.cbm cbm` **links**
on this machine.

**Why the original diagnosis pointed at the wrong thing.** The first reading
was that upstream's Windows binary is an MSVC build and MinGW simply cannot
build the tree. That is not what the evidence says. The implementation of
`cbm_arena_sprintf` is `vsnprintf` - C99 - and `%zu` works perfectly at
runtime. Only the *compile-time* attribute was wrong: GCC's `printf` attribute
means *the target's* printf, and on MinGW that is the Microsoft one, whose
format dialect has no `%z`. The attribute describes the format string, not the
runtime library, so describing a `vsnprintf`-backed helper as `gnu_printf` is
the accurate description rather than a workaround. The tree builds with the
toolchain that is actually present.

**What is still open.** The **house test binary** has not been built and run.
The DX suites are wired into `tests/test_main.c` and `Makefile.cbm`
(`TEST_MCP_SRCS`), but the `test` target has never been built here, so the DX
suites still run standalone against a small driver rather than inside the
project's own harness. That is a real gap: the standalone driver supplies its
own counters and its own `main`, so it does not prove the suites run in the
project's runner.

**Done when.** `make -f Makefile.cbm test` completes on this machine and the
DX suites report their pass counts inside the house runner.

**Evidence - the original blocker, for the record.**

```
src/pipeline/helm_template.c:2202:72: error: unknown conversion type character 'z' in format
internal/cbm/doc_pdf.c:1337:70: error: unknown conversion type character 'z' in format
src/pipeline/pass_k8s.c:4672:58: error: too many arguments for format
```

### CB-21 - The DX surface must be what you get by default, not a flag

**Evidence.** The DX profile works, and it is the only way to reach every DX
feature in this fork. It is also **opt-in**: `src/main.c` initialises the
profile to `CBM_MCP_TOOL_PROFILE_ALL` and only `--tool-profile=dx` (or
`--tool-profile dx`) selects the DX surface. So a user who installs this fork
and runs it gets upstream's seventeen-tool menu, and the thing this fork was
built to replace is the default.

The measurement behind that decision is not in dispute: the upstream base
measured **+66% tokens** for a session with MCP attached versus without it.
The surface, not the graph, is where the tokens go. A product that ships the
old surface by default has not solved the problem it was built to solve.

**What to decide.** Three options, and this is a product decision rather than
a code one:

1. **Default the binary to the DX profile.** `--tool-profile=all` restores the
   upstream menu for anyone who wants it. Cleanest for a product, and it
   matches the name: a `dx-codebase` binary that behaves like
   `codebase-memory-mcp` is confusing.
2. **Default by binary name.** If `argv[0]` ends in `dx-codebase`, select the
   DX profile; otherwise `all`. Two binaries, two behaviours, one build. It
   also leans on `CB-01`, where the binary name is still open.
3. **Default through the install contract.** The install command writes the
   client configuration, so it could write `--tool-profile=dx` into the
   generated client config. The flag stays the product's default without
   changing the binary's behaviour for a hand-run invocation.

**Why it is not done here.** The default profile changes the behaviour of
every existing invocation of this binary, and the binary's name - and
therefore whether option 1 or option 2 is even the right shape - is still
open in `CB-01`. Deciding the default before the name would be deciding it
twice.

**Done when.** A fresh install of the DX binary presents `dx_codebase` as its
only tool without the user passing a flag, and `--tool-profile=all` still
presents the upstream menu.

---

MIT OR Apache-2.0
updated: 2026-10-21 00:00 UTC
