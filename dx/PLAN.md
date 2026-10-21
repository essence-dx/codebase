# PLAN.md - `dx-codebase`: what this fork is becoming

**Repository:** `essence-dx/codebase` (a fork of `DeusData/codebase-memory-mcp`, MIT).
**Base pinned at:** upstream `3457b21b` (2026-10-21), tracked as the `upstream` remote.

**This file is the plan.** `TODO.md` is the ledger of the same work, item by item,
with evidence and done-when checks. `TECH.md` is why this fork exists. This file
is what we are going to build and in what order.

**Claim tiers apply here too** (`G:\Dx\AGENTS.md` section 4). Every number below
is either *repo-managed* (read out of a file, `file:line` named) or *upstream-claimed*
(somebody else's measurement, quoted as theirs). **Nothing here has been measured
in this hub**, because neither tool has been built on this machine yet.

---

## The finding that sets the whole plan

The single most important result of the 2026 research is not a feature. It is
that **the base's graph is good and the base's surface is what loses**.

From `ttsc`'s measured autopsy of all three candidates (2026-06-30), on the
GPT-5.6 grid with the `zod` repository, token cost **relative to running with no
MCP server at all**:

| Tool | Cost vs no MCP | Worst cell in the matrix |
| --- | --- | --- |
| `codebase-memory-mcp` | **+66% more** | +82% |
| `codegraph` | **+4% more** | +34% |
| `serena` | **+130% more** | +207% |
| `@ttsc/graph` (compiler-backed) | **11% of baseline** | - |

Their words about the base:

> *"In the runs I measured it called the MCP zero times and fell back to the
> shell every time. It gave up and grepped. **The graph was right there; the
> surface buried it.**"*
>
> *"Its MCP `initialize` sends no instructions at all. What guidance it does have
> lives in an install-time skill file that the agent never sees if you just wire
> up the server... you get fourteen tools over the wire with almost nothing
> telling the agent which one to reach for or when."*
>
> *"You can't write your way to adoption."*

An independent benchmark agrees from the other direction. Kazennov's 664-run
comparison (`nefayran/code-graph-vs-grep`, 2026-10-01) used a **thin wrapper**
around the base - `cbm-lean` - and the wrapper was **cheaper than the raw server
in every setting** (`p <= 0.021`).

**Therefore: the engine is not the work. The interface is the work.** The base
already has the graph, the embeddings, the Cypher engine, the ADR store, the docs
passes, Smart CI and CBM Lens. What it does not have is a surface an agent
actually reaches.

---

## The five features

Each is a `TODO.md` item with evidence and a done-when check. They are ordered by
leverage, not by difficulty.

### F1 - One tool, typed, with CoT-compliance (`CB-13`)

**What.** One MCP tool, `dx_codebase`, whose input schema makes the reasoning a
*required argument* rather than a request:

```
question : string                      (required)  the code question, in the caller's words
draft    : { reason: string,           (required)  the smallest plan, and why
            type:   <request type> }                it looks smallest
review   : string                      (required)  self-correct; may name a different type
request  : { type: <request type>, ... } (required) the committed request
```

`request.type` is a discriminated union over `search | trace | details | outline
| architecture | schema | compare | coverage | changes | impact | escape`.
`escape` is a first-class no-op: *"this is not a graph question"*.

The response carries two extra fields the base never had:

- `audit` - *what the server checked this answer against.*
- `next`  - *answer now, inspect one more thing, or escape.*

**Why it wins.** A required field on a typed schema cannot be skipped the way a
line of prompt can be ignored. ttsc's measured result: the agent finally used the
tool, and turned a vague question into the precise request itself. Their rule of
thumb: *"free prose can hide a skipped step; a typed submission cannot."*

They also found the small trick that matters: a field of **symbol names, never a
sentence**. On the Vue tour it took Sonnet from **seven calls to one**.

**Done when.** The server advertises exactly one tool under the `dx` profile; a
call that omits any of the four envelope fields is rejected at the call boundary
with a schema error, not a success; `escape` returns a clean non-graph answer;
and a fixture test proves each `request.type` reaches the right underlying
handler.

### F2 - Return the index, never the bodies, under a hard token budget (`CB-14`)

**What.** The tool answers with *relationships* - names, edges, signatures, spans
- not with source bodies. The answer size then does not grow with the repository:
a 10k-line project and a 100k-line one come back about the same. And the budget
is a **gate**: exceeding it fails with a non-zero exit code, the way Repomix's
`--token-budget N` does, not a soft warning.

**Why it wins.** codegraph returns whole source bodies - *"the Read, done for
you"* - and that is the token bomb ttsc measured. Sverklo's bench makes
**tokens-per-correct-answer the primary axis** and F1 secondary, because an agent
in a bounded context window pays for every token returned.

The base already has the mechanism: `src/mcp/compact_out.c`, and the snippet path
already emits `source_truncated`, `source_clipped`, `truncation_reason` and
`next_start_line`. **It is not the default.** Making it the default is a
configuration change plus a policy, not a rewrite.

**Done when.** A default call returns no source bodies, the response carries the
token count, and a call whose budget is exceeded fails with a non-zero exit code
naming the budget.

### F3 - The org graph: code + IaC + decisions + the `dx` manifest (`CB-15`)

**What.** One graph over the whole `G:\Dx` tree, with DX-native nodes and edges:
the `dx` manifest routes, the `TOOL_REGISTRY` entries, the `docs/<tool>.md`
register rows, the `AGENTS.md` rule scopes, and the path-dependency web between
product folders (`bot` -> `metasearch` -> `daemon` -> `tui` -> `forge` ->
`serializer` -> `Rustdesk` outside the hub). Plus a hazard-typed conflict graph
over every in-flight branch, PR and agent task.

**Why it wins.** OpenLore proved the shape: application code, IaC and decisions
project onto **one shared set of node and edge primitives**, so a single traversal
spans all three. No competitor can build the DX layer, because no competitor has
a `dx` manifest. And the in-flight conflict graph has no competitor at all.

**Done when.** One query answers *"which repository owns this symbol, which DX
rule governs it, and what else breaks if it changes"* across the org, and the
answer names the repositories consulted **and the ones skipped**.

### F4 - Governance as verdicts, not prose (`CB-16`)

**What.** Four deterministic verdicts, no LLM in the path:

| Verdict | Answers |
| --- | --- |
| `verify_claim` | `confirmed` / `refuted` / `unverifiable`, with a citation receipt |
| `certify_public_surface` | per export: `breaking` / `non-breaking` / `potentially-breaking`, consumers named |
| `change_impact_certificate` | differential: did this diff **open a new path** into a declared boundary? |
| `enforce` | one commit gate; each finding maps to `blocking` / `frozen` / `advisory` / `off` |

**Why it wins.** This is the part a retrieval tool cannot do, and it is what
makes `dx-codebase` fit DX's own rules instead of sitting next to them. DX's
`AGENTS.md` section 2 rule 1 - never absorb a child repo - becomes a **machine
check** rather than a thing an agent might forget.

**Done when.** A diff that introduces a path dependency across a declared
boundary is refused by the gate with the offending edge named, and the refusal
is reproducible from the same tree state.

### F5 - Memory that goes stale, and `dx-codebase prove` (`CB-17`)

**What.** Two halves.

1. **Anchored memory.** Every memory link records a hash of the symbol's own
   source span, so a recall reports `valid` / `changed` / `unanchored` / `gone`
   instead of a confident stale fact. The base's ADR store has no such check.
2. **`dx-codebase prove`.** Before the tool is trusted on a repository, it
   measures itself there: the cost of orientation from that repository's own call
   graph, offline, with no API key - **including the case where it loses.**

**Why it wins.** OpenLore publishes its own losses (*"Round 1: +43%"* on a small
familiar repo) and says *"Don't guess from our repos - run `openlore prove` on
yours."* A tool that refuses adoption without a measurement is DX's culture, not
a bolt-on. The anchored memory is what stops the base's ADR store from becoming
the confident-but-wrong thing AGENTS.md section 4 warns about.

**Done when.** `dx-codebase prove` on a DX repository prints a cost projection
with its own baseline, and a memory whose symbol was edited recalls as
`changed`.

---

## Measurement: how we prove it, and how we avoid claiming what we did not measure

Three commitments, in order of how much they cost us.

1. **Pre-register the hypotheses.** Kazennov wrote `PLAN-v2.md` *before* any run:
   four falsifiable hypotheses (H1 size, H2 model, H3 exact questions, H4 fixed
   overhead), a fixed budget, a fixed analysis, and a "Changes after start"
   section listing every deviation with its reason. We do the same, and the
   pre-registration is committed before the first measurement run.
2. **Submit to the public benchmarks.** Agent Retrieval Bench (arXiv 2607.24882)
   covers four task types nobody else has framed this way - `code2test`,
   `comment2context`, `trace2code`, `edit2ripple` - over 427 samples in 25
   repositories. Sverklo's bench accepts a new baseline by **pull request**, and
   its CI runs the new baseline in about ten minutes without any local run.
   Neither requires us to trust our own harness.
3. **Publish the losses.** Sverklo's own rule: *"a bench that only shows wins is
   marketing, not measurement."* Whatever we measure, the wins and the losses go
   in the same place, in the same table.

**The base's own rule applies to us.** `docs/MEASURING_SAVINGS.md:277`:
*"Never turn a result from one repository, question set, model, or machine into
a universal savings claim."* We inherit the rule and not the number.

---

## Order of work

| Milestone | Feature | Why this order |
| --- | --- | --- |
| **M1** | F1 - the surface | Highest leverage. Everything else is unreachable until the agent calls the tool. Also the smallest testable slice. |
| **M2** | F2 - index, not bodies | Second: the surface is useless if the answer is a token bomb. |
| **M3** | F4 - governance verdicts | Third: it is what makes this a DX tool rather than a retrieval tool, and it reuses the graph F1 already exposes. |
| **M4** | F3 - the org graph | Fourth: it needs the stable surface from M1 and the receipts from M3 to be worth anything. |
| **M5** | F5 - stale memory + prove | Last: it needs a stable graph under it, and `prove` needs all the above to measure honestly. |

**M1 is the only milestone started in this session**, and it is the one with the
clearest evidence behind it.

### Status

| Milestone | Feature | State |
| --- | --- | --- |
| M1 | F1 - the surface | **Landed 2026-10-21.** The contract module and its 34 tests are in `src/mcp/dx_surface.c` and `tests/test_dx_surface.c`; the tool, the `dx` profile and the dispatch are wired into `src/mcp/mcp.c`. The full server build has not been run in this environment - see `CHANGELOG.md`. |
| M2 | F2 - index, not bodies, under a hard budget | **Landed 2026-10-21, inside M1.** The answer policy (`source`, `max_tokens`), the gate, and the `shape` report are part of the surface, because the shape of the answer is decided at the surface. |
| M3 | F4 - governance verdicts | **Landed 2026-10-21, mostly.** The verdict policy is complete in `src/mcp/dx_governance.c` with 23 contract tests, and three of the four verdicts - `verify`, `certificate`, `enforce` - are reachable through the surface. `certify_public_surface` is implemented as policy but not yet wired: its graph side needs the diff's changed exports and their consumers. That is the one open piece of `CB-10`. |
| M4 | F3 - the org graph | Not started |
| M5 | F5 - stale memory + prove | Not started |

---

## What this fork is not

- **Not a rewrite.** The indexer, the store, the Cypher engine, the 162 grammars
  and CBM Lens are upstream's, under MIT. We keep the licence and the notice.
- **Not a re-implementation of the competitors.** ttsc's compiler-backed graph is
  TypeScript-only and depends on the TypeScript 7 compiler; that is a different
  bet from DX's, and it is `CB-18`'s subject, not this plan's.
- **Not a `dx-codebase` binary.** The organisation is already `essence-dx`, so
  the repository slug carries no second `dx` prefix. The *binary* naming is the
  one open decision - 14 of 14 existing registry entries use `dx-<tool>` - and it
  is `CB-11`.
- **Not allowed to over-claim.** Every number in the public docs carries a
  `file:line` or is marked upstream-claimed. That is the house rule and it is
  also the product's selling point.

---

MIT OR Apache-2.0
updated: 2026-10-21 00:00 UTC
